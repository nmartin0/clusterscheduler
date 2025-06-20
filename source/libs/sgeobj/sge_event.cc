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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "comm/cl_communication.h" 

#include "cull/cull.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_common.h"

/* documentation see libs/evc/sge_event_client.c */
const char *event_text(const lListElem *event, dstring *buffer) 
{
   u_long32 type, intkey, number, intkey2;
   int n=0;
   const char *strkey, *strkey2;
   const lList *lp;
   dstring id_dstring = DSTRING_INIT;

   number = lGetUlong(event, ET_number);
   type = lGetUlong(event, ET_type);
   intkey = lGetUlong(event, ET_intkey);
   intkey2 = lGetUlong(event, ET_intkey2);
   strkey = lGetString(event, ET_strkey);
   strkey2 = lGetString(event, ET_strkey2);
   if ((lp=lGetList(event, ET_new_version))) {
      n = lGetNumberOfElem(lp);
   }

   switch (type) {

   /* -------------------- */
   case sgeE_ADMINHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "ADMINHOST", n);
      break;
   case sgeE_ADMINHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "ADMINHOST", strkey);
      break;
   case sgeE_ADMINHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "ADMINHOST", strkey);
      break;
   case sgeE_ADMINHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "ADMINHOST", strkey);
      break;

   /* -------------------- */
   case sgeE_CALENDAR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CALENDAR", n);
      break;
   case sgeE_CALENDAR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CALENDAR", strkey);
      break;
   case sgeE_CALENDAR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CALENDAR", strkey);
      break;
   case sgeE_CALENDAR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CALENDAR", strkey);
      break;

   /* -------------------- */
   case sgeE_CKPT_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI,  number, "CKPT", n);
      break;
   case sgeE_CKPT_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CKPT", strkey);
      break;
   case sgeE_CKPT_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CKPT", strkey);
      break;
   case sgeE_CKPT_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CKPT", strkey);
      break;

   /* -------------------- */
   case sgeE_CONFIG_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CONFIG", n);
      break;
   case sgeE_CONFIG_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CONFIG", strkey);
      break;
   case sgeE_CONFIG_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CONFIG", strkey);
      break;
   case sgeE_CONFIG_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CONFIG", strkey);
      break;

   /* -------------------- */
   case sgeE_EXECHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "EXECHOST", n);
      break;
   case sgeE_EXECHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "EXECHOST", strkey);
      break;
   case sgeE_EXECHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "EXECHOST", strkey);
      break;
   case sgeE_EXECHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "EXECHOST", strkey);
      break;

   /* -------------------- */
   case sgeE_JATASK_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "JATASK", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JATASK_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "JATASK", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JATASK_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "JATASK", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   /* -------------------- */
   case sgeE_PETASK_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "PETASK", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_PETASK_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "PETASK", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
#if 0      
   /* JG: we'll have it soon ;-) */
   case sgeE_PETASK_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "PETASK", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
