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
 * Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>

#include "uti/sge_arch.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"

#include "comm/commlib.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_usage.h"

#include "sgeobj/ocs_Version.h"
#include "sgeobj/sge_daemonize.h"

#include "job_report_execd.h"
#include "sge_load_sensor.h"
#include "load_avg.h"
#include "execd_ck_to_do.h"
#include "sge_report_execd.h"
#include "msg_execd.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#endif

#if defined(LINUX)
#   include <dlfcn.h>
#endif

static void 
get_reserved_usage(const char*qualified_hostname, lList **job_usage_list);
static int 
execd_add_load_report(lList *report_list, u_long64 now, u_long64 *next_send);
static int 
execd_add_conf_report(lList *report_list, u_long64 now, u_long64 *next_send);
static int 
execd_add_license_report(lList *report_list, u_long64 now, u_long64 *next_send);
static int 
execd_add_job_report(lList *report_list, u_long64 now, u_long64 *next_send);
static int 
sge_get_loadavg(const char *qualified_hostname, lList **lpp);

static int sge_get_topology(const char *qualified_hostname, lList **lpp);
static int sge_get_topology_inuse(const char *qualified_hostname, lList **lpp);
static int sge_get_sockets(const char *qualified_hostname, lList **lpp);
static int sge_get_cores(const char *qualified_hostname, lList **lpp);
static int sge_get_hwthreads(const char *qualified_hostname, lList **lpp);

report_source execd_report_sources[] = {
   { NUM_REP_REPORT_LOAD, execd_add_load_report , 0 },
   { NUM_REP_REPORT_CONF, execd_add_conf_report , 0 },
   { NUM_REP_REPORT_PROCESSORS, execd_add_license_report , 0 },
   { NUM_REP_REPORT_JOB, execd_add_job_report , 0 },
   { 0, nullptr }
};

lUlong sge_execd_report_seqno = 0;
u_long64 qmrestart_time = 0;
static bool delay_job_reports = false;
static bool send_all = true;
static lListElem *last_lr = nullptr;
static lList *lr_list = nullptr;

extern lList *jr_list;

static bool flush_lr = false;

u_long64 sge_get_qmrestart_time()
{
   return qmrestart_time;
}

/* record qmaster restart time, need for use in delayed_reporting */
void sge_set_qmrestart_time(u_long64 qmr)
{
   qmrestart_time = qmr;
}

bool sge_get_delay_job_reports_flag()
{
   return delay_job_reports;
}

void sge_set_delay_job_reports_flag(bool new_val)
{
   delay_job_reports = new_val;
}

bool sge_get_flush_lr_flag()
{
   return flush_lr;
}

void sge_set_flush_lr_flag(bool new_val)
{
   flush_lr = new_val;
}

void execd_merge_load_report(u_long32 seqno)
{
   if (last_lr == nullptr || seqno != lGetUlong(last_lr, REP_seqno)) {
      return;
   } else {
      const lListElem *old_lr;

      for_each_ep(old_lr, lGetList(last_lr, REP_list)) {
         const void *iterator = nullptr;
         const char *hostname = lGetHost(old_lr, LR_host);
         const char *name = lGetString(old_lr, LR_name);
         lListElem *lr, *lr_next;
         bool found = false;

         lr_next = lGetElemStrFirstRW(lr_list, LR_name, name, &iterator);
         while ((lr = lr_next)) {
            lr_next = lGetElemStrNextRW(lr_list, LR_name, name, &iterator);

            if (sge_hostcmp(lGetHost(lr, LR_host), hostname) == 0) {
               found = true;
               if (lGetUlong(old_lr, LR_is_static) == 2) {
                  lRemoveElem(lr_list, &lr); 
               } else {
                  lSetString(lr, LR_value, lGetString(old_lr, LR_value));
               }
               break;
            }
         }
         if (!found) {
            lAppendElem(lr_list, lCopyElem(old_lr));
         }
      }
   }
   lFreeElem(&last_lr);
}

void execd_trash_load_report() {
   send_all = true;
}

