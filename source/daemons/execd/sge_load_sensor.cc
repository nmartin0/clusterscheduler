
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include "uti/sge_arch.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sgeobj/cull/sge_loadsensor_LS_L.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_report.h"

#include "sge_load_sensor.h"
#include "sge_report_execd.h"

#include "msg_execd.h"

static int ls_send_command(lListElem *elem, const char *command);
static pid_t sge_ls_get_pid(const lListElem *this_ls);
static void sge_ls_set_pid(lListElem *this_ls, pid_t pid);
static int sge_ls_status(lListElem *this_ls);
static lListElem *sge_ls_create_ls(const char* qualified_hostname, const char *name, const char *scriptfile);
static int sge_ls_start_ls(const char *qualified_hostname, lListElem *this_ls);
static void sge_ls_stop_ls(lListElem *this_ls, int send_no_quit_command);
static int sge_ls_start(const char* qualified_hostname, const char *binary_path, char *scriptfile);

static int read_ls(void);

/* 
 * time load sensors get to quit cleanly before they get a SIGKILL 
 */
#define LS_QUIT_TIMEOUT (10)

/* 
 * Each element in this list contains elements which show the state
 * of the corresponding load sensor 
 */
static lList *ls_list = nullptr;   /* LS_Type */

/* 
 * should we start the qidle command 
 */
static int has_to_use_qidle = 0;

/* 
 * should we start the (GNU)-load sensor with 
 */
static int has_to_use_gnu_load_sensor = 0;


/****** execd/loadsensor/sge_ls_get_pid() *************************************
*  NAME
*     sge_ls_get_pid -- get pid of a loadsensor 
*
*  SYNOPSIS
*     static pid_t sge_ls_get_pid(lListElem *this_ls)
*
*  FUNCTION
*     Returns the pid which is stored in an CULL element of
*     the type LS_Type. If the corresponding loadsensor was
*     not started until now then -1 will be returned.
*
*  INPUTS
*     this_ls - pointer to a CULL element of type LS_Type
*
*  RESULT
*     returns pid
******************************************************************************/
static pid_t sge_ls_get_pid(const lListElem *this_ls)
{
   pid_t pid = -1;
   const char *pid_string;

   pid_string = lGetString(this_ls, LS_pid);
   if (pid_string) {
      sscanf(pid_string, pid_t_fmt, &pid);
   }
   return pid;
}

/****** execd/loadsensor/sge_ls_set_pid() *************************************
*  NAME
*     sge_ls_set_pid -- set pid in loadsensor element
*
*  SYNOPSIS
*     static void sge_ls_set_pid(lListElem *this_ls, pid_t pid)
*
*  FUNCTION
*     Set the pid entry in a CULL element of the type LS_Type.
*
*  INPUTS
*     this_ls - pointer to a CULL element of type LS_Type
*     pid - pid of the loadsensor process or -1 
*
*  RESULT
*     [this_ls] - LS_pid entry of the CULL element will be modified 
******************************************************************************/
static void sge_ls_set_pid(lListElem *this_ls, pid_t pid)
{
   char pid_buffer[256];

   sprintf(pid_buffer, pid_t_fmt, pid);
   lSetString(this_ls, LS_pid, pid_buffer);
}

/****** execd/loadsensor/sge_ls_status() **************************************
*  NAME
*     sge_ls_status -- returns the status of a loadsensor 
*
*  SYNOPSIS
*     static int sge_ls_status(lListElem *this_ls)
*
*  FUNCTION
*     The functions detects the status of a londsensor
*     and returns the corresponding integer value. 
*     Following values are possible:
*
*        LS_OK              - the ls waits for commands
*        LS_NOT_STARTED     - load sensor not started   
*        LS_BROKEN_PIPE     - ls has exited or is not ready to read    
*
*  INPUTS
*     this_ls - pointer to a CULL element of type LS_Type
*
*  RESULT
*     returns the status of the loadsensor     
******************************************************************************/
static int sge_ls_status(lListElem *this_ls)
{
   fd_set writefds;
   int ret;
   int higest_fd;

   DENTER(TOP_LAYER);

   if (sge_ls_get_pid(this_ls) == -1) {
      DRETURN(LS_NOT_STARTED);
   }

   /* build writefds */
   FD_ZERO(&writefds);
   higest_fd = fileno((FILE *) lGetRef(this_ls, LS_in));
   FD_SET(higest_fd, &writefds);

   /* is load sensor ready to read ? */
   ret = select(higest_fd + 1, nullptr, &writefds, nullptr, nullptr);

   if (ret <= 0) {
      DRETURN(LS_BROKEN_PIPE);
   }

   DRETURN(LS_OK);
}

