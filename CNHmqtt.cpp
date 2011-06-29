#include "CNHmqtt.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>



CNHmqtt::CNHmqtt(int argc, char *argv[])
{
  log = NULL;
  string config_file = "";
  config_file_parsed = false;
  int c;
  std::stringstream out;
  reader = NULL;
  mosq_connected = false;
  log = new CLogging();
  string logfile;
  uid = 0;
  
  debug_mode = false;
    
  /* Read in command line parameters:
   * -c <config file>       specify config file
   * -d                     Do not daemonize & ouput debug info to stdout  s
   * -l <log file>          Specify location to write logfile to TODO
   */
  
  opterr = 0;
  optopt = 1;
  optind = 1;
  while ((c = getopt (argc, argv, "c:dl:")) != -1)
    switch (c)
      {
        case 'c':
          config_file = optarg;
          break;
          
        case 'd':
          log->dbg("Starting in debug mode");
          debug_mode = true;
          break;
        
        case 'l':
          log->dbg("Uncoded -l flag given");
          break;
        
        case '?':
          if (optopt == 'c')
            log->dbg ("Option -c requires an argument.");
          else  
            log->dbg("Unknown option given");
          return;
        
        default:
          log->dbg("CNHmqtt::CNHmqtt ?!?");
      }
      
      
  CNHmqtt::mqtt_topic = "test"; //man_channel;
  CNHmqtt::status = "Running";
  CNHmqtt::mosq_server = "127.0.0.1";
  CNHmqtt::mosq_port = 1883;      
  
  pthread_mutex_init (&mosq_mutex, NULL);
      
  if (config_file != "")
  {
    reader = new INIReader(config_file);
    
    if (reader->ParseError() < 0) 
    {
        log->dbg("Failed to open/parse config file [" + config_file + "]");
    } else
    {
      config_file_parsed = true;
      mosq_server = reader->Get("mqtt", "host", "localhost");
      mosq_port   = reader->GetInteger("mqtt", "port", 1883);
      mqtt_topic  = reader->Get("mqtt", "topic", "test"); 
      logfile     = reader->Get("mqtt", "logfile", ""); 
      uid         = reader->GetInteger("mqtt", "uid", 0);
    }    
  }
  
  // Switch to less privileged user if set
  if (uid)
    if (setuid(uid))
    {
      log->dbg("Failed to switch user!");
      exit(1);
    }
  
  if (!debug_mode)
    if(!log->open_logfile(logfile))
      exit(1);
  
  mqtt_rx = mqtt_topic + "/rx";
  mqtt_tx = mqtt_topic + "/tx";
  
  log->dbg("mosq_server = " + mosq_server);
  out << "" << mosq_port;
  log->dbg("mosq_port = "   + out.str());
  log->dbg("mqtt_tx = "     + mqtt_tx);
  log->dbg("mqtt_rx = "     + mqtt_rx);
  
  terminate = false;
  reset = false;  
}

CNHmqtt::~CNHmqtt()
{

  if (mosq_connected)
  {
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();      
  }
  
  if (reader!=NULL)
  {
    delete reader;
    reader=NULL;
  }
  
  if (log!=NULL)
  {
    delete log;
    log=NULL;
  }
}
  
int CNHmqtt::daemonize()
{
  if (daemonized)
    return 0;
  
  if (debug_mode)
    return 0;
  else
  {
    daemonized = true;
    return  daemon(1, 0); // don't change dir, but do redirect stdio to /dev/null
  }
}

string CNHmqtt::get_str_option(string section, string option, string def_value)
{
  if (config_file_parsed && reader != NULL)
    return reader->Get(section, option, def_value); 
  else
    return def_value;
}

int CNHmqtt::get_int_option(string section, string option, int def_value)
{
  if (config_file_parsed && reader != NULL)
    return reader->GetInteger(section, option, def_value); 
  else
    return def_value;
}

int CNHmqtt::mosq_connect()
{
  std::stringstream out;
  mosquitto_lib_init();
  out << mqtt_topic << "-" << getpid();
  
  log->dbg("Connecting to Mosquitto as [" + out.str() + "]");;
  
  mosq = mosquitto_new(out.str().c_str(), this);  
  if(!mosq)
  {
    cout << "mosquitto_new() failed!";
    return -1;
  }  
  
  mosquitto_connect_callback_set(mosq, CNHmqtt::connect_callback);
  mosquitto_message_callback_set(mosq, CNHmqtt::message_callback);  
  
    // int mosquitto_connect(struct mosquitto *mosq, const char *host, int port, int keepalive, bool clean_session);
  if(mosquitto_connect(mosq, mosq_server.c_str(), mosq_port, 30, false)) 
  {
    log->dbg("mosq_connnect failed!");
    mosq = NULL;
    return -1;
  }
  mosq_connected = true;
    
    
    
  return 0;       
}

