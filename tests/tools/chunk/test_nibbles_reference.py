"""Asset-independent guards for the packed-nibble capture and comparison protocol."""

import argparse
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / "scripts/reference/chunk/compare_nibbles.py"
spec = importlib.util.spec_from_file_location("nibbles_driver_under_test", DRIVER)
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)
common, protocol, executed = driver.load_modules(ROOT)


def synthetic_record(side):
    """Valid record shape with arbitrary observation bytes, never a parity fixture."""
    prepared = bytes((i * 73 + (i >> 3) * 19 + (i >> 8) * 151 + 0xa7) & 255
                     for i in range(protocol.PACKED_BYTES)).hex()
    value = {
        "schema": 1, "side": side, "constructor_cells": protocol.CELL_COUNT,
        "packed_length": protocol.PACKED_BYTES, "fresh_packed": "00" * protocol.PACKED_BYTES,
        "prepared_packed": prepared, "getter_order": "X/Z/Y",
        "initial_getters": "00" * protocol.CELL_COUNT,
        "writes": [dict(zip(("x", "y", "z", "value"), request), observed=0, packed=prepared)
                   for request in protocol.WRITES],
        "final_packed": prepared, "final_getters": "00" * protocol.CELL_COUNT,
        "controls": {name: True for name in protocol.COMMON_CONTROLS},
    }
    if side in protocol.JARS:
        digest, owner = protocol.JARS[side]
        value.update(jar_sha256_before=digest, jar_sha256_after=digest, bindings={
            "owner": owner, "array": "a:[B", "constructor": owner + "(I)V",
            "getter": "a(III)I", "setter": "a(IIII)V",
        })
        value["controls"] = {name: True for name in protocol.ORIGINAL_CONTROLS}
    return value


def encoded(value):
    return (json.dumps(value, separators=(",", ":"), allow_nan=False) + "\n").encode()


def completed_markers(side):
    owner = protocol.JARS[side][1]
    return (f"NIBBLES_SETUP_BEGIN side={side} class={owner}\n"
            f"NIBBLES_SETUP_DONE side={side} class={owner}\n"
            f"NIBBLES_MEASURE_BEGIN side={side}\n"
            "NIBBLES_CONTROL fresh_zero array_identity initial_getters_readonly "
            "written_getters_readonly final_getters_readonly\n"
            f"NIBBLES_MEASURE_DONE side={side}\n").encode()