/****** execd/loadsensor/sge_ls_start_ls() ************************************
*  NAME
*     sge_ls_start_ls -- starts a loadsensor  
*
*  SYNOPSIS
*     static void sge_ls_start_ls(const char *qualified_hostname, lListElem *this_ls)
*
*  FUNCTION
*     An additional loadsensor process will be started. The name
*     of the script has to be stored in the LS_command entry of
*     'this_ls' before this function will be called. 
*
*     The process environment of the loadsensor will contain
*     the HOST variable. This variable containes the hostname
*     of the execution daemon which calls this function.
*
*     If 'this_ls' correlates to the 'qidle'-loadsensor then 
*     also the XAUTHORITY environment variable will be set.
*
*  INPUTS
*     qualified_hostname - qualified host name
*     this_ls - pointer to a CULL element of type LS_Type
*
*  RESULT
*     An additional loadsensor process will be started. 
*     [this_ls] - the CULL element will be modified
*        LS_pid containes the pid of the ls process 
*        LS_in, LS_out, LS_err are the FILE-streams for the
*        communication with the ls-process     
*        returns LS_OK
*     If sge_peopen fails, returns LS_CANT_PEOPEN     
******************************************************************************/
static int sge_ls_start_ls(const char *qualified_hostname, lListElem *this_ls)
{
   pid_t pid = -1;
   FILE *fp_in = nullptr;
   FILE *fp_out = nullptr;
   FILE *fp_err = nullptr;
   char buffer[1024];
   char **envp = nullptr;

   DENTER(TOP_LAYER);

   sprintf(buffer, "%s=%s", "HOST", qualified_hostname);
   if (has_to_use_qidle
       && !strcmp(lGetString(this_ls, LS_name), IDLE_LOADSENSOR_NAME)) {
      envp = (char **) sge_malloc(sizeof(char *) * 3);
      envp[0] = buffer;
      envp[1] = (char *)"XAUTHORITY=/tmp/.xauthority";
      envp[2] = nullptr;
   } else {
      envp = (char **) sge_malloc(sizeof(char *) * 2);
      envp[0] = buffer;
      envp[1] = nullptr;
   }

   /* we need fds for select() .. */
   pid = sge_peopen("/bin/sh", 0, lGetString(this_ls, LS_command), nullptr, envp,
                &fp_in, &fp_out, &fp_err, true);

   if (envp) {
      sge_free(&envp);
   }
   if (pid == -1) {
      return LS_CANT_PEOPEN;
   }
   /* we need load reports non blocking */
   fcntl(fileno(fp_out), F_SETFL, O_NONBLOCK);

   sge_ls_set_pid(this_ls, pid);
   lSetRef(this_ls, LS_in, fp_in);
   lSetRef(this_ls, LS_out, fp_out);
   lSetRef(this_ls, LS_err, fp_err);

   DPRINTF(("%s: successfully started load sensor \"%s\"\n",
            __func__, lGetString(this_ls, LS_command)));

   /* request first load report after starting */
   ls_send_command(this_ls, "\n");

   return LS_OK;
}

