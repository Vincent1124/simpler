(.venv) [l00896871@master37 simpler]$ set -o pipefail
python -m pytest tests/ut/py -q -ra \
  --junitxml=/tmp/simpler-pr12-all-python.xml \
  2>&1 | tee /tmp/simpler-pr12-all-python.log
pre-commit run --all-files
........................................................................ [  1%]
........................................................................ [  3%]
........................................................................ [  5%]
..................................................................ssssss [  6%]
ssssssssss.............................................................. [  8%]
........................................................................ [ 10%]
........................................................................ [ 11%]
........................................................................ [ 13%]
........................................................................ [ 15%]
........................................................................ [ 17%]
........................................................................ [ 18%]
........................................................................ [ 20%]
........................................................................ [ 22%]
........................................................................ [ 23%]
...............ss..ss.............ssssssssssss.......................... [ 25%]
....ssssssssss........ssssssssssssssssssssssssssss.ssssssssssssssss..... [ 27%]
...ssssssss........ssss................................................. [ 29%]
........................................................................ [ 30%]
........................................................................ [ 32%]
........................................^[[C.0.^[[B.............................. [ 34%]
............................................ssss........................ [ 35%]
........................................................................ [ 37%]
........................................................................ [ 39%]
........................................................................ [ 41%]
........................................................................ [ 42%]
................................................ss..ss.................. [ 44%]
..ssssssssssss.......................................................... [ 46%]
........................................................................ [ 47%]
........................................................................ [ 49%]
........................................................................ [ 51%]
........................................................................ [ 53%]
........................................................................ [ 54%]
.............................ssss...ssss................................ [ 56%]
........................................................................ [ 58%]
........................................................................ [ 59%]
........................................................................ [ 61%]
................................ssss.................................... [ 63%]
........................................................................ [ 65%]
........................................................................ [ 66%]
........................................................................ [ 68%]
................sss..................................................... [ 70%]
........................................................................ [ 71%]
........................................................................ [ 73%]
........................................................................ [ 75%]
........................................................................ [ 77%]
........................................................................ [ 78%]
........................ssss.................s.......................... [ 80%]
........................................................................ [ 82%]
........................................................................ [ 83%]
........................................................................ [ 85%]
........................................................................ [ 87%]
........................................................................ [ 89%]
........................................................................ [ 90%]
.....................ss................................................. [ 92%]
........................................................................ [ 94%]
........................................................................ [ 95%]
........................................................................ [ 97%]
...................................sssssssssssssssssssssssssssssssssssss [ 99%]
sssssssssssssssss............                                            [100%]
=============================== warnings summary ===============================
tests/ut/py/test_worker/test_sub_tensor_args.py::test_mapped_args_from_blob_delivers_tensors_and_scalars
  /home/l00896871/pypto-work/simpler/tests/ut/py/test_worker/test_sub_tensor_args.py:134: UserWarning: The given buffer is not writable, and PyTorch does not support non-writable tensors. This means you can write to the underlying (supposedly non-writable) buffer using the tensor. You may want to copy the buffer to protect its data or make it writable before converting it to a tensor. This type of warning will be suppressed for the rest of this program. (Triggered internally at /__w/pytorch/pytorch/torch/csrc/utils/tensor_new.cpp:1556.)
    assert torch.frombuffer(args[0].buffer, dtype=torch.float32, count=4).tolist() == [1.0, 2.0, 3.0, 4.0]

-- Docs: https://docs.pytest.org/en/stable/how-to/capture-warnings.html
=========================== short test summary info ============================
SKIPPED [16] tests/ut/py/test_chip_caller_stream.py:228: --platform required
SKIPPED [4] tests/ut/py/test_host_runtime_abi.py:73: --platform required
SKIPPED [4] tests/ut/py/test_kernel_caller_stream.py:210: --platform required
SKIPPED [8] tests/ut/py/test_kernel_caller_stream.py:230: --platform required
SKIPPED [8] tests/ut/py/test_kernel_mode_c_api.py:1535: --platform required
SKIPPED [2] tests/ut/py/test_kernel_mode_c_api.py:531: --platform required
SKIPPED [16] tests/ut/py/test_kernel_mode_c_api.py:89: --platform required
SKIPPED [4] tests/ut/py/test_kernel_mode_c_api.py:1166: --platform required
SKIPPED [8] tests/ut/py/test_kernel_mode_c_api.py:885: --platform required
SKIPPED [16] tests/ut/py/test_kernel_mode_c_api.py:1570: --platform required
SKIPPED [2] tests/ut/py/test_kernel_mode_c_api.py:1209: --platform required
SKIPPED [4] tests/ut/py/test_kernel_mode_c_api.py:1233: --platform required
SKIPPED [2] tests/ut/py/test_kernel_mode_c_api.py:1187: --platform required
SKIPPED [4] tests/ut/py/test_kernel_mode_c_api.py:823: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:143: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:107: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:114: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:132: --platform required
SKIPPED [2] tests/ut/py/test_runtime_builder.py:1038: ASCEND_HOME_PATH not set; Ascend toolkit required for a2a3
SKIPPED [2] tests/ut/py/test_runtime_builder.py:1038: ASCEND_HOME_PATH not set; Ascend toolkit required for a5
SKIPPED [12] tests/ut/py/test_runtime_publication_launch.py:33: --platform required
SKIPPED [8] tests/ut/py/test_swimlane_export_run_identity.py:86: /home/l00896871/pypto-work/simpler/tests/ut/cpp/build is configured but not built; run cmake --build tests/ut/cpp/build
SKIPPED [1] tests/ut/py/test_teardown_report.py:588: a2a3/onboard/host_build_graph is not built in this tree
SKIPPED [1] tests/ut/py/test_teardown_report.py:588: a2a3/onboard/tensormap_and_ringbuffer is not built in this tree
SKIPPED [1] tests/ut/py/test_teardown_report.py:588: a5/onboard/host_build_graph is not built in this tree
SKIPPED [1] tests/ut/py/test_teardown_report.py:588: a5/onboard/tensormap_and_ringbuffer is not built in this tree
SKIPPED [2] tests/ut/py/test_worker/test_chip_workspace_management_onboard.py:122: --platform required
SKIPPED [1] tests/ut/py/test_worker/test_chip_workspace_management_onboard.py:61: --platform required
SKIPPED [4] tests/ut/py/test_worker/test_device_memory_info.py:165: --platform required
SKIPPED [1] tests/ut/py/test_worker/test_dynamic_alloc_hw.py:39: --platform required
SKIPPED [1] tests/ut/py/test_worker/test_platform_comm.py:168: --platform required
SKIPPED [1] tests/ut/py/test_worker/test_provider_region_onboard.py:88: --platform required
SKIPPED [16] tests/ut/py/test_worker_caller_stream.py:165: --platform required
SKIPPED [8] tests/ut/py/test_worker_diagnostic_selection.py:251: --platform required
SKIPPED [6] tests/ut/py/test_worker_diagnostic_selection.py:218: --platform required
SKIPPED [24] tests/ut/py/test_worker_diagnostic_selection.py:181: --platform required
4011 passed, 194 skipped, 1 warning in 216.96s (0:03:36)
-bash: pre-commit: 未找到命令