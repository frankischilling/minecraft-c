"""Audit the retained Registry random-tick CPU captures without native execution."""
import copy
from functools import lru_cache
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys
import unittest
from unittest import mock

import benchmark_random_tick as benchmark
from test_random_tick import SCENARIOS, STREAMS


ROOT = Path(__file__).resolve().parents[3]
RECEIPT = ROOT / "tests/benchmarks/world_registry_tick_windows.json"
VARIANTS = ("baseline", "candidate")
CAPTURE_DIGESTS = {
    "warned_preliminary": "cf01867c583fcadddf7e3f695b41f36737868356aeded7488dcedc6d65629b13",
    "clean_primary": "a6efcbc5b7c5fccace23970bf851af3e20cf07171aef0016b767e87ca4b267d8",
}
COHORT_DIGESTS = {
    ("warned_preliminary", "initial"): "5ad32bf24ac7a31ddb2ce77afe4ef57ce176b1249d2167a14917b55515c38ba6",
    ("warned_preliminary", "confirmation"): "0ab2ef79223415f76e8d3968a81d60cf1e1a756a881e9f756125d68a4418849f",
    ("clean_primary", "initial"): "03154e5877f2504c3173c559316f5363a3c07090cf5c1dc253307470075e58fb",
    ("clean_primary", "confirmation"): "0b502d8d1b66583c8d9cee6d00543f4078615a8781383b74b6836c1c437fa678",
}
CONTROL_DIGESTS = {
    ("warned_preliminary", "initial", "baseline"): "4723b433263d52cb23d85d2670f97a09bb66701475104841147331344c1bffd0",
    ("warned_preliminary", "initial", "candidate"): "067784c5f93f78a553b42b298957affd4d510d48e36367d93ec529a78eabe1b7",
    ("warned_preliminary", "confirmation", "baseline"): "569004ee21b911e1dfab52b9e5144588bf220f8032af9fba1bca24aab89d3f2f",
    ("warned_preliminary", "confirmation", "candidate"): "6eeef6cad38d886ca39aedc18064c040c29414119d080332d9492fec3f175be5",
    ("clean_primary", "initial", "baseline"): "b019d95a38e92fa487e0787379f3ef94107fb15dc5a423749bd827e2592e2099",
    ("clean_primary", "initial", "candidate"): "c4920a277d0d748057d138ba571ed7494c925381cb55a841eb940da4aecc3a1d",
    ("clean_primary", "confirmation", "baseline"): "56d615894e8491e9cb9f217ed9677257825c56ff5d298d5190de480ae90cf7f9",
    ("clean_primary", "confirmation", "candidate"): "f292abfb691475bf11a9a750957dbf6cec03936273f5950b5664fe6138c80a01",
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value):
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                             ensure_ascii=True, allow_nan=False).encode("ascii"))


def hash_inventory(values):
    require(isinstance(values, dict) and values, "nonempty hash inventory required")
    require(all(isinstance(value, str) and re.fullmatch("[0-9a-f]{64}", value)
                for value in values.values()), "complete SHA256 identities required")
    require(all(isinstance(name, str) and name and "\\" not in name and ":" not in name
                and not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts
                for name in values), "portable projected identities required")


def same_statistics(computed, captured):
    require(set(computed) == set(captured), "complete captured statistics required")
    for name, value in computed.items():
        if isinstance(value, dict):
            same_statistics(value, captured[name])
        else:
            require(isinstance(captured[name], (int, float)) and
                    math.isclose(value, captured[name], rel_tol=1e-12, abs_tol=1e-9),
                    "captured statistics differ from complete raw rows")


def control_signature(control):
    """Output identity, including every callback, failure and binary observation."""
    return {name: control[name] for name in (
        "status", "checks", "profiles", "original_comparison", "native_rows", "native_stdout",
        "native_stderr", "actual", "observations", "outputs_sha256")}


