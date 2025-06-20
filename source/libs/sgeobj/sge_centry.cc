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
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <cfloat>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull_list.h"

#include "comm/commd_message_flags.h"

#include "sched/msg_schedd.h"

#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "basis_types.h"
#include "msg_common.h"
#include "uti/sge.h"

#include "sge_centry_rsmap.h"

#define CENTRY_LAYER BASIS_LAYER

/* EB: ADOC: add commets */

const int max_host_resources = 29;/* specifies the number of elements in the host_resource array */
const struct queue2cmplx host_resource[] = {
        {"arch",             0, 0, 0, TYPE_STR},
        {"cpu",              0, 0, 0, TYPE_DOUBLE},
        {"load_avg",         0, 0, 0, TYPE_DOUBLE},
        {"load_long",        0, 0, 0, TYPE_DOUBLE},
        {"load_medium",      0, 0, 0, TYPE_DOUBLE},
        {"load_short",       0, 0, 0, TYPE_DOUBLE},
        {"mem_free",         0, 0, 0, TYPE_MEM},
        {"mem_total",        0, 0, 0, TYPE_MEM},
        {"mem_used",         0, 0, 0, TYPE_MEM},
        {"min_cpu_inter",    0, 0, 0, TYPE_TIM},
        {"np_load_avg",      0, 0, 0, TYPE_DOUBLE},
        {"np_load_long",     0, 0, 0, TYPE_DOUBLE},
        {"np_load_medium",   0, 0, 0, TYPE_DOUBLE},
        {"np_load_short",    0, 0, 0, TYPE_DOUBLE},
        {"num_proc",         0, 0, 0, TYPE_INT},
        {"swap_free",        0, 0, 0, TYPE_MEM},
        {"swap_rate",        0, 0, 0, TYPE_MEM},
        {"swap_rsvd",        0, 0, 0, TYPE_MEM},
        {"swap_total",       0, 0, 0, TYPE_MEM},
        {"swap_used",        0, 0, 0, TYPE_MEM},
        {"virtual_free",     0, 0, 0, TYPE_MEM},
        {"virtual_total",    0, 0, 0, TYPE_MEM},
        {"virtual_used",     0, 0, 0, TYPE_MEM},
        {"display_win_gui",  0, 0, 0, TYPE_BOO},
        {"m_core",           0, 0, 0, TYPE_INT},
        {"m_socket",         0, 0, 0, TYPE_INT},
        {"m_thread",         0, 0, 0, TYPE_INT},
        {"m_topology",       0, 0, 0, TYPE_STR},
        {"m_topology_inuse", 0, 0, 0, TYPE_STR}
};

const int max_queue_resources = 24; /* specifies the number of elements in the queue_resource array */
const struct queue2cmplx queue_resource[] = {
        {"qname",            QU_qname,            0,                   0,            TYPE_STR},
        {"hostname",         QU_qhostname,        0,                   0,            TYPE_HOST},
        {"slots",            QU_job_slots,        0,                   0,            TYPE_INT},
        {"tmpdir",           QU_tmpdir,           0,                   0,            TYPE_STR},
        {"seq_no",           QU_seq_no,           0,                   0,            TYPE_INT},
        {"rerun",            QU_rerun,            0,                   0,            TYPE_BOO},
        {"calendar",         QU_calendar,         CQ_calendar,         ASTR_value,   TYPE_STR}, /* value is SGE_STRING */
        {"s_rt",             QU_s_rt,             CQ_s_rt,             ATIME_value,  TYPE_TIM}, /* value is SGE_STRING */
        {"h_rt",             QU_h_rt,             CQ_h_rt,             ATIME_value,  TYPE_TIM}, /* value is SGE_STRING */
        {"s_cpu",            QU_s_cpu,            CQ_s_cpu,            ATIME_value,  TYPE_TIM}, /* value is SGE_STRING */
        {"h_cpu",            QU_h_cpu,            CQ_h_cpu,            ATIME_value,  TYPE_TIM}, /* value is SGE_STRING */
        {"s_fsize",          QU_s_fsize,          CQ_s_data,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"h_fsize",          QU_h_fsize,          CQ_h_fsize,          AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"s_data",           QU_s_data,           CQ_s_data,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"h_data",           QU_h_data,           CQ_h_data,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"s_stack",          QU_s_stack,          CQ_s_stack,          AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"h_stack",          QU_h_stack,          CQ_h_stack,          AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"s_core",           QU_s_core,           CQ_s_core,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"h_core",           QU_h_core,           CQ_h_core,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"s_rss",            QU_s_rss,            CQ_s_rss,            AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"h_rss",            QU_h_rss,            CQ_h_rss,            AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"s_vmem",           QU_s_vmem,           CQ_s_vmem,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"h_vmem",           QU_h_vmem,           CQ_h_vmem,           AMEM_value,   TYPE_MEM}, /* value is SGE_STRING */
        {"min_cpu_interval", QU_min_cpu_interval, CQ_min_cpu_interval, AINTER_value, TYPE_TIM}  /* value is SGE_STRING */
};

int get_rsrc(const char *name, bool queue, int *field, int *cqfld, int *valfld, int *type) {
   int pos = 0;
   const struct queue2cmplx *rlist;
   int nitems;

   if (queue) {
      rlist = &queue_resource[0];
      nitems = max_queue_resources;
   } else {
      rlist = &host_resource[0];
      nitems = max_host_resources;
   }

   for (; pos < nitems; pos++) {
      if (strcmp(name, rlist[pos].name) == 0) {
         if (field) *field = rlist[pos].field;
         if (cqfld) *cqfld = rlist[pos].cqfld;
         if (valfld) *valfld = rlist[pos].valfld;
         if (type) *type = rlist[pos].type;
         return 0;
      }
   }

   return -1;
}

