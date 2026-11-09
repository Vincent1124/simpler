"""A5 sync-start cohort benchmark using the runtime's TICKET pop primitive."""
import functools
import json
from pathlib import Path

import pytest
import torch
from simpler.task_interface import ArgDirection, CallConfig, ChipCallable, CoreCallable, DataType, TaskArgs, TensorArgType
from simpler.worker import Worker
from simpler_setup.elf_parser import extract_text_section
from simpler_setup.kernel_compiler import KernelCompiler
from simpler_setup.pto_isa import ensure_pto_isa_root

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RUNTIME = "host_build_graph"
ENTRIES = 4096
ENTRY = 544
SEEN = ENTRY + ENTRIES
STATS = SEEN + ENTRIES


@functools.cache
def build(platform):
    compiler = KernelCompiler(platform=platform)
    extra = [str(ROOT / path) for path in (
        "src/a5/runtime/host_build_graph/runtime", "src/a5/runtime/host_build_graph/common",
        "src/a5/platform/include", "src/a5/platform/onboard", "src/a5/platform/onboard/aicore",
        "src/common/platform/include", "src/common/task_interface", "src/common/log/include", "src/common",
    )]
    children = []
    for i, core in enumerate(("aic", "aiv")):
        blob = compiler.compile_incore(source_path=str(HERE / "kernels/ready_claim_a5.cpp"), core_type=core,
                                       pto_isa_root=ensure_pto_isa_root(), extra_include_dirs=extra)
        children.append((i, CoreCallable.build(signature=[ArgDirection.INOUT], binary=extract_text_section(blob))))
    orchestration = compiler.compile_orchestration(
        runtime_name=RUNTIME, source_path=str(HERE / "kernels/ready_claim_a5_orch.cpp"))
    return ChipCallable.build(signature=[ArgDirection.INOUT], func_name="aicpu_orchestration_entry",
                              binary=orchestration, children=children)


def measure(worker, handle, core, workers, mode, diagnostic):
    host = torch.zeros(STATS + 16 * workers, dtype=torch.int64)
    groups = 1 if mode == 0 else workers if mode == 1 else 4
    width = ENTRIES if mode == 3 else ENTRIES // groups
    for q in range(1 if mode == 3 else groups):
        host[32 + 32 * q + 16] = width
    host[ENTRY:SEEN] = torch.arange(ENTRIES, dtype=torch.int64)
    buffer = worker.malloc(host.nbytes)
    try:
        worker.copy_to(buffer, host)
        args = TaskArgs()
        args.add_tensor(buffer.tensor(shapes=(host.numel(),), dtype=DataType.INT64), TensorArgType.INOUT)
        for scalar in (workers, core, mode, int(diagnostic)):
            args.add_scalar(scalar)
        worker.run(handle, args, CallConfig())
        worker.copy_from(host, buffer)
    finally:
        worker.free(buffer)
    values = host[STATS:].reshape(workers, 16)
    assert int(host[0]) == workers and torch.all(values[:, 0] == 0), values.tolist()
    assert torch.all(values[:, 1] == values[0, 1]) and int(values[0, 1]) > 0
    assert int(values[:, 3].sum()) == ENTRIES and int(values[:, 4].sum()) == ENTRIES * (ENTRIES - 1) // 2
    for q in range(groups):
        cap = 0 if mode == 3 and q else width
        assert int(host[32 + 32 * q]) == cap
    if diagnostic:
        assert torch.all(host[SEEN:STATS] == 1)
    elapsed = int(values[:, 2].max() - values[0, 1])
    assert elapsed > 0
    return dict(elapsed_ticks=elapsed, claimed=values[:, 3].tolist(),
                cas_failures=int(values[:, 5].sum()), live_own=int(values[:, 6].sum()),
                live_remote=int(values[:, 7].sum()), tail_own=int(values[:, 8].sum()),
                tail_remote=int(values[:, 9].sum()), unknown=int(values[:, 10].sum()))


@pytest.mark.platforms(["a5"])
@pytest.mark.device_count(1)
@pytest.mark.runtime(RUNTIME)
@pytest.mark.parametrize("core", [0, 1], ids=["aic", "aiv"])
def test_ready_claim(st_platform, st_device_ids, core):
    worker = Worker(level=2, platform=st_platform, runtime=RUNTIME, device_id=int(st_device_ids[0]))
    try:
        handle = worker.register(build(st_platform))
        worker.init()
        # One checked warm-up per shape, followed by alternated timing and diagnostic rounds.
        matrix = [(workers, (0, 1)) for workers in (1, 4, 8, 16)] + [(16, (2, 3))]
        for workers, layouts in matrix:
            for mode in layouts:
                measure(worker, handle, core, workers, mode, True)
            for diagnostic in (False, True):
                for repeat in range(4):
                    for mode in layouts if repeat % 2 == 0 else layouts[::-1]:
                        result = measure(worker, handle, core, workers, mode, diagnostic)
                        assert worker.scheduler_runtime_mode in (1, 2)
                        print("BATCH04_CLAIM " + json.dumps(dict(
                            core=core, workers=workers, mode=mode, diagnostic=diagnostic,
                            repeat=repeat, **result)), flush=True)
    finally:
        worker.close()
