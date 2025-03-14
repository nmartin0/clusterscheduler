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

#include <cstring>

#include "uti/sge_edit.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "gdi/ocs_gdi_Client.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_answer.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "ocs_qconf_rqs.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qconf.h"

static bool
rqs_provide_modify_context(lList **rqs_list, lList **answer_list, bool ignore_unchanged_message);

/****** resource_quota_qconf/rqs_show() *********************************
*  NAME
*     rqs_show() -- show resource quota sets
*
*  SYNOPSIS
*     bool rqs_show(lList **answer_list, const char *name) 
*
*  FUNCTION
*     This funtion gets the selected resource quota sets from GDI and
*     writes they out on stdout
*
*  INPUTS
*     lList **answer_list - answer list
*     const char *name    - comma separated list of resource quota set names
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_show() is MT safe 
*
*******************************************************************************/
bool
rqs_show(lList **answer_list, const char *name)
{
   lList *rqs_list = nullptr;
   bool ret = false;

   DENTER(TOP_LAYER);

   if (name != nullptr) {
      lList *rqsref_list = nullptr;

      lString2List(name, &rqsref_list, RQS_Type, RQS_name, ", ");
      ret = rqs_get_via_gdi(answer_list, rqsref_list, &rqs_list);
      lFreeList(&rqsref_list);
   } else {
      ret = rqs_get_all_via_gdi(answer_list, &rqs_list);
   }

   if (ret && lGetNumberOfElem(rqs_list)) {
      const char* filename;
      filename = spool_flatfile_write_list(answer_list, rqs_list, RQS_fields, 
                                        &qconf_rqs_sfi,
                                        SP_DEST_STDOUT, SP_FORM_ASCII, nullptr,
                                        false);
      sge_free(&filename);
   }
   if (lGetNumberOfElem(rqs_list) == 0) {
      answer_list_add(answer_list, MSG_NORQSFOUND, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      ret = false;
   }

   lFreeList(&rqs_list);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_get_via_gdi() **************************
*  NAME
*     rqs_get_via_gdi() -- get resource quota sets from GDI
*
*  SYNOPSIS
*     bool rqs_get_via_gdi(lList **answer_list, const lList 
*     *rqsref_list, lList **rqs_list) 
*
*  FUNCTION
*     This function gets the selected resource quota sets from qmaster. The selection
*     is done in the string list rqsref_list.
*
*  INPUTS
*     lList **answer_list       - answer list
*     const lList *rqsref_list - resource quota sets selection
*     lList **rqs_list         - copy of the selected rule sets
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_get_via_gdi() is MT safe 
*
*******************************************************************************/
bool
rqs_get_via_gdi(lList **answer_list, const lList *rqsref_list, lList **rqs_list)
{
   bool ret = false;

   DENTER(TOP_LAYER);
   if (rqsref_list != nullptr) {
      const lListElem *rqsref = nullptr;
      lCondition *where = nullptr;
      lEnumeration *what = nullptr;

      what = lWhat("%T(ALL)", RQS_Type);

      for_each_ep(rqsref, rqsref_list) {
         lCondition *add_where = nullptr;
         add_where = lWhere("%T(%I p= %s)", RQS_Type, RQS_name, lGetString(rqsref, RQS_name));
         if (where == nullptr) {
            where = add_where;
         } else {
            where = lOrWhere(where, add_where);
         }
      }
      *answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_RQS_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, rqs_list, where, what);
      if (!answer_list_has_error(answer_list)) {
         ret = true;
      }

      lFreeWhat(&what);
      lFreeWhere(&where);
   }

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_get_all_via_gdi() **********************
*  NAME
*     rqs_get_all_via_gdi() -- get all resource quota sets from GDI 
*
*  SYNOPSIS
*     bool rqs_get_all_via_gdi(lList **answer_list, lList **rqs_list) 
*
*  FUNCTION
*     This function gets all resource quota sets known by qmaster
*
*  INPUTS
*     lList **answer_list - answer list
*     lList **rqs_list   - copy of all resource quota sets
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_get_all_via_gdi() is MT safe 
*
*******************************************************************************/
bool
rqs_get_all_via_gdi(lList **answer_list, lList **rqs_list)
{
   bool ret = false;
   lEnumeration *what = lWhat("%T(ALL)", RQS_Type);

   DENTER(TOP_LAYER);

   *answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_RQS_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, rqs_list, nullptr, what);
   if (!answer_list_has_error(answer_list)) {
      ret = true;
   }

   lFreeWhat(&what);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_add() ******************************
*  NAME
*     rqs_add() -- add resource quota set list
*
*  SYNOPSIS
*     bool rqs_add(lList **answer_list, const char *name) 
*
*  FUNCTION
*     This function provide a modify context for qconf to add new resource
*     quota sets. If no name is given a template rule set is shown
*
*  INPUTS
*     lList **answer_list - answer list
*     const char *name    - comma seperated list of rule sets to add
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_add() is MT safe 
*
*******************************************************************************/
bool
rqs_add(lList **answer_list, const char *name)
{
   bool ret = false;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lList *rqs_list = nullptr;
      lListElem *rqs = nullptr;

      lString2List(name, &rqs_list, RQS_Type, RQS_name, ", ");
      for_each_rw (rqs, rqs_list) {
         rqs = rqs_set_defaults(rqs);
      }

      ret = rqs_provide_modify_context(&rqs_list, answer_list, true);

      if (ret) {
         ret = rqs_add_del_mod_via_gdi(rqs_list, answer_list, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SET_ALL);
      }

      lFreeList(&rqs_list);
   }

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_modify() ***************************
*  NAME
*     rqs_modify() -- modify resource quota sets
*
*  SYNOPSIS
*     bool rqs_modify(lList **answer_list, const char *name) 
*
*  FUNCTION
*     This function provides a modify context for qconf to modify resource
*     quota sets.
*
*  INPUTS
*     lList **answer_list - answer list
*     const char *name    - comma seperated list of rule sets to modify
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_modify() is MT safe 
*
*******************************************************************************/
bool
rqs_modify(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);
   bool ret = false;
   lList *rqs_list = nullptr;
   ocs::gdi::Command::Cmd cmd = ocs::gdi::Command::SGE_GDI_NONE;
   ocs::gdi::SubCommand::SubCmd sub_cmd = ocs::gdi::SubCommand::SGE_GDI_SUB_NONE;

   if (name != nullptr) {
      lList *rqsref_list = nullptr;

      cmd = ocs::gdi::Command::SGE_GDI_MOD;
      sub_cmd = ocs::gdi::SubCommand::SGE_GDI_SET_ALL;

      lString2List(name, &rqsref_list, RQS_Type, RQS_name, ", ");
      ret = rqs_get_via_gdi(answer_list, rqsref_list, &rqs_list);
      lFreeList(&rqsref_list);
   } else {
      cmd = ocs::gdi::Command::SGE_GDI_REPLACE;
      ret = rqs_get_all_via_gdi(answer_list, &rqs_list);
   }

   if (ret) {
      ret = rqs_provide_modify_context(&rqs_list, answer_list, false);
   }
   if (ret) {
      ret = rqs_add_del_mod_via_gdi(rqs_list, answer_list, cmd, sub_cmd);
   }

   lFreeList(&rqs_list);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_add_from_file() ********************
*  NAME
*     rqs_add_from_file() -- add resource quota set from file
*
*  SYNOPSIS
*     bool rqs_add_from_file(lList **answer_list, const char 
*     *filename) 
*
*  FUNCTION
*     This function add new resource quota sets from file.
*
*  INPUTS
*     lList **answer_list  - answer list
*     const char *filename - filename of new resource quota sets
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_add_from_file() is MT safe 
*
*******************************************************************************/
bool
rqs_add_from_file(lList **answer_list, const char *filename) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (filename != nullptr) {
      lList *rqs_list = nullptr;

      /* fields_out field does not work for rqs because of duplicate entry */
      rqs_list = spool_flatfile_read_list(answer_list, RQS_Type, RQS_fields, nullptr, true, &qconf_rqs_sfi, SP_FORM_ASCII, nullptr, filename);
      if (!answer_list_has_error(answer_list)) {
         ret = rqs_add_del_mod_via_gdi(rqs_list, answer_list, ocs::gdi::Command::SGE_GDI_ADD, ocs::gdi::SubCommand::SGE_GDI_SET_ALL);
      }

      lFreeList(&rqs_list);
   }
   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_provide_modify_context() ***********
*  NAME
*     rqs_provide_modify_context() -- provide qconf modify context
*
*  SYNOPSIS
*     bool rqs_provide_modify_context(lList **rqs_list, lList 
*     **answer_list, bool ignore_unchanged_message) 
*
*  FUNCTION
*     This function provides a editor session to edit the selected resource quota
*     sets interactively. 
*
*  INPUTS
*     lList **rqs_list             - resource quota sets to modify
*     lList **answer_list           - answer list
*     bool ignore_unchanged_message - ignore unchanged message
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_provide_modify_context() is MT safe 
*
*******************************************************************************/
static bool
rqs_provide_modify_context(lList **rqs_list, lList **answer_list, bool ignore_unchanged_message)
{
   bool ret = false;
   int status = 0;
   const char *filename = nullptr;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   
   DENTER(TOP_LAYER);

   if (rqs_list == nullptr) {
      answer_list_add(answer_list, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      DRETURN(ret); 
   }

   if (*rqs_list == nullptr) {
      *rqs_list = lCreateList("", RQS_Type);
   }

   filename = spool_flatfile_write_list(answer_list, *rqs_list, RQS_fields, &qconf_rqs_sfi, SP_DEST_TMP, SP_FORM_ASCII, filename, false);

   if (answer_list_has_error(answer_list)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      DRETURN(ret);
   }

   status = sge_edit(filename, uid, gid);

   if (status == 0) {
      lList *new_rqs_list = nullptr;

      /* fields_out field does not work for rqs because of duplicate entry */
      new_rqs_list = spool_flatfile_read_list(answer_list, RQS_Type, RQS_fields,
                                               nullptr, true, &qconf_rqs_sfi,
                                               SP_FORM_ASCII, nullptr, filename);
      if (answer_list_has_error(answer_list)) {
         lFreeList(&new_rqs_list);
      }
      if (new_rqs_list != nullptr) {
         if (ignore_unchanged_message || object_list_has_differences(new_rqs_list, answer_list, *rqs_list)) {
            lFreeList(rqs_list);
            *rqs_list = new_rqs_list;
            ret = true;
         } else {
            lFreeList(&new_rqs_list);
            answer_list_add(answer_list, MSG_FILE_NOTCHANGED,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
   } else if (status == 1) {
      answer_list_add(answer_list, MSG_FILE_FILEUNCHANGED,
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   } else {
      answer_list_add(answer_list, MSG_PARSE_EDITFAILED,
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }

   unlink(filename);
   sge_free(&filename);
   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_add_del_mod_via_gdi() **************
*  NAME
*    rqs_add_del_mod_via_gdi/rqs_add_del_mod_via_gdi() -- modfies qmaster resource quota sets
*
*  SYNOPSIS
*     bool rqs_add_del_mod_via_gdi(lList *rqs_list, lList 
*     **answer_list, u_long32 gdi_command) 
*
*  FUNCTION
*     This function modifies via GDI the qmaster copy of the resource quota sets.
*
*  INPUTS
*     lList *rqs_list     - resource quota sets to modify on qmaster
*     lList **answer_list  - answer list from qmaster
*     u_long32 gdi_command - commands what to do
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_add_del_mod_via_gdi() is MT safe 
*
*******************************************************************************/
bool
rqs_add_del_mod_via_gdi(lList *rqs_list, lList **answer_list, ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_cmd)
{
   bool ret = false;
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   
   DENTER(TOP_LAYER);

   if (rqs_list != nullptr) {
      bool do_verify = (cmd == ocs::gdi::Command::SGE_GDI_MOD) ||
                       (cmd == ocs::gdi::Command::SGE_GDI_ADD ||
                       (cmd == ocs::gdi::Command::SGE_GDI_REPLACE)) ? true : false;

      if (do_verify) {
         ret = rqs_list_verify_attributes(rqs_list, answer_list, false, master_centry_list);
      }
      if (ret) {
         lList *my_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_RQS_LIST, cmd, sub_cmd, &rqs_list, nullptr, nullptr);
         if (my_answer_list != nullptr) {
            answer_list_append_list(answer_list, &my_answer_list);
         }
      }
   }
   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_modify_from_file() *****************
*  NAME
*     rqs_modify_from_file() -- modifies resource quota sets from file
*
*  SYNOPSIS
*     bool rqs_modify_from_file(lList **answer_list, const char 
*     *filename, const char* name) 
*
*  FUNCTION
*     This function allows to modify one or all resource quota sets from a file
*
*  INPUTS
*     lList **answer_list  - answer list
*     const char *filename - filename with the resource quota sets to change
*     const char* name     - comma separated list of rule sets to change
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_modify_from_file() is MT safe 
*
*******************************************************************************/
bool
rqs_modify_from_file(lList **answer_list, const char *filename, const char* name)
{
   bool ret = false;
   ocs::gdi::Command::Cmd cmd = ocs::gdi::Command::SGE_GDI_NONE;
   ocs::gdi::SubCommand::SubCmd sub_cmd = ocs::gdi::SubCommand::SGE_GDI_SUB_NONE;

   DENTER(TOP_LAYER);
   if (filename != nullptr) {
      lList *rqs_list = nullptr;

      /* fields_out field does not work for rqs because of duplicate entry */
      rqs_list = spool_flatfile_read_list(answer_list, RQS_Type, RQS_fields,
                                          nullptr, true, &qconf_rqs_sfi,
                                          SP_FORM_ASCII, nullptr, filename);
      if (rqs_list != nullptr) {

         if (name != nullptr && strlen(name) > 0 ) {
            lList *selected_rqs_list = nullptr;
            const lListElem *tmp_rqs = nullptr;
            lList *found_rqs_list = lCreateList("rqs_list", RQS_Type);

            cmd = ocs::gdi::Command::SGE_GDI_MOD;
            sub_cmd = ocs::gdi::SubCommand::SGE_GDI_SET_ALL;

            lString2List(name, &selected_rqs_list, RQS_Type, RQS_name, ", ");
            for_each_ep(tmp_rqs, selected_rqs_list) {
               lListElem *found = rqs_list_locate(rqs_list, lGetString(tmp_rqs, RQS_name));
               if (found != nullptr) {
                  lAppendElem(found_rqs_list, lCopyElem(found));
               } else {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_RQS_NOTFOUNDINFILE_SS, lGetString(tmp_rqs, RQS_name), filename);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
                  lFreeList(&found_rqs_list);
                  break;
               }
            }
            lFreeList(&selected_rqs_list);
            lFreeList(&rqs_list);
            rqs_list = found_rqs_list;
         } else {
            cmd = ocs::gdi::Command::SGE_GDI_REPLACE;
         }

         if (rqs_list != nullptr) {
            ret = rqs_add_del_mod_via_gdi(rqs_list, answer_list, cmd, sub_cmd);
         }
      }
      lFreeList(&rqs_list);
   }
   DRETURN(ret);
}