/****** sgeobj/centry/centry_fill_and_check() *********************************
*  NAME
*     centry_fill_and_check() -- fill and check the attribute
*
*  SYNOPSIS
*     int centry_fill_and_check(lListElem *cep,
*                               bool allow_empty_boolean,
*                               bool allow_neg_consumable)
*
*  FUNCTION
*     fill and check the attribute
*
*  INPUTS
*     lListElem *cep           - CE_Type, this object will be checked
*     lList** answer_list      - answer list
*     int allow_empty_boolean  - boolean
*        true  - nullptr values of boolean attributes will
*                be replaced with "true"
*        false - nullptr values will be handled as error
*     int allow_neg_consumable - boolean
*        true  - negative values for consumable
*                resources are allowed.
*        false - function will return with -1 if it finds
*                consumable resources with a negative value
*
*  RESULT
*        0 on success
*       -1 on error
*        an error message will be written into SGE_EVENT
******************************************************************************/
int
centry_fill_and_check(lListElem *this_elem, lList **answer_list, bool allow_empty_boolean,
                      bool allow_neg_consumable) {
   static char tmp[1000];
   const char *name, *s;
   u_long32 type;
   double dval;
   int ret, allow_infinity;

   DENTER(CENTRY_LAYER);

   name = lGetString(this_elem, CE_name);
   s = lGetString(this_elem, CE_stringval);
   DPRINTF("   ===> centry_fill_and_check(%s, %s)\n", name, s);
   /* allow infinity for non-consumables only */
   allow_infinity = (lGetUlong(this_elem, CE_consumable) != CONSUMABLE_NO) ? 0 : 1;

   if (s == nullptr) {
      if (allow_empty_boolean && lGetUlong(this_elem, CE_valtype) == TYPE_BOO) {
         lSetString(this_elem, CE_stringval, "TRUE");
         s = lGetString(this_elem, CE_stringval);
      } else {
/*          ERROR(MSG_CPLX_VALUEMISSING_S, name); */
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CPLX_VALUEMISSING_S, name);
         DRETURN(-1);
      }
   }

   switch (type = lGetUlong(this_elem, CE_valtype)) {
      case TYPE_RSMAP:
      case TYPE_INT:
      case TYPE_TIM:
      case TYPE_MEM:
      case TYPE_BOO:
      case TYPE_DOUBLE:
         if (!extended_parse_ulong_val(&dval, nullptr, type, s, tmp, sizeof(tmp)-1, allow_infinity, false)) {
/*             ERROR(MSG_CPLX_WRONGTYPE_SSS, name, s, tmp); */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ATTRIB_XISNOTAY_SS, name, tmp);
            DRETURN(-1);
         }
         DPRINTF("   ===> centry_fill_and_check(%s, %s), dval = %f\n", name, s, dval);
         lSetDouble(this_elem, CE_doubleval, dval);

         /* normalize time values, so that the string value is based on seconds */
         if (type == TYPE_TIM && dval != DBL_MAX) {
            char str_value[100];
            dstring ds;
            sge_dstring_init(&ds, str_value, sizeof(str_value));
            sge_dstring_sprintf(&ds, "%.0f", dval);
            DPRINTF("normalized time value from \"%s\" to \"%s\"\n",
                    lGetString(this_elem, CE_stringval), str_value);
            lSetString(this_elem, CE_stringval, str_value);
         }

         /* also the CE_defaultval must be parsable for numeric types */
         if ((s=lGetString(this_elem, CE_defaultval))
            && !parse_ulong_val(&dval, nullptr, type, s, tmp, sizeof(tmp)-1)) {
/*             ERROR(MSG_CPLX_WRONGTYPE_SSS, name, s, tmp); */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CPLX_WRONGTYPE_SSS, name, s, tmp);
            DRETURN(-1);
         }

         /* negative values are not allowed for consumable attributes */
         if (!allow_neg_consumable && (lGetUlong(this_elem, CE_consumable) != CONSUMABLE_NO)
             && lGetDouble(this_elem, CE_doubleval) < (double)0.0) {
/*             ERROR(MSG_CPLX_ATTRIBISNEG_S, name); */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CPLX_ATTRIBISNEG_S, name);

            DRETURN(-1);
         }
         break;
      case TYPE_HOST:
         /* resolve hostname and store it */
         ret = sge_resolve_host(this_elem, CE_stringval);
         if (ret != CL_RETVAL_OK) {
            if (ret == CL_RETVAL_GETHOSTNAME_ERROR) {
/*                ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, s); */
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_CANTRESOLVEHOST_S, s);
            } else {
/*                ERROR(MSG_SGETEXT_INVALIDHOST_S, s); */
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_INVALIDHOST_S, s);
            }
            DRETURN(-1);
         }
         break;
      case TYPE_STR:
      case TYPE_CSTR:
      case TYPE_RESTR:
         /* no restrictions - so everything is ok */
         break;

      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
         DRETURN(-1);
   }

   DRETURN(0);
}

const char *
map_op2str(u_long32 op)
{
   static const char *opv[] = {
      "??",
      "==",  /* CMPLXEQ_OP */
      ">=",  /* CMPLXGE_OP */
      ">",   /* CMPLXGT_OP */
      "<",   /* CMPLXLT_OP */
      "<=",  /* CMPLXLE_OP */
      "!=",  /* CMPLXNE_OP */
      "EXCL" /* CMPLXEXCL_OP */
   };

   if (op < CMPLXEQ_OP || op > CMPLXEXCL_OP) {
      op = 0;
   }
   return opv[op];
}

const char *
map_req2str(u_long32 op)
{
   static const char *opv[] = {
      "??",
      "NO",       /* REQU_NO */
      "YES",      /* REQU_YES */
      "FORCED",   /* REQU_FORCED */
   };

   if (op < REQU_NO || op > REQU_FORCED) {
      op = 0;
   }
   return opv[op];
}

/****** sge_centry/map_consumable2str() ****************************************
*  NAME
*     map_consumable2str() -- map to consumable string
*
*  SYNOPSIS
*     const char * map_consumable2str(u_long32 op) 
*
*  FUNCTION
*     maps int representation of CONSUMABLE to string
*
*  INPUTS
*     u_long32 op - CONSUMABLE_*
*
*  RESULT
*     const char * - string representation of consumable definition
*
*  NOTES
*     MT-NOTE: map_consumable2str() is not safe 
*******************************************************************************/
const char *map_consumable2str(u_long32 op) {
   static const char *opv[] = {
      "NO",       /* CONSUMABLE_NO */
      "YES",      /* CONSUMABLE_YES */
      "JOB",      /* CONSUMABLE_JOB */
      "HOST",     /* CONSUMABLE_HOST */
   };

   if (op > CONSUMABLE_HOST) {
      op = CONSUMABLE_NO;
   }
   return opv[op];
}

