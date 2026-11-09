#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/../.."
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
: "${TASK_DEVICE:?Use task-submit with one healthy A5}"
[[ "$TASK_DEVICE" =~ ^[0-9]+$ ]]
bash .claude/skills/onboard-arch-precheck/check.sh a5
python PerformanceTest/scripts/batch03-ready-ab.py "$1" "$2" "$TASK_DEVICE"
