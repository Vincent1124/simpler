(.venv) [l00896871@master37 simpler]$ python -m pytest tests/ut -q -ra --junitxml=pytest-ut-results.xml 2>&1 |tee pytest-ut.log
^C^[[A^C^X^Z
[3]+  已停止               python -m pytest tests/ut -q -ra --junitxml=pytest-ut-results.xml 2>&1 | tee pytest-ut.log
(.venv) [l00896871@master37 simpler]$ python -m pip install --no-build-isolation -e .
Looking in indexes: http://mirrors.tools.huawei.com/pypi/simple
Obtaining file:///home/l00896871/pypto-work/simpler
  Checking if build backend supports build_editable ... done
  Preparing editable metadata (pyproject.toml) ... done
Requirement already satisfied: cloudpickle>=2.2 in /home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages (from simpler==0.1.0) (3.1.2)
Building wheels for collected packages: simpler
  Building editable for simpler (pyproject.toml) ... done
  Created wheel for simpler: filename=simpler-0.1.0-cp311-cp311-linux_aarch64.whl size=47519905 sha256=66f8dd055dec61d361a59f3ac627dafe77226dbcd3fc2eb0c7c932f8663d713e
  Stored in directory: /tmp/pip-ephem-wheel-cache-nm7ru6bi/wheels/85/d9/92/eb173c1284844380e70da926ccccfbae886ebb64a80455205f
Successfully built simpler
Installing collected packages: simpler
  Attempting uninstall: simpler
    Found existing installation: simpler 0.1.0
    Uninstalling simpler-0.1.0:
      Successfully uninstalled simpler-0.1.0
Successfully installed simpler-0.1.0
(.venv) [l00896871@master37 simpler]$ python -m pytest tests/ut -q -ra --junitxml=pytest-ut-results.xml 2>&1 |tee pytest-ut.log
........................................................................ [  1%]
........................................................................ [  3%]
........................................................................ [  5%]
..................................................................ssssss [  7%]
ssssssssss.............................................................. [  9%]
........................................................................ [ 11%]
........................................................................ [ 13%]
........................................................................ [ 15%]
........................................................................ [ 17%]
....................................................................ss.. [ 19%]
ss.............ssssssssssss..............................ssssssssss..... [ 21%]
...ssssssssssssssssssssssssssss.ssssssssssssssss........ssssssss........ [ 23%]
ssss.................................................................... [ 25%]
...........................................sssssss...................... [ 27%]
.......................................F................................ [ 29%]
...................ssss................................................. [ 31%]
........................................................................ [ 33%]
........................................................................ [ 35%]
........................................................................ [ 37%]
........................................................................ [ 39%]
...................ss..ss....................ssssssssssss............... [ 41%]
........................................................................ [ 43%]
........................................................................ [ 45%]
........................................................................ [ 46%]
........................................................................ [ 48%]
........................................................................ [ 50%]
............................................................ssss...ssss. [ 52%]
........................................................................ [ 54%]
........................................................................ [ 56%]
........................................................................ [ 58%]
...............................................................ssss..... [ 60%]
........................................................................ [ 62%]
........................................................................ [ 64%]
...............................................sss...................... [ 66%]
........................................................................ [ 68%]
........................................................................ [ 70%]
........................................................................ [ 72%]
........................................................................ [ 74%]
........................................................................ [ 76%]
......................................................ssss.............. [ 78%]
...s.................................................................... [ 80%]
........................................................................ [ 82%]
........................................................................ [ 84%]
........................................................................ [ 86%]
........................................................................ [ 88%]
........................................................................ [ 90%]
...........................ss........................................... [ 92%]
........................................................................ [ 93%]
........................................................................ [ 95%]
........................................................................ [ 97%]
.........................................ssssssssssssssssssssssss....... [ 99%]
.....                                                                    [100%]
=================================== FAILURES ===================================
____ test_registry_view_rejects_old_session_with_current_directory_version _____

session = (<simpler.worker.Worker object at 0xfffe8564a6d0>, _MpiL3GroupRuntime(group_id='988f8d8e9cd148aebaafaed2dafcce2a', spe... <test_mpi_l3_group._FakeProcess object at 0xfffe85649c90>, <test_mpi_l3_group._ReadyMailbox object at 0xfffe8564a0d0>)

    def test_registry_view_rejects_old_session_with_current_directory_version(session):
        worker, group, _process, _mailbox = session
        registry = worker._get_endpoint_registry()
        views = [registry.mpi_session_for_endpoint(record) for record in registry.records]
        assert {view.session_epoch for view in views if view is not None} == {1}
        worker._validate_endpoint_registry(registry, registry.records)
        worker._invalidate_endpoint_registry()
        current = worker._get_endpoint_registry()
        assert group.session_epoch == 1
        with pytest.raises(ce.EndpointResolveError) as old_directory:
            worker._validate_endpoint_registry(registry, registry.records)
        assert old_directory.value.reason is ce.EndpointResolveReason.REGISTRY_MISMATCH
    
        # A fault-injected generation change isolates epoch checking from directory invalidation.
        group.session_epoch += 1
        with pytest.raises(ce.EndpointResolveError) as old_session:
            worker._validate_endpoint_registry(current, current.records)
        assert old_session.value.reason is ce.EndpointResolveReason.MPI_SESSION_UNAVAILABLE
        ctx = MaterializationContext(
            worker=worker,
            registry=current,
            plan=cast(Any, SimpleNamespace(ordered_members=tuple(record.identity for record in current.records))),
            layout=ce.RegionLayoutSpec(payload_bytes=64, counter_bytes=0),
        )
>       with pytest.raises(MaterializationRefusal) as refusal:
E       Failed: DID NOT RAISE MaterializationRefusal

tests/ut/py/test_mpi_session_epoch.py:93: Failed
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
SKIPPED [8] tests/ut/py/test_kernel_mode_c_api.py:1512: --platform required
SKIPPED [2] tests/ut/py/test_kernel_mode_c_api.py:531: --platform required
SKIPPED [16] tests/ut/py/test_kernel_mode_c_api.py:89: --platform required
SKIPPED [4] tests/ut/py/test_kernel_mode_c_api.py:1161: --platform required
SKIPPED [8] tests/ut/py/test_kernel_mode_c_api.py:880: --platform required
SKIPPED [16] tests/ut/py/test_kernel_mode_c_api.py:1547: --platform required
SKIPPED [2] tests/ut/py/test_kernel_mode_c_api.py:1204: --platform required
SKIPPED [4] tests/ut/py/test_kernel_mode_c_api.py:1228: --platform required
SKIPPED [2] tests/ut/py/test_kernel_mode_c_api.py:1182: --platform required
SKIPPED [4] tests/ut/py/test_kernel_mode_c_api.py:818: --platform required
SKIPPED [7] tests/ut/py/test_mpi_direct.py:764: could not import 'mpi4py': No module named 'mpi4py'
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:143: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:107: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:114: --platform required
SKIPPED [1] tests/ut/py/test_pipeline_contract_capacity.py:132: --platform required
SKIPPED [2] tests/ut/py/test_runtime_builder.py:1038: ASCEND_HOME_PATH not set; Ascend toolkit required for a2a3
SKIPPED [2] tests/ut/py/test_runtime_builder.py:1038: ASCEND_HOME_PATH not set; Ascend toolkit required for a5
SKIPPED [12] tests/ut/py/test_runtime_publication_launch.py:33: --platform required
SKIPPED [8] tests/ut/py/test_swimlane_export_run_identity.py:62: /home/l00896871/pypto-work/simpler/tests/ut/cpp/build/pyut_binaries.json is absent; the roundtrip runs in the C++ unit test CI step, after cmake -B tests/ut/cpp/build -S tests/ut/cpp
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
SKIPPED [8] tests/ut/py/test_worker_swimlane_level.py:98: --platform required
FAILED tests/ut/py/test_mpi_session_epoch.py::test_registry_view_rejects_old_session_with_current_directory_version
1 failed, 3505 passed, 171 skipped, 1 warning in 156.03s (0:02:36)
