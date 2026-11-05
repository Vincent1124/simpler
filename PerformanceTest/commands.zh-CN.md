# 第一批：resident现状基线与协议诊断（按依赖关系分批）

本页是[测量计划](README.md)的第一批执行单。**批次按前置依赖划分，不按用例数量划分**：环境/构建通过后，现有L0基线、fanout诊断、admission/Mix/Graph诊断不依赖彼此的测量结论，全部在本批执行。需要新埋点或据实测设计的并发微基准，才放入后续批次。

源码基准：mdtest合入的upstream `2a298d1eb`，默认TICKET/H8，包含PR #2857。以下在 **A5 Linux Bash、simpler仓库根目录、同一终端** 依次复制执行；本地尚未运行真机测试。本版自动检测task-submit：存在时通过队列分配一张设备、全批持有锁；不存在时才使用显式设备编号直接执行。只生成测试包装脚本，不修改runtime算法、编译开关或任务协议。

2026-11-05回传的error.md显示机器被识别为A3（Ascend910_9382，Short_SoC_version=Ascend910_93），不是本轮目标A5。预检拒绝是正常保护；旧命令直接对登录shell启用errexit，使预检失败连带退出SSH。新版把严格错误处理放在独立bash进程，外层处理失败返回；不对登录shell直接执行set -e，也不绕过架构预检。换到A5机器后再执行本轮；如果认为识别有误，先核对npu-smi与CANN平台信息，不改测试平台强行运行。

## 1. 第一批的目标、测试目的与产物

目标是建立可复现的resident参照，确认实际调度模式、数值和执行链路，找出值得进一步分解的场景。**本批没有新增计时埋点，不能直接给出CAS/锁占比，也不能证明某并发方案更快或无死锁。**

| 用例ID | L0测量目的 | 同批协议诊断 |
| ------ | ---------- | ------------ |
| chain-aic / chain-aiv | 短链控制间隔参照，可能大量走直接候选 | 无单独高等级采集 |
| fanout256 / fanout1024 | 集中释放Ready工作的分布、规模和wall表现；不是多生产者微基准 | 只采256的L3，避免1024高等级扰动 |
| spmd-aic8 / spmd-aiv16 | 纯AIC/AIV多block的admission、block领取与完成路径参照 | 各采一轮L4 |
| mix3 / mix5 / mix6 / mix7 | 独立Mix burst的执行参照，覆盖不同资源集合 | 各采一轮L4 |
| ordinary-mix | 普通工作和Mix交错的参照 | 一轮L4 |
| graph-replay | Definition记录/replay及控制节点路径的参照 | 一轮L4 |

L0共12个case，每case三个独立进程，每进程55轮，丢弃前5轮、统计50轮。诊断共9个进程、每case一轮，和L0分开。**所有进程在同一设备上串行运行，有task-submit时同一次分配覆盖全批**；不能并跑测试进程来冒充核内并发实验。无需每个case后等待分析，全批成功后统一回传。

```text
代码/工具链/设备环境 → 构建与架构确认
                            ├─ L0各case × 三批：取得整体性能参照
                            ├─ fanout L3：观察工作分布与调度阶段
                            └─ admission/Mix/Graph L4：取得协议诊断证据
以上实验不互相依赖测量结论；脚本串行避免本批自身竞争，无队列时设备独占需另行保证。
```

Mix资源预期：mask3→AIC+AIV0，mask5→AIC+AIV1，mask6→AIC控制者+两个AIV，mask7→三lane。mask之间计算量不同，wall不能直接相减当同步收益。协议记录按当前普通v8/native Graph v9解释；数值通过不代替独立严格协议审计。CSA/SWA冻结数据和专用多生产者基准尚有数据/实现前置条件，本批不伪造其命令或结果。

## 2. 准备代码、构建和环境记录

以下准备是设备测试的共同前置条件，不需要先回传一次才能继续。已有源码改动/结果时先保留；快进失败就回传，不要reset/clean。尚未加载CANN时，先按真机实际位置source对应set_env.sh。代码块中的if、PREPARE、then/else/fi需要整块复制；准备失败只退出子进程，成功才生成PREPARED标记供后续步骤检查。venv在子进程中激活，后续执行脚本会自行再次激活。

