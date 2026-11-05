# A5 resident 基线与诊断：复制命令及回显回填

本页保留可直接复制到A5 Linux Bash的现有命令；先阅读[测量目标与实验设计](README.md)。命令针对mdtest中基于2a298d1eb的resident，不会自动实现新埋点或运行尚未编写的并发微基准。

执行顺序：第1节环境确认 → 第2节L0基线 → 按需第2.1节fanout/L3和第2.2节Mix/L4诊断 → 按主README的实验编号回填。命令在同一终端、simpler根目录依次执行。阶段1只能建立现状，不能据此声称测出了CAS、锁等待或安全并发的因果收益。

## 1. 同步、构建与环境回显

在真机 simpler 根目录执行。快进更新不会覆盖已有结果/源码，遇到冲突回传现场，不要 reset/clean。尚未有仓库时，先克隆个人 fork 并进入仓库。

```bash
set -euo pipefail
git fetch origin mdtest
git switch mdtest
git merge --ff-only origin/mdtest
test "$(git branch --show-current)" = mdtest
git status --short
git log -1 --format='%H %s'
# 若尚未加载 CANN，先按本机位置 source 对应 set_env.sh。
test -n "${ASCEND_HOME_PATH:-}" || { printf '请先加载 CANN set_env.sh\n'; exit 1; }
test -x .venv/bin/python || python3 -m venv --system-site-packages .venv
source .venv/bin/activate
export READY_RESULT_DIR="$PWD/PerformanceTest/results/$(date +%Y%m%d-%H%M%S)-baseline"
mkdir -p "$READY_RESULT_DIR/ascend"
python -m pip install --no-build-isolation -e . 2>&1 | tee "$READY_RESULT_DIR/build.log"
bash .claude/skills/onboard-arch-precheck/check.sh a5
{
  git log -1 --format='%H %s'
  git status --short
  cat pto_isa.pin
  python --version
  printf 'CANN=%s\n' "$ASCEND_HOME_PATH"
  command -v ccec
  command -v task-submit || true
  npu-smi info || task-submit --run 'npu-smi info'
  test ! -f /usr/local/Ascend/driver/version.info || cat /usr/local/Ascend/driver/version.info
  find "$ASCEND_HOME_PATH" -maxdepth 2 -name version.info -type f -print -exec cat {} \;
  find build/lib/a5 -type f -name '*.so' -exec sha256sum {} \;
  rg -n 'kReadyPoolGroupWidth|kReadyPoolKind' src/a5/runtime/host_build_graph/host/runtime_maker.cpp
} 2>&1 | tee "$READY_RESULT_DIR/environment.txt"
git diff HEAD -- src python simpler_setup conftest.py > "$READY_RESULT_DIR/source.patch"
printf '\n[READY_BENCH] RESULT_DIR=%s\n' "$READY_RESULT_DIR"
```

**回传 environment.txt 与失败时的末尾输出**。预期 A5 检查通过、mdtest、TICKET/8 和设备列表。缺少 task-submit 时先停止，说明设备编号和独占情况，再安排不加锁的测试。新增未跟踪源码不在 source.patch 中，需额外保存提交。

## 2. 基线与诊断命令

先整块复制生成脚本。每 case 一个进程内执行55轮，固定丢弃前5轮后统计50轮；每 case 三个独立进程，保留 golden 校验，level=0。短链可能走直接候选；fanout 由一个 root 释放后继，不代表所有多生产者竞争。

