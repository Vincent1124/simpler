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

inline __aicore__ void scheduler_publish_measurement(
    __gm__ SchedulerMeasureRecord *destination, const SchedulerMeasureRecord &source, bool finished
) {
    destination->finished = 0;
    destination->run_epoch = source.run_epoch;
    destination->worker_id = source.worker_id;
    destination->overflow = source.overflow;
    destination->core_type = source.core_type;
    destination->cluster_index = source.cluster_index;
    destination->cluster_lane = source.cluster_lane;
    destination->type_rank = source.type_rank;
    for (uint32_t phase = 0; phase < 2; ++phase)
        for (uint32_t counter = 0; counter < SCHEDULER_MEASURE_COUNTERS; ++counter)
            destination->counters[phase][counter] = source.counters[phase][counter];
    for (uint32_t timer = 0; timer < SCHEDULER_MEASURE_TIMERS; ++timer) {
        destination->timers[timer].count = source.timers[timer].count;
        destination->timers[timer].sum = source.timers[timer].sum;
        destination->timers[timer].minimum = source.timers[timer].minimum;
        destination->timers[timer].maximum = source.timers[timer].maximum;
        for (uint32_t bucket = 0; bucket < 4; ++bucket)
            destination->timers[timer].buckets[bucket] = source.timers[timer].buckets[bucket];
    }
    for (uint32_t offset = 0; offset < sizeof(SchedulerMeasureRecord); offset += 64)
        scheduler_publish_cache_line(reinterpret_cast<__gm__ uint8_t *>(destination) + offset);
    // The commit marker becomes visible only after every payload cache line.
    destination->finished = finished ? 1 : 0;
    scheduler_publish_cache_line(&destination->finished);
}