static int 
execd_add_load_report(lList *report_list, u_long64 now, u_long64 *next_send)
{
   const char* qualified_hostname = component_get_qualified_hostname();
   const char* binary_path = bootstrap_get_binary_path();

   DENTER(TOP_LAYER);

   if (*next_send <= now || sge_get_flush_lr_flag()) {
      lSortOrder *order = lParseSortOrderVarArg(LR_Type, "%I+", LR_host);
      lListElem *report;
      lList *tmp_lr_list;

      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());
      sge_set_flush_lr_flag(false);

      /*
      ** problem: add some more error handling here
      */
      /*
      ** 1. load report
      */
      report = lCreateElem(REP_Type);
      if (send_all) {
         lSetUlong(report, REP_type, NUM_REP_FULL_REPORT_LOAD);
      } else {
         lSetUlong(report, REP_type, NUM_REP_REPORT_LOAD);
      }
      lSetUlong(report, REP_version, ocs::Version::get_version());
      lSetUlong(report, REP_seqno, sge_execd_report_seqno);
      lSetHost(report, REP_host, qualified_hostname);

      tmp_lr_list = sge_build_load_report(qualified_hostname, binary_path);

      if (lr_list == nullptr) {
         lr_list = lCopyList("", tmp_lr_list);
         lFreeElem(&last_lr);
         lSortList(tmp_lr_list, order);
         lSetList(report, REP_list, tmp_lr_list);
      } else {
         lListElem *lr;

         for_each_rw(lr, lr_list) {
            const void *iterator = nullptr;
            const char *hostname = lGetHost(lr, LR_host);
            const char *name = lGetString(lr, LR_name);
            const char *value = lGetString(lr, LR_value);
            lListElem *ep, *next_ep;
            bool found = false;
         
            next_ep = lGetElemStrFirstRW(tmp_lr_list, LR_name, name, &iterator);
            while ((ep = next_ep)) {
               next_ep = lGetElemStrNextRW(tmp_lr_list, LR_name, name, &iterator);

               DPRINTF("handling %s in execd_add_load_report\n", name);
               if (sge_hostcmp(lGetHost(ep, LR_host), hostname) == 0) {
                  /* we found the same load value in the temp list */
                  found = true;

                  if (!send_all && sge_strnullcmp(lGetString(ep, LR_value), value) == 0) {
                     /* value hasn't changed, remove it from list */ 
                     lRemoveElem(tmp_lr_list, &ep);
                  } else {
                     DPRINTF("value %s has changed from %s to %s\n", name, value ? value:"nullptr", lGetString(ep, LR_value));
                     /* Old value is no longer valid */
                     lSetString(lr, LR_value, nullptr);
                  }
                  break;
               }
            }

            if (!found) {
               /* the load value is no longer reported, tag is as deleted and
                  add it to the report list */
               lListElem *del_report = lCopyElem(lr);     
               lSetUlong(del_report, LR_is_static, 2); 
               lAppendElem(tmp_lr_list, del_report);
            }
         }

         lSortList(tmp_lr_list, order);
         lSetList(report, REP_list, tmp_lr_list);
         lFreeElem(&last_lr);
         last_lr = lCopyElem(report);
      }
      lFreeSortOrder(&order);

      lAppendElem(report_list, report);
      send_all = false;
   }

   DRETURN(0);
}


static int 
execd_add_conf_report(lList *report_list, u_long64 now, u_long64 *next_send)
{
   const char* qualified_hostname = component_get_qualified_hostname();

   DENTER(TOP_LAYER);
   if (*next_send <= now) {
      lListElem *report;

      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());

      /*
      ** 2. report about the configuration versions
      ** that the exec daemon has right now
      ** order of the reports is irrelevant
      */
      report = lCreateElem(REP_Type);
      lSetUlong(report, REP_type, NUM_REP_REPORT_CONF);
      lSetUlong(report, REP_version, ocs::Version::get_version());
      lSetUlong(report, REP_seqno, sge_execd_report_seqno);
      lSetHost(report, REP_host, qualified_hostname);
      lSetList(report, REP_list, 
         lCopyList("execd config list copy", Execd_Config_List));
      lAppendElem(report_list, report);
      DPRINTF("handling conf report in execd_add_conf_report\n");
   }
   DRETURN(0);
}

static int 
execd_add_license_report(lList *report_list, u_long64 now, u_long64 *next_send)
{
   DENTER(TOP_LAYER);
   if (*next_send == 0) {
      const char* qualified_hostname = component_get_qualified_hostname();
      lListElem *report;

      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());

      /*
      ** 3. license report
      */
      report = lCreateElem(REP_Type);
      lSetUlong(report, REP_type, NUM_REP_REPORT_PROCESSORS);
      lSetUlong(report, REP_version, ocs::Version::get_version());
      lSetUlong(report, REP_seqno, sge_execd_report_seqno);
      lSetHost(report, REP_host, qualified_hostname);
      {
         lList *lp_lic;
         lListElem *ep_lic;
         
         lp_lic = lCreateList("license report list", LIC_Type);
         ep_lic = lCreateElem(LIC_Type);
         /*
         ** no error handling here cause we can just send 1 as #processors
         */
         lSetUlong(ep_lic, LIC_processors,  sge_nprocs());
         lSetString(ep_lic, LIC_arch, sge_get_arch());
         lAppendElem(lp_lic, ep_lic);
         lSetList(report, REP_list, lp_lic);
      }
      lAppendElem(report_list, report);
      DPRINTF("handling license report in execd_add_license_report\n");
   }
   DRETURN(0);
}

static int 
execd_add_job_report(lList *report_list, u_long64 now, u_long64 *next_send)
{
   bool do_send = false;
   bool only_flush = false;
   static u_long64 last_send = 0;
   const char* qualified_hostname = component_get_qualified_hostname();

   DENTER(TOP_LAYER);

   /* return if no job reports are in the list */
   if (lGetNumberOfElem(jr_list) == 0) {
      DRETURN(0);
   }

   /* if report interval expired: send all reports */
   if (*next_send <= now) {
      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());
      do_send = true;
   } else if (sge_get_flush_jr_flag()) {
      /* if we shall flush reports: send only reports marked to flush */
      do_send = true;
      only_flush = true;
   }

   /* 
    * send only one report message per second 
    * we do send empty reports - they trigger rescheduling of a job
    * to the same host on which it executed before.
    */
   if (do_send && (last_send < now)) {
      lListElem *job_report;
      lList *job_report_list;
      lListElem *jr;

      /* remember last send time */
      last_send = now;

      /* create job report */
      job_report = lCreateElem(REP_Type);
      lSetUlong(job_report, REP_type, NUM_REP_REPORT_JOB);
      lSetUlong(job_report, REP_version, ocs::Version::get_version());
      lSetUlong(job_report, REP_seqno, sge_execd_report_seqno);
      lSetHost(job_report, REP_host, qualified_hostname);

      /* create job report list */
      job_report_list = lCreateList("jr", JR_Type);
      lSetList(job_report, REP_list, job_report_list);

      /* copy reports (all or only to flush) 
       * We keep job reports where the job is started via JAPI (qsub -sync or
       * DRMAA). This is done only when we reconnect after a qmaster failover
       */
      for_each_rw (jr, jr_list) {
         if ((!only_flush || lGetBool(jr, JR_flush)) &&
               !lGetBool(jr, JR_no_send) &&
               !(sge_get_delay_job_reports_flag() && lGetBool(jr, JR_delay_report))) {
            lAppendElem(job_report_list, lCopyElem(jr));
            lSetBool(jr, JR_flush, false);
         } /* else not sending report for this job */
      }
     
      /* append this new job report to the report list */
      lAppendElem(report_list, job_report);

      /* now all is sent, reset flush_jr */
      sge_set_flush_jr_flag(false);
   }
   DRETURN(0);
}

