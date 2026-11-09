/*
 * Copyright (c) PyPTO Contributors.
 * Licensed under the CANN Open Software License Agreement Version 2.0.
 * See LICENSE in the root of the software repository.
 */

#include <pto/pto-inst.hpp>
#include "aicore/aicore.h"
#include "intrinsic.h"
#include "scheduler/scheduler_ticket_queue.h"
#include "tensor.h"

namespace {
constexpr uint32_t kEntries = 4096;
constexpr uint32_t kEntryOffset = 544;
constexpr uint32_t kSeenOffset = kEntryOffset + kEntries;
constexpr uint32_t kStatsOffset = kSeenOffset + kEntries;
constexpr uint64_t kTimeout = 100000000;
}

extern "C" __aicore__ void kernel_entry(__gm__ int64_t *args) {
    auto *tensor = reinterpret_cast<__gm__ Tensor *>(args[0]);
    auto *base = reinterpret_cast<__gm__ uint64_t *>(tensor->buffer.addr) + tensor->start_offset;
    const uint32_t rank = get_block_idx(args);
    const uint32_t workers = get_block_num(args);
    const uint32_t mode = args[1];
    const bool diagnostic = args[2] != 0;
    auto *stats = base + kStatsOffset + rank * 16;
    const uint32_t groups = mode == 0 ? 1 : (mode == 1 ? workers : 4);
    const uint32_t own = rank % groups;
    auto *controls = reinterpret_cast<__gm__ SchedulerTicketQueue *>(base + 32);
    const uint64_t deadline = get_sys_cnt_aicore() + kTimeout;
    scheduler_gm_fetch_add(base[0], 1);
    while (scheduler_gm_query(base[0]) != workers) {
        if (get_sys_cnt_aicore() > deadline) {
            st_dev(1, stats, 0);
            return;
        }
    }
    if (rank == 0) st_dev(get_sys_cnt_aicore(), base + 16, 0);
    uint64_t begin = 0;
    while ((begin = scheduler_gm_query(base[16])) == 0) {
        if (get_sys_cnt_aicore() > deadline) {
            st_dev(2, stats, 0);
            return;
        }
    }

    uint64_t total = 0, retries = 0, sum = 0;
    uint64_t live_own = 0, live_remote = 0, tail_own = 0, tail_remote = 0, unknown = 0;
    uint32_t misses = 0, group = own;
    const uint32_t span = mode >= 2 ? groups : 1;
    const uint32_t width = mode == 3 ? kEntries : kEntries / groups;
    uint64_t error = 0;
    uint32_t polls = 0;
    while (misses < span) {
        const uint32_t cap = mode == 3 && group != 0 ? 0 : width;
        const uint32_t offset = mode == 3 ? 0 : group * width;
        const SchedulerTicketQueueView queue{
            controls + group, reinterpret_cast<__gm__ int64_t *>(base + kEntryOffset + offset), cap, kEntries};
        int64_t ref = -1;
        const auto result = scheduler_ticket_try_pop(queue, &ref, 1, diagnostic ? &retries : nullptr);
        if (result == SchedulerTicketResult::SUCCESS) {
            ++total;
            sum += static_cast<uint64_t>(ref);
            misses = 0;
            if (diagnostic && scheduler_gm_compare_exchange(base[kSeenOffset + ref], 0, 1) != 0) {
                error = 3;
                break;
            }
            group = own;
        } else if (result == SchedulerTicketResult::NOT_READY) {
            ++misses;
            if (diagnostic && mode >= 2) {
                bool live = false;
                for (uint32_t q = 0; q < groups; ++q) {
                    const uint32_t qcap = mode == 3 && q != 0 ? 0 : width;
                    if (scheduler_gm_query(controls[q].head) < qcap) live = true;
                }
                if (live) {
                    if (group == own) ++live_own;
                    else ++live_remote;
                } else {
                    // A final claim may have raced the snapshot.
                    if (misses == span) {
                        if (group == own) ++tail_own;
                        else ++tail_remote;
                    } else {
                        ++unknown;
                    }
                }
            }
            group = (group + 1) % groups;
        } else if (result == SchedulerTicketResult::INVALID) {
            error = 4;
            break;
        }
        if ((++polls & 63) == 0 && get_sys_cnt_aicore() - begin > kTimeout) {
            error = 5;
            break;
        }
    }
    const uint64_t end = get_sys_cnt_aicore();
    st_dev(error, stats, 0);
    st_dev(begin, stats + 1, 0);
    st_dev(end, stats + 2, 0);
    st_dev(total, stats + 3, 0);
    st_dev(sum, stats + 4, 0);
    st_dev(retries, stats + 5, 0);
    st_dev(live_own, stats + 6, 0);
    st_dev(live_remote, stats + 7, 0);
    st_dev(tail_own, stats + 8, 0);
    st_dev(tail_remote, stats + 9, 0);
    st_dev(unknown, stats + 10, 0);
    scheduler_cache_barrier();
}
