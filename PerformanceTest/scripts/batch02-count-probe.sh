#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
batch="$(realpath "${1:?Pass the prepared diagnostic batch}")"
[[ "$batch" == "$repo/PerformanceTest/results/"*"-batch02-"* ]] || exit 2
test -f "$batch/PREPARED"
test ! -e "$batch/count-probe.log"
: "${TASK_DEVICE:?Use a queue-allocated device}"
[[ "$TASK_DEVICE" =~ ^[0-9]+$ ]] || exit 2
mkdir -p "$batch/count-probe" "$batch/ascend-count-probe"
export ASCEND_PROCESS_LOG_PATH="$batch/ascend-count-probe"
export SIMPLER_REPORT_SCHEDULER_MODE=1
export SIMPLER_RESIDENT_PERF=count
export SIMPLER_RESIDENT_PERF_DIR="$batch/count-probe"
printf 'COUNT_PROBE source=%s device=%s\n' "$(git rev-parse HEAD)" "$TASK_DEVICE"
python -m pytest tests/st/a5/host_build_graph/single_core_dag/test_single_core_dag.py::TestHbgSingleCoreDag \
    --case TestHbgSingleCoreDag::aic_chain_64 --manual include --platform a5 --device "$TASK_DEVICE" \
    --rounds 1 --enable-chip-swimlane 0 -vv -s > "$batch/count-probe.log" 2>&1
