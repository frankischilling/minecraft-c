"""Filesystem-only controls for prelaunch native product preservation."""

import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import native_product_capture as capture


class NativeProductCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.build.mkdir()
        self.executable = self.build / "game scene.exe"
        self.original = b"test executable bytes\0\xff"
        self.executable.write_bytes(self.original)
        self.evidence = self.root / "evidence"

    def run_capture(self):
        return capture.capture_native_products([str(self.executable), "unused argument"],
                                               self.evidence)

    def manifest(self, receipt):
        return json.loads((self.evidence / receipt["manifest"]).read_text(encoding="utf-8"))

    def failure(self, operation=None):
        with self.assertRaises(capture.NativeProductCaptureError) as raised:
            (operation or self.run_capture)()
        error = raised.exception
        self.assertEqual(error.receipt["status"], "FAIL")
        self.assertIsNotNone(error.manifest)
        self.assertEqual(json.loads(error.manifest.read_text(encoding="utf-8")), error.receipt)
        return error.receipt

    def test_success_schema_and_optional_roles(self):
        receipt = self.run_capture()
        self.assertEqual(self.manifest(receipt), receipt)
        self.assertEqual(receipt["schema"], 1)
        self.assertEqual(receipt["status"], "PASS")
        self.assertIs(receipt["candidate_inputs_are_link_proof"], False)
        self.assertEqual(receipt["command_executable"], str(self.executable.resolve()))
        self.assertEqual(receipt["missing_optional_roles"], ["adjacent_native_libraries",
                         "candidate_sdl_archive", "candidate_sdl_source"])
        self.assertEqual(receipt["manifest"], "native-products/manifest.json")
        product, = receipt["products"]
        self.assertEqual(set(product), {"role", "source", "artifact", "status", "before",
                                       "copied", "preserved", "after"})
        self.assertEqual(product["status"], "CAPTURED")
        self.assertEqual(product["artifact"], "native-products/executable/game scene.exe")
        expected = {"sha256": hashlib.sha256(self.original).hexdigest(), "bytes": len(self.original)}
        for key in ("before", "copied", "preserved", "after"):
            self.assertEqual(product[key], expected)
        self.assertEqual(self.executable.read_bytes(), self.original)

    def test_preserved_bytes_survive_original_replacement(self):
        receipt = self.run_capture()
        replacement = self.build / "replacement"
        replacement.write_bytes(b"new executable")
        os.replace(replacement, self.executable)
        artifact = self.evidence / receipt["products"][0]["artifact"]
        self.assertEqual(artifact.read_bytes(), self.original)
        self.assertEqual(self.manifest(receipt), receipt)

    def test_adjacent_native_files_and_all_available_sdl_candidates(self):
        selected = {"OpenAL32.dll": b"dll", "UPPER.DLL": b"upper", "libAudio.so": b"so",
                    "libAudio.so.2.1": b"version"}
        for name, body in selected.items():
            (self.build / name).write_bytes(body)
        for name in ("game.jar", "image.png", "userdata.dat", "foo.so.jar", "foo.so.png",
                     "userdata.so.dat"):
            (self.build / name).write_bytes(b"excluded")
        candidates = {"_deps/mc_sdl3-build/SDL3-static.lib": b"win archive",
                      "_deps/mc_sdl3-build/libSDL3.a": b"unix archive",
                      "mc-sdl3-native/SDL_windowsevents.c": b"win source",
                      "mc-sdl3-native/SDL_x11events.c": b"x11 source"}
        for name, body in candidates.items():
            path = self.build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(body)
        receipt = self.run_capture()
        self.assertEqual(receipt["missing_optional_roles"], [])
        self.assertEqual(len(receipt["products"]), 9)
        native = {Path(p["artifact"]).name for p in receipt["products"]
                  if p["role"] == "adjacent_native_libraries"}
        self.assertEqual(native, set(selected))
        for product in receipt["products"]:
            self.assertEqual((self.evidence / product["artifact"]).read_bytes(),
                             Path(product["source"]).read_bytes())
        self.assertEqual(len({p["artifact"] for p in receipt["products"]}), 9)

    def test_existing_capture_never_overwritten(self):
        receipt = self.run_capture()
        before = {p.relative_to(self.evidence): p.read_bytes()
                  for p in (self.evidence / "native-products").rglob("*") if p.is_file()}
        failure = self.failure()
        self.assertIn("FileExistsError", failure["error"])
        self.assertNotEqual(failure["manifest"], receipt["manifest"])
        self.assertEqual(before, {p.relative_to(self.evidence): p.read_bytes()
                         for p in (self.evidence / "native-products").rglob("*") if p.is_file()})

    def test_source_inside_destination_rejected_without_source_write(self):
        self.executable = self.evidence / "native-products" / "already.exe"
        self.executable.parent.mkdir(parents=True)
        self.executable.write_bytes(self.original)
        failure = self.failure()
        self.assertIn("disjoint", failure["error"])
        self.assertEqual(self.executable.read_bytes(), self.original)
        self.assertEqual(failure["products"], [])

    def test_source_change_during_copy_fails_with_partial_receipt(self):
        original_copy = capture._copy

        def changing(source, target):
            result = original_copy(source, target)
            source.write_bytes(b"changed during capture")
            return result

        with mock.patch.object(capture, "_copy", side_effect=changing):
            failure = self.failure()
        product, = failure["products"]
        self.assertEqual(product["status"], "FAILED")
        self.assertNotEqual(product["before"], product["after"])
        self.assertEqual((self.evidence / product["artifact"]).read_bytes(), self.original)

    def test_same_bytes_replacement_during_copy_still_fails_identity(self):
        original_copy = capture._copy

        def replacing(source, target):
            result = original_copy(source, target)
            replacement = self.build / "replacement"
            replacement.write_bytes(self.original)
            os.replace(replacement, source)
            return result

        with mock.patch.object(capture, "_copy", side_effect=replacing):
            product, = self.failure()["products"]
        self.assertEqual(product["before"], product["after"])
        self.assertEqual(product["status"], "FAILED")

    def test_partial_copy_failure_retains_actual_partial_bytes(self):
        def interrupted(source, target):
            self.assertEqual(source, self.executable.resolve())
            target.write_bytes(b"partial")
            raise OSError("declared copy interruption")

        with mock.patch.object(capture, "_copy", side_effect=interrupted):
            product, = self.failure()["products"]
        self.assertEqual(product["partial"], {"sha256": hashlib.sha256(b"partial").hexdigest(),
                                             "bytes": 7})
        self.assertEqual(self.executable.read_bytes(), self.original)

    def test_later_failure_preserves_earlier_completed_product(self):
        (self.build / "audio.dll").write_bytes(b"native library")
        original_copy = capture._copy

        def interrupted(source, target):
            if source.suffix == ".dll":
                raise OSError("declared later copy failure")
            return original_copy(source, target)

        with mock.patch.object(capture, "_copy", side_effect=interrupted):
            failure = self.failure()
        self.assertEqual([p["status"] for p in failure["products"]], ["CAPTURED", "FAILED"])
        self.assertEqual((self.evidence / failure["products"][0]["artifact"]).read_bytes(),
                         self.original)

    def test_copied_target_tampering_rejected(self):
        original_copy = capture._copy

        def changed_target(source, target):
            result = original_copy(source, target)
            target.write_bytes(b"different target bytes")
            return result

        with mock.patch.object(capture, "_copy", side_effect=changed_target):
            product, = self.failure()["products"]
        self.assertNotEqual(product["copied"], product["preserved"])
        self.assertEqual(self.executable.read_bytes(), self.original)

    def test_explicit_path_does_not_search_PATH(self):
        name = "exists-only-on-PATH.exe"
        (self.build / name).write_bytes(b"not selected by a bare command")
        with mock.patch.dict(os.environ, {"PATH": str(self.build)}):
            failure = self.failure(lambda: capture.capture_native_products([name], self.evidence))
        self.assertIn("FileNotFoundError", failure["error"])
        self.assertEqual(failure["products"], [])

    def test_invalid_commands_retain_failure_manifest(self):
        for index, command in enumerate(([], "game.exe", [str(self.build)])):
            evidence = self.root / f"invalid-{index}"
            with self.assertRaises(capture.NativeProductCaptureError) as raised:
                capture.capture_native_products(command, evidence)
            error = raised.exception
            self.assertEqual(json.loads(error.manifest.read_text()), error.receipt)
            self.assertEqual(error.receipt["status"], "FAIL")
            self.assertEqual(error.receipt["products"], [])

    def test_streams_multiple_blocks(self):
        body = bytes(range(256)) * 12289
        self.executable.write_bytes(body)
        product, = self.run_capture()["products"]
        self.assertEqual(product["preserved"], {"sha256": hashlib.sha256(body).hexdigest(),
                                               "bytes": len(body)})

    def test_manifest_io_failure_carries_receipt_without_claiming_manifest(self):
        original_open = Path.open

        def denied(path, *args, **kwargs):
            if path.name == "manifest.json":
                raise PermissionError("declared manifest write failure")
            return original_open(path, *args, **kwargs)

        with mock.patch.object(Path, "open", denied):
            with self.assertRaises(capture.NativeProductCaptureError) as raised:
                self.run_capture()
        error = raised.exception
        self.assertIsNone(error.manifest)
        self.assertIsNone(error.receipt["manifest"])
        self.assertEqual(error.receipt["status"], "FAIL")
        self.assertIn("PermissionError", error.receipt["error"])
        self.assertEqual(self.executable.read_bytes(), self.original)


if __name__ == "__main__":
    unittest.main()