```bash
cat > PerformanceTest/run-ready-baseline.sh <<'BASH'
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source .venv/bin/activate
: "${TASK_DEVICE:?需要 task-submit 分配设备}"
: "${READY_RESULT_DIR:?先执行本页第1节}"
device="${TASK_DEVICE%%,*}"
export ASCEND_PROCESS_LOG_PATH="$READY_RESULT_DIR/ascend"
printf 'device=%s\n' "$device" | tee "$READY_RESULT_DIR/device.txt"
run_case() {
  local label="$1" path="$2" selection="$3"
  printf '\n[READY_BENCH] START=%s\n' "$label"
  python -m pytest "$path" -k "$selection" --platform a5 --device "$device" \
    --rounds 55 --enable-chip-swimlane 0 -vv -s \
    2>&1 | tee "$READY_RESULT_DIR/$label.log"
  python -m simpler_setup.tools.strace_timing "$READY_RESULT_DIR/$label.log" \
    --rounds-table > "$READY_RESULT_DIR/$label-timing.txt"
}
for rep in 1 2 3; do
  run_case "chain-$rep" \
    tests/st/a5/host_build_graph/single_core_dag/test_single_core_dag.py \
    'aic_chain_64 and not work8'
  for count in 256 1024; do
    run_case "fanout${count}-$rep" \
      tests/st/a5/host_build_graph/fanout_imbalance/test_fanout_imbalance.py \
      "fanout_${count}"
  done
done
python - "$READY_RESULT_DIR" <<'PY'
import json
import math
import re
import statistics
import sys
from pathlib import Path
root = Path(sys.argv[1])
summary = []
for path in sorted(root.glob('*-timing.txt')):
    text = path.read_text()
    header = next((s for s in text.splitlines() if 'Round' in s and 'Host (us)' in s), '')
    assert 'Device (us)' in header, f'Missing Device wall: {path}'
    rows = [list(map(float, m.groups()[1:])) for m in
            re.finditer(r'^\s*(\d+)\s+([\d.]+)\s+([\d.]+)\s*$', text, re.M)]
    assert len(rows) == 55, f'Expected 55 rounds, got {len(rows)}: {path}'
    result = {'case': path.stem.removesuffix('-timing'), 'samples': 50, 'discarded': 5}
    for index, metric in enumerate(('host_us', 'device_us')):
        values = sorted(row[index] for row in rows[5:])
        result[metric] = {'median': statistics.median(values),
                          'p95': values[math.ceil(.95 * len(values)) - 1]}
    summary.append(result)
assert len(summary) == 9, f'Expected 9 case batches, got {len(summary)}'
output = json.dumps(summary, ensure_ascii=False, indent=2)
(root / 'summary.json').write_text(output + '\n')
print('\n[READY_BENCH] SUMMARY_BEGIN\n' + output + '\n[READY_BENCH] SUMMARY_END')
print('[READY_BENCH] RESULT_DIR=' + str(root))
PY
BASH

bash -n PerformanceTest/run-ready-baseline.sh
command -v task-submit
task-submit --list 2>&1 | tee "$READY_RESULT_DIR/allocation.txt"
bash .claude/skills/onboard-arch-precheck/check.sh a5
task-submit --timeout 7200 --max-time 7200 --device auto --device-num 1 \
  --run "bash '$PWD/PerformanceTest/run-ready-baseline.sh'" \
  2>&1 | tee "$READY_RESULT_DIR/allocation-run.log"
```

**回传 SUMMARY_BEGIN 到 SUMMARY_END，以及 RESULT_DIR**。失败/skip、模式不是 resident、摘要断言失败时，该批无效；回传错误末尾并保留完整日志。摘要使用工具输出的0.1µs精度，原始 STRACE 仍保留更细信息。HBG 看 Device wall 和 Host wall，不套用 TMR 的 Orch/Sched 分解。

### 2.1 独立的小规模诊断

以下块创建新目录，测256 fanout的一轮level=3。1024开启高等级profiling可能因测量扰动超时，不作默认诊断。

```bash
DIAG_DIR="$PWD/PerformanceTest/results/$(date +%Y%m%d-%H%M%S)-diagnostic"
mkdir -p "$DIAG_DIR/ascend"
export ASCEND_PROCESS_LOG_PATH="$DIAG_DIR/ascend"
git rev-parse HEAD > "$DIAG_DIR/source-sha.txt"
git diff HEAD -- src python simpler_setup conftest.py > "$DIAG_DIR/source.patch"
date -Iseconds > "$DIAG_DIR/start-time.txt"
bash .claude/skills/onboard-arch-precheck/check.sh a5
task-submit --timeout 1800 --max-time 1800 --device auto --device-num 1 \
  --run "set -o pipefail; '$PWD/.venv/bin/python' -m pytest \
  tests/st/a5/host_build_graph/fanout_imbalance/test_fanout_imbalance.py \
  -k fanout_256 --platform a5 --device \$TASK_DEVICE \
  --rounds 1 --enable-chip-swimlane 3 -vv -s \
  2>&1 | tee '$DIAG_DIR/run.log'"
find outputs -name chip_swimlane_records.json -newer "$DIAG_DIR/start-time.txt" \
  -print -exec cp --parents {} "$DIAG_DIR" \;
printf '\n[READY_BENCH] DIAGNOSTIC_DIR=%s\n' "$DIAG_DIR"
```

