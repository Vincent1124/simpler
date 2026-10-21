(.venv) [l00896871@master37 simpler]$ python -m pytest -q \
> tests/ut/py/test_mpi_session_epoch.py \
> tests/ut/py/test_mpi_l3_group.py \
> tests/ut/py/test_worker/test_comm_endpoints.py \
> tests/ut/py/test_worker/test_comm_region.py 
....................................................................................................................................................................F.......                                                                                                                [100%]
============================================================================================================================================ FAILURES =============================================================================================================================================
_______________________________________________________________________________________________________________ test_shape_validation_rejects_unsupported_plan_and_non_vmm_backing ________________________________________________________________________________________________________________

    def test_shape_validation_rejects_unsupported_plan_and_non_vmm_backing():
        unsupported = _context(
            _l3(device_ids=[0]),
            [ce.at("L3", ce.HOST_CPU), ce.at("L3/L2[0]", ce.DEVICE_AICPU)],
            ce.SingleOwner(provider=ce.at("L3", ce.HOST_CPU)),
        )
>       _assert_refusal(unsupported, RefusalReason.UNSUPPORTED_PLAN)

tests/ut/py/test_worker/test_comm_region.py:401: 
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
tests/ut/py/test_worker/test_comm_region.py:104: in _assert_refusal
    validate_single_owner_region_shape(ctx)
python/simpler/comm_region.py:845: in validate_single_owner_region_shape
    _validate_registry_matches_worker(ctx)
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _

ctx = MaterializationContext(worker=<simpler.worker.Worker object at 0xffff9ed60850>, registry=<simpler.comm_endpoints.Endpo...plicit transfer materializer is not implemented yet'),)), layout=RegionLayoutSpec(payload_bytes=64, counter_bytes=128))

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
===================================================================================================================================== short test summary info =====================================================================================================================================
FAILED tests/ut/py/test_worker/test_comm_region.py::test_shape_validation_rejects_unsupported_plan_and_non_vmm_backing - AttributeError: 'UnsupportedRegionPlan' object has no attribute 'ordered_members'
1 failed, 171 passed in 1.25s
