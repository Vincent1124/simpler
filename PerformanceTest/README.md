# A5 resident Ready 队列性能测试：复制命令与结果回填

代码基准：2026-11-05 拉取的 upstream/main `2a298d1ebf427a7b2b2286ea052bca733c0ece85`，已包含 PR #2857（`459b69a98`）。mdtest 合入此基准并保留文档，后续源码与结果在本分支维护。以下命令用于 **A5 Linux 真机的 Bash**，依次复制到同一个终端执行。当前尚未运行真机测量。

最新代码核对：Ready pool 默认仍为 **TICKET / group width=8**，不是 PR #2857 新引入。此次变化主要在 Mix 资源成员、领取结束证明、关闭标记隔离、重复检查消减和本地自旋。设计见 [resident/percounter 指南](hbg-resident-vs-percounter-guide.zh-CN.md) 和 [离线 HTML](hbg-resident-vs-percounter-guide.html)。

## 1. 测量对象与数据边界

| 路径 | 同步机制 | 测量重点 |
| ---- | -------- | -------- |
| 默认计算 Ready pool | 按 AIC/AIV/Mix 类型、group 分队列；FAA 预留 entries，CAS 推进 head | FAA/CAS、entry 发布、跨组扫描 |
| Graph 控制节点 | ENTER/EXIT 等进入独立 control inbox | endpoints CAS、控制节点与 wake-list 处理 |
| 历史 owner inbox | 代码保留，默认计算任务不走它 | endpoints CAS、链指针等待、directory |

TICKET 是位置预留，不是持有式 ticket lock；admission 发布锁是另一项成本。基线命令可直接获取 Host/Device wall；level=3 可观察调度记录，均不能直接给出 CAS 的 ns/次。已有 ready_cas_retry_count 主要覆盖 TICKET 领取 CAS，不覆盖 FAA；其余计数未被通用 swimlane 完整导出。不能将整个 Device wall 或 profiling 扰动称作锁耗时。

### 1.1 PR #2857 优化及测量边界

| 改动 | 原理与仍保留的约束 |
| ---- | ------------------ |
| Mix资源集合缩小 | resource_mask=active_mask并上AIC；AIC+AIV0不再等无关AIV1，AIC+AIV1同理；双AIV的Mix仍含AIC控制者 |
| per-lane分配身份 | mailbox保存assigned_seq与各lane前驱，部分lane不参与某轮时仍可安全复用描述符 |
| claim_seal结束证明 | 最后有效block领取确认者发布；正向提示仍需原子确认，缺证明仍走强cursor读取；不等于任务执行完成 |
| 关闭标记隔离 | 每task/cluster独立128字节原子区，减少共享区域干扰并保留跨mailbox复用的关闭证据 |
| 有界检查复用 | 同次服务复用固定配置、准备/提交校验、ACK预探测；重试丢弃证明，错误、epoch和真实身份不缓存成许可 |
| 热路径减少 | 缓存READY后的只读拓扑与配置，按profiling裁剪记录；NOP自旋按进展复位，初始8、idle上限1024、启动/退出上限256 |

GM也有成本：TaskControl从512到640字节；Mix关闭标记从8到128字节/task/cluster；新增量约`128*task_count + 120*mix_tasks*cluster_count`字节，另计对齐；本地admission状态增加32字节。不能只比较耗时而不记录图大小、资源mask与布局版本。

[PR保留报告](../tests/fdwic/pa_hbg/csa-swa-scheduler-optimization-20261103.md)的冻结对照以142ee15a7为起点，同卡每组10次、L0、包含首轮且无额外预热：CSA Device中位数3.132435→2.531305ms（−19.19%），SWA 1.076120→0.898628ms（−16.49%）。这是整组优化效果，且两者仍慢于该报告的AICPU对照；不是单项锁收益，也不是当前整合main的新测量。原报告前段的“排除native Graph”描述冻结提取边界，不能外推成当前main不支持Graph。

本页基线则采用同进程5轮预热后50轮、三个批次，统计口径不同；不能直接用它复现或替换上述百分比。普通协议记录为v8，native Graph为v9，旧审计器或固定“三lane成员”预期不适用于新版。

## 2. 同步、构建与环境回显

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

## 3. 基线与诊断命令

先整块复制生成脚本。每 case 一个进程内执行55轮，固定丢弃前5轮后统计50轮；每 case 三个独立进程，保留 golden 校验，level=0。短链可能走直接候选；fanout 由一个 root 释放后继，不代表所有多生产者竞争。

