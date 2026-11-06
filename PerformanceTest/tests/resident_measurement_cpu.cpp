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

#include <cassert>
#include <sstream>
#include <stdexcept>

#include "scheduler_measurement_export.h"

template <typename Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
}

int main() {
    const auto off = scheduler_measure_config(nullptr, nullptr);
    assert(off.mode == 0);
    rejects([] {
        (void)scheduler_measure_config("count", nullptr);
    });
    rejects([] {
        (void)scheduler_measure_config("invalid", ".");
    });
    const auto count = scheduler_measure_config("count", ".");
    const auto sample = scheduler_measure_config("sample256", ".");
    assert(count.mode == 1 && sample.mode == 2 && sample.period == 256);

    SchedulerMeasurePlan plan;
    assert(scheduler_measure_plan(1024, 3, 42, off, &plan));
    assert(plan.offset == 0 && plan.total_size == 1024);
    assert(scheduler_measure_plan(1025, 3, 42, count, &plan));
    assert(plan.offset == 1152 && plan.header.records_offset == 1280);
    assert(plan.total_size == 1280 + 3 * sizeof(SchedulerMeasureRecord));
    assert(!scheduler_measure_plan(UINT64_MAX, 3, 42, count, &plan));
    assert(!scheduler_measure_plan(1024, 0, 42, count, &plan));
    assert(!scheduler_measure_plan(1024, UINT64_MAX, 42, count, &plan));

    SchedulerMeasureLocal local;
    scheduler_measure_add(&local, SchedulerMeasureCounter::RESERVE_ENTRIES, 8);
    local.phase = 1;
    scheduler_measure_add(&local, SchedulerMeasureCounter::RESERVE_ENTRIES, 4);
    const auto index = static_cast<uint32_t>(SchedulerMeasureCounter::RESERVE_ENTRIES);
    assert(local.record.counters[0][index] == 8 && local.record.counters[1][index] == 4);
    scheduler_measure_add(&local, SchedulerMeasureCounter::RESERVE_ENTRIES, UINT64_MAX);
    assert(local.record.overflow == 1 && local.record.counters[1][index] == UINT64_MAX);
    assert(scheduler_measure_begin(nullptr, SchedulerMeasureTimer::POP) == 0);
    assert(scheduler_measure_begin(&local, SchedulerMeasureTimer::POP) == 0);

    assert(scheduler_measure_plan(1024, 3, 42, count, &plan));
    std::vector<SchedulerMeasureRecord> records(3);
    for (uint64_t worker = 0; worker < records.size(); ++worker) {
        records[worker].run_epoch = 42;
        records[worker].worker_id = worker;
        records[worker].finished = 1;
    }
    std::ostringstream good;
    scheduler_measure_write_json(good, plan.header, records);
    assert(good.str().find("hbg-resident-measure-v1") != std::string::npos);
    assert(good.str().find("\"run_epoch\":42") != std::string::npos);
    assert(good.str().find("\"min\":0") != std::string::npos);
    records[1].finished = 0;
    rejects([&] {
        std::ostringstream out;
        scheduler_measure_write_json(out, plan.header, records);
    });
    records[1].finished = 1;
    records[1].run_epoch = 41;
    rejects([&] {
        std::ostringstream out;
        scheduler_measure_write_json(out, plan.header, records);
    });
    records[1].run_epoch = 42;
    records[1].worker_id = 0;
    rejects([&] {
        std::ostringstream out;
        scheduler_measure_write_json(out, plan.header, records);
    });
    records[1].worker_id = 1;
    records[1].overflow = 1;
    rejects([&] {
        std::ostringstream out;
        scheduler_measure_write_json(out, plan.header, records);
    });
}
