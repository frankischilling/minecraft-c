"""Historical CPU receipt audit and corruption controls; no native execution."""
import copy
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import benchmark_static_cpu as benchmark

CAPTURE_DIGESTS = {
    "block_ready_final": "9f5087d049cbc1004354e96b095aca89aaa3bab527f67ea3b71d17ed3d508507",
    "cache_prototype1": "ac8b927442059b7cb2ec9385524379cdb55ff4c5ca6ce82f58b6841e6806be1d",
    "cache_prototype2": "b15c62097ccb1cda711ed71a6bf5ec40fd4563005075ea500b8d9bf4e2cac8da",
    "cache_final3": "266cbc5b8e9c12c133d8672e929f6ce59b2f46750dc50dac1cba49284bd85cf5",
}


class StaticCpuBenchmarks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.receipt = json.loads(benchmark.RECEIPT.read_text(encoding="utf-8"))
        cls.computed = benchmark.audit_receipt(cls.receipt)

    def reject(self, mutate, recapture=True):
        receipt = copy.deepcopy(self.receipt)
        mutate(receipt)
        if recapture:
            for capture in receipt["captures"]:
                capture["projection_sha256"] = benchmark.canonical(
                    {k: v for k, v in capture.items() if k != "projection_sha256"})
        with self.assertRaises((AssertionError, ValueError, KeyError)):
            benchmark.audit_receipt(receipt)

    @staticmethod
    def replace_stream(receipt, index, before, after):
        process = receipt["captures"][index]["processes"][0]
        assert before in process["stdout"]
        process["stdout"] = process["stdout"].replace(before, after, 1)
        process["stdout_sha256"] = benchmark.digest(process["stdout"].encode("ascii"))

    def test_complete_historical_receipt(self):
        self.assertEqual({c["name"]: c["projection_sha256"] for c in self.receipt["captures"]},
                         CAPTURE_DIGESTS)
        self.assertEqual(sum(len(c["processes"]) for c in self.receipt["captures"]), 32)
        self.assertEqual(sum(len(benchmark.parse_block(p["stdout"]) if c["kind"] == "block"
                                 else benchmark.parse_cache(p["stdout"]))
                             for c in self.receipt["captures"] for p in c["processes"]), 440)

    def test_all_three_cache_histories_and_tradeoff(self):
        for name in ("cache_prototype1", "cache_prototype2", "cache_final3"):
            rows = self.computed[name]
            self.assertTrue(all(row["median_change_percent"] < 0 for row in rows[:3]))
            self.assertTrue(all(row["median_change_percent"] > 0 for row in rows[3:]))
        self.assertAlmostEqual(self.computed["block_ready_final"]["median_change_percent"],
                               -82.81427028817858)

    def test_cache_native_row_corruptions(self):
        cases = (
            ("checks=780", "checks=779"), ("immutable=1", "immutable=0"),
            ("CACHE_WARMUP count=513 pattern=mixed calls=4", "CACHE_WARMUP count=513 pattern=mixed calls=3"),
            ("sample=0", "sample=1"), ("calls=381", "calls=380"),
            ("keys=14732841118490858905", "keys=14732841118490858904"),
            ("baseline-pairs-per-call=131328", "baseline-pairs-per-call=131327"),
            ("sink=8356952006182495849", "sink=8356952006182495848"),
            ("frequency=10000000", "frequency=0"), ("cpu-ns=15625000", "cpu-ns=-1"),
            ("pattern=one-bucket", "pattern=mixed"),
        )
        for before, after in cases:
            with self.subTest(before=before):
                self.reject(lambda r: self.replace_stream(r, 3, before, after))

    def test_block_native_row_corruptions(self):
        for before, after in (("16 achievements", "15 achievements"), ("phase=READY", "phase=RAW"),
                              ("rounds=10000", "rounds=9999"), ("fields=96", "fields=95"),
                              ("calls=960000", "calls=959999"), ("checksum=46560000", "checksum=0"),
                              ("controls=1011", "controls=1010"), ("live-cells=1", "live-cells=0"),
                              ("source-order=1", "source-order=0"), ("frequency=10000000", "frequency=0")):
            with self.subTest(before=before):
                self.reject(lambda r: self.replace_stream(r, 0, before, after))

    def test_process_and_metadata_corruptions(self):
        cases = [
            lambda r: r["captures"].pop(1),
            lambda r: r["captures"][3]["processes"].pop(),
            lambda r: r["captures"][3]["processes"][0].update(position=2),
            lambda r: r["captures"][3]["processes"][0].update(exit=1),
            lambda r: r["captures"][3]["processes"][0].update(timeout=True),
            lambda r: r["captures"][3].update(status="FAIL"),
            lambda r: r["captures"][1].update(qualification="FINAL_FINITE_CPU_WITH_COLLISION_TRADEOFF"),
            lambda r: r["captures"][3]["summary"][3].update(median_change_percent=0),
            lambda r: r["captures"][3]["processes"][0].update(stdout_sha256="0" * 64),
            lambda r: r["captures"][3]["processes"][0].update(exe_sha256="0" * 64),
            lambda r: r["captures"][3]["provenance"]["products_sha256"].update({"observer.obj": "0" * 64}),
            lambda r: r["captures"][3]["provenance"].update(observer_reused=False),
            lambda r: r["captures"][3]["provenance"]["selected_inputs_sha256"].update({"../escape": "0" * 64}),
            lambda r: r["captures"][3]["provenance"]["actual_compile_records"][0].update(flags=[]),
            lambda r: r["publication_sources"]["tests/benchmarks/cache_validation.c"].update(sha256="0" * 64),
            lambda r: r["retained_block_fixture_failure"].update(baseline_exit=0),
        ]
        for index, mutate in enumerate(cases):
            with self.subTest(index=index):
                self.reject(mutate)

    def test_raw_newline_changes_are_detected(self):
        self.reject(lambda r: r["captures"][3]["processes"][0].update(
            stdout=r["captures"][3]["processes"][0]["stdout"].replace("\r\n", "\n")))

    def test_public1032_and_historical1011_are_distinct(self):
        text = self.receipt["public_caller_smoke"]["persisted_streams"]["block.stdout"]["text"]
        with self.assertRaises(AssertionError):
            benchmark.parse_block(text)
        actual = benchmark.parse_block(text, benchmark.PUBLIC_BLOCK_CONTROLS)[0]
        self.assertEqual((actual["rounds"], actual["calls"], actual["checksum"], actual["controls"]),
                         (1, 96, 4656, 1032))
        self.reject(lambda r: r["captures"][0].update(block_controls=1032))
        self.reject(lambda r: r["public_caller_smoke"].update(qualification="TIMING_CREDIT"))
        self.reject(lambda r: r["public_caller_smoke"]["persisted_streams"]["block.stdout"].update(encoding="ascii"))

    def test_fresh_collector_preserves_failed_processes(self):
        for outcome in (SimpleNamespace(returncode=9, stdout=b"reached-prefix\r\n", stderr=b"refusal\r\n"),
                        subprocess.TimeoutExpired(["fixture"], 300, output=b"timeout-prefix", stderr=b"timeout"),
                        SimpleNamespace(returncode=0, stdout=b"\xff", stderr=b""),
                        OSError("authored launch refusal")):
            with self.subTest(outcome=type(outcome).__name__), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                exe = root/"authored-not-executed.bin"
                exe.write_bytes(b"fixture bytes for hash guard only")
                artifacts = root/"attempt"
                kwargs = {"side_effect": outcome} if isinstance(outcome, Exception) else {"return_value": outcome}
                with mock.patch.object(benchmark.subprocess, "run", **kwargs):
                    with self.assertRaises(AssertionError):
                        benchmark.collect("cache", {name: exe for name in benchmark.VARIANTS}, 1, 10000, artifacts)
                report = json.loads((artifacts/"report.json").read_text())
                self.assertEqual(report["status"], "FAIL")
                self.assertEqual(len(report["processes"]), 1)
                self.assertTrue((artifacts/"round-1-1-baseline.stdout").exists())
                self.assertTrue((artifacts/"round-1-1-baseline.stderr").exists())
                with self.assertRaises(AssertionError):
                    benchmark.collect("cache", {name: exe for name in benchmark.VARIANTS}, 1, 10000, artifacts)


if __name__ == "__main__":
    unittest.main()