class ProtocolTests(unittest.TestCase):
    def test_original_and_c_full_records_compare(self):
        client, server, c = (synthetic_record(side) for side in ("client", "server", "c"))
        for value in (client, server, c):
            self.assertEqual(protocol.parse_record(encoded(value), value["side"]), value)
        for left, right in ((client, server), (c, client), (c, server)):
            result = protocol.compare_records(left, right)
            self.assertEqual(result["written_packed_bytes_exact"], 262144)
            self.assertFalse(result["access_expected_results_computed"])

    def test_complete_fields_detect_differences(self):
        client = synthetic_record("client")
        for field in ("initial_getters", "final_getters"):
            server = synthetic_record("server")
            server[field] = server[field][:-2] + "0f"
            with self.subTest(field=field), self.assertRaisesRegex(protocol.ObservationError, field):
                protocol.compare_records(client, server)
        server = synthetic_record("server")
        server["writes"][7]["packed"] = "01" + server["writes"][7]["packed"][2:]
        with self.assertRaisesRegex(protocol.ObservationError, r"writes\[7\].packed"):
            protocol.compare_records(client, server)
        server = synthetic_record("server")
        server["writes"][6]["observed"] = 15
        with self.assertRaisesRegex(protocol.ObservationError, "observed"):
            protocol.compare_records(client, server)

    def test_record_rejects_missing_extra_and_wrong_fields(self):
        changes = (
            lambda value: value.pop("fresh_packed"),
            lambda value: value.update(extra=0),
            lambda value: value.update(schema=True),
            lambda value: value.update(constructor_cells=True),
            lambda value: value.update(packed_length=16383),
            lambda value: value.update(getter_order="X/Y/Z"),
            lambda value: value.update(jar_sha256_after="0" * 64),
            lambda value: value["bindings"].update(owner="ob"),
            lambda value: value["controls"].update(array_identity_unchanged=1),
            lambda value: value["controls"].update(jar_unchanged=False),
            lambda value: value["writes"][0].update(value=True),
            lambda value: value["writes"][0].update(x=16),
            lambda value: value["writes"][0].update(observed=16),
            lambda value: value["writes"].pop(),
        )
        for index, change in enumerate(changes):
            value = synthetic_record("client")
            change(value)
            with self.subTest(index=index), self.assertRaises(protocol.ObservationError):
                protocol.parse_record(encoded(value), "client")

    def test_raw_json_rejects_duplicate_truncated_extra_and_nonfinite_records(self):
        raw = encoded(synthetic_record("client"))
        invalid = (raw.replace(b'"schema":1', b'"schema":1,"schema":1', 1), raw[:-1],
                   raw + raw, raw.replace(b'"schema":1', b'"schema":NaN', 1),
                   raw + b" ", b"\xff\n", b"[" * 2000 + b"]" * 2000 + b"\n",
                   b" " * protocol.MAX_RECORD_BYTES + b"\n")
        for index, value in enumerate(invalid):
            with self.subTest(index=index), self.assertRaises(protocol.ObservationError):
                protocol.parse_record(value, "client")
        self.assertIsNone(protocol.parse_record(b"", "client"))

    def test_hex_and_final_snapshot_controls(self):
        for field, replacement in (("fresh_packed", "01"), ("prepared_packed", "00"),
                                   ("initial_getters", "ff"), ("final_getters", "ff"),
                                   ("final_packed", "00")):
            value = synthetic_record("client")
            value[field] = replacement + value[field][2:]
            with self.subTest(field=field), self.assertRaises(protocol.ObservationError):
                protocol.validate_record(value, "client")
        for replacement in ("A7", "g7", "a", "a7 "):
            value = synthetic_record("client")
            value["writes"][0]["packed"] = replacement + value["writes"][0]["packed"][2:]
            with self.subTest(replacement=replacement), self.assertRaises(protocol.ObservationError):
                protocol.validate_record(value, "client")

    def test_vectors_require_two_originals_and_retain_observed_changes(self):
        client, server = (synthetic_record(side) for side in ("client", "server"))
        for value in (client, server):
            value["writes"][0]["packed"] = "f1" + value["writes"][0]["packed"][2:]
            value["writes"][0]["observed"] = 9
        vectors = protocol.reference_vectors(client, server)
        self.assertEqual(vectors["writes"][0]["observed"], 9)
        self.assertEqual(vectors["writes"][0]["changed_bytes"],
                         [{"offset": 0, "before": 0xa7, "after": 0xf1}])
        self.assertEqual(vectors["writes"][1]["changed_bytes"],
                         [{"offset": 0, "before": 0xf1, "after": 0xa7}])
        with self.assertRaises(protocol.ObservationError):
            protocol.reference_vectors(synthetic_record("c"), server)

    def test_lifecycle_and_failure_statuses(self):
        record = synthetic_record("client")
        markers = protocol.lifecycle(completed_markers("client"), "client")
        self.assertEqual(protocol.classify_original(record, markers, 0, None, False), "pass")
        for code, reason, timeout in ((77, None, False), (1, None, False),
                                      (0, "stdout live byte limit exceeded", False), (0, None, True)):
            with self.subTest(code=code, reason=reason, timeout=timeout), self.assertRaises(protocol.ObservationError):
                protocol.classify_original(record, markers, code, reason, timeout)
        unavailable = protocol.lifecycle(b"UNAVAILABLE: nibbles setup class load java.lang.NoClassDefFoundError\n", "client")
        self.assertEqual(protocol.classify_original(None, unavailable, 77, None, False), "unavailable")
        with self.assertRaises(protocol.ObservationError):
            protocol.classify_original(None, protocol.lifecycle(b"", "client"), 77, None, False)
        with self.assertRaises(protocol.ObservationError):
            protocol.classify_original(record, unavailable, 77, None, False)

    def test_lifecycle_rejects_bad_phase_and_controls(self):
        complete = completed_markers("client")
        malformed = (
            complete + b"NIBBLES_UNKNOWN\n", complete + b"UNAVAILABLE: nibbles setup archive missing/unreadable\n",
            complete.replace(b"NIBBLES_MEASURE_BEGIN side=client\n", b""),
            complete.replace(b"fresh_zero", b"fresh_one"),
            complete.replace(b"NIBBLES_SETUP_DONE side=client class=wi", b"NIBBLES_SETUP_DONE side=server class=ob"),
            b"NIBBLES_MEASURE_DONE side=client\n",
            b"UNAVAILABLE: arbitrary error\n", b"NIBBLES_\xff\n",
        )
        for index, raw in enumerate(malformed):
            with self.subTest(index=index), self.assertRaises(protocol.ObservationError):
                protocol.lifecycle(raw, "client")


