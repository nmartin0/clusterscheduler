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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>

#if 0
#define QEVENT_SHOW_ALL
#endif

#if defined(FREEBSD) || defined(NETBSD) || defined(DARWIN)
#include <sys/time.h>
#endif

#include <sys/resource.h>
#include <sys/wait.h>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_DataStore.h"

#include "mir/sge_mirror.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "ocs_qevent.h"
#include "sig_handlers.h"
#include "msg_clients_common.h"

u_long Global_jobs_running = 0;
u_long Global_jobs_registered = 0;
qevent_options *Global_qevent_options;


static void qevent_show_usage();
static void qevent_testsuite_mode(sge_evc_class_t *evc);
static void qevent_subscribe_mode(sge_evc_class_t *evc);
static const char* qevent_get_event_name(int event);
static void qevent_trigger_scripts(int qevent_event, qevent_options *option_struct, lListElem *event);
static void qevent_start_trigger_script(int qevent_event, const char* script_file, lListElem *event);
static qevent_options* qevent_get_option_struct();
static void qevent_set_option_struct(qevent_options *option_struct);


static void  qevent_set_option_struct(qevent_options *option_struct) {
   Global_qevent_options=option_struct;
}


static qevent_options* qevent_get_option_struct() {
   return Global_qevent_options;
}

static void qevent_dump_pid_file() {
   sge_write_pid("qevent.pid");
}

static sge_callback_result 
print_event([[maybe_unused]] sge_evc_class_t *evc, sge_object_type type,
            [[maybe_unused]] sge_event_action action, lListElem *event, [[maybe_unused]] void *clientdata)
{
   char buffer[1024];
   dstring buffer_wrapper;

   DENTER(TOP_LAYER);

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

   fprintf(stdout, "%s\n", event_text(event, &buffer_wrapper));
   fflush(stdout);

   DRETURN(SGE_EMA_OK);
}

#ifndef QEVENT_SHOW_ALL

static u_long
get_current_jatask_status(u_long job_id, u_long ja_task_id) {
   u_long32 ret = JIDLE;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lListElem *job = lGetElemUlong(master_job_list, JB_job_number, job_id);
   if (job != nullptr) {
      const lListElem *ja_task = lGetSubUlong(job, JAT_task_number, ja_task_id, JB_ja_tasks);
      if (ja_task != nullptr) {
         ret = lGetUlong(ja_task, JAT_status);
      }
   }
   return ret;
}

