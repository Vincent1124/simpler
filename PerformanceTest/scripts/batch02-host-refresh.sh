#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
base_sha=14ffa41b9230af2eeb0ff5f77b916773046ca5c1
cache_sha=b4b0cefd09b7623946c657a0db4de64f4f4b5267
python - "$base_sha" <<'PY'
import subprocess
import sys
changed = set(subprocess.check_output(['git', 'diff', '--name-only', sys.argv[1], 'HEAD', '--', 'src'], text=True).splitlines())
allowed = {'src/a5/runtime/host_build_graph/host/scheduler_measurement_export.h',
           'src/a5/runtime/host_build_graph/host/runtime_maker.cpp'}
assert changed <= allowed, changed
PY
git diff --quiet -- src python simpler_setup
batch="${1:?Pass a new PerformanceTest/results batch02 directory}"
[[ "$batch" == PerformanceTest/results/*-batch02-* ]] || exit 2
mkdir -p "$batch"
batch="$(realpath "$batch")"
[[ "$batch" == "$repo/PerformanceTest/results/"* ]] || exit 2
test ! -e "$batch/build.log"
exec > >(tee "$batch/build.log") 2>&1
export BATCH02_DIR="$batch"
cpus="$(python -c 'import os; print(",".join(str(cpu) for cpu in sorted(os.sched_getaffinity(0))[:2]))')"
taskset -pc "$cpus" $$
renice 10 -p $$
cache="$repo/build/cache-batch02-$cache_sha/a5/onboard/host_build_graph/host"
test -f "$cache/CMakeCache.txt"
cmake --build "$cache" --parallel 2
mapfile -t libraries < <(find "$cache" -maxdepth 2 -type f -name libhost_runtime.so)
test "${#libraries[@]}" -eq 1
cp -- "${libraries[0]}" build/lib/a5/onboard/host_build_graph/libhost_runtime.so
python -m pip install --no-build-isolation -e . --config-settings=build.targets=_task_interface --config-settings=build.tool-args=-j2
python - <<'PY'
import hashlib
import json
import os
import subprocess
from pathlib import Path
import simpler.task_interface
root = Path.cwd()
batch = Path(os.environ['BATCH02_DIR'])
artifacts = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in (root / 'build/lib/a5/onboard/host_build_graph').iterdir() if p.is_file()}
record = {'schema': 'resident-batch02-build-v1',
          'source_sha': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
          'aicore_aicpu_source_sha': '14ffa41b9230af2eeb0ff5f77b916773046ca5c1',
          'runtime_delta': 'host diagnostic or binding metadata refresh; AICore unchanged',
          'pto_pin': (root / 'pto_isa.pin').read_text().strip(),
          'artifacts': artifacts, 'import': 'PASS', 'device_allocation': 'none'}
(batch / 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')
PY
date -Iseconds > "$batch/PREPARED"
