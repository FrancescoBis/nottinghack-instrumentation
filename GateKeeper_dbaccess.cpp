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

#include "GateKeeper_dbaccess.h"
#include "CNHmqtt.h"
#include <string.h>
#include <cstdio>
#include <string>
#include <stdlib.h>

CDBAccess::CDBAccess(string server, string username, string password, string database, CLogging *log)
{
  CDBAccess::username = username;
  CDBAccess::password = password;
  CDBAccess::database = database;
  CDBAccess::server   = server; 
  CDBAccess::log      = log; 
}

int CDBAccess::dbConnect()
{
  
  mysql_init(&mysql);
  
  my_bool reconnect = 1; 
  mysql_options(&mysql, MYSQL_OPT_RECONNECT, &reconnect);
  
  
  if (!mysql_real_connect(&mysql,server.c_str(),username.c_str(),password.c_str(),database.c_str(),0,0,0))
  {
    log->dbg("Error connecting to MySQL:" + (string)mysql_error(&mysql));
    return -1;
  }
  
  return 0;
}

void CDBAccess::dbDisconnect()
{
  mysql_close(&mysql);
  return;
}

int CDBAccess::validate_rfid_tag(string rfid_serial, string &unlock, string &handle)
{
  MYSQL_BIND    bind[2];
  MYSQL_STMT    *stmt;
  char          str_handle[1000];
  char          str_unlock[1000];
  my_bool       is_null[2];
  my_bool       error[2];
  unsigned long length[2];
  string        myQuery;
  bool          serial_found = false;
  
  memset(bind, 0, sizeof(bind));

  // rfid serial
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)rfid_serial.c_str();
  bind[0].buffer_length= rfid_serial.length();
  bind[0].is_null= 0;  
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    return -1;   
  
  myQuery = "select m.unlock_text, m.handle from members m inner join rfid_tags r on r.member_id = m.member_id where r.state = " + CNHmqtt::itos(STATE_ACTIVE) + " and r.rfid_serial = ?";
  
  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed");
    log->dbg("sql = [" + myQuery + "]");
    return -1;  
  }
 
  if (mysql_stmt_bind_param(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_param failed");
    return -1;
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute error: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  // unlock text column
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)str_unlock;
  bind[0].buffer_length= sizeof(str_unlock);
  bind[0].is_null= &is_null[0];
  bind[0].length= &length[0];
  bind[0].error= &error[0];
  
  // unlock text column
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)str_handle;
  bind[1].buffer_length= sizeof(str_handle);
  bind[1].is_null= &is_null[1];
  bind[1].length= &length[1];
  bind[1].error= &error[1];  
  
  if (mysql_stmt_bind_result(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_result failed: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  if (mysql_stmt_store_result(stmt))
  {
    log->dbg("mysql_stmt_store_result failed: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  
  while (!mysql_stmt_fetch(stmt))
  {
    serial_found = true;
    handle = str_handle;
    unlock = str_unlock;
  }

  mysql_stmt_close(stmt);
  
  if (serial_found)
    return 0;
  else
    return 1;
}

int CDBAccess::validate_pin(string pin, string &unlock_text)
{
  MYSQL_BIND    bind[1];
  MYSQL_STMT    *stmt;
  char          str_data[1000];
  my_bool       is_null[1];
  my_bool       error[1];
  unsigned long length[1];
  string        myQuery;
  bool          pin_found = false;
  
  memset(bind, 0, sizeof(bind));

  // pin
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)pin.c_str();
  bind[0].buffer_length= pin.length();
  bind[0].is_null= 0;  
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    return -1;   
  }   
  
  myQuery = "select p.unlock_text from pins p where p.state = " + CNHmqtt::itos(STATE_ACTIVE) + " and p.pin = ?";
  
  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    log->dbg("sql = [" + myQuery + "]");
    return -1;  
  }
 
  if (mysql_stmt_bind_param(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_param failed: " + (string)mysql_error(&mysql));
    return -1;
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute error: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  // Handle column
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)str_data;
  bind[0].buffer_length= sizeof(str_data);
  bind[0].is_null= &is_null[0];
  bind[0].length= &length[0];
  bind[0].error= &error[0];
  
  if (mysql_stmt_bind_result(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_result failed: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  if (mysql_stmt_store_result(stmt))
  {
    log->dbg("mysql_stmt_store_result failed: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  
  while (!mysql_stmt_fetch(stmt))
  {
    pin_found = true;
    unlock_text = str_data;
  }

  mysql_stmt_close(stmt);
  
  if (pin_found)
    return 0;
  else
    return 1;
}

int CDBAccess::log_rfid_access(string rfid, int access)
{
  MYSQL_BIND bind[2];
  MYSQL_STMT  *stmt;
  string myQuery;
  
  memset(bind, 0, sizeof(bind));
  
  // rfid serial
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)rfid.c_str();
  bind[0].buffer_length= rfid.length();
  bind[0].is_null= 0; 
  
  // access result
  bind[1].buffer_type=MYSQL_TYPE_LONG;
  bind[1].buffer= (char *)&access;
  bind[1].buffer_length= 0;
  bind[1].is_null= 0;
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    return -1; 
  
  myQuery = "insert into access_log (rfid_serial, access_result) values (?, ?);";
  
  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    return -1;
  };
  
  if (mysql_stmt_bind_param(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_param failed: " + (string)mysql_error(&mysql));
    return -1;
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute failed: " + (string)mysql_error(&mysql));
    return -1;  
  }
  
  mysql_stmt_close(stmt);  
  
  return 0;
}

int CDBAccess::sp_check_pin(string pin, string &unlock_string, string &handle_string)
{
  MYSQL_BIND    bind[3];
  char          unlock_text[200];
  char          err_text[200];
  char          handle_text[110];
  my_bool       is_null[3];
  my_bool       error[3];
  unsigned long length[3];  
  MYSQL_STMT    *stmt;
  string        myQuery;
  
  memset(bind, 0, sizeof(bind));
  memset(unlock_text, 0, sizeof(unlock_text));
  memset(handle_text, 0, sizeof(handle_text));
  memset(err_text, 0, sizeof(err_text));
  handle_string = "";  
  
  // PIN
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)pin.c_str();
  bind[0].buffer_length= pin.length();
  bind[0].is_null= 0;  
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    return -1;   
  
  myQuery = "call sp_check_pin(?, @unlock_text, @handle, @err)";

  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    log->dbg("sql = [" + myQuery + "]");
    return -1;  
  }
 
  if (mysql_stmt_bind_param(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_param failed: " + (string)mysql_error(&mysql));
    return -1;
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute error: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  mysql_stmt_close(stmt); 
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    return -1;   
  
  myQuery = "select @unlock_text, @handle, @err;";

  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    log->dbg("sql = [" + myQuery + "]");
    return -1;  
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute error2: " + (string)mysql_error(&mysql));
    return -1;
  }
     
  // Outputs
  
  // Unlock text
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)unlock_text;
  bind[0].buffer_length= sizeof(unlock_text);
  bind[0].is_null= &is_null[0];
  bind[0].length= &length[0];
  bind[0].error= &error[0];
  
  // Handle
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)handle_text;
  bind[1].buffer_length= sizeof(handle_text);
  bind[1].is_null= &is_null[1];
  bind[1].length= &length[1];
  bind[1].error= &error[1];  
 
  // error
  bind[2].buffer_type= MYSQL_TYPE_STRING;
  bind[2].buffer= (char *)err_text;
  bind[2].buffer_length= sizeof(err_text);
  bind[2].is_null= &is_null[2];
  bind[2].length= &length[2];
  bind[2].error= &error[2];  
  
  if (mysql_stmt_bind_result(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_result failed: " + (string)mysql_error(&mysql));
    return -1;
  }
  
  if (mysql_stmt_store_result(stmt))
  {
    log->dbg("mysql_stmt_store_result failed: " + (string)mysql_error(&mysql));
    return -1;
  }  
 
  while (!mysql_stmt_fetch(stmt))
  {
    log->dbg("unlock text=[" + (string)unlock_text + "],  err=[" + (string)err_text + "]");
  } 
  
  mysql_stmt_close(stmt); 
  
  unlock_string = unlock_text;
  handle_string = handle_text;
  log->dbg("Handle = [" + handle_string + "]");
 
  return 0;
  
}

int CDBAccess::log_pin_access(string pin, int access)
{
  MYSQL_BIND bind[2];
  MYSQL_STMT  *stmt;
  string myQuery;
  
  memset(bind, 0, sizeof(bind));
  
  // pin
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)pin.c_str();
  bind[0].buffer_length= pin.length();
  bind[0].is_null= 0; 
  
  // access result
  bind[1].buffer_type=MYSQL_TYPE_LONG;
  bind[1].buffer= (char *)&access;
  bind[1].buffer_length= 0;
  bind[1].is_null= 0;
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    return -1; 
  
  myQuery = "insert into access_log (pin, access_result) values (?, ?);";
  
  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    return -1;
  };
  
  if (mysql_stmt_bind_param(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_param failed: " + (string)mysql_error(&mysql));
    return -1;
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute failed: " + (string)mysql_error(&mysql));
    return -1;  
  }
  
  mysql_stmt_close(stmt);  
  
  return 0;
}