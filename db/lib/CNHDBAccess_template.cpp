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
 * 3. Neither the name of the owner nor the names of its
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

#include "CNHDBAccess.h"
#include <string.h>
#include <iostream>
#include <cstdio>
#include <string>
#include <stdlib.h>
#include "../../CLogging.h"
#include <sstream>

string itos(int n);

CNHDBAccess::CNHDBAccess(string server, string username, string password, string database, CLogging *log)
{
  CNHDBAccess::username = username;
  CNHDBAccess::password = password;
  CNHDBAccess::database = database;
  CNHDBAccess::server   = server; 
  CNHDBAccess::log      = log; 
  connected = false;
  pthread_mutex_init (&mysql_mutex, NULL);
}

CNHDBAccess::~CNHDBAccess()
{
  if (connected)
    dbDisconnect();
}
    
int CNHDBAccess::dbConnect()
{
  
  if (mysql_init(&mysql) != NULL)
    connected = true;
  
  my_bool reconnect = 1; 
  mysql_options(&mysql, MYSQL_OPT_RECONNECT, &reconnect);
  
  
  if (!mysql_real_connect(&mysql,server.c_str(),username.c_str(),password.c_str(),database.c_str(),0,0,0))
  {
    log->dbg("Error connecting to MySQL:" + (string)mysql_error(&mysql));
    return -1;
  }
  
  return 0;
}

void CNHDBAccess::dbDisconnect()
{
  mysql_close(&mysql);
  connected = false;
  return;
}

// {AUTOGENERATED-SP-CALLS}
 
