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

#include <cstdint>
#include <type_traits>

enum class SchedulerMeasureCounter : uint32_t {
    RESERVE_CALLS,
    RESERVE_ENTRIES,
    PUBLISH_CALLS,
    PUBLISH_ENTRIES,
    PROBE_OWN,
    PROBE_REMOTE,
    POP_SUCCESS,
    POP_NOT_READY,
    POP_RETRY,
    POP_CAS_FAILURE,
    MIX_UNPLACED,
    ADMISSION_ATTEMPT,
    ADMISSION_FAILURE,
    ADMISSION_SUCCESS,
    COUNT,
};

enum class SchedulerMeasureTimer : uint32_t { RESERVE, PUBLISH, POP, COMMIT, COUNT };
constexpr uint32_t SCHEDULER_MEASURE_COUNTERS = static_cast<uint32_t>(SchedulerMeasureCounter::COUNT);
constexpr uint32_t SCHEDULER_MEASURE_TIMERS = static_cast<uint32_t>(SchedulerMeasureTimer::COUNT);

struct SchedulerMeasureSample {
    uint64_t count{0};
    uint64_t sum{0};
    uint64_t minimum{UINT64_MAX};
    uint64_t maximum{0};
    // Durations in raw cycles: <256, <1024, <4096, and >=4096.
    uint64_t buckets[4]{};
};

struct alignas(128) SchedulerMeasureRecord {
    uint64_t run_epoch{0};
    uint64_t worker_id{0};
    uint64_t finished{0};
    uint64_t overflow{0};
    uint64_t core_type{0};
    uint64_t cluster_index{0};
    uint64_t cluster_lane{0};
    uint64_t type_rank{0};
    // Bootstrap publications and run-time publications have different producers.
    uint64_t counters[2][SCHEDULER_MEASURE_COUNTERS]{};
    SchedulerMeasureSample timers[SCHEDULER_MEASURE_TIMERS]{};
};

struct alignas(128) SchedulerMeasureHeader {
    uint64_t version{1};
    uint64_t mode{0};  // 1=count, 2=sample; an absent region means off.
    uint64_t period{0};
    uint64_t run_epoch{0};
    uint64_t worker_count{0};
    uint64_t records_offset{0};
    uint64_t record_bytes{sizeof(SchedulerMeasureRecord)};
    uint64_t region_bytes{0};
};

struct SchedulerMeasureLocal {
    SchedulerMeasureRecord record{};
    uint64_t mode{0};
    uint64_t period{0};
    uint64_t sequence[SCHEDULER_MEASURE_TIMERS]{};
    uint32_t phase{0};
};

inline __aicore__ uint64_t scheduler_measure_clock() {
#if defined(__CCE_AICORE__)
    return get_sys_cnt_aicore();
#else
    return 0;
#endif
}

inline __aicore__ void
scheduler_measure_add(SchedulerMeasureLocal *local, SchedulerMeasureCounter counter, uint64_t amount = 1) {
    if (local == nullptr) return;
    auto &value = local->record.counters[local->phase][static_cast<uint32_t>(counter)];
    if (amount > UINT64_MAX - value) {
        local->record.overflow = 1;
        value = UINT64_MAX;
    } else {
        value += amount;
    }
}

inline __aicore__ uint64_t scheduler_measure_begin(SchedulerMeasureLocal *local, SchedulerMeasureTimer timer) {
    if (local == nullptr || local->mode != 2 || local->phase == 0) return 0;
    auto &sequence = local->sequence[static_cast<uint32_t>(timer)];
    if ((++sequence & (local->period - 1)) != 0) return 0;
    return scheduler_measure_clock();
}

inline __aicore__ void
scheduler_measure_end(SchedulerMeasureLocal *local, SchedulerMeasureTimer timer, uint64_t start) {
    if (local == nullptr || start == 0) return;
    const uint64_t end = scheduler_measure_clock();
    if (end < start) {
        local->record.overflow = 1;
        return;
    }
    auto &sample = local->record.timers[static_cast<uint32_t>(timer)];
    const uint64_t duration = end - start;
    if (sample.count == UINT64_MAX || duration > UINT64_MAX - sample.sum) {
        local->record.overflow = 1;
        return;
    }
    ++sample.count;
    sample.sum += duration;
    if (duration < sample.minimum) sample.minimum = duration;
    if (duration > sample.maximum) sample.maximum = duration;
    ++sample.buckets[duration < 256 ? 0 : duration < 1024 ? 1 : duration < 4096 ? 2 : 3];
}

static_assert(sizeof(SchedulerMeasureHeader) == 128);
static_assert(sizeof(SchedulerMeasureRecord) % 128 == 0);
static_assert(std::is_standard_layout_v<SchedulerMeasureRecord>);
static_assert(std::is_trivially_copyable_v<SchedulerMeasureRecord>);