#endif

   /* -------------------- */
   case sgeE_JOB_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "JOB", n);
      break;
   case sgeE_JOB_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "JOB", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "JOB", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "JOB", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_USAGE:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXUSAGE_US, 
         number, job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_FINAL_USAGE:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXFINALUSAGE_US, 
         number, job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   case sgeE_JOB_FINISH:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXFINISH_US, 
         number, job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   /* -------------------- */
   case sgeE_JOB_SCHEDD_INFO_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "JOB_SCHEDD_INFO", n);
      break;
   case sgeE_JOB_SCHEDD_INFO_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "JOB_SCHEDD_INFO", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_SCHEDD_INFO_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "JOB_SCHEDD_INFO", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_SCHEDD_INFO_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "JOB_SCHEDD_INFO", job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   /* -------------------- */
   case sgeE_MANAGER_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "MANAGER", n);
      break;
   case sgeE_MANAGER_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "MANAGER", strkey);
      break;
   case sgeE_MANAGER_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "MANAGER", strkey);
      break;
   case sgeE_MANAGER_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "MANAGER", strkey);
      break;

   /* -------------------- */
   case sgeE_OPERATOR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "OPERATOR", n);
      break;
   case sgeE_OPERATOR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "OPERATOR", strkey);
      break;
   case sgeE_OPERATOR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "OPERATOR", strkey);
      break;
   case sgeE_OPERATOR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "OPERATOR", strkey);
      break;

   /* -------------------- */
   case sgeE_NEW_SHARETREE:
      sge_dstring_sprintf(buffer, MSG_EVENT_SHARETREEXNODESYLEAFS_UII, number,
         lGetNumberOfNodes(nullptr, lp, STN_children),
         lGetNumberOfLeafs(nullptr, lp, STN_children));
      break;

   /* -------------------- */
   case sgeE_PE_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "PE", n);
      break;
   case sgeE_PE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "PE", strkey);
      break;
   case sgeE_PE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "PE", strkey);
      break;
   case sgeE_PE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "PE", strkey);
      break;

   /* -------------------- */
   case sgeE_PROJECT_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "PROJECT", n);
      break;
   case sgeE_PROJECT_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "PROJECT", strkey);
      break;
   case sgeE_PROJECT_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "PROJECT", strkey);
      break;
   case sgeE_PROJECT_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "PROJECT", strkey);
      break;

   /* -------------------- */
   case sgeE_QMASTER_GOES_DOWN:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "QMASTER GOES DOWN");
      break;

   /* -------------------- */
   case sgeE_ACK_TIMEOUT:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "ACK TIMEOUT");
      break;

   /* -------------------- */
   case sgeE_CQUEUE_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CLUSTER QUEUE", n);
      break;
   case sgeE_CQUEUE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CLUSTER QUEUE", strkey);
      break;
   case sgeE_CQUEUE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CLUSTER QUEUE", strkey);
      break;
   case sgeE_CQUEUE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CLUSTER QUEUE", strkey);
      break;

   /* -------------------- */
   case sgeE_QINSTANCE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDQINSTANCE_USS, number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELQINSTANCE_USS, number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODQINSTANCE_USS, number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_USOS:
      sge_dstring_sprintf(buffer, MSG_EVENT_UNSUSPENDQUEUEXONSUBORDINATE_US, number, strkey);
      break;
   case sgeE_QINSTANCE_SOS:
      sge_dstring_sprintf(buffer, MSG_EVENT_SUSPENDQUEUEXONSUBORDINATE_US, number, strkey);
      break;

   /* -------------------- */
   case sgeE_SCHED_CONF:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "SCHEDULER CONFIG");
      break;

   /* -------------------- */
   case sgeE_SCHEDDMONITOR:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "TRIGGER SCHEDULER MONITORING");
      break;

   /* -------------------- */
   case sgeE_SHUTDOWN:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "SHUTDOWN");
      break;

   /* -------------------- */
   case sgeE_SUBMITHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "SUBMITHOST", n);
      break;
   case sgeE_SUBMITHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "SUBMITHOST", strkey);
      break;
   case sgeE_SUBMITHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "SUBMITHOST", strkey);
      break;
   case sgeE_SUBMITHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "SUBMITHOST", strkey);
      break;

   /* -------------------- */
   case sgeE_USER_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "USER", n);
      break;
   case sgeE_USER_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "USER", strkey);
      break;
   case sgeE_USER_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "USER", strkey);
      break;
   case sgeE_USER_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "USER", strkey);
      break;

   /* -------------------- */
   case sgeE_USERSET_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "USER SET", n);
      break;
   case sgeE_USERSET_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "USER SET", strkey);
      break;
   case sgeE_USERSET_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "USER SET", strkey);
      break;
   case sgeE_USERSET_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "USER SET", strkey);
      break;

   /* -------------------- */
   case sgeE_HGROUP_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "HOST GROUP", n);
      break;
   case sgeE_HGROUP_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "HOST GROUP", strkey);
      break;
   case sgeE_HGROUP_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "HOST GROUP", strkey);
      break;
   case sgeE_HGROUP_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "HOST GROUP", strkey);
      break;

   /* -------------------- */
   case sgeE_CENTRY_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "COMPLEX ENTRY", n);
      break;
   case sgeE_CENTRY_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "COMPLEX ENTRY", strkey);
      break;
   case sgeE_CENTRY_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "COMPLEX ENTRY", strkey);
      break;
   case sgeE_CENTRY_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "COMPLEX ENTRY", strkey);
      break;

   /* -------------------- */
   case sgeE_RQS_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "RESOURCE QUOTA", n);
      break;
   case sgeE_RQS_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "RESOURCE QUOTA", strkey);
      break;
   case sgeE_RQS_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "RESOURCE QUOTA", strkey);
      break;
   case sgeE_RQS_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "RESOURCE QUOTA", strkey);
      break;

   /* -------------------- */
   case sgeE_AR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "ADVANCE RESERVATION", n);
      break;
   case sgeE_AR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USU, number, "ADVANCE RESERVATION", intkey);
      break;
   case sgeE_AR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USU, number, "ADVANCE RESERVATION", intkey);
      break;
   case sgeE_AR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USU, number, "ADVANCE RESERVATION", intkey);
      break;

   /* -------------------- */
   case sgeE_CATEGORY_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CATEGORY", n);
      break;
   case sgeE_CATEGORY_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USU, number, "CATEGORY", intkey);
      break;
   case sgeE_CATEGORY_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USU, number, "CATEGORY", intkey);
      break;
   case sgeE_CATEGORY_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USU, number, "CATEGORY", intkey);
      break;

   /* -------------------- */
   default:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "????????");
      break;
   }

   u_long64 unique_id = lGetUlong64(event, ET_unique_id);
   sge_dstring_sprintf_append(buffer, " (# " sge_u64 ")", unique_id);

   sge_dstring_free(&id_dstring);

   return sge_dstring_get_string(buffer);
}

