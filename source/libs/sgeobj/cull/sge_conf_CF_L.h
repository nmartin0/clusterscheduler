#ifndef SGE_CF_L_H
#define SGE_CF_L_H
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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(CF_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CF_value) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CF_sublist) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CF_local) - @todo add summary
*    @todo add description
*
*/

enum {
   CF_name = CF_LOWERBOUND,
   CF_value,
   CF_sublist,
   CF_local
};

LISTDEF(CF_Type)
   SGE_STRING(CF_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(CF_value, CULL_SUBLIST)
   SGE_LIST(CF_sublist, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(CF_local, CULL_DEFAULT)
LISTEND

NAMEDEF(CFN)
   NAME("CF_name")
   NAME("CF_value")
   NAME("CF_sublist")
   NAME("CF_local")
NAMEEND

#define CF_SIZE sizeof(CFN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