const char *
map_type2str(u_long32 type)
{
   static const char *typev[] = {
      "??????",
      "INT",      /*  1 TYPE_INT */
      "STRING",   /*  2 TYPE_STR */
      "TIME",     /*  3 TYPE_TIM */
      "MEMORY",   /*  4 TYPE_MEM */
      "BOOL",     /*  5 TYPE_BOO */
      "CSTRING",  /*  6 TYPE_CSTR */
      "HOST",     /*  7 TYPE_HOST */
      "DOUBLE",   /*  8 TYPE_DOUBLE */
      "RESTRING", /*  9 TYPE_RESTR */
      "RSMAP",    /* 10 TYPE_RSMAP */

      "TYPE_ACC", /* 11 TYPE_ACC */
      "TYPE_LOG"  /* 12 TYPE_LOG */
   };

   if (type < TYPE_FIRST || type > TYPE_LAST) {
      type = 0;
   }
   return typev[type];
}

/****** sgeobj/centry/centry_create() *****************************************
*  NAME
*     centry_create() -- Create a preinitialized centry element 
*
*  SYNOPSIS
*     lListElem *
*     centry_create(lList **answer_list, const char *name)
*
*  FUNCTION
*     Create a preinitialized centry element with the given "name".
*
*  INPUTS
*     lList **answer_list  - AN_Type 
*     const char *name     - full name 
*
*  RESULT
*     lListElem * - CE_Type element
*******************************************************************************/
lListElem *
centry_create(lList **answer_list, const char *name) {
   lListElem *ret = nullptr;  /* CE_Type */

   DENTER(CENTRY_LAYER);
   if (name != nullptr) {
      ret = lCreateElem(CE_Type);
      if (ret != nullptr) {
         lSetString(ret, CE_name, name);
         lSetString(ret, CE_shortcut, name);
         lSetUlong(ret, CE_valtype, TYPE_INT);
         lSetUlong(ret, CE_relop, CMPLXLE_OP);
         lSetUlong(ret, CE_requestable, REQU_NO);
         lSetUlong(ret, CE_consumable, CONSUMABLE_NO);
         lSetString(ret, CE_defaultval, "0");
         lSetString(ret, CE_urgency_weight, "0");
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EMALLOC,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                              MSG_INAVLID_PARAMETER_IN_S, __func__);
   }
   DRETURN(ret);
}

/****** sgeobj/centry/centry_is_referenced() **********************************
*  NAME
*     centry_is_referenced() -- Is centry element referenced?
*
*  SYNOPSIS
*     bool 
*     centry_is_referenced(const lListElem *centry, 
*                          lList **answer_list, 
*                          const lList *master_cqueue_list, 
*                          const lList *master_exechost_list, 
*                          const lList *master_sconf_list) 
*
*  FUNCTION
*     Is the centry element referenced in a sublist of
*     "master_queue_list", "master_exechost_list" or 
*     "master_sconf_list". 
*
*  INPUTS
*     const lListElem *centry           - CE_Type 
*     lList **answer_list               - AN_Type 
*     const lList *master_cqueue_list   - CQ_Type 
*     const lList *master_exechost_list - EH_Type 
*
*  RESULT
*     bool - true or false
*******************************************************************************/
bool
centry_is_referenced(const lListElem *centry, lList **answer_list,
                     const lList *master_cqueue_list,
                     const lList *master_exechost_list,
                     const lList *master_rqs_list) {
   bool ret = false;
   const char *centry_name = lGetString(centry, CE_name);

   DENTER(CENTRY_LAYER);

   if (sconf_is_centry_referenced(centry)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_INFO,
                              MSG_CENTRYREFINSCONF_S, centry_name);
      ret = true;
   }
   if (!ret) {
      const lListElem *cqueue = nullptr, *cel = nullptr;

      /* fix for bug 6422335
       * check the cq configuration for centry references instead of qinstances
       */
      for_each_ep(cqueue, master_cqueue_list) {
         for_each_ep(cel, lGetList(cqueue, CQ_consumable_config_list)) {
            if (lGetSubStr(cel, CE_name, centry_name, ACELIST_value)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_INFO,
                                       MSG_CENTRYREFINQUEUE_SS,
                                       centry_name,
                                       lGetString(cqueue, CQ_name));
               ret = true;
               break;
            }
         }
         if (ret) {
            break;
         }
      }
   }
   if (!ret) {
      const lListElem *host = nullptr;    /* EH_Type */

      for_each_ep(host, master_exechost_list) {
         if (host_is_centry_referenced(host, centry)) {
            const char *host_name = lGetHost(host, EH_name);

            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_INFO,
                                    MSG_CENTRYREFINHOST_SS,
                                    centry_name, host_name);
            ret = true;
            break;
         }
      }
   }
   if (!ret) {
      const lListElem *rqs = nullptr;
      for_each_ep(rqs, master_rqs_list) {
         if (sge_centry_referenced_in_rqs(rqs, centry)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_INFO,
                                    MSG_CENTRYREFINRQS_SS,
                                    centry_name, lGetString(rqs, RQS_name));
            ret = true;
            break;
         }
      }
   }

   DRETURN(ret);
}

