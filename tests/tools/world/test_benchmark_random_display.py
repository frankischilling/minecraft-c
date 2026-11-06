"""Reject incomplete or altered fixed-work CPU observations without native execution."""
from pathlib import Path
import json
import statistics
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from benchmark_random_display import ROOT, build_binding, percentile, read_rows


def observation():
    rows = []
    for workload in ("air", "stone"):
        state = 0x5DEECE66D
        for ordinal in range(23):
            before = state
            last = []
            for draw in range(6000):
                state = (state * 0x5DEECE66D + 11) & ((1 << 48) - 1)
                if draw >= 5994:
                    last.append(state >> 44)
            calls = 1000 if workload == "stone" else 0
            rows.append(f"{workload} {ordinal} {int(ordinal >= 2)} 1500 10000000 "
                        f"{before:012x} {state:012x} 1000 1000 6000 {calls} {calls} "
                        f"{last[0] - last[1]} {64 + last[2] - last[3]} {last[4] - last[5]}")
    return rows


class RandomDisplayBenchmarkPacket(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = observation()

    def parse(self, rows):
        with tempfile.TemporaryDirectory(prefix="random-display-cpu-packet-") as directory:
            path = Path(directory) / "native.txt"
            path.write_text("\n".join(rows) + "\n", encoding="ascii")
            return read_rows(path)

    def changed(self, index, field, value):
        rows = self.rows.copy()
        cells = rows[index].split()
        cells[field] = value
        rows[index] = " ".join(cells)
        return rows

    def test_complete_observation(self):
        parsed = self.parse(self.rows)
        self.assertEqual(len(parsed), 46)
        self.assertEqual(sum(row["timed"] for row in parsed), 42)

    def test_missing_or_duplicate_samples(self):
        for rows in (self.rows[:-1], self.rows[1:], self.rows + self.rows[:1],
                     self.rows[:2] + self.rows[3:], self.rows[::-1]):
            with self.subTest(count=len(rows)), self.assertRaises(AssertionError):
                self.parse(rows)

    def test_changed_work_and_timing(self):
        mutations = ((0, 0, "lava"), (1, 1, "0"), (0, 2, "1"), (2, 2, "0"),
                     (0, 3, "0"), (0, 3, "-1"), (0, 4, "0"), (0, 7, "999"),
                     (0, 8, "999"), (0, 9, "5999"), (0, 10, "1000"),
                     (23, 10, "0"), (23, 11, "999"))
        for index, field, value in mutations:
            with self.subTest(index=index, field=field, value=value), self.assertRaises(AssertionError):
                self.parse(self.changed(index, field, value))

    def test_changed_random_or_position(self):
        mutations = ((0, 5, "000000000000"), (1, 5, "0005deece66d"),
                     (0, 6, "000000000000"), (0, 5, "5DEECE66D"),
                     (0, 12, "1"), (0, 13, "64"), (0, 14, "0"))
        for index, field, value in mutations:
            with self.subTest(index=index, field=field, value=value), self.assertRaises(AssertionError):
                self.parse(self.changed(index, field, value))

    def test_retained_windows_cohort(self):
        receipt = json.loads((ROOT / "tests/benchmarks/random_display_windows.json").read_text(
            encoding="utf-8"))
        self.assertEqual(receipt["schema"], 1)
        self.assertEqual(receipt["status"], "PASS")
        self.assertEqual(receipt["mode"], "timing")
        self.assertEqual(receipt["input_before_sha256"], receipt["input_after_sha256"])
        for path, expected in receipt["build"]["product_sha256"].items():
            self.assertEqual(receipt["input_before_sha256"][path], expected)
        self.assertEqual(len(receipt["build"]["compilation"]), 4)
        controls = receipt["controls"]
        self.assertEqual(set(controls), {"detailed", "failure"})
        for control in controls.values():
            self.assertEqual(control["checks"], 72124)
            self.assertEqual(control["profiles"], 114)
        self.assertEqual(controls["detailed"]["equal_input_projection_sha256"],
                         controls["failure"]["equal_input_projection_sha256"])
        self.assertEqual([(p["round"], p["variant"]) for p in receipt["processes"]],
                         [(index, variant) for index in range(4)
                          for variant in (("detailed", "failure") if index % 2 == 0 else
                                          ("failure", "detailed"))])
        for process in receipt["processes"]:
            self.assertEqual(process["status"], "PASS")
            self.assertEqual(process["returncode"], 0)
            self.assertEqual(process["checks"], 276606)
            self.assertEqual(process["input_before_sha256"], process["input_after_sha256"])
            rows = []
            for row in process["rows"]:
                rows.append(" ".join(map(str, (
                    row["workload"], row["ordinal"], int(row["timed"]), row["ticks"],
                    row["frequency"], row["random_before"], row["random_after"],
                    row["samples_started"], row["samples_returned"], row["world_draws"],
                    row["display_calls"], row["display_returns"], *row["final_coordinates"]))))
            self.assertEqual(self.parse(rows), process["rows"])
        for workload, summary in receipt["summary"].items():
            for variant in ("detailed", "failure"):
                values = [r["milliseconds"] for p in receipt["processes"]
                          if p["variant"] == variant for r in p["rows"]
                          if r["workload"] == workload and r["timed"]]
                self.assertEqual(summary[variant], {
                    "samples": 84, "median_ms": statistics.median(values),
                    "mean_ms": statistics.mean(values), "p95_ms": percentile(values, .95),
                    "maximum_ms": max(values)})
            baseline = summary["detailed"]["median_ms"]
            self.assertEqual(summary["median_reduction_percent"],
                             100 * (baseline - summary["failure"]["median_ms"]) / baseline)

    def build_fixture(self, directory, export):
        directory = Path(directory)
        shared = "CMakeFiles/observer/random_display.c.o"
        lines = []
        executables = {}
        for variant in ("detailed", "failure"):
            executable = directory / f"mc_benchmark_random_display_{variant}"
            executable.write_bytes(b"authored binary identity")
            executables[variant] = executable
            adapter = f"CMakeFiles/{variant}/random_display_{variant}.c.o"
            lines.append(f"build {executable.name}: C_EXECUTABLE_LINKER {shared} {adapter} "
                         "| libsimulation.a || observer")
            product = directory / adapter
            product.parent.mkdir(parents=True)
            product.write_bytes(variant.encode("ascii"))
        product = directory / shared
        product.parent.mkdir(parents=True)
        product.write_bytes(b"shared compiled observer")
        (directory / "libsimulation.a").write_bytes(b"identical dependency")
        (directory / "build.ninja").write_text("\n".join(lines), encoding="ascii")
        (directory / "CMakeFiles/rules.ninja").write_text("# Authored rule identity\n", encoding="ascii")
        commands = [{"file": path, "command": "authored compilation inspection"} for path in (
            "/source/src/random_display.c", "/source/tests/random_display.c",
            "/source/tests/random_display_detailed.c", "/source/tests/random_display_failure.c")]
        raw = json.dumps(commands).encode("ascii")
        if export:
            (directory / "compile_commands.json").write_bytes(raw)
        return executables, raw

    def test_exported_build_commands_need_no_ninja_process(self):
        with tempfile.TemporaryDirectory(prefix="random-display-build-binding-") as directory:
            executables, _ = self.build_fixture(directory, True)
            with patch("benchmark_random_display.subprocess.run") as run:
                binding, products = build_binding(executables)
            run.assert_not_called()
            self.assertEqual(len(binding["compilation"]), 4)
            self.assertIn(Path(directory) / "compile_commands.json", products)

    def test_ninja_commands_without_cmake_export(self):
        with tempfile.TemporaryDirectory(prefix="random-display-build-binding-") as directory:
            executables, raw = self.build_fixture(directory, False)
            result = subprocess.CompletedProcess(["ninja", "-t", "compdb"], 0, raw, b"")
            with patch("benchmark_random_display.subprocess.run", return_value=result) as run:
                binding, products = build_binding(executables)
            self.assertEqual(run.call_args.args[0], ["ninja", "-t", "compdb"])
            self.assertEqual(run.call_args.kwargs["cwd"], Path(directory))
            self.assertEqual(len(binding["compilation"]), 4)
            self.assertIn(Path(directory) / "CMakeFiles/rules.ninja", products)
            self.assertNotIn(Path(directory) / "compile_commands.json", products)

    def test_failed_or_incomplete_ninja_inspection(self):
        with tempfile.TemporaryDirectory(prefix="random-display-build-binding-") as directory:
            executables, raw = self.build_fixture(directory, False)
            results = (subprocess.CompletedProcess([], 1, raw, b""),
                       subprocess.CompletedProcess([], 0, raw, b"inspection diagnostic"),
                       subprocess.CompletedProcess([], 0, b"[]", b""))
            for result in results:
                with self.subTest(returncode=result.returncode, stderr=result.stderr), \
                        patch("benchmark_random_display.subprocess.run", return_value=result), \
                        self.assertRaises(AssertionError):
                    build_binding(executables)


if __name__ == "__main__":
    unittest.main()
