/*   
 * Copyright (c) 2012, Daniel Swann <hs@dswann.co.uk>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define P_DIR_IN 1
#define P_DIR_OUT 2
#define P_DIR_INOUT 3

#define P_TYPE_INT 1
#define P_TYPE_VARCHAR 2
#define P_TYPE_FLOAT 3
#define P_TYPE_TEXT 4

enum lang 
{
   LANG_CPP_HEAD,
   LANG_CPP_FUNC,
   LANG_PHP
};


struct param 
{
  int  p_direction;
  char p_name[66];
  int  p_type;
  int  p_len;
  struct param *next_param;
};

struct sp_def 
{
  char sp_name[66]; 
  struct param *sp_params; 
  struct sp_def *next_sp;
};

int add_param(struct param  **param_list, char *param_line);
int seek_to(FILE *fp, char *search_str, int case_sensitive);
int generate_sp_function(struct sp_def *sp, FILE *out_imp);
//int generate_sp_header(struct sp_def *sp, FILE *out_hed);
int read_file(char *filename, struct sp_def *sps);
int output_func_def(struct sp_def *sps, FILE *out, int header);
int write_file(char *path, char *template_filename, char *output_filename, char *search_str, struct sp_def *sp_list, enum lang langtyp);
int output_body(FILE *fh_out, struct sp_def *sp_ptr, enum lang langtyp);

int main(int argc, char *argv[])
{
  int n;
  FILE *out_imp;      // Implementation - output .cpp file
  FILE *out_hed;      // Header - output .h file
  FILE *in_imp_tpl;   // Implementation template
  FILE *in_hed_tpl;   // Header template
  char filepath [256];
  int path_end;


  
  struct sp_def *sp_list_base;
  struct sp_def *sp_list_ptr;
  
  if (argc <= 2)
  {
    printf ("Usage:\n\t%s <base dir> <sp_one.sql> <sp_two.sql>....\n", argv[0]);
    return -1;
  }
  
  sp_list_base = NULL;
  sp_list_ptr = NULL;
  
  sp_list_base = sp_list_ptr = malloc(sizeof(struct sp_def));
  memset(sp_list_base, 0, sizeof(struct sp_def));
  
  /* Read in SP definitions */
  for (n=2; n < argc; n++)
  {
    read_file(argv[n], sp_list_ptr);
    
    sp_list_ptr->next_sp = malloc(sizeof(struct sp_def));
    sp_list_ptr = sp_list_ptr->next_sp;
    memset(sp_list_ptr, 0, sizeof(struct sp_def));
  }
  
 // generate_sp_function(sp_list_base, stdout);
  
  /* argv[1] = path */
  write_file(argv[1], "/CNHDBAccess_template.cpp", "/CNHDBAccess.cpp", "// {AUTOGENERATED-SP-CALLS}\n"      , sp_list_base, LANG_CPP_FUNC);
  write_file(argv[1], "/CNHDBAccess_template.h"  , "/CNHDBAccess.h"  , "// {AUTOGENERATED-SP-DEFINITIONS}\n", sp_list_base, LANG_CPP_HEAD);
  write_file(argv[1], "/CNHDBAccess_template.php", "/CNHDBAccess.php", "// {AUTOGENERATED-SP-CALLS}\n"      , sp_list_base, LANG_PHP);
    
      
  
  
  
  /*
  strcpy(filepath, argv[1]);
  path_end = strlen(filepath);
  strcpy(filepath+path_end, "/CNHDBAccess.cpp");
  out_imp = fopen(filepath, "w");
  if (out_imp == NULL)
  {
    printf("Failed to open [%s] for writing\n", filepath);
    return -1;
  }
  
  strcpy(filepath+path_end, "/CNHDBAccess.h");
  out_hed = fopen(filepath, "w");
  if (out_hed == NULL)
  {
    printf("Failed to open [%s] for writing\n", filepath);
    return -1;
  }
  */

  /*
  strcpy(filepath+path_end, "/CNHDBAccess_template.cpp");
  in_imp_tpl = fopen(filepath, "r");
  if (in_imp_tpl == NULL)
  {
    puts("Failed to open CNHDBAccess_template.cpp for reading");
    return -1;
  }
  
  strcpy(filepath+path_end, "/CNHDBAccess_template.h");
  in_hed_tpl = fopen(filepath, "r");
  if (in_hed_tpl == NULL)
  {
    puts("Failed to open CNHDBAccess_template.h for reading");
    return -1;
  }
  
  //copy contents of CNHDBAccess_template.cpp to CNHDBAccess.cpp until "// {AUTOGENERATED-SP-CALLS}" reached
  while (fgets(buf, sizeof(buf), in_imp_tpl)!=NULL)
  {
    if (!strcmp(buf, "// {AUTOGENERATED-SP-CALLS}\n"))
    {
      while (fgets(buf2, sizeof(buf), in_hed_tpl)!=NULL) 
      {
        if (!strcmp(buf2, "// {AUTOGENERATED-SP-DEFINITIONS}\n"))
        {
          for (n=2; n < argc; n++)
            read_file(argv[n], out_imp, out_hed);
        } else
          fputs(buf2, out_hed);
      }
    } else
      fputs(buf, out_imp);
  }
  fclose(in_imp_tpl);
  fclose(out_imp);
 
  */
  return 0;
}

