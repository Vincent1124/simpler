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
.............................................F......................ss.. [ 19%]
ss.............ssssssssssss..............................ssssssssss..... [ 21%]
...ssssssssssssssssssssssssssss.ssssssssssssssss........ssssssss........ [ 23%]
ssss.................................................................... [ 25%]
...........................................sssssss...................... [ 27%]
........................................................................ [ 29%]
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
....................F................................................... [ 76%]
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
__ test_mpirun_group_global_domain_uses_mpi_prepare_commit_without_l4_import ___

monkeypatch = <_pytest.monkeypatch.MonkeyPatch object at 0xfffe366db390>

    def test_mpirun_group_global_domain_uses_mpi_prepare_commit_without_l4_import(monkeypatch):
        from simpler.remote_l3_protocol import ControlName  # noqa: PLC0415
        from simpler.task_interface import CommBufferSpec  # noqa: PLC0415
    
        worker, resources, node_ids = _mpi_static_worker()
        calls = []
        commands = []
    
        def control(worker_id, control_name, payload, *, group=False):
            control_name = ControlName(control_name)
            if control_name is ControlName.COMM_INIT:
                init = decode_comm_init(payload)
                calls.append(("COMM_INIT", worker_id))
                return encode_comm_init_result(
                    resolve_global_comm_capability(
                        platform="a2a3sim",
                        profile=init.profile,
                        local_device_count=1,
                    )
                )
            assert control_name is ControlName.ALLOC_DOMAIN
            command = decode_domain_command(payload)
            commands.append(command)
            calls.append((command.phase, worker_id))
            if command.phase is GlobalDomainPhase.PREPARE_EXPORT:
                descriptors = tuple(
                    GlobalDomainDescriptor(
                        version=GLOBAL_DOMAIN_VERSION,
                        profile_id=GLOBAL_DOMAIN_PROFILE_IDS[command.profile],
                        domain_rank=member.domain_rank,
                        rank_count=len(command.members),
                        mapping_size=4096,
                        handle=f"/mpi-prepared-{member.domain_rank}".encode(),
                    )
                    for member in command.members
                )
                return encode_descriptor_table(descriptors)
            if command.phase is GlobalDomainPhase.IMPORT:
                raise RuntimeError("L4 broker IMPORT should not run for a full mpirun group")
            return b""
    
        monkeypatch.setattr(worker, "_global_domain_control", control)
        try:
>           handle = worker._allocate_global_domain(
                name="mpi-static",
                members=((node_ids[0], 0), (node_ids[1], 0)),
                window_size=4096,
                buffers=[CommBufferSpec("payload", "uint8", 4096, 4096)],
                retain_after_run=False,
            )

tests/ut/py/test_global_comm_domain.py:1523: 
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 
python/simpler/worker.py:11503: in _allocate_global_domain
    attachment_matrix = self._global_domain_attachment_matrix(
python/simpler/worker.py:11017: in _global_domain_attachment_matrix
    plan = resolver.plan(
python/simpler/comm_endpoints.py:928: in plan
    self._registry.require_mpi_sessions_available(members)
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 

self = <simpler.comm_endpoints.EndpointRegistry object at 0xfffe5b631090>
members = (EndpointRecord(identity=EndpointIdentity(session_instance_id=b'\x8cB\x80/\xd9V\x10\xfa', registry_epoch=1, endpoint_i...istry_epoch=1, endpoint_id=1), path='L4/L3[0]', deployment=<EndpointDeployment.HOST_CPU: 'HOST_CPU'>, node_scope_id=0))

    def require_mpi_sessions_available(self, members: Sequence[EndpointRecord]) -> None:
        """Check snapshot availability; only the owner can validate a live generation."""
        for member in members:
            view = self.mpi_session_for_endpoint(member)
            if view is not None and not view.available:
>               raise EndpointResolveError(
                    EndpointResolveReason.MPI_SESSION_UNAVAILABLE,
                    f"MPI session for {member.path} is not observed available",
                )
E               simpler.comm_endpoints.EndpointResolveError: MPI session for L4/L3[0]/L2[0] is not observed available

python/simpler/comm_endpoints.py:422: EndpointResolveError
______ test_shape_validation_rejects_unsupported_plan_and_non_vmm_backing ______

    def test_shape_validation_rejects_unsupported_plan_and_non_vmm_backing():
        unsupported = _context(
            _l3(device_ids=[0]),
            [ce.at("L3", ce.HOST_CPU), ce.at("L3/L2[0]", ce.DEVICE_AICPU)],
            ce.SingleOwner(provider=ce.at("L3", ce.HOST_CPU)),
        )
>       _assert_refusal(unsupported, RefusalReason.UNSUPPORTED_PLAN)

tests/ut/py/test_worker/test_comm_region.py:401: 
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 
tests/ut/py/test_worker/test_comm_region.py:104: in _assert_refusal
    validate_single_owner_region_shape(ctx)
python/simpler/comm_region.py:845: in validate_single_owner_region_shape
    _validate_registry_matches_worker(ctx)
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 

ctx = MaterializationContext(worker=<simpler.worker.Worker object at 0xfffe535bb450>, registry=<simpler.comm_endpoints.Endpo...plicit transfer materializer is not implemented yet'),)), layout=RegionLayoutSpec(payload_bytes=64, counter_bytes=128))

    def _validate_registry_matches_worker(ctx: MaterializationContext) -> None:
        worker_instance_id = getattr(ctx.worker, "_owner_instance_id", None)
        worker_epoch = getattr(ctx.worker, "_endpoint_registry_epoch", None)
        if worker_instance_id != ctx.registry.session_instance_id or worker_epoch != ctx.registry.registry_epoch:
            raise MaterializationRefusal(
                RefusalReason.REGISTRY_MISMATCH,
                "Region materialization requires a registry from the current worker endpoint epoch",
            )
        validator = getattr(ctx.worker, "_validate_endpoint_registry", None)
        if callable(validator):
            try:
                validator(ctx.registry, ())
>               members = tuple(ctx.registry._record_for(identity) for identity in ctx.plan.ordered_members)
                                                                                   ^^^^^^^^^^^^^^^^^^^^^^^^
E               AttributeError: 'UnsupportedRegionPlan' object has no attribute 'ordered_members'

python/simpler/comm_region.py:1347: AttributeError
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
FAILED tests/ut/py/test_global_comm_domain.py::test_mpirun_group_global_domain_uses_mpi_prepare_commit_without_l4_import
FAILED tests/ut/py/test_worker/test_comm_region.py::test_shape_validation_rejects_unsupported_plan_and_non_vmm_backing
2 failed, 3504 passed, 171 skipped, 1 warning in 165.04s (0:02:45)
