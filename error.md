(.venv) [l00896871@master37 simpler]$ python -m pytest tests/ut -q -ra \
> --junitxml=pytest-ut-results.xml \
> 2>&1 |tee pytest-ut.log

==================================== ERRORS ====================================
____________ ERROR collecting tests/ut/py/test_gcc_setup_action.py _____________
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_gcc_setup_action.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_gcc_setup_action.py:19: in <module>
    import yaml
E   ModuleNotFoundError: No module named 'yaml'
_______ ERROR collecting tests/ut/py/test_packaging_compiler_identity.py _______
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_packaging_compiler_identity.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_packaging_compiler_identity.py:15: in <module>
    import yaml
E   ModuleNotFoundError: No module named 'yaml'
_______ ERROR collecting tests/ut/py/test_pre_commit_build_selection.py ________
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_pre_commit_build_selection.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_pre_commit_build_selection.py:17: in <module>
    import yaml
E   ModuleNotFoundError: No module named 'yaml'
________ ERROR collecting tests/ut/py/test_qwen_clone_worker_submit.py _________
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_qwen_clone_worker_submit.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_qwen_clone_worker_submit.py:20: in <module>
    from clone_worker_submit import (  # noqa: E402
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/clone_worker_submit.py:22: in <module>
    from real_worker_submit import BATCH, PADDED_VOCAB, _bind, _view
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/real_worker_submit.py:19: in <module>
    from safetensors.torch import load_file
E   ModuleNotFoundError: No module named 'safetensors'
______ ERROR collecting tests/ut/py/test_qwen_continuous_worker_submit.py ______
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_qwen_continuous_worker_submit.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_qwen_continuous_worker_submit.py:20: in <module>
    from continuous_worker_submit import (  # noqa: E402
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/continuous_worker_submit.py:21: in <module>
    from clone_worker_submit import _assert_disjoint_storage, _await_joined_evidence, _source_head
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/clone_worker_submit.py:22: in <module>
    from real_worker_submit import BATCH, PADDED_VOCAB, _bind, _view
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/real_worker_submit.py:19: in <module>
    from safetensors.torch import load_file
E   ModuleNotFoundError: No module named 'safetensors'
__________ ERROR collecting tests/ut/py/test_qwen_device_feedback.py ___________
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_qwen_device_feedback.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_qwen_device_feedback.py:23: in <module>
    import device_feedback_worker_submit as feedback  # noqa: E402
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/device_feedback_worker_submit.py:20: in <module>
    from clone_worker_submit import (
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/clone_worker_submit.py:22: in <module>
    from real_worker_submit import BATCH, PADDED_VOCAB, _bind, _view
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/real_worker_submit.py:19: in <module>
    from safetensors.torch import load_file
E   ModuleNotFoundError: No module named 'safetensors'
_________ ERROR collecting tests/ut/py/test_qwen_reference_contract.py _________
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_qwen_reference_contract.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_qwen_reference_contract.py:16: in <module>
    from safetensors.torch import save_file
E   ModuleNotFoundError: No module named 'safetensors'
________ ERROR collecting tests/ut/py/test_qwen_resident_kv_lifetime.py ________
ImportError while importing test module '/home/l00896871/pypto-work/simpler/tests/ut/py/test_qwen_resident_kv_lifetime.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
tests/ut/py/test_qwen_resident_kv_lifetime.py:21: in <module>
    import reference_worker_submit as consumer  # noqa: E402
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/reference_worker_submit.py:22: in <module>
    from real_worker_submit import (
examples/a2a3/host_build_graph/qwen3_14b_serving_effective/real_worker_submit.py:19: in <module>
    from safetensors.torch import load_file
E   ModuleNotFoundError: No module named 'safetensors'
=========================== short test summary info ============================
ERROR tests/ut/py/test_gcc_setup_action.py
ERROR tests/ut/py/test_packaging_compiler_identity.py
ERROR tests/ut/py/test_pre_commit_build_selection.py
ERROR tests/ut/py/test_qwen_clone_worker_submit.py
ERROR tests/ut/py/test_qwen_continuous_worker_submit.py
ERROR tests/ut/py/test_qwen_device_feedback.py
ERROR tests/ut/py/test_qwen_reference_contract.py
ERROR tests/ut/py/test_qwen_resident_kv_lifetime.py
!!!!!!!!!!!!!!!!!!! Interrupted: 8 errors during collection !!!!!!!!!!!!!!!!!!!!
8 errors in 8.30s