int write_file(char *path, char *template_filename, char *output_filename, char *search_str, struct sp_def *sp_list, enum lang langtyp)
{
  FILE *fh_out;
  FILE *fh_in;
  struct sp_def *sp_ptr;
  char buf[512];
  char buf2[512];
  char file_path[512];
  
  if (sp_list == NULL)
    return -1;
  
  sp_ptr = sp_list;  
  
  strcpy(file_path, path);
  strcpy(file_path+strlen(file_path), output_filename);  
  
  /* Open output file */
  fh_out = fopen(file_path, "w");
  if (fh_out == NULL)
  {
    printf("Failed to open [%s] for writing\n", file_path);
    return -1;
  }

  strcpy(file_path, path);
  strcpy(file_path+strlen(file_path), template_filename);  
  
  
  /* Open template */
  fh_in = fopen(file_path, "r");
  if (fh_in == NULL)
  {
    printf("Failed to open [%s] for reading\n", file_path);
    return -1;
  }    
  
  if (langtyp == LANG_PHP)
    fprintf(fh_out,"<?php\n");    
  fprintf(fh_out," /********************************************/ \n");
  fprintf(fh_out," /***** Auto-generated file DO NOT EDIT! *****/ \n");
  fprintf(fh_out," /********************************************/ \n");    
   
  
  /* copy contents of template to output file until search_str reached */
  while (fgets(buf2, sizeof(buf), fh_in)!=NULL) 
  {
    if (!strcmp(buf2, search_str))
    {
      while (sp_ptr != NULL)
      {
        if (sp_ptr->sp_name[0] != '\0')
          output_body(fh_out, sp_ptr, langtyp);
        sp_ptr = sp_ptr->next_sp;  
      }
    } else
      fputs(buf2, fh_out);
  }

  return 0;
}

int read_file(char *filename, struct sp_def *sps)
{
  FILE *fp;
  char x;
  char *sp_name;
  int n;
  char param_line[256];
  int end_of_param_line;
  int end_of_params;
  int paren;
 // struct param *param_list;
 
  char create_proc_str[] = "CREATE PROCEDURE ";
  
  //param_list = &sps->sp_params;
  sp_name = sps->sp_name;
  
  fp = fopen(filename, "rb");
  if (fp==NULL)
  {
    printf("Failed to open [%s]\n", filename);
    return -1;
  }
  printf("Processing [%s]\n", filename);
  
  if (seek_to (fp, create_proc_str, 0))
    return -1;
  
  // Now read up until the next '(' (or 65 chars, whichever comes first), 
  // and store the result as the sp name
  n=0;
  memset(sp_name, 0, sizeof(sp_name));
  while ((fread(&x, 1, 1, fp) > 0) && (n < 65)) // one char at a time
  {
    if (x == '(')
      break;
    else
      if ( (x != ' ') && (x != '\n') && (x != '\r') )
        sp_name[n++] = x;
  }
  if (x != '(')
  {
    printf("Failed to find sp name!\n");
    return -1;
  }
  
  // read in parameters
  end_of_params=0;
  while (!end_of_params)
  {
    n=0;
    end_of_param_line=0;
    memset(param_line, 0, sizeof(param_line));
    paren=0;
    while ( (fread(&x, 1, 1, fp) > 0) && !end_of_param_line) // one char at a time
    {
      // Keep track of brackets used to specify variable size
      if (x == '(')  
        paren++;
      if ( (x == ')') && paren)
      {        
        paren--;      
        param_line[n++] = x;
        continue;
      }
      
      if ( (x == ',') || (!paren && (x == ')')) )
      {
        end_of_param_line = 1;
        break;
      }
      
      if ( (x == '\r') || (x == '\n')) // don't care about newlines
        continue;

      param_line[n++] = x; 
    }
    
    if ((!end_of_param_line) || (x == ')'))
      end_of_params = 1;

    if(add_param(&sps->sp_params, param_line))
    {
      //TODO: Free param list
      printf("Error adding param - [%s]\n", param_line);
      return -1;
    }
  }
/*
  //generate_sp_function(sp_name, param_list, out_imp);
  printf("---\n");
  output_func_def(sp_name, sps->sp_params, stdout, 1);
  */
  // TODO: Free param list
  return 0;
    
}

