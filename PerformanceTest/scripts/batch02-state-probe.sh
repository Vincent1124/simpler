#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
: "${TASK_DEVICE:?Use one queue-allocated A5 device}"
[[ "$TASK_DEVICE" =~ ^[0-9]+$ ]] || exit 2
batch="$(realpath "${1:?Pass a prepared diagnostic directory}")"
[[ "$batch" == "$repo/PerformanceTest/results/"*"-batch02-"* ]] || exit 2
test -f "$batch/PREPARED"
test ! -e "$batch/state-probe"
mkdir -p "$batch/state-probe" "$batch/ascend-state-probe"
export SIMPLER_RESIDENT_PERF=off
export SIMPLER_REPORT_SCHEDULER_MODE=1
export ASCEND_PROCESS_LOG_PATH="$batch/ascend-state-probe"
failed=0
for mode in direct plain volatile inline_plain inline_volatile guarded; do
    printf 'STATE_PROBE %s\n' "$mode"
    if python -m pytest -p tests.st.a5.host_build_graph.conftest \
        PerformanceTest/cases/test_measurement_state.py::TestMeasurementState \
        --case "TestMeasurementState::$mode" --platform a5 --device "$TASK_DEVICE" \
        --manual include --rounds 1 --enable-chip-swimlane 0 -vv -s > "$batch/state-probe/$mode.log" 2>&1; then
        printf 'STATE_PROBE_PASS %s\n' "$mode"
    else
        rc=$?
        printf 'STATE_PROBE_FAIL %s exit=%s\n' "$mode" "$rc"
        failed=1
        if [[ "$mode" == direct ]]; then exit "$rc"; fi
    fi
done
exit "$failed"
