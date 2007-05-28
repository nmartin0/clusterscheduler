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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "symbols.h"
#include "sge_all_listsL.h"
#include "parse_qsubL.h"
#include "parse_job_cull.h"
#include "sge_mailrec.h"
#include "parse_qsub.h"
#include "sge_feature.h"
#include "sge_userset.h"
#include "sge_parse_num_par.h"
#include "parse.h"
#include "sge_options.h"
#include "sgermon.h"
#include "sge_log.h"
#include "cull_parse_util.h"
#include "sge_string.h"
#include "sge_stdlib.h"
#include "sge_answer.h"
#include "sge_range.h"
#include "sge_ckpt.h"
#include "sge_ulong.h"
#include "sge_str.h"
#include "sge_centry.h"
#include "sge_job.h"
#include "sge_var.h"
#include "sge_answer.h"

#include "msg_common.h"

static int sge_parse_priority(lList **alpp, int *valp, char *priority_str);
static int var_list_parse_from_environment(lList **lpp, char **envp);
static int sge_parse_hold_list(char *hold_str, u_long32 prog_number);
static int sge_parse_mail_options(lList **alpp, char *mail_str, u_long32 prog_number); 
static int cull_parse_destination_identifier_list(lList **lpp, char *dest_str);
static int sge_parse_checkpoint_interval(char *time_str); 
static int set_yn_option (lList **opts, u_long32 opt, char *arg, char *value,
                          lList **alp);