/****** sgeobj/centry/centry_print_resource_to_dstring() **********************
*  NAME
*     centry_print_resource_to_dstring() -- Print to dstring 
*
*  SYNOPSIS
*     bool 
*     centry_print_resource_to_dstring(const lListElem *this_elem, 
*                                      dstring *string) 
*
*  FUNCTION
*     Print resource string (memory, time) to dstring.
*
*  INPUTS
*     const lListElem *this_elem - CE_Type 
*     dstring *string            - dynamic string 
*
*  RESULT
*     bool - error state 
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: centry_print_resource_to_dstring() is MT safe
*******************************************************************************/
bool
centry_print_resource_to_dstring(const lListElem *this_elem, dstring *string) {
   bool ret = true;

   DENTER(CENTRY_LAYER);
   if (this_elem != nullptr && string != nullptr) {
      u_long32 type = lGetUlong(this_elem, CE_valtype);
      double val = lGetDouble(this_elem, CE_doubleval);

      switch (type) {
         case TYPE_TIM:
            double_print_time_to_dstring(val, string);
            break;
         case TYPE_MEM:
            double_print_memory_to_dstring(val, string);
            break;
         default:
            double_print_to_dstring(val, string);
            break;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/centry/centry_list_locate() ************************************
*  NAME
*     centry_list_locate() -- Find Centry element 
*
*  SYNOPSIS
*     lListElem *centry_list_locate(const lList *this_list, const char *name) 
*
*  FUNCTION
*     Find CEntry element with "name" in "this_list". 
*
*  INPUTS
*     const lList *this_list - CE_Type list 
*     const char *name       - name of an CE_Type entry 
*
*  RESULT
*     lListElem * - CE_Type element 
*******************************************************************************/
lListElem *
centry_list_locate(const lList *this_list, const char *name) {
   lListElem *ret = nullptr;   /* CE_Type */

   DENTER(CENTRY_LAYER);
   if (this_list != nullptr && name != nullptr) {
      ret = lGetElemStrRW(this_list, CE_name, name);
      if (ret == nullptr) {
         ret = lGetElemStrRW(this_list, CE_shortcut, name);
      }
   }
   DRETURN(ret);
}

/****** sgeobj/centry/centry_list_sort() **************************************
*  NAME
*     centry_list_sort() -- Sort a CE_Type list 
*
*  SYNOPSIS
*     bool centry_list_sort(lList *this_list) 
*
*  FUNCTION
*     Sort a CE_Type list 
*
*  INPUTS
*     lList *this_list - CE_Type list 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*******************************************************************************/
bool
centry_list_sort(lList *this_list) {
   bool ret = true;

   DENTER(CENTRY_LAYER);
   if (this_list != nullptr) {
      lSortOrder *order = nullptr;

      order = lParseSortOrderVarArg(lGetListDescr(this_list), "%I+", CE_name);
      lSortList(this_list, order);
      lFreeSortOrder(&order);
   }
   DRETURN(ret);
}

/****** sgeobj/centry/centry_list_init_double() *******************************
*  NAME
*     centry_list_init_double() -- Initialize double from string 
*
*  SYNOPSIS
*     bool centry_list_init_double(lList *this_list) 
*
*  FUNCTION
*     Initialize all double values contained in "this_list" 
*
*  INPUTS
*     lList *this_list - CE_Type list 
*
*  RESULT
*     bool - true
*******************************************************************************/
bool
centry_list_init_double(lList *this_list) {
   bool ret = true;

   DENTER(CENTRY_LAYER);
   if (this_list != nullptr) {
      lListElem *centry;

      for_each_rw (centry, this_list) {
         double new_val = 0.0; /* 
                                * parse_ulong_val will not set it for all 
                                * data types! 
                                */
         parse_ulong_val(&new_val, nullptr, lGetUlong(centry, CE_valtype),
                         lGetString(centry, CE_stringval), nullptr, 0);
         lSetDouble(centry, CE_doubleval, new_val);
      }
   }
   DRETURN(ret);
}

/****** sgeobj/centry/centry_list_fill_request() ******************************
*  NAME
*     centry_list_fill_request() -- fills and checks list of complex entries
*
*  SYNOPSIS
*     int centry_list_fill_request(lList *centry_list,
*                                  lList *master_centry_list,
*                                  bool allow_non_requestable,
*                                  bool allow_empty_boolean,
*                                  bool allow_neg_consumable)
*
*  FUNCTION
*     This function fills a given list of complex entries with missing
*     attributes which can be found in the complex. It checks also
*     wether the given in the centry_list-List are valid.
*
*  INPUTS
*     lList *this_list           - resources as complex list CE_Type
*     lList **answer_list        - answer list
*     lList *master_centry_list  - the global complex list
*     bool allow_non_requestable - needed for qstat -l or qmon customize
*                                 dialog
*     int allow_empty_boolean    - boolean
*        true  - nullptr values of boolean attributes will
*                be replaced with "true"
*        false - nullptr values will be handled as error
*     int allow_neg_consumable  - boolean
*        true  - negative values for consumable
*                resources are allowed.
*        false - function will return with -1 if it finds
*                consumable resources with a negative value
*
*  RESULT
*     int - error
*        0 on success
*       -1 on error
*        an error message will be written into SGE_EVENT
*******************************************************************************/
int
centry_list_fill_request(lList *this_list, lList **answer_list, const lList *master_centry_list,
                         bool allow_non_requestable, bool allow_empty_boolean,
                         bool allow_neg_consumable) {
   lListElem *entry = nullptr;
   lListElem *cep = nullptr;

   DENTER(CENTRY_LAYER);

   for_each_rw(entry, this_list) {
      const char *name = lGetString(entry, CE_name);
      u_long32 requestable;

      cep = centry_list_locate(master_centry_list, name);
      if (cep != nullptr) {
         requestable = lGetUlong(cep, CE_requestable);
         if (!allow_non_requestable && requestable == REQU_NO) {
/*             ERROR(MSG_SGETEXT_RESOURCE_NOT_REQUESTABLE_S, name); */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_RESOURCE_NOT_REQUESTABLE_S, name);
            DRETURN(-1);
         }

         /* replace name in request/threshold/consumable list,
            it may have been a shortcut */
         lSetString(entry, CE_name, lGetString(cep, CE_name));

         /* we found the right complex attrib */
         /* so we know the type of the requested data */
         lSetUlong(entry, CE_valtype, lGetUlong(cep, CE_valtype));

         /* we also know whether it is a consumable attribute */
         lSetUlong(entry, CE_consumable, lGetUlong(cep, CE_consumable));

         if (centry_fill_and_check(entry, answer_list, allow_empty_boolean, allow_neg_consumable)) {
            /* no error msg here - centry_fill_and_check() makes it */
            DRETURN(-1);
         }
      } else {
         /* CLEANUP: message should be put into answer_list and
            returned via argument. */
/*          ERROR(MSG_SGETEXT_UNKNOWN_RESOURCE_S, name); */
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_UNKNOWN_RESOURCE_S, name);
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

/**
 * @brief fill in configuration attributes into a list of complex attributes
 *
 * The function makes sure that all configuration attributes of objects in a CE_Type list
 * are properly filled in.
 * The actual configuration values are taken from the complex definition (the master_centry_list).
 *
 * @param centry_list
 * @param master_centry_list
 */
void
centry_list_fill_config(lList *centry_list, const lList *master_centry_list) {
   lListElem *centry;
   for_each_rw(centry, centry_list) {
      const lListElem *master_centry = centry_list_locate(master_centry_list, lGetString(centry, CE_name));
      if (master_centry != nullptr) {
         lSetString(centry, CE_name, lGetString(master_centry, CE_name));
         lSetString(centry, CE_shortcut, lGetString(master_centry, CE_shortcut));
         lSetUlong(centry, CE_valtype, lGetUlong(master_centry, CE_valtype));
         lSetUlong(centry, CE_relop, lGetUlong(master_centry, CE_relop));
         lSetUlong(centry, CE_requestable, lGetUlong(master_centry, CE_requestable));
         lSetUlong(centry, CE_consumable, lGetUlong(master_centry, CE_consumable));
         lSetString(centry, CE_defaultval, lGetString(master_centry, CE_defaultval));
         lSetString(centry, CE_urgency_weight, lGetString(master_centry, CE_urgency_weight));
      }
   }
}

bool
centry_list_are_queues_requestable(const lList *this_list) {
   bool ret = false;

   DENTER(CENTRY_LAYER);
   if (this_list != nullptr) {
      lListElem *centry = centry_list_locate(this_list, "qname");

      if (centry != nullptr) {
         ret = (lGetUlong(centry, CE_requestable) != REQU_NO) ? true : false;
      }
   }
   DRETURN(ret);
}

const char *
centry_list_append_to_dstring(const lList *this_list, dstring *string) {
   DENTER(CENTRY_LAYER);

   const char *ret = nullptr;

   if (string != nullptr) {
      bool printed = false;

      const lListElem *elem;
      for_each_ep(elem, this_list) {
         if (printed) {
            sge_dstring_append(string, ",");
         }
         sge_dstring_sprintf_append(string, "%s=", lGetString(elem, CE_name));
         if (lGetString(elem, CE_stringval) != nullptr) {
            sge_dstring_append(string, lGetString(elem, CE_stringval));
         }
         printed = true;
      }
      if (!printed) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }

   DRETURN(ret);
}

/* CLEANUP: should be replaced by centry_list_append_to_dstring() */
int
centry_list_append_to_string(lList *this_list, char *buff, u_long32 max_len) {
   int attr_fields[] = {CE_name, CE_stringval, 0};
   const char *attr_delis[] = {"=", ",", "\n"};
   int ret;

   DENTER(TOP_LAYER);

   if (buff)
      buff[0] = '\0';

   lPSortList(this_list, "%I+", CE_name);

   ret = uni_print_list(nullptr, buff, max_len, this_list, attr_fields, attr_delis, 0);
   if (ret) {
      DRETURN(ret);
   }

   DRETURN(0);
}

/* CLEANUP: add answer_list remove SGE_EVENT */
/*
 * NOTE
 *    MT-NOTE: centry_list_parse_from_string() is MT safe
 */
lList *
centry_list_parse_from_string(lList *complex_attributes,
                              const char *str, bool check_value) {
   char *cp;
   struct saved_vars_s *context = nullptr;

   DENTER(TOP_LAYER);

   /* allocate space for attribute list if no list is passed */
   if (complex_attributes == nullptr) {
      if ((complex_attributes = lCreateList("qstat_l_requests", CE_Type)) == nullptr) {
         ERROR(SFNMAX, MSG_PARSE_NOALLOCATTRLIST);
         DRETURN(nullptr);
      }
   }

   /* str now points to the attr=value pairs */
   while ((cp = sge_strtok_r(str, ", ", &context))) {
      lListElem *complex_attribute = nullptr;
      const char *attr = nullptr;
      char *value = nullptr;

      str = nullptr;       /* for the next strtoks */

      /*
      ** recursive strtoks did not work
      */
      attr = cp;
      if ((value = strchr(cp, '='))) {
         *value++ = 0;
      }

      if (attr == nullptr || *attr == '\0') {
         ERROR(MSG_SGETEXT_UNKNOWN_RESOURCE_S, "");
         lFreeList(&complex_attributes);
         sge_free_saved_vars(context);
         DRETURN(nullptr);
      }

      /*
       * If no default value was specified then use TRUE
       */
      if (!check_value && value == nullptr) {
         value = (char*)TRUE_STR;
      } else if (check_value && (value == nullptr || *value == '\0')) {
         ERROR(MSG_CPLX_VALUEMISSING_S, attr);
         lFreeList(&complex_attributes);
         sge_free_saved_vars(context);
         DRETURN(nullptr);
      }

      /* create new element, fill in the values and append it */
      complex_attribute = lGetElemStrRW(complex_attributes, CE_name, attr);
      if (complex_attribute == nullptr) {
         complex_attribute = lCreateElem(CE_Type);
         lSetString(complex_attribute, CE_name, attr);
         lAppendElem(complex_attributes, complex_attribute);
      }

      lSetString(complex_attribute, CE_stringval, value);
   }

   sge_free_saved_vars(context);

   DRETURN(complex_attributes);
}

void
centry_list_remove_duplicates(lList *this_list) {
   DENTER(TOP_LAYER);
   cull_compress_definition_list(this_list, CE_name, CE_stringval, 0);
   DRETURN_VOID;
}


/****** sgeobj/centry/centry_elem_validate() **********************************
*  NAME
*     centry_elem_validate() -- validates a element and checks for duplicates 
*
*  SYNOPSIS
*     int centry_elem_validate(lListElem *centry,
*                                  lList *centry_list,
*                                  lList *answer_list)
*
*  FUNCTION
*     Checks weather the configuration within the new centry is okay or not. 
*     A centry is valid, when it satisfies the following rules:   
*         name 	  : has to be unique
*         Short cu  : has to be unique
*         Type	     : every type from the list (string, host, cstring, int, 
*                                               double, boolean, memory, time)
*         Consumable : can only be defined for: int, double, memory, time, RSMAP
*
*         Relational operator:
*         - for consumables:              only <=
*         - for non consumables:
*            - string, host, cstring:     only ==, !=
*            - boolean:	                  only ==
*            - int, double, memory, time: ==, !=, <=, <, =>, >
*
*         Requestable	   : for all attribute
*         default value 	: only for consumables
*
*     A RSMAP must be a consumable.
*
*     The type for build in attributes is not allowed to be changed!
*
*     When no centy list is passed in, the check for uniqie name and 
*     short cuts is skipt.
*
*  INPUTS
*     lListElem *centry     - the centry list, which should be validated
*     lList *centry_list    - if not null, the function checks, if the 
*                             centry element is already in the list
*     lList *answer_list    - contains the error messages
*
*  RESULT   
*     bool  false - error (the anwer_list contains the error message)
*           true - okay
*
*******************************************************************************/
bool centry_elem_validate(lListElem *centry, const lList *centry_list,
                          lList **answer_list) {
   u_long32 relop = lGetUlong(centry, CE_relop);
   u_long32 type = lGetUlong(centry, CE_valtype);
   const char *attrname = lGetString(centry, CE_name);
   const char *temp;
   bool ret = true;

   DENTER(TOP_LAYER);

   switch (type) {
      case TYPE_RSMAP:
      case TYPE_INT:
      case TYPE_MEM:
      case TYPE_DOUBLE:
      case TYPE_TIM:
         if (relop == CMPLXEXCL_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_MUST_BOOL_TO_BE_EXCL_S, attrname);
            ret = false;
         }
         break;

      case TYPE_STR:
      case TYPE_CSTR:
      case TYPE_RESTR:
      case TYPE_HOST:
         if (!(relop == CMPLXEQ_OP || relop == CMPLXNE_OP)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_TYPE_RELOP_S, attrname);
            ret = false;
         }
         if (lGetUlong(centry, CE_consumable)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_CONSUMABLE_TYPE_SS, attrname,
                                    map_type2str(type));
            ret = false;
         }
         break;

      case TYPE_BOO:
         if (relop != CMPLXEQ_OP && relop != CMPLXEXCL_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_TYPE_RELOP_S, attrname);
            ret = false;
         }
         if (lGetUlong(centry, CE_consumable) && relop != CMPLXEXCL_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_EXCL_S, attrname,
                                    map_type2str(type));
            ret = false;
         }
         if (relop == CMPLXEXCL_OP && !lGetUlong(centry, CE_consumable)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_EXCL_MUST_BE_CONSUMABLE_S, attrname,
                                    map_type2str(type));
            ret = false;
         }

         break;

      default: /* error unknown type */
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
         ret = false;
         break;
   }

   {
      double dval;
      char error_msg[200];
      error_msg[0] = '\0';

      /* donot allow REQUESTABLE for "tmpdir" attribute, refer CR6650497 */
      if (!strcmp(attrname, "tmpdir") && lGetUlong(centry, CE_requestable) != REQU_NO) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_CENTRY_NOTREQUESTABLE_S, attrname);
         ret = false;

      }

      if (lGetUlong(centry, CE_consumable)) {

         if (relop != CMPLXEXCL_OP && relop != CMPLXLE_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_CONSUMABLE_RELOP_S, attrname);
            ret = false;
         }

         if (lGetUlong(centry, CE_requestable) == REQU_NO) {
            if (!parse_ulong_val(&dval, nullptr, type, lGetString(centry, CE_defaultval), error_msg, 199)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_PARSE_DEFAULT_SS, attrname, error_msg);
               ret = false;
            }
            if (dval == 0) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_CONSUMABLE_REQ1_S, attrname);
               ret = false;
            }
         } else if (lGetUlong(centry, CE_requestable) == REQU_FORCED) {
            if (!parse_ulong_val(&dval, nullptr, type, lGetString(centry, CE_defaultval), error_msg, 199)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_PARSE_DEFAULT_SS, attrname, error_msg);
               ret = false;
            }
            if (dval != 0) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_CONSUMABLE_REQ2_S, attrname);
               ret = false;
            }
         }
      } else if ((temp = lGetString(centry, CE_defaultval))) {

         switch (type) {
            case TYPE_RSMAP:
            case TYPE_INT:
            case TYPE_TIM:
            case TYPE_MEM:
            case TYPE_BOO:
            case TYPE_DOUBLE:

               if (!parse_ulong_val(&dval, nullptr, type, temp, error_msg, 199)) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_PARSE_DEFAULT_SS, attrname, error_msg);
                  ret = false;
               }

               /* accept non-zero default values for consumables only */
               if (dval != 0) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_DEFAULT_S, attrname);
                  ret = false;
               }

               break;
            case TYPE_HOST:
            case TYPE_STR:
            case TYPE_RESTR:
            case TYPE_CSTR:
               if (strcasecmp(temp, "NONE") != 0) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_DEFAULT_S, attrname);
                  ret = false;
               }
               break;
            default:
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
               ret = false;
         }
      }

      /* verify urgency always */
      if ((temp = lGetString(centry, CE_urgency_weight))) {
         switch (type) {
            case TYPE_RSMAP:
            case TYPE_INT:
            case TYPE_TIM:
            case TYPE_MEM:
            case TYPE_BOO:
            case TYPE_DOUBLE:
            case TYPE_HOST:
            case TYPE_STR:
            case TYPE_CSTR:
            case TYPE_RESTR:
               if (!parse_ulong_val(&dval, nullptr, TYPE_DOUBLE, temp, error_msg, 199)) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_PARSE_URGENCY_SS, attrname, error_msg);
                  ret = false;
               }
               break;

            default:
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
               ret = false;
         }
      }
   }

   if (type == TYPE_RSMAP) {
      ret = centry_check_rsmap(answer_list, lGetUlong(centry, CE_consumable), attrname);
   }


   /* check if it's a built-in value and if the type is correct */
   {
      int i;
      int type = lGetUlong(centry, CE_valtype);
      for (i = 0; i < max_queue_resources; i++) {
         if (strcmp(queue_resource[i].name, attrname) == 0 &&
             queue_resource[i].type != type) {
            if ((queue_resource[i].type != TYPE_STR && queue_resource[i].type != TYPE_CSTR &&
                 queue_resource[i].type != TYPE_RESTR) ||
                (type != TYPE_CSTR && type != TYPE_RESTR && type != TYPE_STR)) {

               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_TYPE_CHANGE_S, attrname);

               ret = false;
               break;
            }
         }
      }

      for (i = 0; i < max_host_resources; i++) {
         if (strcmp(host_resource[i].name, attrname) == 0 &&
             host_resource[i].type != type) {

            if ((host_resource[i].type != TYPE_STR && host_resource[i].type != TYPE_CSTR &&
                 host_resource[i].type != TYPE_RESTR) ||
                (type != TYPE_CSTR && type != TYPE_RESTR && type != TYPE_STR)) {

               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_TYPE_CHANGE_S, attrname);

               ret = false;
               break;
            }
         }
      }
   }

   /* check for duplicates */
   if (centry_list) {
      const char *shortcut = lGetString(centry, CE_shortcut);
      const lListElem *ce1 = centry_list_locate(centry_list, attrname);
      const lListElem *ce2 = centry_list_locate(centry_list, shortcut);

      /* 
       * if we already have a centry with this name or shortcut,
       * that is not the current centry -> cannot add/mod this one
       */
      if ((ce1 != nullptr && ce1 != centry) ||
          (ce2 != nullptr && ce2 != centry)) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_ANSWER_COMPLEXXALREADYEXISTS_SS,
                                 attrname, shortcut);
         ret = false;
      }
   }
   DRETURN(ret);
}