先查看设备与芯片型号。若裸npu-smi因权限失败，使用task-submit重试同一只读查询，不分配设备；两者都失败则回传错误。工具存在不表示机器就是A5，后面的架构预检仍须通过。

```bash
if npu-smi info || { command -v task-submit >/dev/null 2>&1 && task-submit --run 'npu-smi info'; }; then
  printf '设备查询完成，请核对芯片型号和设备编号。\n'
else
  printf '设备查询失败；请回传错误，SSH会话保留。\n'
fi
```

有task-submit时保留`export BENCH_DEVICE=auto`即可，无需修改设备编号；若要固定设备，将auto改为单个编号。无task-submit时必须改为单个空闲设备编号并自行保证独占。编号须与pytest/CANN可见设备映射一致。不要手动设置TASK_DEVICE，它应由队列分配。

```bash
export BENCH_DEVICE=auto
export BATCH01_DIR="$PWD/PerformanceTest/results/$(date +%Y%m%d-%H%M%S)-batch01"
if bash <<'PREPARE'
set -Eeuo pipefail
cd "$(git rev-parse --show-toplevel)"
git fetch origin mdtest
git switch mdtest
git merge --ff-only origin/mdtest
test "$(git branch --show-current)" = mdtest
git merge-base --is-ancestor 2a298d1ebf427a7b2b2286ea052bca733c0ece85 HEAD
[[ "$BENCH_DEVICE" == auto || "$BENCH_DEVICE" =~ ^[0-9]+$ ]] || { printf 'BENCH_DEVICE须为auto或单个非负整数\n'; exit 1; }
if ! command -v task-submit >/dev/null 2>&1; then
  [[ "$BENCH_DEVICE" != auto ]] || { printf '无task-submit，请设置显式BENCH_DEVICE编号。\n'; exit 1; }
fi
test -n "${ASCEND_HOME_PATH:-}" || { printf '请先加载CANN set_env.sh\n'; exit 1; }
test -x .venv/bin/python || python3 -m venv --system-site-packages .venv
source .venv/bin/activate
mkdir -p "$BATCH01_DIR/ascend" "$BATCH01_DIR/l0" "$BATCH01_DIR/diagnostic"
bash .claude/skills/onboard-arch-precheck/check.sh a5 2>&1 | tee "$BATCH01_DIR/arch.txt"
python -m pip install --no-build-isolation -e . 2>&1 | tee "$BATCH01_DIR/build.log"
{
  git log -1 --format='%H %s'
  git status --short
  cat pto_isa.pin
  python --version
  printf 'CANN=%s\n' "$ASCEND_HOME_PATH"
  printf 'requested_device=%s\n' "$BENCH_DEVICE"
  if command -v task-submit; then printf 'execution=task-submit\n'; else printf 'execution=manual-no-queue-lock\n'; fi
  printf 'ASCEND_RT_VISIBLE_DEVICES=%s\n' "${ASCEND_RT_VISIBLE_DEVICES:-unset}"
  command -v ccec
  npu-smi info || { command -v task-submit >/dev/null 2>&1 && task-submit --run 'npu-smi info'; }
  test ! -f /usr/local/Ascend/driver/version.info || cat /usr/local/Ascend/driver/version.info
  find "$ASCEND_HOME_PATH" -maxdepth 2 -name version.info -type f -print -exec cat {} \;
  find build/lib/a5 -type f -name '*.so' -exec sha256sum {} \;
  rg -n 'kReadyPoolGroupWidth|kReadyPoolKind' src/a5/runtime/host_build_graph/host/runtime_maker.cpp
} 2>&1 | tee "$BATCH01_DIR/environment.txt"
git diff HEAD -- src python simpler_setup conftest.py > "$BATCH01_DIR/source.patch"
printf '\n[BATCH01] RESULT_DIR=%s\n' "$BATCH01_DIR"
touch "$BATCH01_DIR/PREPARED"
PREPARE
then
  printf '\n[BATCH01] PREPARED：可以继续第3节。\n'
else
  printf '\n[BATCH01] 准备失败：停止后续步骤，SSH会话保留。RESULT_DIR=%s\n' "${BATCH01_DIR:-未设置}"
fi
```