lList *sge_build_load_report(const char* qualified_hostname, const char* binary_path)
{
   lList *lp = nullptr;
   const lListElem *ep;
   int nprocs = 1;
   double load;
   const void *iterator = nullptr;

   DENTER(TOP_LAYER);

   /* 
      adding load values to the load report 
      overwrites load values that are still
      in the load list; 
      so we first validate external load values
      then we get internal load values 
      overwriting the external values

   */

   /* build up internal report list */
   sge_switch2start_user();
   sge_get_loadavg(qualified_hostname, &lp);
   sge_switch2admin_user(); 

   /* report topology reporting */
   sge_get_topology(qualified_hostname, &lp);
   sge_get_topology_inuse(qualified_hostname, &lp);
   sge_get_sockets(qualified_hostname, &lp);
   sge_get_cores(qualified_hostname, &lp);
   sge_get_hwthreads(qualified_hostname, &lp);

   /* get load report from external load sensor */
   sge_ls_get(qualified_hostname, binary_path, &lp);

   /* make derived load values */
   /* retrieve num procs first - we need it for all other derived load values */
   ep = lGetElemStrFirst(lp, LR_name, LOAD_ATTR_NUM_PROC, &iterator);
   while (ep != nullptr) {
      const char *value = lGetString(ep, LR_value);

      if (sge_hostcmp(lGetHost(ep, LR_host), qualified_hostname) == 0) {
         if (value) {
            nprocs = MAX(1, atoi(value));
         }
         break;
      }   
      ep = lGetElemStrNext(lp, LR_name, LOAD_ATTR_NUM_PROC, &iterator);
   }

   /* now make the derived load values */
   ep = lGetElemHostFirst(lp, LR_host, qualified_hostname, &iterator);
   while (ep != nullptr) {
      const char *name = lGetString(ep, LR_name);
      const char *value = lGetString(ep, LR_value);

      if (strcmp(name, LOAD_ATTR_LOAD_AVG) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_AVG, (load/nprocs), qualified_hostname, nullptr);
         }
      } else if (strcmp(name, LOAD_ATTR_LOAD_SHORT) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_SHORT, (load/nprocs), 
                                          qualified_hostname, nullptr);
         }
      } else if (strcmp(name, LOAD_ATTR_LOAD_MEDIUM) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_MEDIUM, (load/nprocs), 
                                          qualified_hostname, nullptr);
         }
      } else if (strcmp(name, LOAD_ATTR_LOAD_LONG) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(&lp, LOAD_ATTR_NP_LOAD_LONG, (load/nprocs), 
                                          qualified_hostname, nullptr);
         }
      }

      ep = lGetElemHostNext(lp, LR_host, qualified_hostname, &iterator);
   }

   /* qmaster expects a list sorted by host */
   lPSortList(lp, "%I+", LR_host);

   DRETURN(lp);
}

/****** load_avg/sge_get_sockets() *********************************************
*  NAME
*     sge_get_sockets() -- Appends the amount of sockets on a Linux platform 
*
*  SYNOPSIS
*     static int sge_get_sockets(const char* qualified_hostname, lList **lpp) 
*
*  FUNCTION
*     Appends to the given list of load values the amount of sockets on 
*     Linux platform only
*
*  INPUTS
*     const char* qualified_hostname - Hostname 
*     lList **lpp                    - List with load values 
*
*  RESULT
*     static int - 
*
*  NOTES
*     MT-NOTE: sge_get_sockets() is MT safe 
*
*******************************************************************************/
static int sge_get_sockets(const char* qualified_hostname, lList **lpp) {
  
   int sockets = 0;
   
   DENTER(TOP_LAYER);
   
   /* get total amount of sockets installed on system */ 
   sockets = get_execd_amount_of_sockets();
   
   /* append the amount of sockets to the load report list */
   sge_add_int2load_report(lpp, LOAD_ATTR_SOCKETS, sockets, qualified_hostname);

   DRETURN(0);
}

/****** load_avg/sge_get_cores() ***********************************************
*  NAME
*     sge_get_cores() -- ??? 
*
*  SYNOPSIS
*     static int sge_get_cores(const char* qualified_hostname, lList **lpp) 
*
*  FUNCTION
*     Appends to the given list of load values the amount of cores 
*     on current system (Linux platform only). For other OSs it is 0.
*
*  INPUTS
*     const char* qualified_hostname - Hostname 
*     lList **lpp                    - List with load values 
*
*  RESULT
*     static int - 0 if everything was ok
*
*  NOTES
*     MT-NOTE: sge_get_cores() is MT safe 
*
*******************************************************************************/
static int sge_get_cores(const char* qualified_hostname, lList **lpp) {
   
   int cores = 0;
   
   DENTER(TOP_LAYER);

   /* get the total amount of cores */
   cores = get_execd_amount_of_cores();
   
   /* append the amount of cores to the list */
   sge_add_int2load_report(lpp, LOAD_ATTR_CORES, cores, qualified_hostname);
   
   DRETURN(0);
}

