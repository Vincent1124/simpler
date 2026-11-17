# A3 HBG AICPU 与 A5 HBG AI Core 调度实验手册

指南位于 `PerformanceTest/PERFORMANCE_TESTING.md`。
A3 结果保存到 `PerformanceTest/A3_hbg_result/`，A5 结果保存到 `PerformanceTest/A5_hbg_result/`。
两个目录的 `.gitkeep` 仅用于让 Git 保留目录；采样前没有实际结果数据。

本手册按顺序执行。每一步说明目的、操作、产物和通过条件。
除分支发布外，Linux 命令均在对应机器的 simpler 仓库根目录执行。
命令中的变量跨步骤使用，请在同一个 Bash 会话执行；重新登录后先恢复环境变量。

## 1. 实验目标与执行地图

### 1.1 要回答的问题

比较同为 Host 构图的两种方案：

| 项目 | A3 | A5 |
| --- | --- | --- |
| platform 参数 | a2a3 | a5 |
| runtime | host_build_graph | host_build_graph |
| 任务调度位置 | AICPU | AI Core resident |
| 图构建位置 | Host | Host |
| 主测指标 | Host/Device 耗时 | Host/Device 耗时 |

跨代结果包含硬件、kernel 和调度实现差异，应称为“整体方案加速比”。
通过任务粒度扫描和泳道证据分析调度影响，不能把全部加速归因于调度位置。
A5 的 AICPU 仍承担启动、监督和退出职责，不能用是否存在 AICPU 程序判断调度位置。

### 1.2 实验顺序与产物

| 步骤 | 做什么 | 产物 | 完成标准 |
| --- | --- | --- | --- |
| 2 | 发布分支、统一代码 | experiment SHA | 两端提交一致且工作区干净 |
| 3 | 安装与硬件检查 | 环境清单、构建日志 | 架构检查和安装成功 |
| 4 | 审计负载 | workload-audit.csv | 两端数学语义和配置对齐 |
| 5 | 单轮正确性与预检 | correctness 日志 | 全部 case 校验通过 |
| 6 | 正式跑分 | 每 case 每批原始日志 | 7 case × 3 批，每批 110 轮 |
| 7 | 离线统计 | 逐轮 CSV、汇总 CSV | 每批保留 100 个有效样本 |
| 8 | 单轮诊断 | 泳道 JSON、截图 | 记录完整，能解释执行间隙 |
| 9 | 对比汇报 | 主表、图、限制说明 | 每个数字可追溯到原始日志 |

第一轮只跑单卡七个 case。Qwen 和 BGEMM 扫描作为后续扩展。
本手册尚无 NPU 实测结果，不能将命令示例当作通过记录。

## 2. 发布分支并在两台机器固定同一版本

**目的：** 排除代码版本差异，确保 Linux 能取得本地新增的指南和实验改动。

当前分支为 `perf/a3-a5-hbg-scheduling`，创建自个人 fork 的 `origin/main`。
基线 SHA 为 `92550b6c5a5eef76617a04e660fbdde3a14d40cb`。
`origin` 是个人仓，`upstream` 是正式仓。本分支允许局部实验改动；
测试配置、计时改动和调度优化分别记录，不在性能测量中途变更。

### 2.1 在本地实验仓发布一次

以下是操作者提交和推送指南的命令，并不表示已经执行过：

```bash
 git switch perf/a3-a5-hbg-scheduling
 git diff --check
 git add PerformanceTest
 git commit -m "Docs: describe A3 and A5 HBG performance experiments"
 git push -u origin perf/a3-a5-hbg-scheduling
 git rev-parse HEAD
```

记录最后输出的完整 SHA，作为两端共同的 `EXPERIMENT_SHA`。
若还有实验代码改动，先审查并提交相关文件，再记录新的 SHA。

### 2.2 A3 机器获取代码

```bash
git clone --branch perf/a3-a5-hbg-scheduling https://github.com/Vincent1124/simpler.git simpler-perf
cd simpler-perf
export EXPERIMENT_SHA=替换为完整实验提交SHA
git checkout --detach "$EXPERIMENT_SHA"
git status --short
```

### 2.3 A5 机器获取代码

