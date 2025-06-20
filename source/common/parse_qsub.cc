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
#include <cstdlib>
#include <unistd.h>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/ocs_Binding.h"

#include "parse_job_cull.h"
#include "parse_qsub.h"
#include "sge_options.h"
#include "msg_common.h"

static int var_list_parse_from_environment(lList **lpp, char **envp);
static int sge_parse_checkpoint_interval(const char *time_str);
static int set_yn_option (lList **opts, u_long32 opt, const char *arg, const char *value, lList **alpp);
static int ocs_parse_sync_switch(lList **opts, u_long32 opt, const char *arg, const char *value, lList **alpp);


/*
** NAME
**   cull_parse_cmdline
** PARAMETER
**   prog_number         - program number (QSUB, QSH etc.)
**   arg_list            - argument string list, e.g. argv,
**                         knows qsub, qalter and qsh options
**   envp                - pointer to environment
**   pcmdline            - nullptr or pointer to list, SPA_Type
**                         set to contain the parsed options
**   flags               - FLG_USE_PSEUDOS: apply the following syntax:
**                         the first non-switch token is the job script, what
**                         follows are job arguments, mark the tokens as "script"
**                         or "jobarg" in the SPA_switch_val field
**                         0: dont do that, if a non-switch occurs, add it to
**                         the list with SPA_switch_val "" (an argument to a non-existing
**                         switch)
**                         FLG_QALTER: change treatment of non-options to qalter-specific
**                         pseudo options (only if FLG_USE_PSEUDOS is given) and changes
**                         parsing of -h
**
** RETURN
**   answer list, AN_Type or nullptr if everything ok, the following stati can occur:
**   STATUS_EUNKNOWN   - bad internal error like nullptr pointer received or no memory
**   STATUS_EEXIST     - option has been specified more than once, should be treated
**                       as a warning
**   STATUS_ESEMANTIC  - option that should have an argument had none
**   STATUS_ESYNTAX    - error parsing option argument
** DESCRIPTION
**
** NOTES
**    MT-NOTE: cull_parse_cmdline() is MT safe
*/
lList *cull_parse_cmdline(
        u_long32 prog_number,
        const char **arg_list,
        char **envp,
        lList **pcmdline,
        u_long32 flags
) {
   const char **sp;
   lList *answer = nullptr;
   char str[MAX_STRING_SIZE];
   lListElem *ep_opt;
   int i_ret;
   u_long32 is_qalter = flags & FLG_QALTER;
   bool is_hold_option = false;

   DENTER(TOP_LAYER);

   if (!arg_list || !pcmdline) {
      answer_list_add(&answer, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   sp = arg_list;

   /* reset hard/soft flag */
   int hard_soft_flag = 0;
   char scope_flag = JRS_SCOPE_GLOBAL;

   while (*sp) {
      DPRINTF("==> %s\n", *sp);
/*----------------------------------------------------------------------------*/
      /* "-a date_time */

      if (!strcmp("-a", *sp)) {
         u_long32 timeval;

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, 
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field(s) is "date_time" */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-a");

            DRETURN(answer);
         }

         if (!ulong_parse_date_time_from_string(&timeval, nullptr, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTOAOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, a_OPT, lUlong64T, *(sp - 1), *sp);
         lSetUlong64(ep_opt, SPA_argval_lUlong64T, sge_gmt32_to_gmt64(timeval));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-A account_string" */

      if (!strcmp("-A", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, 
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is account_string */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-A");
             DRETURN(answer);
         }

         DPRINTF("\"-A %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, A_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* either
       *    -ar ar_id
       * or
       *    -ar ar_id_list
       */
      if (prog_number == QRSTAT) {
         /* "-ar  advance_reservation */

         if (!strcmp("-ar", *sp)) {
            lList *ar_id_list = nullptr;

            sp++;
            if (!*sp) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, 
                                       "-ar");                
               DRETURN(answer);
            }

            DPRINTF("\"-ar %s\"\n", *sp);

            ulong_list_parse_from_string(&ar_id_list, &answer, *sp, ",");

            ep_opt = sge_add_arg(pcmdline, ar_OPT, lListT, *(sp - 1), *sp);
            lSetList(ep_opt, SPA_argval_lListT, ar_id_list);

            sp++;
            continue;
         }  
      } else {
         /* "-ar  advance_reservation */

         if (!strcmp("-ar", *sp)) {
            u_long32 ar_id;
            double ar_id_d;

            sp++;
            if (!*sp) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, 
                                       "-ar");                
               DRETURN(answer);
            }

            if (!parse_ulong_val(&ar_id_d, nullptr, TYPE_INT, *sp, nullptr, 0)) {
               answer_list_add(&answer, MSG_PARSE_INVALID_AR_MUSTBEUINT,
                                STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }


            if (ar_id_d < 0) {
               answer_list_add(&answer, MSG_PARSE_INVALID_AR_MUSTBEUINT,
                                STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }

            ar_id = ar_id_d;
            
            ep_opt = sge_add_arg(pcmdline, ar_OPT, lUlongT, *(sp - 1), *sp);
            lSetUlong(ep_opt, SPA_argval_lUlongT, ar_id);

            sp++;
            continue;
         }  
      }
/*----------------------------------------------------------------------------*/
      /* "-ac context_list */
      if (!strcmp("-ac", *sp)) {
         lList *variable_list = nullptr;
         lListElem* lep = nullptr;

         /* next field is context_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_ACOPTIONMUSTHAVECONTEXTLISTLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF("\"-ac %s\"\n", *sp);
         i_ret = var_list_parse_from_string(&variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                     MSG_ANSWER_WRONGCONTEXTLISTFORMATAC_S , *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, ac_OPT, lListT, *(sp - 1), *sp);
         lep = lCreateElem(VA_Type);
         lSetString(lep, VA_variable, "+");
         lInsertElem(variable_list, nullptr, lep);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-b y|n" */

      if (!is_qalter && !strcmp("-b", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-b");
             DRETURN(answer);
         }

         DPRINTF("\"-b %s\"\n", *sp);

         if (set_yn_option(pcmdline, b_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-binding [set|env|pe] linear:<amount>[:<socket>,<core>] 
            | striding:<amount>:<stepsize>[:<socket>,<core>] 
            | explicit:<socket>,<core>[:<socket>,<core>]* " */

      if (!strcmp("-binding", *sp)) {
         lListElem *binding_elem = lCreateElem(BN_Type);
         dstring argument_string = DSTRING_INIT;
         const char *switch_name = "-binding";
         const char *switch_argument = nullptr;

         /* next field is [set|env|pe] "linear" or "striding" or "explicit" */
         sp++;
         if (*sp == nullptr) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-binding");
            DRETURN(answer);
         }
         if (strcmp(*sp, "set") == 0 || strcmp(*sp, "env") == 0 || strcmp(*sp, "pe") == 0) {

            sge_dstring_append(&argument_string, *sp);
            sge_dstring_append_char(&argument_string, ' ');
            sp++;
            if (*sp == nullptr) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                        MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-binding");
               sge_dstring_free(&argument_string);
               DRETURN(answer);
            }
         }
         sge_dstring_append(&argument_string, *sp);
         switch_argument = sge_dstring_get_string(&argument_string);

         if (ocs::BindingIo::binding_parse_from_string(binding_elem, &answer, &argument_string)) {
            lList *binding_list = lCreateList("binding", BN_Type);

            lAppendElem(binding_list, binding_elem);
            ep_opt = sge_add_arg(pcmdline, binding_OPT, lListT, switch_name, switch_argument);
            lSetList(ep_opt, SPA_argval_lListT, binding_list);
         } else {
            /* answer has ween written by binding_parse_from_string() */
            sge_dstring_free(&argument_string);
            DRETURN(answer);
         }
         sge_dstring_free(&argument_string);
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------*/
      /* "-c [op] interval */

      if (!strcmp("-c", *sp)) {
          int attr;
          long interval;

         /* next field is [op] interval */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-c");  
            DRETURN(answer);
         }

         DPRINTF("\"%s %s\"\n", *(sp - 1), *sp);

         attr = sge_parse_checkpoint_attr(*sp);
         if (attr == 0) {
            interval = sge_parse_checkpoint_interval(*sp);
            if (!interval) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                       SFNMAX, MSG_PARSE_CARGUMENTINVALID);
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, c_OPT, lLongT, *(sp - 1), *sp);
            lSetLong(ep_opt, SPA_argval_lLongT, (long) interval);
         }
         else if (attr == -1) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    SFNMAX, MSG_PARSE_CSPECIFIERINVALID);
            DRETURN(answer);
         }
         else {
            ep_opt = sge_add_arg(pcmdline, c_OPT, lIntT, *(sp - 1), *sp);
            lSetInt(ep_opt, SPA_argval_lIntT, attr);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-cell cell_name or -ckpt ckpt_object" */

      /* This is another small change that needed to be made to allow DRMAA to
       * resuse this method.  The -cat option doesn't actually exist for any
       * command line utility.  It is a qsub style representation of the
       * DRMAA_JOB_CATEGORY attribute.  It will be processed and removed by the
       * drmaa_job2sge_job method. */
      if (!strcmp ("-cat", *sp) || !strcmp("-cell", *sp) || !strcmp("-ckpt", *sp)) {

         /* next field is job_cat or cell_name or ckpt_object*/
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *(sp - 1));
            DRETURN(answer);
         }

         DPRINTF("\"%s %s\"\n", *(sp - 1), *sp);

         ep_opt = sge_add_arg(pcmdline, 0, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-clear" */

      if (!strcmp("-clear", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, clear_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-explain" */

      if (!strcmp("-explain", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, explain_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-xml" */

      if (!strcmp("-xml", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, xml_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-cwd" is mapped as -wd with nullptr path, this case is handled in the second parsing stage*/

      if (!strcmp("-cwd", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, wd_OPT, "-wd", nullptr);
         DPRINTF("\"%s\"\n", *sp);
         
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-C directive_prefix" */
      if (!strcmp("-C", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is directive_prefix */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-C");
             DRETURN(answer);
         }

         DPRINTF("\"-C %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, C_OPT, lStringT, *(sp - 1), *sp);

         if (strlen(*sp) > 0) {
            lSetString(ep_opt, SPA_argval_lStringT, *sp);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-d time */
      if (!strcmp("-d", *sp)) {
         u_long32 timeval;
         char tmp[1000];

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is "time" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-d");
             DRETURN(answer);
         }

         DPRINTF("\"-d %s\"\n", *sp);

         if (!parse_ulong_val(nullptr, &timeval, TYPE_TIM, *sp, tmp, sizeof(tmp)-1)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTODOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, d_OPT, lUlong64T, *(sp-1), *sp);
         lSetUlong64(ep_opt, SPA_argval_lUlong64T, sge_gmt32_to_gmt64(timeval));

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-dc simple_context_list */
      if (!strcmp("-dc", *sp)) {
         lList *variable_list = nullptr;
         lListElem* lep = nullptr;

         /* next field is simple_context_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_DCOPTIONMUSTHAVESIMPLECONTEXTLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF("\"-dc %s\"\n", *sp);
         i_ret = var_list_parse_from_string(&variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                     MSG_PARSE_WRONGCONTEXTLISTFORMATDC_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, dc_OPT, lListT, *(sp - 1), *sp);
         lep = lCreateElem(VA_Type);
         lSetString(lep, VA_variable, "-");
         lInsertElem(variable_list, nullptr, lep);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dept name" */
      if (!strcmp("-dept", *sp)) {
         DTRACE;

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            snprintf(str, sizeof(str), MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }

         /* next field is the department name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-dept");
             DRETURN(answer);
         }

         DPRINTF("\"-dept %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, dept_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-display -- only for qsh " */

      if (!strcmp("-display", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is display to redirect interactive jobs to */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_DISPLAYOPTIONMUSTHAVEARGUMENT);
             DRETURN(answer);
         }

         DPRINTF("\"-display %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, display_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;

      }

/*----------------------------------------------------------------------------*/
      /* "-dl date_time */

      if (!strcmp("-dl", *sp)) {
         u_long32 timeval;

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, 
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field(s) is "date_time" */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-dl");
            DRETURN(answer);
         }

         if (!ulong_parse_date_time_from_string(&timeval, nullptr, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_WRONGTIMEFORMATXSPECTODLOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, dl_OPT, lUlongT, *(sp - 1), *sp);
         lSetUlong64(ep_opt, SPA_argval_lUlong64T, sge_gmt32_to_gmt64(timeval));

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-e path_name" */

      if (!strcmp("-e", *sp)) {
         lList *path_list = nullptr;

         if (prog_number == QRSUB) {
            if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
               answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                       MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            }
         }

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-e");
             DRETURN(answer);
         }

         DPRINTF("\"-e %s\"\n", *sp);

         if (prog_number == QRSUB) {
            u_long32 timeval;
            if (!ulong_parse_date_time_from_string(&timeval, nullptr, *sp)) {
               answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTOAOPTION_S, *sp);
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, e_OPT, lUlong64T, *(sp-1), *sp);
            lSetUlong64(ep_opt, SPA_argval_lUlong64T, sge_gmt32_to_gmt64(timeval));
         } else {
            /* next field is path_name */
            i_ret = cull_parse_path_list(&path_list, *sp);
            if (i_ret) {
                answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                        MSG_PARSE_WRONGPATHLISTFORMATXSPECTOEOPTION_S, *sp);
                DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, e_OPT, lListT, *(sp-1), *sp);
            lSetList(ep_opt, SPA_argval_lListT, path_list);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-i path_name" */

      if (!strcmp("-i", *sp)) {
         lList *path_list = nullptr;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-i");
             DRETURN(answer);
         }

         DPRINTF("\"-i %s\"\n", *sp);

         i_ret = cull_parse_path_list(&path_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_WRONGPATHLISTFORMATXSPECTOEOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, i_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, path_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-h [hold_list]" */

      if (!strcmp("-h", *sp)) {
            int hold;
            const char *cmd_switch;
            const char *cmd_arg = "";
         is_hold_option = true;
         cmd_switch = *sp;

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         if (is_qalter) {
            /* next field is hold_list */
            sp++;
            if (!*sp) {
                answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                        MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-h");
                DRETURN(answer);
            }
            hold = sge_parse_hold_list(*sp, prog_number);
            if (hold == -1) {
                answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                        MSG_PARSE_UNKNOWNHOLDLISTXSPECTOHOPTION_S, *sp);
                DRETURN(answer);
            }
            cmd_arg = *sp;
         }
         else {
            hold = MINUS_H_TGT_USER;
         }
         DPRINTF("\"-h %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, cmd_switch, cmd_arg);
         lSetInt(ep_opt, SPA_argval_lIntT, hold);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-hard" */

      if (!strcmp("-hard", *sp)) {

         DPRINTF("\"%s\"\n", *sp);

         hard_soft_flag = 1;
         ep_opt = sge_add_noarg(pcmdline, hard_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
     /*  -he y[es]|n[o] */

      if(!strcmp("-he", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            snprintf(str, sizeof(str), MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }
         /* next field is yes/no switch */
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-he");
            DRETURN(answer);
         }
         
         DPRINTF("\"-he %s\"\n", *sp);
         
         if (set_yn_option(pcmdline, he_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-help" */

      if (!strcmp("-help", *sp)) {
         DPRINTF("\"%s\"\n", *sp);

         ep_opt = sge_add_noarg(pcmdline, help_OPT, *sp, nullptr);

         sp++;
         continue;

      }

/*-----------------------------------------------------------------------------*/
      /* "-hold_jid jid[,jid,...]" */

      if (!strcmp("-hold_jid", *sp)) {
         lList *jid_hold_list = nullptr;

         /* next field is hold_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-hold_jid");
             DRETURN(answer);
         }

         DPRINTF("\"-hold_jid %s\"\n", *sp);
         i_ret = cull_parse_jid_hold_list(&jid_hold_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                        MSG_PARSE_WRONGJIDHOLDLISTFORMATXSPECTOHOLDJIDOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, hold_jid_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, jid_hold_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-hold_jid_ad jid[,jid,...]" */

      if (!strcmp("-hold_jid_ad", *sp)) {
         lList *jid_ad_hold_list = nullptr;

         /* next field is hold_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-hold_jid_ad");
             DRETURN(answer);
         }

         DPRINTF("\"-hold_jid_ad %s\"\n", *sp);
         i_ret = cull_parse_jid_hold_list(&jid_ad_hold_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                        MSG_PARSE_WRONGJIDHOLDLISTFORMATXSPECTOHOLDJIDADOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, hold_jid_ad_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, jid_ad_hold_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-j y|n" */

      if (!strcmp("-j", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                   MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-j");
             DRETURN(answer);
         }

         DPRINTF("\"-j %s\"\n", *sp);

         if (set_yn_option(pcmdline, j_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-js jobshare */

      if (!strcmp("-js", *sp)) {
         u_long32 jobshare;
         double jobshare_d;

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-js");
            DRETURN(answer);
         }

         if (!parse_ulong_val(&jobshare_d, nullptr, TYPE_INT, *sp, nullptr, 0)) {
            answer_list_add(&answer, MSG_PARSE_INVALIDJOBSHAREMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }


         if (jobshare_d < 0) {
            answer_list_add(&answer, MSG_PARSE_INVALIDJOBSHAREMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }

         jobshare = jobshare_d;
         
         ep_opt = sge_add_arg(pcmdline, js_OPT, lUlongT, *(sp - 1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, jobshare);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-jsv path_name" */

      if (!strcmp("-jsv", *sp)) {
         dstring input = DSTRING_INIT;
         dstring type = DSTRING_INIT;
         dstring user = DSTRING_INIT;
         dstring path = DSTRING_INIT;
         lList *path_list = nullptr;
         bool success;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-jsv" );
             DRETURN(answer);
         }

         DPRINTF("\"-jsv %s\"\n", *sp);

         sge_dstring_append(&input, *sp);
         success = jsv_url_parse(&input, &answer, &type, &user, &path, true);
         sge_dstring_free(&input);
         sge_dstring_free(&type);
         sge_dstring_free(&user);
         sge_dstring_free(&path);
         if (success) {
            lListElem *elem = lAddElemStr(&path_list, PN_path, *sp, PN_Type);

            if (elem != nullptr) {
               ep_opt = sge_add_arg(pcmdline, jsv_OPT, lListT, *(sp - 1), *sp);
               lSetList(ep_opt, SPA_argval_lListT, path_list);
            } else {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                       SFNMAX, MSG_SGETEXT_NOMEM);
               DRETURN(answer);
            }
         } else {
            /* answer is filled in jsv_url_parse */
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*---------------------------------------------------------------------------*/
      /* "-l resource_list" */

      if (!strcmp("-l", *sp)) {
         lList *resource_list = nullptr;

         /* next field is resource_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-l" );
             DRETURN(answer);
         }

         DPRINTF("\"-l %s\"\n", *sp);

         resource_list = centry_list_parse_from_string(nullptr, *sp, false);
         if (!resource_list) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_WRONGRESOURCELISTFORMATXSPECTOLOPTION_S, *sp);
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, l_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, resource_list);
         lSetInt(ep_opt, SPA_argval_lIntT, hard_soft_flag);
         lSetChar(ep_opt, SPA_argval_lCharT, scope_flag);

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-m mail_options */

      if (!strcmp("-m", *sp)) {
         int mail_options;

         /* next field is mail_options */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-m");
             DRETURN(answer);
         }

         DPRINTF("\"-m %s\"\n", *sp);
         mail_options = sge_parse_mail_options(&answer, *sp, prog_number);
         if (!mail_options) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGMAILOPTIONSLISTFORMATXSPECTOMOPTION_S, *sp);
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, m_OPT, lIntT, *(sp - 1), *sp);
         lSetInt(ep_opt, SPA_argval_lIntT, mail_options);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-masterq destination_identifier_list" */

      if (!strcmp("-masterq", *sp)) {
         lList *id_list = nullptr;

         /* next field is destination_identifier */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEDESTIDLISTARGUMENT_S, "-masterq");
             DRETURN(answer);
         }

         DPRINTF("\"-masterq %s\"\n", *sp);
         i_ret = cull_parse_destination_identifier_list(&id_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOXOPTION_SS, *sp, "-masterq");
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, masterq_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, id_list);
         lSetInt(ep_opt, SPA_argval_lIntT, 0);
         lSetChar(ep_opt, SPA_argval_lCharT, JRS_SCOPE_MASTER);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-M mail_list" */

      if (!strcmp("-M", *sp)) {
         lList *mail_list = nullptr;

         /* next field is mail_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-M" );
             DRETURN(answer);
         }

         DPRINTF("\"-M %s\"\n", *sp);

         i_ret = mailrec_parse(&mail_list, *sp);
          if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGMAILLISTFORMATXSPECTOMOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, M_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, mail_list);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-N name" */

      if (!strcmp("-N", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                  MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-N" );
             DRETURN(answer);
         }
         if (strchr(*sp, '/')) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                 SFNMAX, MSG_PARSE_ARGUMENTTONOPTIONMUSTNOTCONTAINBSL );
             DRETURN(answer);
         }

         if (**sp == '\0') {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    SFNMAX, MSG_PARSE_EMPTYSTRINGARGUMENTTONOPTIONINVALID);
             DRETURN(answer);
         }

         DPRINTF("\"-N %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, N_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-notify" */

      if (!strcmp("-notify", *sp)) {

         DPRINTF("\"-notify\"\n");

         ep_opt = sge_add_noarg(pcmdline, notify_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
     /*  -now y[es]|n[o] */

      if(!strcmp("-now", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                     MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }
         /* next field is yes/no switch */
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-now" );
            DRETURN(answer);
         }
         
         DPRINTF("\"-now %s\"\n", *sp);
         
         if (set_yn_option(pcmdline, now_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
            
/*----------------------------------------------------------------------------*/
      /* "-o path_name" */

      if (!strcmp("-o", *sp)) {
         lList *stdout_path_list = nullptr;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-o" );
             DRETURN(answer);
         }

         DPRINTF("\"-o %s\"\n", *sp);
         i_ret = cull_parse_path_list(&stdout_path_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGSTDOUTPATHLISTFORMATXSPECTOOOPTION_S, *sp );
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, o_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, stdout_path_list);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ot override tickets */

      if (!strcmp("-ot", *sp)) {
         int otickets;
         double otickets_d;

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-ot");
             DRETURN(answer);
         }

         if (!parse_ulong_val(&otickets_d, nullptr, TYPE_INT, *sp, nullptr, 0)) {
            answer_list_add(&answer, MSG_PARSE_INVALIDOTICKETSMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }


         if (otickets_d < 0) {
            answer_list_add(&answer, MSG_PARSE_INVALIDOTICKETSMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }

         otickets = otickets_d;
         
         ep_opt = sge_add_arg(pcmdline, ot_OPT, lIntT, *(sp - 1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, otickets);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-p priority */

      if (!strcmp("-p", *sp)) {
         int priority;

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-p" );
             DRETURN(answer);
         }

         if (!ulong_parse_priority(&answer, &priority, *sp)) {
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, p_OPT, lIntT, *(sp - 1), *sp);
         lSetInt(ep_opt, SPA_argval_lIntT, priority);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-P name" */

      if (!strcmp("-P", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            snprintf(str, sizeof(str), MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }

         /* next field is the projects name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-P");
             DRETURN(answer);
         }

         DPRINTF("\"-P %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, P_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      if (!strcmp("-pe", *sp)) {
         lList *pe_range = nullptr;
         dstring d_arg = DSTRING_INIT;

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }
DTRACE;
         /* next field must be the pe_name ... */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    SFNMAX, MSG_PARSE_PEOPTIONMUSTHAVEPENAMEARGUMENT );
             sge_dstring_free(&d_arg);
             DRETURN(answer);
         }

         /* ... followed by a range */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_PEOPTIONMUSTHAVERANGEAS2NDARGUMENT, 
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             sge_dstring_free(&d_arg);
             DRETURN(answer);
         }

         range_list_parse_from_string(&pe_range, &answer, *sp, 
                                      false, false, INF_ALLOWED);

         if (!pe_range) {
            sge_dstring_free(&d_arg);
            DRETURN(answer);
         }
         sge_dstring_append(&d_arg, *(sp-1));
         sge_dstring_append(&d_arg, " ");
         sge_dstring_append(&d_arg, *sp);
         ep_opt = sge_add_arg(pcmdline, pe_OPT, lStringT, *(sp - 2), sge_dstring_get_string(&d_arg));
         lSetString(ep_opt, SPA_argval_lStringT, *(sp - 1));
         lSetList(ep_opt, SPA_argval_lListT, pe_range);

         sp++;

         sge_dstring_free(&d_arg);

         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-q destination_identifier_list" */

      if (!strcmp("-q", *sp)) {
         lList *id_list = nullptr;

         /* next field is destination_identifier */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEDESTIDLISTARGUMENT_S, "-q");
             DRETURN(answer);
         }

         DPRINTF("\"-q %s\"\n", *sp);
         i_ret = cull_parse_destination_identifier_list(&id_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOXOPTION_SS, *sp, "-q");
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, q_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, id_list);
         lSetInt(ep_opt, SPA_argval_lIntT, hard_soft_flag);
         lSetChar(ep_opt, SPA_argval_lCharT, scope_flag);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-r y|n" */

      if (!strcmp("-r", *sp)) {


         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-r" );
            DRETURN(answer);
         }

         DPRINTF("\"-r %s\"\n", *sp);

         if ((strcasecmp("y", *sp) == 0) || (strcasecmp("yes", *sp) == 0)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            lSetInt(ep_opt, SPA_argval_lIntT, 1);
         } else if ((strcasecmp ("n", *sp) == 0) || (strcasecmp ("no", *sp) == 0)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            lSetInt(ep_opt, SPA_argval_lIntT, 2);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_INVALIDOPTIONARGUMENTRX_S, *sp );
            DRETURN(answer);
         }

         sp++;
         continue;

      }

/*-----------------------------------------------------------------------------*/
      /* "-R y|n" */

      if (!strcmp("-R", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                 MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-R" );
             DRETURN(answer);
         }

         DPRINTF("\"-R %s\"\n", *sp);

         if (set_yn_option(pcmdline, R_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sc variable_list" */
      /* set context */

      if (!strcmp("-sc", *sp)) {
         lList *variable_list = nullptr;
         lListElem* lep = nullptr;

         /* next field is context_list  [ == variable_list ] */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_SCOPTIONMUSTHAVECONTEXTLISTARGUMENT, 
                     STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-sc %s\"\n", *sp);
         i_ret = var_list_parse_from_string(&variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                          MSG_PARSE_WRONGCONTEXTLISTFORMATSCX_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, sc_OPT, lListT, *(sp - 1), *sp);
         lep = lCreateElem(VA_Type);
         lSetString(lep, VA_variable, "=");
         lInsertElem(variable_list, nullptr, lep);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scope" */

      if (strcmp("-scope", *sp) == 0) {

         DPRINTF("\"%s\"\n", *sp);

         sp++;
         if (*sp == nullptr) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-scope");
            DRETURN(answer);
         }

         if (!job_parse_scope_string(*sp, scope_flag)) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_INVALIDOPTIONARGUMENT_SS, "-scope", *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_noarg(pcmdline, scope_OPT, *(sp - 1), nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-soft" */

      if (!strcmp("-soft", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         hard_soft_flag = 2;
         ep_opt = sge_add_noarg(pcmdline, soft_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-shell y|n" */

      if (!is_qalter && !strcmp("-shell", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-shell");
             DRETURN(answer);
         }

         DPRINTF("\"-shell %s\"\n", *sp);

         if (set_yn_option (pcmdline, shell_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
     /*  -sync y[es]|n[o] */
     /*  -sync r|x|n */

      if (!strcmp("-sync", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-sync");
            DRETURN(answer);
         }
         
         DPRINTF("\"-sync %s\"\n", *sp);
         
         if (ocs_parse_sync_switch(pcmdline, sync_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
            
/*----------------------------------------------------------------------------*/
      /* "-S path_name" */

      if (!strcmp("-S", *sp)) {
         lList *shell_list = nullptr;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_SOPTIONMUSTHAVEPATHNAMEARGUMENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-S %s\"\n", *sp);

         i_ret = cull_parse_path_list(&shell_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_WRONGSHELLLISTFORMATXSPECTOSOPTION_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, S_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, shell_list);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* -t <TaskIdRange>
       */

      if (!strcmp("-t", *sp)) {
         lList *task_id_range_list = nullptr;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_TOPTIONMUSTHAVEALISTOFTASKIDRANGES, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-t %s\"\n", *sp);

         range_list_parse_from_string(&task_id_range_list, &answer, *sp,
                                      false, true, INF_NOT_ALLOWED);
         if (!task_id_range_list) {
            DRETURN(answer);
         }

         range_list_sort_uniq_compress(task_id_range_list, &answer, false);
         if (lGetNumberOfElem(task_id_range_list) > 1) {
            answer_list_add(&answer, MSG_QCONF_ONLYONERANGE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            lFreeList(&task_id_range_list);
            DRETURN(answer);
         }


         ep_opt = sge_add_arg(pcmdline, t_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, task_id_range_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-tc number */

      if (!strcmp("-tc", *sp)) {
         int number;

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-tc" );
             DRETURN(answer);
         }

         if (!ulong_parse_task_concurrency(&answer, &number, *sp)) {
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, tc_OPT, lUlongT, *(sp - 1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, number);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-terse" */

      if(!strcmp("-terse", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, terse_OPT, *sp, nullptr);
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-u" */
      if (!strcmp("-u", *sp)) {
         lList *user_list = nullptr;

         /* next field is user_list */
         sp++;
         if (!*sp) {
            answer_list_add(&answer, MSG_PARSE_UOPTMUSTHAVEALISTUSERNAMES,
                  STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
 
         DPRINTF("\"-u %s\"\n", *sp);

         if (prog_number == QRSUB) {
            int rule[] = {ARA_name, 0};
            char **dest = nullptr;
            char *tmp;

            tmp = sge_strdup(nullptr, *sp);
            dest = string_list(tmp, ",", nullptr);
            cull_parse_string_list(dest, "user_list", ARA_Type, rule, &user_list);
            sge_free(&tmp);
            sge_free(&dest);
         } else {
            str_list_parse_from_string(&user_list, *sp, ",");
         }

         ep_opt = sge_add_arg(pcmdline, u_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, user_list);  
         sp++;

         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-uall " */
      if (!strcmp("-uall", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, u_OPT, *sp, nullptr);

         DPRINTF("\"-uall \"\n");
         sp++;
  
         continue;
      }           

/*-----------------------------------------------------------------------------*/
      /* "-v variable_list" */

      if (!strcmp("-v", *sp)) {
         lList *variable_list = nullptr;

         /* next field is variable_list */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_VOPTMUSTHAVEVARIABLELISTARGUMENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-v %s\"\n", *sp);

         i_ret = var_list_parse_from_string(&variable_list, *sp, 1);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                  MSG_PARSE_WRONGVARIABLELISTFORMATVORENVIRONMENTVARIABLENOTSET_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, v_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-verify" */

      if (!strcmp("-verify", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, verify_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-V" */

      if (!strcmp("-V", *sp)) {
         lList *env_list = nullptr;

         DPRINTF("\"%s\"\n", *sp);
         i_ret = var_list_parse_from_environment(&env_list, envp);
         if (i_ret) {
             answer_list_add(&answer, MSG_PARSE_COULDNOTPARSEENVIRIONMENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, V_OPT, lListT, *sp, "");
         lSetList(ep_opt, SPA_argval_lListT, env_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-w e|w|n|v" */

      if (!strcmp("-w", *sp)) {
         int level;
         int lret;

         if (lGetElemStr(*pcmdline, SPA_switch_val, *sp)) {
            answer_list_add_sprintf(&answer,STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S,
                                    *sp);
         }

         /* next field is "e|w|n|v|p" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-w");
             DRETURN(answer);
         }

         DPRINTF("\"-w %s\"\n", *sp);
         lret = job_parse_validation_level(&level, *sp, prog_number, &answer);
         if (!lret) {
            DRETURN(answer);
         } else {
            ep_opt = sge_add_arg(pcmdline, w_OPT, lIntT, *(sp - 1), *sp);
            lSetInt(ep_opt, SPA_argval_lIntT, level);
         }

         sp++;
         continue;

      }
/*-----------------------------------------------------------------------------*/
      /* "-wd" */

      if (!strcmp("-wd", *sp)) {

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer,STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                  MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *(sp - 1));
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, wd_OPT, lStringT, "-wd", *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-@" */
      /* reentrancy is built upon here */

      if (!strcmp("-@", *sp)) {
         lList *alp;
         const lListElem *aep;
         int do_exit = 0;

         sp++;
         if (!*sp) {
             snprintf(str, sizeof(str), SFNMAX, MSG_PARSE_ATSIGNOPTIONMUSTHAVEFILEARGUMENT);
             answer_list_add(&answer, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-@ %s\"\n", *sp);

         alp = parse_script_file(prog_number, *sp, "", pcmdline, envp, FLG_USE_NO_PSEUDOS); /* MT-NOTE: !!!! */
         for_each_ep(aep, alp) {
            u_long32 quality;

            quality = lGetUlong(aep, AN_quality);
            if (quality == ANSWER_QUALITY_ERROR) {
               do_exit = 1;
            }
            lAppendElem(answer, lCopyElem(aep));
         }

         lFreeList(&alp);
         if (do_exit) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-verbose" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-verbose", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, verbose_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-inherit" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-inherit", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, inherit_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-pty" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-pty", *sp)) {
         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-pty");
             DRETURN(answer);
         }

         DPRINTF("\"-pty %s\"\n", *sp);

         if (set_yn_option(pcmdline, pty_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-suspend_remote" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-suspend_remote", *sp)) {
         /* next field is "y|n" */
# if !defined(UGE)
         answer_list_add_sprintf(&answer, STATUS_ENOIMP, ANSWER_QUALITY_WARNING,
                                 MSG_PARSE_FEATURENOTSUPPORTEDOS_S, "-suspend_remote");
# endif
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-suspend_remote");
             DRETURN(answer);
         }

         DPRINTF("\"-suspend_remote %s\"\n", *sp);

         if (set_yn_option(pcmdline, suspend_remote_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-nostdin" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-nostdin", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, nostdin_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-noshell" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-noshell", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, noshell_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-huh?" */

      if (!strncmp("-", *sp, 1) && strcmp("--", *sp)) {
         answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
              MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp );
         DRETURN(answer);
      }

/*-----------------------------------------------------------------------------*/
      /* - for read script from stdin */

      if (!strcmp("-", *sp)) {
         DPRINTF("\"%s\"\n", *sp);

         if ((flags & FLG_USE_PSEUDOS)) {
            if (!is_qalter) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, nullptr);
               lSetString(ep_opt, SPA_argval_lStringT, *sp);
               for (sp++; *sp; sp++) {
                  ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, nullptr);
                  lSetString(ep_opt, SPA_argval_lStringT, *sp);
               }
            } else {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
               DRETURN(answer);
            }
            continue;
         }
         else {
            ep_opt = sge_add_noarg(pcmdline, 0, *sp, nullptr);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* -- separator for qalter between job ids and job args */

      if (!strcmp("--", *sp)) {
         DPRINTF("\"%s\"\n", *sp);

         if ((flags & FLG_USE_PSEUDOS)) {
            sp++;
            if (!*sp) {
               answer_list_add(&answer, MSG_PARSE_OPTIONMUSTBEFOLLOWEDBYJOBARGUMENTS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }
            for (; *sp; sp++) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, nullptr);
               lSetString(ep_opt, SPA_argval_lStringT, *sp);
            }
            continue;
         }
         else {
            ep_opt = sge_add_noarg(pcmdline, 0, *sp, nullptr);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      DPRINTF("===%s===\n", *sp);

      /*
      ** with qsub and qsh this can only
      ** be a script file, remember this
      ** in case of qalter we need jobids
      */
      if ((flags & FLG_USE_PSEUDOS)) {
         if (!is_qalter) {
            ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, nullptr);
            lSetString(ep_opt, SPA_argval_lStringT, *sp);
            for (sp++; *sp; sp++) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, nullptr);
               lSetString(ep_opt, SPA_argval_lStringT, *sp);
            }
         } else {
            lList *jid_list = nullptr;

            if (!strcmp(*sp, "--")) {
               ep_opt = sge_add_noarg(pcmdline, 0, *sp, nullptr);
               break;
            }
            i_ret = cull_parse_jid_hold_list(&jid_list, *sp);

            if (i_ret) {
               answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGJOBIDLISTFORMATXSPECIFIED_S, *sp );
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, 0, lListT, STR_PSEUDO_JOBID, *sp);
            lSetList(ep_opt, SPA_argval_lListT, jid_list);
            sp++;
         }
         continue;
      } else {
         ep_opt = sge_add_arg(pcmdline, 0, lStringT, "", nullptr);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);
      }

      sp++;
   }

   if (!is_hold_option) {
      if (prog_number == QHOLD) { 
         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, "-h", "u");
         lSetInt(ep_opt, SPA_argval_lIntT, MINUS_H_TGT_USER);

      } else if (prog_number == QRLS) {
         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, "-h", "u");
         lSetInt(ep_opt, SPA_argval_lIntT, MINUS_H_CMD_SUB | MINUS_H_TGT_USER);
      }
   }

   DRETURN(answer);
}

/***************************************************************************/
static int sge_parse_checkpoint_interval(
        const char *time_str
) {
   u_long32 seconds;

   DENTER(TOP_LAYER);

   DPRINTF("--------time_string: %s\n", time_str);
   if (!parse_ulong_val(nullptr, &seconds, TYPE_TIM, time_str, nullptr, 0))
      seconds = 0;

     DPRINTF("-------- seconds: %d\n", (int) seconds);
   DRETURN(seconds);
}

/***************************************************************************/

/****** client/var/var_list_parse_from_environment() **************************
*  NAME
*     var_list_parse_from_environment() -- create var list from env 
*
*  SYNOPSIS
*     static int var_list_parse_from_environment(lList **lpp, char **envp) 
*
*  FUNCTION
*     Create a list of variables from the given environment. 
*
*  INPUTS
*     lList **lpp - VA_Type list 
*     char **envp - environment pointer 
*
*  RESULT
*     static int - error state
*         0 - OK
*        >0 - Error   
*
*  NOTES
*     MT-NOTES: var_list_parse_from_environment() is MT safe
*******************************************************************************/
static int var_list_parse_from_environment(lList **lpp, char **envp) 
{
   DENTER(TOP_LAYER);

   if (!lpp || !envp) {
      DRETURN(1);
   }

   if (!*lpp) {
      *lpp = lCreateList("env list", VA_Type);
      if (!*lpp) {
         DRETURN(3);
      }
   }

   for (; *envp; envp++) {
      char *env_name;
      char *env_description;
      char *env_entry;
      lListElem *ep;
      struct saved_vars_s *context = nullptr;

      ep = lCreateElem(VA_Type);
      lAppendElem(*lpp, ep);

      env_entry = sge_strdup(nullptr, *envp);
      SGE_ASSERT((env_entry));

      env_name = sge_strtok_r(env_entry, "=", &context);
      SGE_ASSERT((env_name));
      lSetString(ep, VA_variable, env_name);

      env_description = sge_strtok_r((char *) 0, "\n", &context);
      if (env_description)
         lSetString(ep, VA_value, env_description);
      sge_free(&env_entry);
      sge_free_saved_vars(context);
   }

   DRETURN(0);
}

/****** set_yn_option() ********************************************************
*  NAME
*     set_yn_option() -- Sets the value of a y|n option
*
*  SYNOPSIS
*     static int set_yn_option (lList **opts, u_long32 opt, char *arg,
*                               char *value, lList **alpp)
*
*  FUNCTION
*     Sets the value of the option element to TRUE or FALSE depending on whether
*     the option value is y[es] or n[o].
*
*  INPUT
*     lList **opts - The list of options to which to append the option element
*     u_long32 opt - The option code of the option
*     char *arg    - The option text
*     char *value  - The option value
*     lList **alpp - The answer list
*
*  RESULT
*     int - STATUS_OK if success, STATUS_ERROR1 otherwise
*
*  NOTES
*     MT-NOTES: set_yn_option() is MT safe
*******************************************************************************/
static int set_yn_option (lList **opts, u_long32 opt, const char *arg, const char *value,
                          lList **alpp)
{
   lListElem *ep_opt = nullptr;
   
   if ((strcasecmp("y", value) == 0) || (strcasecmp("yes", value) == 0)) {
      ep_opt = sge_add_arg(opts, opt, lIntT, arg, value);
      lSetInt(ep_opt, SPA_argval_lIntT, TRUE);
      lSetUlong(ep_opt,SPA_argval_lUlongT, TRUE);
   }
   else if ((strcasecmp ("n", value) == 0) || (strcasecmp ("no", value) == 0)) {
      ep_opt = sge_add_arg(opts, opt, lIntT, arg, value);
      lSetInt(ep_opt, SPA_argval_lIntT, FALSE);
      lSetUlong(ep_opt,SPA_argval_lUlongT, FALSE);
    }
   else {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE,  MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
       answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
       
       return STATUS_ERROR1;
   }
   
   return STATUS_OK;
}

/** @brief Parse the -sync option switch
 *
 * Supports the old and new format of the -sync option switch.
 * Old format: -sync y(es)|n(o)
 * New format: -sync r|y|x|E|n
 *
 *    r - job start
 *    x or y - job end
 *    n - no sync
 *
 * @param opts list of options
 * @param opt option code
 * @param arg option argument
 * @param value option value
 * @param alpp answer list
 *
 * @return STATUS_OK if success, STATUS_ERROR1 otherwise
 */
static int
ocs_parse_sync_switch(lList **opts, u_long32 opt, const char *arg, const char *value, lList **alpp) {
   DENTER(TOP_LAYER);
   u_long32 sync_bits = SYNC_UNINITIALIZED;

   // -sync yes|no (pre OCS format )
   if (strcasecmp("yes", value) == 0) {
      sync_bits = SYNC_JOB_END;
   } else if (strcasecmp ("no", value) == 0) {
      sync_bits = SYNC_NO;
   }

   // -sync r|y|x|E|n (new OCS format)
   if (sync_bits == SYNC_UNINITIALIZED) {
      const int len = strlen(value);
      for (int pos = 0; pos < len; pos++) {
         switch (value[pos]) {
         case 'r':
            sync_bits |= SYNC_JOB_START;
            break;
         case 'x':
         case 'y':
            sync_bits |= SYNC_JOB_END;
            break;
         case 'n':
            sync_bits |= SYNC_NO;
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ERROR1);
         }
      }
   }

   // check if SYNC_NO was combined with other bits which might make no sense
   if ((sync_bits & SYNC_NO) != 0 && sync_bits != SYNC_NO) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_ERROR1);
   }

   // transport bits via opts list
   lListElem *ep_opt = nullptr;
   ep_opt = sge_add_arg(opts, opt, lUlongT, arg, value);
   lSetUlong(ep_opt,SPA_argval_lUlongT, sync_bits);

   DRETURN(STATUS_OK);
}

/* This method is not thread safe.  Fortunately, it is only used by the
 * -cwd switch which can be forbidden in DRMAA. */
char *reroot_path(lListElem* pjob, const char *path, lList **alpp) {
   const char *home = nullptr;
   char tmp_str[SGE_PATH_MAX + 1];
   char tmp_str2[SGE_PATH_MAX + 1];
   char tmp_str3[SGE_PATH_MAX + 1];

   DENTER(TOP_LAYER);

   home = job_get_env_string(pjob, VAR_PREFIX "O_HOME");
   strcpy(tmp_str, path);

   if (chdir(home) == 0) {
      /* If chdir() succeeds... */
      if (getcwd(tmp_str2, sizeof(tmp_str2)) == nullptr) {
         /* If getcwd() fails... */
         answer_list_add(alpp, MSG_ANSWER_GETCWDFAILED,
                         STATUS_EDISK, ANSWER_QUALITY_ERROR);
         DRETURN(nullptr);
      }

      if (chdir(tmp_str) == 0) {
         if (strncmp(tmp_str2, tmp_str, strlen(tmp_str2)) == 0) {
            /* If they are equal, build a new CWD using the value of the HOME
             * as the root instead of whatever that directory is called by
             * the -(c)wd path. */
            snprintf(tmp_str3, sizeof(tmp_str3), "%s%s", home, (char *) tmp_str + strlen(tmp_str2));
            strcpy(tmp_str, tmp_str3);
         }
      }
   }

   DRETURN(strdup(tmp_str));
}

