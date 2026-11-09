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

#include "scheduler_ready.h"
#include "ready_pool_plan.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
constexpr uint32_t kTaskCount = 128;
constexpr uint32_t kClusters = 16;
constexpr uint32_t kWidth = 8;

void check(const char *name, const std::vector<int64_t> &refs) {
    std::vector<SchedulerTaskMetadata> metadata(kTaskCount);
    for (auto &task : metadata) {
        task.flags = SCHEDULER_TASK_EXECUTABLE;
        task.active_mask = LaneMask{true, false, false};
        task.logical_block_num = 1;
    }
    AicoreSchedulerLayout layout{};
    SchedulerReadyPoolPlan plan{};
    assert(scheduler_plan_layout(kTaskCount, kTaskCount, 0, &layout));
    assert(scheduler_plan_ready_pool(metadata, kClusters, kWidth, layout.total_size, &plan));
    layout.total_size = plan.total_size;
    alignas(SCHEDULER_STATE_ALIGNMENT) static unsigned char memory[1024 * 1024];
    assert(layout.total_size <= sizeof(memory));
    assert(scheduler_init_data_from_layout(memory, layout, reinterpret_cast<uint64_t>(memory)));
    scheduler_init_ready_pool(memory, plan);
    std::copy(metadata.begin(), metadata.end(),
              scheduler_state_at<SchedulerTaskMetadata>(memory, layout.task_metadata_offset));
    auto graph = scheduler_ordinary_graph_view(0, kTaskCount, kTaskCount - 1);
    SchedulerLocalState local{};
    local.config.graph = &graph;
    local.config.task_controls_offset = layout.task_controls_offset;
    local.config.task_metadata_offset = layout.task_metadata_offset;
    local.config.ready_pool_offset = plan.header_offset;
    SchedulerReadyBatch batch{};
    SchedulerReadyStats stats{};
    for (const auto ref : refs)
        assert(scheduler_ready_batch_append(memory, &local, ref, &batch, nullptr));
    assert(scheduler_ready_pool_push_batch(memory, &local, 0, &batch, &stats));
    assert(batch.count == 0);
    for (uint32_t group = 0; group < 2; ++group) {
        const auto queue = scheduler_ready_pool_queue(memory, plan.header, plan.header.queue_begins[0] + group);
        uint64_t index = 0;
        for (const auto ref : refs) {
            if (scheduler_ready_pool_ref_group(ref, 2) == group)
                assert(queue.entries[index++] == ref);
        }
        assert(queue.control->reserved_tail == index);
    }
    std::printf("%s entries=%zu reserves=%llu\n", name, refs.size(),
                static_cast<unsigned long long>(stats.batch_count));
}
}  // namespace

int main() {
    std::vector<int64_t> groups[2];
    for (uint32_t ref = 0; ref < kTaskCount; ++ref)
        groups[scheduler_ready_pool_ref_group(ref, 2)].push_back(ref);
    assert(groups[0].size() >= 10 && groups[1].size() >= 5);
    std::vector<int64_t> alternating;
    for (uint32_t i = 0; i < 5; ++i) {
        alternating.push_back(groups[0][i]);
        alternating.push_back(groups[1][i]);
    }
    check("alternating", alternating);
    check("short-alternating", {alternating.begin(), alternating.begin() + 4});
    check("same-group", {groups[0].begin(), groups[0].begin() + 10});
    check("singleton", {groups[0][0]});
}
