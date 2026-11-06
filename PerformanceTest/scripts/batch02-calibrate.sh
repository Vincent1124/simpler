#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
: "${TASK_DEVICE:?Use one queue-allocated healthy A5 device}"
[[ "$TASK_DEVICE" =~ ^[0-9]+$ ]] || exit 2
bash .claude/skills/onboard-arch-precheck/check.sh a5
python PerformanceTest/scripts/batch02-calibrate.py "${1:?Pass a validated batch02 directory}" "${2:-64}"
