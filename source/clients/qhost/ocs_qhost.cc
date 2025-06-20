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
 *  Portions of this code are Copyright 2011 Univa Corporation.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <math.h>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"
#include "uti/sge_io.h"
#include "uti/sge_string.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cull_xml.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "basis_types.h"
#include "sig_handlers.h"
#include "ocs_qhost_print.h"
#include "ocs_client_parse.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qhost.h"

extern char **environ;

static bool sge_parse_cmdline_qhost(char **argv, char **envp, lList **ppcmdline, lList **alpp);
static int sge_parse_qhost(lList **ppcmdline, lList **pplres, lList **ppFres, lList **pphost, lList **ppuser, u_long32 *show, qhost_report_handler_t **report_handler, lList **alpp);
static bool qhost_usage(FILE *fp);

static qhost_report_handler_t* xml_report_handler_create(lList **alpp);
static int xml_report_handler_destroy(qhost_report_handler_t** handler, lList **alpp);
static int xml_report_finished(qhost_report_handler_t* handler, lList **alpp);
static int xml_report_started(qhost_report_handler_t* handler, lList **alpp);
static int xml_report_host_begin(qhost_report_handler_t* handler, const char* host_name, lList **alpp);
static int xml_report_host_string_value(qhost_report_handler_t* handler, const char* name, const char *value, lList **alpp);
static int xml_report_host_ulong_value(qhost_report_handler_t* handler, const char* name, u_long32 value, lList **alpp);
static int xml_report_host_finished(qhost_report_handler_t* handler, const char* host_name, lList **alpp);
static int xml_report_resource_value(qhost_report_handler_t* handler, const char* dominance, const char* name, const char* value, lList **alpp);
static int xml_report_queue_begin(qhost_report_handler_t* handler, const char* qname, lList **alpp);
static int xml_report_queue_string_value(qhost_report_handler_t* handler, const char* qname, const char* name, const char *value, lList **alpp);
static int xml_report_queue_ulong_value(qhost_report_handler_t* handler, const char* qname, const char* name, u_long32 value, lList **alpp);
static int xml_report_queue_finished(qhost_report_handler_t* handler, const char* qname, lList **alpp);
static int xml_report_job_begin(qhost_report_handler_t* handler, const char *qname, const char* jobname, lList **alpp);
static int xml_report_job_string_value(qhost_report_handler_t* handler, const char *qname, const char* jobname, const char* name, const char *value, lList **alpp);
static int xml_report_job_ulong64_value(qhost_report_handler_t* handler, const char *qname, const char* jobname, const char* name, u_long64 value, lList **alpp);
static int xml_report_job_double_value(qhost_report_handler_t* handler, const char *qname, const char* jobname, const char* name, double value, lList **alpp);
static int xml_report_job_finished(qhost_report_handler_t* handler, const char *qname, const char* jobname, lList **alpp);