/******* execd/loadsensor/sge_ls_create_ls() **********************************
*  NAME
*     sge_ls_create_ls -- creates a new CULL loadsensor element 
*
*  SYNOPSIS
*     static lListElem* sge_ls_create_ls(const char *qualified_hostname,
*                                        char *name, const char *scriptfile)
*
*  FUNCTION
*     The function creates a new CULL element of type LS_Type and
*     returns a pointer to this object. The loadsensor will be
*     started immediately.
*     If it cannot be started then, LS_has_to_restart is set to 
*     true so that it will be attempted to be restarted in the next load interval
*
*  INPUTS
*     qualified_hostname - qualified host name
*     name - pseudo name of the ls
*              "extern" for user defined loadsensors
*              "intern" for qidle and qloadsensor
*     scriptfile - absolute path to the ls scriptfile
*
*  RESULT
*     new CULL element of type LS_Type will be returned
*     and a new loadsensor process will be created by this function
******************************************************************************/
static lListElem *sge_ls_create_ls(const char *qualified_hostname, const char *name, const char *scriptfile)
{
   lListElem *new_ls = nullptr;    /* LS_Type */
   SGE_STRUCT_STAT st;

   DENTER(TOP_LAYER);

   if (scriptfile != nullptr) {
      if (SGE_STAT(scriptfile, &st) != 0) {
         if (strcmp(name, "extern") == 0) {
            WARNING(MSG_LS_NOMODTIME_SS, scriptfile, strerror(errno));
         }
         DRETURN(nullptr);
      }

      new_ls = lCreateElem(LS_Type);
      if (new_ls) {
         /* initialize all attributes */
         lSetString(new_ls, LS_name, name);
         lSetString(new_ls, LS_command, scriptfile);
         sge_ls_set_pid(new_ls, -1);
         lSetRef(new_ls, LS_in, nullptr);
         lSetRef(new_ls, LS_out, nullptr);
         lSetRef(new_ls, LS_err, nullptr);
         lSetBool(new_ls, LS_has_to_restart, false);
         lSetUlong(new_ls, LS_tag, 0);
         lSetList(new_ls, LS_incomplete, lCreateList("", LR_Type));
         lSetList(new_ls, LS_complete, lCreateList("", LR_Type));
         lSetUlong(new_ls, LS_last_mod, st.st_mtime);

         /* start loadsensor, if couldn't set the restart flag so that we
          * restart it in the next load interval
          */
         if (sge_ls_start_ls(qualified_hostname, new_ls) != LS_OK) {
            lSetBool(new_ls, LS_has_to_restart, true);
         }
      }
   }
   DRETURN(new_ls);
}

/****** execd/loadsensor/sge_ls_stop_ls() *************************************
*  NAME
*     sge_ls_stop_ls -- stop a loadsensor process
*
*  SYNOPSIS
*     static void sge_ls_stop_ls(lListElem *this_ls, 
*        int send_no_quit_command) 
*
*  FUNCTION
*     The "quit" command will be send to the loadsensor process.
*     So the loadsensor process can stop itself.
*
*  INPUTS
*     this_ls - pointer to a CULL element of type LS_Type
*     send_no_quit_command - 
*        0 - send quit command
*        1 - no quit command will be send (kill without notification)
*
*  RESULT
*     the loadsensor process will be terminated
*     [this_ls] the entries will be reinitialized
******************************************************************************/
static void sge_ls_stop_ls(lListElem *this_ls, int send_no_quit_command)
{
   int ret, exit_status;
   struct timeval t;

   DENTER(TOP_LAYER);

   if (sge_ls_get_pid(this_ls) == -1) {
      DRETURN_VOID;
   }

   if (!send_no_quit_command) {
      ls_send_command(this_ls, "quit\n");
      ret = sge_ls_status(this_ls);
   } else {
      ret = LS_BROKEN_PIPE;
   }

   memset(&t, 0, sizeof(t));
   if (ret == LS_OK) {
      t.tv_sec = LS_QUIT_TIMEOUT;
   } else {
      t.tv_sec = 0;
   }

   /* close all fds to load sensor */
   if (ret != LS_NOT_STARTED) {
      exit_status = sge_peclose(sge_ls_get_pid(this_ls), (FILE *)lGetRef(this_ls, LS_in),
                            (FILE *)lGetRef(this_ls, LS_out), (FILE *)lGetRef(this_ls, LS_err),
                            (t.tv_sec ? &t : nullptr));
      DPRINTF(("%s: load sensor `%s` stopped, exit status from sge_peclose= %d\n",
               __func__, lGetString(this_ls, LS_command), exit_status));
   }

   sge_ls_set_pid(this_ls, -1);
   DRETURN_VOID;
}