static bool event_client_verify_subscription(const lListElem *event_client, lList **answer_list, int d_time)
{
   bool ret = true;
   const lListElem *ep;

   DENTER(TOP_LAYER);

   for_each_ep(ep, lGetList(event_client, EV_subscribed)) {
      u_long32 id = lGetUlong(ep, EVS_id);
      if (id <= sgeE_ALL_EVENTS || id >= sgeE_EVENTSIZE) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDEVENT);
         ret = false;
         break;
      }
   }

   /* TODO: verify the where and what elements */

   DRETURN(ret);
}

/****** sge_event/event_client_verify() ****************************************
*  NAME
*     event_client_verify() -- verify an event client object
*
*  SYNOPSIS
*     bool 
*     event_client_verify(const lListElem *event_client, lList **answer_list) 
*
*  FUNCTION
*     Verifies, if an incoming event client object (new event client registration
*     through a GDI_ADD operation or event client modification through GDI_MOD
*     operation is correct.
*
*     We do the following verifications:
*        - EV_id correct:
*           - add request usually may only request dynamic event client id, 
*             if a special id is requested, we must be on local host and be
*             admin user or root.
*        - EV_name (valid string, limited length)
*        - EV_d_time (valid delivery interval)
*        - EV_flush_delay (valid flush delay)
*        - EV_subscribed (valid subscription list)
*        - EV_busy_handling (valid busy handling)
*        - EV_session (valid string, limited length)
*
*     No verification will be done
*        - EV_host (is always overwritten by qmaster code)
*        - EV_commproc (comes from commlib)
*        - EV_commid (comes from commlib)
*        - EV_uid (is always overwritten  by qmaster code)
*        - EV_last_heard_from (only used by qmaster)
*        - EV_last_send_time (only used by qmaster)
*        - EV_next_send_time (only used by qmaster)
*        - EV_sub_array (only used by qmaster)
*        - EV_changed (?)
*        - EV_next_number (?)
*        - EV_state (?)
*
*  INPUTS
*     const lListElem *event_client - ??? 
*     lList **answer_list           - ??? 
*     bool add                      - is this an add request (or mod)?
*
*  RESULT
*     bool - 
*
*  NOTES
*     MT-NOTE: event_client_verify() is MT safe 
*******************************************************************************/
bool 
event_client_verify(const lListElem *event_client, lList **answer_list, bool add)
{
   bool ret = true;
   const char *str;
   u_long d_time = 0;
  
   DENTER(TOP_LAYER);

   if (event_client == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      DTRACE;
      ret = false;
   }

#if 1
   if (ret) {
      if (!object_verify_cull(event_client, EV_Type)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_OBJECT_STRUCTURE_ERROR);
         DTRACE;
         ret = false;
      }
   }
