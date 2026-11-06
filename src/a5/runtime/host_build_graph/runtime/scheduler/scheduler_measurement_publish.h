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

#pragma once

#include "scheduler_measurement.h"
#include "scheduler_memory.h"

__aicore__ __attribute__((always_inline)) inline void scheduler_publish_measurement(
    __gm__ SchedulerMeasureRecord *destination, const volatile SchedulerMeasureRecord &source, bool finished,
    bool include_timers = true
) {
    // Host initialization clears every record before each run's publication.
    static_assert(sizeof(SchedulerMeasureRecord) <= 32 * 64);
    uint32_t dirty_lines = 1;
    destination->finished = 0;
    destination->run_epoch = source.run_epoch;
    destination->worker_id = source.worker_id;
    destination->overflow = source.overflow;
    destination->core_type = source.core_type;
    destination->cluster_index = source.cluster_index;
    destination->cluster_lane = source.cluster_lane;
    destination->type_rank = source.type_rank;
    for (uint32_t phase = 0; phase < 2; ++phase) {
        for (uint32_t counter = 0; counter < SCHEDULER_MEASURE_COUNTERS; ++counter) {
            const uint64_t value = source.counters[phase][counter];
            if (value == 0) continue;
            destination->counters[phase][counter] = value;
            const uint32_t offset = offsetof(SchedulerMeasureRecord, counters) +
                                    (phase * SCHEDULER_MEASURE_COUNTERS + counter) * sizeof(uint64_t);
            dirty_lines |= 1U << (offset / 64);
        }
    }
    for (uint32_t timer = 0; include_timers && timer < SCHEDULER_MEASURE_TIMERS; ++timer) {
        const uint64_t count = source.timers[timer].count;
        if (count == 0) continue;
        destination->timers[timer].count = count;
        destination->timers[timer].sum = source.timers[timer].sum;
        destination->timers[timer].minimum = source.timers[timer].minimum;
        destination->timers[timer].maximum = source.timers[timer].maximum;
        for (uint32_t bucket = 0; bucket < 4; ++bucket)
            destination->timers[timer].buckets[bucket] = source.timers[timer].buckets[bucket];
        const uint32_t offset = offsetof(SchedulerMeasureRecord, timers) + timer * sizeof(SchedulerMeasureSample);
        dirty_lines |= 1U << (offset / 64);
        dirty_lines |= 1U << ((offset + sizeof(SchedulerMeasureSample) - 1) / 64);
    }
    for (uint32_t offset = 0; offset < sizeof(SchedulerMeasureRecord); offset += 64) {
        if ((dirty_lines & (1U << (offset / 64))) != 0)
            scheduler_publish_cache_line(reinterpret_cast<__gm__ uint8_t *>(destination) + offset);
    }
    // The commit marker becomes visible only after every payload cache line.
    destination->finished = finished ? 1 : 0;
    scheduler_publish_cache_line(&destination->finished);
}
