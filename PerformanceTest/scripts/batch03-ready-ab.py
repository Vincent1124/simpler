# Copyright (c) PyPTO Contributors.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.


"""Run same-device Ready publication A/B with separate AICore artifacts."""

from dataclasses import replace
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys

from importlib import import_module

calibration = import_module("batch02-calibrate")
CASES = {case[0]: case[1:] for case in calibration.CASES}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def scene(binary, case, rounds, device):
    import pytest
    from simpler_setup.runtime_builder import RuntimeBuilder

    original = RuntimeBuilder.get_binaries
    used = []

    def select(builder, name, *args, **kwargs):
        binaries = original(builder, name, *args, **kwargs)
        if name == "host_build_graph":
            used.append(binary)
            print(f"BATCH03_AICORE {digest(binary)}", flush=True)
            return replace(binaries, aicore_path=binary)
        return binaries

    RuntimeBuilder.get_binaries = select
    path, cls, selection = CASES[case]
    status = pytest.main([
        f"tests/st/a5/host_build_graph/{path}::{cls}", "--case", f"{cls}::{selection}",
        "--manual", "include", "--platform", "a5", "--device", device, "--rounds", rounds,
        "--enable-chip-swimlane", "0", "-vv", "-s",
    ])
    return int(status) or int(not used)


def main(artifact_dir, output, device):
    artifact_dir = Path(artifact_dir).resolve()
    output = Path(output).resolve()
    repo = Path(__file__).resolve().parents[2]
    assert output.is_relative_to(repo / "PerformanceTest/results") and "-batch03-" in output.name
    assert device.isdecimal()
    output.mkdir()
    (output / "ascend").mkdir()
    manifest = json.loads((artifact_dir / "manifest.json").read_text())
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    assert head == manifest["source_sha"]
    subprocess.run(["git", "diff", "--quiet", "--", "src", "python", "simpler_setup", "tests/st/a5"], check=True)
    for path, sha in manifest["shared"].items():
        assert digest(Path(path)) == sha, path
    for arm, sha in manifest["aicore"].items():
        assert digest(artifact_dir / f"{arm}.o") == sha, arm
    plan = [("smoke", case, arm, "off", 2)
            for case in ("fanout256", "chain-aic", "chain-aiv", "spmd-aic8", "mix7")
            for arm in ("A", "B")]
    plan += [("timing", case, arm, "off", 55)
             for case in ("fanout256", "chain-aic", "mix7")
             for arm in ("A", "B", "B", "A", "B", "A", "A", "B")]
    plan += [("count", "fanout256", arm, "count", 55) for arm in ("A", "B", "B", "A")]
    (output / "plan.json").write_text(json.dumps(
        {"device": device, "runner_sha256": digest(Path(__file__)), "manifest": manifest, "runs": plan},
        indent=2) + "\n")
    records = []
    for index, (phase, case, arm, mode, rounds) in enumerate(plan, 1):
        label = f"{index:02d}-{phase}-{case}-{arm}"
        print(f"BATCH03 {index}/{len(plan)} {label}", flush=True)
        raw = output / label
        raw.mkdir()
        env = dict(os.environ, SIMPLER_RESIDENT_PERF=mode, SIMPLER_RESIDENT_PERF_DIR=str(raw),
                   SIMPLER_REPORT_SCHEDULER_MODE="1", ASCEND_PROCESS_LOG_PATH=str(output / "ascend"))
        log = output / f"{label}.log"
        binary = artifact_dir / f"{arm}.o"
        with log.open("w") as stream:
            result = subprocess.run([sys.executable, __file__, "scene", str(binary), case, str(rounds), device],
                                    env=env, stdout=stream, stderr=subprocess.STDOUT)
        assert result.returncode == 0, log
        content = log.read_text(errors="replace")
        assert f"BATCH03_AICORE {manifest['aicore'][arm]}" in content, log
        assert f"mode=1 (resident (pending)) published={rounds}" in content, log
        record = dict(phase=phase, case=case, arm=arm, mode=mode, label=label)
        if phase == "timing":
            timing = subprocess.check_output(
                [sys.executable, "-m", "simpler_setup.tools.strace_timing", str(log), "--rounds-table"], text=True)
            (output / f"{label}-timing.txt").write_text(timing)
            rows = calibration.parse_timing(timing)[5:]
            record["device_us"] = calibration.describe([row[2] for row in rows])
            record["host_us"] = calibration.describe([row[1] for row in rows])
        elif phase == "count":
            aggregate = calibration.validate_records(raw, mode, case)
            (output / f"{label}-workers.json").write_text(json.dumps(aggregate) + "\n")
            record["running"] = {
                key: sum(w["phases"][1][key] for w in aggregate["workers"]) / aggregate["rounds"]
                for key in ("reserve_calls", "reserve_entries", "publish_calls", "publish_entries", "pop_cas_failure")
            }
        records.append(record)
        (output / "process-summary.json").write_text(json.dumps(records, indent=2) + "\n")
    summary = {}
    for case in ("fanout256", "chain-aic", "mix7"):
        arms = {arm: [x["device_us"]["median"] for x in records
                      if x["phase"] == "timing" and x["case"] == case and x["arm"] == arm]
                for arm in ("A", "B")}
        base = statistics.median(arms["A"])
        summary[case] = dict(
            medians_us=arms, delta_pct=(statistics.median(arms["B"]) / base - 1) * 100,
            a_spread_pct=(max(arms["A"]) - min(arms["A"])) / base * 100,
        )
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    (output / "COMPLETED").touch(exist_ok=False)


if __name__ == "__main__":
    if sys.argv[1] == "scene":
        raise SystemExit(scene(Path(sys.argv[2]), *sys.argv[3:]))
    main(*sys.argv[1:])
