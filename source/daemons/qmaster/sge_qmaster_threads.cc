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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <csignal>
#include <cstring>
#include <cerrno>

#include "uti/msg_utilib.h" /* remove once 'sge_daemonize_qmaster' did become 'sge_daemonize' */
#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_security.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "comm/cl_commlib.h"

#include "basis_types.h"
#include "sge_qmaster_threads.h"
#include "sge_thread_main.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "msg_daemons_common.h"

/****** qmaster/sge_qmaster_main/sge_gdi_kill_master() *************************
*  NAME
*     ocs::gdi::Client::sge_gdi_kill_master() -- Shutdown qmaster via GDI
*
*  SYNOPSIS
*     void ocs::gdi::Client::sge_gdi_kill_master(ocs::gdi::Packet *packet, ocs::gdi::Task *task);
*
*  FUNCTION
*     Shutdown qmaster by means of a GDI request. This operation is only
*     permitted for a user of type 'manager'.
*
*  INPUTS
*     ocs::gdi::Packet *packet - request packet
*     ocs::gdi::Task *task     - request task
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: ocs::gdi::Client::sge_gdi_kill_master() is not MT safe.
*     MT-NOTE:
*     MT-NOTE: This is acceptable for now, because this function is currently
*     MT-NOTE: only invoked from the message thread.
*
*     TODO-AD: make this function thread safe. 'manop_is_manager()' is NOT MT
*     TODO-AD  safe.
*
*******************************************************************************/
void
sge_gdi_kill_master(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
   DENTER(GDI_LAYER);

   // show information in debug output
   if (DPRINTF_IS_ACTIVE) {
      dstring dbg_msg = DSTRING_INIT;
      ocs_id2dstring(&dbg_msg, packet->uid, packet->user, packet->gid, packet->group, packet->amount, packet->grp_array);
      sge_dstring_free(&dbg_msg);
   }

   const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
   if (!manop_is_manager(packet, master_manager_list)) {
      ERROR(SFNMAX, MSG_SHUTDOWN_SHUTTINGDOWNQMASTERREQUIRESMANAGERPRIVILEGES);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
      DRETURN_VOID;
   }

   if (sge_qmaster_shutdown_via_signal_thread(0) == 0) {
      INFO(MSG_SGETEXT_KILL_SSS, packet->user, packet->host, prognames[QMASTER]);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   } else {
      ERROR(MSG_SGETEXT_KILL_FAILED_SSS, packet->user, packet->host, prognames[QMASTER]);
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }

   DRETURN_VOID;
} /* ocs::gdi::Client::sge_gdi_kill_master() */

/****** qmaster/sge_qmaster_main/sge_daemonize_qmaster() ***************************
*  NAME
*     sge_daemonize_qmaster() -- Turn qmaster into a daemon. 
*
*  SYNOPSIS
*     static void sge_daemonize_qmaster() 
*
*  FUNCTION
*     If the environment variable 'SGE_ND' is set, the functions does return
*     immediately.
*
*     First, we call 'fork()'. If the process was started as a shell command in
*     the foreground, when the parent terminates, the shell thinks the command
*     is done. This automatically runs the child process in the background.
*     Also, the child inherits the process group ID from the parent but gets
*     its own process ID. This guarantees that the child is not a process group
*     leader. 
*
*     We call 'setsid()' to create a new session. The process becomes the
*     session leader of the new session, becomes the process group leader of a
*     new process group, and has no controlling terminal.
*
*     By calling 'fork()' a second time, we guarantee the the daemon (second
*     child) is no longer a session leader, so it cannot acquire a controlling
*     terminal. We must ignore 'SIGHUP' because when the session leader
*     terminates (the first child), all processes in the session (our second
*     child) receive the 'SIGHUP' signal.
*
*     We close any open descriptors that are inherited from the process that
*     executed 'sge_qmaster', normally a shell. We redirect 'stdin', 'stdout'
*     and 'stderr' to '/dev/null'. The reason for opening these descriptors
*     is so that any library function called by 'sge_qmaster' that assumes it
*     can read from standard input or write to either standard ouput or
*     standard error will not fail.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: sge_daemonize_qmaster() is not MT safe 
*
*******************************************************************************/
bool
sge_daemonize_qmaster() {
   pid_t pid = -1;
   int failed_fd;

   DENTER(TOP_LAYER);

   if (getenv("SGE_ND") != nullptr) {
      DPRINTF("sge_qmaster is not daemonized\n");
      DRETURN(false);
   }

   if ((pid = fork()) != 0) {
      if (pid < 0) {
         CRITICAL(MSG_PROC_FIRSTFORKFAILED_S, strerror(errno));
      }
      exit(0); /* parent terminates */
   }

   setsid();

   signal(SIGHUP, SIG_IGN);

   if ((pid = fork()) != 0) {
      if (pid < 0) {
         CRITICAL(MSG_PROC_SECONDFORKFAILED_S, strerror(errno));
      }
      exit(0); /* child 1 terminates */
   }

   sge_close_all_fds(nullptr, 0);

   failed_fd = sge_occupy_first_three();
   if (failed_fd != -1) {
      CRITICAL(MSG_CANNOT_REDIRECT_STDINOUTERR_I, failed_fd);
      sge_exit(0);
   }

   DRETURN(true);
} /* sge_daemonize_qmaster() */

/****** qmaster/sge_qmaster_main/sge_become_admin_user() ***************************
*  NAME
*     sge_become_admin_user() -- Become admin user. 
*
*  SYNOPSIS
*     static void sge_become_admin_user() 
*
*  FUNCTION
*     Get admin user from bootstrap configuration. Set admin user and change
*     the effective UID/GID to the admin user UID/GID. 
*
*     Note: The effective UID does determine file access permissions.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: sge_become_admin_user() is not MT safe 
*
*******************************************************************************/
void
sge_become_admin_user(const char *admin_user) {
   char str[MAX_STRING_SIZE];

   DENTER(TOP_LAYER);

   if (sge_set_admin_username(admin_user, str, sizeof(str)) == -1) {
      CRITICAL(SFNMAX, str);
      sge_exit(1);
   }

   if (sge_switch2admin_user()) {
      CRITICAL(SFNMAX, MSG_ERROR_CANTSWITCHTOADMINUSER);
      sge_exit(1);
   }

   DRETURN_VOID;
} /* sge_become_admin_user() */

/****** qmaster/sge_qmaster_main/sge_exit_func() **********************************
*  NAME
*     sge_exit_func() -- qmaster exit function
*
*  SYNOPSIS
*     static void sge_exit_func(int anExitValue) 
*
*  FUNCTION
*     qmaster exit function. This function should be used BEFORE qmaster
*     did change its working directory to be the spool directory. This
*     exit function does NOT lock the qmaster lock file.
*
*  INPUTS
*     int anExitValue - exit value 
*
*  RESULT
*     void - none 
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE: sge_exit_func() is MT safe.
*
*******************************************************************************/
void
sge_exit_func(int anExitValue) {
   DENTER(TOP_LAYER);
   ocs::gdi::ClientBase::shutdown();
   component_ts0_destroy();

   DRETURN_VOID;
}

