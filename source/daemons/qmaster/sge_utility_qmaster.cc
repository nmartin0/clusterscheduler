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
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <cctype>

#include "uti/config_file.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_hostname.h"

#include "cull/cull.h"

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_SubCommand.h"

#include "comm/lists/cl_errors.h"
#include "comm/cl_commlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"

#include "msg_common.h"
#include "msg_qmaster.h"

#define CQUEUE_LAYER TOP_LAYER

/****** sge_utility_qmaster/attr_mod_procedure() *******************************
*  NAME
*     attr_mod_procedure() -- modify the prolog/epilog/pe_start/pe_stop procedures
*
*  SYNOPSIS
*     int attr_mod_procedure(lList **alpp, lListElem *qep, lListElem *new_ep, 
*     int nm, char *attr_name, char *variables[]) 
*
*  FUNCTION
*     This function modifies a prolog/epilog/pe_start/pe_stop of "new_qep".
*     The attribute of "qep" element is identified by nm.
*     Possible errors will be reported in "alpp". "qep"
*     "qep" element will be used to identify the changes which have been done.
*     "attr_name" is used to report errors
*
*  INPUTS
*     lList **alpp      - AN_Type, The answer list 
*     lListElem *qep    - CQ_Type, reduced changes source element 
*     lListElem *new_ep - CQ_Type, target element 
*     int nm            - CULL attribute name (CQ_Type)
*     char *attr_name   - CULL sublist attribute name of that
*                              field which containes the value of
*                              the attribute to be modified. 
*     char *variables[] - procedure variables 
*
*  RESULT
*     int - 0 success, error othewise
*
*  NOTES
*     MT-NOTE: attr_mod_procedure() is MT safe 
*
*******************************************************************************/
int
attr_mod_procedure(lList **alpp, lListElem *qep, lListElem *new_ep, int nm, const char *attr_name, const char *variables[]) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      const char *s;
      DPRINTF("got new %s\n", attr_name);

      s = lGetString(qep, nm);
      if (s) {
         char *t;
         char *script = (char *) s;

         /* skip user name */
         if ((t = strpbrk(script, "@ ")) && *t == '@') {
            script = &t[1];
         }

         /* ensure that variables are valid */
         if (replace_params(script, nullptr, 0, variables)) {
            ERROR(MSG_GDI_VARS_SS, attr_name, err_msg);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         // force use of absolut path's
         // we might also see a $ sign from a special variable, whose validity has been checked above
         if (script[0] != '/' && script[0] != '$') {
            ERROR(MSG_GDI_APATH_S, attr_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }
      lSetString(new_ep, nm, s);
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_zerostr() *********************************
*  NAME
*     attr_mod_zerostr() -- modify strings, no verification 
*
*  SYNOPSIS
*     int attr_mod_zerostr(lListElem *qep, lListElem *new_ep, int nm, char 
*     *attr_name) 
*
*  FUNCTION
*      This function modifies "new_qep" attribute with string from "qep 
*      without any verification. nullptr is a valid value.
*
*  INPUTS
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name 
*
*  RESULT
*     int - 0, success, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_zerostr() is MT safe 
*
*******************************************************************************/
int
attr_mod_zerostr(lListElem *qep, lListElem *new_ep, int nm, const char *attr_name) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      DPRINTF("got new %s\n", attr_name);
      lSetString(new_ep, nm, lGetString(qep, nm));
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_str() *************************************
*  NAME
*     attr_mod_str() -- modify strings except that it may not be nullptr
*
*  SYNOPSIS
*     int attr_mod_str(lList **alpp, lListElem *qep, lListElem *new_ep, int nm, 
*     char *attr_name) 
*
*  FUNCTION
*      This function modifies "new_qep" attribute with string from "qep 
*      except that the value of an attribute may not be nullptr.
*
*  INPUTS
*     lList **alpp      - AN_Type, The answer list 
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name 
*
*  RESULT
*     int -  0 success, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_str() is MT safe 
*     
*******************************************************************************/
int
attr_mod_str(lList **alpp, lListElem *qep, lListElem *new_ep, int nm, const char *attr_name) {
   int dataType;
   int pos;

   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if ((pos = lGetPosViaElem(qep, nm, SGE_NO_ABORT)) >= 0) {
      const char *s;

      DPRINTF("got new %s\n", attr_name);

      dataType = lGetPosType(lGetElemDescr(qep), pos);
      switch (dataType) {
         case lStringT:
            if (!(s = lGetString(qep, nm))) {
               ERROR(MSG_GDI_VALUE_S, lNm2Str(nm));
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EUNKNOWN);
            }
            lSetString(new_ep, nm, s);
            break;
         case lHostT:
            if (!(s = lGetHost(qep, nm))) {
               ERROR(MSG_GDI_VALUE_S, attr_name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EUNKNOWN);
            }
            lSetHost(new_ep, nm, s);
            break;
         default:
            DPRINTF("unexpected data type\n");
            DRETURN(STATUS_EUNKNOWN);
      }
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_bool() ************************************
*  NAME
*     attr_mod_bool() -- modify raw boolean, no verification 
*
*  SYNOPSIS
*     int attr_mod_bool(lListElem *qep, lListElem *new_ep, int nm, char 
*     *attr_name) 
*
*  FUNCTION
*     This function modifies "new_qep" attribute with boolean value from "qep"
*
*  INPUTS
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name 
*
*  RESULT
*     int - 0 succes, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_bool() is MT safe 
*
*******************************************************************************/
int
attr_mod_bool(lListElem *qep, lListElem *new_ep, int nm, const char *attr_name) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      DPRINTF("got new %s\n", attr_name);
      lSetBool(new_ep, nm, lGetBool(qep, nm));
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_ulong() ***********************************
*  NAME
*     attr_mod_ulong() -- modify raw ulong, no verification 
*
*  SYNOPSIS
*     int attr_mod_ulong(lListElem *qep, lListElem *new_ep, int nm, char 
*     *attr_name) 
*
*  FUNCTION
*     This function modifies "new_qep" attribute with boolean value from "qep"
*
*  INPUTS
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name 
*
*  RESULT
*     int - 0 succes, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_ulong() is MT safe 
*    
*******************************************************************************/
int attr_mod_ulong(lListElem *qep, lListElem *new_ep, int nm, const char *attr_name) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      DPRINTF("got new %s\n", attr_name);
      lSetUlong(new_ep, nm, lGetUlong(qep, nm));
   }

   DRETURN(0);
}

int attr_mod_ulong64(lListElem *qep, lListElem *new_ep, int nm, const char *attr_name) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      DPRINTF("got new %s\n", attr_name);
      lSetUlong64(new_ep, nm, lGetUlong64(qep, nm));
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_double() **********************************
*  NAME
*     attr_mod_double() --  modify raw double, no verification 
*
*  SYNOPSIS
*     int attr_mod_double(lListElem *qep, lListElem *new_ep, int nm, char 
*     *attr_name) 
*
*  FUNCTION
*    This function modifies "new_qep" attribute with double value from "qep" 
*
*  INPUTS
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name 
*
*  RESULT
*     int - 0 success, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_double() is MT safe 
*
*******************************************************************************/
int
attr_mod_double(lListElem *qep, lListElem *new_ep, int nm, char *attr_name) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      DPRINTF("got new %s\n", attr_name);
      lSetDouble(new_ep, nm, lGetDouble(qep, nm));
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_mem_str() *********************************
*  NAME
*     attr_mod_mem_str() -- modify memory string, nullptr is not allowed
*
*  SYNOPSIS
*     int attr_mod_mem_str(lList **alpp, lListElem *qep, lListElem *new_ep, int 
*     nm, char *attr_name) 
*
*  FUNCTION
*      This function modifies "new_qep" attribute with string from "qep 
*      except that the value of an memory attribute may not be nullptr.
*
*  INPUTS
*     lList **alpp      - AN_Type, The answer list 
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name
*
*  RESULT
*     int - 0 success, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_mem_str() is MT safe 
*
*******************************************************************************/
int
attr_mod_mem_str(lList **alpp, lListElem *qep, lListElem *new_ep, int nm, char *attr_name) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      const char *str;

      str = lGetString(qep, nm);
      DPRINTF("got new %s\n", attr_name);

      if (!parse_ulong_val(nullptr, nullptr, TYPE_MEM, str, nullptr, 0)) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_TYPE_MEM_SS, attr_name, str ? str : "(null)");
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESYNTAX);
      }

      lSetString(new_ep, nm, str);
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_time_str() ********************************
*  NAME
*     attr_mod_time_str() -- modify a valid time string
*
*  SYNOPSIS
*     int attr_mod_time_str(lList **alpp, lListElem *qep, lListElem *new_ep, 
*     int nm, char *attr_name, int enable_infinity) 
*
*  FUNCTION
*     This function modifies "new_qep" attribute with string from "qep" 
*     The value of an time_str attribute may not be nullptr and must be valid.
*       
*  INPUTS
*     lList **alpp      - AN_Type, The answer list 
*     lListElem *qep    - CQ_Type, source of the modification 
*     lListElem *new_ep - CQ_Type, destination of the modification 
*     int nm            - CULL attribute name (CQ_Type) of the element
*     char *attr_name   - CULL sublist attribute name 
*     int enable_infinity - The "infinity" string can be there
*
*  RESULT
*     int - 0 on success, othewise error
*
*  NOTES
*     MT-NOTE: attr_mod_time_str() is MT safe 
*
*******************************************************************************/
int
attr_mod_time_str(lList **alpp, lListElem *qep, lListElem *new_ep, int nm, char *attr_name, int enable_infinity) {
   DENTER(TOP_LAYER);

   /* ---- attribute nm */
   if (lGetPosViaElem(qep, nm, SGE_NO_ABORT) >= 0) {
      const char *str;

      str = lGetString(qep, nm);
      DPRINTF("got new %s\n", attr_name);

      if (str != nullptr) {
         /* don't allow infinity for these parameters */
         if ((strcasecmp(str, "infinity") == 0) && (enable_infinity == 0)) {
            DPRINTF("ERROR! Infinity value for \"%s\"\n", attr_name);
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_SIG_DIGIT_SS, attr_name, str);
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ESYNTAX);
         }
      }

      if (!parse_ulong_val(nullptr, nullptr, TYPE_TIM, str, nullptr, 0)) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_TYPE_TIME_SS, attr_name, str ? str : "(null)");
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_ESYNTAX);
      }

      lSetString(new_ep, nm, str);
   }

   DRETURN(0);
}