def audit_control(control, executable):
    require(control["status"] == "PASS" and control["profiles"] == 15 and
            control["checks"] == 89842, "complete successful native control inventory required")
    require(control["original_comparison"] is None, "no new original-game comparison was captured")
    before, after = control["input_before_sha256"], control["input_after_sha256"]
    hash_inventory(before)
    hash_inventory(after)
    require(len(before) == 54 and before == after, "all control inputs must remain unchanged")
    require(before["executable"] == executable, "control executable binding differs")

    observations, outputs = control["observations"], control["outputs_sha256"]
    expected = {f"case{index}-{stream}.bin" for index in range(15) for stream in STREAMS}
    require(set(observations) == expected, "all seventy-five binary observations required")
    require(set(outputs) == expected | {"native.txt", "native.stdout", "native.stderr", "actual.json"},
            "complete native output hash inventory required")
    hash_inventory(outputs)
    for name, observed in observations.items():
        require(set(observed) == {"sha256", "bytes"} and
                type(observed["bytes"]) is int and 0 <= observed["bytes"] <= 32 * 1024 * 1024,
                "finite complete binary observation required")
        require(observed["sha256"] == outputs[name], "binary observation/output binding differs")

    rows, actual = control["native_rows"], control["actual"]
    require(len(rows) == 94 and all(isinstance(row, str) and "\n" not in row and "\r" not in row
                                   for row in rows), "complete native transcript required")
    require(sha256(("\n".join(rows) + "\n").encode("ascii")) == outputs["native.txt"],
            "native transcript binding differs")
    for stream in ("stdout", "stderr"):
        require(sha256(control["native_" + stream].encode("ascii")) == outputs["native." + stream],
                "native stream binding differs")
    require(control["native_stderr"] == "" and control["native_stdout"].endswith(
        "World random tick: 89842 checks;14 paired profiles and native h-ignored control\r\n"),
        "successful native control footer required")

    prefixes = {"frame_rows": ("RANDOM_FRAME ",), "event_rows": (
        "RANDOM_SOUND ", "RANDOM_CELL ", "RANDOM_RANGE "),
        "native_state_rows": ("NATIVE_RANDOM_STATE ",), "failure_rows": ("RANDOM_FAILURE ",)}
    require(set(actual) == set(prefixes) | {"hash_rows"}, "complete actual observation fields required")
    for field, accepted in prefixes.items():
        require(actual[field] == [row for row in rows if row.startswith(accepted)],
                "actual observation/native transcript differs")
    require(sum(len(actual[field]) for field in prefixes) == len(rows), "untyped native rows differ")
    require([int(row.split()[1]) for row in actual["frame_rows"]] == list(range(15)) and
            [int(row.split()[1]) for row in actual["native_state_rows"]] == list(range(15)),
            "all fifteen native profiles required")
    require([int(row.split()[1]) for row in actual["failure_rows"]] == [4, 5, 7, 8, 13],
            "all retained failed source prefixes required")
    for index, (frame, state) in enumerate(zip(actual["frame_rows"], actual["native_state_rows"])):
        frame, state = frame.split(), state.split()
        pc = {4: 443, 5: 172, 7: 944, 8: 982, 13: 470}.get(index, 994)
        require(len(frame) == 16 and frame[2] == SCENARIOS[index] and len(state) == 18 and
                int(state[2]) == pc, "native frame/source boundary differs")
        if pc == 994:
            require(int(state[7]) == (28880 if index else 0), "complete native sample count differs")
    expected_hash_rows = ["RANDOM_HASH " + str(index) + " " + " ".join(
        observations[f"case{index}-{stream}.bin"]["sha256"] for stream in STREAMS)
        for index in range(15)]
    require(actual["hash_rows"] == expected_hash_rows, "complete actual/binary observation binding differs")
    actual_bytes = (json.dumps(actual, indent=2) + "\n").replace("\n", "\r\n").encode("ascii")
    require(sha256(actual_bytes) == outputs["actual.json"], "captured Windows actual.json binding differs")
    require(canonical_digest({k: v for k, v in control.items() if k != "projection_sha256"}) ==
            control["projection_sha256"], "control projection integrity differs")
    return control_signature(control)