#endif   

   if (ret) {
      /* get the event delivery time - we'll need it in later checks */
      d_time = lGetUlong(event_client, EV_d_time);

      /* 
       * EV_name has to be a valid string.
       * TODO: Is verify_str_key too restrictive? Does drmaa allow to set the name?
       */
      str = lGetString(event_client, EV_name);
      if (str == nullptr ||
         verify_str_key(
            answer_list, str, MAX_VERIFY_STRING,
            lNm2Str(EV_name), KEY_TABLE) != STATUS_OK) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDNAME);
         DTRACE;
         ret = false;
         DPRINTF("EV name false\n");
      }
   }

   /* 
    * Verify the EV_id:
    * Add requests may only contain EV_ID_ANY or a special id.
    * If a special id is requested, it must come from admin/root
    */
   if (ret) {
#if 1      
      /* TODO: jgdi does not work with the check fix needed */
      u_long32 id = lGetUlong(event_client, EV_id);
      if (add && id >= EV_ID_FIRST_DYNAMIC) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDID);
         DTRACE;
         ret = false;
         DPRINTF("EV_id false: " sge_u32 "\n", id);
      }
#endif
   }

   /* Event delivery time may not be gt commlib timeout */
   if (ret) {
      if (d_time < 1 || d_time > CL_DEFINE_CLIENT_CONNECTION_LIFETIME-5) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDDTIME_II, d_time,
                                 CL_DEFINE_CLIENT_CONNECTION_LIFETIME-5);
         ret = false;
         DPRINTF("d_time false\n");
      }
   }

   /* 
    * flush delay cannot be gt event delivery time 
    * We can very easily run into this problem by configuring scheduler interval
    * and flush_submit|finish_secs. Disabling the check.
    */
   if (ret) {
      if (lGetUlong(event_client, EV_flush_delay) > d_time) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_FLUSHDELAYCANNOTBEGTDTIME);
         ret = false;
      }
   }

   /* subscription */
   if (ret) {
      ret = event_client_verify_subscription(event_client, answer_list, (int)d_time);
   }

   /* busy handling comes from an enum with defined set of values */
   if (ret) {
      u_long32 busy = lGetUlong(event_client, EV_busy_handling);
      if (busy != EV_BUSY_NO_HANDLING && busy != EV_BUSY_UNTIL_ACK &&
          busy != EV_BUSY_UNTIL_RELEASED) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDBUSYHANDLING);
         ret = false;
      }
   }

   /* verify session key. TODO: verify_str_key too restrictive? */
   if (ret) {
      const char *session = lGetString(event_client, EV_session);
      if (session != nullptr) {
         if (verify_str_key(
         answer_list, session, MAX_VERIFY_STRING,
         "session key", KEY_TABLE) != STATUS_OK) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_EVENT_INVALIDSESSIONKEY);
            ret = false;
         }
      }
   }

   /* disallow an EV_update_function from event_client request */
   if (ret) {
      if (lGetRef(event_client, EV_update_function) != nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_EVENT_INVALIDUPDATEFUNCTION);
         ret = false;
      }
   }

   DRETURN(ret);
}
