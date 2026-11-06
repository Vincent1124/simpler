#!/usr/bin/env bash
set -Eeuo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo"
source /usr/local/Ascend/cann-9.3.0/set_env.sh
source .venv/bin/activate
export CMAKE_BUILD_PARALLEL_LEVEL=2
python - <<'PY'
from pathlib import Path
import runpy
from simpler_setup.kernel_compiler import KernelCompiler
root = Path.cwd()
spec = runpy.run_path('PerformanceTest/cases/test_measurement_state.py')
compiler = KernelCompiler('a5')
directory = root / 'build/batch02-state-precompile'
directory.mkdir(parents=True, exist_ok=True)
binary = compiler.compile_incore(
    str(root / 'PerformanceTest/cases/kernels/measurement_state.cpp'),
    core_type='aic', pto_isa_root=str(root / 'build/pto-isa'),
    extra_include_dirs=spec['EXTRA'], build_dir=str(directory),
)
print('STATE_PRECOMPILE_PASS bytes=', len(binary))
PY