/* EB: CLEANUP: change order of parameter */

/****** sgeobj/centry/centry_urgency_contribution() ***************************
*  NAME
*     centry_urgency_contribution() -- Compute urgency for a particular resource
*
*  SYNOPSIS
*     double 
*     centry_urgency_contribution(int slots, const char *name, double 
*                                 value, const lListElem *centry) 
*
*  FUNCTION
*     The urgency contribution for a particular resource 'name' is determined 
*     based on the 'slot' amount and using 'value' as per slot request. The
*     urgency value in the 'centry' element is used.
*
*  INPUTS
*     int slots               - The slot amount assumed.
*     const char *name        - The resource name.
*     double value            - The per slot request.
*     const lListElem *centry - The centry element (CE_Type)
*
*  RESULT
*     double - The resulting urgency contribution 
*
*  NOTES
*     MT-NOTES: centry_urgency_contribution() is MT safe
*******************************************************************************/
double
centry_urgency_contribution(int slots, const char *name, double value,
                            const lListElem *centry) {
   double contribution, weight;
   const char *strval;
   u_long32 complex_type;

   DENTER(TOP_LAYER);

   if (!centry ||
       !(strval = lGetString(centry, CE_urgency_weight)) ||
       !(parse_ulong_val(&weight, nullptr, TYPE_INT, strval, nullptr, 0))) {
      DPRINTF("no contribution for attribute\n");
      DRETURN(0);
   }

   switch ((complex_type = lGetUlong(centry, CE_valtype))) {
      case TYPE_RSMAP:
      case TYPE_INT:
      case TYPE_TIM:
      case TYPE_MEM:
      case TYPE_BOO:
      case TYPE_DOUBLE:
         contribution = value * weight * slots;
         DPRINTF("   %s: %7f * %7f * %d    ---> %7f\n", name, value, weight, slots, contribution);
         break;

      case TYPE_STR:
      case TYPE_CSTR:
      case TYPE_HOST:
      case TYPE_RESTR:
         contribution = weight;
         DPRINTF("   %s: using weight as contrib ---> %7f\n", name, weight);
         break;

   default:
      ERROR(MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, complex_type);
      contribution = 0;
      break;
   }

   DRETURN(contribution);
}

