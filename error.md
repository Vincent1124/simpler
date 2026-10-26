=================================== FAILURES ===================================
_ test_real_xdist_controller_refuses_strict_environment_before_bodies[arguments0] _

tmp_path = PosixPath('/tmp/pytest-of-l00896871/pytest-7/test_real_xdist_controller_ref0')
arguments = ('-n', '2')

    @pytest.mark.parametrize("arguments", [("-n", "2"), ("--dist", "load", "--tx", "2*popen")])
    def test_real_xdist_controller_refuses_strict_environment_before_bodies(tmp_path, arguments):
        result, plan, _ = _run_pytest(
            tmp_path, _recording_direct_source(tmp_path), "-p", "xdist.plugin", *arguments, "--max-worker-restart=0"
        )
        assert result.returncode != 0, result.stdout + result.stderr
        assert not list(tmp_path.glob("body-*"))
        assert not plan.exists()
>       assert "direct pytest execution only" in result.stdout + result.stderr
E       assert 'direct pytest execution only' in ('' + 'Traceback (most recent call last):\n  File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/c...line 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n')
E        +  where '' = CompletedProcess(args=['/home/l00896871/pypto-work/.venv/bin/python', '-m', 'pytest', '/tmp/pytest-of-l00896871/pytest...ine 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n').stdout
E        +  and   'Traceback (most recent call last):\n  File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/c...line 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n' = CompletedProcess(args=['/home/l00896871/pypto-work/.venv/bin/python', '-m', 'pytest', '/tmp/pytest-of-l00896871/pytest...ine 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n').stderr

tests/ut/py/test_test_environments.py:459: AssertionError
_ test_real_xdist_controller_refuses_strict_environment_before_bodies[arguments1] _

tmp_path = PosixPath('/tmp/pytest-of-l00896871/pytest-7/test_real_xdist_controller_ref1')
arguments = ('--dist', 'load', '--tx', '2*popen')

    @pytest.mark.parametrize("arguments", [("-n", "2"), ("--dist", "load", "--tx", "2*popen")])
    def test_real_xdist_controller_refuses_strict_environment_before_bodies(tmp_path, arguments):
        result, plan, _ = _run_pytest(
            tmp_path, _recording_direct_source(tmp_path), "-p", "xdist.plugin", *arguments, "--max-worker-restart=0"
        )
        assert result.returncode != 0, result.stdout + result.stderr
        assert not list(tmp_path.glob("body-*"))
        assert not plan.exists()
>       assert "direct pytest execution only" in result.stdout + result.stderr
E       assert 'direct pytest execution only' in ('' + 'Traceback (most recent call last):\n  File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/c...line 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n')
E        +  where '' = CompletedProcess(args=['/home/l00896871/pypto-work/.venv/bin/python', '-m', 'pytest', '/tmp/pytest-of-l00896871/pytest...ine 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n').stdout
E        +  and   'Traceback (most recent call last):\n  File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/c...line 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n' = CompletedProcess(args=['/home/l00896871/pypto-work/.venv/bin/python', '-m', 'pytest', '/tmp/pytest-of-l00896871/pytest...ine 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n').stderr

tests/ut/py/test_test_environments.py:459: AssertionError
_______ test_real_xdist_n_zero_preserves_direct_collection_and_receipts ________

tmp_path = PosixPath('/tmp/pytest-of-l00896871/pytest-7/test_real_xdist_n_zero_preserv0')

    def test_real_xdist_n_zero_preserves_direct_collection_and_receipts(tmp_path):
        result, plan, xml = _run_pytest(tmp_path, _recording_direct_source(tmp_path), "-p", "xdist.plugin", "-n", "0")
>       _success(result)

tests/ut/py/test_test_environments.py:464: 
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 

result = CompletedProcess(args=['/home/l00896871/pypto-work/.venv/bin/python', '-m', 'pytest', '/tmp/pytest-of-l00896871/pytest...ine 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n')

    def _success(result):
>       assert result.returncode == 0, result.stdout + result.stderr
E       AssertionError: Traceback (most recent call last):
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 913, in import_plugin
E             mod = importlib.import_module(importspec)
E                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/usr/lib64/python3.11/importlib/__init__.py", line 126, in import_module
E             return _bootstrap._gcd_import(name[level:], package, level)
E                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "<frozen importlib._bootstrap>", line 1204, in _gcd_import
E           File "<frozen importlib._bootstrap>", line 1176, in _find_and_load
E           File "<frozen importlib._bootstrap>", line 1126, in _find_and_load_unlocked
E           File "<frozen importlib._bootstrap>", line 241, in _call_with_frames_removed
E           File "<frozen importlib._bootstrap>", line 1204, in _gcd_import
E           File "<frozen importlib._bootstrap>", line 1176, in _find_and_load
E           File "<frozen importlib._bootstrap>", line 1140, in _find_and_load_unlocked
E         ModuleNotFoundError: No module named 'xdist'
E         
E         The above exception was the direct cause of the following exception:
E         
E         Traceback (most recent call last):
E           File "<frozen runpy>", line 198, in _run_module_as_main
E           File "<frozen runpy>", line 88, in _run_code
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/pytest/__main__.py", line 9, in <module>
E             raise SystemExit(_console_main())
E                              ^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 253, in _console_main
E             code = _main(prog=_get_prog_name(sys.argv))
E                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 223, in _main
E             config = _prepareconfig(new_args, plugins, prog=prog)
E                      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 410, in _prepareconfig
E             config: Config = pluginmanager.hook.pytest_cmdline_parse(
E                              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/pluggy/_caller.py", line 197, in __call__
E             return self._hookexec(
E                    ^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/pluggy/_manager.py", line 183, in _hookexec
E             return self._inner_hookexec(hook_name, methods, kwargs, firstresult)
E                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/pluggy/_execution.py", line 195, in _multicall
E             raise exception
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/pluggy/_execution.py", line 167, in _multicall
E             teardown.throw(exception)
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/helpconfig.py", line 124, in pytest_cmdline_parse
E             config = yield
E                      ^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/pluggy/_execution.py", line 147, in _multicall
E             res = hook_impl.function(*args)
E                   ^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 1232, in pytest_cmdline_parse
E             self.parse(args)
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 1576, in parse
E             self.pluginmanager.consider_preparse(args, exclude_only=False)
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 835, in consider_preparse
E             self.consider_pluginarg(parg)
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 864, in consider_pluginarg
E             self.import_plugin(arg, consider_entry_points=True)
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 920, in import_plugin
E             raise ImportError(
E           File "/home/l00896871/pypto-work/.venv/lib64/python3.11/site-packages/_pytest/config/__init__.py", line 913, in import_plugin
E             mod = importlib.import_module(importspec)
E                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "/usr/lib64/python3.11/importlib/__init__.py", line 126, in import_module
E             return _bootstrap._gcd_import(name[level:], package, level)
E                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E           File "<frozen importlib._bootstrap>", line 1204, in _gcd_import
E           File "<frozen importlib._bootstrap>", line 1176, in _find_and_load
E           File "<frozen importlib._bootstrap>", line 1126, in _find_and_load_unlocked
E           File "<frozen importlib._bootstrap>", line 241, in _call_with_frames_removed
E           File "<frozen importlib._bootstrap>", line 1204, in _gcd_import
E           File "<frozen importlib._bootstrap>", line 1176, in _find_and_load
E           File "<frozen importlib._bootstrap>", line 1140, in _find_and_load_unlocked
E         ImportError: Error importing plugin "xdist.plugin": No module named 'xdist'
E         
E       assert 1 == 0
E        +  where 1 = CompletedProcess(args=['/home/l00896871/pypto-work/.venv/bin/python', '-m', 'pytest', '/tmp/pytest-of-l00896871/pytest...ine 1140, in _find_and_load_unlocked\nImportError: Error importing plugin "xdist.plugin": No module named \'xdist\'\n').returncode

tests/ut/py/test_test_environments.py:130: AssertionError
_ test_real_xdist_workerinput_is_rejected_even_after_worker_options_are_reset __

tmp_path = PosixPath('/tmp/pytest-of-l00896871/pytest-7/test_real_xdist_workerinput_is0')

    def test_real_xdist_workerinput_is_rejected_even_after_worker_options_are_reset(tmp_path):
        # A default controller reaches a real xdist worker. The worker selects the
        # strict profile before the production configure hook; workerinput is real.
        extra = f"""
    import json
    original_configure = pytest_configure
    def pytest_configure(config):
        if hasattr(config, "workerinput"):
            record = {{"workerinput": config.workerinput,
                       "numprocesses": config.getoption("numprocesses", default=None),
                       "dist": config.getoption("dist", default=None)}}
            Path({str(tmp_path)!r}, "worker-" + config.workerinput["workerid"] + ".json").write_text(json.dumps(record))
            config.option.test_environment = "torch_npu"
        original_configure(config)
    """
        result, _, _ = _run_pytest(
            tmp_path,
            _recording_direct_source(tmp_path),
            "-p",
            "xdist.plugin",
            "-n",
            "1",
            "--max-worker-restart=0",
            profile="default",
            extra_conftest=extra,
        )
        records = [json.loads(path.read_text()) for path in tmp_path.glob("worker-*.json")]
>       assert records and records[0]["workerinput"]["workerid"] == "gw0"
E       assert ([])

tests/ut/py/test_test_environments.py:499: AssertionError
__________ test_mpi_direct_registry_registers_rank_hosts_and_devices ___________

    def test_mpi_direct_registry_registers_rank_hosts_and_devices():
        worker = _l4_with_mpi_direct(
            hosts=("localhost", "10.0.0.7", "10.0.0.7"),
            device_ids_by_rank=((12, 13), (14,), (15,)),
        )
>       registry = worker._get_endpoint_registry()
                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

tests/ut/py/test_worker/test_comm_endpoints.py:572: 
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 
python/simpler/worker.py:7874: in _get_endpoint_registry
    self._observe_mpi_sessions()
_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ 

self = <simpler.worker.Worker object at 0xfffe31bcaf50>

    def _observe_mpi_sessions(self) -> None:
        with self._hierarchical_start_cv:
            session = self._mpi_direct_session
            if session is None or session.retired:
                return
            epoch = session.session_epoch
>       closed, failure, unavailable = session.hub.session_snapshot()
                                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E       AttributeError: 'object' object has no attribute 'session_snapshot'

python/simpler/worker.py:7845: AttributeError
=============================== warnings summary ===============================
tests/ut/py/test_worker/test_sub_tensor_args.py::test_mapped_args_from_blob_delivers_tensors_and_scalars
  /home/l00896871/pypto-work/simpler/tests/ut/py/test_worker/test_sub_tensor_args.py:134: UserWarning: The given buffer is not writable, and PyTorch does not support non-writable tensors. This means you can write to the underlying (supposedly non-writable) buffer using the tensor. You may want to copy the buffer to protect its data or make it writable before converting it to a tensor. This type of warning will be suppressed for the rest of this program. (Triggered internally at /__w/pytorch/pytorch/torch/csrc/utils/tensor_new.cpp:1556.)
    assert torch.frombuffer(args[0].buffer, dtype=torch.float32, count=4).tolist() == [1.0, 2.0, 3.0, 4.0]

-- Docs: https://docs.pytest.org/en/stable/how-to/capture-warnings.html