/****** sge_utility_qmaster/attr_mod_sub_list() ********************************
*  NAME
*     attr_mod_sub_list() -- This function modifies a certain configuration sublist 
*
*  SYNOPSIS
*     bool attr_mod_sub_list(lList **alpp, lListElem *this_elem, int 
*     this_elem_name, int this_elem_primary_key, lListElem *delta_elem, int 
*     sub_command, const char *sub_list_name, const char *object_name, int 
*     no_info) 
*
*  FUNCTION
*     This function modifies a certain configuration sublist of "this_elem".
*     Possible errors will be reported in answer_list "alpp". The reduced_elem 
*     "delta_elem" will be used to identify the changes which have been made.
*     "sub_command" defines how these changes should be applied to "this_elem".
*
*  INPUTS
*     lList **alpp              - The AN_Type, answer_list
*     lListElem *this_elem      - The target object element, CQ_Type 
*     int this_elem_name        - The name of the list elemet (lList) 
*     int this_elem_primary_key - The primary field for sublist 
*     lListElem *delta_elem     - The source (probably reduced) list of the elements, CQ_Type 
*     int sub_command           - The add, modify, remove command, GDI subcommand
*     const char *sub_list_name - The sublist name
*     const char *object_name   - The target object name
*     int no_info               - Skip or add the info messages 
*     bool *changed             - Report back if changes have been made (may be nullptr)
*
*  RESULT
*     bool - true, the success
*
*  NOTES
*     MT-NOTE: attr_mod_sub_list() is MT safe 
*
*******************************************************************************/
bool
attr_mod_sub_list(lList **alpp, lListElem *this_elem, int this_elem_name, int this_elem_primary_key,
                  const lListElem *delta_elem, ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_cmd,
                  const char *sub_list_name, const char *object_name, int no_info, bool *changed) {
   bool ret = true;
   bool did_changes = false;

   DENTER(TOP_LAYER);
   if (lGetPosViaElem(delta_elem, this_elem_name, SGE_NO_ABORT) >= 0) {
      if (sub_cmd & ocs::gdi::SubCommand::SGE_GDI_CHANGE ||
          sub_cmd & ocs::gdi::SubCommand::SGE_GDI_APPEND ||
          sub_cmd & ocs::gdi::SubCommand::SGE_GDI_REMOVE) {
         lList *reduced_sublist;
         lList *full_sublist;
         lListElem *reduced_element, *next_reduced_element;
         lListElem *full_element, *next_full_element;

         reduced_sublist = lGetListRW(delta_elem, this_elem_name);
         full_sublist = lGetListRW(this_elem, this_elem_name);
         next_reduced_element = lFirstRW(reduced_sublist);
         /*
         ** we try to find each element of the delta_elem
         ** in the sublist if this_elem. Elements which can be found
         ** will be moved into sublist of this_elem.
         */
         while ((reduced_element = next_reduced_element)) {
            int restart_loop = 0;

            next_reduced_element = lNextRW(reduced_element);
            next_full_element = lFirstRW(full_sublist);
            while ((full_element = next_full_element)) {
               int pos, type;
               const char *rstring = nullptr, *fstring = nullptr;

               next_full_element = lNextRW(full_element);

               pos = lGetPosViaElem(reduced_element, this_elem_primary_key, SGE_NO_ABORT);
               type = lGetPosType(lGetElemDescr(reduced_element), pos);
               if (type == lStringT) {
                  rstring = lGetString(reduced_element, this_elem_primary_key);
                  fstring = lGetString(full_element, this_elem_primary_key);
               } else if (type == lHostT) {
                  rstring = lGetHost(reduced_element, this_elem_primary_key);
                  fstring = lGetHost(full_element, this_elem_primary_key);
               }

               if (rstring == nullptr || fstring == nullptr) {
                  ERROR(SFNMAX, MSG_OBJECT_VALUEMISSING);
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
                  ret = false;
               }

               if (ret &&
                   (((type == lStringT) && strcmp(rstring, fstring) == 0) ||
                    ((type == lHostT) && sge_hostcmp(rstring, fstring) == 0))) {
                  lListElem *new_sub_elem;
                  lListElem *old_sub_elem;

                  /* element already exists */
                  next_reduced_element = lNextRW(reduced_element);
                  if (sub_cmd & ocs::gdi::SubCommand::SGE_GDI_CHANGE) {
                     if (object_has_differences(reduced_element, nullptr, full_element)) {
                        /* new object differs from old one - exchange them */
                        did_changes = true;
                        new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                        old_sub_elem = lDechainElem(full_sublist, full_element);
                        lFreeElem(&old_sub_elem);
                        lAppendElem(full_sublist, new_sub_elem);
                     } else {
                        /* objects do not differ - delete the new one */
                        new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                        lFreeElem(&new_sub_elem);
                     }

                     restart_loop = 1;
                     break;
                  } else if (sub_cmd & ocs::gdi::SubCommand::SGE_GDI_APPEND) {
                     if (!no_info) {
                        INFO(MSG_OBJECT_ALREADYEXIN_SSS, rstring, sub_list_name, object_name);
                        answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                        ret = false;
                     }

                     new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                     lFreeElem(&new_sub_elem);

                     restart_loop = 1;
                     break;
                  } else if (sub_cmd & ocs::gdi::SubCommand::SGE_GDI_REMOVE) {
                     did_changes = true;
                     new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                     old_sub_elem = lDechainElem(full_sublist, full_element);
                     lFreeElem(&old_sub_elem);
                     lFreeElem(&new_sub_elem);

                     restart_loop = 1;
                     break;
                  }
               }
            }
            if (!ret) {
               break;
            }
            if (restart_loop) {
               next_reduced_element = lFirstRW(reduced_sublist);
            }
         }

         /* now we process elements which have not been found in the first step above */
         if (ret && (sub_cmd & ocs::gdi::SubCommand::SGE_GDI_CHANGE || sub_cmd & ocs::gdi::SubCommand::SGE_GDI_APPEND || sub_cmd & ocs::gdi::SubCommand::SGE_GDI_REMOVE)) {
            next_reduced_element = lFirstRW(reduced_sublist);

            while ((reduced_element = next_reduced_element)) {
               int pos, type;
               const char *rstring = nullptr;
               lListElem *new_sub_elem;

               next_reduced_element = lNextRW(reduced_element);

               pos = lGetPosViaElem(reduced_element, this_elem_primary_key, SGE_NO_ABORT);
               type = lGetPosType(lGetElemDescr(reduced_element), pos);
               if (type == lStringT) {
                  rstring = lGetString(reduced_element, this_elem_primary_key);
               } else if (type == lHostT) {
                  rstring = lGetHost(reduced_element, this_elem_primary_key);
               }

               if (rstring == nullptr) {
                  ERROR(SFNMAX, MSG_OBJECT_VALUEMISSING);
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC,
                                  ANSWER_QUALITY_ERROR);
                  ret = false;
               }

               if (ret) {
                  if (!no_info && sub_cmd & ocs::gdi::SubCommand::SGE_GDI_REMOVE) {
                     INFO(SFQ " does not exist in " SFQ " of " SFQ, rstring, sub_list_name, object_name);
                     answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                  } else {
                     if (!full_sublist) {
                        if (!no_info && sub_cmd & ocs::gdi::SubCommand::SGE_GDI_CHANGE) {
                           INFO(SFQ " of " SFQ " is empty - Adding new element(s).", sub_list_name, object_name);
                           answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                        }
                        lSetList(this_elem, this_elem_name, lCopyList("", lGetList(delta_elem, this_elem_name)));
                        full_sublist = lGetListRW(this_elem, this_elem_name);

                        did_changes = true;
                        break;
                     } else {
                        if (!no_info && sub_cmd & ocs::gdi::SubCommand::SGE_GDI_CHANGE) {
                           INFO("Unable to find " SFQ " in " SFQ " of " SFQ " - Adding new element.", rstring, sub_list_name, object_name);
                           answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                        }
                        new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                        lAppendElem(full_sublist, new_sub_elem);
                        did_changes = true;
                     }
                  }
               }
            }
         }
      } else {
         /* Overwrite the complete list */
         lSetList(this_elem, this_elem_name, lCopyList("", lGetList(delta_elem, this_elem_name)));
         did_changes = true;
      }

      /* If the list does not contain any elements, we will delete the list itself */
      if (ret) {
         const lList *tmp_list = lGetList(this_elem, this_elem_name);

         if (tmp_list != nullptr && lGetNumberOfElem(tmp_list) == 0) {
            lSetList(this_elem, this_elem_name, nullptr);
         }
      }
   } else {
      ret = false;
   }

   if (changed != nullptr) {
      *changed = did_changes;
   }

   DRETURN(ret);
}