/****** execd/loadsensor/read_ls() ********************************************
*  NAME
*     read_ls -- read sensor output and add it to load report
*
*  SYNOPSIS
*     static int read_ls(void)
*
*  FUNCTION
*     This function loops over all loadsensor elements in 
*     the ls_list (LS_Type). It tries to read from the
*     output stream (LS_out). The output will be parsed
*     and stored in the LS_incomplete entry (LR_Type). 
*     
*     If the protocol part of the loadsensor is correct
*     then the entries of LS_incomplete will be moved
*     LS_complete. 
* 
*     The last complete set of load values (LS_complete)
*     will be added to the load report.
*     
*  INPUTS
*     this_ls - pointer to a CULL element of type LS_Type
*
*  RESULT
*     [this_ls] LS_incomplete and LS_complete will be modified.
******************************************************************************/
static int read_ls(void)
{
   char input[10000];
   char host[1000];
   char name[1000];
   char value[1000];
   lListElem *ls_elem;
   bool flag = true;

   DENTER(TOP_LAYER);

   for_each_rw(ls_elem, ls_list) {
         FILE *file = (FILE *)lGetRef(ls_elem, LS_out);
      
      if (sge_ls_get_pid(ls_elem) == -1) {
         continue;
      }

      DPRINTF(("receiving from %s\n", lGetString(ls_elem, LS_command)));

      while (flag) {
         if (fscanf(file, "%[^\n]\n", input) != 1) {
            break;
         }
         DPRINTF(("received: >>%s<<\n", input));

         if (!strcmp(input, "begin") || !strcmp(input, "start")) {
            /* remove last possibly incomplete load report */
            lSetList(ls_elem, LS_incomplete, lCreateList("", LR_Type));
            continue;
         }

         if (!strcmp(input, "end")) {
            /* replace old load report by new one */
            lList *tmp_list = nullptr;
            lXchgList(ls_elem, LS_incomplete, &tmp_list);
            lXchgList(ls_elem, LS_complete, &tmp_list);
            lFreeList(&tmp_list);

            /* request next load report from ls */
            ls_send_command(ls_elem, "\n");
            break;
         }

         /* add a newline for pattern matching in sscanf */
         strcat(input, "\n");
         if (sscanf(input, "%[^:]:%[^:]:%[^\n]", host, name, value) != 3) {
            DPRINTF(("format error in line: \"%100s\"\n", input));
            ERROR(MSG_LS_FORMAT_ERROR_SS, lGetString(ls_elem, LS_command), input);
         } else {
            {
               lList *tmp_list = lGetListRW(ls_elem, LS_incomplete);
               sge_add_str2load_report(&tmp_list, name, value, host);
            }
         }
      }
   }

   DRETURN(0);
}

/****** execd/loadsensor/ls_send_command() ************************************
*  NAME
*     ls_send_command -- send a command to a loadsensor 
*
*  SYNOPSIS
*     static int ls_send_command(lListElem *this_ls, const char *command)
*
*  FUNCTION
*     This function will send a command through the input
*     stream (LS_in) to the loadsensor. 
*
*  INPUTS
*     this_ls - pointer to a CULL element of type LS_Type
*     command - valid loadsensor command
*
*  RESULT
*      0 - success
*     -1 - error
******************************************************************************/
static int ls_send_command(lListElem *this_ls, const char *command)
{
   fd_set writefds;
   struct timeval timeleft;
   int ret;
   FILE *file;
   int higest_fd;

   DENTER(TOP_LAYER);

   FD_ZERO(&writefds);
   higest_fd = fileno((FILE *) lGetRef(this_ls, LS_in));
   FD_SET(higest_fd, &writefds);

   timeleft.tv_sec = 0;
   timeleft.tv_usec = 0;

   /* wait for writing on fd_in */
   ret = select(higest_fd + 1, nullptr, &writefds, nullptr, &timeleft);
   if (ret == -1) {
      switch (errno) {
      case EINTR:
         DPRINTF(("select failed with EINTR\n"));
         WARNING("[load_sensor %s] select failed with EINTR", lGetString(this_ls, LS_pid));
         break;
      case EBADF:
         DPRINTF(("select failed with EBADF\n"));
         WARNING("[load_sensor %s] select failed with EBADF", lGetString(this_ls, LS_pid));
         break;
      case EINVAL:
         DPRINTF(("select failed with EINVAL\n"));
         WARNING("[load_sensor %s] select failed with EINVAL", lGetString(this_ls, LS_pid));
         break;
      default:
         DPRINTF(("select failed with unexpected errno %d", errno));
         WARNING("[load_sensor %s] select failed with [%s]", lGetString(this_ls, LS_pid), strerror(errno));
      }
      DRETURN(-1);
   }

   if (!FD_ISSET(fileno((FILE *) lGetRef(this_ls, LS_in)), &writefds)) {
      DPRINTF(("received: cannot read\n"));
      WARNING("[load_sensor %s] received: cannot read", lGetString(this_ls, LS_pid));
      DRETURN(-1);
   }

   /* send command to load sensor */
   file = (FILE *)lGetRef(this_ls, LS_in);
   if (fprintf(file, "%s", command) != (int)strlen(command)) {
      WARNING("[load_sensor %s] couldn't send command [%s]", lGetString(this_ls, LS_pid), strerror(errno));
      DRETURN(-1);
   }
   if (fflush(file) != 0) {
      WARNING("[load_sensor %s] fflush failed [%s]", lGetString(this_ls, LS_pid), strerror(errno));
      DRETURN(-1);
   }

   DRETURN(0);
}

