"""Java-free guards for live region comparison; synthetic records are not parity data."""

import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zlib


ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / "scripts/reference/region/compare_region.py"
SPEC = importlib.util.spec_from_file_location("region_live_subject", DRIVER)
REGION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REGION)
COMMON, NBT, FIXTURES, EXECUTED = REGION.helpers()
C_KEYS = ("open_status", "view", "locations", "timestamps", "entry", "record_status",
          "payload_hex", "compression_status", "compressed_consumed", "compressed_trailing_bytes",
          "raw_hex", "nbt_status", "nbt_consumed", "raw_trailing_bytes", "canonical_hex")
HELPER_CLASSES = ("RegionObservation", "RegionObservation$BoundedInput", "RegionObservation$BoundedOutput",
                  "RegionObservation$Model", "RegionObservation$ObservationFailure",
                  "RegionObservation$OriginalException", "RegionObservation$Request")


def fingerprint(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def region_file(raw=b"\x0a\x00\x00\x00", *, selector=2, compressed_tail=b""):
    payload = (FIXTURES._gzip(raw) if selector == 1 else zlib.compress(raw)) + compressed_tail
    data = bytearray(12288)
    struct.pack_into(">I", data, 0, 0x201)
    struct.pack_into(">I", data, 4096, 0xFFFFFFFF)
    struct.pack_into(">I", data, 8192, len(payload) + 1)
    data[8196] = selector
    data[8197:8197 + len(payload)] = payload
    return bytes(data)


def synthetic_c(plan):
    """Construct protocol data only; geometry assertions below use explicit constants."""
    data = plan["bytes"]
    expected = REGION.format_expectation(data, *plan["coordinates"])
    result = dict.fromkeys(C_KEYS)
    result["open_status"] = expected["open"]
    if expected["open"] != "ok":
        return result
    result.update({key: expected[key] for key in ("view", "locations", "timestamps", "entry")})
    entry = expected["entry"]
    if entry is None:
        result["record_status"] = "invalid local coordinate"
    elif not entry["location"]:
        result["record_status"] = "missing region entry"
    elif entry["issues"]:
        result["record_status"] = "unsupported compression selector" if entry["issues"] == 64 else "invalid region record"
    else:
        result["record_status"] = "ok"
        begin, length = entry["byte_offset"], entry["record_length"]
        payload = data[begin + 5:begin + 4 + length]
        result["payload_hex"] = payload.hex()
        try:
            decoder = zlib.decompressobj(31 if entry["selector"] == 1 else 15)
            raw = decoder.decompress(payload)
            if not decoder.eof:
                raise zlib.error("synthetic incomplete container")
        except zlib.error:
            result["compression_status"] = "invalid compressed stream"
        else:
            result.update(compression_status="ok", compressed_consumed=len(payload) - len(decoder.unused_data),
                          compressed_trailing_bytes=len(decoder.unused_data), raw_hex=raw.hex(),
                          nbt_status="ok", nbt_consumed=len(raw), raw_trailing_bytes=0, canonical_hex=raw.hex())
    return result


def synthetic_original_row(plan, c_value=None):
    data = plan["bytes"]
    result = {"index": plan["index"], "x": plan["coordinates"][0], "z": plan["coordinates"][1],
              "input_before": fingerprint(data), "input_after": fingerprint(data),
              "constructor": {"status": "ok"}, "locations": data[:4096].hex(),
              "timestamps": data[4096:8192].hex(), "growth": 0, "free_sectors": "00",
              "present": {"status": "ok", "value": False}, "root": {"status": "null"},
              "drain": {"status": "null"}, "close": {"status": "ok"}}
    if c_value is not None and c_value["record_status"] == "ok":
        result["present"]["value"] = True
        if c_value["compression_status"] == "ok":
            result["root"] = {"status": "root", "canonical_hex": c_value["canonical_hex"]}
            result["drain"] = {"status": "drain", "hex": c_value["raw_hex"]}
        else:
            result["root"] = result["drain"] = {"status": "exception", "class": "java.util.zip.ZipException"}
    return result


class RegionLiveGuards(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="mc-region-live-guards-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()

    def plan(self, data, x=0, z=0):
        return {"index": 0, "label": "synthetic", "coordinates": (x, z), "bytes": data}

    def original_observation(self):
        jar = self.root / "synthetic selected.jar"
        jar.write_bytes(b"synthetic selected archive")
        corpus, copies, records = [], [], []
        for index in range(49):
            plan = self.plan(bytes(8192))
            plan["index"] = index
            path = self.root / (str(index) + ".fixture")
            path.write_bytes(plan["bytes"])
            corpus.append(plan)
            copies.append(path)
            records.append(synthetic_original_row(plan))
        origin = jar.as_uri().replace("file:///", "file:/", 1)
        value = {"schema": 1, "side": "client", "platform_parent": True,
                 "jar": {"bytes": jar.stat().st_size, "sha256": REGION.JAR_HASHES["client"]},
                 "class_origins": {role: {"name": name, "origin": origin, "loader": "isolated"}
                                   for role, name in zip(("region", "utility", "compound"), ("qj", "as", "nu"))},
                 "records": records}
        return value, corpus, copies, jar

    def validate_original(self, value, corpus, copies, jar):
        # This is simulated protocol provenance. No real archive or original class runs.
        original_record = COMMON.file_record

        def record(path, byte_limit=None):
            result = original_record(path, byte_limit=byte_limit)
            if Path(path) == jar:
                result["sha256"] = REGION.JAR_HASHES["client"]
            return result

        with patch.object(COMMON, "file_record", side_effect=record):
            REGION.validate_original(value, "client", corpus, copies, COMMON, jar)

    def test_fixture_import_is_inert_and_gzip_header_is_portable(self):
        path = ROOT / "scripts/reference/region/fixtures.py"
        source = path.read_bytes()
        module = types.ModuleType("region_inert_fixture_check")
        with patch.object(Path, "read_bytes", side_effect=AssertionError("import read")), \
                patch.object(Path, "write_bytes", side_effect=AssertionError("import write")), \
                patch.object(subprocess, "Popen", side_effect=AssertionError("import process")):
            exec(compile(source, str(path), "exec"), module.__dict__)
        first, second = module.cases(), module.cases()
        self.assertEqual(first, second)
        self.assertIsNot(first, second)
        self.assertIsNot(first[0]["queries"], second[0]["queries"])
        encoded = module._gzip(b"portable synthetic data")
        self.assertEqual(encoded[:10], bytes.fromhex("1f8b08000000000002ff"))
        self.assertEqual(struct.unpack("<II", encoded[-8:]), (zlib.crc32(b"portable synthetic data"), 23))
        self.assertEqual(zlib.decompress(encoded, 31), b"portable synthetic data")

    def test_complete_case_inventory_and_ordered_query_coverage(self):
        cases = FIXTURES.cases()
        expected_labels = ["empty-header", "coordinate-and-timestamp-mapping", "same-sector-overlap", "range-overlap",
                           "past-end", "zero-count", "header-sector-zero", "negative-offset-zero-count",
                           "negative-offset-one-count", "high-bit-offset", "count-255-past-end", "header-sector-one-readable"]
        expected_labels += ["zlib-declared-length-" + str(n) for n in (-1, 0, 1, 4092, 4093, 4095, 4096, 4097)]
        expected_labels += ["empty-payload-selector-" + str(n) for n in (0, 1, 2, 3, 255)]
        expected_labels += ["zlib-under-gzip-selector", "gzip-under-zlib-selector", "gzip-bad-unread-crc"]
        expected_labels += ["short-file-" + str(n) for n in (0, 1, 4095, 4096, 4097, 8191)]
        expected_labels += ["unaligned-header-overwrite-17", "unaligned-header-overwrite-1"]
        self.assertEqual([case["label"] for case in cases], expected_labels)
        self.assertEqual(cases[0]["queries"], [(0, 0), (31, 31), (-1, 0), (32, 0)])
        self.assertEqual(cases[1]["queries"], [(0, 0), (31, 0), (0, 1), (1, 0), (32, 0), (-1, 1), (-1, 0), (0, -1), (31, 31)])
        self.assertEqual(cases[2]["queries"], [(0, 0), (1, 0)])
        self.assertEqual(cases[3]["queries"], [(0, 0), (1, 0)])
        self.assertTrue(all(case["queries"] == [(0, 0)] for case in cases[4:]))
        plans = REGION.plans(cases)
        self.assertEqual(len(plans), 49)
        self.assertEqual([plan["index"] for plan in plans], list(range(49)))

    def test_corpus_omissions_duplicates_mutable_data_and_bad_coordinates_fail(self):
        for mutation in ("case", "label", "query", "duplicate_query", "boolean", "coordinate_range", "mutable", "size"):
            cases = FIXTURES.cases()
            if mutation == "case":
                cases.pop()
            elif mutation == "label":
                cases[-1]["label"] = cases[0]["label"]
            elif mutation == "query":
                cases[0]["queries"].pop()
            elif mutation == "duplicate_query":
                cases[1]["queries"][1] = cases[1]["queries"][0]
            elif mutation == "boolean":
                cases[0]["queries"][0] = (True, 0)
            elif mutation == "coordinate_range":
                cases[0]["queries"][0] = (1 << 31, 0)
            elif mutation == "mutable":
                cases[0]["bytes"] = bytearray(8192)
            else:
                cases[0]["bytes"] = bytes(65536)
            with self.subTest(mutation=mutation), self.assertRaises(REGION.Failure):
                REGION.plans(cases)

    def test_geometry_preserves_full_headers_unsigned_bits_and_local_addressing(self):
        data = bytearray(region_file())
        struct.pack_into(">I", data, 31 * 4, 0xFFFFFF01)
        struct.pack_into(">I", data, 4096 + 31 * 4, 0x80000000)
        value = REGION.format_expectation(bytes(data), 31, 0)
        self.assertEqual(value["locations"], bytes(data[:4096]).hex())
        self.assertEqual(value["timestamps"], bytes(data[4096:8192]).hex())
        self.assertEqual(value["view"], {"length": 12288, "trailing_bytes": 0, "present_entries": 2, "invalid_entries": 1})
        self.assertEqual(value["entry"], {"location": 0xFFFFFF01, "timestamp": 0x80000000, "issues": 4,
                                          "record_length": 0, "byte_offset": 68719472640, "extent_bytes": 4096, "selector": 0})
        self.assertEqual(REGION.format_expectation(bytes(data), 0, 1)["entry"]["location"], 0)
        self.assertIsNone(REGION.format_expectation(bytes(data), -1, 1)["entry"])
        self.assertIsNone(REGION.format_expectation(bytes(data), 32, 0)["entry"])

    def test_geometry_diagnoses_both_overlap_owners_and_signed_lengths(self):
        data = bytearray(12288)
        struct.pack_into(">II", data, 0, 0x202, 0x301)
        for x in (0, 1):
            self.assertEqual(REGION.format_expectation(bytes(data), x, 0)["entry"]["issues"], 4 | 8)
        for length, selector, issue in ((0, 2, 16), (0xFFFFFFFF, 2, 16), (4093, 2, 32), (1, 3, 64)):
            changed = bytearray(region_file())
            struct.pack_into(">I", changed, 8192, length)
            changed[8196] = selector
            with self.subTest(length=length, selector=selector):
                value = REGION.format_expectation(bytes(changed), 0, 0)
                self.assertEqual(value["entry"]["issues"], issue)
                self.assertEqual(value["entry"]["record_length"], length)
        self.assertEqual(REGION.format_expectation(bytes(8191), 0, 0)["open"], "truncated region header")
        self.assertEqual(REGION.format_expectation(bytes(8193), 0, 0)["view"]["trailing_bytes"], 1)

    def test_complete_profiles_count_compatibility_and_diagnostics_separately(self):
        profiles = []
        for plan in REGION.plans(FIXTURES.cases()):
            c_value = synthetic_c(plan)
            original = synthetic_original_row(plan, c_value)
            profiles.append(REGION.compare_c(c_value, original, plan, NBT)["profile"])
        self.assertEqual(profiles.count("compatible"), 8)
        self.assertEqual(profiles.count("diagnostic-departure"), 41)

    def test_valid_root_compares_all_bytes_consumption_and_only_compound_order(self):
        raw = bytes.fromhex("0a00000100016202010001610100")
        plan = self.plan(region_file(raw, compressed_tail=b"compressed-tail"))
        c_value = synthetic_c(plan)
        original = synthetic_original_row(plan, c_value)
        original["root"]["canonical_hex"] = "0a00000100016101010001620200"
        comparison = REGION.compare_c(c_value, original, plan, NBT)
        self.assertTrue(comparison["compound_order_only"])
        for mutation in ("raw", "canonical", "consumed", "nbt_consumed", "header"):
            changed = copy.deepcopy(c_value)
            if mutation == "raw":
                changed["raw_hex"] = changed["raw_hex"][:-2] + "01"
            elif mutation == "canonical":
                changed["canonical_hex"] = changed["canonical_hex"].replace("6202", "6203")
            elif mutation == "consumed":
                changed["compressed_consumed"] -= 1
                changed["compressed_trailing_bytes"] += 1
            elif mutation == "nbt_consumed":
                changed["nbt_consumed"] = 0
                changed["raw_trailing_bytes"] = len(raw)
            else:
                changed["locations"] = "0" + changed["locations"][1:4096] + "f" + changed["locations"][4097:]
            with self.subTest(mutation=mutation), self.assertRaises(REGION.Failure):
                REGION.compare_c(changed, original, plan, NBT)

    def test_diagnostic_records_require_complete_protocol_and_unattempted_nulls(self):
        for plan in (self.plan(bytes(1)), self.plan(bytes(8192), -1, 0), self.plan(region_file(selector=3))):
            actual = synthetic_c(plan)
            original = synthetic_original_row(plan, actual)
            REGION.compare_c(actual, original, plan, NBT)
            for mutation in ("missing", "invented_raw", "invented_nbt"):
                changed = copy.deepcopy(actual)
                if mutation == "missing":
                    del changed["canonical_hex"]
                elif mutation == "invented_raw":
                    changed["raw_hex"] = "00"
                else:
                    changed["nbt_status"] = "ok"
                    changed["canonical_hex"] = "0a000000"
                with self.subTest(coordinates=plan["coordinates"], mutation=mutation), self.assertRaises(REGION.Failure):
                    REGION.compare_c(changed, original, plan, NBT)

    def test_original_provenance_shape_and_every_copy_are_required(self):
        value, corpus, copies, jar = self.original_observation()
        self.validate_original(value, corpus, copies, jar)
        for mutation in ("schema", "side", "jar_hash", "jar_bytes", "origin", "loader", "class",
                         "records", "copies", "ordinal", "coordinate", "before", "after", "metadata", "extra"):
            changed = copy.deepcopy(value)
            selected = copies
            if mutation == "schema":
                changed["schema"] = True
            elif mutation == "side":
                changed["side"] = "server"
            elif mutation == "jar_hash":
                changed["jar"]["sha256"] = "0" * 64
            elif mutation == "jar_bytes":
                changed["jar"]["bytes"] += 1
            elif mutation == "origin":
                for item in changed["class_origins"].values():
                    item["origin"] = (self.root / "different.jar").as_uri().replace("file:///", "file:/", 1)
            elif mutation == "loader":
                changed["class_origins"]["region"]["loader"] = "application"
            elif mutation == "class":
                changed["class_origins"]["region"]["name"] = "jx"
            elif mutation == "records":
                changed["records"].pop()
            elif mutation == "copies":
                selected = copies[:-1]
            elif mutation == "ordinal":
                changed["records"][0]["index"] = False
            elif mutation == "coordinate":
                changed["records"][0]["x"] = False
            elif mutation == "before":
                changed["records"][0]["input_before"]["sha256"] = "f" * 64
            elif mutation == "after":
                changed["records"][0]["input_after"]["bytes"] += 1
            elif mutation == "metadata":
                changed["records"][0]["timestamps"] = "f" + changed["records"][0]["timestamps"][1:]
            else:
                changed["records"][0]["invented"] = "ignored data"
            with self.subTest(mutation=mutation), self.assertRaises(REGION.Failure):
                self.validate_original(changed, corpus, selected, jar)

    def test_original_outcomes_require_exact_typed_bounded_payloads(self):
        value, corpus, copies, jar = self.original_observation()
        for mutation in ("present_value", "present_exception", "root_hex", "root_bound", "drain_hex",
                         "null_payload", "close_exception", "constructor_class", "constructor_state"):
            changed = copy.deepcopy(value)
            row = changed["records"][0]
            if mutation == "present_value":
                row["present"]["value"] = 0
            elif mutation == "present_exception":
                row["present"] = {"status": "exception"}
            elif mutation == "root_hex":
                row["root"] = {"status": "root", "canonical_hex": "zz"}
            elif mutation == "root_bound":
                row["root"] = {"status": "root", "canonical_hex": "00" * (1024 * 1024 + 1)}
            elif mutation == "drain_hex":
                row["drain"] = {"status": "drain", "hex": "0"}
            elif mutation == "null_payload":
                row["root"] = {"status": "null", "canonical_hex": "0a000000"}
            elif mutation == "close_exception":
                row["close"] = {"status": "exception", "class": None}
            else:
                row["constructor"] = {"status": "exception", "class": "java.lang.IndexOutOfBoundsException"}
                for name in ("locations", "timestamps", "growth", "free_sectors", "present", "root", "drain"):
                    row[name] = None
                row["close"] = {"status": "not_created"}
                if mutation == "constructor_class":
                    del row["constructor"]["class"]
                else:
                    row["growth"] = 0
            with self.subTest(mutation=mutation), self.assertRaises(REGION.Failure):
                self.validate_original(changed, corpus, copies, jar)

        accepted = copy.deepcopy(value)
        row = accepted["records"][0]
        row["constructor"] = {"status": "exception", "class": "java.lang.IndexOutOfBoundsException"}
        for name in ("locations", "timestamps", "growth", "free_sectors", "present", "root", "drain"):
            row[name] = None
        row["close"] = {"status": "not_created"}
        self.validate_original(accepted, corpus, copies, jar)

    def test_loaded_helpers_are_fingerprinted_and_restore_fixture_import(self):
        marker = types.ModuleType("synthetic_fixture_sentinel")
        with patch.dict(sys.modules, {"fixtures": marker}):
            _, _, _, executed = REGION.helpers()
            self.assertIs(sys.modules["fixtures"], marker)
        paths = {Path(item["path"]).resolve() for item in executed}
        self.assertIn(ROOT / "scripts/reference/region/fixtures.py", paths)
        self.assertIn(ROOT / "scripts/reference/nbt/compare_nbt.py", paths)
        for item in executed:
            data = Path(item["path"]).read_bytes()
            self.assertEqual((item["bytes"], item["sha256"]), (len(data), hashlib.sha256(data).hexdigest()))

    def test_renamed_executing_driver_is_rejected(self):
        with patch.object(REGION, "__file__", str(DRIVER.with_name("renamed_compare_region.py"))), \
                self.assertRaises(REGION.Failure):
            REGION.helpers()

    def main_setup(self):
        paths = {}
        for name in ("client", "server", "executable", "java", "javac", "javap", "jdk_modules", "jdk_ct_sym"):
            path = self.root / name
            path.write_bytes(b"synthetic-input")
            paths[name] = path
        release = self.root / "release"
        release.write_text('JAVA_VERSION="17.0.17"\n', encoding="utf-8")
        paths["jdk_release"] = release
        tools = {key: paths[key] for key in ("java", "javac", "javap", "jdk_release", "jdk_modules", "jdk_ct_sym")}
        arguments = ["--client", str(paths["client"]), "--server", str(paths["server"]),
                     "--executable", str(paths["executable"]), "--work-dir", str(self.root / "reports"),
                     "--tested-commit", "a" * 40]
        return paths, tools, arguments

    def test_environment_and_source_label_errors_precede_unavailable_runtime(self):
        _, _, arguments = self.main_setup()
        for name in (*REGION.JAVA_ENVIRONMENT, "invalid_commit"):
            selected = arguments.copy()
            environment = {key: "" for key in REGION.JAVA_ENVIRONMENT}
            if name == "invalid_commit":
                selected[-1] = "A" * 40
            else:
                environment[name] = "injected"
            with self.subTest(name=name), patch.dict(os.environ, environment), \
                    patch.object(REGION, "helpers", return_value=(COMMON, NBT, FIXTURES, EXECUTED)), \
                    patch.object(COMMON, "runtime_tools", side_effect=COMMON.Unavailable("missing synthetic runtime")) as runtime, \
                    patch.object(COMMON, "run_live", side_effect=AssertionError("no process permitted")), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(REGION.main(selected), 1)
                runtime.assert_not_called()
        self.assertFalse((self.root / "reports").exists())

    def test_missing_runtime_is_unavailable_before_any_process_or_output(self):
        _, _, arguments = self.main_setup()
        with patch.dict(os.environ, {key: "" for key in REGION.JAVA_ENVIRONMENT}), \
                patch.object(REGION, "helpers", return_value=(COMMON, NBT, FIXTURES, EXECUTED)), \
                patch.object(COMMON, "runtime_tools", side_effect=COMMON.Unavailable("missing synthetic runtime")), \
                patch.object(COMMON, "run_live", side_effect=AssertionError("no process permitted")), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(REGION.main(arguments), 77)
        self.assertFalse((self.root / "reports").exists())

    def fake_run(self, stage, code, *, stop_reason=None, mutate=None, executed=None):
        paths, tools, arguments = self.main_setup()
        original_record = COMMON.file_record
        calls = []
        active_classes = None
        previous = set((self.root / "reports").glob("capture-*/manifest.json"))

        def record(path, byte_limit=None):
            value = original_record(path, byte_limit=byte_limit)
            for side in ("client", "server"):
                if Path(path) == paths[side]:
                    value["sha256"] = REGION.JAR_HASHES[side]
            return value

        def run(command, stdout, stderr, cwd, **kwargs):
            nonlocal active_classes
            current = "compile" if Path(command[0]) == paths["javac"] else "c"
            calls.append(current)
            Path(stdout).write_bytes(b"")
            Path(stderr).write_bytes(b"")
            if current == "compile":
                classes = Path(command[command.index("-d") + 1])
                active_classes = classes
                for name in HELPER_CLASSES:
                    (classes / (name + ".class")).write_bytes(bytes.fromhex("cafebabe0000003d") + bytes(8))
            if current == "c" and mutate == "input":
                paths["executable"].write_bytes(b"SYNTHETIC-INPUT")
            if current == "c" and mutate == "class":
                (active_classes / "RegionObservation.class").write_bytes(b"different-class")
            selected = current == stage
            return COMMON.ProcessResult(list(map(str, command)), code if selected else 0, 0.0,
                                        False, stop_reason if selected else None, 0, 0, 0, 0, 0)

        with patch.dict(os.environ, {key: "" for key in REGION.JAVA_ENVIRONMENT}), \
                patch.object(REGION, "helpers", return_value=(COMMON, NBT, FIXTURES, EXECUTED if executed is None else executed)), \
                patch.object(COMMON, "runtime_tools", return_value=tools), \
                patch.object(COMMON, "tool_versions"), patch.object(COMMON, "file_record", side_effect=record), \
                patch.object(COMMON, "run_live", side_effect=run), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            status = REGION.main(arguments)
        manifests = set((self.root / "reports").glob("capture-*/manifest.json")) - previous
        self.assertLessEqual(len(manifests), 1)
        report = json.loads(next(iter(manifests)).read_text(encoding="utf-8")) if manifests else None
        return status, report, calls

    def test_compile_and_measured_c_failures_are_never_unavailable(self):
        for stage, code, reason in (("compile", 77, None), ("c", 77, None), ("c", 1, None), ("c", 0, "output limit")):
            with self.subTest(stage=stage, code=code, stop=reason):
                status, report, calls = self.fake_run(stage, code, stop_reason=reason)
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "fail")
                self.assertTrue(any("failed" in message for message in report["errors"]))
                self.assertEqual(calls, ["compile"] if stage == "compile" else ["compile", "c"])

    def test_input_and_class_preservation_errors_remain_failures(self):
        for mutation, expected in (("input", "input changed"), ("class", "compiled helpers changed")):
            with self.subTest(mutation=mutation):
                status, report, _ = self.fake_run("c", 77, mutate=mutation)
                self.assertEqual(status, 1)
                self.assertEqual(report["status"], "fail")
                self.assertTrue(any(expected in message for message in report["errors"]), report["errors"])

    def test_loaded_source_fingerprint_mismatch_fails_before_processes(self):
        executed = copy.deepcopy(EXECUTED)
        executed[0]["sha256"] = "0" * 64
        status, report, calls = self.fake_run("compile", 77, executed=executed)
        self.assertEqual(status, 1)
        self.assertIsNone(report)
        self.assertEqual(calls, [])


if __name__ == "__main__":
    unittest.main()
