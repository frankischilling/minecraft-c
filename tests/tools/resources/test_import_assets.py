"""Public tests use generated resources and never need Minecraft files."""

from __future__ import annotations

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock
import zipfile


SPEC = importlib.util.spec_from_file_location(
    "asset_import", Path(__file__).resolve().parents[3] / "scripts" / "resources" / "import_assets.py"
)
assert SPEC and SPEC.loader
assets = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets)


class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.jar = self.root / "fixture.jar"
        self.resources = self.root / "resources"
        self.output = self.root / "imported"
        self.make_jar()
        for name in assets.REQUIRED_EXTERNAL:
            path = self.resources / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"OggS synthetic public fixture")

    def make_jar(self, *, omit=None, extras=()):
        with zipfile.ZipFile(self.jar, "w") as archive:
            for name in assets.JAR_RESOURCES:
                if name != omit:
                    archive.writestr(name, ("synthetic " + name).encode())
            archive.writestr("Example.class", b"Not copied")
            archive.writestr("META-INF/MANIFEST.MF", b"Not copied")
            for name, data in extras:
                archive.writestr(name, data)

    def import_fixture(self):
        # Production CLI has no hash override. Only this test's module constant
        # changes so the complete import path can use a harmless generated JAR.
        digest = hashlib.sha256(self.jar.read_bytes()).hexdigest()
        with mock.patch.object(assets, "CLIENT_SHA256", digest):
            return assets.import_assets(self.jar, self.resources, self.output)

    def test_imports_only_approved_paths_and_writes_content_manifest(self):
        ignored = self.resources / "sound3" / "unrelated.ogg"
        ignored.parent.mkdir()
        ignored.write_bytes(b"OggS excluded")
        mus = self.resources / "streaming" / "fixture.mus"
        mus.write_bytes(b"synthetic encrypted audio")
        result = self.import_fixture()
        self.assertEqual(len(result["files"]), len(assets.JAR_RESOURCES) + len(assets.REQUIRED_EXTERNAL) + 1)
        self.assertFalse((self.output / "jar" / "Example.class").exists())
        self.assertFalse((self.output / "resources" / "sound3").exists())
        self.assertEqual(mus.read_bytes(), (self.output / "resources/streaming/fixture.mus").read_bytes())
        self.assertEqual(json.loads((self.output / "manifest.json").read_text()), result)
        for item in result["files"]:
            data = (self.output / item["path"]).read_bytes()
            self.assertEqual(len(data), item["bytes"])
            self.assertEqual(hashlib.sha256(data).hexdigest(), item["sha256"])

    def test_cli_rejects_unsupported_jar_and_does_not_create_output(self):
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
            code = assets.main(["--jar", str(self.jar), "--resources", str(self.resources), "--output", str(self.output)])
        self.assertEqual(code, 1)
        self.assertIn("Unsupported client JAR SHA-256", stderr.getvalue())
        self.assertFalse(self.output.exists())

    def test_missing_jar_resource_reports_name_before_writing(self):
        self.make_jar(omit="font/default.png")
        with self.assertRaisesRegex(assets.ImportFailure, "Missing required JAR resources: font/default.png"):
            self.import_fixture()
        self.assertFalse(self.output.exists())

    def test_missing_external_resource_reports_expected_root(self):
        (self.resources / "streaming/cat.ogg").unlink()
        with self.assertRaisesRegex(assets.ImportFailure, "streaming/cat.ogg.*--resources"):
            self.import_fixture()
        self.assertFalse(self.output.exists())

    def test_archive_traversal_rejected_without_writing(self):
        self.make_jar(extras=[("../../escape.png", b"bad")])
        with self.assertRaisesRegex(assets.ImportFailure, "Unsafe resource path"):
            self.import_fixture()
        self.assertFalse((self.root / "escape.png").exists())
        self.assertFalse(self.output.exists())

    def test_duplicate_archive_entry_is_rejected(self):
        with mock.patch("warnings.warn"):
            self.make_jar(extras=[("terrain.png", b"duplicate")])
        with self.assertRaisesRegex(assets.ImportFailure, "Duplicate archive entry"):
            self.import_fixture()

    def test_archive_link_is_rejected(self):
        entry = zipfile.ZipInfo("link")
        entry.create_system = 3
        entry.external_attr = (stat.S_IFLNK | 0o777) << 16
        with zipfile.ZipFile(self.jar, "a") as archive:
            archive.writestr(entry, "../outside")
        with self.assertRaisesRegex(assets.ImportFailure, "Archive link"):
            self.import_fixture()

    def test_existing_destination_survives_failed_import(self):
        self.output.mkdir()
        marker = self.output / "keep.txt"
        marker.write_text("keep", encoding="utf-8")
        with self.assertRaisesRegex(assets.ImportFailure, "Output already exists"):
            self.import_fixture()
        self.assertEqual(marker.read_text(), "keep")

    def test_bad_ogg_header_cleans_staging_directory(self):
        (self.resources / "newsound/random/click.ogg").write_bytes(b"invalid")
        with self.assertRaisesRegex(assets.ImportFailure, "lacks an Ogg header"):
            self.import_fixture()
        self.assertFalse(self.output.exists())
        self.assertFalse(list(self.root.glob(".asset-import-*")))

    def test_resource_and_total_size_bounds(self):
        with mock.patch.object(assets, "MAX_FILE_BYTES", 1):
            with self.assertRaisesRegex(assets.ImportFailure, "size outside allowed range"):
                self.import_fixture()
        with mock.patch.object(assets, "MAX_TOTAL_BYTES", 1):
            with self.assertRaisesRegex(assets.ImportFailure, "total bytes"):
                self.import_fixture()
        self.assertFalse(self.output.exists())

    def test_external_links_are_rejected(self):
        target = self.resources / "newsound/random/click.ogg"
        original = self.root / "actual.ogg"
        target.rename(original)
        try:
            target.symlink_to(original)
        except OSError as error:
            self.skipTest(f"OS cannot create test symlink: {error}")
        with self.assertRaisesRegex(assets.ImportFailure, "Links and reparse points"):
            self.import_fixture()
        self.assertFalse(self.output.exists())

    def test_destination_ancestor_links_are_rejected(self):
        actual = self.root / "actual-output"
        actual.mkdir()
        linked = self.root / "linked-output"
        try:
            linked.symlink_to(actual, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"OS cannot create test symlink: {error}")
        self.output = linked / "assets"
        with self.assertRaisesRegex(assets.ImportFailure, "Links and reparse points"):
            self.import_fixture()
        self.assertFalse((actual / "assets").exists())

    def test_windows_reparse_attribute_is_rejected_without_symlink_privilege(self):
        fake = mock.Mock(st_mode=stat.S_IFDIR | 0o755, st_file_attributes=0x400)
        with mock.patch.object(Path, "lstat", return_value=fake):
            with self.assertRaisesRegex(assets.ImportFailure, "Links and reparse points"):
                assets.plain_path(self.resources, directory=True)

    def test_portable_paths_reject_windows_and_posix_escape_forms(self):
        for path in ("../escape", "/escape", "C:/escape", "sound\\escape", "sound//escape", "sound/./escape", "sound/NUL.ogg", "sound/x:stream", "sound/x."):
            with self.subTest(path=path), self.assertRaises(assets.ImportFailure):
                assets.safe_relative(path)

    def test_copy_bound_detects_source_changes(self):
        for data, size in ((b"short", 6), (b"long", 3)):
            with self.subTest(data=data), self.assertRaises(assets.ImportFailure):
                assets.copy_bounded(io.BytesIO(data), io.BytesIO(), size)

    def test_unexpected_external_extension_is_rejected(self):
        (self.resources / "music/unexpected.txt").write_text("fixture")
        with self.assertRaisesRegex(assets.ImportFailure, "Unsupported external resource extension"):
            self.import_fixture()


if __name__ == "__main__":
    unittest.main()