int seek_to(FILE *fp, char *search_str, int case_sensitive)
{
  char *buf;
  int buf_len;
  int x;
  int found;
  
  found = 0;
  
  buf_len = strlen(search_str) + 1; // +1 for null terminator
  
  buf = malloc(buf_len);
  memset(buf, 0, buf_len);
  
  if (fread(buf, buf_len-1, 1, fp) <= 0)
  {
    free(buf);
    printf("Error - EOF reached searching for [%s]\n", search_str);
    return -2; // EOF;
  }
  
  if (case_sensitive)
  {
    if (!strcmp(buf, search_str))
    {
      free(buf);
      return 0;
    } else
    if (!strcasecmp(buf, search_str))
    {
      free(buf);
      return 0;
    }
  }

  while (!found)
  {
    for (x = 1; x < (buf_len-1); x++)
        buf[x-1] = buf[x];
    
    if (fread(&buf[buf_len-2], 1, 1, fp) <= 0)
    {
      free(buf);
      printf("Error - EOF reached searching for [%s]\n", search_str);
      return -2; // EOF;
    }  
    
    if (case_sensitive)
    {
      if (!strcmp(buf, search_str))
        found = 1;
    } else
    {    
      if (!strcasecmp(buf, search_str)) 
        found = 1;
    }    
  }
  
  free(buf);
  if (found)
    return 0;
  else
  {
    printf("Error - EOF reached searching for [%s]\n", search_str);
    return -2; // EOF
  }
}

int add_param(struct param **param_list, char *param_line)
{
  int line_len;
  char *ptr;
  int n;
  struct param *new_param;
  struct param *lst;
  
  int  p_type;
  int  p_dir;
  int  p_len;
  char p_len_str[32];
  char p_name[66];
  
  memset(p_name, 0, sizeof(p_name));
  memset(p_len_str, 0, sizeof(p_len_str));
  line_len = strlen(param_line);
  ptr = param_line;
  
  // Skip over any leading white space
  while ((*ptr==' ') && (ptr<(param_line+line_len)))
    ptr++;
  
  // Set direction
  if (!strncasecmp(ptr, "IN", 2))
  {
    p_dir = P_DIR_IN;    
    ptr += 3;
  }
  else if (!strncasecmp(ptr, "OUT", 3))
  {
    p_dir = P_DIR_OUT;
    ptr += 3;
  }
  else if (!strncasecmp(ptr, "INOUT", 5))
  {
    ptr += 5;
    p_dir = P_DIR_INOUT;
  }
  else
  {
    printf ("Unknown direction! (line=[%s])\n", param_line);
    return -1;
  }
  
  // Skip over any white space
  while ((*ptr==' ') && (ptr<(param_line+line_len)))
    ptr++;
  
  // Set variable name
  n=0;
  while ((*ptr != ' ') && (ptr<(param_line+line_len)))
    p_name[n++] = *ptr++;
 
  // Skip over any white space
  while ((*ptr==' ') && (ptr<(param_line+line_len)))
    ptr++;  
  
  // Set variable type
  if (!strncasecmp(ptr, "int", 3))
  {
    p_type = P_TYPE_INT;    
    ptr += 3;
  }
  else if (!strncasecmp(ptr, "varchar", 7))
  {
    p_type = P_TYPE_VARCHAR;
    ptr += 7;
  }
  else if (!strncasecmp(ptr, "text", 4))
  {
    p_type = P_TYPE_TEXT;
    ptr += 4;
  }  
  else if (!strncasecmp(ptr, "float", 5))
  {
    p_type = P_TYPE_FLOAT;
    ptr += 5;
  }
  else
  {
    printf ("Unknown variable type! (line=[%s])\n", param_line);
    return -1;
  }  
  
  // Get vaiable length (if present)
  if (*ptr++=='(')
  {
    n=0;
    while ((*ptr!=')') && (ptr<(param_line+line_len)))
      p_len_str[n++] = *ptr++;
    
    p_len = atoi(p_len_str);
  } else
    p_len = 0;
    
  // Now add param to list.
  new_param = malloc(sizeof(struct param));
  memset(new_param, 0, sizeof(new_param));
  new_param->p_direction = p_dir;
  memcpy(new_param->p_name, p_name, sizeof(new_param->p_name));
  new_param->p_type = p_type;
  new_param->p_len = p_len;
  new_param->next_param = NULL;
  
  // No list yet, so this entry becomes the head
  if (*param_list == NULL)
  {
    *param_list = new_param;
    return 0;
  }
  
  // Find end of list and add new param there
  lst = *param_list;
  while (lst->next_param != NULL)
    lst = lst->next_param;
  lst->next_param = new_param;
  
  //printf("dir = %d, param_name = [%s], type = %d. len_str = %s, len = %d\n", p_dir, p_name, p_type, p_len_str, p_len);
  
  return 0;
}