class DriverTests(unittest.TestCase):
    def test_unsupported_java_is_unavailable(self):
        with tempfile.TemporaryDirectory() as temporary:
            release = Path(temporary) / "release"
            release.write_bytes(b'JAVA_VERSION="17.0.20"\r\n')
            self.assertEqual(driver.release_version(release), "17.0.20")
            for version in ("1.8.0_452", "9.0.4", "10.0.2"):
                release.write_text('JAVA_VERSION="' + version + '"\n')
                with self.subTest(version=version), self.assertRaises(driver.Unavailable):
                    driver.release_version(release)
            release.write_text("missing version\n")
            with self.assertRaises(driver.Failure):
                driver.release_version(release)

    def test_missing_originals_are_unavailable_before_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            work = folder / "must-not-exist"
            with contextlib.redirect_stderr(io.StringIO()), mock.patch.object(common, "run_live") as child:
                status = driver.main(["--capture-only", "--client", str(folder / "missing-client.jar"),
                                      "--server", str(folder / "missing-server.jar"), "--work-dir", str(work)])
            self.assertEqual(status, 77)
            self.assertFalse(work.exists())
            child.assert_not_called()

    def test_java_options_reject_without_exposing_value_or_writing(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "must-not-exist"
            error = io.StringIO()
            with mock.patch.dict(os.environ, {"JAVA_TOOL_OPTIONS": "private-test-value"}), \
                    contextlib.redirect_stderr(error):
                status = driver.main(["--capture-only", "--work-dir", str(work)])
            self.assertEqual(status, 77)
            self.assertIn("JAVA_TOOL_OPTIONS", error.getvalue())
            self.assertNotIn("private-test-value", error.getvalue())
            self.assertFalse(work.exists())

    def test_work_ancestor_rejects_without_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            supplied = folder / "input.txt"
            supplied.write_text("preserve me")
            with self.assertRaises(common.Failure):
                common.samefile_or_ancestor(folder, [supplied])
            self.assertEqual(supplied.read_text(), "preserve me")

    def test_hash_preservation_stays_bounded_when_input_grows(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "input.txt"
            path.write_bytes(b"x")
            before = common.file_record(path, byte_limit=1)
            path.write_bytes(b"xx")
            _, errors = driver.after_inputs(common, {str(path): before})
            self.assertTrue(errors)

    def test_raw_stream_and_unexpected_artifact_bounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            path = folder / "unexpected.txt"
            path.write_bytes(b"x")
            inventory = driver.owned_inventory(common, folder)
            self.assertFalse(inventory["complete"])
            self.assertIn("unexpected", inventory["error"])
            with mock.patch.object(driver, "MAX_STREAM", 0), self.assertRaises(driver.Failure):
                driver.read_stream(path)

    def run_synthetic_capture(self, folder, *, mismatch=False, client_unavailable=False, change_input=False):
        """Exercise supervision and status handling with no Java or C process."""
        jars = {side: folder / (side + ".jar") for side in protocol.JARS}
        for jar in jars.values():
            jar.write_bytes(b"synthetic archive marker")
        release = folder / "release"
        release.write_text('JAVA_VERSION="17.0.20"\n')
        original_record = common.file_record
        changed = False

        def attributed(path, byte_limit=None):
            value = original_record(path, byte_limit=byte_limit)
            for side, jar in jars.items():
                if Path(value["path"]) == jar:
                    value["sha256"] = protocol.JARS[side][0]
            if changed and Path(value["path"]) == driver.SOURCE:
                value["sha256"] = "0" * 64
            return value

        inputs = {str(path): attributed(path) for path in [driver.SOURCE, *jars.values()]}
        members = {side: {protocol.JARS[side][1] + ".class": {"bytes": 618, "sha256": "0" * 64}}
                   for side in jars}
        args = argparse.Namespace(capture_only=True, tested_commit=None)

        def fake_child(command, stdout, stderr, cwd, **bounds):
            nonlocal changed
            self.assertEqual(bounds["seconds"], 30)
            self.assertEqual(bounds["stdout_limit"], 4 * 1024 * 1024)
            self.assertEqual(bounds["stderr_limit"], 4 * 1024 * 1024)
            out, err, code = b"", b"", 0
            if "--release" in command:
                classes = Path(command[command.index("-d") + 1])
                (classes / "NibblesObservation.class").write_bytes(bytes.fromhex("cafebabe00000037"))
            else:
                side = command[-2]
                if side == "client" and client_unavailable:
                    err = b"UNAVAILABLE: nibbles setup class load java.lang.NoClassDefFoundError\n"
                    code = 77
                    changed = change_input
                else:
                    value = synthetic_record(side)
                    if side == "server" and mismatch:
                        value["final_getters"] = "0f" + value["final_getters"][2:]
                        code = 77
                    out, err = encoded(value), completed_markers(side)
            stdout.write_bytes(out)
            stderr.write_bytes(err)
            return common.ProcessResult(command, code, 0.01, False, None,
                                        len(out), len(err), len(out), len(err), 0)

        with mock.patch.object(common, "file_record", side_effect=attributed), \
                mock.patch.object(common, "run_live", side_effect=fake_child), \
                mock.patch.object(driver, "archive_members", side_effect=lambda path: members[next(
                    side for side, jar in jars.items() if jar == path)]), \
                contextlib.redirect_stdout(io.StringIO()):
            code = driver.capture(args, common, protocol, [], inputs, jars,
                                  {"java": "synthetic-java", "javac": "synthetic-javac", "jdk_release": release},
                                  members, None, folder / "captures")
        manifests = list((folder / "captures").glob("*/manifest.json"))
        self.assertEqual(len(manifests), 1)
        return code, json.loads(manifests[0].read_text())

    def test_synthetic_capture_retains_complete_pair_and_vectors(self):
        with tempfile.TemporaryDirectory() as temporary:
            code, report = self.run_synthetic_capture(Path(temporary))
            self.assertEqual(code, 0)
            self.assertEqual(report["status"], "pass")
            self.assertTrue(report["completed"])
            self.assertTrue(report["inputs_preserved"])
            self.assertTrue(report["original_members_preserved"])
            self.assertTrue(report["classes_preserved"])
            self.assertEqual(report["sides"]["client"]["records"], 1)
            self.assertEqual(report["sides"]["server"]["records"], 1)
            self.assertEqual(len(report["comparisons"]), 1)
            self.assertIn("reference_vectors_artifact", report)

    def test_numeric_mismatch_precedes_later_exit77(self):
        with tempfile.TemporaryDirectory() as temporary:
            code, report = self.run_synthetic_capture(Path(temporary), mismatch=True)
            self.assertEqual(code, 1)
            self.assertEqual(report["status"], "fail")
            self.assertFalse(report["completed"])
            self.assertIsNone(report["unavailable_reason"])
            self.assertTrue(any("final_getters" in error for error in report["errors"]))

    def test_setup_unavailable_requires_preservation(self):
        for changed in (False, True):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as temporary:
                code, report = self.run_synthetic_capture(Path(temporary), client_unavailable=True,
                                                         change_input=changed)
                self.assertEqual(code, 1 if changed else 77)
                self.assertEqual(report["status"], "fail" if changed else "unavailable")
                self.assertFalse(report["completed"])


if __name__ == "__main__":
    unittest.main()