def audit_capture(group):
    capture = group["capture"]
    require(capture["base_commit"] == "31c803e2fffe8ef1f7a5b4565d0558b3f1d0fff4",
            "frozen C baseline binding differs")
    libraries = capture["shared_libraries_sha256"]
    hash_inventory(libraries)
    require(len(libraries) == 31 and len(capture["shared_library_tail"]) == 45 and
            all(name in capture["shared_library_tail"] for name in libraries),
            "complete shared library closure required")
    hash_inventory(capture["executables_sha256"])
    require(set(capture["executables_sha256"]) == set(VARIANTS), "two frozen executable identities required")
    hash_inventory(capture["objects_sha256"])
    require(set(capture["objects_sha256"]) == {
        f"mc_{owner}-{variant}.obj" for owner in ("registry", "simulation") for variant in VARIANTS},
        "exactly two corresponding runtime overrides per variant required")
    inputs = capture["candidate_project_inputs_sha256"]
    hash_inventory(inputs)
    require(len(inputs) == 182, "all frozen project compilation inputs required")
    external = capture["external_compilation_inputs_sha256"]
    hash_inventory(external)
    require(len(external) == 24, "all captured compiler/SDK headers required")
    require(canonical_digest(capture) == group["capture_projection_sha256"] == CAPTURE_DIGESTS[group["name"]],
            "frozen capture closure integrity differs")

    require([cohort["name"] for cohort in group["cohorts"]] == ["initial", "confirmation"],
            "initial and separate confirmation must remain distinct")
    results, signature, fixture_inputs = {}, None, None
    for cohort, rounds in zip(group["cohorts"], (3, 6)):
        require(cohort["status"] == "PASS_FIXED_WORK_CPU_ONLY" and cohort["rounds"] == rounds,
                "complete fixed-work capture required")
        require(cohort["executables_sha256"] == capture["executables_sha256"],
                "cohort executable binding differs")
        computed = benchmark.audit_cohort(cohort)
        same_statistics(computed, cohort["captured_statistics"])
        projection = {k: v for k, v in cohort.items()
                      if k not in ("controls", "captured_statistics", "projection_sha256")}
        require(canonical_digest(projection) == cohort["projection_sha256"] ==
                COHORT_DIGESTS[group["name"], cohort["name"]], "captured cohort projection integrity differs")
        require(set(cohort["controls"]) == set(VARIANTS), "both variant controls required")
        for variant in VARIANTS:
            control = cohort["controls"][variant]
            observed = audit_control(control, capture["executables_sha256"][variant])
            common_inputs = {k: v for k, v in control["input_before_sha256"].items() if k != "executable"}
            if signature is None:
                signature, fixture_inputs = observed, common_inputs
            require(signature == observed, "full paired control observations or output hashes differ")
            require(fixture_inputs == common_inputs, "paired control fixture inputs differ")
            require(control["projection_sha256"] == CONTROL_DIGESTS[group["name"], cohort["name"], variant],
                    "captured control projection integrity differs")
        results[cohort["name"]] = computed
    return results


def audit_receipt(receipt):
    require(receipt["schema"] == 1 and receipt["kind"] == "registry_random_tick_windows",
            "known Registry CPU receipt schema required")
    require([(group["name"], group["qualification"]) for group in receipt["captures"]] == [
        ("warned_preliminary", "RETAINED_PRELIMINARY_NO_FINAL_CREDIT"),
        ("clean_primary", "PRIMARY_FIXED_WORK_CPU_ONLY")], "complete ordered capture history required")
    return {group["name"]: audit_capture(group) for group in receipt["captures"]}


class RegistryTickReceiptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.receipt = json.loads(RECEIPT.read_text(encoding="utf-8"))
        # Negative cases repeatedly audit the same complete rows. Cache only
        # exact row sequences already checked by the existing scalar parser;
        # a changed row always reaches that parser again.
        parse_rows = benchmark.parse_rows

        @lru_cache(maxsize=128)
        def parsed(lines, workload):
            return parse_rows(lines, workload)

        patch = mock.patch.object(benchmark, "parse_rows",
                                  side_effect=lambda lines, workload: parsed(tuple(lines), workload))
        patch.start()
        cls.addClassCleanup(patch.stop)

    def changed(self):
        return copy.deepcopy(self.receipt)

    def rejected(self, receipt, message):
        with self.assertRaisesRegex(AssertionError, message):
            audit_receipt(receipt)

    def test_complete_captures_and_statistics(self):
        result = audit_receipt(self.receipt)
        confirmation = result["clean_primary"]["confirmation"]
        for workload in ("air_qhigh", "stone_q0"):
            self.assertEqual(confirmation[workload]["baseline"]["count"], 84)
            self.assertEqual(confirmation[workload]["candidate"]["count"], 84)

    def test_missing_preliminary_history(self):
        receipt = self.changed()
        receipt["captures"].pop(0)
        self.rejected(receipt, "capture history")

    def test_dropped_process_or_sample(self):
        for target in ("process", "sample"):
            with self.subTest(target=target):
                receipt = self.changed()
                cohort = receipt["captures"][-1]["cohorts"][-1]
                if target == "process":
                    cohort["processes"].pop()
                else:
                    cohort["processes"][0]["rows"].pop()
                self.rejected(receipt, "fresh process/order|complete samples")

    def test_changed_order_or_failed_process(self):
        for target in ("order", "exit", "timeout"):
            with self.subTest(target=target):
                receipt = self.changed()
                process = receipt["captures"][-1]["cohorts"][-1]["processes"][0]
                process[{"order": "position_in_pair", "exit": "exit", "timeout": "timeout"}[target]] = 1
                self.rejected(receipt, "fresh process/order|native failure")

    def test_changed_rng_work_or_nonfinite_time(self):
        for column, replacement in ((6, "00000000"), (11, "28879"), (3, "nan"), (4, "inf")):
            with self.subTest(column=column):
                receipt = self.changed()
                rows = receipt["captures"][-1]["cohorts"][-1]["processes"][0]["rows"]
                cells = rows[2].split()
                cells[column] = replacement
                rows[2] = " ".join(cells)
                self.rejected(receipt, "RNG/LCG|work required|finite timing")

    def test_changed_statistics_or_retained_timing(self):
        receipt = self.changed()
        cohort = receipt["captures"][-1]["cohorts"][-1]
        cohort["captured_statistics"]["air_qhigh"]["candidate"]["median"] += 1
        self.rejected(receipt, "captured statistics")
        receipt = self.changed()
        cohort = receipt["captures"][-1]["cohorts"][-1]
        rows = cohort["processes"][0]["rows"]
        cells = rows[2].split()
        cells[3] = str(float(cells[3]) + 1)
        rows[2] = " ".join(cells)
        self.rejected(receipt, "captured statistics")

    def test_changed_library_or_frozen_source(self):
        for field in ("shared_libraries_sha256", "candidate_project_inputs_sha256"):
            with self.subTest(field=field):
                receipt = self.changed()
                values = receipt["captures"][-1]["capture"][field]
                values[next(iter(values))] = "0" * 64
                self.rejected(receipt, "capture closure integrity")

    def test_changed_executable_binding(self):
        receipt = self.changed()
        receipt["captures"][-1]["cohorts"][0]["executables_sha256"]["candidate"] = "0" * 64
        self.rejected(receipt, "cohort executable binding")

    def test_changed_control_input(self):
        receipt = self.changed()
        control = receipt["captures"][-1]["cohorts"][0]["controls"]["candidate"]
        control["input_after_sha256"]["executable"] = "0" * 64
        self.rejected(receipt, "control inputs")

    def test_missing_binary_observation_or_output(self):
        for field in ("observations", "outputs_sha256"):
            with self.subTest(field=field):
                receipt = self.changed()
                receipt["captures"][-1]["cohorts"][0]["controls"]["candidate"][field].pop("case14-P.bin")
                self.rejected(receipt, "binary observations|output hash inventory")

    def test_changed_callback_or_failure(self):
        for prefix in ("RANDOM_SOUND ", "RANDOM_FAILURE "):
            with self.subTest(prefix=prefix):
                receipt = self.changed()
                control = receipt["captures"][-1]["cohorts"][0]["controls"]["candidate"]
                index = next(i for i, row in enumerate(control["native_rows"]) if row.startswith(prefix))
                control["native_rows"][index] += " changed"
                self.rejected(receipt, "native transcript binding")

    def test_changed_observation_despite_same_check_count(self):
        receipt = self.changed()
        control = receipt["captures"][-1]["cohorts"][0]["controls"]["candidate"]
        control["observations"]["case1-raw.bin"]["bytes"] -= 1
        control["projection_sha256"] = canonical_digest({k: v for k, v in control.items()
                                                          if k != "projection_sha256"})
        self.rejected(receipt, "full paired control observations")

    def test_coordinated_raw_row_and_statistics_edit(self):
        receipt = self.changed()
        cohort = receipt["captures"][-1]["cohorts"][-1]
        cells = cohort["processes"][0]["rows"][2].split()
        cells[3] = str(float(cells[3]) + 1)
        cohort["processes"][0]["rows"][2] = " ".join(cells)
        cohort["captured_statistics"] = benchmark.audit_cohort(cohort)
        cohort["projection_sha256"] = canonical_digest({k: v for k, v in cohort.items()
            if k not in ("controls", "captured_statistics", "projection_sha256")})
        self.rejected(receipt, "cohort projection integrity")

    def test_coordinated_control_metadata_edit(self):
        receipt = self.changed()
        control = receipt["captures"][-1]["cohorts"][0]["controls"]["candidate"]
        control["capture_run_sha256"] = "0" * 64
        control["projection_sha256"] = canonical_digest({k: v for k, v in control.items()
                                                          if k != "projection_sha256"})
        self.rejected(receipt, "captured control projection integrity")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--receipt":
        print(json.dumps(audit_receipt(json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))), indent=2))
    else:
        unittest.main()
