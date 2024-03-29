/* 
 * Copyright (c) 2011, Daniel Swann <hs@dswann.co.uk>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of nh-irc nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "irc.h"
#include <stdio.h>

irc::irc(string address, int port, string nick, string nickserv_password, CLogging *l)
{
    state = CLOSED;
    lastError = "";
    irc::address = address;
    irc::port = port;
    irc::nick = nick;
    irc::nickserv_password = nickserv_password;
    pw_sent = false;
    alt_nick = false;
    log = l;
    rThread = -1;
    aThread = -1;
    last_rx = -1;
    ping_sent = -1;
    pthread_mutex_init (&socket_mutex, NULL);
}

irc::~irc()
{
  string tx;
  if (state == irc::CONNECTED)
  {
    tx = "QUIT :Yeah. Right on.\r\n";
    write(tx);
    usleep(1000);
    state = UNLOADING;
    close(skt);
    log->dbg("Connection to IRC server closed");
    if (rThread > 0)
      pthread_join(rThread, NULL);
    rThread = -1;
    
    // Interupt the activity monitoring thread
    kill(getpid(), SIGUSR1);
    
    if (aThread > 0)
      pthread_join(aThread, NULL);
    aThread = -1;    
  }
}
    

// Blocks until connected (or errors trying)
int irc::ircConnect()
{
    string tx;
    string buffer;
    char ch;
    
    skt = socket(AF_INET, SOCK_STREAM, 0);
    if (skt < 0) 
    {
      state = ERROR;
      lastError =  "Unable to open socket";
      return -1;
    }
    
    server = gethostbyname(address.c_str());
    if (server == NULL) 
    {
        state = ERROR;
        lastError = "No such host";
        return -1;
    }
    
    memset(&skt_addr, 0, sizeof(skt_addr));
    skt_addr.sin_family = AF_INET;
    skt_addr.sin_port = htons(port);
    memcpy(&skt_addr.sin_addr.s_addr, server->h_addr, server->h_length); // FIXME: h_length 

    if (connect (skt, (struct sockaddr *)&skt_addr, sizeof(skt_addr)) < 0)
    {
        state = ERROR;
        lastError = "Failed to connect";
        return -1;      
    }
    
    tx = "nick " + nick + "\r\n";
    if (write(tx))
    {
      lastError = "Failed to send nick cmd";
      state = ERROR;
      return -1;
    }

/*
 *     Command: USER
 *  Parameters: <username> <hostname> <servername> <realname>
 */
    tx = "user bot bothost " + address + " bot\r\n";
    if (write(tx))
    {
      lastError = "Failed to send user cmd";
      state = ERROR;
      return -1;
    }
    
    // Read up to newline
    buffer = "";
    ch = 0;
    while ((read(skt, &ch, 1) > 0) && state != CONNECTED)
    {
      buffer = buffer + ch;
      if (ch == '\n')
      {
        log->dbg(" < " + buffer.substr(0, buffer.length()-1));  
        processMessage(buffer);
        buffer = "";
      }
    }
    if (state == CONNECTED)
    {
      // Now conencted. Start thread to process incoming messages, then return
      pthread_create(&rThread, NULL, &irc::readThread, this);
      
      // Create thead to monitor connection
      pthread_create(&aThread, NULL, &irc::activityThread, this);
      return 0;
    } else
    {
      state = DISCONNECTED;
      return -1;
    }
    
}

void irc::readThread()
{
    string buffer;
    char ch;
   
    while ((read(skt, &ch, 1) > 0) && state == CONNECTED)
    {
      buffer = buffer + ch;
      if (ch == '\n')
      {
        log->dbg(" < " + buffer.substr(0, buffer.length()-1));  
        processMessage(buffer);
        buffer = "";
        last_rx = time(NULL);
      }
    }
    if (state != UNLOADING) // UNLOADING state is set in the destructor
      state = DISCONNECTED;
    processMessage("");  
}

void *irc::readThread(void *arg)
{
    irc *Irc;
    Irc = (irc*) arg;  
    Irc->readThread();
}