解释：架构预检避免用错芯片；本地venv和重新构建避免加载旧runtime；SHA/PTO-ISA/runtime哈希固定版本证据。source.patch不含未跟踪的新源码，需额外保留。架构/构建失败时停止并回传现有日志，不自动换sim或legacy。环境记录执行方式；实际分配编号在运行时写入device.txt。无队列时前后快照不能证明全程独占，干扰情况需一并回传。

## 3. 生成第一批执行脚本

整块复制。case清单中的路径和筛选对应现有测试；`SIMPLER_REPORT_SCHEDULER_MODE=1`是已有诊断选项，记录实际模式，resident marker还会检查回退。L0与诊断分别落盘。某case失败时立即停止，防止后续测试使用异常设备状态；此前结果保留，不自动重试或重置设备。

```bash
if bash <<'GENERATE'
set -Eeuo pipefail
: "${BATCH01_DIR:?请先执行第2节}"
test -f "$BATCH01_DIR/PREPARED" || { printf '准备未成功，请先处理第2节错误。\n'; exit 1; }
cat > "$BATCH01_DIR/run-batch01.sh" <<'BASH'
#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/../../.."
source .venv/bin/activate
: "${BENCH_DEVICE:?请在第2节设置BENCH_DEVICE}"
: "${BATCH01_DIR:?需要第2节创建的结果目录}"
trap 'rc=$?; printf "\n[BATCH01] FAILED exit=%s line=%s RESULT_DIR=%s\n" "$rc" "$LINENO" "$BATCH01_DIR"; exit "$rc"' ERR
if command -v task-submit >/dev/null 2>&1; then
  : "${TASK_DEVICE:?请通过第4节task-submit执行，不要直接运行脚本}"
  device="${TASK_DEVICE%%,*}"
  isolation=task-submit
else
  device="$BENCH_DEVICE"
  isolation=manual-selection-no-queue-lock
fi
[[ "$device" =~ ^[0-9]+$ ]] || { printf '实际设备编号必须是单个非负整数\n'; exit 1; }
export ASCEND_PROCESS_LOG_PATH="$BATCH01_DIR/ascend"
export SIMPLER_REPORT_SCHEDULER_MODE=1
printf 'device=%s\nisolation=%s\n' "$device" "$isolation" | tee "$BATCH01_DIR/device.txt"
{ date -Iseconds; npu-smi info; } 2>&1 | tee "$BATCH01_DIR/device-before.txt"
cat > "$BATCH01_DIR/cases.txt" <<'CASES'
chain-aic|single_core_dag/test_single_core_dag.py|aic_chain_64 and not work8|0
chain-aiv|single_core_dag/test_single_core_dag.py|aiv_chain_64|0
fanout256|fanout_imbalance/test_fanout_imbalance.py|fanout_256|3
fanout1024|fanout_imbalance/test_fanout_imbalance.py|fanout_1024|0
spmd-aic8|spmd_multiblock_aic/test_spmd_multiblock_aic.py|aic_block_num_8|4
spmd-aiv16|spmd_multiblock_aic/test_spmd_multiblock_aic.py|aiv_block_num_16|4
mix3|single_block_mix/test_single_block_mix.py|burst16_mask3|4
mix5|single_block_mix/test_single_block_mix.py|burst16_mask5|4
mix6|single_block_mix/test_single_block_mix.py|burst16_mask6|4
mix7|single_block_mix/test_single_block_mix.py|burst16_mask7|4
ordinary-mix|single_block_mix/test_single_block_mix.py|ordinary_mix_interleaved|4
graph-replay|graph_execution/test_graph_execution.py|record_then_replay_2d|4
CASES
run_case() {
  local log="$1" path="$2" selection="$3" rounds="$4" level="$5"
  printf '\n[BATCH01] START=%s rounds=%s level=%s\n' "$log" "$rounds" "$level"
  python -m pytest "tests/st/a5/host_build_graph/$path" -k "$selection" \
    --platform a5 --device "$device" --rounds "$rounds" \
    --enable-chip-swimlane "$level" -vv -s 2>&1 | tee "$log"
}
for rep in 1 2 3; do
  while IFS='|' read -r label path selection diagnostic_level; do
    log="$BATCH01_DIR/l0/$label-$rep.log"
    run_case "$log" "$path" "$selection" 55 0
    python -m simpler_setup.tools.strace_timing "$log" --rounds-table \
      > "$BATCH01_DIR/l0/$label-$rep-timing.txt"
  done < "$BATCH01_DIR/cases.txt"
done
date -Iseconds > "$BATCH01_DIR/diagnostic-start.marker"
while IFS='|' read -r label path selection level; do
  if [[ "$level" == 0 ]]; then continue; fi
  run_case "$BATCH01_DIR/diagnostic/$label.log" "$path" "$selection" 1 "$level"
done < "$BATCH01_DIR/cases.txt"
find outputs -name chip_swimlane_records.json -newer "$BATCH01_DIR/diagnostic-start.marker" \
  -print -exec cp --parents {} "$BATCH01_DIR/diagnostic" \;
{ date -Iseconds; npu-smi info; } 2>&1 | tee "$BATCH01_DIR/device-after.txt"
python - "$BATCH01_DIR" <<'PY'
import json
import math
import re
import statistics
import sys
from pathlib import Path
root = Path(sys.argv[1])
cases = [s.split('|') for s in (root / 'cases.txt').read_text().splitlines() if s]
assert len(cases) == 12 and len({s[0] for s in cases}) == 12
summary, reply = [], ['[BATCH01] SUMMARY_BEGIN', 'case\trep\tn\thost_median_us\thost_p95_us\tdevice_median_us\tdevice_p95_us']
for label, _, _, level in cases:
    for rep in range(1, 4):
        stem = root / 'l0' / f'{label}-{rep}'
        log = stem.with_suffix('.log').read_text(errors='replace')
        assert '[SCHEDULER-MODE]' in log, f'Missing actual scheduler report: {stem}'
        text = Path(str(stem) + '-timing.txt').read_text()
        header = next((s for s in text.splitlines() if 'Round' in s and 'Host (us)' in s), '')
        assert 'Device (us)' in header, f'Missing Device wall: {stem}'
        rows = [list(map(float, m.groups()[1:])) for m in
                re.finditer(r'^\s*(\d+)\s+([\d.]+)\s+([\d.]+)\s*$', text, re.M)]
        assert len(rows) == 55, f'Expected 55 rounds, got {len(rows)}: {stem}'
        assert all(row[1] > 0 for row in rows), f'Invalid Device wall: {stem}'
        result = {'case': label, 'rep': rep, 'samples': 50, 'discarded': 5}
        for index, metric in enumerate(('host_us', 'device_us')):
            values = sorted(row[index] for row in rows[5:])
            result[metric] = {'median': statistics.median(values),
                              'p95': values[math.ceil(.95 * len(values)) - 1]}
        summary.append(result)
        reply.append(f"{label}\t{rep}\t50\t{result['host_us']['median']:.2f}\t{result['host_us']['p95']:.2f}\t{result['device_us']['median']:.2f}\t{result['device_us']['p95']:.2f}")
    if level != '0':
        log = (root / 'diagnostic' / f'{label}.log').read_text(errors='replace')
        assert '[SCHEDULER-MODE]' in log, f'Missing diagnostic scheduler report: {label}'
reply.extend(['[BATCH01] SUMMARY_END', '[BATCH01] DIAGNOSTIC_JSON'])
paths = sorted((root / 'diagnostic').rglob('chip_swimlane_records.json'))
reply.extend(str(p.relative_to(root)) for p in paths)
reply.extend([f'[BATCH01] diagnostic_json_count={len(paths)}', '[BATCH01] RESULT_DIR=' + str(root)])
(root / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
(root / 'return.txt').write_text('\n'.join(reply) + '\n')
print('\n'.join(reply))
PY
BASH
bash -n "$BATCH01_DIR/run-batch01.sh"
GENERATE
then
  printf '\n[BATCH01] 脚本已生成并通过语法检查。\n'
else
  printf '\n[BATCH01] 脚本未生成：停止后续步骤，SSH会话保留。\n'
fi
```

