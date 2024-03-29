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

#include "CNHmqtt.h"
#include "irc.h"
#include <stdio.h>

bool CNHmqtt::debug_mode = false;
bool CNHmqtt::daemonized = false; 

class nh_irc : public CNHmqtt
{
  
  public:
    irc *irccon;
    string irc_server;
    int irc_port;
    string irc_nick;
    string irc_channel;
    string irc_mqtt_tx;
    string irc_mqtt_rx;
    string irc_nickserv_password;
  
    nh_irc(int argc, char *argv[]) : CNHmqtt(argc, argv)
    {
      irc_server  = get_str_option("irc", "server", "irc.freenode.net");
      irc_nick    = get_str_option("irc", "nick", "test12341234");
      irc_mqtt_tx = get_str_option("irc", "mqtt_tx", "irc/tx");
      irc_mqtt_rx = get_str_option("irc", "mqtt_rx", "irc/rx");
      irc_channel = "#" + get_str_option("irc", "channel", "test4444");
      irc_port    = get_int_option("irc", "port", 6667);
      irc_nickserv_password =  get_str_option("irc", "nickserv_password", "");
      irccon = NULL;
    }
    
    ~nh_irc() 
    {
      if (irccon != NULL)
      {
        delete irccon;
      }
    }
   
    int irc_connect()
    {
      if (_mosq == NULL)
      {
        log->dbg ("Must connect to mosquitto before IRC");
        return -1;
      }
      
      log->dbg("Connecting to irc...");
      irccon = new irc(irc_server, irc_port, irc_nick, irc_nickserv_password, log);
      
      if (irccon->ircConnect())
      {
        log->dbg("Failed to connect to IRC server");
        return -1;
      }
  
      irccon->join(irc_channel); 
      irccon->addCallback("", &irc_callback, this);
      log->dbg("Connected to irc.");
      subscribe(irc_mqtt_tx);      
      subscribe(irc_mqtt_tx + "/#");
      return 0;
    }
    
  static int irc_callback(string user, string channel, string message, void *obj)
  {
    nh_irc *m;
    
    // "channel" is either the channel name (e.g. '#nottinghack'), or an irc nick
    // if the bot was privmsg'd (e.g. 'daniel1111')
    
    m = (nh_irc*)obj;
    
    if ((user=="") && (channel=="INTERNAL") && (message=="DISCONNECTED"))
    {
      // Disconnected from IRC, so send ourselves a reset message
      m->log->dbg("Disconnected from IRC!");
      m->message_send(m->_mqtt_rx, "TERMINATE");      
      return NULL;
    }
      
    // If the bot is sent a private message, publish to /pm/<nick>, if 
    // it's a chat message in the channel, send to /<channel>/<nick>.
    if (m->_mosq_connected)
    {
      if (channel.substr(0,1)=="#")
        m->message_send(m->irc_mqtt_rx + "/" + channel.substr(1) + "/" + user, message);
      else
        m->message_send(m->irc_mqtt_rx + "/pm/" + user, message);
    }
        
    return NULL;
  }    
    
  void process_message(string topic, string message)
  {
    string nick_chan;
    
    if ((irc_mqtt_tx.length() <= topic.length()) && (irc_mqtt_tx == topic.substr(0, irc_mqtt_tx.length())))
    {
      // Send to the channel
      if (topic == irc_mqtt_tx)
      {
        irccon->send(message);
        return;
      }
    
      // remove leading nh/irc/tx/
      nick_chan = topic.substr(irc_mqtt_tx.length()+1);
      if (nick_chan.length() < 2) 
      {
        log->dbg("Ignoring <2 char nick/chan");
        return;
      }
      
      // PM
      if (nick_chan.substr(0, 2) == "pm")
      {
        // must be in the format nh/irc/tx/pm/<nick>
        
        if (nick_chan.length() <= 4)
        {
          log->dbg("Ignoring nick/chan <= 4 char");
          return;
        } else
        {
          irccon->send(nick_chan.substr(3), message); // skip over "pm/"
        } 
      } else
      {    
        // Msg specific channel (for future use if the bot can ever join more than one...)
        irccon->send("#" + nick_chan,  message);
      }
    }
 
    CNHmqtt::process_message(topic, message);
  }
  
};

int main(int argc, char *argv[])
{
  nh_irc nh = nh_irc(argc, argv);
 
  nh_irc::daemonize();
  
  while (nh.mosq_connect())
    sleep(10);
    
  while (nh.irc_connect())
    sleep(10);
    
  nh.message_loop();
  
  return 0;
}