```bash
git clone --branch perf/a3-a5-hbg-scheduling https://github.com/Vincent1124/simpler.git simpler-perf
cd simpler-perf
export EXPERIMENT_SHA=替换为与A3相同的完整SHA
git checkout --detach "$EXPERIMENT_SHA"
git status --short
```

已有专用 clone 时改用 `git fetch origin`，再 checkout 同一 SHA，不必重复 clone。
**通过条件：** 两端 `git rev-parse HEAD` 相同，`git status --short` 无输出。

## 3. 准备环境并留存硬件、软件信息

**目的：** 获得可执行环境，并让汇报可解释硬件与工具链差异。
先按 [安装说明](../docs/install.md) 配置对应架构的 CANN、驱动和编译器。
不强制两代硬件使用不兼容的同版 CANN，实际版本必须记录。

### 3.1 A3 初始化

```bash
export PLATFORM=a2a3
export PERF_LABEL=a3-hbg-aicpu
source /usr/local/Ascend/ascend-toolkit/latest/bin/setenv.bash
python3 -m venv --system-site-packages .venv
source .venv/bin/activate
```

### 3.2 A5 初始化

```bash
export PLATFORM=a5
export PERF_LABEL=a5-hbg-aicore
source /usr/local/Ascend/ascend-toolkit/latest/bin/setenv.bash
python3 -m venv --system-site-packages .venv
source .venv/bin/activate
```

### 3.3 两端分别执行：安装、建输出目录、记录版本

```bash
if [ "$PLATFORM" = a2a3 ]; then
  result_root="$PWD/PerformanceTest/A3_hbg_result"
else
  result_root="$PWD/PerformanceTest/A5_hbg_result"
fi
export PERF_OUT="$result_root/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PERF_OUT"/{environment,correctness,timing,diagnostics,analysis}
mkdir -p build/performance
printf '%s\n' "$PERF_OUT" > build/performance/latest-$PERF_LABEL.txt
set -o pipefail
pip install . 2>&1 | tee "$PERF_OUT/environment/install.log"
bash .claude/skills/onboard-arch-precheck/check.sh "$PLATFORM" \
  > "$PERF_OUT/environment/arch-check.log" 2>&1
command -v task-submit
git rev-parse HEAD > "$PERF_OUT/environment/revision.txt"
git status --short > "$PERF_OUT/environment/status.txt"
python --version > "$PERF_OUT/environment/python.txt" 2>&1
pip freeze > "$PERF_OUT/environment/packages.txt"
cat pto_isa.pin > "$PERF_OUT/environment/pto_isa.pin"
cat /usr/local/Ascend/driver/version.info > "$PERF_OUT/environment/driver.txt"
npu-smi info > "$PERF_OUT/environment/npu-smi.txt" 2>&1 || \
  task-submit --run "npu-smi info" > "$PERF_OUT/environment/npu-smi.txt" 2>&1
find build/lib -type f -name '*.so' -exec sha256sum {} + \
  > "$PERF_OUT/environment/runtime-sha256.txt"
```

每条安装/检查命令成功后再继续；不能因后续命令成功而忽略前面的失败。
首次执行可能编译 kernel，不把安装与冷编译时间计入稳态跑分。

**手动补充：** 在 `environment/notes.txt` 记录芯片具体型号、AIC/AIV 数、
CANN 与编译器版本、Host CPU、频率/功耗策略、是否存在同卡任务、实际分配设备。
无法查询的字段填 unknown，不猜测。若架构不匹配或没有设备分配工具，先解决环境问题。

**通过条件：** 安装退出码为 0、arch-check 成功、可调用 task-submit，元数据已落盘。

## 4. 建立共有用例清单与负载审计

**目的：** 避免把不同 shape、不同算法或不同任务图直接当作调度对照。

### 4.1 A3 与 A5 使用同一份首批清单

两端分别创建：

```bash
cat > "$PERF_OUT/cases.txt" <<'CASES'
benchmark_bgemm Case0
alternating_matmul_add Case1
paged_attention_unroll Case1
paged_attention_unroll Case2
paged_attention_unroll_manual_scope Case1
paged_attention_unroll_manual_scope Case2
batch_paged_attention Case1
CASES
```

