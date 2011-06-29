#include "CNHmqtt.h"
#include "GateKeeper_dbaccess.h"

#include <stdio.h>

bool CNHmqtt::debug_mode = false;
bool CNHmqtt::daemonized = false; 

class GateKeeper : public CNHmqtt
{
  public:
    string door_buzzer;
    string door_contact;
    string door_button;
    string rfid;
    string unlock;
    string keypad;
    string door_bell_msg;
    int bell_duration;
    CDBAccess *db;

    GateKeeper(int argc, char *argv[]) : CNHmqtt(argc, argv)
    {
      irc_in = get_str_option("gatekeeper", "irc_in", "irc/rx");
      irc_out = get_str_option("gatekeeper", "irc_out", "irc/tx");
      door_buzzer = get_str_option("gatekeeper", "door_buzzer", "nh/gk/buzzer");
      bell_duration = get_int_option("gatekeeper", "bell_duration", 100);
      door_contact = get_str_option("gatekeeper", "door_contact", "nh/gk/contact");
      door_button = get_str_option("gatekeeper", "door_button", "nh/gk/button");
      rfid = get_str_option("gatekeeper", "rfid", "nh/gk/RFID");
      unlock = get_str_option("gatekeeper", "unlock", "nh/gk/Unlock");
      keypad = get_str_option("gatekeeper", "keypad", "nh/gk/Keypad");
      door_bell_msg = get_str_option("gatekeeper", "door_bell_msg", "Door Bell");

      db = new CDBAccess(get_str_option("mysql", "server", "localhost"), get_str_option("mysql", "username", "gatekeeper"), get_str_option("mysql", "password", "gk"), get_str_option("mysql", "database", "gk"), log);   
    }
    
    ~GateKeeper()
    {
      delete db;
    }
    
    void process_message(string topic, string message)
    {
      pthread_attr_t tattr;
      pthread_t bell_thread;
      string unlock_text;
      irc_dest dst;
      
      // Deal with messages from IRC
      if (is_irc(topic, &dst))
      {   
          if (message=="!help")
          {
            irc_send("!bell - Ring doorbell in hackspace", dst);
          }
          
          if (message=="!bell")
          {
            pthread_attr_init(&tattr);
            pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
            pthread_create(&bell_thread, &tattr, &ring_bell, this);
          } 
      }      
      
      if (topic==door_contact)
      {
        if (message=="LOW")
          message_send(irc_out, "Door opened");
        else if (message=="HIGH")
          message_send(irc_out, "Door closed");
        else message_send(irc_out, message); // If not HIGH or LOW, just pass the message on verbatim
      }   
          
      if (topic==door_button)
      {
        // v1 - actaully ring the bell
        if (message=="HIGH")
          message_send(door_buzzer, "HIGH");
        else if (message=="LOW")
          message_send(door_buzzer, "LOW");
        
       // v2 - output to irc
       if (message=="BING")
       {
         message_send(irc_out, door_bell_msg);
         
         // Now ring the bell
         pthread_attr_init(&tattr);
         pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
         pthread_create(&bell_thread, &tattr, &ring_bell, this);         
       }
       
      }          
      
      if (topic==rfid)
      {
        if (message == "Unknown Card Type")
        {
          message_send(unlock, "Unknown Card Type");
        } else
        {
          if(db->validate_rfid_tag(message, unlock_text))
          {
            // access denied
            db->log_rfid_access(message, ACCESS_DENIED);
            message_send(unlock, "Access denied");
          } else
          {
            // Ok - unlock
            db->log_rfid_access(message, ACCESS_GRANTED);   
            message_send(unlock, "Unlock:" + unlock_text);
          }
        }
      }
      
      if (topic==keypad)
      {
        db->sp_check_pin(message, unlock_text);
        message_send(unlock, unlock_text);
      }
        
      CNHmqtt::process_message(topic, message);
    }
  
  int db_connect()
  {
    db->dbConnect();
    return 0;
  }

  static void *ring_bell(void *arg)
  {
    GateKeeper *gk;
    int n;
      
    gk = (GateKeeper*) arg;
    
    // Ring door bell
    gk->message_send(gk->door_buzzer, "HIGH");
          
    for (n=0; n < (gk->bell_duration); n++)
      usleep(1000); // 1ms
          
    gk->message_send(gk->door_buzzer, "LOW");
    
    return NULL;
  }
    
    void setup()
    {
      subscribe(irc_in + "/#");
      subscribe(door_contact);
      subscribe(door_button);
      subscribe(rfid);
      subscribe(keypad);
    }
};



int main(int argc, char *argv[])
{
 
  string nick;
  string channel;

  string handle="";
  GateKeeper nh = GateKeeper(argc, argv);
  
  
  nh.db_connect();  
  
  GateKeeper::daemonize(); // will only work on first run
  
  nh.mosq_connect();
  nh.setup();
  nh.message_loop(); 
  return 0;
  
}