解释：清单最后一列指定诊断等级，0表示不额外采集；全部case都做L0测量。55轮在同一进程内，丢弃前5轮避免启动影响；三个批次分别统计，不混为一个样本池。诊断一轮保留golden和模式检查，L4耗时不用作性能值。表格摘要取官方rounds-table输出，精度0.1µs，原始STRACE仍保存。

## 4. 自动选择执行方式并回传

```bash
if bash <<'EXECUTE'
set -Eeuo pipefail
: "${BATCH01_DIR:?请先执行第2节}"
test -f "$BATCH01_DIR/PREPARED" || { printf '准备未成功，请先处理第2节错误。\n'; exit 1; }
: "${BENCH_DEVICE:?请先设置设备编号}"
: "${BATCH01_DIR:?请先执行第2节}"
bash .claude/skills/onboard-arch-precheck/check.sh a5
if command -v task-submit >/dev/null 2>&1; then
  task-submit --list 2>&1 | tee "$BATCH01_DIR/allocation.txt"
  device_args=(--device "$BENCH_DEVICE")
  if [[ "$BENCH_DEVICE" == auto ]]; then device_args+=(--device-num 1); fi
  task-submit --timeout 7200 --max-time 7200 "${device_args[@]}" \
    --run "bash '$BATCH01_DIR/run-batch01.sh'" \
    2>&1 | tee "$BATCH01_DIR/batch-run.log"
else
  [[ "$BENCH_DEVICE" =~ ^[0-9]+$ ]] || { printf '无队列时必须指定设备编号。\n'; exit 1; }
  printf '[BATCH01] 无队列锁，请确认设备全程独占。\n'
  bash "$BATCH01_DIR/run-batch01.sh" 2>&1 | tee "$BATCH01_DIR/batch-run.log"
fi
printf '\n[BATCH01] RESULT_DIR=%s\n' "$BATCH01_DIR"
EXECUTE
then
  printf '\n[BATCH01] 全批执行完成，请按回传清单提供结果。\n'
else
  printf '\n[BATCH01] 执行失败：保留现场，SSH会话保留。RESULT_DIR=%s\n' "${BATCH01_DIR:-未设置}"
fi
```

