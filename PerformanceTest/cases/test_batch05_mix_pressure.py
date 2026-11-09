"""Fixed-block Mix admission pressure cases."""
import pytest
import torch
from simpler.task_interface import ArgDirection as D
from simpler_setup import Scalar, SceneTestCase, TaskArgsBuilder, TensorArg, scene_test


@pytest.mark.scheduler_mode("resident")
@scene_test(level=2, runtime="host_build_graph")
class TestBatch05MixPressure(SceneTestCase):
    RTOL = ATOL = 0
    CALLABLE = {
        "orchestration": {"source": "kernels/admission_orch.cpp",
                          "function_name": "aicpu_orchestration_entry", "signature": [D.INOUT]},
        "incores": [{"func_id": i, "source": f"kernels/stamp_{i}.cpp", "core_type": core,
                     "signature": [D.INOUT]}
                    for i, core in enumerate(("aic", "aiv", "aiv"))],
    }
    CASES = [{"name": f"tasks{tasks}", "platforms": ["a5"], "params": {"tasks": tasks}}
             for tasks in (1, 4, 16, 64, 256)]

    def generate_args(self, params):
        return TaskArgsBuilder(TensorArg("state", torch.zeros(256 * 48, dtype=torch.int64)),
                               Scalar("tasks", params["tasks"]), Scalar("kind", 2),
                               Scalar("mask", 7), Scalar("blocks", 256))

    def compute_golden(self, args, params):
        state = args.state.reshape(256, 48)
        for lane in range(3):
            state[:, lane * 16] = torch.arange(1, 257, dtype=torch.int64)
            state[:, lane * 16 + 1] = 256 // params["tasks"]
            state[:, lane * 16 + 2] = lane