static sge_callback_result
print_jatask_event([[maybe_unused]] sge_evc_class_t *evc, sge_object_type type,
                   [[maybe_unused]] sge_event_action action, lListElem *event, [[maybe_unused]] void *client_data)
{
   char buffer[1024];
   dstring buffer_wrapper;

   DENTER(TOP_LAYER);

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));
   
   DPRINTF("%s\n", event_text(event, &buffer_wrapper));
   if (lGetPosViaElem(event, ET_type, SGE_NO_ABORT) >= 0) {
      u_long32 event_type = lGetUlong(event, ET_type);
      u_long64 timestamp = lGetUlong64(event, ET_timestamp);
      
      if (event_type == sgeE_JATASK_MOD) {
         lList *jat = lGetListRW(event, ET_new_version);
         u_long job_id  = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         // we might get a JATASK_MOD event for a task that is already in transfer or running == already counted
         // then ignore the event
         u_long32 current_status = get_current_jatask_status(job_id, task_id);
         if (current_status != JRUNNING && current_status != JTRANSFERING) {
            const lListElem *ep = lFirst(jat);
            u_long job_status = lGetUlong(ep, JAT_status);
            int task_running = (job_status==JRUNNING || job_status==JTRANSFERING);

            if (task_running) {
               fprintf(stdout,"JOB_START (%ld.%ld:ECL_TIME=" sge_u64 ")\n", job_id ,task_id, timestamp);
               fflush(stdout);
               Global_jobs_running++;
            }
         }
      }
      if (event_type == sgeE_JOB_USAGE) {
         u_long job_id = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         fprintf(stdout,"JOB_USAGE (%ld.%ld:ECL_TIME=" sge_u64 ")\n", job_id, task_id, timestamp);
         Global_jobs_running--;
         fflush(stdout);
      }
      if (event_type == sgeE_JOB_FINAL_USAGE) {
         /* lList *jat = lGetList(event,ET_new_version); */
         u_long job_id = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         /* lWriteElemTo(event, stdout); */
         fprintf(stdout,"JOB_FINAL_USAGE (%ld.%ld:ECL_TIME=" sge_u64 ")\n", job_id, task_id, timestamp);
         Global_jobs_running--;
         fflush(stdout);  
      }
      if (event_type == sgeE_JOB_FINISH) {
         u_long job_id = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         fprintf(stdout,"JOB_FINISH (%ld.%ld:ECL_TIME=" sge_u64 ")\n", job_id, task_id, timestamp);
         Global_jobs_running--;
         fflush(stdout);
      }
      if (event_type == sgeE_JOB_ADD) {
         const lList *jat = lGetListRW(event,ET_new_version);
         u_long job_id  = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         const lListElem *ep = lFirst(jat);
         const char* job_project = lGetString(ep, JB_project);
         if (job_project == nullptr) {
            job_project = "NONE";
         }
         fprintf(stdout,"JOB_ADD (%ld.%ld:ECL_TIME=" sge_u64 ":project=%s)\n", job_id, task_id, timestamp,job_project);
         Global_jobs_registered++;
         fflush(stdout);  
      }
      if (event_type == sgeE_JOB_DEL) {
         u_long job_id  = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         fprintf(stdout,"JOB_DEL (%ld.%ld:ECL_TIME=" sge_u64 ")\n", job_id, task_id, timestamp);
         Global_jobs_registered--;
         fflush(stdout);  
      }
      if (event_type == sgeE_CATEGORY_ADD) {
         u_long category_id  = lGetUlong(event, ET_intkey);
         fprintf(stdout,"CATEGORY_ADD (%ld:ECL_TIME=" sge_u64 ")\n", category_id, timestamp);
         Global_jobs_registered--;
         fflush(stdout);
      }
      if (event_type == sgeE_CATEGORY_MOD) {
         u_long category_id  = lGetUlong(event, ET_intkey);
         fprintf(stdout,"CATEGORY_MOD (%ld:ECL_TIME=" sge_u64 ")\n", category_id, timestamp);
         Global_jobs_registered--;
         fflush(stdout);
      }
      if (event_type == sgeE_CATEGORY_DEL) {
         u_long category_id  = lGetUlong(event, ET_intkey);
         fprintf(stdout,"CATEGORY_DEL (%ld:ECL_TIME=" sge_u64 ")\n", category_id, timestamp);
         Global_jobs_registered--;
         fflush(stdout);
      }
   }
   DRETURN(SGE_EMA_OK);
}
#endif

static sge_callback_result
analyze_jatask_event([[maybe_unused]] sge_evc_class_t *evc, sge_object_type type,
                     [[maybe_unused]] sge_event_action action, lListElem *event, [[maybe_unused]] void *client_data)
{
   char buffer[1024];
   dstring buffer_wrapper;

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));
   
   if (lGetPosViaElem(event, ET_type, SGE_NO_ABORT) >= 0) {
      u_long32 event_type = lGetUlong(event, ET_type);

      if (event_type == sgeE_JATASK_MOD) {
         const lList *jat = lGetList(event,ET_new_version);
         const lListElem *ep = lFirst(jat);
         u_long job_status = lGetUlong(ep, JAT_status);
         int task_running = (job_status==JRUNNING || job_status==JTRANSFERING);
         if (task_running) {
         }
      }

      if (event_type == sgeE_JOB_ADD) {
         /* lList *jat = lGetList(event,ET_new_version);
         u_long job_id  = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         lListElem *ep = lFirst(jat); */
      }

      if (event_type == sgeE_JOB_DEL) {
         qevent_trigger_scripts(QEVENT_JB_END, qevent_get_option_struct(), event);
      }

      if (event_type == sgeE_JATASK_DEL) {
         qevent_trigger_scripts(QEVENT_JB_TASK_END,qevent_get_option_struct() , event);
      }


   }

   return SGE_EMA_OK;
}



static void qevent_trigger_scripts( int qevent_event, qevent_options *option_struct, lListElem *event) {

   DENTER(TOP_LAYER);
   if (option_struct->trigger_option_count > 0) {
      INFO("trigger for event " SFN "\n", qevent_get_event_name(qevent_event) );
      for (int i = 0; i < option_struct->trigger_option_count; i++) {
         if ( (option_struct->trigger_option_events)[i] == qevent_event ) {
            qevent_start_trigger_script(qevent_event ,(option_struct->trigger_option_scripts)[i], event);
         }
      }
   }
   DRETURN_VOID;
}