A3/A5 源码分布在 `examples/<platform>/host_build_graph/` 和 `tests/st/<platform>/host_build_graph/`。
下文命令优先检查 examples，再查 tests/st；精确选择 test_<example>.py，避免选中辅助测试。
逐个检查 `test_*.py` 的 CASES、generate_args、compute_golden，以及 orchestration/kernel 源码。

### 4.2 需要填入的数据

建立 `workload-audit.csv`，每个 case 一行，至少包含：

```text
case,A3_shape,A5_shape,dtype,input_initialization,logical_tasks,block_counts,dependencies,sync_semantics,kernel_differences,status
```

相同 case 名称不保证任务图相同。明确输入规模、精度、工作量、INOUT 初始状态、
逻辑任务数、block 数和依赖方向；A3/A5 kernel 指令不同可接受，但需记录。
若需要固定随机 seed，应先在本分支两端生成输入的相同位置实现并提交，不能假定现有 CLI 有 seed 开关。
不能对齐的用例标记 excluded，写出原因；不要用其他 case 的数字填补。

**通过条件：** 七行完成审计，或明确缩减后的共同集合。实际执行清单与审计表一致。

## 5. 单轮正确性与计时预检

**目的：** 确认设备、编译、输出校验都正常，再执行长批次。

### 5.1 A3 执行

确认 `PLATFORM=a2a3`、`PERF_LABEL=a3-hbg-aicpu`，然后执行下方公共命令。

### 5.2 A5 执行

确认 `PLATFORM=a5`、`PERF_LABEL=a5-hbg-aicore`，然后执行同一公共命令。

### 5.3 两端分别执行：全清单 golden 校验

```bash
while read -r example case_name; do
  test_file="examples/$PLATFORM/host_build_graph/$example/test_$example.py"
  if [ ! -f "$test_file" ]; then
    test_file="tests/st/$PLATFORM/host_build_graph/$example/test_$example.py"
  fi
  test -f "$test_file" || { echo "missing test: $example"; break; }
  task-submit --device auto --device-num 1 --run \
    "python '$test_file' --platform $PLATFORM --device \$TASK_DEVICE --case $case_name --manual include --rounds 1" \
    > "$PERF_OUT/correctness/$example-$case_name.log" 2>&1
  rc=$?
  printf '%s,%s,%s\n' "$example" "$case_name" "$rc" >> "$PERF_OUT/correctness/status.csv"
  test "$rc" -eq 0 || break
done < "$PERF_OUT/cases.txt"
```

不加 `--skip-golden`。查看每份日志，确认选中的 case 确实执行、输出比较通过，非仅收集/跳过。
这些日志还应包含 STRACE；若没有，检查日志级别与构建的 `SIMPLER_HOST_STRACE`，不要把缺失计时填 0。
验证多轮模式对 INOUT 的恢复逻辑与两端一致，尤其 BGEMM 累加输出不得无意跨轮累加。

**产物：** 每个 case 的完整日志和 status.csv。
**通过条件：** 所有实际入选 case 退出码 0、数值验证通过，无跳过或超时。

## 6. 正式性能采样：每个 case 三批

**目的：** 获取稳态耗时分布，并评估批次间波动。
本步骤不启用泳道、PMU、参数 dump；golden 已在步骤 5 验证。
每批运行 110 轮，按时间顺序丢弃前 10 轮，分析后 100 轮。

### 6.1 A3 执行

保持 A3 环境，执行下方公共命令。输出属于 `a3-hbg-aicpu`。

### 6.2 A5 执行

保持 A5 环境，执行同一公共命令。输出属于 `a5-hbg-aicore`。

### 6.3 两端分别执行：保留分 case 原始日志

```bash
(
  set -e
  for batch in 1 2 3; do
    while read -r example case_name; do
      test_file="examples/$PLATFORM/host_build_graph/$example/test_$example.py"
  if [ ! -f "$test_file" ]; then
    test_file="tests/st/$PLATFORM/host_build_graph/$example/test_$example.py"
  fi
      test -f "$test_file"
      log="$PERF_OUT/timing/$example-$case_name-batch$batch.log"
      task-submit --device auto --device-num 1 --run \
        "python '$test_file' --platform $PLATFORM --device \$TASK_DEVICE --case $case_name --manual include --rounds 110 --skip-golden" \
        > "$log" 2>&1
      python -m simpler_setup.tools.strace_timing "$log" --rounds-table > "${log%.log}.table.txt"
    done < "$PERF_OUT/cases.txt"
  done
)
```

