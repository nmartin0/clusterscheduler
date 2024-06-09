#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/*
 * This code was generated from file source/libs/sgeobj/json/RU.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(RU_job_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RU_task_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RU_state) - @todo add summary
*    @todo add description
*
*/

enum {
   RU_job_number = RU_LOWERBOUND,
   RU_task_number,
   RU_state
};

LISTDEF(RU_Type)
   SGE_ULONG(RU_job_number, CULL_DEFAULT)
   SGE_ULONG(RU_task_number, CULL_DEFAULT)
   SGE_ULONG(RU_state, CULL_DEFAULT)
LISTEND

NAMEDEF(RUN)
   NAME("RU_job_number")
   NAME("RU_task_number")
   NAME("RU_state")
NAMEEND

#define RU_SIZE sizeof(RUN)/sizeof(char *)