```bash
cat > PerformanceTest/run-ready-baseline.sh <<'BASH'
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source .venv/bin/activate
: "${TASK_DEVICE:?需要 task-submit 分配设备}"
: "${READY_RESULT_DIR:?先执行第2节}"
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

### 3.1 独立的小规模诊断

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

### 3.2 PR #2857 的 Mix 资源范围诊断

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

## 4. 精确耗时、归因与回填

后续埋点直接在 mdtest 实现。本次只更新文档，不假定下面新增指标已经可读取。

| 指标 | 位置与口径 |
| ---- | ---------- |
| FAA次数/ticks | scheduler_ticket_reserve；与CAS分开 |
| entry发布条数/ticks | scheduler_ticket_publish；区分每批/每任务 |
| CAS尝试/失败/ticks | scheduler_ticket_try_pop_batch及Mix placement专用分支 |
| NOT_READY/RETRY | 区分entry未发布、容量末尾和竞争预算耗尽，不等于全局没工作 |
| 跨组扫描数/ticks | scheduler_ready_pool_claim；包括每第四次远端优先 |
| ready_enqueue/batch/pop/steal/retry/giveup | 从每核context导出；当前steal表示跨组领取 |
| Graph控制inbox | endpoints CAS、链等待另计 |
| admission commit_lock | 等待与持有时间另计，不混入Ready竞争 |
| claim_seal与cursor | 证明命中/强读回退分别计数；区分“领完”和“执行完” |
| Mix到达/释放等待 | 按resource_mask计所需ACK及等待时间；同时记录无关lane是否可推进 |
| 自旋/重复校验 | NOP等待、固定配置验证与ACK强确认开销分开；L0与L4不能直接混比 |

现有统计在 aicore_executor.cpp 的 publish_worker_stats 回写，Host 在 runtime_maker.cpp 的 publish_aicore_scheduler_profiling 收回。当前 TICKET 可算 `Σready_cas_retry_count / Σready_pop_count`，表示每次成功领取的重试数，不是严格失败率，也不覆盖 FAA。平均发布批大小为 `Σready_enqueue_count / Σready_batch_count`；零分母填 N/A，bootstrap与执行期分开。

精确计时用每核本地统计，结束时一次回写，避免每次都写共享计数器制造竞争。测空计时框架，校验读计时器与原子完成的顺序。`ns = ticks × 1e9 / timer_hz`，不套计算核主频，不跨核相减时间戳。记录采样与计时扰动。

微基准应覆盖独占字、多核独立字、多核共享字、单生产者多消费者、多生产者双向竞争、同/跨group；并发度从1/2/4/8起步。调用生产版helper才覆盖实际协议；纯CAS循环不等于完整queue成本。保留唯一执行与数值校验，不在使用中复用节点。

检验串行限制需分层：同group head/reserved_tail热点、队首已预留未发布、admission提交锁、按序成员义务、单cluster Mix rendezvous、单核非抢占kernel。保持同任务量/核数，对比同组集中与分散发布；保持同计算工作，改变协作组织或参与lane耗时。仅看Device wall不能归因。资源mask改变时计算工作也可能改变，不能直接把mask3与mask7耗时差算成协议收益。

端到端保持同语义、同工具链，同卡独占、关闭profiling后交替A/B/B/A。不能删除CAS作为并发对照。多核累计原子服务时间除以Device wall，不是可节省墙钟比例；profiling增量不是锁成本。Ready pool有结构性改进目标，但[已有性能记录](../docs/zh-cn/design/a5-hbg-graph-task/no-graph-performance.md)仍有退化，不预设一定更快。

### 4.1 结果回填

| 结果目录 | 实测SHA/patch | pool/group | 设备/CANN/驱动 | 样本 | Device median/p95 | FAA/CAS | 结论 |
| -------- | ------------- | ---------- | -------------- | ---- | ----------------- | ------- | ---- |
| 待回填 | 2a298d1eb + mdtest文档/实际patch | TICKET/8 | | 每批50×3批 | | 尚未埋点 | 未真机测量 |

数据放在 `PerformanceTest/results/<本次目录>/`。日志提交前脱敏；大型设备日志可留真机，提交摘要、位置和校验值。默认忽略.log，需要提交的具体日志使用git add -f，不要加入整个构建目录。

```bash
git status --short
git add PerformanceTest/README.md
# 按实际目录逐个添加摘要、环境、CSV、源码与必要日志。
# git add PerformanceTest/results/<本次目录>/summary.json
# git add -f PerformanceTest/results/<本次目录>/<具体日志>.log
git diff --cached --stat
```