static void qevent_start_trigger_script(int qevent_event, const char* script_file, lListElem *event ) {
   u_long32 jobid, taskid;
   const char* event_name;
   int pid;
   char buffer[MAX_STRING_SIZE];
   char buffer2[MAX_STRING_SIZE];

   DENTER(TOP_LAYER);

   jobid  = lGetUlong(event, ET_intkey);
   taskid = lGetUlong(event, ET_intkey2);
   event_name = qevent_get_event_name(qevent_event);
   

   /* test if script is executable and valid file */
   if (!sge_is_file(script_file)) {
      ERROR("no script file: " SFQ, script_file);
      DRETURN_VOID;
   }

   /* is file executable ? */
   if (!sge_is_executable(script_file)) {  
      ERROR("file not executable: " SFQ, script_file);
      DRETURN_VOID;
   } 

   pid = fork();
   if (pid < 0) {
      ERROR("fork() error");
      DRETURN_VOID;
   }

   if (pid > 0) {
      int exit_status;
         struct rusage rusage{};
         int status;
         wait3(&status, 0, &rusage);
         exit_status = status;

      if ( WEXITSTATUS(exit_status) == 0 ) {
         INFO("exit status of script: %d\n", WEXITSTATUS(exit_status));
      } else {
         ERROR("exit status of script: %d\n", WEXITSTATUS(exit_status));
      }
      DRETURN_VOID;
   } else {
      const char *basename = sge_basename( script_file, '/' );
      /*      SETPGRP;  */
      /*      sge_close_all_fds(nullptr); */
      snprintf(buffer, sizeof(buffer), sge_u32, jobid);
      snprintf(buffer2, sizeof(buffer2), sge_u32, taskid);
      execlp(script_file, basename, event_name, buffer, buffer2, (char *) nullptr);
   }
   exit(1);
}

static void qevent_show_usage() {
   dstring ds;
   char buffer[256];
   
   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(stdout, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));
   fprintf(stdout, "%s\n", MSG_SRC_USAGE );

   fprintf(stdout,"qevent [-h|-help] -ts|-testsuite\n");
   fprintf(stdout,"qevent [-h|-help] -sm|-subscribe\n");
   fprintf(stdout,"qevent [-h|-help] -trigger EVENT SCRIPT [ -trigger EVENT SCRIPT, ... ]\n\n");
   
   fprintf(stdout,"   -h,  -help             show usage\n");
   fprintf(stdout,"   -ts, -testsuite        run in testsuite mode\n");
   fprintf(stdout,"   -sm, -subscribe        run in subscribe mode\n");
   fprintf(stdout,"   -trigger EVENT SCRIPT  start SCRIPT (executable) when EVENT occurs\n");
   fprintf(stdout,"\n");
   fprintf(stdout,"SCRIPT - path to a executable shell script\n");
   fprintf(stdout,"         1. command line argument: event name\n");
   fprintf(stdout,"         2. command line argument: jobid\n");
   fprintf(stdout,"         3. command line argument: taskid\n");
   fprintf(stdout,"EVENT  - One of the following event category:\n");
   fprintf(stdout,"         %s      - job end event\n", qevent_get_event_name(QEVENT_JB_END));
   fprintf(stdout,"         %s - job task end event\n", qevent_get_event_name(QEVENT_JB_TASK_END));
}