/****** execd/loadsensor/sge_ls_qidle() ***************************************
*  NAME
*     sge_ls_qidle -- enable/diable qidle loadsensor 
*
*  SYNOPSIS
*     static void sge_ls_qidle(int qidle)
*
*  FUNCTION
*     enable/diable qidle loadsensor
*
*  INPUTS
*     qidle: 1 - enable qidle
*            0 - diable qidle 
******************************************************************************/

void sge_ls_qidle(int qidle)
{
   has_to_use_qidle = qidle;
}

/****** execd/loadsensor/sge_ls_gnu_ls() **************************************
*  NAME
*     sge_ls_gnu_ls -- enable/diable qloadsensor
*
*  SYNOPSIS
*     static void sge_ls_gnu_ls(int gnu_ls)
*
*  FUNCTION
*     enable/diable qidle loadsensor
*
*  INPUTS
*     qidle: 1 - enable qidle
*            0 - diable qidle
******************************************************************************/
void sge_ls_gnu_ls(int gnu_ls)
{
   has_to_use_gnu_load_sensor = gnu_ls;
}

/****** execd/loadsensor/sge_ls_start() ***************************************
*  NAME
*     sge_ls_start -- start/stop/restart loadsensors 
*
*  SYNOPSIS
*     int sge_ls_start(const char *scriptfiles)
*
*  FUNCTION
*     The 'scriptfiles' parameter will be parsed. Each
*     loadsensor not contained in the global list 
*     'ls_list' (LS_Type) will be added and the process 
*     will be started. 
*
*     Loadsensors wich are contained in the global list
*     but not in 'scriptfiles' will be stopped and 
*     removed.
*
*     Depending on global variables additional internal
*     loadsensors will be started:
*      'has_to_use_gnu_load_sensor' == 1  => start qloadsensor
*      'has_to_use_qidle' == 1            => start qidle
*     
*  INPUTS
*     scriptfiles - comma separated list of scriptfiles
*
*  RESULT
*     LS_OK
******************************************************************************/
static int sge_ls_start(const char *qualified_hostname, const char *binary_path, char *scriptfiles)
{
   lListElem *ls_elem = nullptr;        /* LS_Type */
   lListElem *nxt_ls_elem = nullptr;    /* LS_Type */
   char scriptfiles_buffer[MAX_STRING_SIZE];
   SGE_STRUCT_STAT stat_buffer;

   DENTER(TOP_LAYER);

   /* tag all elements */
   for_each_rw(ls_elem, ls_list) {
      lSetUlong(ls_elem, LS_tag, 1);
   }

   /* add / remove load sensors */
   if ((scriptfiles != nullptr) && (strcasecmp(scriptfiles, "NONE") != 0)) {
      char *scriptfile = nullptr;

       if (strlen(scriptfiles) > MAX_STRING_SIZE - 1) {
          DRETURN(LS_NOT_STARTED);
       }
   
      strcpy(scriptfiles_buffer, scriptfiles);
      /* add new load sensors if necessary 
       * and remove tags from the existing load sensors 
       * contained in 'scriptfiles' */
      scriptfile = strtok(scriptfiles_buffer, ",\n");
      while (scriptfile) {
         ls_elem = lGetElemStrRW(ls_list, LS_command, scriptfile);

         if (ls_elem == nullptr) {
            INFO(MSG_LS_STARTLS_S, scriptfile);
            ls_elem = sge_ls_create_ls(qualified_hostname, "extern", scriptfile);

            if (ls_list == nullptr) {
               ls_list = lCreateList("", LS_Type);
            }
            lAppendElem(ls_list, ls_elem);
         }
         if (ls_elem != nullptr) {
            lSetUlong(ls_elem, LS_tag, 0);
         }
         scriptfile = strtok(nullptr, ",\n");
      }
   }

   /* QIDLE loadsensor */
   if (has_to_use_qidle) {
      snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s/%s",
               binary_path, sge_get_arch(),
               IDLE_LOADSENSOR_NAME);
      
      if (SGE_STAT(scriptfiles_buffer, &stat_buffer) != 0) {
         snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s",
                  binary_path, IDLE_LOADSENSOR_NAME);
      }
      
      ls_elem = lGetElemStrRW(ls_list, LS_command, scriptfiles_buffer);
      if (ls_elem == nullptr) {
         ls_elem = sge_ls_create_ls(qualified_hostname, IDLE_LOADSENSOR_NAME, scriptfiles_buffer);

         if (ls_list == nullptr) {
            ls_list = lCreateList("", LS_Type);
         }
         lAppendElem(ls_list, ls_elem);
      }
      if (ls_elem != nullptr) {
         lSetUlong(ls_elem, LS_tag, 0);
      }
   }

   /* GNU loadsensor */
   if (has_to_use_gnu_load_sensor) {
      snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s/%s",
               binary_path, sge_get_arch(),
               GNU_LOADSENSOR_NAME);
      
      if (SGE_STAT(scriptfiles_buffer, &stat_buffer) != 0) {
         snprintf(scriptfiles_buffer, MAX_STRING_SIZE, "%s/%s",
                  binary_path, GNU_LOADSENSOR_NAME);
      }
      
      ls_elem = lGetElemStrRW(ls_list, LS_command, scriptfiles_buffer);
      if (ls_elem == nullptr) {
         ls_elem = sge_ls_create_ls(qualified_hostname, GNU_LOADSENSOR_NAME, scriptfiles_buffer);

         if (ls_list == nullptr) {
            ls_list = lCreateList("", LS_Type);
         }
         lAppendElem(ls_list, ls_elem);
      }
      if (ls_elem != nullptr) {
         lSetUlong(ls_elem, LS_tag, 0);
      }
   }

   /* tagged elements are not contained in 'scriptfiles'
    * => we will remove them */
   nxt_ls_elem = lFirstRW(ls_list);
   while ((ls_elem = nxt_ls_elem)) {
      nxt_ls_elem = lNextRW(ls_elem);
      if (lGetUlong(ls_elem, LS_tag) == 1) {
         INFO(MSG_LS_STOPLS_S, lGetString(ls_elem, LS_command));
         sge_ls_stop_ls(ls_elem, 0);
         lRemoveElem(ls_list, &ls_elem);
      }
   }

   DRETURN(LS_OK);
}

