#ifndef __SGE_CALENDAR_QMASTER_H
#define __SGE_CALENDAR_QMASTER_H
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



#include "sge_c_gdi.h"

int calendar_mod(lList **alpp, lListElem *new_cal, lListElem *cep, int add, char *ruser, char *rhost, gdi_object_t *object, int sub_command);

int calendar_spool(lList **alpp, lListElem *cep, gdi_object_t *object);

int calendar_update_queue_states(lListElem *cep, lListElem *old_cep, gdi_object_t *object);

int sge_del_calendar(lListElem *ep, lList **alpp, char *ruser, char *rhost);

void calendar_event(u_long32 type, u_long32 when, u_long32 uval0, u_long32 uval1, const char *sval);

u_long32 act_cal_state(lListElem *cep, time_t *then);
    
int parse_year(lList **alpp, lListElem *cal);

int parse_week(lList **alpp, lListElem *cal);

#endif /* __SGE_CALENDAR_QMASTER_H */