static void qevent_parse_command_line([[maybe_unused]] int argc, char **argv, qevent_options *option_struct) {

   
   DENTER(TOP_LAYER);

   option_struct->help_option = 0;
   option_struct->testsuite_option = 0;
   option_struct->subscribe_option = 0;
   option_struct->trigger_option_count =0;

   while (*(++argv)) {
      if (!strcmp("-h", *argv) || !strcmp("-help", *argv)) {
         option_struct->help_option = 1;
         continue;
      }
      if (!strcmp("-ts", *argv) || !strcmp("-testsuite", *argv)) {
         option_struct->testsuite_option = 1;
         continue;
      }
      if (!strcmp("-sm", *argv) || !strcmp("-subscribe", *argv)) {
         option_struct->subscribe_option = 1;
         continue;
      }
      if (!strcmp("-trigger", *argv)) {
         int ok = 0;
         if (option_struct->trigger_option_count >= MAX_TRIGGER_SCRIPTS ) {
            sge_dstring_sprintf(option_struct->error_message,
                                "option \"-trigger\": only %d trigger arguments supported\n",
                                MAX_TRIGGER_SCRIPTS);
            break; 
         }

         ++argv;
         if (*argv) {
            /* get EVENT argument */
            if (strcmp(qevent_get_event_name(QEVENT_JB_END),*argv) == 0) {
               ok = 1;
               (option_struct->trigger_option_events)[option_struct->trigger_option_count] = QEVENT_JB_END;
            } 
            if (strcmp(qevent_get_event_name(QEVENT_JB_TASK_END),*argv) == 0) {
               ok = 1;
               (option_struct->trigger_option_events)[option_struct->trigger_option_count] = QEVENT_JB_TASK_END;
            } 

            if (!ok) {
               sge_dstring_append(option_struct->error_message,"option \"-trigger\": undefined EVENT type\n");
               break; 
            }
         } else {
            sge_dstring_append(option_struct->error_message,"option \"-trigger\": found no EVENT argument\n");
            break;
         }
         ++argv;
         if (*argv) {
            /* get SCRIPT argument */

            /* check for SCRIPT file */
            if (!sge_is_file(*argv)) {
               sge_dstring_sprintf(option_struct->error_message,
                                   "option \"-trigger\": SCRIPT file %s not found\n",
                                   (*argv));
               break;
            }

            /* is file executable ? */
            if (!sge_is_executable(*argv)) {  
               sge_dstring_sprintf(option_struct->error_message,
                                   "option \"-trigger\": SCRIPT file %s not executable\n",
                                   (*argv));
               break;

            } 
 
            (option_struct->trigger_option_scripts)[option_struct->trigger_option_count] = *argv;
            (option_struct->trigger_option_count)++;
         } else {
            sge_dstring_append(option_struct->error_message,"option \"-trigger\": found no SCRIPT argument\n");
            break;
         }
         continue;
      }


      /* unknown option */
      if ( *argv[0] == '-' ) {  
         sge_dstring_append(option_struct->error_message,"unknown option: ");
         sge_dstring_append(option_struct->error_message,*argv);
         sge_dstring_append(option_struct->error_message,"\n");
      } else {
         sge_dstring_append(option_struct->error_message,"unknown argument: ");
         sge_dstring_append(option_struct->error_message,*argv);
         sge_dstring_append(option_struct->error_message,"\n");
      }
   } 
   DRETURN_VOID;
}

