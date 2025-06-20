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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"

#include "sge_serf.h"

/****** SERF/-SERF_Implementation *******************************************
*  NAME
*     SERF_Implementation -- Functions that implement a generic schedule 
*                             entry recording facility (SERF)
*
*  SEE ALSO
*     SERF/serf_init()
*     SERF/serf_record_entry()
*     SERF/serf_new_interval()
*     SERF/serf_get_active()
*     SERF/serf_set_active()
*     SERF/serf_exit()
*******************************************************************************/
typedef struct {
   record_schedule_entry_func_t record_schedule_entry;   
   new_schedule_func_t new_schedule;   
} sge_serf_t;
static sge_serf_t current_serf = { nullptr, nullptr }; /* thread local */


/****** sge_resource_utilization/serf_init() ***********************************
*  NAME
*     serf_init() -- Initializes SERF
*
*  SYNOPSIS
*     void serf_init(record_schedule_entry_func_t write, new_schedule_func_t 
*     newline) 
*
*  NOTES
*     MT-NOTE: serf_init() is not MT safe 
*******************************************************************************/
void serf_init(record_schedule_entry_func_t write, new_schedule_func_t newline)
{
   current_serf.record_schedule_entry = write;
   current_serf.new_schedule          = newline;
}


/****** sge_resource_utilization/serf_record_entry() ***************************
*  NAME
*     serf_record_entry() -- Add a new schedule entry record
*
*  SYNOPSIS
*     void serf_record_entry(u_long32 job_id, u_long32 ja_taskid, const char 
*     *state, u_long32 start_time, u_long32 end_time, u_long32 level, const
*     char *object_name, const char *name, double utilization) 
*
*  FUNCTION
*     The entirety of all information passed to this function describes
*     the schedule that was created during a scheduling interval of a
*     Cluster Scheduler scheduler. To reflect multiple resource debitations 
*     of a job multiple calls to serf_record_entry() are required. For
*     parallel jobs the serf_record_entry() is called one times with a
*     'P' as level_char.
*
*  INPUTS
*     u_long32 job_id         - The job id
*     u_long32 ja_taskid      - The task id
*     const char *type        - A string indicating the reason why the 
*                               utilization was put into the schedule:
*
*                               RUNNING    - Job was running before scheduling run
*                               SUSPENDED  - Job was suspended before scheduling run
*                               PREEMPTING - Job gets preempted currently 
*                               STARTING   - Job will be started 
*                               RESERVING  - Job reserves resources
*
*     u_long32 start_time     - Start of the resource utilization
*
*     u_long32 end_time       - End of the resource utilization
*
*     char level_char         - Q - Queue 
*                               H - Host
*                               G - Global
*                               P - Parallel Environment (PE)
*
*     const char *object_name - Name of Queue/Host/Global/PE
*
*     const char *name        - Resource name
*
*     double utilization      - Utilization amount
*
*  NOTES
*     MT-NOTE: (1) serf_record_entry() is MT safe if no recording function
*     MT-NOTE:     was registered via serf_init(). 
*     MT-NOTE: (2) Otherwise MT safety of serf_record_entry() depends on 
*     MT-NOTE:     MT safety of registered recording function
*******************************************************************************/
void serf_record_entry(u_long32 job_id, u_long32 ja_taskid,
                       const char *type, u_long64 start_time, u_long64 end_time, u_long32 level,
                       const char *object_name, const char *name, double utilization)
{
   DENTER(TOP_LAYER);

   char level_char = CENTRY_LEVEL_TO_CHAR(level);

   /* human-readable format */
   if (DPRINTF_IS_ACTIVE) {
      DSTRING_STATIC(dstr_s, 64);
      DSTRING_STATIC(dstr_e, 64);
      DPRINTF("J=" sge_u32 "." sge_u32 " T=%s S=%s E=%s L=%c O=%s R=%s U=%f\n",
              job_id, ja_taskid, type, sge_ctime64(start_time, &dstr_s), sge_ctime64(end_time, &dstr_e),
              level_char, object_name, name, utilization);
   }

   if (current_serf.record_schedule_entry && serf_get_active()) {
      (current_serf.record_schedule_entry)(job_id, ja_taskid, type, start_time, end_time, 
            level_char, object_name, name, utilization);
   }
   DRETURN_VOID;
}


/****** sge_resource_utilization/serf_new_interval() ***************************
*  NAME
*     serf_new_interval() -- Indicate the end of a  scheduling run
*
*  SYNOPSIS
*     void serf_new_interval(u_long32 time) 
*
*  FUNCTION
*     When a new scheduling run ended serf_new_interval() shall be
*     called to indicate this. This allows assigning of schedule entry
*     records to different schedule runs.
*
*  NOTES
*     MT-NOTE: (1) serf_new_interval() is MT safe if no recording function
*     MT-NOTE:     was registered via serf_init(). 
*     MT-NOTE: (2) Otherwise MT safety of serf_new_interval() depends on 
*     MT-NOTE:     MT safety of registered recording function
*******************************************************************************/
void serf_new_interval()
{
   DENTER(TOP_LAYER);

   DPRINTF("================[SCHEDULING-DONE]==================\n");

   if (current_serf.new_schedule && serf_get_active()) {
      (current_serf.new_schedule)();
   }

   DRETURN_VOID;
}


/****** sge_resource_utilization/serf_exit() ***********************************
*  NAME
*     serf_exit() -- Closes SERF
*
*  SYNOPSIS
*     void serf_exit() 
*
*  FUNCTION
*     All operations requited to cleanly shutdown the SERF are done.
*
*  NOTES
*     MT-NOTE: serf_exit() is MT safe 
*******************************************************************************/
void serf_exit()
{
   memset(&current_serf, 0, sizeof(sge_serf_t)); 
}