bool
centry_list_do_all_exists(const lList *this_list, lList **answer_list,
                          const lList *centry_list) {
   bool ret = true;
   const lListElem *centry = nullptr;

   DENTER(TOP_LAYER);
   for_each_ep(centry, centry_list) {
      const char *name = lGetString(centry, CE_name);

      if (centry_list_locate(this_list, name) == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_CQUEUE_UNKNOWNCENTRY_S, name);
         DTRACE;
         ret = false;
         break;
      }
   }
   DRETURN(ret);
}

bool
centry_list_is_correct(lList *this_list, lList **answer_list) {

   bool ret = true;

   DENTER(TOP_LAYER);
   if (this_list != nullptr) {
      const lListElem *centry = lGetElemStr(this_list, CE_name, "qname");

      if (centry != nullptr) {
         const char *value = lGetString(centry, CE_stringval);

         if (strchr(value, (int) '@')) {
            answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                    ANSWER_QUALITY_ERROR,
                                    MSG_CENTRY_QINOTALLOWED);
            ret = false;
         }
      }
   }

/* do complex attributes syntax verification */
   if (ret) {
      const lListElem *elem;
      for_each_ep(elem, this_list) {
         ret = object_verify_expression_syntax(elem, answer_list);
         if (!ret) break;
      }
   }
   DRETURN(ret);
}

