/*
 * Copyright (c) PyPTO Contributors.
 * Licensed under the CANN Open Software License Agreement Version 2.0.
 * See LICENSE in the root of the software repository.
 */

#include "orchestration_api.h"  // NOLINT(build/include_subdir)

extern "C" {
__attribute__((visibility("default"))) OrchestrationConfig aicpu_orchestration_config(const ChipTaskArgs &) {
    return OrchestrationConfig{.expected_arg_count = 5};
}

__attribute__((visibility("default"))) void aicpu_orchestration_entry(const ChipTaskArgs &input) {
    CoreTaskArgs args;
    args.add_no_dep(input.tensor(0).ref());
    args.add_scalar(input.scalar<int64_t>(2));
    args.add_scalar(input.scalar<int64_t>(3));
    args.launch_spec.set_block_num(static_cast<int32_t>(input.scalar<int64_t>(0)));
    args.launch_spec.set_require_sync_start(true);
    if (input.scalar<int64_t>(1) == 0) rt_submit_aic_task(0, args);
    else rt_submit_aiv_task(1, args);
}
}