int main(int argc, char *argv[])
{
   DENTER_MAIN(TOP_LAYER, "qevent");
   qevent_options enabled_options;
   dstring errors = DSTRING_INIT;
   int i, gdi_setup;
   lList *alp = nullptr;
   sge_evc_class_t *evc = nullptr;

   sge_setup_sig_handlers(QEVENT);

   /* dump pid to file */
   qevent_dump_pid_file();

   /* parse command line */
   enabled_options.error_message = &errors;
   qevent_set_option_struct(&enabled_options);
   qevent_parse_command_line(argc, argv, &enabled_options);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   /* check if help option is set */
   if (enabled_options.help_option) {
      qevent_show_usage();
      sge_dstring_free(enabled_options.error_message);
      sge_exit(0);
   }

   /* are there command line parsing errors ? */
   if (sge_dstring_get_string(enabled_options.error_message)) {
      ERROR("%s", sge_dstring_get_string(enabled_options.error_message) );
      qevent_show_usage();
      sge_dstring_free(enabled_options.error_message);
      sge_exit(1);
   }


   // select primary DS
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::GLOBAL);

   /* setup event client */
   gdi_setup = ocs::gdi::ClientBase::setup_and_enroll(QEVENT, MAIN_THREAD, &alp);
   if (gdi_setup != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_dstring_free(enabled_options.error_message);
      sge_exit(1);
   }
   if (false == sge_gdi2_evc_setup(&evc, EV_ID_ANY, &alp, nullptr)) {
      answer_list_output(&alp);
      sge_dstring_free(enabled_options.error_message);
      sge_exit(1);
   }

   /* ok, start over ... */
   /* check for testsuite option */
   
   if (enabled_options.testsuite_option) {
      /* only for testsuite */
      qevent_testsuite_mode(evc);
      sge_dstring_free(enabled_options.error_message);
      sge_exit(0);
   }

   /* check for subscribe option */
   if (enabled_options.subscribe_option) {
      /* only for testsuite */
      qevent_subscribe_mode(evc);
      sge_dstring_free(enabled_options.error_message);
      sge_exit(0);
   }

   if (enabled_options.trigger_option_count > 0) {
      lCondition *where =nullptr;
      lEnumeration *what = nullptr;

      sge_mirror_initialize(evc, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
      evc->ec_set_busy_handling(evc, EV_BUSY_UNTIL_ACK);

      /* put out information about -trigger option */
      for (i=0;i<enabled_options.trigger_option_count;i++) {
         INFO("trigger script for %s events: %s\n", qevent_get_event_name((enabled_options.trigger_option_events)[i]), (enabled_options.trigger_option_scripts)[i]);
         switch((enabled_options.trigger_option_events)[i]) {
            case QEVENT_JB_END:
                  
                  /* build mask for the job structure to contain only the needed elements */
                  where = nullptr;
                  what = lWhat("%T(%I %I %I %I %I %I %I %I)", JB_Type, JB_job_number, JB_ja_tasks, 
                                                              JB_ja_structure, JB_ja_n_h_ids, JB_ja_u_h_ids, 
                                                              JB_ja_s_h_ids,JB_ja_o_h_ids, JB_ja_template);
                  
                  /* register for job events */ 
                  sge_mirror_subscribe(evc, SGE_TYPE_JOB, analyze_jatask_event, nullptr, nullptr, where, what);
                  evc->ec_set_flush(evc, sgeE_JOB_DEL,true, 1);

                  /* the mirror interface registers more events, than we need,
                     thus we free the ones, we do not need */
                /*  evc->ec_unsubscribe(evc, sgeE_JOB_LIST); */
                  evc->ec_unsubscribe(evc, sgeE_JOB_MOD);
                  evc->ec_unsubscribe(evc, sgeE_JOB_USAGE);
                  evc->ec_unsubscribe(evc, sgeE_JOB_FINAL_USAGE);
                  evc->ec_unsubscribe(evc, sgeE_JOB_FINISH);
               /*   evc->ec_unsubscribe(evc, sgeE_JOB_ADD); */

                  /* free the what and where mask */
                  lFreeWhere(&where);
                  lFreeWhat(&what);
               break;
            case QEVENT_JB_TASK_END:
            
                  /* build mask for the job structure to contain only the needed elements */
                  where = nullptr;
                  what = lWhat("%T(%I)", JAT_Type, JAT_status);
                  /* register for JAT events */ 
                  sge_mirror_subscribe(evc, SGE_TYPE_JATASK, analyze_jatask_event, nullptr, nullptr, where, what);
                  evc->ec_set_flush(evc, sgeE_JATASK_DEL,true, 1);
                  
                  /* the mirror interface registers more events, than we need,
                     thus we free the ones, we do not need */ 
                  evc->ec_unsubscribe(evc, sgeE_JATASK_ADD);
                  evc->ec_unsubscribe(evc, sgeE_JATASK_MOD);
                  /* free the what and where mask */
                  lFreeWhere(&where);
                  lFreeWhat(&what);
               break;
         }        
      }

      while(!shut_me_down) {
         sge_mirror_error error = sge_mirror_process_events(evc);
         if (error == SGE_EM_TIMEOUT && !shut_me_down ) {
            sleep(10);
            continue;
         }
      }

      sge_mirror_shutdown(evc);

      sge_dstring_free(enabled_options.error_message);
      sge_prof_cleanup();
      sge_exit(0);
      return 0;
   }


   ERROR("no option selected\n" );
   qevent_show_usage();
   sge_dstring_free(enabled_options.error_message);
   sge_prof_cleanup();
   sge_exit(1);
   return 1;
}

static const char* qevent_get_event_name(int event) {
   switch(event) {
      case QEVENT_JB_END:
         return "JB_END";
      case QEVENT_JB_TASK_END:
         return "JB_TASK_END";
   }
   return "unexpected event id";
}



