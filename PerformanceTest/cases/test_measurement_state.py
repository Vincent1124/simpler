# Copyright (c) PyPTO Contributors.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

"""Isolate measurement serialization from the resident scheduler's live state."""

from pathlib import Path

import pytest
import torch
from simpler.task_interface import ArgDirection as D
from simpler_setup import Scalar, SceneTestCase, TaskArgsBuilder, TensorArg, scene_test

ROOT = Path(__file__).resolve().parents[2]
EXTRA = [str(ROOT / path) for path in (
    "src/a5/runtime/host_build_graph/runtime", "src/a5/runtime/host_build_graph/common",
    "src/a5/platform/include", "src/a5/platform/onboard", "src/a5/platform/onboard/aicore",
    "src/common/platform/include",
    "src/common/task_interface", "src/common/log/include", "src/common",
)]


@pytest.mark.scheduler_mode("resident")
@scene_test(level=2, runtime="host_build_graph")
class TestMeasurementState(SceneTestCase):
    RTOL = 0
    ATOL = 0
    CALLABLE = {
        "orchestration": {"source": "kernels/measurement_state_orch.cpp",
                          "function_name": "aicpu_orchestration_entry", "signature": [D.INOUT]},
        "incores": [{"func_id": 0, "source": "kernels/measurement_state.cpp", "core_type": "aic",
                     "signature": [D.INOUT], "extra_include_dirs": EXTRA}],
    }
    CASES = [{"name": name, "platforms": ["a5"], "params": {"mode": mode}}
             for name, mode in (("direct", 2), ("plain", 0), ("volatile", 1))]

    def generate_args(self, params):
        return TaskArgsBuilder(TensorArg("state", torch.zeros(80, dtype=torch.int64)), Scalar("mode", params["mode"]))

    def compute_golden(self, args, params):
        values = [101, 3, 1, 0, 1, 7, 2, 5]
        values += [phase * 1000 + counter + 1 for phase in range(2) for counter in range(14)]
        for timer in range(4):
            count = 10 + timer * 4
            values += [count, count * (15 + timer), 10 + timer, 20 + timer,
                       timer + 1, timer + 2, timer + 3, timer + 4]
        args.state[:len(values)] = torch.tensor(values, dtype=torch.int64)
