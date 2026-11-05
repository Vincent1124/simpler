# 第一批：resident现状基线与协议诊断（按依赖关系分批）

本页是[测量计划](README.md)的第一批执行单。**批次按前置依赖划分，不按用例数量划分**：环境/构建通过后，现有L0基线、fanout诊断、admission/Mix/Graph诊断不依赖彼此的测量结论，全部在本批执行。需要新埋点或据实测设计的并发微基准，才放入后续批次。

源码基准：mdtest合入的upstream `2a298d1eb`，默认TICKET/H8，包含PR #2857。以下在 **A5 Linux Bash、simpler仓库根目录、同一终端** 依次复制执行；本地尚未运行真机测试。本版适用于没有task-submit的机器，直接运行Bash脚本，由操作者指定设备。只生成测试包装脚本，不修改runtime算法、编译开关或任务协议。

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

L0共12个case，每case三个独立进程，每进程55轮，丢弃前5轮、统计50轮。诊断共9个进程、每case一轮，和L0分开。**所有进程在手动选定的同一设备上串行运行**；不能并跑测试进程来冒充核内并发实验。无需每个case后等待分析，全批成功后统一回传。

```text
代码/工具链/设备环境 → 构建与架构确认
                            ├─ L0各case × 三批：取得整体性能参照
                            ├─ fanout L3：观察工作分布与调度阶段
                            └─ admission/Mix/Graph L4：取得协议诊断证据
以上实验不互相依赖测量结论；脚本串行避免本批自身竞争，设备独占需另行保证。
```

Mix资源预期：mask3→AIC+AIV0，mask5→AIC+AIV1，mask6→AIC控制者+两个AIV，mask7→三lane。mask之间计算量不同，wall不能直接相减当同步收益。协议记录按当前普通v8/native Graph v9解释；数值通过不代替独立严格协议审计。CSA/SWA冻结数据和专用多生产者基准尚有数据/实现前置条件，本批不伪造其命令或结果。

## 2. 准备代码、构建和环境记录

以下准备是设备测试的共同前置条件，不需要先回传一次才能继续。已有源码改动/结果时先保留；快进失败就回传，不要reset/clean。尚未加载CANN时，先按真机实际位置source对应set_env.sh。

先查看设备列表，选择空闲且本批期间无人共用的设备；共享机器需与使用者约定使用时段。本脚本没有全局排队或设备锁，无法阻止其他程序占用同一卡。以下查询失败时回传错误，不自动提权。

```bash
npu-smi info
```

下面**只需按机器修改 `export BENCH_DEVICE=0` 这一行的编号**，不要同时填写多个设备。它是传给pytest/CANN的可见设备编号；若配置了可见设备映射，要核对映射，不能直接把芯片号或PCIe编号当作设备编号。

```bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
git fetch origin mdtest
git switch mdtest
git merge --ff-only origin/mdtest
test "$(git branch --show-current)" = mdtest
git merge-base --is-ancestor 2a298d1ebf427a7b2b2286ea052bca733c0ece85 HEAD
export BENCH_DEVICE=0
[[ "$BENCH_DEVICE" =~ ^[0-9]+$ ]] || { printf 'BENCH_DEVICE必须是单个非负整数\n'; exit 1; }
test -n "${ASCEND_HOME_PATH:-}" || { printf '请先加载CANN set_env.sh\n'; exit 1; }
test -x .venv/bin/python || python3 -m venv --system-site-packages .venv
source .venv/bin/activate
export BATCH01_DIR="$PWD/PerformanceTest/results/$(date +%Y%m%d-%H%M%S)-batch01"
mkdir -p "$BATCH01_DIR/ascend" "$BATCH01_DIR/l0" "$BATCH01_DIR/diagnostic"
bash .claude/skills/onboard-arch-precheck/check.sh a5 2>&1 | tee "$BATCH01_DIR/arch.txt"
python -m pip install --no-build-isolation -e . 2>&1 | tee "$BATCH01_DIR/build.log"
{
  git log -1 --format='%H %s'
  git status --short
  cat pto_isa.pin
  python --version
  printf 'CANN=%s\n' "$ASCEND_HOME_PATH"
  printf 'device=%s\nisolation=manual-selection-no-queue-lock\n' "$BENCH_DEVICE"
  printf 'ASCEND_RT_VISIBLE_DEVICES=%s\n' "${ASCEND_RT_VISIBLE_DEVICES:-unset}"
  command -v ccec
  npu-smi info
  test ! -f /usr/local/Ascend/driver/version.info || cat /usr/local/Ascend/driver/version.info
  find "$ASCEND_HOME_PATH" -maxdepth 2 -name version.info -type f -print -exec cat {} \;
  find build/lib/a5 -type f -name '*.so' -exec sha256sum {} \;
  rg -n 'kReadyPoolGroupWidth|kReadyPoolKind' src/a5/runtime/host_build_graph/host/runtime_maker.cpp
} 2>&1 | tee "$BATCH01_DIR/environment.txt"
git diff HEAD -- src python simpler_setup conftest.py > "$BATCH01_DIR/source.patch"
printf '\n[BATCH01] RESULT_DIR=%s\n' "$BATCH01_DIR"
```

