#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
batch="$(realpath "${1:?Pass a prepared batch02 directory}")"
[[ "$batch" == "$repo/PerformanceTest/results/"*"-batch02-"* ]] || exit 2
test -f "$batch/PREPARED"
: "${TASK_DEVICE:?Use task-submit with one healthy device}"
[[ "$TASK_DEVICE" =~ ^[0-9]+$ ]] || exit 2
test ! -d "$batch/validation"
mkdir -p "$batch/validation" "$batch/ascend-validation"
export ASCEND_PROCESS_LOG_PATH="$batch/ascend-validation"
export SIMPLER_REPORT_SCHEDULER_MODE=1
for mode in off count sample64 sample256; do
    for label in chain-aic mix7 graph-replay; do
        case "$label" in
            chain-aic) path=single_core_dag/test_single_core_dag.py; class=TestHbgSingleCoreDag; selection=aic_chain_64 ;;
            mix7) path=single_block_mix/test_single_block_mix.py; class=TestSingleBlockMixHostBuildGraphA5; selection=burst16_mask7 ;;
            graph-replay) path=graph_execution/test_graph_execution.py; class=TestGraphExecutionHostBuildGraphA5; selection=record_then_replay_2d ;;
        esac
        export SIMPLER_RESIDENT_PERF="$mode"
        export SIMPLER_RESIDENT_PERF_DIR="$batch/validation/$label-$mode"
        mkdir -p "$SIMPLER_RESIDENT_PERF_DIR"
        log="$batch/validation/$label-$mode.log"
        printf 'VALIDATE %s %s\n' "$label" "$mode"
        python -m pytest "tests/st/a5/host_build_graph/${path}::${class}" \
            --case "$class::$selection" --manual include --platform a5 --device "$TASK_DEVICE" \
            --rounds 2 --enable-chip-swimlane 0 -vv -s 2>&1 | tee "$log"
        python - "$SIMPLER_RESIDENT_PERF_DIR" "$mode" "$log" <<'PY'
import json
import sys
from pathlib import Path
directory, mode, log = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
assert 'mode=1 (resident (pending)) published=2' in log.read_text(errors='replace')
files = list(directory.glob('resident-*.json'))
assert len(files) == (0 if mode == 'off' else 2), (mode, len(files))
for path in files:
    data = json.loads(path.read_text())
    assert data['schema'] == 'hbg-resident-measure-v1'
    assert data['mode'] == (1 if mode == 'count' else 2)
    assert data['period'] == (256 if mode == 'sample256' else 64)
    assert len(data['workers']) == data['worker_count']
    for worker in data['workers']:
        for phase in worker['phases']:
            assert phase['admission_attempt'] == phase['admission_failure'] + phase['admission_success']
            assert phase['publish_entries'] == phase['reserve_entries']
        if mode == 'count':
            assert all(value['count'] == 0 for value in worker['timers'].values())
print('VALIDATION=PASS', mode)
PY
    done
done
date -Iseconds > "$batch/VALIDATED"
