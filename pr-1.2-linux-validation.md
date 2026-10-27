# PR 1.2：Linux 验证命令指南

本文存放在 `mdtest` 分支根目录。执行测试时，请切换到功能分支 `mpi-session-health-timeout`。

本次实现提交：`8a2fc2c067248bd0db6c66cce457598abb7e99ee`。
以下命令均在 Linux 的 simpler 仓库根目录执行，命令本身可直接复制。

## 1. 拉取并切换到功能分支

先查看工作区状态。如果存在未提交的修改，请先妥善保存，再切换分支。

```bash
git status --short --branch
git fetch origin mdtest mpi-session-health-timeout
git show origin/mdtest:pr-1.2-linux-validation.md > /tmp/simpler-pr12-validation.md
git switch mpi-session-health-timeout
git pull --ff-only origin mpi-session-health-timeout
git rev-parse HEAD
```

如果本地尚无该功能分支，且只有 `origin` 存在同名分支，`git switch` 会自动建立本地跟踪分支。
如果多个远端都有同名分支，首次创建本地分支时，改用 `git switch --track origin/mpi-session-health-timeout`。

文档已保存到 `/tmp/simpler-pr12-validation.md`，切换分支后仍可查看。
测试前，请确认 HEAD 与上述实现提交一致，或与后续明确提供的更新提交一致。

## 2. 安装依赖并确认环境

使用项目已有的开发虚拟环境和构建依赖。MPI launcher（作业启动程序）与 `mpi4py` 需要使用兼容的 MPI 运行库。

```bash
source .venv/bin/activate
python -m pip install --no-build-isolation -e '.[test,mpi]'
python -c "import simpler; print(simpler.__file__)"
python -c "from mpi4py import MPI; print(MPI.get_vendor())"
command -v mpirun || command -v mpiexec
```

确认 `simpler.__file__` 指向当前要验证的源码目录，避免实际导入另一份旧安装包。
如果找不到 `mpirun` 或 `mpiexec`，请先安装或加载所在 Linux 环境的 MPI 软件。

正确的 Python 包名是 **`mpi4py`**，不是 `mpi4y`。若仍提示 `No module named mpi4py`，在同一虚拟环境中执行：

```bash
python -m pip install mpi4py
python -c "from mpi4py import MPI; print(MPI.get_vendor())"
```

若提示找不到 `libmpi`，或 launcher 与运行库不匹配，需要检查 MPI 环境；这与 Python 包未安装是不同的问题。
使用 `python -m pip` 可以确保依赖安装到当前运行测试的 Python 环境。

## 3. 单独验证两个关键真实 MPI 场景

```bash
set -o pipefail
python -m pytest \
  'tests/ut/py/test_mpi_direct.py::test_real_mpi_cpu_world_and_launcher_cleanup[slow-command]' \
  'tests/ut/py/test_mpi_direct.py::test_real_mpi_cpu_world_and_launcher_cleanup[idle-executor-stop]' \
  -v -ra 2>&1 | tee /tmp/simpler-pr12-mpi-key.log
```

- `slow-command`：心跳间隔 0.1 秒，健康超时 2 秒，命令超时 15 秒，每个任务等待 4 秒。持续收到心跳时，任务虽然超过健康窗口，仍应正常完成。
- `idle-executor-stop`：健康超时 1.5 秒，命令超时 15 秒。停止一个空闲 executor 后，应触发缺失 HEALTH 的故障路径。该用例预期作业失败，因此测试本身应显示 PASSED。

这两个场景不能因为缺少 MPI 环境而被跳过后，仍被视为已验证通过。

`set -o pipefail` 用于在通过 `tee` 保存日志时保留测试失败状态，避免只得到日志写入成功的退出码。

## 4. 执行针对性 Python UT 回归

```bash
set -o pipefail
python -m pytest \
  tests/ut/py/test_mpi_direct.py \
  tests/ut/py/test_mpi_session_epoch.py \
  tests/ut/py/test_mpi_direct_cleanup.py \
  tests/ut/py/test_mpi_direct_runtime_cleanup.py \
  tests/ut/py/test_worker/test_comm_endpoints.py \
  tests/ut/py/test_worker/test_comm_region.py \
  -q -ra --junitxml=/tmp/simpler-pr12-python.xml \
  2>&1 | tee /tmp/simpler-pr12-python.log
```

覆盖范围包括完整的真实 MPI 参数组、topology 配置、session epoch、endpoint registry、region 物化与清理。
新增配置测试覆盖缺省继承、显式健康超时、非法值、manifest 往返，以及健康超时与命令超时的独立接线。

如果需要定位真实 MPI 场景的问题，可以只重新运行完整 MPI 参数组：

```bash
python -m pytest tests/ut/py/test_mpi_direct.py \
  -k test_real_mpi_cpu_world_and_launcher_cleanup -v -ra
```

## 5. 编译并执行 C++ native transport UT

```bash
cmake -S tests/ut/cpp -B tests/ut/cpp/build
cmake --build tests/ut/cpp/build --target test_mpi_direct_transport -j "$(nproc)"
ctest --test-dir tests/ut/cpp/build \
  -R '^test_mpi_direct_transport$' --output-on-failure
```

扩展后的 `ProgressReplyExpiresAtItsDeadline` 检查：收到 HEALTH 帧后，命令自己的截止时间仍会到期，不会被心跳延长。
既有测试还会检查空闲失联、从 HELLO 开始的健康计时，以及取消 route 后的行为。

## 6. 执行全量 Python UT 与静态检查

**本章全量 Python UT 包含第 3、4 章的所有 Python 用例。**
可以直接执行本章来替代第 3、4 章，避免重复验证；局部命令主要用于快速检查和定位失败。
前提是相关用例确实被执行并通过，尤其要检查真实 MPI 用例的 SKIPPED 原因。

**本章不包含第 5 章的 C++ native UT，第 5 章仍需单独执行。**

```bash
set -o pipefail
python -m pytest tests/ut/py -q -ra \
  --junitxml=/tmp/simpler-pr12-all-python.xml \
  2>&1 | tee /tmp/simpler-pr12-all-python.log
pre-commit run --all-files
```

最后一条命令需要开发环境中已安装 `pre-commit`。

## 7. 保留验证结果

请保留以下信息：实现提交 SHA、PASSED/FAILED/SKIPPED 汇总、跳过原因，以及失败用例的完整 traceback。
上述命令将日志和 JUnit 报告保存到 `/tmp` 下，便于后续排查。

因缺少硬件而跳过的设备测试，与因缺少 launcher 或 `mpi4py` 而跳过的真实 MPI 测试，需要分别判断。
真实 MPI 测试被跳过不能证明对应能力已经通过验证。

当前实现提交尚未收到 Linux pytest、GoogleTest 与真实 MPI 验收通过的反馈。
此前 Windows 上已完成静态检查、7 个独立配置场景、兼容性检查，以及使用实际源码编译的独立 native transport 检查。
这些结果不替代本文要求的 Linux 验收。