static void qevent_testsuite_mode(sge_evc_class_t *evc) 
{
#ifndef QEVENT_SHOW_ALL
   u_long64 timestamp;
   lCondition *where =nullptr;
   lEnumeration *what = nullptr;
 
   const int job_nm[] = {       
         JB_job_number,
         JB_host,
         JB_category,            
         JB_project, 
         JB_ja_tasks,
         JB_ja_structure,
         JB_ja_n_h_ids,
         JB_ja_u_h_ids,
         JB_ja_s_h_ids,
         JB_ja_o_h_ids,   
         JB_ja_template,
         NoName
      };

   const int jat_nm[] = {     
      JAT_status, 
      JAT_task_number,
      NoName
   };  
#endif
   
   DENTER(TOP_LAYER);

   sge_mirror_initialize(evc, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

#ifdef QEVENT_SHOW_ALL
   sge_mirror_subscribe(evc, SGE_TYPE_ALL, print_event, nullptr, nullptr, nullptr, nullptr);
#else /* QEVENT_SHOW_ALL */
   where = nullptr;
   what =  lIntVector2What(JB_Type, job_nm); 
   sge_mirror_subscribe(evc, SGE_TYPE_JOB, print_jatask_event, nullptr, nullptr, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);
   
   where = nullptr;
   what = lIntVector2What(JAT_Type, jat_nm); 
   sge_mirror_subscribe(evc, SGE_TYPE_JATASK, print_jatask_event, nullptr, nullptr, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);

   where = nullptr;
   what =  lIntVector2What(JB_Type, job_nm);
   sge_mirror_subscribe(evc, SGE_TYPE_CATEGORY, print_jatask_event, nullptr, nullptr, nullptr, nullptr);
   lFreeWhere(&where);
   lFreeWhat(&what);

   /* we want a 5-second event delivery interval */
   evc->ec_set_edtime(evc, 5);

   /* and have our events flushed immediately */
   evc->ec_set_flush(evc, sgeE_JATASK_MOD, true, 1);
   evc->ec_set_flush(evc, sgeE_JOB_FINAL_USAGE, true, 1);
   evc->ec_set_flush(evc, sgeE_JOB_FINISH, true, 1);
   evc->ec_set_flush(evc, sgeE_JOB_ADD, true, 1);
   evc->ec_set_flush(evc, sgeE_JOB_DEL, true, 1);

   evc->ec_set_flush(evc, sgeE_CATEGORY_ADD, true, 1);
   evc->ec_set_flush(evc, sgeE_CATEGORY_MOD, true, 1);
   evc->ec_set_flush(evc, sgeE_CATEGORY_DEL, true, 1);

#endif /* QEVENT_SHOW_ALL */
   
   while (!shut_me_down) {
      sge_mirror_error error = sge_mirror_process_events(evc);
      if (error == SGE_EM_TIMEOUT && !shut_me_down) {
         sleep(10);
         continue;
      }

#ifndef QEVENT_SHOW_ALL
      timestamp = sge_get_gmt64();
      fprintf(stdout,"ECL_STATE (jobs_running=%ld:jobs_registered=%ld:ECL_TIME=" sge_u64 ")\n",
              Global_jobs_running, Global_jobs_registered, timestamp);
      fflush(stdout);  
#endif
   }

   sge_mirror_shutdown(evc);
   DRETURN_VOID;
}

/****** qevent/qevent_subscribe_mode() *****************************************
*  NAME
*     qevent_subscribe_mode() -- ??? 
*
*  SYNOPSIS
*     static void qevent_subscribe_mode(sge_evc_class_t *evc) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     sge_evc_class_t *evc - ??? 
*
*  RESULT
*     static void - 
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE: qevent_subscribe_mode() is not MT safe 
*
*  BUGS
*     ??? 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
static void qevent_subscribe_mode(sge_evc_class_t *evc) 
{
   int event_type = SGE_TYPE_ADMINHOST;
   
   DENTER(TOP_LAYER);

   sge_mirror_initialize(evc, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_SHUTDOWN, print_event, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_ADMINHOST, print_event, nullptr, nullptr, nullptr, nullptr);

   while(!shut_me_down) {
      sge_mirror_error error = sge_mirror_process_events(evc);
      if (evc != nullptr) {
         if (event_type < SGE_TYPE_NONE) {
            event_type++;
            printf("Subscribe event_type: %d\n", event_type);
            error = sge_mirror_subscribe(evc, (sge_object_type)event_type, print_event, nullptr, nullptr, nullptr, nullptr);
         } else {   
            event_type = SGE_TYPE_ADMINHOST;
            printf("Unsubscribe all event_types\n");
            error = sge_mirror_unsubscribe(evc, SGE_TYPE_ALL);
         }
      }   
      if (error == SGE_EM_TIMEOUT && !shut_me_down) {
         printf("error was SGE_EM_TIMEOUT\n");
         sleep(10);
         continue;
      }
   }

   sge_mirror_shutdown(evc);
   DRETURN_VOID;
}