static int xml_report_started(qhost_report_handler_t* handler, lList **alpp)
{
   DENTER(TOP_LAYER);

   printf("<?xml version='1.0'?>\n");
   printf("<qhost xmlns:xsd=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qhost/qhost.xsd\">\n");
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_finished(qhost_report_handler_t* handler, lList **alpp)
{
   DENTER(TOP_LAYER);
   
   printf("</qhost>\n");
   
   DRETURN(QHOST_SUCCESS);
}

static qhost_report_handler_t* xml_report_handler_create(lList **alpp)
{
   qhost_report_handler_t* ret = (qhost_report_handler_t*)sge_malloc(sizeof(qhost_report_handler_t));

   DENTER(TOP_LAYER);

   if (ret == nullptr ) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      DRETURN(nullptr);
   }
   /*
   ** for xml_report_handler ctx is a dstring
   */
   ret->ctx = sge_malloc(sizeof(dstring));
   if (ret->ctx == nullptr ) {
      answer_list_add_sprintf(alpp, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      DRETURN(nullptr);
   }
   /*
   ** corresponds to initializing with DSTRING_INIT
   */
   memset(ret->ctx, 0, sizeof(dstring));
   
   ret->report_started = xml_report_started;
   ret->report_finished = xml_report_finished;
   ret->report_host_begin = xml_report_host_begin;
   ret->report_host_string_value = xml_report_host_string_value;
   ret->report_host_ulong_value = xml_report_host_ulong_value;
   ret->report_host_finished = xml_report_host_finished;
   
   ret->report_resource_value = xml_report_resource_value;

   ret->report_queue_begin = xml_report_queue_begin;
   ret->report_queue_string_value = xml_report_queue_string_value;
   ret->report_queue_ulong_value = xml_report_queue_ulong_value;
   ret->report_queue_finished = xml_report_queue_finished;
   
   ret->report_job_begin = xml_report_job_begin;
   ret->report_job_string_value = xml_report_job_string_value;
   ret->report_job_ulong64_value = xml_report_job_ulong64_value;
   ret->report_job_double_value = xml_report_job_double_value;
   ret->report_job_finished = xml_report_job_finished;

   ret->destroy = xml_report_handler_destroy;

   DRETURN(ret);
}

static int xml_report_handler_destroy(qhost_report_handler_t** handler, lList **alpp)
{
   DENTER(TOP_LAYER);

   if (handler != nullptr && *handler != nullptr ) {
      sge_dstring_free((dstring*)(*handler)->ctx);
      sge_free(&((*handler)->ctx));
      sge_free(handler);
      *handler = nullptr;
   }

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_host_begin(qhost_report_handler_t* handler, const char* host_name, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(host_name, (dstring*)handler->ctx);
   printf(" <host name='%s'>\n", sge_dstring_get_string((dstring*)handler->ctx));

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_host_string_value(qhost_report_handler_t* handler, const char*name, const char *value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);
   printf("   <hostvalue name='%s'>", sge_dstring_get_string((dstring*)handler->ctx) );
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(value, (dstring*)handler->ctx);
   printf("%s</hostvalue>\n", sge_dstring_get_string((dstring*)handler->ctx));

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_host_ulong_value(qhost_report_handler_t* handler, const char* name, u_long32 value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);
   printf("   <hostvalue name='%s'>" sge_u32 "</hostvalue>\n", sge_dstring_get_string((dstring*)handler->ctx), value);

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_host_finished(qhost_report_handler_t* handler, const char* host_name, lList **alpp)
{
   DENTER(TOP_LAYER);

   printf(" </host>\n");   

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_resource_value(qhost_report_handler_t* handler, const char* dominance, const char* name, const char* value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);
   printf("   <resourcevalue name='%s' ", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(dominance, (dstring*)handler->ctx);   
   printf("dominance='%s'>", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(value, (dstring*)handler->ctx);   
   printf("%s</resourcevalue>\n", sge_dstring_get_string((dstring*)handler->ctx));
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_queue_begin(qhost_report_handler_t* handler, const char* qname, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(qname, (dstring*)handler->ctx);
   printf(" <queue name='%s'>\n", sge_dstring_get_string((dstring*)handler->ctx));

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_queue_string_value(qhost_report_handler_t* handler, const char* qname, const char* name, const char *value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(qname, (dstring*)handler->ctx);
   printf("   <queuevalue qname='%s'", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);      
   printf(" name='%s'>", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(value, (dstring*)handler->ctx);      
   printf("%s</queuevalue>\n", sge_dstring_get_string((dstring*)handler->ctx));
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_queue_ulong_value(qhost_report_handler_t* handler, const char* qname, const char* name, u_long32 value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(qname, (dstring*)handler->ctx);
   printf("   <queuevalue qname='%s'", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);      
   printf(" name='%s'>" sge_u32 "</queuevalue>\n", sge_dstring_get_string((dstring*)handler->ctx), value);
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_queue_finished(qhost_report_handler_t* handler, const char* qname, lList **alpp)
{
   DENTER(TOP_LAYER);

   printf(" </queue>\n");   

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_job_begin(qhost_report_handler_t* handler, const char *qname, const char* jobname, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(jobname, (dstring*)handler->ctx);
   printf(" <job name='%s'>\n", sge_dstring_get_string((dstring*)handler->ctx));

   DRETURN(QHOST_SUCCESS);
}

static int xml_report_job_string_value(qhost_report_handler_t* handler, const char *qname, const char* jobname, const char* name, const char *value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(jobname, (dstring*)handler->ctx);
   printf("   <jobvalue jobid='%s'", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);      
   printf(" name='%s'>", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(value, (dstring*)handler->ctx);      
   printf("%s</jobvalue>\n", sge_dstring_get_string((dstring*)handler->ctx));
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_job_ulong64_value(qhost_report_handler_t* handler, const char *qname, const char* jobname, const char* name, u_long64 value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(jobname, (dstring*)handler->ctx);
   printf("   <jobvalue jobid='%s'", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);      
   printf(" name='%s'>" sge_u64 "</jobvalue>\n", sge_dstring_get_string((dstring*)handler->ctx), value);
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_job_double_value(qhost_report_handler_t* handler, const char *qname, const char* jobname, const char* name, double value, lList **alpp)
{
   DENTER(TOP_LAYER);

   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(jobname, (dstring*)handler->ctx);
   printf("   <jobvalue jobid='%s'", sge_dstring_get_string((dstring*)handler->ctx));
   
   sge_dstring_clear((dstring*)handler->ctx);
   escape_string(name, (dstring*)handler->ctx);      
   printf(" name='%s'>'%f'</jobvalue>\n", sge_dstring_get_string((dstring*)handler->ctx), value);
   
   DRETURN(QHOST_SUCCESS);
}

static int xml_report_job_finished(qhost_report_handler_t* handler, const char *qname, const char* job_name, lList **alpp)
{
   DENTER(TOP_LAYER);

   printf(" </job>\n");   

   DRETURN(QHOST_SUCCESS);
}

bool
switch_list_qhost_parse_from_file(lList **switch_list, lList **answer_list, const char *file) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (switch_list == nullptr) {
      ret = false;
   } else {
      if (!sge_is_file(file)) {
         // it is ok if the file does not exist
         ret = true;
         DTRACE;
      } else {
         int file_as_string_length;
         char *file_as_string = sge_file2string(file, &file_as_string_length);
         if (file_as_string == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_ERRORREADINGFROMFILEX_S, file);
            ret = false;
            DTRACE;
         } else {
            char **token = stra_from_str(file_as_string, " \n\t");
            ret = sge_parse_cmdline_qhost(token, environ, switch_list, answer_list);
            sge_strafree(&token);
            DTRACE;
         }
         sge_free(&file_as_string);
      }
   }
   DRETURN(ret);
}

/************************************************************************/
int main(int argc, char **argv)
{
   DENTER_MAIN(TOP_LAYER, "qhost");
   lList *pcmdline = nullptr;
   lList *ul = nullptr;
   lList *host_list = nullptr;
   u_long32 show = 0;
   lList *resource_list = nullptr;
   lList *resource_match_list = nullptr;
   lList *alp = nullptr;
   qhost_report_handler_t *report_handler = nullptr;
   int is_ok = 0;
   int qhost_result = 0;


   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(QHOST);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QHOST, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   // read parameter from a default qhost files
   dstring file = DSTRING_INIT;
   const char *cell_root = bootstrap_get_cell_root();
   const char *username = component_get_username();
   lList *args_file = nullptr;
   get_root_file_path(&file, cell_root, SGE_COMMON_DEF_QHOST_FILE);
   switch_list_qhost_parse_from_file(&args_file, &alp, sge_dstring_get_string(&file));
   if (get_user_home_file_path(&file, SGE_HOME_DEF_QHOST_FILE, username, &alp)) {
      switch_list_qhost_parse_from_file(&args_file, &alp, sge_dstring_get_string(&file));
   }
   sge_dstring_free(&file);

   // parse the command line
   if (!sge_parse_cmdline_qhost(++argv, environ, &pcmdline, &alp)) {
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      sge_prof_cleanup();
      sge_exit(1);
   }

   // find duplicates in file and commandline and remove them
   const lListElem *ep_1;
   lListElem *ep_2;
   for_each_ep(ep_1, pcmdline) {
      bool more;
      do {
         more = false;
         for_each_rw(ep_2, args_file) {
            if (strcmp(lGetString(ep_1, SPA_switch_val), lGetString(ep_2, SPA_switch_val)) == 0) {
               lRemoveElem(args_file, &ep_2);
               more = true;
               break;
            }
         }
      } while(more);
   }

   // merge the command line and the file options
   if (lGetNumberOfElem(pcmdline) > 0) {
      lAppendList(pcmdline, args_file);
      lFreeList(&args_file);
   } else if (lGetNumberOfElem(args_file) > 0) {
      lAppendList(args_file, pcmdline);
      lFreeList(&pcmdline);
      pcmdline = args_file;
   }
   sge_dstring_free(&file);

   // parse the commandline
   is_ok = sge_parse_qhost(&pcmdline, &resource_match_list, &resource_list, &host_list,
                           &ul, &show, &report_handler, &alp);
   lFreeList(&pcmdline);
   if (is_ok == 0) {     
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   } else if (is_ok == 2) {
      /* -help output generated, exit normally */ 
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(0);
   }

   qhost_result = do_qhost(host_list, ul, resource_match_list, resource_list,
                           show, &alp, report_handler);

   if (report_handler != nullptr) {
      report_handler->destroy(&report_handler, &alp);
   }
   
   if (qhost_result != QHOST_SUCCESS) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   sge_prof_cleanup();
   sge_exit(0); /* 0 means ok - others are errors */
   DRETURN(0);
}


/*
** NAME
**   qhost_usage
** PARAMETER
**   none
** RETURN
**   none
** EXTERNAL
**   none
** DESCRIPTION
**   displays qhost_usage for qlist client
**   note that the other clients use a common function
**   for this. output was adapted to a similar look.
*/
static bool qhost_usage(
FILE *fp 
) {
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER);

   if (fp == nullptr) {
      DRETURN(false);
   }

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

   fprintf(fp,"%s qhost [options]\n", MSG_SRC_USAGE);
         
   fprintf(fp, "  [-F [resource_attribute]]  %s\n", MSG_QHOST_F_OPT_USAGE); 
   fprintf(fp, "  [-h hostlist]              %s\n", MSG_QHOST_h_OPT_USAGE);
   fprintf(fp, "  [-help]                    %s\n", MSG_COMMON_help_OPT_USAGE);
   fprintf(fp, "  [-j]                       %s\n", MSG_QHOST_j_OPT_USAGE);
   fprintf(fp, "  [-l attr=val,...]          %s\n", MSG_QHOST_l_OPT_USAGE);
   fprintf(fp, "  [-ncb]                     %s\n", MSG_QHOST_ncb_OPT_USAGE);
   fprintf(fp, "  [-q]                       %s\n", MSG_QHOST_q_OPT_USAGE);
#ifdef WITH_EXTENSIONS
   fprintf(fp, "  [-sdv]                     %s\n", MSG_QHOST_SHOW_DEPT_VIEW);
#endif
   fprintf(fp, "  [-u user[,user,...]]       %s\n", MSG_QHOST_u_OPT_USAGE);
   fprintf(fp, "  [-xml]                     %s\n", MSG_COMMON_xml_OPT_USAGE);

   DRETURN(true);
}

/****
 **** sge_parse_cmdline_qhost (static)
 ****
 **** 'stage 1' parsing of qhost-options. Parses options
 **** with their arguments and stores them in ppcmdline.
 ****/ 
static bool sge_parse_cmdline_qhost(
char **argv,
char **envp,
lList **ppcmdline,
lList **alpp
) {
   char **sp;
   char **rp;
   DENTER(TOP_LAYER);

   rp = argv;

   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, alpp)) != sp)
         continue;
 
      /* -ncb */
      if ((rp = parse_noopt(sp, "-ncb", nullptr, ppcmdline, alpp)) != sp)
         continue;

#ifdef WITH_EXTENSIONS
      /* -sdv */
      if ((rp = parse_noopt(sp, "-sdv", nullptr, ppcmdline, alpp)) != sp)
         continue;
#endif

      /* -q */
      if ((rp = parse_noopt(sp, "-q", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -F */
      if ((rp = parse_until_next_opt2(sp, "-F", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -h */
      if ((rp = parse_until_next_opt(sp, "-h", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -j */
      if ((rp = parse_noopt(sp, "-j", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -l */
      if ((rp = parse_until_next_opt(sp, "-l", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -u */
      if ((rp = parse_until_next_opt(sp, "-u", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -xml */
      if ((rp = parse_noopt(sp, "-xml", nullptr, ppcmdline, alpp)) != sp)
         continue;
      
      /* oops */
      qhost_usage(stderr);
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      DRETURN(false);
   }
   DRETURN(true);
}

/****
 **** sge_parse_qhost (static)
 ****
 **** 'stage 2' parsing of qhost-options. Gets the options from pcmdline
 ****/
static int sge_parse_qhost(lList **ppcmdline,
                            lList **pplres,
                            lList **ppFres,
                            lList **pphost,
                            lList **ppuser,
                            u_long32 *show,
                            qhost_report_handler_t **report_handler,
                            lList **alpp) 
{
   u_long32 helpflag;
   bool usageshowed = false;
   u_long32 full = 0;
   u_long32 binding = 0;
   char * argstr = nullptr;
   lListElem *ep;
   int ret = 1;

   DENTER(TOP_LAYER);
 
   /* Loop over all options. Only valid options can be in the
      ppcmdline list. 
   */

   /* display topology related information per default */
   (*show) |= QHOST_DISPLAY_BINDING;

   while (lGetNumberOfElem(*ppcmdline))
   {
      if (parse_flag(ppcmdline, "-help",  alpp, &helpflag)) {
         usageshowed = true;
         qhost_usage(stdout);
         ret = 2;
         goto exit;   
      }

      if (parse_multi_stringlist(ppcmdline, "-h", alpp, pphost, ST_Type, ST_name)) {
         /* 
         ** resolve hostnames and replace them in list
         */
         for_each_rw(ep, *pphost) {
            if (sge_resolve_host(ep, ST_name) != CL_RETVAL_OK) {
               char buf[BUFSIZ];
               snprintf(buf, sizeof(buf), MSG_SGETEXT_CANTRESOLVEHOST_S, lGetString(ep,ST_name) );
               answer_list_add(alpp, buf, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               goto error;
            }
         }
         continue;
      }

      if (parse_multi_stringlist(ppcmdline, "-F", alpp, ppFres, ST_Type, ST_name)) {
         (*show) |= QHOST_DISPLAY_RESOURCES;
         continue;
      }
      if (parse_flag(ppcmdline, "-ncb", alpp, &binding)) {
         /* disable topology related information */
         (*show) ^= QHOST_DISPLAY_BINDING;
         continue;
      }
#ifdef WITH_EXTENSIONS
      if (parse_flag(ppcmdline, "-sdv", alpp, &binding)) {
         // show department view
         (*show) |= QHOST_DISPLAY_DEPT_VIEW;
         continue;
      }
#endif
      if (parse_flag(ppcmdline, "-q", alpp, &full)) {
         if(full) {
            (*show) |= QHOST_DISPLAY_QUEUES;
            full = 0;
         }
         continue;
      }

      if (parse_flag(ppcmdline, "-j", alpp, &full)) {
         if(full) {
            (*show) |= QHOST_DISPLAY_JOBS;
            full = 0;
         }
         continue;
      }
      while (parse_string(ppcmdline, "-l", alpp, &argstr)) {
         *pplres = centry_list_parse_from_string(*pplres, argstr, false);
         sge_free(&argstr);
         continue;
      }
      
      if (parse_multi_stringlist(ppcmdline, "-u", alpp, ppuser, ST_Type, ST_name)) {
         (*show) |= QHOST_DISPLAY_JOBS;
         continue;
      }

      if (parse_flag(ppcmdline, "-xml", alpp, &full)) {
         *report_handler = xml_report_handler_create(alpp);
         continue;
      }

   }
   if (lGetNumberOfElem(*ppcmdline)) {
     answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_PARSE_TOOMANYOPTIONS);
     goto error;
   }

   DRETURN(1);

   error:
      ret = 0;
   exit:
      if (!usageshowed) {
         qhost_usage(stderr);
      }
      if (report_handler && *report_handler) {
         (*report_handler)->destroy(report_handler, alpp);
      }
      lFreeList(pplres);
      lFreeList(ppFres);
      lFreeList(pphost);
      lFreeList(ppuser);
   
      DRETURN(ret);
}

