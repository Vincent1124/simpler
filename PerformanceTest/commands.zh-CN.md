# 第一批：通过 SSH 执行 resident 基线与协议诊断

本页是[测量计划](README.md)的第一批执行单。**批次按前置依赖划分，不按用例数量划分**：环境/构建通过后，现有L0基线、fanout诊断、admission/Mix/Graph诊断不依赖彼此的测量结论，全部在本批执行。需要新埋点或据实测设计的并发微基准，才放入后续批次。

源码基准：mdtest合入的upstream `2a298d1eb`，默认TICKET/H8，包含PR #2857，实测以实际 SHA 和 patch 为准。以下由助手通过 SSH 在 **A5 Linux Bash、已创建的 `liuyiming_aicore_resident_perf/simpler` 仓库根目录**执行，并直接读取和分析结果。用户无需手动执行或粘贴回显。当前尚未在本服务器执行真机性能测试。本页生成测试包装脚本，不修改 runtime 算法、编译开关或任务协议。

执行前在对话中列出完整命令及目的；远端工作目录使用已确认路径，仓库内不存登录地址或密钥信息。每组 SSH 命令显式 `cd`，不得依赖上一个 SSH 会话的当前目录。第2节创建批次并保存 `batch.env`；之后每次连接显式指定同一个绝对 `BATCH01_DIR`，再加载该文件。连接断开后先检查队列及已有进程，不直接重跑。删除、清理、设备重置类命令不包含在执行单中，需单独审阅。

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

L0共12个case，每case三个独立进程，每进程55轮，丢弃前5轮、统计50轮。诊断共9个进程、每case一轮，和L0分开。**所有进程在同一设备上串行运行，同一次 task-submit 分配覆盖全批**；不能并跑测试进程来冒充核内并发实验。无需每个case后等待人工反馈，助手直接获取全批结果；失败则停止并分析已保存的日志。

```text
代码/工具链/设备环境 → 构建与架构确认
                            ├─ L0各case × 三批：取得整体性能参照
                            ├─ fanout L3：观察工作分布与调度阶段
                            └─ admission/Mix/Graph L4：取得协议诊断证据
以上实验不互相依赖测量结论；脚本串行避免本批自身竞争，设备独占由队列分配保证。
```

Mix资源预期：mask3→AIC+AIV0，mask5→AIC+AIV1，mask6→AIC控制者+两个AIV，mask7→三lane。mask之间计算量不同，wall不能直接相减当同步收益。协议记录按当前普通v8/native Graph v9解释；数值通过不代替独立严格协议审计。CSA/SWA冻结数据和专用多生产者基准尚有数据/实现前置条件，本批不伪造其命令或结果。

## 2. 准备代码、构建和环境记录

本节是首次准备/受影响环境更新的完整流程。当前服务器已完成 `results/20261105-170356-batch01/` 的准备并生成PREPARED，继续本批时恢复对应环境与批次目录后进入第3节，不重复执行本节的依赖安装或构建。后续其他实验按照[README中的环境复用规则](README.md)判断：代码、工具链和产物未变时只做环境恢复、版本/哈希和设备检查，并建立新的实验记录；运行时源码或埋点变化才显式增量构建。测试kernel首次编译与运行时构建分开记录，不将编译耗时当作稳态L0数据。

以下准备是设备测试的共同前置条件。助手先核对远端工作区和已同步的 mdtest 版本；本页不执行 fetch、reset 或 clean。源码更新另列命令，离线传输需校验后使用。尚未加载 CANN 时，助手先检查实际安装位置，再列出并执行对应 set_env.sh；每个新 SSH 会话恢复同一工具链环境。严格错误处理放在独立 Bash 中，准备成功才生成 PREPARED 标记。venv 仅使用仓库内 `.venv`。

本服务器的设备查询也必须通过 task-submit。以下只读查询不申请设备锁；工具缺失或查询失败时停止并分析错误。工具存在不表示机器就是 A5，后面的架构预检仍须通过。

```bash
if command -v task-submit >/dev/null 2>&1 && task-submit --run 'npu-smi info'; then
  printf '设备查询完成，请核对芯片型号和设备编号。\n'
else
  printf '设备查询失败；停止后续步骤并读取错误，SSH会话保留。\n'
fi
```

默认 `BENCH_DEVICE=auto` 申请一张卡；需要固定设备时使用单个编号，且仍通过队列分配。不要手动设置 TASK_DEVICE。下面在仓库根目录执行，助手记录打印的绝对批次目录；后续 SSH 会话恢复该目录，不重新生成时间戳。