/****** load_avg/sge_get_hwthreads() ***********************************************
*  NAME
*     sge_get_hwthreads() -- Sets the amount of threads.
*
*  SYNOPSIS
*     static int sge_get_hwthreads(const char* qualified_hostname, lList **lpp)
*
*  FUNCTION
*     Appends to the given list of load values the amount of threads.
*
*  INPUTS
*     const char* qualified_hostname - name of the host
*     lList **lpp                    - list with load values
*
*  RESULT
*     static int - 0 per default
*
*  NOTES
*     MT-NOTE: sge_get_hwthreads() is MT safe
*
*******************************************************************************/

static int sge_get_hwthreads(const char* qualified_hostname, lList **lpp)
{
   int hwthreads = 0;
   DENTER(TOP_LAYER);

   /* get the total amount of threads */
   hwthreads = get_execd_amount_of_threads();
   /* append the amount of threads to the list */
   sge_add_int2load_report(lpp, LOAD_ATTR_THREADS, hwthreads, qualified_hostname);

   DRETURN(0);
}

static int sge_get_topology(const char* qualified_hostname, lList **lpp) {

   /* Because the linux topology is a list with an undefined length 
     (each socket could have a differnt amount of cores, so we need 
     for each socket the amount of cores) we put the whole topology 
     in a String value. 
     The format is following: SCCSCC 
            s(cc)s(cc) or sccscc or 22? 
     This means that we have a 2 socket machine with 2 cores on 
     socket one (or 0 when you start at 0) and 2 cores on socket 
     two.
     When threads could be detected this could be changed to 
           SCTTCTTSCTTCTT 
   */

   /* pointer to topology string */
   char* topology = nullptr;
   int topology_length = 0;

   DENTER(TOP_LAYER);
   
   if (get_execd_topology(&topology, &topology_length)) {
      /* add topology to return value */
      sge_add_str2load_report(lpp, LOAD_ATTR_TOPOLOGY, topology, qualified_hostname);

   } else {
      /* add 'NONE' as topology in case of only error */
      sge_add_str2load_report(lpp, LOAD_ATTR_TOPOLOGY, "NONE", qualified_hostname);
   }

   sge_free(&topology);

   DRETURN(0);
}

static int sge_get_topology_inuse(const char* qualified_hostname, lList **lpp) {
   
   /* this topology is the topology in use which means that 
      characters like S C T are showing not (fully) occupied resources 
      while s c t are showing that they are used */
   
   /* pointer to topology string */
   char* topology = nullptr;
   
   DENTER(TOP_LAYER);

   if (get_execd_topology_in_use(&topology)) {
      /* add topology to return value */
      sge_add_str2load_report(lpp, LOAD_ATTR_TOPOLOGY_INUSE, topology, qualified_hostname);

   } else {
      /* add 'NONE' as topology in case of only error */
      sge_add_str2load_report(lpp, LOAD_ATTR_TOPOLOGY_INUSE, "NONE", qualified_hostname);
   }

   sge_free(&topology);
   
   DRETURN(0);
}

static int sge_get_loadavg(const char* qualified_hostname, lList **lpp) 
{
   double avg[3];
   int loads;
   int nprocs; 
#ifdef SGE_LOADMEM
   sge_mem_info_t mem_info;
#endif

   DENTER(TOP_LAYER);

   loads = sge_getloadavg(avg, 3);
   nprocs = sge_nprocs();
   if (loads == -1) {
      // log warning only every 2 hours
      static u_long64 next_log = 0;
      u_long64 now = sge_get_gmt64();

      if (now >= next_log) {
         WARNING(SFNMAX, MSG_SGETEXT_NO_LOAD);
         next_log = now + sge_gmt32_to_gmt64(7200);
      }
   } else if (loads == -2) {
      static bool logged_at_startup = false;
      if (!logged_at_startup) {
         logged_at_startup = true;
         WARNING(MSG_LS_USE_EXTERNAL_LS_S, sge_get_arch());
      }
   }

   /* build a list of load values */
   if (loads >= 0) {
      DPRINTF("---> %f %f %f - %d\n", avg[0], avg[1], avg[2], (int) (avg[2] * 100.0));

      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_AVG, avg[1], qualified_hostname, nullptr);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_SHORT, avg[0], qualified_hostname, nullptr);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_MEDIUM, avg[1], qualified_hostname, nullptr);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_LONG, avg[2], qualified_hostname, nullptr);
   }

   /* these are some static load values */
   sge_add_str2load_report(lpp, LOAD_ATTR_ARCH, sge_get_arch(), qualified_hostname);
   sge_add_int2load_report(lpp, LOAD_ATTR_NUM_PROC, nprocs, qualified_hostname);

#ifdef SGE_LOADMEM
   /* memory load report */
   memset(&mem_info, 0, sizeof(sge_mem_info_t));
   if (sge_loadmem(&mem_info)) {
      static int mem_fail = 0;
      if (!mem_fail) {
         ERROR(SFNMAX, MSG_LOAD_NOMEMINDICES);
         mem_fail =1;
      }
      DRETURN(1);
   }

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_FREE,        mem_info.mem_free, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_FREE,       mem_info.swap_free, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_FREE,    mem_info.mem_free  + mem_info.swap_free,
   qualified_hostname,"M");

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_TOTAL,       mem_info.mem_total, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_TOTAL,      mem_info.swap_total, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_TOTAL,   mem_info.mem_total + mem_info.swap_total,
   qualified_hostname, "M");

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_USED,        mem_info.mem_total - mem_info.mem_free, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_USED,       mem_info.swap_total - mem_info.swap_free,
   qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_USED,    (mem_info.mem_total + mem_info.swap_total)- 
                                          (mem_info.mem_free  + mem_info.swap_free), qualified_hostname, "M");

