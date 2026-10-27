# PR 1.2: Linux validation commands

This document is stored on `mdtest`. Run the tests against
`mpi-session-health-timeout`, not against `mdtest`.

The implementation commit is `8a2fc2c067248bd0db6c66cce457598abb7e99ee`.
Run the commands below from the root of your Linux simpler checkout.

## 1. Fetch and select the implementation

Check for uncommitted work before switching branches. Preserve any local work
before continuing; the commands do not reset or discard it.

```bash
git status --short --branch
git fetch origin mdtest mpi-session-health-timeout
git show origin/mdtest:pr-1.2-linux-validation.md > /tmp/simpler-pr12-validation.md
git switch mpi-session-health-timeout
git pull --ff-only origin mpi-session-health-timeout
git rev-parse HEAD
```

`git switch` creates a tracking branch automatically if the branch only exists
on `origin` and its name is unambiguous. For an ambiguous remote name, use
`git switch --track origin/mpi-session-health-timeout` when creating the local
branch for the first time.

The document remains available at `/tmp/simpler-pr12-validation.md` after
switching to the implementation branch. Confirm that HEAD matches the commit
above, or a later explicitly supplied implementation commit, before testing.

## 2. Install and check the environment

Use the existing project-local development virtual environment and build
prerequisites. The MPI launcher and `mpi4py` must use compatible MPI libraries.

```bash
source .venv/bin/activate
python -m pip install --no-build-isolation -e '.[test,mpi]'
python -c "import simpler; print(simpler.__file__)"
python -c "from mpi4py import MPI; print(MPI.get_vendor())"
command -v mpirun || command -v mpiexec
```

Check that `simpler.__file__` points to the intended checkout. If the launcher
is missing, install or load the MPI environment before the real MPI tests.

## 3. Run the two key real MPI cases

```bash
set -o pipefail
python -m pytest \
  'tests/ut/py/test_mpi_direct.py::test_real_mpi_cpu_world_and_launcher_cleanup[slow-command]' \
  'tests/ut/py/test_mpi_direct.py::test_real_mpi_cpu_world_and_launcher_cleanup[idle-executor-stop]' \
  -v -ra 2>&1 | tee /tmp/simpler-pr12-mpi-key.log
```

- `slow-command`: heartbeat interval 0.1 seconds, health timeout 2 seconds,
  command timeout 15 seconds, and a 4-second wait in each task. The job should
  complete successfully while independent heartbeats continue.
- `idle-executor-stop`: health timeout 1.5 seconds and command timeout
  15 seconds. Stopping an idle executor must trigger the missing-HEALTH failure
  path. The test expects that failure and should report PASSED.

Neither case should be skipped when validating real MPI behavior.

## 4. Run the targeted Python regression suite

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

This includes all existing real MPI parameterized cases as well as topology,
session epoch, endpoint registry, region materialization and cleanup tests.
The new configuration cases cover inherited and explicit health timeouts,
invalid values, manifest round trips and command/health timeout separation.

To rerun only the complete real MPI parameter group during diagnosis:

```bash
python -m pytest tests/ut/py/test_mpi_direct.py \
  -k test_real_mpi_cpu_world_and_launcher_cleanup -v -ra
```

## 5. Build and run the native transport tests

```bash
cmake -S tests/ut/cpp -B tests/ut/cpp/build
cmake --build tests/ut/cpp/build --target test_mpi_direct_transport -j "$(nproc)"
ctest --test-dir tests/ut/cpp/build \
  -R '^test_mpi_direct_transport$' --output-on-failure
```

The extended `ProgressReplyExpiresAtItsDeadline` case checks that receiving a
HEALTH frame does not extend an outstanding command's deadline. Existing cases
also check idle health loss, HELLO-based health initialization and cancelled
routes.

## 6. Run the full Python UT suite and static checks

After the targeted checks pass, run the broader regression suite:

```bash
set -o pipefail
python -m pytest tests/ut/py -q -ra \
  --junitxml=/tmp/simpler-pr12-all-python.xml \
  2>&1 | tee /tmp/simpler-pr12-all-python.log
pre-commit run --all-files
```

Run the last command in the configured development environment with
`pre-commit` installed.

## 7. Record the results

Keep the commit SHA, PASSED/FAILED/SKIPPED summary, skip reasons and complete
failure tracebacks. Logs and JUnit reports are written under `/tmp` by the
commands above. A test skipped because MPI is unavailable does not validate
the corresponding MPI scenario.

Linux pytest, GoogleTest and real MPI acceptance results have not yet been
reported for this implementation commit. Previously completed Windows checks
were static checks, seven isolated configuration cases, compatibility checks
and an independently compiled actual-source native transport check; those
checks do not substitute for the Linux suites above.
