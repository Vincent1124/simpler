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
#include "scheduler/scheduler_measurement_publish.h"
#include "scheduler_measurement_export.h"

#include <cassert>

int main() {
    assert(sizeof(SchedulerWorkerContext) == 1024);
    assert(sizeof(SchedulerGraphSummary) == 128);
    assert(sizeof(SchedulerMeasureRecord) == 640);
    assert(offsetof(SchedulerMeasureLocal, record) == 128);
    SchedulerLocalState local;
    SchedulerMeasureLocal measurement;
    local.measurement = &measurement;
    scheduler_measure_add(local.measurement, SchedulerMeasureCounter::ADMISSION_ATTEMPT);
    assert(measurement.record.counters[0][static_cast<uint32_t>(SchedulerMeasureCounter::ADMISSION_ATTEMPT)] == 1);
    measurement.record.run_epoch = 7;
    measurement.record.worker_id = 3;
    measurement.record.core_type = 1;
    measurement.record.cluster_index = 2;
    measurement.record.cluster_lane = 1;
    measurement.record.type_rank = 5;
    for (uint32_t phase = 0; phase < 2; ++phase)
        for (uint32_t counter = 0; counter < SCHEDULER_MEASURE_COUNTERS; ++counter)
            measurement.record.counters[phase][counter] = phase * 100 + counter;
    for (uint32_t timer = 0; timer < SCHEDULER_MEASURE_TIMERS; ++timer) {
        auto &sample = measurement.record.timers[timer];
        sample.count = timer + 1;
        sample.sum = timer + 2;
        sample.minimum = timer + 3;
        sample.maximum = timer + 4;
        for (uint32_t bucket = 0; bucket < 4; ++bucket)
            sample.buckets[bucket] = timer * 10 + bucket;
    }
    SchedulerMeasureRecord published;
    scheduler_publish_measurement(&published, measurement.record, true);
    assert(published.finished == 1 && published.run_epoch == 7 && published.worker_id == 3);
    assert(published.core_type == 1 && published.cluster_index == 2 && published.cluster_lane == 1);
    assert(published.type_rank == 5 && published.overflow == 0);
    for (uint32_t phase = 0; phase < 2; ++phase)
        for (uint32_t counter = 0; counter < SCHEDULER_MEASURE_COUNTERS; ++counter)
            assert(published.counters[phase][counter] == phase * 100 + counter);
    for (uint32_t timer = 0; timer < SCHEDULER_MEASURE_TIMERS; ++timer) {
        const auto &sample = published.timers[timer];
        assert(sample.count == timer + 1 && sample.sum == timer + 2);
        assert(sample.minimum == timer + 3 && sample.maximum == timer + 4);
        for (uint32_t bucket = 0; bucket < 4; ++bucket)
            assert(sample.buckets[bucket] == timer * 10 + bucket);
    }
    scheduler_publish_measurement(&published, measurement.record, false);
    assert(published.finished == 0);
}
