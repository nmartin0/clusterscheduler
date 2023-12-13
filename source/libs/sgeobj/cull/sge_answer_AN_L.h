#ifndef SGE_AN_L_H
#define SGE_AN_L_H
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
*    SGE_ULONG(AN_status) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AN_text) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(AN_quality) - @todo add summary
*    @todo add description
*
*/

enum {
   AN_status = AN_LOWERBOUND,
   AN_text,
   AN_quality
};

LISTDEF(AN_Type)
   SGE_ULONG(AN_status, CULL_DEFAULT)
   SGE_STRING(AN_text, CULL_DEFAULT)
   SGE_ULONG(AN_quality, CULL_DEFAULT)
LISTEND

NAMEDEF(ANN)
   NAME("AN_status")
   NAME("AN_text")
   NAME("AN_quality")
NAMEEND

#define AN_SIZE sizeof(ANN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