#if 0
   /* identical to "virtual_free" */
   if (!getenv("SGE_MAP_LOADVALUE")) {
      sge_add_double2load_report(lpp, "s_vmem",          mem_info.mem_total + mem_info.swap_total, qualified_hostname, "M");
      sge_add_double2load_report(lpp, "h_vmem",          mem_info.mem_total + mem_info.swap_total, qualified_hostname, "M");
   }
#endif
#endif /* SGE_LOADMEM */

#ifdef SGE_LOADCPU
   /* this code can cause nasty problems
      this is a hack to work around the problem */
   {
      double cpu_percentage;

      /* sge_getcpuload() must be called multiple 
         before it can return correct load values */
      static int first_time = 1;
      if (first_time) {
         sge_getcpuload(&cpu_percentage);
         first_time = 0;
      }

      if (sge_getcpuload(&cpu_percentage) != -1) {
         sge_add_double2load_report(lpp, "cpu", cpu_percentage, qualified_hostname, nullptr);
      } else {
         static u_long64 next_log2 = 0;

         u_long64 now = sge_get_gmt64();
         if (now >= next_log2) {
            WARNING(SFNMAX, MSG_SGETEXT_NO_LOAD);
            next_log2 = now + sge_gmt32_to_gmt64(7200);
         }
      }
   }
#endif /* SGE_LOADCPU */

   DRETURN(0);
}