/*
** NAME
**   cull_parse_cmdline
** PARAMETER
**   prog_number         - program number (QSUB, QSH etc.)
**   arg_list            - argument string list, e.g. argv,
**                         knows qsub, qalter and qsh options
**   envp                - pointer to environment
**   pcmdline            - NULL or pointer to list, SPA_Type
**                         set to contain the parsed options
**   flags               - FLG_USE_PSEUDOS: apply the following syntax:
**                         the first non-switch token is the job script, what
**                         follows are job arguments, mark the tokens as "script"
**                         or "jobarg" in the SPA_switch field
**                         0: dont do that, if a non-switch occurs, add it to
**                         the list with SPA_switch "" (an argument to a non-existing
**                         switch)
**                         FLG_QALTER: change treatment of non-options to qalter-specific
**                         pseudo options (only if FLG_USE_PSEUDOS is given) and changes
**                         parsing of -h
**
** RETURN
**   answer list, AN_Type or NULL if everything ok, the following stati can occur:
**   STATUS_EUNKNOWN   - bad internal error like NULL pointer received or no memory
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
char **arg_list,
char **envp,
lList **pcmdline,
u_long32 flags 
) {
   char **sp;
   lList *answer = NULL;
   char str[1024 + 1];
   lListElem *ep_opt;
   int i_ret;
   u_long32 is_qalter = flags & FLG_QALTER;
   bool is_hold_option = false;
   int hard_soft_flag = 0;

   DENTER(TOP_LAYER, "cull_parse_cmdline");

   if (!arg_list || !pcmdline) {
      answer_list_add(&answer, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   sp = arg_list;

   /* reset hard/soft flag */
   hard_soft_flag = 0;
   while (*sp) {

/*----------------------------------------------------------------------------*/
      /* "-a date_time */

      if (!strcmp("-a", *sp)) {
         u_long32 timeval;

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         if (!ulong_parse_date_time_from_string(&timeval, NULL, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTOAOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, a_OPT, lUlongT, *(sp - 1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, timeval);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-A account_string" */

      if (!strcmp("-A", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-A %s\"\n", *sp));

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
            lList *ar_id_list = NULL;

            sp++;
            if (!*sp) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, 
                                       "-ar");                
               DRETURN(answer);
            }

            DPRINTF(("\"-ar %s\"\n", *sp));

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

            if (!parse_ulong_val(&ar_id_d, NULL, TYPE_INT, *sp, NULL, 0)) {
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
         lList *variable_list = NULL;
         lListElem* lep = NULL;

         /* next field is context_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_ACOPTIONMUSTHAVECONTEXTLISTLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF(("\"-ac %s\"\n", *sp));
         i_ret = var_list_parse_from_string(&variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                     MSG_ANSWER_WRONGCONTEXTLISTFORMATAC_S , *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, ac_OPT, lListT, *(sp - 1), *sp);
         lep = lCreateElem(VA_Type);
         lSetString(lep, VA_variable, "+");
         lInsertElem(variable_list, NULL, lep);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-b y|n" */

      if (!is_qalter && !strcmp("-b", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-b %s\"\n", *sp));

         if (set_yn_option(pcmdline, b_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }


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

         DPRINTF(("\"%s %s\"\n", *(sp - 1), *sp));

         attr = sge_parse_checkpoint_attr(*sp);
         if (attr == 0) {
            interval = sge_parse_checkpoint_interval(*sp);
            if (!interval) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                       MSG_PARSE_CARGUMENTINVALID);
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, c_OPT, lLongT, *(sp - 1), *sp);
            lSetLong(ep_opt, SPA_argval_lLongT, (long) interval);
         }
         else if (attr == -1) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_CSPECIFIERINVALID);
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

         DPRINTF(("\"%s %s\"\n", *(sp - 1), *sp));

         ep_opt = sge_add_arg(pcmdline, 0, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-clear" */

      if (!strcmp("-clear", *sp)) {

         DPRINTF(("\"%s\"\n", *sp));
         ep_opt = sge_add_noarg(pcmdline, clear_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-explain" */

      if (!strcmp("-explain", *sp)) {

         DPRINTF(("\"%s\"\n", *sp));
         ep_opt = sge_add_noarg(pcmdline, explain_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-xml" */

      if (!strcmp("-xml", *sp)) {

         DPRINTF(("\"%s\"\n", *sp));
         ep_opt = sge_add_noarg(pcmdline, xml_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-cwd" is mapped as -wd with NULL path, this case is handled in the second parsing stage*/

      if (!strcmp("-cwd", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, wd_OPT, "-wd", NULL);
         DPRINTF(("\"%s\"\n", *sp));
         
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-C directive_prefix" */
      if (!strcmp("-C", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-C %s\"\n", *sp));

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
         double timeval;
         char tmp[1000];

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-d %s\"\n", *sp));

         if (!parse_ulong_val(&timeval, NULL, TYPE_TIM, *sp, tmp, sizeof(tmp)-1)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTODOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, d_OPT, lUlongT, *(sp-1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, timeval);

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-dc simple_context_list */
      if (!strcmp("-dc", *sp)) {
         lList *variable_list = NULL;
         lListElem* lep = NULL;

         /* next field is simple_context_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_DCOPTIONMUSTHAVESIMPLECONTEXTLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF(("\"-dc %s\"\n", *sp));
         i_ret = var_list_parse_from_string(&variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                     MSG_PARSE_WRONGCONTEXTLISTFORMATDC_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, dc_OPT, lListT, *(sp - 1), *sp);
         lep = lCreateElem(VA_Type);
         lSetString(lep, VA_variable, "-");
         lInsertElem(variable_list, NULL, lep);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }


      
/*----------------------------------------------------------------------------*/
      /* "-display -- only for qsh " */

      if (!strcmp("-display", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is display to redirect interactive jobs to */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_DISPLAYOPTIONMUSTHAVEARGUMENT);
             DRETURN(answer);
         }

         DPRINTF(("\"-display %s\"\n", *sp));

         ep_opt = sge_add_arg(pcmdline, display_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;

      }

/*----------------------------------------------------------------------------*/
      /* "-dl date_time */

      if (!strcmp("-dl", *sp)) {
         u_long32 timeval;

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         if (!ulong_parse_date_time_from_string(&timeval, NULL, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_WRONGTIMEFORMATXSPECTODLOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, dl_OPT, lUlongT, *(sp - 1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, timeval);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-e path_name" */

      if (!strcmp("-e", *sp)) {
         lList *path_list = NULL;

         if (prog_number == QRSUB) {
            if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-e %s\"\n", *sp));

         if (prog_number == QRSUB) {
            u_long32 timeval;
            if (!ulong_parse_date_time_from_string(&timeval, NULL, *sp)) {
               answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTOAOPTION_S, *sp);
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, e_OPT, lUlongT, *(sp-1), *sp);
            lSetUlong(ep_opt, SPA_argval_lUlongT, timeval);
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
         lList *path_list = NULL;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-i");
             DRETURN(answer);
         }

         DPRINTF(("\"-i %s\"\n", *sp));

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
            char *cmd_switch;
            char *cmd_arg = "";
         is_hold_option = true;
         cmd_switch = *sp;

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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
         DPRINTF(("\"-h %s\"\n", *sp));

         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, cmd_switch, cmd_arg);
         lSetInt(ep_opt, SPA_argval_lIntT, hold);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-hard" */

      if (!strcmp("-hard", *sp)) {

         DPRINTF(("\"%s\"\n", *sp));

         hard_soft_flag = 1;
         ep_opt = sge_add_noarg(pcmdline, hard_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
     /*  -he y[es]|n[o] */

      if(!strcmp("-he", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
            sprintf(str,
               MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S,
               *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }
         /* next field is yes/no switch */
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-he");
            DRETURN(answer);
         }
         
         DPRINTF(("\"-he %s\"\n", *sp));
         
         if (set_yn_option(pcmdline, he_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-help" */

      if (!strcmp("-help", *sp)) {
         DPRINTF(("\"%s\"\n", *sp));

         ep_opt = sge_add_noarg(pcmdline, help_OPT, *sp, NULL);

         sp++;
         continue;

      }

/*-----------------------------------------------------------------------------*/
      /* "-hold_jid jid[,jid,...]" */

      if (!strcmp("-hold_jid", *sp)) {
         lList *jid_hold_list = NULL;

         /* next field is hold_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-hold_jid");
             DRETURN(answer);
         }

         DPRINTF(("\"-hold_jid %s\"\n", *sp));
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
      /* "-j y|n" */

      if (!strcmp("-j", *sp)) {

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-j %s\"\n", *sp));

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

         if (!parse_ulong_val(&jobshare_d, NULL, TYPE_INT, *sp, NULL, 0)) {
            answer_list_add(&answer, MSG_PARSE_INVALIDJOBSHAREMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DEXIT;
            return answer;
         }


         if (jobshare_d < 0) {
            answer_list_add(&answer, MSG_PARSE_INVALIDJOBSHAREMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DEXIT;
            return answer;
         }

         jobshare = jobshare_d;
         
         ep_opt = sge_add_arg(pcmdline, js_OPT, lUlongT, *(sp - 1), *sp);
         lSetUlong(ep_opt, SPA_argval_lUlongT, jobshare);

         sp++;
         continue;
      }

/*---------------------------------------------------------------------------*/
      /* "-l resource_list" */

      if (!strcmp("-l", *sp)) {
         lList *resource_list = NULL;

         /* next field is resource_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-l" );
             DRETURN(answer);
         }

         DPRINTF(("\"-l %s\"\n", *sp));

         resource_list = centry_list_parse_from_string(NULL, *sp, false);
         if (!resource_list) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_WRONGRESOURCELISTFORMATXSPECTOLOPTION_S, *sp);
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, l_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, resource_list);
         lSetInt(ep_opt, SPA_argval_lIntT, hard_soft_flag);

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

         DPRINTF(("\"-m %s\"\n", *sp));
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
         lList *id_list = NULL;

         /* next field is destination_identifier */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_QOPTIONMUSTHAVEDESTIDLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF(("\"-masterq %s\"\n", *sp));
         i_ret = cull_parse_destination_identifier_list(&id_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOQOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, masterq_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, id_list);
         lSetInt(ep_opt, SPA_argval_lIntT, 0);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-M mail_list" */

      if (!strcmp("-M", *sp)) {
         lList *mail_list = NULL;

         /* next field is mail_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-M" );
             DRETURN(answer);
         }

         DPRINTF(("\"-M %s\"\n", *sp));

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

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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
                 MSG_PARSE_ARGUMENTTONOPTIONMUSTNOTCONTAINBSL );
             DRETURN(answer);
         }

         if (*sp == '\0') {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_EMPTYSTRINGARGUMENTTONOPTIONINVALID );
             DRETURN(answer);
         }

         DPRINTF(("\"-N %s\"\n", *sp));
         ep_opt = sge_add_arg(pcmdline, N_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-notify" */

      if (!strcmp("-notify", *sp)) {

         DPRINTF(("\"-notify\"\n"));

         ep_opt = sge_add_noarg(pcmdline, notify_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
     /*  -now y[es]|n[o] */

      if(!strcmp("-now", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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
         
         DPRINTF(("\"-now %s\"\n", *sp));
         
         if (set_yn_option(pcmdline, now_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
            
/*----------------------------------------------------------------------------*/
      /* "-o path_name" */

      if (!strcmp("-o", *sp)) {
         lList *stdout_path_list = NULL;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-o" );
             DRETURN(answer);
         }

         DPRINTF(("\"-o %s\"\n", *sp));
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

         if (!parse_ulong_val(&otickets_d, NULL, TYPE_INT, *sp, NULL, 0)) {
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

         if (sge_parse_priority(&answer, &priority, *sp)) {
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

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
            sprintf(str, MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S,
               *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }

         /* next field is the projects name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-P");
             DRETURN(answer);
         }

         DPRINTF(("\"-P %s\"\n", *sp));
         ep_opt = sge_add_arg(pcmdline, P_OPT, lStringT, *(sp - 1), *sp);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      if (!strcmp("-pe", *sp)) {
         lList *pe_range = NULL;
         dstring d_arg = DSTRING_INIT;

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }
DTRACE;
         /* next field must be the pe_name ... */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_PEOPTIONMUSTHAVEPENAMEARGUMENT );
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
         lList *id_list = NULL;

         /* next field is destination_identifier */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_QOPTIONMUSTHAVEDESTIDLISTARGUMENT, 
                     STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF(("\"-q %s\"\n", *sp));
         i_ret = cull_parse_destination_identifier_list(&id_list, *sp);
         if (i_ret) {
             answer_list_add_sprintf(&answer,STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOQOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, q_OPT, lListT, *(sp - 1), *sp);
         lSetList(ep_opt, SPA_argval_lListT, id_list);
         lSetInt(ep_opt, SPA_argval_lIntT, hard_soft_flag);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-r y|n" */

      if (!strcmp("-r", *sp)) {


         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-r %s\"\n", *sp));

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

         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-R %s\"\n", *sp));

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
         lList *variable_list = NULL;
         lListElem* lep = NULL;

         /* next field is context_list  [ == variable_list ] */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_SCOPTIONMUSTHAVECONTEXTLISTARGUMENT, 
                     STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF(("\"-sc %s\"\n", *sp));
         i_ret = var_list_parse_from_string(&variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                          MSG_PARSE_WRONGCONTEXTLISTFORMATSCX_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, sc_OPT, lListT, *(sp - 1), *sp);
         lep = lCreateElem(VA_Type);
         lSetString(lep, VA_variable, "=");
         lInsertElem(variable_list, NULL, lep);
         lSetList(ep_opt, SPA_argval_lListT, variable_list);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-soft" */

      if (!strcmp("-soft", *sp)) {

         DPRINTF(("\"%s\"\n", *sp));
         hard_soft_flag = 2;
         ep_opt = sge_add_noarg(pcmdline, soft_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-shell y|n" */

      if (!is_qalter && !strcmp("-shell", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
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

         DPRINTF(("\"-shell %s\"\n", *sp));

         if (set_yn_option (pcmdline, shell_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
     /*  -sync y[es]|n[o] */

      if(!strcmp("-sync", *sp)) {
         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }
         /* next field is yes/no switch */
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-sync" );
            DRETURN(answer);
         }
         
         DPRINTF(("\"-sync %s\"\n", *sp));
         
         if (set_yn_option (pcmdline, sync_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
            
/*----------------------------------------------------------------------------*/
      /* "-S path_name" */

      if (!strcmp("-S", *sp)) {
         lList *shell_list = NULL;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_SOPTIONMUSTHAVEPATHNAMEARGUMENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF(("\"-S %s\"\n", *sp));

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
         lList *task_id_range_list = NULL;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_TOPTIONMUSTHAVEALISTOFTASKIDRANGES, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF(("\"-t %s\"\n", *sp));

         range_list_parse_from_string(&task_id_range_list, &answer, *sp,
                                      false, true, INF_NOT_ALLOWED);
         if (!task_id_range_list) {
            DRETURN(answer);
         }

         range_list_sort_uniq_compress(task_id_range_list, &answer);
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
      /* "-terse" */

      if(!strcmp("-terse", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, terse_OPT, *sp, NULL);
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-u" */
      if (!strcmp("-u", *sp)) {
         lList *user_list = NULL;

         /* next field is user_list */
         sp++;
         if (!*sp) {
            answer_list_add(&answer, MSG_PARSE_UOPTMUSTHAVEALISTUSERNAMES,
                  STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
 
         DPRINTF(("\"-u %s\"\n", *sp));

         if (prog_number == QRSUB) {
            int rule[] = {ARA_name, 0};
            char **dest = NULL;
            char *tmp;

            tmp = sge_strdup(NULL, *sp);
            dest = string_list(tmp, ",", NULL);
            cull_parse_string_list(dest, "user_list", ARA_Type, rule, &user_list);
            FREE(tmp);
            FREE(dest);
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
         ep_opt = sge_add_noarg(pcmdline, u_OPT, *sp, NULL);

         DPRINTF(("\"-uall \"\n"));
         sp++;
  
         continue;
      }           

/*-----------------------------------------------------------------------------*/
      /* "-v variable_list" */

      if (!strcmp("-v", *sp)) {
         lList *variable_list = NULL;

         /* next field is variable_list */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_VOPTMUSTHAVEVARIABLELISTARGUMENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF(("\"-v %s\"\n", *sp));

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

         DPRINTF(("\"%s\"\n", *sp));
         ep_opt = sge_add_noarg(pcmdline, verify_OPT, *sp, NULL);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-V" */

      if (!strcmp("-V", *sp)) {
         lList *env_list = NULL;

         DPRINTF(("\"%s\"\n", *sp));
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


         if (lGetElemStr(*pcmdline, SPA_switch, *sp)) {
            answer_list_add_sprintf(&answer,STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "e|w|n|v" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-w");
             DRETURN(answer);
         }

         DPRINTF(("\"-w %s\"\n", *sp));

         if (!strcmp("e", *sp)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            if (prog_number == QRSUB) {
               lSetInt(ep_opt, SPA_argval_lIntT, AR_ERROR_VERIFY);
            } else {
               lSetInt(ep_opt, SPA_argval_lIntT, ERROR_VERIFY);
            }
         }
         else if (!strcmp("w", *sp)) {
            if (prog_number == QRSUB) {
               answer_list_add_sprintf(&answer,STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, *sp);
               DRETURN(answer);
            } else {
               ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            }
            lSetInt(ep_opt, SPA_argval_lIntT, WARNING_VERIFY);
         }
         else if (!strcmp("n", *sp)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            if (prog_number == QRSUB) {
               answer_list_add_sprintf(&answer,STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, *sp);
               DRETURN(answer);
            } else {
               lSetInt(ep_opt, SPA_argval_lIntT, SKIP_VERIFY);
            }
         }
         else if (!strcmp("v", *sp)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            if (prog_number == QRSUB) {
               lSetInt(ep_opt, SPA_argval_lIntT, AR_JUST_VERIFY);
            } else {
               lSetInt(ep_opt, SPA_argval_lIntT, JUST_VERIFY);
            }
         } else {
             answer_list_add_sprintf(&answer,STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_INVALIDOPTIONARGUMENTWX_S, *sp);
             DRETURN(answer);
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
         lListElem *aep;
         int do_exit = 0;

         sp++;
         if (!*sp) {
             sprintf(str, MSG_PARSE_ATSIGNOPTIONMUSTHAVEFILEARGUMENT);
             answer_list_add(&answer, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DEXIT;
             return answer;
         }

         DPRINTF(("\"-@ %s\"\n", *sp));

         alp = parse_script_file(prog_number, *sp, "", pcmdline, envp, FLG_USE_NO_PSEUDOS); /* MT-NOTE: !!!! */
         for_each(aep, alp) {
            u_long32 quality;

            quality = lGetUlong(aep, AN_quality);
            if (quality == ANSWER_QUALITY_ERROR) {
               do_exit = 1;
            }
            lAppendElem(answer, lCopyElem(aep));
         }

         lFreeList(&alp);
         if (do_exit) {
            DEXIT;
            return answer;
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-verbose" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-verbose", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, verbose_OPT, *sp, NULL);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-inherit" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-inherit", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, verbose_OPT, *sp, NULL);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-nostdin" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-nostdin", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, nostdin_OPT, *sp, NULL);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-noshell" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-noshell", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, noshell_OPT, *sp, NULL);
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
         DPRINTF(("\"%s\"\n", *sp));

         if ((flags & FLG_USE_PSEUDOS)) {
            if (!is_qalter) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, NULL);
               lSetString(ep_opt, SPA_argval_lStringT, *sp);
               for (sp++; *sp; sp++) {
                  ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, NULL);
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
            ep_opt = sge_add_noarg(pcmdline, 0, *sp, NULL);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* -- separator for qalter between job ids and job args */

      if (!strcmp("--", *sp)) {
         DPRINTF(("\"%s\"\n", *sp));

         if ((flags & FLG_USE_PSEUDOS)) {
            sp++;
            if (!*sp) {
               answer_list_add(&answer, MSG_PARSE_OPTIONMUSTBEFOLLOWEDBYJOBARGUMENTS, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }
            for (; *sp; sp++) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, NULL);
               lSetString(ep_opt, SPA_argval_lStringT, *sp);
            }
            continue;
         }
         else {
            ep_opt = sge_add_noarg(pcmdline, 0, *sp, NULL);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      DPRINTF(("===%s===\n", *sp));

      /*
      ** with qsub and qsh this can only
      ** be a script file, remember this
      ** in case of qalter we need jobids
      */
      if ((flags & FLG_USE_PSEUDOS)) {
         if (!is_qalter) {
            ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, NULL);
            lSetString(ep_opt, SPA_argval_lStringT, *sp);
            for (sp++; *sp; sp++) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, NULL);
               lSetString(ep_opt, SPA_argval_lStringT, *sp);
            }
         }
         else {
            lList *jid_list = NULL;

            if (!strcmp(*sp, "--")) {
               ep_opt = sge_add_noarg(pcmdline, 0, *sp, NULL);
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
      }
      else {
         ep_opt = sge_add_arg(pcmdline, 0, lStringT, "", NULL);
         lSetString(ep_opt, SPA_argval_lStringT, *sp);
      }

      sp++;
   }

   if (!is_hold_option) {
      if (prog_number == QHOLD) { 
         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, "-h", "u");
         lSetInt(ep_opt, SPA_argval_lIntT, MINUS_H_TGT_USER);

      }
      else if (prog_number == QRLS) {
         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, "-h", "u");
         lSetInt(ep_opt, SPA_argval_lIntT, MINUS_H_CMD_SUB | MINUS_H_TGT_USER);
      }
   }
   DEXIT;
   return answer;
}


/***********************************************************************/
/* "-h [hold_list]" */
/* MT-NOTE: sge_parse_hold_list() is MT safe */
static int sge_parse_hold_list(
char *hold_str,
u_long32 prog_number
) {
   int i, j;
   int target = 0;
   int op_code = 0;

   DENTER(TOP_LAYER, "sge_parse_hold_list");

   i = strlen(hold_str);

   for (j = 0; j < i; j++) {
      switch (hold_str[j]) {
      case 'n':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = MINUS_H_TGT_USER|MINUS_H_TGT_OPERATOR|MINUS_H_TGT_SYSTEM;
         break;
      case 's':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_SYSTEM;         
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_SYSTEM;
         }   
         break;
      case 'o':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_OPERATOR;         
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_OPERATOR;
         }
         break;
         
      case 'u':
         if (prog_number == QRLS) {
            if (op_code && op_code != MINUS_H_CMD_SUB) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_SUB;
            target = target|MINUS_H_TGT_USER;
         }
         else {
            if (op_code && op_code != MINUS_H_CMD_ADD) {
               target = -1;
               break;
            }
            op_code = MINUS_H_CMD_ADD;
            target = target|MINUS_H_TGT_USER;
         }
         break;
      case 'S':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_SYSTEM;
         break;
      case 'U':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_USER;
         break;
      case 'O':
         if ((prog_number == QHOLD)  || 
             (prog_number == QRLS) || 
             (op_code && op_code != MINUS_H_CMD_SUB)) {
            target = -1;
            break;
         }
         op_code = MINUS_H_CMD_SUB;
         target = target|MINUS_H_TGT_OPERATOR;
         break;
      default:
         target = -1;
      }

      if (target == -1)
         break;
   }

   if (target != -1)
      target |= op_code;

   DEXIT;
   return target;
}

/***********************************************************************/
/* MT-NOTE: sge_parse_mail_options() is MT safe */
static int sge_parse_mail_options(lList **alpp, char *mail_str, u_long32 prog_number) 
{
   int i, j;
   int mail_opt = 0;

   DENTER(TOP_LAYER, "sge_parse_mail_options");

   i = strlen(mail_str);

   for (j = 0; j < i; j++) {
      if ((char) mail_str[j] == 'a') {
         mail_opt = mail_opt | MAIL_AT_ABORT;
      } else if ((char) mail_str[j] == 'b') {
         mail_opt = mail_opt | MAIL_AT_BEGINNING;
      } else if ((char) mail_str[j] == 'e') {
         mail_opt = mail_opt | MAIL_AT_EXIT;
      } else if ((char) mail_str[j] == 'n') {
         mail_opt = mail_opt | NO_MAIL;
      } else if ((char) mail_str[j] == 's') {
         if (prog_number == QRSUB) {
            answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                   MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-m");
            DRETURN(0);
         }
         mail_opt = mail_opt | MAIL_AT_SUSPENSION;
      } else {
         DRETURN(0);
      }
   }

   DRETURN(mail_opt);

}

/***************************************************************************/
static int sge_parse_checkpoint_interval(
char *time_str 
) {
   u_long32 seconds;

   DENTER(TOP_LAYER, "sge_parse_checkpoint_interval");

   DPRINTF(("--------time_string: %s\n", time_str));
   if (!parse_ulong_val(NULL, &seconds, TYPE_TIM, time_str, NULL, 0))
      seconds = 0;

     DPRINTF(("-------- seconds: %d\n", (int) seconds));
   DEXIT;
   return seconds;
}

/***************************************************************************/


/****** parse_qsub/cull_parse_path_list() **************************************
*  NAME
*     cull_parse_path_list() -- ??? 
*
*  SYNOPSIS
*     int cull_parse_path_list(lList **lpp, char *path_str) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lList **lpp    - ??? 
*     char *path_str - ??? 
*
*  RESULT
*     int - error code 
*        0 = okay
*        1 = error 
*
*  NOTES
*     MT-NOTE: cull_parse_path_list() is MT safe
*******************************************************************************/
int cull_parse_path_list(
lList **lpp,
char *path_str 

/*
   [[host]:]path[,[[host]:]path...]

   str0 - path
   str1 - host
   str3 - temporary
   int  - temporary
 */

) {
   char *path = NULL;
   char *cell = NULL;
   char **str_str = NULL;
   char **pstr = NULL;
   lListElem *ep = NULL;
   char *path_string = NULL;
   bool ret_error = false;

   DENTER(TOP_LAYER, "cull_parse_path_list");

   ret_error = (lpp == NULL) ? true : false;
/*
   if (!lpp) {
      DEXIT;
      return 1;
   }
*/

   if(!ret_error){
      path_string = sge_strdup(NULL, path_str);
      ret_error = (path_string == NULL) ? true : false;
   }
/*
   if (!path_string) {
      *lpp = NULL;
      DEXIT;
      return 2;
   }
*/
   if(!ret_error){
      str_str = string_list(path_string, ",", NULL);
      ret_error = (str_str == NULL || *str_str == NULL) ? true : false;
   }
/*
   if (!str_str || !*str_str) {
      *lpp = NULL;
      FREE(path_string);
      DEXIT;
      return 3;
   }
*/
   if ( (!ret_error) && (!*lpp)) {
      *lpp = lCreateList("path_list", PN_Type);
      ret_error = (*lpp == NULL) ? true : false;
/*
      if (!*lpp) {
         FREE(path_string);
         FREE(str_str);
         DEXIT;
         return 4;
      }
*/
   }

   if(!ret_error){
      for (pstr = str_str; *pstr; pstr++) {
      /* cell given ? */
         if (*pstr[0] == ':') {  /* :path */
            cell = NULL;
            path = *pstr+1;
         } else if ((path = strstr(*pstr, ":"))){ /* host:path */
            path[0] = '\0';
            cell = strdup(*pstr);
            path[0] = ':';
            path += 1;
         } else { /* path */
            cell = NULL;
            path = *pstr;
         }

         SGE_ASSERT((path));
         ep = lCreateElem(PN_Type);
         /* SGE_ASSERT(ep); */
         lAppendElem(*lpp, ep);

         lSetString(ep, PN_path, path);
        if (cell) {
            lSetHost(ep, PN_host, cell);
            FREE(cell);
         }
      }
   }
   if(path_string)
      FREE(path_string);
   if(str_str)
      FREE(str_str);
   DEXIT;
   if(ret_error)
      return 1;
   else
      return 0;
}


/***************************************************************************/
/* MT-NOTE: sge_parse_priority() is MT safe */
static int sge_parse_priority(
lList **alpp,
int *valp,
char *priority_str 
) {
   char *s;

   DENTER(TOP_LAYER, "sge_parse_priority");

   *valp = strtol(priority_str, &s, 10);
   if (priority_str==s || *valp > 1024 || *valp < -1023) {
       SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_PARSE_INVALIDPRIORITYMUSTBEINNEG1023TO1024));
       answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
       DEXIT;
       return -1;
   }
   DEXIT;
   return 0;
}

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
   DENTER(TOP_LAYER, "var_list_parse_from_environment");

   if (!lpp || !envp) {
      DEXIT;
      return 1;
   }

   if (!*lpp) {
      *lpp = lCreateList("env list", VA_Type);
      if (!*lpp) {
         DEXIT;
         return 3;
      }
   }

   for (; *envp; envp++) {
      char *env_name;
      char *env_description;
      char *env_entry;
      lListElem *ep;
      struct saved_vars_s *context;

      ep = lCreateElem(VA_Type);
      lAppendElem(*lpp, ep);

      env_entry = sge_strdup(NULL, *envp);
      SGE_ASSERT((env_entry));

      context = NULL;
      env_name = sge_strtok_r(env_entry, "=", &context);
      SGE_ASSERT((env_name));
      lSetString(ep, VA_variable, env_name);

      env_description = sge_strtok_r((char *) 0, "\n", &context);
      if (env_description)
         lSetString(ep, VA_value, env_description);
      FREE(env_entry);
      sge_free_saved_vars(context);

   }

   DEXIT;
   return 0;
}

/***************************************************************************/
/* MT-NOTE: cull_parse_destination_identifier_list() is MT safe */
static int cull_parse_destination_identifier_list(
lList **lpp, /* QR_Type */
char *dest_str 
) {
   int rule[] = {QR_name, 0};
   char **str_str = NULL;
   int i_ret;
   char *s;

   DENTER(TOP_LAYER, "cull_parse_destination_identifier_list");

   if (!lpp) {
      DEXIT;
      return 1;
   }

   s = sge_strdup(NULL, dest_str);
   if (!s) {
      *lpp = NULL;
      DEXIT;
      return 3;
   }
   str_str = string_list(s, ",", NULL);
   if (!str_str || !*str_str) {
      *lpp = NULL;
      FREE(s);
      DEXIT;
      return 2;
   }

   i_ret = cull_parse_string_list(str_str, "destin_ident_list", QR_Type, rule, lpp);
   if (i_ret) {
      FREE(s);
      FREE(str_str);
      DEXIT;
      return 3;
   }

   FREE(s);
   FREE(str_str);
   DEXIT;
   return 0;
}

/***************************************************************************/
/*   MT-NOTE: cull_parse_jid_hold_list() is MT safe */
int cull_parse_jid_hold_list(
lList **lpp,
char *str 

/*   jid[,jid,...]

   int0 - jid

 */

) {
   int rule[] = {ST_name, 0};
   char **str_str = NULL;
   int i_ret;
   char *s;

   DENTER(TOP_LAYER, "cull_parse_jid_hold_list");

   if (!lpp) {
      DEXIT;
      return 1;
   }

   s = sge_strdup(NULL, str);
   if (!s) {
      *lpp = NULL;
      DEXIT;
      return 3;
   }
   str_str = string_list(s, ",", NULL);
   if (!str_str || !*str_str) {
      *lpp = NULL;
      FREE(s);
      DEXIT;
      return 2;
   }
   i_ret = cull_parse_string_list(str_str, "jid_hold list", ST_Type, rule, lpp);
   
   if (i_ret) {
      FREE(s);
      FREE(str_str);
      DEXIT;
      return 3;
   }

   FREE(s);
   FREE(str_str);
   DEXIT;
   return 0;
}

/****** client/var/var_list_parse_from_string() *******************************
*  NAME
*     var_list_parse_from_string() -- parse vars from string list 
*
*  SYNOPSIS
*     int var_list_parse_from_string(lList **lpp, 
*                                    const char *variable_str, 
*                                    int check_environment) 
*
*  FUNCTION
*     Parse a list of variables ("lpp") from a comma separated 
*     string list ("variable_str"). The boolean "check_environment"
*     defined wether the current value of a variable is taken from
*     the environment of the calling process.
*
*  INPUTS
*     lList **lpp              - VA_Type list 
*     const char *variable_str - source string 
*     int check_environment    - boolean
*
*  RESULT
*     int - error state
*         0 - OK
*        >0 - Error
*
*  NOTES
*     MT-NOTE: var_list_parse_from_string() is MT safe
*******************************************************************************/
int var_list_parse_from_string(lList **lpp, const char *variable_str,
                               int check_environment)
{
   char *variable;
   char *val_str;
   int var_len;
   char **str_str;
   char **pstr;
   lListElem *ep;
   char *va_string;

   DENTER(TOP_LAYER, "var_list_parse_from_string");

   if (!lpp) {
      DEXIT;
      return 1;
   }

   va_string = sge_strdup(NULL, variable_str);
   if (!va_string) {
      *lpp = NULL;
      DEXIT;
      return 2;
   }
   str_str = string_list(va_string, ",", NULL);
   if (!str_str || !*str_str) {
      *lpp = NULL;
      FREE(va_string);
      DEXIT;
      return 3;
   }

   if (!*lpp) {
      *lpp = lCreateList("variable list", VA_Type);
      if (!*lpp) {
         FREE(va_string);
         FREE(str_str);
         DEXIT;
         return 4;
      }
   }

   for (pstr = str_str; *pstr; pstr++) {
      struct saved_vars_s *context;
      ep = lCreateElem(VA_Type);
      /* SGE_ASSERT(ep); */
      lAppendElem(*lpp, ep);
      
      context = NULL;
      variable = sge_strtok_r(*pstr, "=", &context);
      SGE_ASSERT((variable));
      var_len=strlen(variable);
      lSetString(ep, VA_variable, variable);
      val_str=*pstr;

      /* The character at the end of the first token must be either '=' or '\0'.
       * If it's a '=' then we treat the following string as the value */
      if (val_str[var_len] == '=') {
          lSetString(ep, VA_value, &val_str[var_len+1]);
      }
      /* If it's a '\0' and check_environment is set, then we get the value from
       * the environment variable value. */
      else if(check_environment) {
         lSetString(ep, VA_value, sge_getenv(variable));
      }
      /* If it's a '\0' and check_environment is not set, then we set the value
       * to NULL. */
      else {
         lSetString(ep, VA_value, NULL);
      }
      sge_free_saved_vars(context);
   }

   FREE(va_string);
   FREE(str_str);
   DEXIT;
   return 0;
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
static int set_yn_option (lList **opts, u_long32 opt, char *arg, char *value,
                          lList **alpp)
{
   lListElem *ep_opt = NULL;
   
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
       sprintf(SGE_EVENT, MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
       answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
       
       return STATUS_ERROR1;
   }
   
   return STATUS_OK;
}

/* This method is not thread safe.  Fortunately, it is only used by the
 * -cwd switch which can be forbidden in DRMAA. */
char *reroot_path(lListElem* pjob, const char *path, lList **alpp) {
   const char *home = NULL;
   char tmp_str[SGE_PATH_MAX + 1];
   char tmp_str2[SGE_PATH_MAX + 1];
   char tmp_str3[SGE_PATH_MAX + 1];
   
   DENTER (TOP_LAYER, "reroot_path");
   
   home = job_get_env_string(pjob, VAR_PREFIX "O_HOME");
   strcpy (tmp_str, path);
   
   if (!chdir(home)) {
      /* If chdir() succeeds... */
      if (!getcwd(tmp_str2, sizeof(tmp_str2))) {
         /* If getcwd() fails... */
         answer_list_add(alpp, MSG_ANSWER_GETCWDFAILED, 
                         STATUS_EDISK, ANSWER_QUALITY_ERROR);
         DRETURN(NULL);
      }

      chdir(tmp_str);

      if (strncmp(tmp_str2, tmp_str, strlen(tmp_str2)) == 0) {
         /* If they are equal, build a new CWD using the value of the HOME
          * as the root instead of whatever that directory is called by
          * the -(c)wd path. */
         sprintf(tmp_str3, "%s%s", home, (char *) tmp_str + strlen(tmp_str2));
         strcpy(tmp_str, tmp_str3);
      }
   }
   
   DRETURN(strdup(tmp_str));
}