/****** execd/loadsensor/trigger_ls_restart() *********************************
*  NAME
*     trigger_ls_restart -- restart loadsensors
*
*  SYNOPSIS
*     void trigger_ls_restart(void)
*
*  FUNCTION
*     Trigger the restart of all loadsensors
******************************************************************************/
void trigger_ls_restart(void)
{
   lListElem *ls;

   DENTER(TOP_LAYER);

   for_each_rw(ls, ls_list) {
      lSetBool(ls, LS_has_to_restart, true);
   }

   DRETURN_VOID;
}

/****** execd/loadsensor/sge_ls_stop_if_pid() *********************************
*  NAME
*     sge_ls_stop_if_pid -- restart loadsensor with given pid 
*
*  SYNOPSIS
*     int sge_ls_stop_if_pid(pid_t pid, int send_no_quit_command)
*
*  FUNCTION
*     If the given pid is the pid of a loadsensor we started
*     previously, then we will trigger a restart. This is
*     necessary when a loadsensor process dies horribly.
*     The execd notifies this module by invoking this function.
*
*  INPUTS
*     pid - process id
*
*  RESULT
*     0 - pid was not a loadsensor
*     1 - we triggerd the restart because pid was a loadsensor
******************************************************************************/
int sge_ls_stop_if_pid(pid_t pid)
{
   lListElem *ls;

   DENTER(TOP_LAYER);

   for_each_rw(ls, ls_list) {
      if (pid == sge_ls_get_pid(ls)) {
         trigger_ls_restart();
         DRETURN(1);
      }
   }

   DRETURN(0);
}