解释：有队列时全批持同一个设备锁，auto自动分配一张卡，7200秒是排队/执行预算而非样本时间；无队列才手动执行。36个L0和9个诊断进程始终顺序运行。严格错误处理只作用于独立Bash，外层if处理失败返回并保留SSH。实际设备和隔离方式在device.txt，前后快照在device-before/after.txt，完整回显在batch-run.log。失败不绕过队列、不自动重试，保留现场回传。

**成功后先粘贴回传：** environment.txt中的源码/工具链/设备/队列配置、SUMMARY_BEGIN到SUMMARY_END之间36行摘要、DIAGNOSTIC_JSON列表和RESULT_DIR；同时说明设备是否始终独占，保留device-before/after.txt、完整environment.txt及build.log。没有JSON不代表通过了协议诊断，应回传原始日志检查。诊断case成功和严格协议审计是两件事。

**失败时回传：** `[BATCH01] FAILED`、失败case、终端最后约100行、RESULT_DIR及已有environment.txt。不自行删日志、改容差、切legacy/sim或连续重跑。无SUMMARY时先用日志定位，不补写零值冒充测量。

完整结果在PerformanceTest/results的本次目录：cases.txt、source.patch、runtime哈希、36份L0原始日志/逐轮表、9份诊断日志、协议JSON、summary.json、return.txt与设备日志。提交前脱敏；默认忽略.log，需要回填的具体日志用git add -f。生成脚本不是runtime改动，不需要先将整个结果目录提交才能回传。

## 5. 收到第一批结果后如何形成下一批

第一批完成后先分析实际resident模式、数值、批间波动、同类任务的执行分布、Mix资源成员和Graph控制路径。不同用例计算量不同，不直接相减得出锁成本；kernel间隙也不能单独证明空闲核可以合法执行其他任务。

后续按前置依赖推进：精确P/G/S成本需要新增每核统计/采样与导出；同group/跨group、生产者/消费者扩展需要专用基准；锁或执行许可放宽候选需要热点证据和协议论证。实现完成的项目中，彼此没有数据依赖的可以合并下一批执行，不强制一项一批。届时提供对应patch、构建命令、运行命令和验收口径；本页没有这些尚未实现的命令。