// Watch for connection timeouts. If we don't get anyting from the server in the timeout period,
// try sending a ping. If no reply, assume the connection has been lost.
void irc::activityThread()
{
  while (state == CONNECTED)
  {
    // We've received something since the last ping sent, so connection is good
    if (last_rx >= ping_sent)
      ping_sent = -1;
    
    if (((time(NULL) - last_rx) > (4 * 60)) && (last_rx != -1) && (ping_sent==-1))
    {
      // Nothing received within last 4 minutes... send a ping
      write ("PING NH\r\n"); // In all likleyhood, this will trigger a disconnection
      ping_sent = time(NULL);
    }
    
    if (((time(NULL) - last_rx) > (5 * 60)) && (last_rx != -1) && (ping_sent!=-1))
    {
      // Nothing received within 5 minutes despite sending a ping... give up.
      shutdown(skt, SHUT_RDWR);
      close(skt);
      
      state = DISCONNECTED;
      log->dbg("Ping timout...");
    }    
    sleep (30);   
  }
}

void *irc::activityThread(void *arg)
{
    irc *Irc;
    Irc = (irc*) arg;  
    Irc->activityThread();
}

void irc::wait()
{
  log->dbg("Waiting.");
  pthread_join(rThread, NULL);
  pthread_join(aThread, NULL);
}

void irc::processMessage(string message)
{
  string tx;
  int pos;
  int n;
  char buf[20];
  
  string prefix;
  string cmd;
  string params;
  
  if (state == UNLOADING)
    return;
  
  if (state == DISCONNECTED && message == "")
  {
    // Send disconnection message to all callbacks registered
    for (n=0; n < callbacks.size(); n++)
    { 
      (*callbacks.at(n).ircCallback)("", "INTERNAL", "DISCONNECTED", callbacks.at(n).obj);      
      wait();
      return;
    }
  }
  
  // Get prefix (message source - so either the server or a user)
  if (message.substr(0, 1) == ":")
  {
    pos = message.find(' ');
    if (pos>1)
      prefix = message.substr(0, pos);
    else
      prefix = "";    
  } else
    prefix = "";
  
  // Get command
  pos = message.find(' ', prefix.length()+1);
  if (pos > 1)
  {
    cmd = message.substr(prefix.length(), pos-prefix.length());
  } else
    cmd = "?";
  
  // Get params
  if (cmd != "?")
  {
    if (message.length() >  (prefix.length() + cmd.length()+1))
      params = message.substr(prefix.length() + cmd.length(), -1);
    else
      params = "";
  }
  
  if (prefix.length() >= 2)
    prefix = prefix.substr(1, -1); // remove leading ':' 
    
  if (cmd.length() >= 2)
    if (cmd[0] == ' ')
      cmd = cmd.substr(1, -1); // remove leading space
   
  if (params.length() > 2) // remove leading space and trailing cr/lf
  {
    pos = params.find_first_of("\r\n");
    if (pos >= 0) pos--;
    params = params.substr(1, pos);
  }
    
//log->dbg("[" + prefix + "] [" + cmd + "] [" + params + "]");
  
  // Respond to pings
  if (cmd == "PING")
  {
    tx = "PONG " + params + "\r\n";
    if (write(tx))
    {
      lastError = "Failed to send PONG [" + tx + "]";
      state = ERROR;
      return;
    }
  }
  
  // When the end of the MOTD is recieved, update our status to CONNECTED
  if (cmd == "376")
  {
    state = CONNECTED;
   
    // if we're connected with an alternative nick, and have a nickserv password,
    // ghost whoever's using our name
    if (alt_nick && (nickserv_password != ""))
    {
      tx = "PRIVMSG nickserv :ghost " + nick + " " + nickserv_password + "\r\n";
      
      if (write(tx))
      {
        lastError = "Failed to send: [" + tx + "]";
        return;
      }          
    }
  }
  
  // chat message
  if (cmd == "PRIVMSG")
    rx_privmsg(prefix, params);
  
  // nick already taken - use current nick + pid as temp username
  if ((cmd == "433") && (!alt_nick))
  {
    alt_nick = true;
    sprintf(buf, "%s-%d\r\n", nick.c_str(), getpid());
    tx = "NICK " + (string)buf;
    if (write(tx))
    {
      lastError = "Failed to send: [" + tx + "]";
      return;
    }        
  }
  
  // Identify with nickserv
  if ( (prefix.compare(0, 8, "NickServ") == 0) && (cmd == "NOTICE") && (!pw_sent) && (params.find("identify") != string::npos))
  {
    // Bit nauty - shoudn't really be sending an automated response to a notice.
    pw_sent = true;
    
    if (nickserv_password == "")
    {
      log->dbg("Asked to IDENITFY, but no password set!");
    } else
    {
      tx = "PRIVMSG nickserv :identify " + nickserv_password + "\r\n";
      
      if (write(tx))
      {
        lastError = "Failed to send: [" + tx + "]";
        return;
      }      
    }
    
  }
  
  // If we've just ghosted someone/thing using our nick, change to our primary nick
  if ( (prefix.compare(0, 8, "NickServ") == 0) && (cmd == "NOTICE") && (params.find("ghosted") != string::npos))
  {
    tx = "NICK " + nick + "\r\n";;
      
    if (write(tx))
    {
      lastError = "Failed to send: [" + tx + "]";
      return;
    }       
  }
}

