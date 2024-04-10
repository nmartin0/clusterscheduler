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

#include <utility>

#include "oge_ReportingFileWriter.h"

namespace oge {
   class BaseReportingFileWriter : public ReportingFileWriter {
   protected:
      bool do_joblog;
      bool log_consumables;
      u_long32 sharelog_interval;
      u_long32 next_sharelog;
   public:
      explicit BaseReportingFileWriter(std::string filename) : ReportingFileWriter(std::move(filename)),
         do_joblog(false), log_consumables(false), sharelog_interval(0), next_sharelog(0) {
      }

      u_long32
      trigger(monitoring_t *monitor) override;

      void update_config() override;

      virtual void
      create_sharelog_record(monitoring_t *monitor) = 0;
   };
}