回传诊断 run.log 的测试结果和 JSON 路径。缺 JSON 时保留日志。level=3 wall 与基线分开报告；state_probe/resolve 整段时间不是原子操作时间。该步骤尚未新增 FAA/CAS 计时。

### 2.2 PR #2857 的 Mix 资源范围诊断

短链/fanout不能覆盖双lane Mix。以下块在一个设备分配内运行现有Mix用例，L4每case一轮，保留数值检查并收集协议证据。这是功能/协作诊断，不能用来报告无profiling性能或证明空闲AIV的计算收益。

```bash
export MIX_RESULT_DIR="$PWD/PerformanceTest/results/$(date +%Y%m%d-%H%M%S)-mix-l4"
mkdir -p "$MIX_RESULT_DIR/ascend"
git rev-parse HEAD > "$MIX_RESULT_DIR/source-sha.txt"
git diff HEAD -- src python simpler_setup conftest.py > "$MIX_RESULT_DIR/source.patch"
date -Iseconds > "$MIX_RESULT_DIR/start-time.txt"
cat > "$MIX_RESULT_DIR/run-mix.sh" <<'BASH'
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
source .venv/bin/activate
: "${TASK_DEVICE:?需要设备分配}"
: "${MIX_RESULT_DIR:?需要结果目录}"
export ASCEND_PROCESS_LOG_PATH="$MIX_RESULT_DIR/ascend"
for selection in chain16_mask3 chain16_mask5 chain16_mask6 chain16_mask7 ordinary_mix_interleaved; do
  python -m pytest tests/st/a5/host_build_graph/single_block_mix/test_single_block_mix.py \
    -k "$selection" --platform a5 --device "${TASK_DEVICE%%,*}" \
    --rounds 1 --enable-chip-swimlane 4 -vv -s \
    2>&1 | tee "$MIX_RESULT_DIR/$selection.log"
done
BASH
bash -n "$MIX_RESULT_DIR/run-mix.sh"
bash .claude/skills/onboard-arch-precheck/check.sh a5
task-submit --timeout 3600 --max-time 3600 --device auto --device-num 1 \
  --run "bash '$MIX_RESULT_DIR/run-mix.sh'" \
  2>&1 | tee "$MIX_RESULT_DIR/allocation-run.log"
find outputs -name chip_swimlane_records.json -newer "$MIX_RESULT_DIR/start-time.txt" \
  -print -exec cp --parents {} "$MIX_RESULT_DIR" \;
printf '\n[READY_BENCH] MIX_RESULT_DIR=%s\n' "$MIX_RESULT_DIR"
```

回传五个case的pytest结果及本次JSON。每cluster的资源预期：mask3→AIC+AIV0，mask5→AIC+AIV1，mask6→AIC控制者+AIV0+AIV1，mask7→三lane。未在active_mask中的AIC可只承担控制，不调用subtask kernel；无关AIV可跳过该任务义务，但其普通执行仍受自身后续admission约束。检查assigned/arrived/released与关闭证据，而不是只看kernel数量。协议证据采集后须使用同版本审计工具进一步核验；pytest通过本身不等于已经独立完成严格协议审计。

## 3. 回传与提交

基线回传environment.txt、summary.json、设备编号与RESULT_DIR；诊断回传各case结果和本次JSON路径。失败时回传错误末尾并保留完整日志。结果位于PerformanceTest/results，按[README结果模板](README.md#5-结果归因验收与下一步)记录假设、实际实现的指标及限制。不能把缺失的埋点数据填成0。

```bash
git status --short
# 按实际目录逐个添加摘要、环境、CSV、源码与必要日志。
# git add PerformanceTest/results/<本次目录>/summary.json
# git add -f PerformanceTest/results/<本次目录>/<具体日志>.log
git diff --cached --stat
```

源码SHA包含mdtest的文档提交，须同时记录上游基准、runtime哈希和源码patch；新增未跟踪文件额外保存。日志提交前脱敏，大型日志留真机并记录位置与校验值。不要将本页生成的临时执行脚本误认为新的生产runtime实现。