int output_func_def(struct sp_def *sps, FILE *out, int header)
{
  char *sp_name;
  struct param *param_list;
  struct param *lst;
  int param_count;
  
  sp_name = sps->sp_name;
  param_list = sps->sp_params;
  
  param_count = 0;
  
  // Output function definition
  if (header)
    fprintf (out, "    int %s (", sp_name);
  else
    fprintf (out, "int CNHDBAccess::%s (", sp_name);
  
  if (param_list != NULL)
  {
    lst = param_list;
    do
    {
      if (lst->p_type == P_TYPE_INT) 
        fprintf(out, "int ");
      else if ((lst->p_type == P_TYPE_VARCHAR))
        fprintf(out, "string ");
      else if ((lst->p_type == P_TYPE_FLOAT))
        fprintf(out, "float ");
      else if ((lst->p_type == P_TYPE_TEXT))
        fprintf(out, "string ");      
      else 
        return -1;

      if (lst->p_direction == P_DIR_IN) 
      {} //printf("");
      else if ((lst->p_direction == P_DIR_OUT))
        fprintf(out, "&");
      else if ((lst->p_direction == P_DIR_INOUT))
        fprintf(out, "&");      
      else 
        return -1;      
   
      
      if (lst->next_param == NULL)
        fprintf(out, "%s", lst->p_name);
      else
        fprintf(out, "%s, ", lst->p_name);
        
      lst = lst->next_param;
      param_count++;
    } while (lst != NULL);  
    
    if (header)
      fprintf(out, ");\n");
    else
      fprintf(out, ")\n");
  }
  
  return param_count;
}

int generate_sp_function(struct sp_def *sp, FILE *out_imp)
{
  char *sp_name;
  struct param *param_list;
  
  struct param *lst;
  int param_count;
  param_count = 0;
  
  sp_name = sp->sp_name;
  param_list = sp->sp_params;
    
  param_count = output_func_def(sp, out_imp,0);
  fprintf(out_imp, "{\n");
  
  // Function body
  fprintf(out_imp, "  int param_type[%d];\n"   , param_count);  
  fprintf(out_imp, "  int param_dir[%d];\n"    , param_count);
  fprintf(out_imp, "  void *param_value[%d];\n", param_count);
  fprintf(out_imp, "  int param_len[%d];\n"    , param_count);
  fprintf(out_imp, "  int retval;\n");   
  fprintf(out_imp, "\n");
  
  param_count = 0;
  if (param_list != NULL)
  {
    lst = param_list;
    do
    {
      if (lst->p_type == P_TYPE_INT) 
        fprintf(out_imp, "  param_type[%d] =  P_TYPE_INT;\n", param_count);
      else if ((lst->p_type == P_TYPE_VARCHAR))
        fprintf(out_imp, "  param_type[%d] =  P_TYPE_VARCHAR;\n", param_count);
      else if ((lst->p_type == P_TYPE_FLOAT))
        fprintf(out_imp, "  param_type[%d] =  P_TYPE_FLOAT;\n", param_count);
      else if ((lst->p_type == P_TYPE_TEXT))
        fprintf(out_imp, "  param_type[%d] =  P_TYPE_TEXT;\n", param_count);      
      else 
        return -1;

      if (lst->p_direction == P_DIR_IN) 
        fprintf(out_imp, "  param_dir[%d] = P_DIR_IN;\n", param_count);
      else if ((lst->p_direction == P_DIR_OUT))
        fprintf(out_imp, "  param_dir[%d] = P_DIR_OUT;\n", param_count);
      else if ((lst->p_direction == P_DIR_INOUT))
        fprintf(out_imp, "  param_dir[%d] = P_DIR_INOUT;\n", param_count);   
      else 
        return -1;      
   
       fprintf(out_imp, "  param_value[%d] = &%s;\n", param_count, lst->p_name);
       fprintf(out_imp, "  param_len[%d] = %d;\n", param_count, lst->p_len);
       fprintf(out_imp, "\n");
        
      lst = lst->next_param;
      param_count++;
    } while (lst != NULL);
    
    fprintf(out_imp, "  pthread_mutex_lock(&mysql_mutex);\n");
    fprintf(out_imp, "  retval = exec_sp(\"%s\", param_dir, param_type, param_value, param_len, %d);\n", sp_name, param_count);
    fprintf(out_imp, "  pthread_mutex_unlock(&mysql_mutex);\n");
    fprintf(out_imp, "  return retval;\n");
    fprintf(out_imp, "}\n\n");
  }   
  
  return 0;
}

