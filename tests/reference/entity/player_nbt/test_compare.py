"""Authored receipt-rejection controls; these are not game observations."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("player_nbt_comparison_controls", HERE / "compare.py")
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class ComparisonControls(unittest.TestCase):
    def receipt(self):
        return {"exit": 0, "reader_errors": {}, "pipe_capture_complete": True, "reaped": True,
                "timed_out": False, "output_overflow": False, "stdout_bytes": 4, "stderr_bytes": 0}

    def test_complete_authored_receipt(self):
        driver.capture_receipt(self.receipt(), b"text", b"")

    def test_reader_failure_or_missing_reader_inventory(self):
        for value in ({"stderr": [{"phase": "write", "type": "OSError"}]}, None):
            r = self.receipt()
            if value is None:
                del r["reader_errors"]
            else:
                r["reader_errors"] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                driver.capture_receipt(r, b"text", b"")

    def test_failed_exit_and_unreaped_or_incomplete_capture(self):
        for key, value in (("exit", 1), ("reaped", False), ("pipe_capture_complete", False)):
            r = self.receipt(); r[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                driver.capture_receipt(r, b"text", b"")

    def test_bounds_and_nonempty_stderr(self):
        for key, value in (("timed_out", True), ("output_overflow", True), ("stdout_bytes", 3),
                           ("stderr_bytes", 1)):
            r = self.receipt(); r[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                driver.capture_receipt(r, b"text", b"")
        with self.assertRaises(ValueError):
            driver.capture_receipt(self.receipt(), b"text", b"diagnostic")

    def test_partial_native_output_never_counts_as_run(self):
        for text in ("", "Player NBT: 1 checks\n",
                     "Native Player NBT observations: 33 cases, 11 complete reads, 20 interrupted reads\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.native_projection(text)

    def test_duplicate_case_inventory(self):
        lines = [f"END {case} OK" for case in driver.original.CASES]
        lines.append(lines[-1])
        with self.assertRaises(ValueError):
            driver.native_projection("\n".join(lines))

    def test_case_order_cannot_be_sorted_away(self):
        lines = [f"END {case} OK" for case in driver.original.CASES]
        lines[0], lines[1] = lines[1], lines[0]
        with self.assertRaises(ValueError):
            driver.native_projection("\n".join(lines))

    def living_prefix_fixture(self):
        # Authored frame records, using the four actual ls.a(Lnu;)V getter PCs.
        return [["PREFIX", name, "311", str(pc), "2", "2", "0", "0", "18446744073709551615"]
                for name, pc in (("health_wrong", 4), ("hurt_wrong", 29),
                                 ("death_wrong", 39), ("attack_wrong", 49))]

    def test_actual_living_getter_failure_sites(self):
        driver.living_getter_sites(self.living_prefix_fixture())

    def test_later_living_stores_cannot_report_getter_failure(self):
        for index, store_pc in enumerate((7, 32, 42, 52)):
            rows = self.living_prefix_fixture()
            rows[index][3] = str(store_pc)
            with self.subTest(index=index), self.assertRaisesRegex(ValueError, "source-frame differs"):
                driver.living_getter_sites(rows)

    def test_living_getter_failure_inventory_and_order(self):
        rows = self.living_prefix_fixture()
        for changed in (rows[:-1], rows + [rows[-1]], [rows[1], rows[0], *rows[2:]]):
            with self.subTest(rows=changed), self.assertRaisesRegex(ValueError, "inventory/order differs"):
                driver.living_getter_sites(changed)

    def authored_receipt_fixture(self, root):
        """Receipt schema fixture only; no file here is run as a program."""
        executable = root / "authored-executable-identity.exe"
        executable.write_bytes(b"Authored identity only; never executed.")
        extra = root / "authored-input.txt"
        extra.write_bytes(b"authored fixture input")
        stdout = b"Authored banner\nPlayer active tick: 1 checks\nPlayer NBT: 1 checks\n"
        (root / "native.stdout").write_bytes(stdout)
        (root / "native.stderr").write_bytes(b"")
        capture = self.receipt()
        capture.update(command=[str(executable)], stdout_bytes=len(stdout))
        (root / "native.capture.json").write_text(json.dumps(capture))
        hashes = {str(p): driver.digest(p) for p in (executable, extra)}
        report = {"status": "PASS", "kind": "nbt", "checks": 1, "capture": capture,
                  "input_before_sha256": dict(hashes), "input_after_sha256": dict(hashes),
                  "products": {name: driver.digest(root / name) for name in
                               ("native.stdout", "native.stderr", "native.capture.json")}}
        path = root / "report.json"
        path.write_text(json.dumps(report))
        return path, executable, report

    def test_configured_executable_required(self):
        with tempfile.TemporaryDirectory() as directory:
            path, executable, report = self.authored_receipt_fixture(Path(directory))
            self.assertEqual(driver.checked_run(path, native=True, executable=executable)["checks"], 1)
            with self.assertRaisesRegex(ValueError, "configured native executable"):
                driver.checked_run(path, native=True)

    def test_executable_must_be_before_and_after_input(self):
        with tempfile.TemporaryDirectory() as directory:
            path, executable, report = self.authored_receipt_fixture(Path(directory))
            for name in ("input_before_sha256", "input_after_sha256"):
                report[name].pop(str(executable))
            path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "executable is not bound"):
                driver.checked_run(path, native=True, executable=executable)

    def test_missing_or_foreign_invoked_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, executable, report = self.authored_receipt_fixture(root)
            foreign = root / "foreign.exe"
            foreign.write_bytes(executable.read_bytes())
            for invoked in (root / "missing.exe", foreign):
                report["capture"]["command"][0] = str(invoked)
                path.write_text(json.dumps(report))
                with self.subTest(invoked=invoked), self.assertRaisesRegex(ValueError, "command executable differs"):
                    driver.checked_run(path, native=True, executable=executable)

    def test_embedded_capture_must_equal_pinned_product(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for phase in ("native", "original", "prefix"):
                receipt = self.receipt()
                path = root / (phase + ".capture.json")
                path.write_text(json.dumps(receipt))
                products = {path.name: driver.digest(path)}
                driver.captured_receipt(root, products, phase, receipt)
                receipt["stdout_bytes"] = 3
                with self.subTest(phase=phase), self.assertRaisesRegex(ValueError, "embedded receipt differs"):
                    driver.captured_receipt(root, products, phase, receipt)

    def test_missing_capture_product_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path, executable, report = self.authored_receipt_fixture(Path(directory))
            del report["products"]["native.capture.json"]
            path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "required captured"):
                driver.checked_run(path, native=True, executable=executable)

    def test_changed_capture_file_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path, executable, report = self.authored_receipt_fixture(Path(directory))
            (path.parent / "native.capture.json").write_text("{}")
            with self.assertRaisesRegex(ValueError, "captured product differs"):
                driver.checked_run(path, native=True, executable=executable)

    def test_boolean_exit_is_not_an_exit_code(self):
        receipt = self.receipt(); receipt["exit"] = False
        with self.assertRaises(ValueError):
            driver.capture_receipt(receipt, b"text", b"")

    def test_every_consumed_stream_and_receipt_requires_hash(self):
        names = [phase + suffix for phase in
                 ("java-version", "javac-version", "compile", "original", "prefix")
                 for suffix in (".stdout", ".stderr", ".capture.json")]
        names += ["sleep-target.stdout", "sleep-target.stderr"]
        products = dict.fromkeys(names, "authored hash shape only")
        driver.required_products(products, names)
        for name in names:
            changed = dict(products); del changed[name]
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "required captured"):
                driver.required_products(changed, names)

    def test_missing_stdout_hash_cannot_hide_same_length_change(self):
        with tempfile.TemporaryDirectory() as directory:
            path, executable, report = self.authored_receipt_fixture(Path(directory))
            stdout = path.parent / "native.stdout"
            data = stdout.read_bytes(); stdout.write_bytes(b"a" + data[1:])
            del report["products"]["native.stdout"]
            path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "required captured"):
                driver.checked_run(path, native=True, executable=executable)

    def helper_fixture(self, root):
        classes = root / "classes"; classes.mkdir()
        for name in driver.original.HELPERS:
            (classes / name).write_bytes(b"Authored inventory identity only; never loaded.")
        hashes = {name: driver.digest(classes / name) for name in driver.original.HELPERS}
        return {"helper_before_sha256": dict(hashes), "helper_after_sha256": dict(hashes),
                "helper_class_code": dict.fromkeys(hashes, {})}

    def test_empty_helper_inventory_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            report = {"helper_before_sha256": {}, "helper_after_sha256": {}, "helper_class_code": {}}
            with self.assertRaisesRegex(ValueError, "helper inventory is missing"):
                driver.checked_helpers(report, Path(directory))

    def test_missing_helper_inventory_entry_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); report = self.helper_fixture(root)
            name = next(iter(driver.original.HELPERS))
            for key in ("helper_before_sha256", "helper_after_sha256"):
                del report[key][name]
            with self.assertRaisesRegex(ValueError, "helper inventory is missing"):
                driver.checked_helpers(report, root)

    def test_extra_actual_helper_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); report = self.helper_fixture(root)
            (root / "classes/Unexpected.class").write_bytes(b"authored identity")
            with self.assertRaisesRegex(ValueError, "actual compiled helper inventory differs"):
                driver.checked_helpers(report, root)

    def test_missing_helper_code_inventory_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); report = self.helper_fixture(root)
            report["helper_class_code"] = {}
            with self.assertRaisesRegex(ValueError, "Code inventory differs"):
                driver.checked_helpers(report, root)

    def test_missing_original_map_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing without an explicit mapping"):
            driver.original_input_path("Z:\\no-such-nbt-original\\input.bin", {})

    def test_missing_mapped_original_file_rejected(self):
        with self.assertRaisesRegex(ValueError, "mapped original input is missing"):
            driver.original_input_path("Z:\\no-such-nbt-original\\input.bin",
                                       {"Z:\\no-such-nbt-original\\input.bin": "Z:\\no-such-mapped-file.bin"})

    def test_wrong_original_map_hash_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            expected = root / "expected.bin"; expected.write_bytes(b"authored expected identity")
            wrong = root / "wrong.bin"; wrong.write_bytes(b"authored different identity")
            recorded = "Z:\\no-such-nbt-original\\input.bin"
            hashes = {recorded: driver.digest(expected)}
            report = {"status": "PASS", "input_before_sha256": hashes,
                      "input_after_sha256": hashes, "products": {}}
            path = root / "report.json"; path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "run input no longer matches"):
                driver.checked_run(path, native=False, relocations={recorded: str(wrong)})

    def test_origin_view_changes_only_filesystem_location(self):
        raw = "ORIGIN dc C:\\Root Name\\actual.jar\nSTATE untouched scalar values\n"
        view, count = driver.original_origin_view(raw, "C:\\Root Name\\actual.jar", Path("/mapped/actual.jar"))
        self.assertEqual(count, 1)
        self.assertEqual(view, "ORIGIN dc " + str(Path("/mapped/actual.jar")) + "\nSTATE untouched scalar values\n")
        self.assertEqual(raw, "ORIGIN dc C:\\Root Name\\actual.jar\nSTATE untouched scalar values\n")
        uri, count = driver.original_origin_view("ORIGIN dc file:///C:/Root%20Name/actual.jar\n",
                                                 "C:\\Root Name\\actual.jar", Path("/mapped/actual.jar"))
        self.assertEqual(count, 1)
        self.assertEqual(uri, view.splitlines()[0] + "\n")

    def test_wrong_raw_original_origin_rejected(self):
        for origin in ("C:\\Other\\actual.jar", "D:\\Root Name\\actual.jar",
                       "file://foreign-host/C:/Root%20Name/actual.jar"):
            with self.subTest(origin=origin), self.assertRaises(ValueError):
                driver.original_origin_view("ORIGIN dc " + origin + "\n",
                                             "C:\\Root Name\\actual.jar", Path("/mapped/actual.jar"))

    def test_native_inputs_cannot_be_rescued_by_original_maps(self):
        with tempfile.TemporaryDirectory() as directory:
            path, executable, report = self.authored_receipt_fixture(Path(directory))
            missing = str(path.parent / "missing-native-input.bin")
            for key in ("input_before_sha256", "input_after_sha256"):
                report[key][missing] = driver.digest(executable)
            path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, "native path mappings are forbidden"):
                driver.checked_run(path, native=True, executable=executable,
                                   relocations={missing: str(executable)})

    def test_original_origin_query_rejected(self):
        with self.assertRaisesRegex(ValueError, "query or fragment"):
            driver.original_origin_view("ORIGIN dc file:///C:/Root/actual.jar?different=1\n",
                                         "C:\\Root\\actual.jar", Path("/mapped/actual.jar"))

    def test_original_origin_fragment_rejected(self):
        with self.assertRaisesRegex(ValueError, "query or fragment"):
            driver.original_origin_view("ORIGIN dc file:///C:/Root/actual.jar#different\n",
                                         "C:\\Root\\actual.jar", Path("/mapped/actual.jar"))


if __name__ == "__main__":
    unittest.main()