void CNHmqtt::connect_callback(void *obj, int result)
{
  CNHmqtt *m = (CNHmqtt*)obj;

  if(!result)
  {  
    m->log->dbg("Connected to mosquitto.");  
    mosquitto_subscribe(m->mosq, NULL, m->get_topic().c_str(), 0);
  } else
  {
    m->log->dbg("Failed to connect to mosquitto!");
  }
}

void CNHmqtt::message_callback(void *obj, const struct mosquitto_message *message)
{
  CNHmqtt *m = (CNHmqtt*)obj;
  
  if(message->payloadlen)
  { 
    m->log->dbg("Got mqtt message, topic=[" + (string)((char *)message->topic) + "], message=[" + (string)((char *)message->payload) + "]");
    m->process_message((char *)message->topic, (char *)message->payload);
  }
}

int CNHmqtt::subscribe(string topic)
{
  log->dbg("Subscribing to topic [" + topic + "]");
  if (topic=="")
  {
    log->dbg("Cannot subscribe to empty topic!");
    return -1;
  }
    
  if(mosquitto_subscribe(mosq, NULL, topic.c_str(), 0))
  {
    log->dbg("Subscribe failed!");
    return -1;
  }
  
  return 0;
}

void CNHmqtt::process_message(string topic, string message)
{
  // Process any messages sent to our management topic (terminate or status)
  if (topic == mqtt_rx)
  {
    if (message == "TERMINATE")
    {
      log->dbg("Terminate message received...");  
      terminate = true;
    }
    
    if (message == "STATUS")
      message_send(mqtt_tx, status);
    
    if (message == "RESET")
    {
      log->dbg("RESET message received...");
      reset = true;
    }
  }
}

int CNHmqtt::message_send(string topic, string message)
{
  int ret;
  log->dbg("Sending message,  topic=[" + topic + "], message=[" + message + "]");
  pthread_mutex_lock(&mosq_mutex);
  ret = mosquitto_publish(mosq, NULL, topic.c_str(), message.length(), (uint8_t*)message.c_str(), 0, false);
  pthread_mutex_unlock(&mosq_mutex);
  return ret;
}

string CNHmqtt::get_topic()
{
  return mqtt_rx;
 
}

int CNHmqtt::message_loop(void)
{
  string dbgmsg="";
  
  if (mosq==NULL)
    return -1;
  
  while(!mosquitto_loop(mosq, 50)  && !terminate && !reset);
  
  if (terminate)
    log->dbg("terminate=true");
  else
    log->dbg("terminate=false");
  
  if (reset)
    log->dbg("reset=true");
  else
    log->dbg("reset=false");  
  
  log->dbg("Exit.");
  mosq_connected = false;
  mosquitto_disconnect(mosq);
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();     
  
  
  if (terminate)
    return 1;
  else if (reset)
    return 2;
  else
    return 0;
}


// Other stuff not mqtt / instrumentation specifc
// Integer to String
string CNHmqtt::itos(int n)
{
  string s;
  stringstream out;
  out << n;
  return out.str();
}

// irc_in = base topic, e.g. "nh/irc/rx"
// topic  = topic actaully received, e.g. "nh/irc/rx/nottinghack/daniel1111
bool CNHmqtt::decode_irc_topic(string irc_in, string topic, string &nick, string &channel)
{    
  
  if (irc_in.length() >= topic.length())
  {
    nick="";
    channel="";
    return false;
  }   
 
  // remove irc_in from front
  topic = topic.substr(irc_in.length()+1);
  
  if (topic.find_first_of("/") != string::npos)
  {
    // topic indicates it's a channel chat message
    channel = topic.substr(0, topic.find_first_of("/"));
    nick = topic.substr(topic.find_first_of("/")+1);
  }
  
  if (channel == "pm") // A 'Channel' of pm means it was actaully a private message
    channel = "";

  return true;    
}

int CNHmqtt::irc_send(string message, irc_dest dst)
{
  if (dst.channel=="")
    return irc_send_nick (message, dst.nick);
  else
    return irc_send_channel (message, dst.channel);
}

int CNHmqtt::irc_send_nick (string message, string nick)
{
  return message_send(irc_out + "/pm/" + nick, message);
}


int CNHmqtt::irc_send_channel (string message, string channel)
{
  return message_send(irc_out + "/" + channel, message);
}

bool CNHmqtt::is_irc(string topic, irc_dest *dst)
{
  if (topic.length() > irc_in.length())
    if(topic.substr(0, irc_in.length())==irc_in)
      return decode_irc_topic(irc_in, topic, dst->nick, dst->channel);

  dst->nick = "";
  dst->channel = "";
  return false;
}
