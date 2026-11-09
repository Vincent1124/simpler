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

#include "orchestration_api.h"  // NOLINT(build/include_subdir)

extern "C" {
__attribute__((visibility("default"))) OrchestrationConfig aicpu_orchestration_config(const ChipTaskArgs &) {
    return OrchestrationConfig{.expected_arg_count = 5};
}

__attribute__((visibility("default"))) void aicpu_orchestration_entry(const ChipTaskArgs &input) {
    const int64_t tasks = input.scalar<int64_t>(0);
    const int64_t kind = input.scalar<int64_t>(1);
    const int64_t mask = input.scalar<int64_t>(2);
    const int64_t total = input.scalar<int64_t>(3);
    if (tasks < 1 || tasks > 256 || total != 256 || total % tasks != 0 || kind < 0 || kind > 2 || mask < 1 ||
        mask > 7) {
        rt_report_fatal(SIMPLER_ERROR_INVALID_ARGS, "invalid admission measurement configuration");
        return;
    }
    const int64_t blocks = total / tasks;
    for (int64_t task = 0; task < tasks; ++task) {
        CoreTaskArgs args;
        args.add_no_dep(input.tensor(0).ref());
        args.add_scalar(task * blocks);
        args.launch_spec.set_block_num(static_cast<int32_t>(blocks));
        if (kind == 0) rt_submit_aic_task(0, args);
        else if (kind == 1) rt_submit_aiv_task(1, args);
        else {
            MixedKernels kernels{};
            kernels.aic_kernel_id = (mask & 1) != 0 ? 0 : INVALID_KERNEL_ID;
            kernels.aiv0_kernel_id = (mask & 2) != 0 ? 1 : INVALID_KERNEL_ID;
            kernels.aiv1_kernel_id = (mask & 4) != 0 ? 2 : INVALID_KERNEL_ID;
            rt_submit_task(kernels, args);
        }
    }
}
}
