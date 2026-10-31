"""Package guards for exact original-only dedicated numerical observations."""
import base64
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location("dedicated_fixture_controls", ROOT / "scripts/reference/world/population/dedicated/fixtures.py")
F = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = F
SPEC.loader.exec_module(F)


class DedicatedFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = F.comparison().strict_json(F.DEFAULT.read_bytes())
        cls.case = F.decode_package(cls.package)

    def reject(self, edit):
        changed = copy.deepcopy(self.package)
        edit(changed)
        with self.assertRaises(ValueError):
            F.decode_package(changed)

    def test_all_original_bytes_and_provenance_survive(self):
        for role, expected in F.PAYLOADS.items():
            self.assertEqual(F.fingerprint(self.case[role]), expected)
        self.assertEqual(len(self.case["constructor_inputs"].splitlines()), 10)
        self.assertTrue(self.case["provenance"]["source_dirty"])
        self.assertIsNone(self.case["provenance"]["tested_commit"])
        self.assertFalse(self.case["validation"]["source_classfile_audit_repeated"])
        self.assertEqual(self.case["validation"]["boundaries"], 18)
        self.assertEqual(self.case["validation"]["source_blobs"], 240)
        self.assertNotIn("native", self.package["payloads"])

    def test_exact_compression_round_trip(self):
        for role, expected in F.PAYLOADS.items():
            with self.subTest(role=role):
                packed = F.packed(self.case[role])
                self.assertEqual(F.unpack(packed, expected), self.case[role])
                self.assertLessEqual(expected["bytes"], packed["compressed_bytes"] * F.MAX_RATIO)

    def test_native_expected_state_and_unknown_roles_rejected(self):
        self.reject(lambda package: package["payloads"].update(native={}))
        self.reject(lambda package: package.update(expected={}))
        self.reject(lambda package: package["provenance"].update(processes=[]))

    def test_dirty_attribution_cannot_be_upgraded(self):
        self.reject(lambda package: package["provenance"].update(tested_commit="a" * 40))
        self.reject(lambda package: package["provenance"].update(source_dirty=False))
        self.reject(lambda package: package["provenance"]["inherited_audit"].update(qualification=True))
        self.reject(lambda package: package["provenance"]["inherited_audit"].update(semantic_audit_complete=False))
        self.reject(lambda package: package["provenance"]["inherited_audit"].update(passed=1))

    def test_source_audit_and_clock_bindings_are_fixed(self):
        self.reject(lambda package: package["provenance"]["observer_sources"].pop("ChunkObservation.java"))
        self.reject(lambda package: package["provenance"]["observer_sources"]["ChunkObservation.java"].update(sha256="0" * 64))
        self.reject(lambda package: package["provenance"]["inherited_audit"]["artifact"].update(sha256="0" * 64))
        self.reject(lambda package: package["provenance"].update(root_semantics="normalize roots"))
        self.reject(lambda package: package["provenance"].update(clock_semantics="replace native clocks"))

    def test_payload_metadata_and_encoded_bytes_are_checked(self):
        controls = (
            lambda row: row.update(bytes=row["bytes"] + 1),
            lambda row: row.update(sha256="0" * 64),
            lambda row: row.update(compressed_sha256="0" * 64),
            lambda row: row.update(compressed_bytes=row["compressed_bytes"] + 1),
            lambda row: row.update(base64="!" + row["base64"][1:]),
            lambda row: row.update(base64=row["base64"] + "AAAA"),
            lambda row: row.update(codec="unbounded"),
        )
        for role in F.PAYLOADS:
            for index, edit in enumerate(controls):
                with self.subTest(role=role, control=index):
                    self.reject(lambda package: edit(package["payloads"][role]))

    def test_recomputed_hashes_do_not_allow_extra_or_truncated_streams(self):
        for suffix in (b"x", zlib.compress(b"extra", 9)):
            def append(package):
                row = package["payloads"]["trace"]
                raw = base64.b64decode(row["base64"]) + suffix
                row.update(compressed_bytes=len(raw), compressed_sha256=hashlib.sha256(raw).hexdigest(),
                           base64=base64.b64encode(raw).decode("ascii"))
            self.reject(append)
        def truncate(package):
            row = package["payloads"]["trace"]
            raw = base64.b64decode(row["base64"])[:-1]
            row.update(compressed_bytes=len(raw), compressed_sha256=hashlib.sha256(raw).hexdigest(),
                       base64=base64.b64encode(raw).decode("ascii"))
        self.reject(truncate)

    def test_ratio_and_decode_limits_precede_decompression(self):
        def excessive_ratio(package):
            package["payloads"]["original"].update(compressed_bytes=1,
                compressed_sha256=hashlib.sha256(b"x").hexdigest(), base64=base64.b64encode(b"x").decode("ascii"))
        self.reject(excessive_ratio)
        self.reject(lambda package: package["payloads"]["original"].update(bytes=F.MAX_DECODED + 1))

    def test_strict_json_rejects_duplicate_or_noninteger_metadata(self):
        for raw in (b'{"schema":1,"schema":1}', b'{"schema":1.0}', b'{"schema":NaN}'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                F.comparison().strict_json(raw)


if __name__ == "__main__":
    unittest.main()