/****** sgeobj/cqueue/cqueue_mod_sublist() ***********************************
*  NAME
*     cqueue_mod_sublist() -- modify cqueues configuration sublist 
*
*  SYNOPSIS
*     bool 
*     cqueue_mod_sublist(lListElem *this_elem, lList **answer_list, 
*                        lListElem *reduced_elem, int sub_command, 
*                        int attribute_name, int sublist_host_name, 
*                        int sublist_value_name, int subsub_key, 
*                        const char *attribute_name_str, 
*                        const char *object_name_str) 
*
*  FUNCTION
*     This function modifies a certain configuration sublist of "this_elem".
*     Possible errors will be reported in "answer_list". "reduced_elem"
*     will be used to identify the changes which have been done.
*     "sub_command" defines how these changes should be applied to 
*     "this_elem". "sublist_value_name" is the sublist of "this_elem" which
*     should be modified whereas "subsub_key" defines the attribute
*     which containes the primary key of that sublist. "attribute_name_str"
*     is the name of the cqueue attribute which will be modified with
*     this operation. It will be used for error output. "object_name_str"
*     is the name of the cqueue which will be modified by this operation.
*      
*
*  INPUTS
*     lListElem *this_elem           - CQ_Type 
*     lList **answer_list            - AN_Type 
*     lListElem *reduced_elem        - reduced CQ_Type element 
*     int sub_command                - GDI subcommand 
*     int attribute_name             - CULL attribute name (CQ_Type)
*     int sublist_host_name          - CULL sublist attribute name
*                                      depend on sublist of CQ_Type 
*     int sublist_value_name         - CULL sublist attribute name of that
*                                      field which containes the value of
*                                      the attribute to be modified.
*     int subsub_key                 - CULL sublist attribute key
*     const char *attribute_name_str - string used for user output 
*     const char *object_name_str    - cqueue name 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: cqueue_mod_sublist() is MT safe 
*******************************************************************************/
bool
cqueue_mod_sublist(lListElem *this_elem, lList **answer_list, lListElem *reduced_elem,
                   ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_cmd,
                   int attribute_name, int sublist_host_name, int sublist_value_name, int subsub_key,
                   const char *attribute_name_str, const char *object_name_str) {
   bool ret = true;
   int pos;

   DENTER(CQUEUE_LAYER);

   pos = lGetPosViaElem(reduced_elem, attribute_name, SGE_NO_ABORT);
   if (pos >= 0) {
      lList *mod_list = lGetPosList(reduced_elem, pos);
      lList *org_list = lGetListRW(this_elem, attribute_name);
      lListElem *mod_elem;

      /* 
       * Delete all configuration lists except the default-configuration
       * if sub_command is SGE_GDI_SET_ALL
       */
      if (sub_cmd & ocs::gdi::SubCommand::SGE_GDI_SET_ALL) {
         lListElem *elem, *next_elem;

         next_elem = lFirstRW(org_list);
         while ((elem = next_elem)) {
            const char *name = lGetHost(elem, sublist_host_name);

            next_elem = lNextRW(elem);
            mod_elem = lGetElemHostRW(mod_list, sublist_host_name, name);
            if (mod_elem == nullptr) {
               DPRINTF("Removing attribute list for " SFQ "\n", name);
               lRemoveElem(org_list, &elem);
            }
         }
      }

      /*
       * Do modifications for all given elements of 
       * domain/host-configuration list
       */
      for_each_rw(mod_elem, mod_list) {
         const char *name = lGetHost(mod_elem, sublist_host_name);
         char resolved_name[CL_MAXHOSTNAMELEN + 1];
         lListElem *org_elem = nullptr;

         if (name == nullptr) {
            ERROR(MSG_SGETEXT_INVALIDHOST_S, "");
            answer_list_add(answer_list, SGE_EVENT,
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         /* Don't try to resolve hostgroups */
         if (name[0] != '@') {
            int back = getuniquehostname(name, resolved_name, 0);

            if (back == CL_RETVAL_OK) {
               /* 
                * This assignment is ok because preious name contained a const
                * string from the mod_elem that we didn't need to free.  
                * Now it will contain a string that's on the stack, 
                * so we still don't have to free it. 
                */
               name = resolved_name;
            } else {
               /*
                * Due to CR 6319231, IZ 1760 this is allowed
                */
            }
         }

         org_elem = lGetElemHostRW(org_list, sublist_host_name, name);

         /*
          * Create element if it does not exist
          */
         if (org_elem == nullptr && !(sub_cmd & ocs::gdi::SubCommand::SGE_GDI_REMOVE)) {
            if (org_list == nullptr) {
               org_list = lCreateList("", lGetElemDescr(mod_elem));
               lSetList(this_elem, attribute_name, org_list);
            }
            org_elem = lCreateElem(lGetElemDescr(mod_elem));
            lSetHost(org_elem, sublist_host_name, name);
            lAppendElem(org_list, org_elem);
         }

         /*
          * Modify sublist according to subcommand
          */
         if (org_elem != nullptr) {
            if (subsub_key != NoName) {
               attr_mod_sub_list(answer_list, org_elem, sublist_value_name, subsub_key, mod_elem, cmd, sub_cmd,
                                 attribute_name_str, object_name_str, 0, nullptr);
            } else {
               object_replace_any_type(org_elem, sublist_value_name, mod_elem);
            }
         }
      }
   }

   DRETURN(ret);
}

int
multiple_occurances(lList **alpp, const lList *lp1, const lList *lp2, int nm, const char *name, const char *obj_name) {
   const lListElem *ep1;
   const char *s;

   DENTER(TOP_LAYER);

   if (!lp1 || !lp2) {
      DRETURN(0);
   }

   for_each_ep(ep1, lp1) {
      s = lGetString(ep1, nm);
      if (lGetElemStr(lp2, nm, s)) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_MULTIPLE_OCCUR_SSSS, (nm == US_name) ? MSG_OBJ_USERSET : MSG_JOB_PROJECT, s, obj_name, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

void
normalize_sublist(lListElem *ep, int nm) {
   const lList *lp;

   if ((lp = lGetList(ep, nm)) && lGetNumberOfElem(lp) == 0)
      lSetList(ep, nm, nullptr);
}

