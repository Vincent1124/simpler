#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/../.."
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
base=0dc618f2b3581242f6b4a51bbfb36482ee249745
cache_base=b4b0cefd09b7623946c657a0db4de64f4f4b5267
ready=src/a5/runtime/host_build_graph/runtime/scheduler/scheduler_ready.h
lib=build/lib/a5/onboard/host_build_graph
output=${1:-build/batch03-ready-ab}
[[ "$output" == build/batch03-ready-ab || "$output" == build/batch03-ready-selective || "$output" == build/batch04-ready-retry ]]
test "$(git diff --name-only "$base" HEAD -- src python simpler_setup tests/st/a5)" = "$ready"
git diff --quiet -- src python simpler_setup tests/st/a5
printf '%s  %s\n' 2262046192cbf0bb7057cdf786ce2099efc24f1eccd4e45624fce442de0efb96 "$lib/aicore_kernel.o" | sha256sum -c -
mkdir "$output"
cp -- "$lib/aicore_kernel.o" "$output/A.o"
cpus="$(python -c 'import os; print(",".join(map(str, sorted(os.sched_getaffinity(0))[:2])))')"
taskset -pc "$cpus" $$
renice 10 -p $$
cache="build/cache-batch02-$cache_base/a5/onboard/host_build_graph/aicore"
cmake --build "$cache" --parallel 2
cp -- "$cache/aicore_kernel.o" "$output/B.o"
python -m pip install --no-build-isolation -e . --config-settings=build.targets=_task_interface --config-settings=build.tool-args=-j2
python - "$output" "$lib" "$base" <<'PY'
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import simpler.task_interface

output, lib = map(Path, sys.argv[1:3])
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

artifacts = {arm: digest(output / f"{arm}.o") for arm in ("A", "B")}
assert artifacts["A"] != artifacts["B"]
previous = Path("PerformanceTest/results/20261106-lowoverhead-batch02-0dc618f2b/manifest.json")
shared = {p: sha for p, sha in json.loads(previous.read_text())["artifacts"].items()
          if Path(p) != lib / "aicore_kernel.o"}
assert shared and all(digest(Path(p)) == sha for p, sha in shared.items())
record = {"source_sha": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
          "baseline_sha": sys.argv[3], "aicore": artifacts, "shared": shared}
(output / "manifest.json").write_text(json.dumps(record, indent=2) + "\n")
PY