int CNHDBAccess::exec_sp (string sp_name, int param_dir[], int param_type[], void **param_value, int param_length[], int param_count)
{
  MYSQL_STMT    *stmt;
  string        myQuery;
  
  MYSQL_BIND    *bind;
  my_bool       *is_null;
  my_bool       *error;
  unsigned long *length; 
  
  void          *buf[param_count];
  
  int count;
  
  bind    = (MYSQL_BIND*) malloc(sizeof(MYSQL_BIND) * param_count);
  is_null = (my_bool*) malloc(sizeof(my_bool) * param_count);
  error   = (my_bool*) malloc(sizeof(my_bool) * param_count);
  length  = (unsigned long*) malloc(sizeof(unsigned long) * param_count);
  
  memset(bind, 0,sizeof(MYSQL_BIND) * param_count);
  memset(is_null, 0,sizeof(my_bool) * param_count);
  memset(error, 0,sizeof(my_bool) * param_count);
  memset(length, 0,sizeof(unsigned long) * param_count);
  memset(buf, 0, sizeof(buf));

  count = 0;
  for (int n=0; n < param_count; n++)
  {
    if (param_dir[n] == P_DIR_IN)
    {
      if (param_type[n] == P_TYPE_VARCHAR)
      {
        bind[count].buffer_type= MYSQL_TYPE_STRING;
        bind[count].buffer= (char *)  (((string*)(param_value[n]))->c_str());
        bind[count].buffer_length= (((string*)(param_value[n]))->length());
        bind[count].is_null= 0;      
      } else if (param_type[n] == P_TYPE_INT)
      {
        bind[count].buffer_type= MYSQL_TYPE_LONG;
        bind[count].buffer= (char *) param_value[n];
        bind[count].buffer_length= 0; //param_length[n];
        bind[count].is_null= 0;      
      } else if (param_type[n] == P_TYPE_FLOAT)
	{
        bind[count].buffer_type= MYSQL_TYPE_FLOAT;
        bind[count].buffer= (char *) param_value[n];
        bind[count].buffer_length= 0; //param_length[n];
        bind[count].is_null= 0;      
	}
      count++;      
    }
  }
  
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    goto exec_sp_exit_fail;    
  
  myQuery = "call " + sp_name + "(";
  for (int n=0; n < param_count; n++)
  {
    if (param_dir[n] == P_DIR_IN)
      myQuery += "?";
    else
      myQuery += "@" + itos(n);
    
    if (n < (param_count-1))
      myQuery += ",";
  }
  myQuery += ")";
  
  log->dbg("SQL = [" + myQuery + "]");
  
  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed: " + (string)mysql_error(&mysql));
    log->dbg("sql = [" + myQuery + "]"); 
    goto exec_sp_exit_fail;    
  }  
  
  if (mysql_stmt_bind_param(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_param failed: " + (string)mysql_error(&mysql));
    mysql_stmt_close(stmt);
    goto exec_sp_exit_fail;    
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute error: " + (string)mysql_error(&mysql));
    mysql_stmt_close(stmt);
    goto exec_sp_exit_fail;
  }
  
  mysql_stmt_close(stmt);   
  
  // If there aren't ant output parameters for the SP, then return success now
  count = 0;
  for (int x=0; x < param_count; x++)
    if (param_dir[x] != P_DIR_IN)
      count++; 
  if (!count)
  {
    free(bind);  
    free(is_null);
    free(error);  
    free(length);    
    return 0;
  }
  
  // Now get any outputs from the SP
  memset(bind, 0,sizeof(MYSQL_BIND) * param_count);
  stmt = mysql_stmt_init(&mysql);
  if (!stmt)
    goto exec_sp_exit_fail;   
  
  myQuery = "select ";
  for (int n=0; n < param_count; n++)
  {
    if (param_dir[n] != P_DIR_IN)
    {
      myQuery += "@" + itos(n);
    
      if (n < (param_count-1))
        myQuery += ",";
    }
  }
  
  log->dbg("SQL = [" + myQuery + "]");  

  if (mysql_stmt_prepare(stmt, myQuery.c_str(), myQuery.length()))
  {
    log->dbg("mysql_stmt_prepare failed2: " + (string)mysql_error(&mysql));
    log->dbg("sql = [" + myQuery + "]");
    goto exec_sp_exit_fail;
  }
 
  if (mysql_stmt_execute(stmt))
  {
    log->dbg("mysql_stmt_execute error2: " + (string)mysql_error(&mysql));
    mysql_stmt_close(stmt);
    goto exec_sp_exit_fail;
  }  
  
  count = 0;
  for (int n=0; n < param_count; n++)
  {
    if (param_dir[n] != P_DIR_IN)
    {  
      if (param_type[n] == P_TYPE_VARCHAR)
      {      
        buf[n] = malloc(param_length[n]+1); // +1 for null term.
        memset(buf[n], 0, param_length[n]+1);

        bind[count].buffer_type= MYSQL_TYPE_STRING;
        bind[count].buffer= buf[n];
        bind[count].buffer_length= param_length[n];
        bind[count].is_null= &is_null[n];
        bind[count].length= &length[n];
        bind[count].error= &error[n];
      }
      else if (param_type[n] == P_TYPE_INT)
      {
        buf[n] = malloc(sizeof(long));
        memset(buf[n], 0, sizeof(long));
        
        bind[count].buffer_type= MYSQL_TYPE_LONG;
        bind[count].buffer= buf[n];
        bind[count].buffer_length= sizeof(long);
        bind[count].is_null= &is_null[n];
        bind[count].length= &length[n]; // 0
        bind[count].error= &error[n];        
      }
      else if (param_type[n] == P_TYPE_FLOAT)
      {
		  buf[n] = malloc(sizeof(float));
		  memset(buf[n], 0, sizeof(float));
		  
		  bind[count].buffer_type= MYSQL_TYPE_FLOAT;
		  bind[count].buffer= buf[n];
		  bind[count].buffer_length= sizeof(float);
		  bind[count].is_null= &is_null[n];
		  bind[count].length= &length[n]; // 0
		  bind[count].error= &error[n];        
      }
      count++;
    }
  }

  if (mysql_stmt_bind_result(stmt, bind))
  {
    log->dbg("mysql_stmt_bind_result failed: " + (string)mysql_error(&mysql));
    mysql_stmt_close(stmt);
    goto exec_sp_exit_fail;
  }
  
  if (mysql_stmt_store_result(stmt))
  {
    log->dbg("mysql_stmt_store_result failed: " + (string)mysql_error(&mysql));
    mysql_stmt_close(stmt);
    goto exec_sp_exit_fail;
  }  
  
  while (!mysql_stmt_fetch(stmt))
  { 
    for (int n=0; n < param_count; n++)
      if (param_dir[n] != P_DIR_IN)
      {  
        if (param_type[n] == P_TYPE_VARCHAR)    
          *((string*)(param_value[n])) = (char*)buf[n];
        
        if (param_type[n] == P_TYPE_INT)    
           *((int*)(param_value[n])) = (*((long*)buf[n]));       
		  
		if (param_type[n] == P_TYPE_FLOAT)    
			*((float*)(param_value[n])) = (*((float*)buf[n]));  
      }    
      
      mysql_stmt_free_result(stmt); 
  }
 
  mysql_stmt_close(stmt); 
 
  // Free mysql output buffers
  for (int n=0; n < param_count; n++)
    if (param_dir[n] != P_DIR_IN)
      if (buf[n]!=NULL) free(buf[n]);
    
  free(bind);  
  free(is_null);
  free(error);  
  free(length);
  return 0;
  
  exec_sp_exit_fail:
    // Free mysql output buffers
    for (int n=0; n < param_count; n++)
      if (param_dir[n] != P_DIR_IN)
        if (buf[n]!=NULL) free(buf[n]);
    
    free(bind);  
    free(is_null);
    free(error);  
    free(length); 
    return -1;
  
}
/*
int main(int argc, char *argv[])
{
  CNHDBAccess *db;
  CLogging *log;
  log = new CLogging();
  string a;
  string b;
  string c;
  int ret;
  
  db = new CNHDBAccess("localhost", "gk", "gk", "instrumentation", log);   
  db->dbConnect();
  
  ret = db->sp_check_pin ("123", a, b, c);
  
  log->dbg("a=" + a);
  log->dbg("b=" + b);
  log->dbg("c=" + c);
}

*/
string itos(int n)
{
  string s;
  stringstream out;
  out << n;
  return out.str();
}