```bash
export BENCH_DEVICE=auto
export BATCH01_DIR="$PWD/PerformanceTest/results/$(date +%Y%m%d-%H%M%S)-batch01"
if bash <<'PREPARE'
set -Eeuo pipefail
cd "$(git rev-parse --show-toplevel)"
test "$(git branch --show-current)" = mdtest
git status --short
git merge-base --is-ancestor 2a298d1ebf427a7b2b2286ea052bca733c0ece85 HEAD
[[ "$BENCH_DEVICE" == auto || "$BENCH_DEVICE" =~ ^[0-9]+$ ]] || { printf 'BENCH_DEVICE须为auto或单个非负整数\n'; exit 1; }
command -v task-submit >/dev/null 2>&1 || { printf '缺少task-submit，停止测试。\n'; exit 1; }
test -n "${ASCEND_HOME_PATH:-}" || { printf '请先加载CANN set_env.sh\n'; exit 1; }
test -x .venv/bin/python || python3 -m venv --system-site-packages .venv
source .venv/bin/activate
mkdir -p "$BATCH01_DIR/ascend" "$BATCH01_DIR/l0" "$BATCH01_DIR/diagnostic"
printf 'export BATCH01_DIR=%q\nexport BENCH_DEVICE=%q\n' "$BATCH01_DIR" "$BENCH_DEVICE" > "$BATCH01_DIR/batch.env"
task-submit --run "cd '$PWD' && bash .claude/skills/onboard-arch-precheck/check.sh a5" 2>&1 | tee "$BATCH01_DIR/arch.txt"
python -m pip install --no-build-isolation -e . 2>&1 | tee "$BATCH01_DIR/build.log"
{
  git log -1 --format='%H %s'
  git status --short
  cat pto_isa.pin
  python --version
  printf 'CANN=%s\n' "$ASCEND_HOME_PATH"
  printf 'requested_device=%s\n' "$BENCH_DEVICE"
  command -v task-submit
  printf 'execution=task-submit\n'
  printf 'ASCEND_RT_VISIBLE_DEVICES=%s\n' "${ASCEND_RT_VISIBLE_DEVICES:-unset}"
  command -v ccec
  task-submit --run 'npu-smi info'
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

解释：架构预检避免用错芯片；本地venv和重新构建避免加载旧runtime；SHA/PTO-ISA/runtime哈希固定版本证据。source.patch不含未跟踪的新源码，需额外保留。架构/构建失败时停止并直接分析现有日志，不自动换sim或legacy。实际分配编号在运行时写入device.txt。准备阶段的设备查询和架构预检通过无设备分配的 task-submit 执行，不持有测试卡锁；正式测试才申请单设备。

## 3. 生成第一批执行脚本

助手在新 SSH 会话中先进入仓库根目录，显式设置第2节记录的绝对 `BATCH01_DIR`，再执行本节。这里不提供可误执行的占位目录。case清单和测量配置保留；`SIMPLER_REPORT_SCHEDULER_MODE=1`记录实际模式，resident marker还会检查回退。L0与诊断分别落盘。某case失败立即停止；保留结果，不自动重试或重置设备。

```bash
if bash <<'GENERATE'
set -Eeuo pipefail
: "${BATCH01_DIR:?请先执行第2节}"
source "$BATCH01_DIR/batch.env"
test -f "$BATCH01_DIR/PREPARED" || { printf '准备未成功，请先处理第2节错误。\n'; exit 1; }
cat > "$BATCH01_DIR/run-batch01.sh" <<'BASH'
#!/usr/bin/env bash
set -Eeuo pipefail
BATCH01_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
source "$BATCH01_DIR/batch.env"
cd "$BATCH01_DIR/../../.."
source .venv/bin/activate
: "${BENCH_DEVICE:?请在第2节设置BENCH_DEVICE}"
: "${BATCH01_DIR:?需要第2节创建的结果目录}"
trap 'rc=$?; printf "\n[BATCH01] FAILED exit=%s line=%s RESULT_DIR=%s\n" "$rc" "$LINENO" "$BATCH01_DIR"; exit "$rc"' ERR
: "${TASK_DEVICE:?请通过第4节task-submit执行，不要直接运行脚本}"
device="${TASK_DEVICE%%,*}"
isolation=task-submit
bash .claude/skills/onboard-arch-precheck/check.sh a5
[[ "$device" =~ ^[0-9]+$ ]] || { printf '实际设备编号必须是单个非负整数\n'; exit 1; }
export ASCEND_PROCESS_LOG_PATH="$BATCH01_DIR/ascend"
export SIMPLER_REPORT_SCHEDULER_MODE=1
printf 'device=%s\nisolation=%s\n' "$device" "$isolation" | tee "$BATCH01_DIR/device.txt"
{ date -Iseconds; npu-smi info; } 2>&1 | tee "$BATCH01_DIR/device-before.txt"
cat > "$BATCH01_DIR/cases.txt" <<'CASES'
chain-aic|single_core_dag/test_single_core_dag.py|TestHbgSingleCoreDag::aic_chain_64|0
chain-aiv|single_core_dag/test_single_core_dag.py|TestHbgSingleCoreDag::aiv_chain_64|0
fanout256|fanout_imbalance/test_fanout_imbalance.py|TestFanoutImbalance::fanout_256|3
fanout1024|fanout_imbalance/test_fanout_imbalance.py|TestFanoutImbalance::fanout_1024|0
spmd-aic8|spmd_multiblock_aic/test_spmd_multiblock_aic.py|TestSpmdMultiblockAic::aic_block_num_8|4
spmd-aiv16|spmd_multiblock_aic/test_spmd_multiblock_aic.py|TestSpmdMultiblockAic::aiv_block_num_16|4
mix3|single_block_mix/test_single_block_mix.py|TestSingleBlockMixHostBuildGraphA5::burst16_mask3|4
mix5|single_block_mix/test_single_block_mix.py|TestSingleBlockMixHostBuildGraphA5::burst16_mask5|4
mix6|single_block_mix/test_single_block_mix.py|TestSingleBlockMixHostBuildGraphA5::burst16_mask6|4
mix7|single_block_mix/test_single_block_mix.py|TestSingleBlockMixHostBuildGraphA5::burst16_mask7|4
ordinary-mix|single_block_mix/test_single_block_mix.py|TestSingleBlockMixHostBuildGraphA5::ordinary_mix_interleaved|4
graph-replay|graph_execution/test_graph_execution.py|TestGraphExecutionHostBuildGraphA5::record_then_replay_2d|4
CASES
run_case() {
  local log="$1" path="$2" selection="$3" rounds="$4" level="$5"
  printf '\n[BATCH01] START=%s rounds=%s level=%s\n' "$log" "$rounds" "$level"
  python -m pytest "tests/st/a5/host_build_graph/$path" --case "$selection" --manual include \
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
date -Iseconds > "$BATCH01_DIR/COMPLETED"
BASH
bash -n "$BATCH01_DIR/run-batch01.sh"
GENERATE
then
  printf '\n[BATCH01] 脚本已生成并通过语法检查。\n'
else
  printf '\n[BATCH01] 脚本未生成：停止后续步骤，SSH会话保留。\n'
fi
```

解释：清单最后一列指定诊断等级，0表示不额外采集；全部case都做L0测量。场景类的多个case位于同一个pytest条目内，须用项目的 `--case ClassName::case_name` 精确选择，不能用 `-k` 匹配内部case名称。`--manual include`仅允许已精确选中的manual case，不扩大清单。提交前使用项目的case选择函数核对每行恰好选中一个case。55轮在同一进程内，丢弃前5轮避免启动影响；三个批次分别统计，不混为一个样本池。诊断一轮保留golden和模式检查，L4耗时不用作性能值。表格摘要取官方rounds-table输出，精度0.1µs，原始STRACE仍保存。

## 4. 队列执行与直接采集

助手先恢复第2节的绝对 BATCH01_DIR 和 CANN 环境，再执行。批次日志落盘；不依赖 SSH 终端滚屏保存证据。任务提交后若连接中断，先执行 `task-submit --list` 并核对该批任务及日志，再决定等待或故障分析；不得直接重新提交同一批。该命令本身不是断线续跑保证，任务是否仍存活以队列状态为准。

```bash
if bash <<'EXECUTE'
set -Eeuo pipefail
: "${BATCH01_DIR:?请先执行第2节}"
source "$BATCH01_DIR/batch.env"
test -f "$BATCH01_DIR/PREPARED" || { printf '准备未成功，请先处理第2节错误。\n'; exit 1; }
: "${BENCH_DEVICE:?请先设置设备编号}"
: "${BATCH01_DIR:?请先执行第2节}"
command -v task-submit >/dev/null 2>&1
test ! -e "$BATCH01_DIR/batch-run.log" || { printf '本批已有运行日志，先核对队列与现场，禁止重复提交。\n'; exit 1; }
task-submit --list 2>&1 | tee "$BATCH01_DIR/allocation.txt"
device_args=(--device "$BENCH_DEVICE")
if [[ "$BENCH_DEVICE" == auto ]]; then device_args+=(--device-num 1); fi
task-submit --timeout 7200 --max-time 7200 "${device_args[@]}" \
  --run "bash '$BATCH01_DIR/run-batch01.sh'" \
  2>&1 | tee "$BATCH01_DIR/batch-run.log"
test -f "$BATCH01_DIR/COMPLETED"
printf '\n[BATCH01] RESULT_DIR=%s\n' "$BATCH01_DIR"
EXECUTE
then
  printf '\n[BATCH01] 全批执行完成，助手直接读取结果并更新性能台账。\n'
else
  printf '\n[BATCH01] 执行失败：保留现场，SSH会话保留。RESULT_DIR=%s\n' "${BATCH01_DIR:-未设置}"
fi
```

解释：全批持同一个设备锁，auto自动分配一张卡，7200秒是排队/执行预算而非样本时间。36个L0和9个诊断进程始终顺序运行。严格错误处理只作用于独立Bash，外层if处理失败返回并保留SSH。实际设备和隔离方式在device.txt，前后快照在device-before/after.txt，完整回显在batch-run.log。失败不绕过队列、不自动重试。COMPLETED仅表示脚本与摘要生成成功，不代替协议审计；缺失时检查队列与日志，不假定测试已完成。

**成功后由助手读取：** environment.txt中的源码/工具链/设备/队列配置、summary.json、return.txt中的36行摘要、诊断JSON及device-before/after.txt；检查原始pytest结果与实际resident模式。没有JSON不代表通过了协议诊断，应检查原始日志。诊断case成功和严格协议审计是两件事。

**失败时由助手定位：** `[BATCH01] FAILED`、失败case、batch-run.log末尾约100行及已有environment.txt；构建失败读取build.log，设备异常读取本批ascend目录。原始返回码、失败阶段、已有样本及是否有效写入性能台账。不删日志、改容差、切legacy/sim或连续重跑。无SUMMARY时先定位原因，不补写零值冒充测量。

完整结果在PerformanceTest/results的本次目录：batch.env、cases.txt、source.patch、runtime哈希、36份L0原始日志/逐轮表、9份诊断日志、协议JSON、summary.json、return.txt与设备日志。助手按该目录直接分析并更新[性能数据与分析台账](performance-data.zh-CN.md)，无需先提交原始结果。提交前脱敏；batch.env含服务器绝对路径，不纳入Git；原始日志按证据需要选择性脱敏保存，不整目录强制添加。

每次采集先显式恢复 BATCH01_DIR，再执行以下只读命令；随后按清单读取具体原始日志与JSON，不能只读摘要就下结论：

```bash
test -d "$BATCH01_DIR" || exit 1
ls -l "$BATCH01_DIR"
test ! -f "$BATCH01_DIR/environment.txt" || cat "$BATCH01_DIR/environment.txt"
test ! -f "$BATCH01_DIR/summary.json" || cat "$BATCH01_DIR/summary.json"
test ! -f "$BATCH01_DIR/return.txt" || cat "$BATCH01_DIR/return.txt"
test ! -f "$BATCH01_DIR/batch-run.log" || tail -n 100 "$BATCH01_DIR/batch-run.log"
find "$BATCH01_DIR/diagnostic" -name chip_swimlane_records.json -type f -print
```

### 资源与耗时说明

| 操作 | 资源占用 | 预计耗时与限制 |
| ---- | -------- | -------------- |
| SSH核对、文档与日志读取 | 少量CPU/磁盘/网络，不运行kernel | 通常秒级；大JSON按大小读取 |
| task-submit设备查询/架构预检 | 管理查询，不申请测试卡锁 | 通常秒级，以工具实际响应为准 |
| venv与编译安装 | CPU、内存、磁盘及可能的依赖网络，不执行性能kernel | 冷构建可能数分钟至数十分钟；当前服务器尚未实测，执行前核对并限制构建并发 |
| 生成包装脚本 | 少量CPU/磁盘，不占NPU | 通常秒级 |
| 第一批L0与诊断 | 一张队列分配的NPU，Host CPU和日志空间 | 排队时长未知；45个进程包含JIT，尚无可靠全批估时。分别记录排队、构建/JIT和实际执行时间；7200秒是预算，不是预计耗时 |
| 汇总与分析 | CPU/磁盘，无新增kernel | 摘要通常秒级，协议轨迹随数据规模增长 |

## 5. 分析第一批结果并形成下一批

第一批完成后，助手直接读取服务器数据，先分析实际resident模式、数值、批间波动、同类任务的执行分布、Mix资源成员和Graph控制路径，并在性能台账关联原始文件。不同用例计算量不同，不直接相减得出锁成本；kernel间隙也不能单独证明空闲核可以合法执行其他任务。

后续按前置依赖推进：精确P/G/S成本需要新增每核统计/采样与导出；同group/跨group、生产者/消费者扩展需要专用基准；锁或执行许可放宽候选需要热点证据和协议论证。实现完成的项目中，彼此没有数据依赖的可以合并下一批执行，不强制一项一批。届时提供对应patch、构建命令、运行命令和验收口径；本页没有这些尚未实现的命令。
