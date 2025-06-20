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

#include <cstring>

#include "gdi/ocs_gdi_ClientBase.h"

#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "qmaster_heartbeat.h"
#include "sge_qmaster_heartbeat.h"
#include "sge_thread_main.h"
#include "shutdown.h"

#include "msg_daemons_common.h"

/****** qmaster/sge_qmaster_main/sge_start_heartbeat() *****************************
*  NAME
*     sge_start_heartbeat() -- Start qmaster heartbeat. 
*
*  SYNOPSIS
*     static void sge_start_heartbeat() 
*
*  FUNCTION
*     Add heartbeat event and register according event handler. 
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: sge_start_heartbeat() is MT safe 
*
*******************************************************************************/
void
heartbeat_initialize()
{
   te_event_t ev     = nullptr;

   DENTER(TOP_LAYER);

   te_register_event_handler(increment_heartbeat, TYPE_HEARTBEAT_EVENT);
   ev = te_new_event(sge_gmt32_to_gmt64(HEARTBEAT_INTERVAL), TYPE_HEARTBEAT_EVENT, RECURRING_EVENT,
                     0, 0, "heartbeat-event");
   te_add_event(ev);
   te_free_event(&ev);

   /* this is for testsuite shadowd test */
   if (getenv("SGE_TEST_HEARTBEAT_TIMEOUT") != nullptr) {
      u_long32 test_timeout = SGE_STRTOU_LONG32(getenv("SGE_TEST_HEARTBEAT_TIMEOUT"));
      set_inc_qmaster_heartbeat_test_mode(test_timeout);
      DPRINTF("heartbeat timeout test enabled (timeout=" sge_u32 ")\n", test_timeout);
   }

   DRETURN_VOID;
} /* sge_start_heartbeat() */

/****** qmaster/sge_qmaster_heartbeat/increment_heartbeat() *************************
*  NAME
*     increment_heartbeat() -- Event handler for heartbeat events
*
*  SYNOPSIS
*     void increment_heartbeat(te_event_t anEvent) 
*
*  FUNCTION
*     Update qmaster heartbeat file.
*
*  INPUTS
*     te_event_t anEvent - heartbeat event 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: increment_hearbeat() is NOT MT safe. This function is only
*     MT-NOTE: invoked from within the event delivery thread.
*
*     We do assume that the system clock does NOT run backwards. However, we
*     do cope with a system clock which has been put back.
*
*******************************************************************************/
void 
increment_heartbeat(te_event_t anEvent, monitoring_t *monitor)
{
   int retval = 0;
   int heartbeat = 0;
   int check_act_qmaster_file = 0;
   char act_qmaster_name[CL_MAXHOSTNAMELEN];
   char act_resolved_qmaster_name[CL_MAXHOSTNAMELEN];
   char err_str[SGE_PATH_MAX+128];
   const char *act_qmaster_file = bootstrap_get_act_qmaster_file();
   const char *qualified_hostname = component_get_qualified_hostname();

   DENTER(TOP_LAYER);

   retval = inc_qmaster_heartbeat(QMASTER_HEARTBEAT_FILE, 30, &heartbeat);

   switch(retval) {
      case 0: {
         DPRINTF("(heartbeat) - incremented (or created) heartbeat file: %s(beat=%d)\n", QMASTER_HEARTBEAT_FILE, heartbeat);
         break;
      }
      default: {
         DPRINTF("(heartbeat) - inc_qmaster_heartbeat() returned %d !!! (beat=%d)\n", retval, heartbeat);
         check_act_qmaster_file = 1;
         break;
      }
   }

   if (heartbeat % 20 == 0) {
      DPRINTF("(heartbeat) - checking act_qmaster file this time\n");
      check_act_qmaster_file = 1;
   }

   if (check_act_qmaster_file == 1) {
      strcpy(err_str,"");
      if (ocs::gdi::ClientBase::get_qm_name(act_qmaster_name, act_qmaster_file, err_str, sizeof(err_str)) == 0) {
         /* got qmaster name */
         if ( getuniquehostname(act_qmaster_name, act_resolved_qmaster_name, 0) == CL_RETVAL_OK &&
              sge_hostcmp(act_resolved_qmaster_name, qualified_hostname) != 0      ) {
            /* act_qmaster file has been changed */
            WARNING(SFNMAX, MSG_HEART_ACT_QMASTER_FILE_CHANGED);
            if (sge_qmaster_shutdown_via_signal_thread(100) != 0) {
               ERROR(SFNMAX, MSG_HEART_CANT_SIGNAL);
               /* TODO: here the ctx reference is not transported back
               **       event_handler functions should use &ctx instead
               */
               sge_shutdown(1);
            }
         } else {
            DPRINTF("(heartbeat) - act_qmaster file contains hostname " SFQ "\n", act_qmaster_name);
         }
      } else {
         WARNING(MSG_HEART_CANNOT_READ_FILE_S, err_str );
      }
   }

   DRETURN_VOID;
} /* increment_heartbeat() */