子 shell 遇失败停止，先读失败日志再重跑对应批次；不要覆盖原失败日志，另加 retry 后缀。
自动分配可能更换卡；从分配日志记录实际设备。若设备不同，按设备分层分析或重跑，不能隐去差异。
需要固定卡时，在遵守队列规则的前提下把 auto 替换为已选设备，仍通过 task-submit 加锁。

`tools/benchmark_rounds.sh` 可用于快速检查，但默认临时原始日志会清理；本手册因此采用分 case 直跑。
不要用其整批平均值替代预热剔除后的统计，也不要将其 Trimmed Avg 当作丢弃前十轮：
工具的截尾平均按耗时排序去掉两端，与本实验按时间丢弃预热的定义不同。

**产物：** 每端 21 份 .log 和 21 份 .table.txt。
**通过条件：** 每表 110 轮，Host/Device 均存在且为正；没有错误或丢记录告警。
若一轮产生多个调用或多个 callable，应先审计调用对应关系，不能直接套用 110 行规则。

## 7. 提取逐轮数据并统计

**目的：** 将原始日志变成可复查的样本，使用一致的预热和 p95 定义。
A3、A5 均在各自 PERF_OUT 执行下方命令。
本例读取标准工具打印的表，数值精度为 0.1 微秒；更细精度分析应直接读取原始 STRACE。

```bash
python - "$PERF_OUT" <<'PY'
import csv
import math
from pathlib import Path
import re
import statistics
import sys

root = Path(sys.argv[1])
summary = []
for path in sorted((root / 'timing').glob('*.table.txt')):
    columns, rows = None, []
    for line in path.read_text().splitlines():
        if line.strip().startswith('Round '):
            columns = re.findall(r'(\w+) \(us\)', line)
        elif columns and re.match(r'^\s*\d+\s+', line):
            fields = line.split()
            if len(fields) != len(columns) + 1:
                raise ValueError(f'malformed row: {path}')
            rows.append([int(fields[0])] + [float(v) for v in fields[1:]])
    if not columns or len(rows) != 110 or [r[0] for r in rows] != list(range(110)):
        raise ValueError(f'expected rounds 0..109: {path}')
    if not {'Host', 'Device'}.issubset(columns):
        raise ValueError(f'missing Host/Device: {path}')
    retained = rows[10:]
    for metric in ('Host', 'Device'):
        index = 1 + columns.index(metric)
        if any(row[index] <= 0 for row in rows):
            raise ValueError(f'invalid {metric}: {path}')
        values = sorted(row[index] for row in retained)
        summary.append([path.name, metric, len(values), statistics.mean(values),
                        statistics.median(values), values[math.ceil(.95 * len(values)) - 1],
                        min(values), max(values)])
    with (root / 'analysis' / (path.stem + '.csv')).open('w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['round'] + columns + ['retained'])
        writer.writerows(row + [row[0] >= 10] for row in rows)
with (root / 'analysis' / 'summary.csv').open('w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['source', 'metric', 'n', 'mean_us', 'median_us', 'p95_us', 'min_us', 'max_us'])
    writer.writerows(summary)
print(f'Wrote {len(summary)} metric rows; expected 42 for seven cases x three batches.')
PY
```

**检查：** 每 case 三批，summary 共 42 行指标数据，每行 n=100。
该脚本只验证数值表，仍须检查原始日志的丢记录告警、失败和退出状态。
重跑时将失败批次移到单独目录；不要让多个 retry 同时进入统计。
p95 为 nearest-rank：100 个样本排序后的第 95 个值。

每个 case 的主结果取“三个批次 median 的中位数”；同时展示三个批次 median 范围。
p95 逐批保留，展示三批 p95 的中位数，并注明它不是合并 300 样本的 p95。
不要只挑最快一批，也不要未经说明删除慢样本。