/****** execd/loadsensor/sge_ls_get() *****************************************
*  NAME
*     sge_ls_get -- reqeust a load report 
*
*  SYNOPSIS
*     int sge_ls_get(lList **lpp)
*
*  FUNCTION
*     This functions starts/stops/restarts all loadsensors
*     contained in the global variable 'conf.load_sensor'.
*
*     The restart of a loadsensor process will be triggered 
*     when the modification time of the scriptfile changed.
*
*     After that it collects load values by reading the
*     output of each loadsensor process. The last complete
*     list of load values will be added into the given load
*     report list 'lpp'.
*     
*  INPUTS
*     lpp - last complete list of load values determined
*           by the started loadsensors 
*
*  RESULT
*     0 - OK
******************************************************************************/
int sge_ls_get(const char *qualified_hostname, const char *binary_path, lList **lpp)
{
   lListElem *ls_elem;          /* LS_Type */
   lListElem *ep;
   char* load_sensor = nullptr;

   DENTER(TOP_LAYER);

   load_sensor = mconf_get_load_sensor();
   sge_ls_start(qualified_hostname, binary_path, load_sensor);

   for_each_rw(ls_elem, ls_list) {
      bool restart = false;
      SGE_STRUCT_STAT st;
      const char *ls_command;
      const char *ls_name;

      ls_command = lGetString(ls_elem, LS_command);
      ls_name = lGetString(ls_elem, LS_name);

      /* someone triggered the restart */
      if (lGetBool(ls_elem, LS_has_to_restart)) {
         restart = true;
      }

      /* the modification time of the ls script changed */
      if (!restart) {
         if (ls_command && SGE_STAT(ls_command, &st)) {
            if (!strcmp(GNU_LOADSENSOR_NAME, ls_name) ||
                !strcmp(IDLE_LOADSENSOR_NAME, ls_name)) {
               WARNING(MSG_LS_NOMODTIME_SS, ls_command, strerror(errno));
            }
            continue;
         }
         if ((time_t)lGetUlong(ls_elem, LS_last_mod) != st.st_mtime) {
            restart = true;
            lSetUlong(ls_elem, LS_last_mod, st.st_mtime);
         }
      }

      if (restart) {
         INFO(MSG_LS_RESTARTLS_S, ls_command ? ls_command : "");
         sge_ls_stop_ls(ls_elem, 0);
         /* start the load sensor script, set the restart flag if the load
          * sensor didn't start so that we try to start it again in the next
          * load interval! 
          */
         if (sge_ls_start_ls(qualified_hostname, ls_elem) == LS_OK) {
            lSetBool(ls_elem, LS_has_to_restart, false);
         }
      }
   }

   read_ls();

   for_each_rw(ls_elem, ls_list) {
      /* merge external load into existing load report list */
      for_each_rw(ep, lGetList(ls_elem, LS_complete)) {
         sge_add_str2load_report(lpp, lGetString(ep, LR_name),
                                 lGetString(ep, LR_value),
                                 lGetHost(ep, LR_host));
      }
   }

   sge_free(&load_sensor);

   DRETURN(0);
}

/****** execd/loadsensor/sge_ls_stop() ****************************************
*  NAME
*     sge_ls_stop -- stop all loadsensor
*
*  SYNOPSIS
*     void sge_ls_stop(int exited)
*
*  FUNCTION
*     Stop all loadsensors and destroy the complete
*     'ls_list'. 
*
*  INPUTS
*     exited - notify the loadsensors before exit
*        0 - notify them
*        1 - do not notify them (avoid communication with ls)
******************************************************************************/
void sge_ls_stop(int exited)
{
   lListElem *ls_elem;

   DENTER(TOP_LAYER);

   for_each_rw(ls_elem, ls_list) {
      sge_ls_stop_ls(ls_elem, exited);
   }
   lFreeList(&ls_list);

   DRETURN_VOID;
}