int generate_sp_function_php(struct sp_def *sp, FILE *fh_out)
{
  char *sp_name;
  struct param *param_list;
  
  struct param *lst;
  int param_count;
  param_count = 0;
  
  sp_name = sp->sp_name;
  param_list = sp->sp_params;
    
  // Output function definition
  fprintf (fh_out, "  function %s (", sp_name);
  if (param_list != NULL)
  {
    lst = param_list;
    do
    {
      if (lst->p_direction == P_DIR_IN) 
        fprintf(fh_out, "$");
      else if ((lst->p_direction == P_DIR_OUT))
        fprintf(fh_out, "&$");
      else if ((lst->p_direction == P_DIR_INOUT))
        fprintf(fh_out, "&$");      
      else 
        return -1;      
         
      fprintf(fh_out, "%s, ", lst->p_name);
        
      lst = lst->next_param;
      param_count++;
    } while (lst != NULL);  
    
   fprintf(fh_out, "&$rs = array())\n");
  }
  
  fprintf(fh_out, "  {\n");
  
  // Function body
  fprintf(fh_out, "    $params = Array();\n\n");  
  
  param_count = 0;
  if (param_list != NULL)
  {
    lst = param_list;
    do
    {
      if (lst->p_type == P_TYPE_INT) 
        fprintf(fh_out, "    $params[%d]['type'] =  P_TYPE::INT;\n", param_count);
      else if ((lst->p_type == P_TYPE_VARCHAR))
        fprintf(fh_out, "    $params[%d]['type'] =  P_TYPE::VARCHAR;\n", param_count);
      else if ((lst->p_type == P_TYPE_FLOAT))
        fprintf(fh_out, "    $params[%d]['type'] =  P_TYPE::FLOAT;\n", param_count);
      else if ((lst->p_type == P_TYPE_TEXT))
        fprintf(fh_out, "    $params[%d]['type'] =  P_TYPE::TEXT;\n", param_count);      
      else 
        return -1;

      if (lst->p_direction == P_DIR_IN) 
        fprintf(fh_out, "    $params[%d]['direction'] = P_DIR::IN;\n", param_count);
      else if ((lst->p_direction == P_DIR_OUT))
        fprintf(fh_out, "    $params[%d]['direction'] = P_DIR::OUT;\n", param_count);
      else if ((lst->p_direction == P_DIR_INOUT))
        fprintf(fh_out, "    $params[%d]['direction'] = P_DIR::INOUT;\n", param_count);
      else 
        return -1;      
   
      fprintf(fh_out, "    $params[%d]['value'] = &$%s;\n", param_count, lst->p_name);
           
      lst = lst->next_param;
      param_count++;
    } while (lst != NULL);
    //    $this->exec_sp("sp_check_rfid", $params, $rs);
    fprintf(fh_out, "    return $this->exec_sp(\"%s\", $params, $rs);\n", sp_name);
    fprintf(fh_out, "  }\n\n");
  }   
  
  return 0;
}

int output_body(FILE *fh_out, struct sp_def *sp_ptr, enum lang langtyp)
{
  
  switch (langtyp)
  {
    case LANG_CPP_HEAD:
      output_func_def(sp_ptr, fh_out, 1);
      break;
    
    case LANG_CPP_FUNC:
      generate_sp_function(sp_ptr, fh_out);
      break;
      
    case LANG_PHP:
      generate_sp_function_php(sp_ptr, fh_out);
      break;
      
    default:
      break;
      // ERR
  }
  
  return 0;
}
 
