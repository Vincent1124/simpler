# Copyright (c) PyPTO Contributors.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

"""Run B0 only after measurement validity has passed; stop on invalid evidence."""

import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time


CASES = (
    ("chain-aic", "single_core_dag/test_single_core_dag.py", "TestHbgSingleCoreDag", "aic_chain_64"),
    ("chain-aiv", "single_core_dag/test_single_core_dag.py", "TestHbgSingleCoreDag", "aiv_chain_64"),
    ("fanout256", "fanout_imbalance/test_fanout_imbalance.py", "TestFanoutImbalance", "fanout_256"),
    ("spmd-aic8", "spmd_multiblock_aic/test_spmd_multiblock_aic.py", "TestSpmdMultiblockAic", "aic_block_num_8"),
    ("mix7", "single_block_mix/test_single_block_mix.py", "TestSingleBlockMixHostBuildGraphA5", "burst16_mask7"),
)


def parse_timing(text):
    """Reject missing/duplicated rounds and exclude only a zero-valued Round 0."""
    assert any("Round" in line and "Host (us)" in line and "Device (us)" in line for line in text.splitlines())
    parsed = [(int(m[1]), float(m[2]), float(m[3])) for m in
              re.finditer(r"^\s*(\d+)\s+([\d.]+)\s+([\d.]+)\s*$", text, re.M)]
    assert all(h == 0 and d == 0 for n, h, d in parsed if n == 0)
    measured = [(n, h, d) for n, h, d in parsed if n != 0]
    assert [n for n, _, _ in measured] == list(range(1, 56)), "Expected rounds 1..55 exactly once"
    assert all(math.isfinite(h) and math.isfinite(d) and h > 0 and d > 0 for _, h, d in measured)
    return measured


def describe(values):
    values = sorted(values)
    return {"n": len(values), "median": statistics.median(values),
            "p95": values[math.ceil(.95 * len(values)) - 1]}


def validate_records(directory, mode, case=None):
    files = list(directory.glob("resident-*.json"))
    assert len(files) == (0 if mode == "off" else 55), (mode, len(files))
    if not files:
        return None
    files.sort(key=lambda p: int(p.stem.rsplit("-", 2)[1]))
    assert len({p.stem.split("-")[1] for p in files}) == 1, "Unexpected multiple exporting processes"
    sequences = [int(p.stem.rsplit("-", 2)[1]) for p in files]
    assert sequences == list(range(sequences[0], sequences[0] + 55))
    totals = {}
    epochs = []
    for index, path in enumerate(files):
        data = json.loads(path.read_text())
        assert data["schema"] == "hbg-resident-measure-v1"
        assert data["mode"] == (1 if mode == "count" else 2)
        assert data["period"] == (256 if mode == "sample256" else 64)
        assert data["timer_unit"] == "cycles"
        assert len(data["workers"]) == data["worker_count"] > 0
        epochs.append(data["run_epoch"])
        identities = set()
        for wid, worker in enumerate(data["workers"]):
            assert worker["worker_id"] == wid
            assert worker["core_type"] in (0, 1)
            assert worker["cluster_lane"] in ((0,) if worker["core_type"] == 0 else (1, 2))
            identities.add((worker["cluster_index"], worker["cluster_lane"]))
            assert len(worker["phases"]) == 2
            for phase in worker["phases"]:
                assert phase["admission_attempt"] == phase["admission_failure"] + phase["admission_success"]
                assert phase["publish_entries"] == phase["reserve_entries"]
            if mode == "count":
                assert all(timer["count"] == 0 for timer in worker["timers"].values())
            if index < 5:
                continue
            if wid not in totals:
                totals[wid] = {key: worker[key] for key in ("worker_id", "core_type", "cluster_index", "cluster_lane", "type_rank")}
                totals[wid].update(phases=[{}, {}], timers={})
            target = totals[wid]
            for phase_id, phase in enumerate(worker["phases"]):
                for key, value in phase.items():
                    target["phases"][phase_id][key] = target["phases"][phase_id].get(key, 0) + value
            for name, timer in worker["timers"].items():
                assert sum(timer["buckets"]) == timer["count"]
                combined = target["timers"].setdefault(name, {"count": 0, "sum": 0, "minimum": None, "maximum": None, "buckets": [0] * 4})
                combined["count"] += timer["count"]
                combined["sum"] += timer["sum"]
                if timer["count"]:
                    combined["minimum"] = timer["min"] if combined["minimum"] is None else min(combined["minimum"], timer["min"])
                    combined["maximum"] = timer["max"] if combined["maximum"] is None else max(combined["maximum"], timer["max"])
                combined["buckets"] = [a + b for a, b in zip(combined["buckets"], timer["buckets"])]
        assert len(identities) == data["worker_count"]
        for core_type in (0, 1):
            ranks = sorted(w["type_rank"] for w in data["workers"] if w["core_type"] == core_type)
            assert ranks == list(range(len(ranks)))
        if case in ("chain-aic", "chain-aiv"):
            assert sum(w["phases"][0]["reserve_entries"] for w in data["workers"]) == 1
            if mode.startswith("sample"):
                assert sum(t["count"] for w in data["workers"] for t in w["timers"].values()) > 0
    return {"rounds": 50, "aggregation": "sum over retained rounds; minima/maxima over sampled events",
            "epochs": epochs, "workers": list(totals.values())}


