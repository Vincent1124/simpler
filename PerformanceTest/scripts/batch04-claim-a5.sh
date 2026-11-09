#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/../.."
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
: "${TASK_DEVICE:?Use task-submit with one healthy A5}"
[[ "$TASK_DEVICE" =~ ^[0-9]+$ ]]
bash .claude/skills/onboard-arch-precheck/check.sh a5
output="${1:?Pass a new result directory}"
[[ "$output" == PerformanceTest/results/*-batch04-* ]]
mkdir "$output"
mkdir "$output/ascend"
export ASCEND_PROCESS_LOG_PATH="$PWD/$output/ascend"
export SIMPLER_RESIDENT_PERF=off
python -m pytest PerformanceTest/cases/test_ready_claim_a5.py --platform a5 --device "$TASK_DEVICE" -vv -s 2>&1 | tee "$output/run.log"
rg -o 'BATCH04_CLAIM \{.*\}' "$output/run.log" > "$output/measurements.jsonl"
test "$(wc -l < "$output/measurements.jsonl")" -eq 160
date -Iseconds > "$output/COMPLETED"
