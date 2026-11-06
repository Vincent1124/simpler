# Copyright (c) PyPTO Contributors.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

import json
from pathlib import Path
import runpy
import unittest
from unittest.mock import Mock

MODULE = runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/batch02-calibrate.py"))


class CalibrationEvidenceTest(unittest.TestCase):
    def test_round_zero_and_incomplete_rounds(self):
        header = "Round Host (us) Device (us)\n"
        rounds = "\n".join(f"{n} 123.0 12.0" for n in range(1, 56))
        self.assertEqual(len(MODULE["parse_timing"](header + "0 0.0 0.0\n" + rounds)), 55)
        with self.assertRaises(AssertionError):
            MODULE["parse_timing"](header + "0 1.0 0.0\n" + rounds)
        with self.assertRaises(AssertionError):
            MODULE["parse_timing"](header + rounds + "\n55 123.0 12.0")
        with self.assertRaises(AssertionError):
            MODULE["parse_timing"](header + "\n".join(rounds.splitlines()[:-1]))

    def test_serialized_field_names_and_warmup(self):
        phase = dict(admission_attempt=3, admission_failure=1, admission_success=2,
                     publish_entries=1, reserve_entries=1)
        worker = dict(worker_id=0, core_type=0, cluster_index=0, cluster_lane=0, type_rank=0,
                      phases=[phase, phase], timers={"pop": dict(count=1, sum=20, min=20, max=20, buckets=[1, 0, 0, 0])})
        files = []
        for sequence in range(55):
            data = dict(schema="hbg-resident-measure-v1", mode=2, period=64, timer_unit="cycles",
                        worker_count=1, run_epoch=sequence + 1, workers=[worker])
            files.append(Mock(stem=f"resident-123-456-{sequence}-{sequence + 1}",
                              read_text=Mock(return_value=json.dumps(data))))
        directory = Mock()
        directory.glob.return_value = files
        result = MODULE["validate_records"](directory, "sample64")
        self.assertEqual(result["workers"][0]["phases"][1]["admission_attempt"], 150)
        self.assertEqual(result["workers"][0]["timers"]["pop"]["count"], 50)
        self.assertEqual(result["workers"][0]["timers"]["pop"]["minimum"], 20)
        with self.assertRaises(AssertionError):
            MODULE["validate_records"](directory, "count")


if __name__ == "__main__":
    unittest.main()
