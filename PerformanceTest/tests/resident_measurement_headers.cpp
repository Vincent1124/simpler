/*
 * Copyright (c) PyPTO Contributors.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 * -----------------------------------------------------------------------------------------------------------
 */

#include "scheduler/scheduler_admission.h"
#include "scheduler_measurement_export.h"

#include <cassert>

int main() {
    assert(sizeof(SchedulerWorkerContext) == 1024);
    assert(sizeof(SchedulerGraphSummary) == 128);
    assert(sizeof(SchedulerMeasureRecord) == 640);
    SchedulerLocalState local;
    SchedulerMeasureLocal measurement;
    local.measurement = &measurement;
    scheduler_measure_add(local.measurement, SchedulerMeasureCounter::ADMISSION_ATTEMPT);
    assert(measurement.record.counters[0][static_cast<uint32_t>(SchedulerMeasureCounter::ADMISSION_ATTEMPT)] == 1);
}
