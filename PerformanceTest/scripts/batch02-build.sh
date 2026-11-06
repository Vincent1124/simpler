#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
test "$(git branch --show-current)" = mdtest
git diff --quiet -- src python simpler_setup
sha="$(git rev-parse HEAD)"
batch="${1:?Pass a new PerformanceTest/results batch directory}"
[[ "$batch" == PerformanceTest/results/*-batch02-* ]] || exit 2
mkdir -p "$batch"
batch="$(realpath "$batch")"
[[ "$batch" == "$repo/PerformanceTest/results/"* ]] || exit 2
test ! -e "$batch/build.log"
exec > >(tee "$batch/build.log") 2>&1
printf 'BATCH02_DIR=%s\nSOURCE=%s\n' "$batch" "$sha"
started="$(date -Iseconds)"
export BATCH02_DIR="$batch"
export BATCH02_CACHE="$repo/build/cache-batch02-$sha"
tool_dir="$repo/build/batch02-tools-$sha"
mkdir -p "$tool_dir"
export BATCH02_REAL_CMAKE="$(command -v cmake)"
python - "$tool_dir/cmake" <<'PY'
import sys
from pathlib import Path
path = Path(sys.argv[1])
path.write_text('''#!/usr/bin/env python3
import os
import sys
args = list(sys.argv[1:])
if "--build" in args:
    for flag in ("--parallel", "-j"):
        if flag in args:
            index = args.index(flag)
            if index + 1 < len(args) and args[index + 1].isdigit():
                args[index + 1] = "2"
            else:
                args.insert(index + 1, "2")
os.execv(os.environ["BATCH02_REAL_CMAKE"], [os.environ["BATCH02_REAL_CMAKE"], *args])
''')
path.chmod(0o700)
PY
export PATH="$tool_dir:$PATH"
export CMAKE_BUILD_PARALLEL_LEVEL=2
cpus="$(python -c 'import os; print(",".join(str(cpu) for cpu in sorted(os.sched_getaffinity(0))[:2]))')"
taskset -pc "$cpus" $$
renice 10 -p $$
python - <<'PY'
import os
from pathlib import Path
from simpler_setup.runtime_builder import RuntimeBuilder
RuntimeBuilder._CACHE_DIR = Path(os.environ['BATCH02_CACHE'])
RuntimeBuilder('a5').get_binaries('host_build_graph', build=True, profiling_config={
    'SIMPLER_DFX': '1', 'SIMPLER_ORCH_PROFILING': '0',
    'SIMPLER_SCHED_PROFILING': '0', 'SIMPLER_TENSORMAP_PROFILING': '0',
})
PY
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
          'pto_pin': (root / 'pto_isa.pin').read_text().strip(), 'artifacts': artifacts,
          'import': 'PASS', 'device_allocation': 'none', 'measurement_executed': False}
(batch / 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')
PY
printf 'BUILD_STARTED=%s\nBUILD_FINISHED=%s\n' "$started" "$(date -Iseconds)"
date -Iseconds > "$batch/PREPARED"
