#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/SPTR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   SPTR_is_default = 8350,
   SPTR_rule_name,
   SPTR_rule
};

constexpr const int SPTR_Type[] = {
   SPTR_is_default,
   SPTR_rule_name,
   SPTR_rule,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define SPTR_ATTRIBUTES \
   {SPTR_is_default, "SPTR_is_default", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPTR_rule_name, "SPTR_rule_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_NON_UNIQUE, false, false}, \
   {SPTR_rule, "SPTR_rule", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

