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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "scheduler/scheduler_measurement.h"

struct SchedulerMeasureConfig {
    uint64_t mode{0};
    uint64_t period{64};
    std::string directory;
};

inline SchedulerMeasureConfig scheduler_measure_config(const char *mode, const char *directory) {
    SchedulerMeasureConfig result;
    if (mode == nullptr || std::string(mode) == "off") return result;
    const std::string value(mode);
    if (value == "count") result.mode = 1;
    else if (value == "sample64") result.mode = 2;
    else if (value == "sample256") {
        result.mode = 2;
        result.period = 256;
    } else {
        throw std::invalid_argument("SIMPLER_RESIDENT_PERF must be off, count, sample64 or sample256");
    }
    if (directory == nullptr || *directory == '\0')
        throw std::invalid_argument("SIMPLER_RESIDENT_PERF_DIR is required when resident measurement is enabled");
    result.directory = directory;
    return result;
}

struct SchedulerMeasurePlan {
    uint64_t offset{0};
    uint64_t total_size{0};
    SchedulerMeasureHeader header{};
};

inline bool scheduler_measure_plan(
    uint64_t start, uint64_t workers, uint64_t epoch, const SchedulerMeasureConfig &config, SchedulerMeasurePlan *plan
) {
    if (plan == nullptr) return false;
    *plan = {};
    plan->total_size = start;
    if (config.mode == 0) return true;
    if ((config.mode != 1 && config.mode != 2) || (config.period != 64 && config.period != 256) || workers == 0 ||
        workers > UINT16_MAX || start == 0 || start > UINT64_MAX - 127)
        return false;
    const uint64_t aligned = (start + 127) & ~UINT64_C(127);
    const uint64_t bytes = sizeof(SchedulerMeasureHeader) + workers * sizeof(SchedulerMeasureRecord);
    if (aligned > UINT64_MAX - bytes) return false;
    plan->offset = aligned;
    plan->total_size = aligned + bytes;
    plan->header.mode = config.mode;
    plan->header.period = config.period;
    plan->header.run_epoch = epoch;
    plan->header.worker_count = workers;
    plan->header.records_offset = aligned + sizeof(SchedulerMeasureHeader);
    plan->header.region_bytes = bytes;
    return true;
}

inline void scheduler_measure_write_json(
    std::ostream &out, const SchedulerMeasureHeader &header, const std::vector<SchedulerMeasureRecord> &records
) {
    if (header.version != 1 || records.size() != header.worker_count ||
        header.record_bytes != sizeof(SchedulerMeasureRecord))
        throw std::invalid_argument("resident measurement layout mismatch");
    static constexpr const char *names[] = {
        "reserve_calls", "reserve_entries",   "publish_calls",     "publish_entries",   "probe_own",
        "probe_remote",  "pop_success",       "pop_not_ready",     "pop_retry",         "pop_cas_failure",
        "mix_unplaced",  "admission_attempt", "admission_failure", "admission_success",
    };
    static constexpr const char *timers[] = {"reserve", "publish", "pop", "commit"};
    static_assert(sizeof(names) / sizeof(*names) == SCHEDULER_MEASURE_COUNTERS);
    static_assert(sizeof(timers) / sizeof(*timers) == SCHEDULER_MEASURE_TIMERS);
    out << "{\"schema\":\"hbg-resident-measure-v1\",\"run_epoch\":" << header.run_epoch << ",\"mode\":" << header.mode
        << ",\"period\":" << header.period << ",\"timer_unit\":\"cycles\",\"worker_count\":" << header.worker_count
        << ",\"workers\":[";
    for (size_t worker = 0; worker < records.size(); ++worker) {
        const auto &record = records[worker];
        if (record.worker_id != worker || record.run_epoch != header.run_epoch || record.finished != 1 ||
            record.overflow)
            throw std::invalid_argument(
                "resident measurement invalid worker=" + std::to_string(worker) +
                " observed_worker=" + std::to_string(record.worker_id) + " epoch=" + std::to_string(record.run_epoch) +
                " expected_epoch=" + std::to_string(header.run_epoch) + " finished=" + std::to_string(record.finished) +
                " overflow=" + std::to_string(record.overflow)
            );
        if (worker != 0) out << ',';
        out << "{\"worker_id\":" << worker << ",\"core_type\":" << record.core_type
            << ",\"cluster_index\":" << record.cluster_index << ",\"cluster_lane\":" << record.cluster_lane
            << ",\"type_rank\":" << record.type_rank << ",\"phases\":[";
        for (uint32_t phase = 0; phase < 2; ++phase) {
            if (phase != 0) out << ',';
            out << '{';
            for (uint32_t field = 0; field < SCHEDULER_MEASURE_COUNTERS; ++field) {
                if (field != 0) out << ',';
                out << '"' << names[field] << "\":" << record.counters[phase][field];
            }
            out << '}';
        }
        out << "],\"timers\":{";
        for (uint32_t field = 0; field < SCHEDULER_MEASURE_TIMERS; ++field) {
            if (field != 0) out << ',';
            const auto &sample = record.timers[field];
            out << '"' << timers[field] << "\":{\"count\":" << sample.count << ",\"sum\":" << sample.sum
                << ",\"min\":" << (sample.count == 0 ? 0 : sample.minimum) << ",\"max\":" << sample.maximum
                << ",\"buckets\":[";
            for (uint32_t bucket = 0; bucket < 4; ++bucket) {
                if (bucket != 0) out << ',';
                out << sample.buckets[bucket];
            }
            out << "]}";
        }
        out << "}}";
    }
    out << "]}\n";
    if (!out) throw std::runtime_error("resident measurement write failed");
}