## 8. 单独诊断并解释差异

**目的：** 判断差异更多来自 kernel 时间、调度间隙、依赖等待还是拖尾。
先选 BGEMM Case0 和一个 Attention 用例，诊断不进入正式性能样本。

### 8.1 A3 采集

```bash
task-submit --device auto --device-num 1 --run \
  "python examples/a2a3/host_build_graph/benchmark_bgemm/test_benchmark_bgemm.py --platform a2a3 --device \$TASK_DEVICE --case Case0 --manual include --rounds 1 --enable-chip-swimlane 1" \
  > "$PERF_OUT/diagnostics/bgemm-l1.log" 2>&1
```

### 8.2 A5 采集

```bash
task-submit --device auto --device-num 1 --run \
  "python examples/a5/host_build_graph/benchmark_bgemm/test_benchmark_bgemm.py --platform a5 --device \$TASK_DEVICE --case Case0 --manual include --rounds 1 --enable-chip-swimlane 1" \
  > "$PERF_OUT/diagnostics/bgemm-l1.log" 2>&1
```

### 8.3 两端导出并记录发现

从本次日志定位具体 outputs 目录，复制该目录到 diagnostics；不自动取 latest，避免误用其他实验结果。
将本次 chip_swimlane_records JSON 的实际路径填入：

```bash
export RECORDS=替换为本次输出的chip_swimlane_records_JSON路径
python -m simpler_setup.tools.swimlane_converter "$RECORDS" \
  -o "$PERF_OUT/diagnostics/bgemm-trace.json"
```

在 Perfetto 打开 trace，保存同一时间尺度的全图和尾部截图，另建 findings.csv：

```text
platform,case,kernel_execution_count,kernel_duration_distribution,active_core_count,idle_gaps,tail,trace_path,interpretation
```

先核对执行次数、任务身份和完整性，再分析 kernel 时间分布及任务之间空隙。
如需更高层级，将独立诊断命令的泳道级别改为 4；两端支持的调度阶段可能不同，缺失字段记 N/A。
空隙也可能来自依赖和资源限制，不能直接全部标成调度开销。
不能用 Device wall 减去所有并行核 kernel 时长之和计算调度开销。

## 9. 整理汇报、归档与扩展实验

### 9.1 两端数据合并

将两个完整 PERF_OUT 目录复制到汇报机器，保留 a3/a5 独立目录。
核对 revision.txt、审计表、成功日志、实际设备和样本数量后再填表。

| case | A3 Host median/p95 | A5 Host median/p95 | A3 Device median/p95 | A5 Device median/p95 | Device 加速比 | 正确性 |
| --- | --- | --- | --- | --- | --- | --- |
| 每个共有 case 一行 | 待测 | 待测 | 待测 | 待测 | 待计算 | 待校验 |

所有时间统一用微秒。加速比=A3/A5；耗时下降比例=(A3-A5)/A3。
Host 与 Device 分别计算，缺失值用 N/A，不填 0。
跨 case 汇总可用几何平均加速比，不将各 case 耗时之和称作模型端到端延迟。

推荐汇报四页：实验条件与差异；七 case 主结果；代表泳道及解释；结论与未解决问题。
明确当前数据是跨代整体方案对比，硬件贡献尚未被单独剥离。

### 9.2 第二阶段扩展

首批完成后，再在本分支同步补齐 A3/A5 BGEMM case：
固定单任务计算量扫任务数；固定任务数扫计算量；固定总计算量改变任务切分；扫描依赖深度。
当前 A5 HBG 只声明 Case0，不应直接照搬 A3 的 Case1–4 名称执行。
扩展须先提交两端统一参数和正确性检查，再重复步骤 4–8。
Qwen decode 作为业务补充，先审计 shape、kernel 与任务图，单独报告。

### 9.3 归档清单

保留实验提交、环境信息、输入与配置说明、负载审计表、全部成功/失败日志、
逐轮 CSV、汇总 CSV、泳道原始数据、转换 JSON、截图和统计口径。
如提交 runtime 计时或调度改动，分别记录其目的及观测开销，重新建立基线。