int ensure_attrib_available(lList **alpp, lListElem *ep, int nm, const lList *master_centry_list) {
   int ret = 0;
   lListElem *attr = nullptr;

   DENTER(TOP_LAYER);
   if (ep != nullptr) {
      for_each_rw (attr, lGetList(ep, nm)) {
         const char *name = lGetString(attr, CE_name);
         lListElem *centry = centry_list_locate(master_centry_list, name);

         if (centry == nullptr) {
            ERROR(MSG_GDI_NO_ATTRIBUTE_S, name != nullptr ? name : "<noname>");
            answer_list_add(alpp, SGE_EVENT, 
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
            break;
         } else {
            const char *fullname = lGetString(centry, CE_name);

            /*
             * Replace shortcuts by the fullname silently 
             */
            if (strcmp(fullname, name) != 0) {
               lSetString(attr, CE_name, fullname);
            }
         }
      }
   }
   DRETURN(ret);
}

/****** sge_centry/validate_load_formula() ********************
*  NAME
*     validate_load_formula() 
*
*  SYNOPSIS
*     bool validate_load_formula(lListElem *schedd_conf, lList 
*     **answer_list, lList *centry_list, const char *name) 
*
*  FUNCTION
*     The function validates a load formula string.
*
*  INPUTS
*     const char *load_formula - string that should be a valid load formula
*     lList **answer_list      - error messages
*     lList *centry_list       - list of defined complex values 
*     const char*              - name (used for error messages)
*
*  RESULT
*     bool - true if valid
*            false if unvalid 
*
* MT-NOTE: is MT-safe, works only on the passed in data
*
*******************************************************************************/
bool validate_load_formula(const char *load_formula, lList **answer_list, const lList *centry_list, const char *name) {
   bool ret = true;

   DENTER(TOP_LAYER);

   /* Check for keyword 'none' */
   if (!strcasecmp(load_formula, "none")) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_NONE_NOT_ALLOWED_S, name);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, 
                      ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /* Check complex attributes and type */
   if (ret) {
      const char *term_delim = "+-";
      const char *term, *next_term;
      struct saved_vars_s *term_context = nullptr;

      next_term = sge_strtok_r(load_formula, term_delim, &term_context);
      while ((term = next_term) && ret) {
         const char *fact_delim = "*";
         const char *fact, *next_fact, *end;
         struct saved_vars_s *fact_context = nullptr;

         next_term = sge_strtok_r(nullptr, term_delim, &term_context);

         fact = sge_strtok_r(term, fact_delim, &fact_context);
         next_fact = sge_strtok_r(nullptr, fact_delim, &fact_context);
         end = sge_strtok_r(nullptr, fact_delim, &fact_context);

         /* first factor has to be a complex attr */
         if (fact != nullptr) {
            if (strchr(fact, '$')) {
               fact++;
            }

            // in case the master list is available we can check that it is a complex name
            if (centry_list != nullptr) {
               const lListElem *cmplx_attr = centry_list_locate(centry_list, fact);

               if (cmplx_attr != nullptr) {
                  int type = lGetUlong(cmplx_attr, CE_valtype);

                  if (type == TYPE_STR || type == TYPE_CSTR || type == TYPE_HOST || type == TYPE_RESTR) {
                     snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_WRONGTYPE_ATTRIBUTE_SS, name, fact);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     ret = false;
                  }
               } else if (!sge_str_is_number(fact)) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_NOTEXISTING_ATTRIBUTE_SS, name, fact);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            }
         }
         /* is weighting factor a number? */
         if (next_fact != nullptr) {
            if (!sge_str_is_number(next_fact)) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_WEIGHTFACTNONUMB_SS, name, next_fact);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }

         /* multiple weighting factors? */
         if (end != nullptr) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MULTIPLEWEIGHTFACT_S, name);
            answer_list_add(answer_list, SGE_EVENT, 
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         sge_free_saved_vars(fact_context);
      }
      sge_free_saved_vars(term_context);
   }

   DRETURN(ret);
}

