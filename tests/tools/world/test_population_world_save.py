"""Corruption controls for the lossless full World save oracle."""

import copy
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "population_world_save_controls", ROOT / "scripts/reference/world/population/compare_world_save.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing full World save comparison")
D = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = D
SPEC.loader.exec_module(D)
F = D.P.load("population_world_save_fixture_controls", "scripts/reference/world/population/world_save_fixtures.py")
M = D.P.load("population_world_save_mutations", "tests/tools/world/test_population_persistence.py")


class WorldSaveEvidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = ROOT / "tests/reference/world/population/world_save_fixtures.json"
        cls.package = D.C.PUBLIC.strict_json(cls.path.read_bytes())
        cls.cases = F.load_package(cls.path, D)
        cls.original = cls.cases["client"]["original"]
        cls.jar = cls.cases["client"]["jar"]

    def reject_original(self, edit):
        value = copy.deepcopy(self.original)
        edit(value)
        with self.assertRaises((ValueError, RuntimeError, AssertionError, KeyError, TypeError)):
            D.project_original(value, "client", self.jar)

    def test_both_originals_rederive(self):
        self.assertEqual(set(self.cases), {"client", "server"})
        for side, case in self.cases.items():
            with self.subTest(side=side):
                expected = D.project_original(case["original"], side, case["jar"])
                self.assertEqual(expected, case["expected"])
                self.assertEqual(len(expected["boundaries"]), 7)
                self.assertEqual(len(expected["records"]), 4)
                self.assertEqual([row["size_on_disk"] for row in expected["metadata"]], [0, 24576])

    def test_actual_collection_owners_and_contents(self):
        for role in ("players", "named_cache", "ordered_list", "id_map"):
            def collection(value):
                graph = value["world_save"]["initial"]["collections"]
                return graph["players"] if role == "players" else graph["saved_data"][role]
            with self.subTest(role=role, change="size"):
                self.reject_original(lambda value: collection(value).update(size=1))
            with self.subTest(role=role, change="entry"):
                self.reject_original(lambda value: collection(value)["entries"].append(None))
            with self.subTest(role=role, change="owner"):
                self.reject_original(lambda value: collection(value).update(
                    object=value["world_save"]["initial"]["collections"]["world"]))

    def test_collection_owner_retention(self):
        for boundary in range(1, 7):
            with self.subTest(boundary=boundary):
                def edit(value):
                    D.all_boundaries(value["world_save"])[boundary]["collections"]["players"]["object"]["ref"] = "r999999"
                self.reject_original(edit)

    def test_live_info_version_size_and_last_played(self):
        for field, replacement in (("version", -1), ("size_on_disk", "0000000000000000"),
                                   ("last_played", "0000000000000001")):
            with self.subTest(field=field):
                def edit(value):
                    fields = value["world_save"]["saves"][0]["after"]["state"]["info"]["fields"]
                    next(row for row in fields if row["role"] == field)["value"] = replacement
                self.reject_original(edit)

    def test_complete_boundary_schedule(self):
        self.reject_original(lambda value: value["world_save"]["saves"].pop())
        self.reject_original(lambda value: value["world_save"]["saves"][0].update(returned_void=False))
        self.reject_original(lambda value: value["world_save"]["time_api"].update(previous="0000000000000001"))
        self.reject_original(lambda value: value["world_save"]["saves"][0].update(epoch_ms=[2, 1]))

    def test_metadata_fields_and_clock(self):
        saved = self.original["world_save"]["saves"][0]
        inventory = saved["after"]["state"]["disk"]
        raw = bytes.fromhex(M.file_entry(inventory, D.LEVEL)["hex"])
        plain = zlib.decompress(raw, 31)
        before = self.cases["client"]["expected"]["boundaries"][3]["info"]
        interval = saved["epoch_ms"]
        for name, kind, size in (("RandomSeed", 4, 8), ("SpawnX", 3, 4), ("SpawnY", 3, 4),
                                 ("SpawnZ", 3, 4), ("Time", 4, 8), ("SizeOnDisk", 4, 8),
                                 ("version", 3, 4), ("raining", 1, 1), ("rainTime", 3, 4),
                                 ("thundering", 1, 1), ("thunderTime", 3, 4)):
            with self.subTest(field=name):
                prefix = bytes((kind,)) + struct.pack(">H", len(name)) + name.encode("ascii")
                self.assertEqual(plain.count(prefix), 1)
                damaged = bytearray(plain)
                damaged[plain.index(prefix) + len(prefix) + size - 1] ^= 1
                encoded = zlib.compress(bytes(damaged), wbits=31)
                with self.assertRaises((ValueError, RuntimeError, AssertionError)):
                    D.metadata(encoded, before, interval)
        with self.assertRaises((ValueError, RuntimeError, AssertionError)):
            D.metadata(raw, before, [0, 1])

    def test_metadata_stream_completeness(self):
        saved = self.original["world_save"]["saves"][0]
        raw = bytes.fromhex(M.file_entry(saved["after"]["state"]["disk"], D.LEVEL)["hex"])
        before = self.cases["client"]["expected"]["boundaries"][3]["info"]
        for damaged in (raw[:-1], raw + b"x", raw + raw):
            with self.subTest(length=len(damaged)):
                with self.assertRaises((ValueError, RuntimeError, AssertionError, zlib.error)):
                    D.metadata(damaged, before, saved["epoch_ms"])

    def test_exact_rotation_and_complete_inventory(self):
        def rotation(value):
            disk = value["world_save"]["saves"][1]["after"]["state"]["disk"]
            raw = bytes.fromhex(M.file_entry(disk, D.LEVEL)["hex"])
            M.replace_file(disk, D.OLD_LEVEL, raw + b"x")
        self.reject_original(rotation)
        def omitted(value):
            disk = value["world_save"]["saves"][0]["after"]["state"]["disk"]
            disk["entries"] = [row for row in disk["entries"] if row["path"] != D.LEVEL]
            M.recount(disk)
        self.reject_original(omitted)

    def test_all_saved_arrays_derive_from_live_owners(self):
        for name in M.NBT_ARRAYS:
            with self.subTest(array=name):
                def edit(value):
                    disk = value["world_save"]["saves"][0]["after"]["state"]["disk"]
                    raw = bytes.fromhex(M.file_entry(disk, D.P.REGION)["hex"])
                    M.replace_file(disk, D.P.REGION, M.rewrite_record(
                        raw, plain_edit=lambda plain: M.change_named_array(plain, name)))
                self.reject_original(edit)

    def test_random_continuations_and_aliases(self):
        def random(value):
            state = value["world_save"]["initial"]["state"]["numerical"]
            record = state["provider"]["generator_random"]
            record["next_longs"][0] = "0000000000000000"
        # The complete original state must still equal the held population tail.
        self.reject_original(random)
        def alias(value):
            value["world_save"]["initial"]["state"]["numerical"]["provider"]["generator_random"] = None
        self.reject_original(alias)

    def test_source_pins_and_expected_records(self):
        for mode in ("source", "projection", "archive"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                package = copy.deepcopy(self.package)
                if mode == "source":
                    first = next(iter(package["provenance"]["sources"].values()))
                    first["sha256"] = "0" * 64
                elif mode == "projection":
                    expected = D.C.PUBLIC.strict_json(D.C.PUBLIC.unpack(package["sides"]["client"]["expected"]))
                    expected["records"].pop()
                    package["sides"]["client"]["expected"] = D.C.PUBLIC.packed(F._json_bytes(expected, F.MAX_RECORD))
                else:
                    package["jar_hashes"]["client"] = "0" * 64
                path = Path(directory) / "damaged.json"
                path.write_text(json.dumps(package), encoding="utf-8")
                with self.assertRaises((ValueError, RuntimeError, AssertionError)):
                    F.load_package(path, D)


if __name__ == "__main__":
    unittest.main()
