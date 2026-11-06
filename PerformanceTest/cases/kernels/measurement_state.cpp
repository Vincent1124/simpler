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

#include <cstdint>
#include <pto/pto-inst.hpp>

#include "aicore/aicore.h"
#include "intrinsic.h"
#include "scheduler/scheduler_measurement_publish.h"
#include "tensor.h"

template <typename Record>
__aicore__ __attribute__((always_inline)) inline void fill_record(Record &record) {
    record.run_epoch = 101;
    record.worker_id = 3;
    record.finished = 0;
    record.overflow = 0;
    record.core_type = 1;
    record.cluster_index = 7;
    record.cluster_lane = 2;
    record.type_rank = 5;
    for (uint32_t phase = 0; phase < 2; ++phase)
        for (uint32_t counter = 0; counter < SCHEDULER_MEASURE_COUNTERS; ++counter)
            record.counters[phase][counter] = phase * 1000 + counter + 1;
    for (uint32_t timer = 0; timer < SCHEDULER_MEASURE_TIMERS; ++timer) {
        auto &sample = record.timers[timer];
        sample.count = 10 + timer * 4;
        sample.sum = sample.count * (15 + timer);
        sample.minimum = 10 + timer;
        sample.maximum = 20 + timer;
        for (uint32_t bucket = 0; bucket < 4; ++bucket)
            sample.buckets[bucket] = timer + bucket + 1;
    }
}

extern "C" __aicore__ void kernel_entry(__gm__ int64_t *args) {
    auto *tensor = reinterpret_cast<__gm__ Tensor *>(args[0]);
    auto *words = reinterpret_cast<__gm__ uint64_t *>(tensor->buffer.addr) + tensor->start_offset;
    auto *destination = reinterpret_cast<__gm__ SchedulerMeasureRecord *>(words);
    if (args[1] == 0) {
        SchedulerMeasureRecord record{};
        fill_record(record);
        scheduler_publish_measurement(destination, record, true);
    } else if (args[1] == 1) {
        volatile SchedulerMeasureRecord record{};
        fill_record(record);
        scheduler_publish_measurement(destination, record, true);
    } else {
        words[0] = 101;
        words[1] = 3;
        words[2] = 1;
        words[3] = 0;
        words[4] = 1;
        words[5] = 7;
        words[6] = 2;
        words[7] = 5;
        for (uint32_t phase = 0; phase < 2; ++phase)
            for (uint32_t counter = 0; counter < SCHEDULER_MEASURE_COUNTERS; ++counter)
                words[8 + phase * SCHEDULER_MEASURE_COUNTERS + counter] = phase * 1000 + counter + 1;
        for (uint32_t timer = 0; timer < SCHEDULER_MEASURE_TIMERS; ++timer) {
            const uint32_t start = 36 + timer * 8;
            words[start] = 10 + timer * 4;
            words[start + 1] = (10 + timer * 4) * (15 + timer);
            words[start + 2] = 10 + timer;
            words[start + 3] = 20 + timer;
            for (uint32_t bucket = 0; bucket < 4; ++bucket)
                words[start + 4 + bucket] = timer + bucket + 1;
        }
        for (uint32_t offset = 0; offset < sizeof(SchedulerMeasureRecord); offset += 64)
            scheduler_publish_cache_line(reinterpret_cast<__gm__ uint8_t *>(destination) + offset);
    }
}