/****** sge_centry/load_formula_is_centry_referenced() *************************
*  NAME
*     load_formula_is_centry_referenced() -- search load formula for centry reference
*
*  SYNOPSIS
*     bool load_formula_is_centry_referenced(const char *load_formula, const 
*     lListElem *centry) 
*
*  FUNCTION
*     This function searches for a centry reference in the defined algebraic
*     expression
*
*  INPUTS
*     const char *load_formula - load formula expression
*     const lListElem *centry  - centry to search for
*
*  RESULT
*     bool - true if referenced
*            false if not referenced
*
*  NOTES
*     MT-NOTE: load_formula_is_centry_referenced() is MT safe 
*
*******************************************************************************/
bool load_formula_is_centry_referenced(const char *load_formula, const lListElem *centry) {
   bool ret = false;
   const char *term_delim = "+-";
   const char *term, *next_term;
   struct saved_vars_s *term_context = nullptr;
   const char *centry_name = lGetString(centry, CE_name);

   DENTER(TOP_LAYER);

   if (load_formula == nullptr) {
      DRETURN(ret);
   }

   next_term = sge_strtok_r(load_formula, term_delim, &term_context);
   while ((term = next_term) && !ret) {
      const char *fact_delim = "*";
      const char *fact;
      struct saved_vars_s *fact_context = nullptr;

      next_term = sge_strtok_r(nullptr, term_delim, &term_context);

      fact = sge_strtok_r(term, fact_delim, &fact_context);
      if (fact != nullptr) {
         if (strchr(fact, '$')) {
            fact++;
         }
         if (strcmp(fact, centry_name) == 0) {
            ret = true;
         }
      }
      sge_free_saved_vars(fact_context);
   }
   sge_free_saved_vars(term_context);

   DRETURN(ret);
}

const char *sge_get_dominant_stringval(lListElem *rep, u_long32 *dominant_p, dstring *resource_string_p) {
   const char *s = nullptr;
   u_long32 type = lGetUlong(rep, CE_valtype);

   DENTER(TOP_LAYER);

   switch (type) {
      case TYPE_HOST:
      case TYPE_STR:
      case TYPE_CSTR:
      case TYPE_RESTR:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            s = lGetString(rep, CE_pj_stringval);
         } else {
            *dominant_p = lGetUlong(rep, CE_dominant);
            s = lGetString(rep, CE_stringval);
         }
         break;
      case TYPE_TIM:

         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            double val = lGetDouble(rep, CE_pj_doubleval);

            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_time_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            double val = lGetDouble(rep, CE_doubleval);

            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_time_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
      case TYPE_MEM:

         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            double val = lGetDouble(rep, CE_pj_doubleval);

            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_memory_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            double val = lGetDouble(rep, CE_doubleval);

            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_memory_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
      case TYPE_INT:
      case TYPE_RSMAP:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            double val = lGetDouble(rep, CE_pj_doubleval);
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_int_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            double val = lGetDouble(rep, CE_doubleval);

            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_int_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
      default:

         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            double val = lGetDouble(rep, CE_pj_doubleval);

            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            double val = lGetDouble(rep, CE_doubleval);

            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
   }

   DRETURN(s);
}

int slot_signum(int slots) {
   int ret = 0;

   if (slots > 0) {
      ret = 1;
   } else if (slots < 0) {
      ret = -1;
   }

   return ret;
}

/**
 * Evaluate if booking for a specific consumable shall actually be done.
 *
 * @param[in] consumable type, e.g. CONSUMABLE_NO, CONSUMABLE_YES, ...
 * @param[in] is_master_task is booking done for the master task of a parallel job (or the one task of a sequential job)
 * @param[in] do_per_host_booking shall booking be done for a per-host consumable (true only for the first task on a host)
 * @return true, if booking shall be done, else false
 */
bool consumable_do_booking(u_long32 consumable, bool is_master_task, bool do_per_host_booking) {
   bool ret = true;

   switch (consumable) {
      case CONSUMABLE_NO:
         ret = false;
         break;
      case CONSUMABLE_JOB:
         if (!is_master_task) {
            ret = false;
         }
         break;
      case CONSUMABLE_HOST:
         // if we do not do per_host_booking,
         // or we already did the booking for a per-host variable
         if (!do_per_host_booking) {
            ret = false;
         }
         break;
   }

   return ret;
}

/**
 * Returns the number of slots which shall be debited for.
 * Depends on the consumable type,
 * for CONSUMABLE_YES it is the given number of slots,
 * but for CONSUMABLE_JOB and CONSUMABLE_HOST requests are only booked once (1, or -1 for undebiting)
 *
 * @param[in] consumable type, e.g. CONSUMABLE_NO, CONSUMABLE_YES, ...
 * @param[in] slots, the number of slots (tasks) which shall be booked on a resource
 */
int consumable_get_debit_slots(u_long32 consumable, int slots) {
   // default: CONSUMABLE_YES
   int ret = slots;

   if (consumable == CONSUMABLE_JOB || consumable == CONSUMABLE_HOST) {
      // it's a job consumable or a host consumable, we don't multiply with slots
      ret = slot_signum(slots);
   }

   return ret;
}