void update_job_usage(const char* qualified_hostname)
{
   DENTER(TOP_LAYER);

   if (mconf_get_simulate_jobs()) {
      lListElem *jr;

      for_each_rw(jr, jr_list) {
         add_usage(jr, USAGE_ATTR_CPU, nullptr, 0.1);
         add_usage(jr, USAGE_ATTR_MEM, nullptr, 0.1);
         add_usage(jr, USAGE_ATTR_IO, nullptr, 0.0);
         add_usage(jr, USAGE_ATTR_IOW, nullptr, 0.0);
         add_usage(jr, USAGE_ATTR_VMEM, nullptr, 256);
         add_usage(jr, USAGE_ATTR_MAXVMEM, nullptr, 256);
         add_usage(jr, USAGE_ATTR_RSS, nullptr, 234);
         add_usage(jr, USAGE_ATTR_MAXRSS, nullptr, 234);
      }
      DRETURN_VOID;
   }

   lList *usage_list = nullptr;
#ifdef COMPILE_DC
   if (!mconf_get_sharetree_reserved_usage()) {
      int ptf_error;

      if ((ptf_error=ptf_get_usage(&usage_list))) {
         ERROR(MSG_LOAD_NOPTFUSAGE_S, ptf_errstr(ptf_error));
         /*
            use the old usage values in job report or none
            in case this is the first call to ptf_get_usage()
            since a new job was started
         */
         DRETURN_VOID;
      }
   }
#endif

   if (mconf_get_sharetree_reserved_usage()) {
      get_reserved_usage(qualified_hostname, &usage_list);
   }

   if (usage_list == nullptr) {
      DRETURN_VOID;
   }

   if (lGetNumberOfElem(usage_list) == 0) {
      /* could be an empty list head */
      lFreeList(&usage_list);
      DRETURN_VOID;
   }

#ifdef COMPILE_DC
#ifdef DEBUG_DC
   ptf_show_registered_jobs();
#endif
#endif

   /* replace existing usage in the job report with the new one */
   const lListElem *usage;
   for_each_ep(usage, usage_list) {
      u_long32 job_id;
      const lListElem *ja_task;

      job_id = lGetUlong(usage, JB_job_number);

      for_each_ep(ja_task, lGetList(usage, JB_ja_tasks)) {
         u_long32 ja_task_id;
         const lListElem *uep;
         const lListElem *pe_task;

         ja_task_id = lGetUlong(ja_task, JAT_task_number);
         /* search matching job report */
         lListElem *jr;
         if (!(jr = get_job_report(job_id, ja_task_id, nullptr))) {
            /* should not happen in theory */
            ERROR("removing unreferenced job " sge_u32 "." sge_u32 " without job report from ptf", job_id ,ja_task_id );
#ifdef COMPILE_DC
            ptf_unregister_registered_job(job_id ,ja_task_id);
#endif
            continue;
         }

         /* JG: TODO: make a function updating all load values in job report */
         /* replace cpu/mem/io with newer values */
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_CPU, JAT_usage_list))) {
            DPRINTF("added/updated 'cpu' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_CPU, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_MEM, JAT_usage_list))) {
            DPRINTF("added/updated 'mem' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_MEM, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_IO, JAT_usage_list))) {
            DPRINTF("added/updated 'io' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_IO, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_IOW, JAT_usage_list))) {
            DPRINTF("added/updated 'iow' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_IOW, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_VMEM, JAT_usage_list))) {
            DPRINTF("added/updated 'vmem' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_VMEM, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_MAXVMEM, JAT_usage_list))) {
            DPRINTF("added/updated 'maxvmem' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_MAXVMEM, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_RSS, JAT_usage_list))) {
            DPRINTF("added/updated 'rss' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_RSS, nullptr, lGetDouble(uep, UA_value));
         }
         if ((uep = lGetSubStr(ja_task, UA_name, USAGE_ATTR_MAXRSS, JAT_usage_list))) {
            DPRINTF("added/updated 'maxrss' usage: %f\n", lGetDouble(uep, UA_value));
            add_usage(jr, USAGE_ATTR_MAXRSS, nullptr, lGetDouble(uep, UA_value));
         }

         DPRINTF("---> updating job report usage for job " sge_u32 "." sge_u32 "\n", job_id, ja_task_id);

         for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
            const char *pe_task_id = lGetString(pe_task, PET_id);

            /* search matching job report */
            if (!(jr = get_job_report(job_id, ja_task_id, pe_task_id))) {
               /* should not happen in theory */
               ERROR("could not find job report for job " sge_u32"." sge_u32" " "task " SFN " contained in job usage from ptf", job_id, ja_task_id, pe_task_id);
#ifdef COMPILE_DC
#ifdef DEBUG_DC
               ptf_show_registered_jobs();
#endif
#endif
               continue;
            }

            /* replace cpu/mem/io with newer values */
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_CPU, PET_usage))) {
               DPRINTF("added/updated 'cpu' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_CPU, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_MEM, PET_usage))) {
               DPRINTF("added/updated 'mem' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_MEM, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_IO, PET_usage))) {
               DPRINTF("added/updated 'io' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_IO, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_IOW, PET_usage))) {
               DPRINTF("added/updated 'iow' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_IOW, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_VMEM, PET_usage))) {
               DPRINTF("added/updated 'vmem' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_VMEM, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_MAXVMEM, PET_usage))) {
               DPRINTF("added/updated 'maxvmem' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_MAXVMEM, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_RSS, PET_usage))) {
               DPRINTF("added/updated 'rss' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_RSS, nullptr, lGetDouble(uep, UA_value));
            }
            if ((uep = lGetSubStr(pe_task, UA_name, USAGE_ATTR_MAXRSS, PET_usage))) {
               DPRINTF("added/updated 'maxrss' usage: %f\n", lGetDouble(uep, UA_value));
               add_usage(jr, USAGE_ATTR_MAXRSS, nullptr, lGetDouble(uep, UA_value));
            }

            DPRINTF("---> updating job report usage for job " sge_u32 "." sge_u32" task \"%s\"\n",
                job_id, ja_task_id, pe_task_id);

         }
      }
   }
   lFreeList(&usage_list);

   DRETURN_VOID;
}

// @brief calculate the reserved memory
//
// @param queue the queue element
// @param nslots the number of slots
// @param h_nm the name of the hard resource queue limit
// @param s_nm the name of the soft resource queue limit
//
// @return the reserved memory
static double
calculate_reserved_memory(lListElem *queue, int nslots, int h_nm, int s_nm)
{
   double mem = 0.0;


   if (queue != nullptr) {
      double lim, h_mem_lim, s_mem_lim;
      char err_str[128];

      parse_ulong_val(&h_mem_lim, nullptr, TYPE_MEM,
                      lGetString(queue, h_nm),
                      err_str, sizeof(err_str)-1);

      parse_ulong_val(&s_mem_lim, nullptr, TYPE_MEM,
                      lGetString(queue, s_nm),
                      err_str, sizeof(err_str)-1);

      lim = MIN(h_mem_lim, s_mem_lim);

      /* INFINITY is mapped to DBL_MAX -> use 0; we cannot account INFINITY! */
      if (lim == DBL_MAX) {
         lim = 0.0;
      }

      mem = lim * nslots;
   }

   return mem;
}

static lList *
calculate_reserved_usage(const char* qualified_hostname, const lListElem *ja_task, const lListElem *pe_task,
                         u_long32 job_id, u_long32 ja_task_id, 
                         const char *pe_task_id,
                         const lListElem *pe, u_long64 now)
{
   lList *ul = nullptr;
   lListElem *jr;

   /* We only build new reserved online usage,
    * when the final (acct) usage has not yet been generated.
    * Otherwise, online usage might get higher than final usage!
    */
   jr = get_job_report(job_id, ja_task_id, pe_task_id);
   if (lGetSubStr(jr, UA_name, USAGE_ATTR_CPU_ACCT, JR_usage) == nullptr) {
      double cpu = 0, mem = 0, io = 0, iow = 0, maxvmem = 0, maxrss = 0;
      double wall_clock_time;

      build_reserved_usage(now, ja_task, pe_task, &wall_clock_time, &cpu, &mem, &maxvmem, &maxrss);

      io = iow = 0.0;

#ifdef COMPILE_DC
      {
         /* use PDC actual I/O if available */
         lList *jul;
         const lListElem *uep;
         if ((jul=ptf_get_job_usage(job_id, ja_task_id, pe_task_id))) {
            io = ((uep=lGetElemStr(jul, UA_name, USAGE_ATTR_IO))) ?
                     lGetDouble(uep, UA_value) : 0;
            iow = ((uep=lGetElemStr(jul, UA_name, USAGE_ATTR_IOW))) ?
                     lGetDouble(uep, UA_value) : 0;
            lFreeList(&jul);
         }
      }
#endif

      /* create the reserved usage list */
      ul = lCreateList("usage_list", UA_Type);

      usage_list_set_double_usage(ul, USAGE_ATTR_CPU, cpu);
      usage_list_set_double_usage(ul, USAGE_ATTR_MEM, mem);
      usage_list_set_double_usage(ul, USAGE_ATTR_IO, io);
      usage_list_set_double_usage(ul, USAGE_ATTR_IOW, iow);

      /* for reserved usage, we assume that the job always
       * consumes the maximum allowed memory (by h_vmem/s_vmem)
       */
      if (maxvmem != 0) {
         usage_list_set_double_usage(ul, USAGE_ATTR_VMEM, maxvmem);
         usage_list_set_double_usage(ul, USAGE_ATTR_MAXVMEM, maxvmem);
      }
      if (maxrss != 0) {
         usage_list_set_double_usage(ul, USAGE_ATTR_RSS, maxrss);
         usage_list_set_double_usage(ul, USAGE_ATTR_MAXRSS, maxrss);
      }
   }

   return ul;
}

static lListElem *
calculate_reserved_usage_ja_task(const char* qualified_hostname, const lListElem *ja_task, 
                                 u_long32 job_id, u_long32 ja_task_id, 
                                 const lListElem *pe, u_long64 now,
                                 lListElem *new_job) 
{
   lList *usage_list;
   lListElem *new_ja_task;

   /* create data structures */
   if (new_job == nullptr) {
      new_job = lCreateElem(JB_Type);
      lSetUlong(new_job, JB_job_number, job_id);
   }

   new_ja_task = lAddSubUlong(new_job, JAT_task_number, ja_task_id, 
                              JB_ja_tasks, JAT_Type); 

   usage_list = calculate_reserved_usage(qualified_hostname, ja_task, nullptr,
                                         job_id, ja_task_id, nullptr,
                                         pe, now);
  
   lSetList(new_ja_task, JAT_usage_list, usage_list);

   return new_job;
}

static lListElem *
calculate_reserved_usage_pe_task(const char* qualified_hostname, 
                                 const lListElem *ja_task, 
                                 const lListElem *pe_task,
                                 u_long32 job_id, u_long32 ja_task_id, 
                                 const char *pe_task_id, 
                                 const lListElem *pe, u_long64 now,
                                 lListElem *new_job) 
{
   lListElem *new_ja_task, *new_pe_task;
   lList *usage_list;

   /* create data structures */
   if (new_job == nullptr) {
      new_job = lCreateElem(JB_Type);
      lSetUlong(new_job, JB_job_number, job_id);
   }

   new_ja_task = lGetElemUlongRW(lGetList(new_job, JB_ja_tasks), JAT_task_number, ja_task_id);
   
   if (new_ja_task == nullptr) {
      new_ja_task = lAddSubUlong(new_job, JAT_task_number, ja_task_id, JB_ja_tasks, JAT_Type); 
   }

   new_pe_task = lAddSubStr(new_ja_task, PET_id, pe_task_id, JAT_task_list, PET_Type);
   
   usage_list = calculate_reserved_usage(qualified_hostname, ja_task, pe_task, 
                                         job_id, ja_task_id, pe_task_id, pe, now);
  
   lSetList(new_pe_task, PET_usage, usage_list);

   return new_job;
}

/* calculate reserved resource usage */
static void get_reserved_usage(const char *qualified_hostname, lList **job_usage_list)
{
   lList *temp_job_usage_list;
   const lListElem *job;
   lEnumeration *what;

   DENTER(TOP_LAYER);

   u_long64 now = sge_get_gmt64();
   what = lWhat("%T(%I %I)", JB_Type, JB_job_number, JB_ja_tasks);
   /* JG: TODO: why use JB_Type etc.? We only need an object containing
    * job_id, ja_task_id, pe_task_id and a usage_list.
    * Same structure is delivered from PTF.
    */

   temp_job_usage_list = lCreateList("JobResUsageList", JB_Type);

   for_each_ep(job, *ocs::DataStore::get_master_list(SGE_TYPE_JOB)) {
      u_long32 job_id;
      const lListElem *pe, *ja_task;
      lListElem *new_job = nullptr;

      job_id = lGetUlong(job, JB_job_number);

      for_each_ep(ja_task, lGetList(job, JB_ja_tasks)) {
         u_long32 ja_task_id;
         const lListElem *pe_task;

         ja_task_id = lGetUlong(ja_task, JAT_task_number);

         /* we need the pe to be able to calculate the number of slots used
          * as multiplication factor for usage
          */
         pe = lGetObject(ja_task, JAT_pe_object);

         /* If we have a pid for the ja_task: it's either a non-parallel job
          * or the master task of a parallel job.
          * Produce a usage record for it.
          */
         if (lGetUlong(ja_task, JAT_pid) != 0) { 
            new_job = calculate_reserved_usage_ja_task(qualified_hostname, ja_task,
                                                       job_id, ja_task_id,
                                                       pe, now, new_job);
         }

         /* If we have pe tasks (tightly integrated): Produce a usage record
          * for each of them.
          * Do not report reserved usage, if accounting_summary is activated!
          */
         if (!(pe != nullptr && lGetBool(pe, PE_accounting_summary))) {
            for_each_ep(pe_task, lGetList(ja_task, JAT_task_list)) {
               const char *pe_task_id;

               pe_task_id = lGetString(pe_task, PET_id);
               new_job = calculate_reserved_usage_pe_task(qualified_hostname, 
                                                          ja_task, pe_task, 
                                                          job_id, ja_task_id, 
                                                          pe_task_id, 
                                                          pe, now, new_job);
            }
         }
      }

      if (new_job != nullptr) {
         lAppendElem(temp_job_usage_list, new_job);
      }
   }

   *job_usage_list = lSelect("PtfJobUsageList", temp_job_usage_list, nullptr,
                             what);

   lFreeList(&temp_job_usage_list);
   lFreeWhat(&what);

   DRETURN_VOID;
}

static void build_reserved_mem_usage(const lListElem *gdil_ep, int slots, double wallclock, double *mem, double *maxvmem,
                                      double *maxrss)
{
   /* 
    * sum up memory usage (integral current memory * wallclock time)
    * and maxvmem (assume it is vmem)
    */
   lListElem *queue = lGetObject(gdil_ep, JG_queue);
   double vmem = calculate_reserved_memory(queue, slots, QU_h_vmem, QU_s_vmem);
   *mem += vmem * wallclock / (1024*1024*1024);
   *maxvmem += vmem;
   *maxrss += calculate_reserved_memory(queue, slots, QU_h_rss, QU_s_rss);
}

/****** load_avg/build_reserved_usage() ****************************************
*  NAME
*     build_reserved_usage() -- calculate reserved usage for job or pe task
*
*  SYNOPSIS
*     void build_reserved_usage(const u_long32 now, const lListElem *ja_task, 
*                               const lListElem *pe_task, double *wallclock, 
*                               double *cpu, double *mem, double *maxvmem) 
*
*  FUNCTION
*     Computes reserved usage for a job (array task) or the task of a tightly 
*     integrated parallel job.
*     The following values are computed and returned via call by reference:
*     - wallclock time (current time - start time)
*     - memory usage (integral of current memory usage times wallclock time)
*       This can only be computed if the job requests memory (h_vmem/s_vmem).
*     - maxvmem (assume the job will consume as much memory as possible (as
*       requested by h_vmem or s_vmem).
*
*  INPUTS
*     const u_long32 now       - current time
*     const lListElem *ja_task - job array task
*     const lListElem *pe_task - parallel task, or nullptr for job ja task
*
*  RESULT
*     double *wallclock        - returns the wallclock time
*     double *cpu              - returns the reserved cpu usage
*     double *mem              - returns the reserved memory (integral vmem * wallclock)
*     double *maxvmem          - returns the maximum virtual memory used
*     double *maxrss           - returns the maximum resident memory used
*
*  NOTES
*     MT-NOTE: build_reserved_usage() is MT safe 
*******************************************************************************/
void build_reserved_usage(const u_long64 now, const lListElem *ja_task, const lListElem *pe_task,
                          double *wallclock, double *cpu, double *mem, double *maxvmem, double *maxrss)
{
   u_long64 start_time;

   if (ja_task == nullptr || wallclock == nullptr || cpu == nullptr || mem == nullptr || maxvmem == nullptr) {
      return;
   }

   /* calculate wallclock time */ 
   if (pe_task == nullptr) {
      start_time = lGetUlong64(ja_task, JAT_start_time);
   } else {
      start_time = lGetUlong64(pe_task, PET_start_time);
   }
   if (start_time > 0 && start_time < now) {
      *wallclock = sge_gmt64_to_gmt32_double(now - start_time); // reported wallclock is in seconds as double
   } else {
      *wallclock = 0;
   }

   /* if wallclock == 0, something is wrong with start_time vs. end_time,
    * and we cannot report any usage
    */
   *cpu = 0.0;
   *mem = 0.0;
   *maxvmem = 0.0;
   *maxrss = 0.0;
   if (*wallclock != 0) {
      /* 
       * compute cpu, mem and maxvmem 
       * we must take into account that we might have multiple gdil elements
       * having different settings for h_vmem!
       * mem is the integral of h_vmem * slots * wallclock
       * this computation only works, if h_vmem is *not* INFINITY (DBL_MAX)
       *
       * we have to consider different cases:
       * - ordinary sequential job (only one slot == all gdil)
       * - loosely integrated parallel job (usage computation only for master task, but all gdil)
       * - tightly integrated parallel job: usage computation done
       *   - if we want to see the accounting summary:
       *       - for the master task (all gdil, similar to loose integration)
       *   - without accounting_summary
       *       - for the master task (1 slot)
       *       - for the individual pe tasks
       */
      if (pe_task != nullptr) {
         /*
          * A pe task occupies one slot,
          * we get the virtual memory information from the queue
          * it is running in.
          */
         const char *queue_name;
         const lListElem *gdil_ep;

         *cpu = *wallclock;
         queue_name = lGetString(lFirst(lGetList(pe_task, PET_granted_destin_identifier_list)), JG_qname);
         gdil_ep = lGetElemStr(lGetList(ja_task, JAT_granted_destin_identifier_list), JG_qname, queue_name);
         build_reserved_mem_usage(gdil_ep, 1, *wallclock, mem, maxvmem, maxrss);
      } else {
         /* compute cpu */
         int slots = 0;
         int slots_total = 0;
         const lList *gdil = lGetList(ja_task, JAT_granted_destin_identifier_list);
         const lListElem *gdil_ep;
         const lListElem *pe = lGetObject(ja_task, JAT_pe_object);

         /* sequential job, or loose integration, or tight integration with accounting_summary */
         if (pe == nullptr || !lGetBool(pe, PE_control_slaves) || lGetBool(pe, PE_accounting_summary)) {
            /* account for all gdil */
            const lListElem *master_gdil_ep = lFirst(gdil);
            for_each_ep(gdil_ep, gdil) {
               slots = lGetUlong(gdil_ep, JG_slots);
               /* respect job_is_first_task, only once (for the master task gdil) */
               // @todo CS-547 we cannot differentiate master task only and master task + 1 slave task
               if (pe != nullptr && gdil_ep == master_gdil_ep && !lGetBool(pe, PE_job_is_first_task)) {
                  slots++;
               }
               slots_total += slots;
               build_reserved_mem_usage(gdil_ep, slots, *wallclock, mem, maxvmem, maxrss);
            }
         } else {
            /* tightly integrated without accounting_summary, this is the master task only */
            gdil_ep = lFirst(gdil);
            slots_total = 1;
            build_reserved_mem_usage(gdil_ep, 1, *wallclock, mem, maxvmem, maxrss);
         }

         /* cpu is wallclock time * total number of job slots */
         *cpu = *wallclock * slots_total;
      }
   }
}

