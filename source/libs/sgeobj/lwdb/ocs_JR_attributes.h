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
 * This code was generated from file source/libs/sgeobj/json/JR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JR_job_number = 4750,
   JR_ja_task_number,
   JR_queue_name,
   JR_state,
   JR_failed,
   JR_general_failure,
   JR_err_str,
   JR_usage,
   JR_job_pid,
   JR_ckpt_arena,
   JR_pe_task_id_str,
   JR_osjobid,
   JR_wait_status,
   JR_flush,
   JR_no_send,
   JR_delay_report
};

constexpr const int JR_Type[] = {
   JR_job_number,
   JR_ja_task_number,
   JR_queue_name,
   JR_state,
   JR_failed,
   JR_general_failure,
   JR_err_str,
   JR_usage,
   JR_job_pid,
   JR_ckpt_arena,
   JR_pe_task_id_str,
   JR_osjobid,
   JR_wait_status,
   JR_flush,
   JR_no_send,
   JR_delay_report,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JR_ATTRIBUTES \
   {JR_job_number, "JR_job_number", AttributeStatic::UINT32, AttributeStatic::UNORDERED_UNIQUE}, \
   {JR_ja_task_number, "JR_ja_task_number", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_queue_name, "JR_queue_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JR_state, "JR_state", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_failed, "JR_failed", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_general_failure, "JR_general_failure", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_err_str, "JR_err_str", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JR_usage, "JR_usage", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JR_job_pid, "JR_job_pid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_ckpt_arena, "JR_ckpt_arena", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_pe_task_id_str, "JR_pe_task_id_str", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JR_osjobid, "JR_osjobid", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JR_wait_status, "JR_wait_status", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JR_flush, "JR_flush", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JR_no_send, "JR_no_send", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JR_delay_report, "JR_delay_report", AttributeStatic::BOOL, AttributeStatic::NO_HASH} \

} // end namespace