def main():
    repo = Path(__file__).resolve().parents[2]
    os.chdir(repo)
    batch = Path(sys.argv[1]).resolve()
    assert batch.is_relative_to(repo / "PerformanceTest/results") and "-batch02-" in batch.name
    assert (batch / "PREPARED").is_file() and (batch / "VALIDATED").is_file()
    period = int(sys.argv[2])
    assert period in (64, 256)
    device = os.environ["TASK_DEVICE"]
    assert device.isdigit()
    sha = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    assert json.loads((batch / "manifest.json").read_text())["source_sha"] == sha
    subprocess.run(["git", "diff", "--quiet", "--", "src", "python", "simpler_setup", "tests/st/a5"], check=True)
    output = batch / f"b0-sample{period}"
    output.mkdir(exist_ok=False)
    plan = []
    for label, path, cls, selection in CASES:
        for contrast in ("count", f"sample{period}") if period == 64 else (f"sample{period}",):
            for repeat in range(2):
                for slot, mode in enumerate(("off", contrast, contrast, "off")):
                    plan.append(dict(case=label, path=path, cls=cls, selection=selection, contrast=contrast,
                                     repeat=repeat + 1, slot=slot + 1, mode=mode))
    artifacts = json.loads((batch / "manifest.json").read_text())["artifacts"]
    for path, digest in artifacts.items():
        assert hashlib.sha256((repo / path).read_bytes()).hexdigest() == digest, path
    (output / "plan.json").write_text(json.dumps({"source_sha": sha, "device": device, "rounds": 55, "discarded": 5,
                                                 "scripts": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in
                                                             (Path(__file__), Path(__file__).with_suffix('.sh'))},
                                                 "plan": plan}, indent=2) + "\n")
    summaries = []
    for index, item in enumerate(plan):
        name = f"{index + 1:03d}-{item['case']}-{item['mode']}"
        log = output / f"{name}.log"
        stats = output / name
        stats.mkdir()
        env = dict(os.environ, SIMPLER_RESIDENT_PERF=item["mode"], SIMPLER_RESIDENT_PERF_DIR=str(stats),
                   SIMPLER_REPORT_SCHEDULER_MODE="1", ASCEND_PROCESS_LOG_PATH=str(batch / "ascend-b0"))
        command = [sys.executable, "-m", "pytest", f"tests/st/a5/host_build_graph/{item['path']}::{item['cls']}",
                   "--case", f"{item['cls']}::{item['selection']}", "--manual", "include", "--platform", "a5",
                   "--device", device, "--rounds", "55", "--enable-chip-swimlane", "0", "-vv", "-s"]
        print(f"B0 START {index + 1}/{len(plan)} {name}", flush=True)
        started = time.time()
        with log.open("w") as stream:
            result = subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT)
        entry = dict(item, name=name, returncode=result.returncode, elapsed_s=time.time() - started)
        with (output / "attempts.jsonl").open("a") as stream:
            stream.write(json.dumps(entry) + "\n")
        assert result.returncode == 0, f"Failed test; inspect {log}"
        assert "mode=1 (resident (pending)) published=55" in log.read_text(errors="replace")
        timing = subprocess.check_output([sys.executable, "-m", "simpler_setup.tools.strace_timing", str(log), "--rounds-table"], text=True)
        (output / f"{name}-timing.txt").write_text(timing)
        rows = parse_timing(timing)
        entry.update(host_us=describe([row[1] for row in rows[5:]]), device_us=describe([row[2] for row in rows[5:]]))
        aggregate = validate_records(stats, item["mode"], item["case"])
        if aggregate:
            (output / f"{name}-workers.json").write_text(json.dumps(aggregate) + "\n")
        summaries.append(entry)
        (output / "process-summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
        print(f"B0 PASS {name} device_median_us={entry['device_us']['median']:.3f}", flush=True)
    calibration = []
    for label, *_ in CASES:
        for contrast in ("count", f"sample{period}") if period == 64 else (f"sample{period}",):
            selected = [row for row in summaries if row["case"] == label and row["contrast"] == contrast]
            off = [row["device_us"]["median"] for row in selected if row["mode"] == "off"]
            on = [row["device_us"]["median"] for row in selected if row["mode"] == contrast]
            reference = statistics.median(off)
            variation = (max(off) - min(off)) / reference * 100
            delta = (statistics.median(on) / reference - 1) * 100
            budget = max(2, variation)
            calibration.append(dict(case=label, mode=contrast, off_medians_us=off, on_medians_us=on,
                                    delta_pct=delta, off_spread_pct=variation, budget_pct=budget,
                                    eligible=abs(delta) <= budget))
    (output / "calibration.json").write_text(json.dumps(calibration, indent=2) + "\n")
    (output / "COMPLETED").write_text(time.strftime("%Y-%m-%dT%H:%M:%S%z") + "\n")
    print(json.dumps(calibration, indent=2), flush=True)


if __name__ == "__main__":
    main()