解释：架构预检避免用错芯片；本地venv和重新构建避免加载旧runtime；SHA/PTO-ISA/runtime哈希固定版本证据。source.patch不含未跟踪的新源码，需额外保留。架构/构建失败时停止并回传现有日志，不自动换sim或legacy。环境记录明确标注本次未使用队列锁；开始/结束的设备快照不能证明整个运行期间无人共用，干扰情况需一并回传。

## 3. 生成第一批执行脚本

整块复制。case清单中的路径和筛选对应现有测试；`SIMPLER_REPORT_SCHEDULER_MODE=1`是已有诊断选项，记录实际模式，resident marker还会检查回退。L0与诊断分别落盘。某case失败时立即停止，防止后续测试使用异常设备状态；此前结果保留，不自动重试或重置设备。

```bash
cat > "$BATCH01_DIR/run-batch01.sh" <<'BASH'
#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/../../.."
source .venv/bin/activate
: "${BENCH_DEVICE:?请在第2节设置BENCH_DEVICE}"
: "${BATCH01_DIR:?需要第2节创建的结果目录}"
trap 'rc=$?; printf "\n[BATCH01] FAILED exit=%s line=%s RESULT_DIR=%s\n" "$rc" "$LINENO" "$BATCH01_DIR"; exit "$rc"' ERR
[[ "$BENCH_DEVICE" =~ ^[0-9]+$ ]] || { printf 'BENCH_DEVICE必须是单个非负整数\n'; exit 1; }
device="$BENCH_DEVICE"
export ASCEND_PROCESS_LOG_PATH="$BATCH01_DIR/ascend"
export SIMPLER_REPORT_SCHEDULER_MODE=1
printf 'device=%s\nisolation=manual-selection-no-queue-lock\n' "$device" | tee "$BATCH01_DIR/device.txt"
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
```

解释：清单最后一列指定诊断等级，0表示不额外采集；全部case都做L0测量。55轮在同一进程内，丢弃前5轮避免启动影响；三个批次分别统计，不混为一个样本池。诊断一轮保留golden和模式检查，L4耗时不用作性能值。表格摘要取官方rounds-table输出，精度0.1µs，原始STRACE仍保存。

## 4. 在指定设备上直接执行并回传

```bash
: "${BENCH_DEVICE:?请先设置设备编号}"
: "${BATCH01_DIR:?请先执行第2节}"
bash .claude/skills/onboard-arch-precheck/check.sh a5
{
  date -Iseconds
  printf 'device=%s\nisolation=manual-selection-no-queue-lock\n' "$BENCH_DEVICE"
  npu-smi info
} 2>&1 | tee "$BATCH01_DIR/device-before.txt"
bash "$BATCH01_DIR/run-batch01.sh" 2>&1 | tee "$BATCH01_DIR/batch-run.log"
{
  date -Iseconds
  npu-smi info
} 2>&1 | tee "$BATCH01_DIR/device-after.txt"
printf '\n[BATCH01] RESULT_DIR=%s\n' "$BATCH01_DIR"
```

解释：Bash前台顺序完成36个L0进程和9个诊断进程，设备编号来自BENCH_DEVICE；不需要安装调度工具，也不设置队列超时。保持终端会话，在整个运行期间避免其他工作共用该卡。开始/结束状态记录在device-before/after.txt，完整回显在batch-run.log。每项已有独立目的，不因前一项快或慢而自动跳过其他项；失败停机属于环境健康依赖。

**成功后先粘贴回传：** environment.txt中的源码/工具链/设备/队列配置、SUMMARY_BEGIN到SUMMARY_END之间36行摘要、DIAGNOSTIC_JSON列表和RESULT_DIR；同时说明设备是否始终独占，保留device-before/after.txt、完整environment.txt及build.log。没有JSON不代表通过了协议诊断，应回传原始日志检查。诊断case成功和严格协议审计是两件事。

**失败时回传：** `[BATCH01] FAILED`、失败case、终端最后约100行、RESULT_DIR及已有environment.txt。不自行删日志、改容差、切legacy/sim或连续重跑。无SUMMARY时先用日志定位，不补写零值冒充测量。

完整结果在PerformanceTest/results的本次目录：cases.txt、source.patch、runtime哈希、36份L0原始日志/逐轮表、9份诊断日志、协议JSON、summary.json、return.txt与设备日志。提交前脱敏；默认忽略.log，需要回填的具体日志用git add -f。生成脚本不是runtime改动，不需要先将整个结果目录提交才能回传。

## 5. 收到第一批结果后如何形成下一批

第一批完成后先分析实际resident模式、数值、批间波动、同类任务的执行分布、Mix资源成员和Graph控制路径。不同用例计算量不同，不直接相减得出锁成本；kernel间隙也不能单独证明空闲核可以合法执行其他任务。

后续按前置依赖推进：精确P/G/S成本需要新增每核统计/采样与导出；同group/跨group、生产者/消费者扩展需要专用基准；锁或执行许可放宽候选需要热点证据和协议论证。实现完成的项目中，彼此没有数据依赖的可以合并下一批执行，不强制一项一批。届时提供对应patch、构建命令、运行命令和验收口径；本页没有这些尚未实现的命令。