int irc::rx_privmsg(string prefix, string params)
{
  //e.g.[daniel1111ss!~daniel@94-195-XXX-XXX.zone9.bethere.co.uk]  [#test4444 :09876]
  string user;
  string channel;
  string message;
  int pos, n;
  
  if (prefix.length() <= 2)
    return -1; // A chat message with no user is no good!

  user = prefix.substr(0, prefix.find('!'));
  
  pos = params.find_first_of(' ');
  if (pos <= 0)
    return -1;
  channel = params.substr(0, pos);

  pos = params.find_first_of(':');
  if ((pos <= 0) || ((pos) >= params.length()))
    return -1;
  message = params.substr(pos+1, -1); 
  
  // Now see if the start of the message matches a word we're looking for, and
  // call the appropriate callback if it does
  for (n=0; n < callbacks.size(); n++)
    if (callbacks.at(n).trigger == message.substr(0, callbacks.at(n).trigger.length()))
      (*callbacks.at(n).ircCallback)(user, channel, message, callbacks.at(n).obj);
}

int irc::join(string room)
{
  string tx;
  
  tx = "join " + room + "\r\n";
  if (write(tx))
  {
    lastError = "Failed to join " + room;
    return -1;
  }  
  channels.push_back(room);
  return 0;
}

int irc::write(string msg)
{
  string d;
  int ret;
  
  d = msg;
  d.erase(d.find_last_not_of("\r\n")+1);

  log->dbg(" > " + d);
  ret=0;
  pthread_mutex_lock(&socket_mutex);
  if (::write(skt,msg.c_str(),msg.length()) < msg.length())
  {
    log->dbg("write failed!");
    ret = -1;
  }
  pthread_mutex_unlock(&socket_mutex);
  
  return ret;
}

int irc::send(string message)
{
  if (channels.size() > 0)
    return send(channels[0], message);
  else 
    return -1;
}

int irc::send(string room, string message)
{
    string tx;

    if (state != irc::CONNECTED)
    {
      log->dbg("irc::send: Not connected!");
      return -1;
    } 
    tx = "NOTICE " + room + " :" + message + "\r\n";
    
    if (write(tx))
    {
      lastError = "Failed to send: [" + tx + "]";
      return -1;
    }  
  return 0;
}

int irc::addCallback(string trigger, int (*ircCallback)(string, string, string, void*), void *obj)
{
  struct callback cb;
  int n;
  
  cb.trigger = trigger;
  cb.ircCallback = ircCallback;
  cb.obj = obj;
  
  if (ircCallback = NULL)
    return -1;
  
  // Don't allow the same trigger word/phrase to be used more than once.
  for (n=0; n < callbacks.size(); n++)
    if (callbacks.at(n).trigger == trigger)
      return -1;
    
   callbacks.push_back(cb);
  
  return 0;
}

