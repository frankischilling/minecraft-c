"""Corruption controls for original-derived named MapData observations."""

import base64
import copy
import hashlib
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
    "population_named_data_controls", ROOT / "scripts/reference/world/population/compare_named_data.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing named MapData comparison")
D = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = D
SPEC.loader.exec_module(D)
A, P, W = D.A, D.P, D.W
F = P.load("population_named_data_fixture_controls", "scripts/reference/world/population/named_data_fixtures.py")
M = P.load("population_named_data_mutations", "tests/tools/world/test_population_persistence.py")
FAILURES = (ValueError, D.C.PUBLIC.driver.Failure)
NBT_FAILURES = (ValueError, P.STORAGE.Failure, P.STORAGE.level.Failure, zlib.error)


def tag_offset(plain, name, kind):
    encoded = name.encode("ascii")
    prefix = bytes((kind,)) + struct.pack(">H", len(encoded)) + encoded
    if plain.count(prefix) != 1:
        raise AssertionError("one complete source NBT tag is required: " + name)
    return plain.index(prefix), len(prefix)


def change_scalar(plain, name, kind, size):
    start, header = tag_offset(plain, name, kind)
    changed = bytearray(plain)
    changed[start + header + size - 1] ^= 1
    return bytes(changed)


def reference(identity, kind):
    return {"ref": identity, "class": kind}


class NamedDataEvidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = ROOT / "tests/reference/world/population/named_data_fixtures.json"
        cls.package = D.C.PUBLIC.strict_json(cls.path.read_bytes())
        # Decode both lossless original documents and derive their expectations once.
        cls.cases = F.load_package(cls.path, D)
        cls.original = cls.cases["client"]["original"]
        cls.expected = cls.cases["client"]["expected"]
        cls.jar = cls.cases["client"]["jar"]
        cls.rows = A.all_boundaries(cls.original["named_data"])
        cls.saved_disk = cls.rows[10]["state"]["disk"]
        cls.interval = cls.original["named_data"]["saves"][0]["epoch_ms"]
        cls.current_map = cls.rows[9]["map_data"][0]
        cls.named_raw = bytes.fromhex(M.file_entry(cls.saved_disk, A.NAMED_FILE)["hex"])
        cls.logical = D.C.PUBLIC.strict_json(D.C.PUBLIC.unpack(cls.package["sides"]["client"]["original"], F.MAX_RECORD))

    def reject_original(self, edit, message=None):
        value = copy.deepcopy(self.original)
        edit(value)
        assertion = self.assertRaisesRegex(FAILURES, message) if message else self.assertRaises(FAILURES)
        with assertion:
            D.project_original(value, "client", self.jar)

    def reject_package(self, edit, message=None):
        value = copy.deepcopy(self.package)
        edit(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.json"
            path.write_text(json.dumps(value, ensure_ascii=True, allow_nan=False), encoding="ascii")
            assertion = self.assertRaisesRegex(FAILURES, message) if message else self.assertRaises(FAILURES)
            with assertion:
                F.load_package(path, D)

    def reject_logical(self, edit):
        original = copy.deepcopy(self.logical)
        edit(original)
        packed = D.C.PUBLIC.packed(F._json_bytes(original, F.MAX_RECORD))
        self.reject_package(lambda package: package["sides"]["client"].update(original=packed))

    def numerical_states(self):
        return copy.deepcopy(self.original["population"]["states"] +
                             [row["state"]["numerical"] for row in self.rows])

    def test_both_originals_rederive_complete_state(self):
        self.assertEqual(set(self.cases), {"client", "server"})
        for side, case in self.cases.items():
            with self.subTest(side=side):
                expected = D.project_original(case["original"], side, case["jar"])
                self.assertEqual(expected, case["expected"])
                self.assertEqual(case["jar"], F.synthetic_jar(side))
                self.assertEqual(len(expected["boundaries"]), 19)
                self.assertEqual(len(case["original"]["population"]["states"]), 5)
                self.assertEqual(expected["summary"]["numerical_states"], 24)
                self.assertEqual(expected["summary"]["copied_random_records"], 72)
                self.assertEqual(expected["summary"]["array_descriptors"], 360)
                self.assertEqual([row["slot"] for row in expected["records"]], [0, 1, 32, 33])
                self.assertEqual(expected["named_record"]["tags"], 7)
                self.assertEqual(expected["named_record"]["colors_bytes"], 16384)
                self.assertEqual([row["size_on_disk"] for row in expected["metadata"]], [0, 24576])
                self.assertFalse(case["original"]["native_comparison"])
                self.assertIsNone(case["original"]["tested_commit"])

    def test_profile_schedule_and_source_bindings(self):
        def swap_read_events(value):
            saved = value["named_data"]
            saved["reader_load"], saved["reader_cache_hit"] = saved["reader_cache_hit"], saved["reader_load"]
        mutations = {
            "profile": lambda value: value.update(profile="full-world-save"),
            "typed_schema": lambda value: value.update(schema=True),
            "missing_event": lambda value: value["named_data"].pop("dirty_mark"),
            "save_order": lambda value: value["named_data"]["saves"].reverse(),
            "read_order": swap_read_events,
            "origin_substitution": lambda value: value.update(population=copy.deepcopy(self.cases["server"]["original"]["population"])),
            "dirty_descriptor": lambda value: value["named_data"]["bindings"]["value_mark_dirty"].update(descriptor="(Z)V"),
            "name_field_owner": lambda value: value["named_data"]["field_bindings"]["value_name"].update(**{"class": "iu"}),
        }
        for name, edit in mutations.items():
            with self.subTest(name=name):
                self.reject_original(edit)

    def test_global_live_owner_retention(self):
        def world_manager(value):
            A.all_boundaries(value["named_data"])[18]["collections"]["saved_data"]["object"]["ref"] = "r999900"
        def colors_replaced_by_save(value):
            for row in A.all_boundaries(value["named_data"])[10:]:
                row["map_data"][0]["colors"]["object"]["ref"] = "r999901"
        def shared_reader_manager(value):
            saved = value["named_data"]
            owner = saved["initial"]["collections"]["saved_data"]["object"]
            saved["reader_constructor"]["result"] = copy.deepcopy(owner)
            for row in A.all_boundaries(saved)[14:]:
                row["collections"]["reader_manager"]["object"] = copy.deepcopy(owner)
            for name in ("reader_load", "reader_cache_hit"):
                saved[name]["receiver"] = copy.deepcopy(owner)
        def shared_reader_colors(value):
            for row in A.all_boundaries(value["named_data"])[16:]:
                row["map_data"][1]["colors"]["object"] = copy.deepcopy(row["map_data"][0]["colors"]["object"])
        def player_list_alias(value):
            saved = value["named_data"]
            owner = saved["initial"]["collections"]["players"]["object"]
            for row in A.all_boundaries(saved)[8:]:
                row["collections"]["saved_data"]["ordered_list"]["object"] = copy.deepcopy(owner)
        for name, edit in {"world_manager": world_manager, "save_replaces_colors": colors_replaced_by_save,
                           "reader_is_world_manager": shared_reader_manager, "readback_colors_alias": shared_reader_colors,
                           "ordered_is_player_list": player_list_alias}.items():
            with self.subTest(name=name):
                self.reject_original(edit)

    def test_dirty_changes_belong_to_the_actual_operations(self):
        sequence = [self.rows[index]["map_data"][0]["dirty"] for index in (4, 6, 8, 10, 12)]
        self.assertEqual(sequence, [False, True, True, False, False])
        for name, start, finish, replacement in (("constructor_dirty", 4, 6, True),
                                                 ("mark_no_effect", 6, 10, False),
                                                 ("save_does_not_clear", 10, 19, True)):
            def edit(value, low=start, high=finish, dirty=replacement):
                # Keep adjacent snapshots coherent so the dirty rule itself is exercised.
                for row in A.all_boundaries(value["named_data"])[low:high]:
                    row["map_data"][0]["dirty"] = dirty
            with self.subTest(name=name):
                self.reject_original(edit, "dirty cell")

    def test_manager_contents_occurrences_and_backings(self):
        source = self.rows[8]["collections"]["saved_data"]
        raw = self.rows[8]["state"]["cache"]
        key = self.original["named_data"]["put"]["key"]
        mapped = self.original["named_data"]["map_constructor"]["result"]
        mutations = {
            "named_missing": lambda value: value["named_cache"].update(size=0, entries=[]),
            "ordered_duplicate": lambda value: value["ordered_list"].update(size=2, entries=[
                {"index": 0, "value": mapped}, {"index": 1, "value": mapped}]),
            "ordered_wrong_value": lambda value: value["ordered_list"]["entries"][0]["value"].update(ref="r999902"),
            "key_identity": lambda value: value["named_cache"]["entries"][0]["key"].update(ref="r999903"),
            "independent_id_map": lambda value: value["id_map"].update(size=1, entries=[None]),
            "wrong_storage": lambda value: value["storage"].update(ref="r999904"),
            "map_backing_alias": lambda value: value["named_cache"].update(object=copy.deepcopy(value["id_map"]["object"])),
        }
        A.manager(source, "client", raw, key, mapped)
        for name, edit in mutations.items():
            value = copy.deepcopy(source)
            edit(value)
            with self.subTest(name=name), self.assertRaises(ValueError):
                A.manager(value, "client", raw, key, mapped)

    def test_complete_map_fields_and_colors(self):
        source = self.rows[4]["map_data"][0]
        supplied_name = self.original["named_data"]["map_constructor"]["name"]
        mutations = {name: (lambda value, field=name: value.update({field: 1}))
                     for name in ("x_center", "z_center", "dimension", "scale", "update_counter")}
        mutations.update({
            "dirty_type": lambda value: value.update(dirty=0),
            "name_owner": lambda value: value["name"]["object"].update(ref="r999905"),
            "colors_digest": lambda value: value["colors"].update(sha256="0" * 64),
            "colors_length": lambda value: value["colors"].update(bytes=16383),
            "colors_class": lambda value: value["colors"]["object"].update(**{"class": "[D"}),
            "tracker_alias": lambda value: value["trackers"].update(object=copy.deepcopy(value["decorations"]["object"])),
            "tracker_entry": lambda value: value["tracker_cache"].update(size=1, entries=[None]),
        })
        for name, edit in mutations.items():
            value = copy.deepcopy(source)
            edit(value)
            with self.subTest(name=name), self.assertRaises(ValueError):
                A.map_data(value, "client", supplied_name, False)
        for offset in (0, 16383):
            value = copy.deepcopy(source)
            raw = bytearray.fromhex(value["colors"]["hex"])
            raw[offset] ^= 1
            value["colors"].update(hex=raw.hex(), sha256=hashlib.sha256(raw).hexdigest())
            with self.subTest(changed_color=offset), self.assertRaisesRegex(ValueError, "all default colors zero"):
                A.map_data(value, "client", supplied_name, False)

    def test_name_lookup_key_and_filename_stay_distinct(self):
        self.reject_original(lambda value: value["named_data"]["put"].update(
            key=copy.deepcopy(value["named_data"]["map_constructor"]["name"])))
        def wrong_filename(value):
            for row in A.all_boundaries(value["named_data"])[10:]:
                disk = row["state"]["disk"]
                M.file_entry(disk, A.NAMED_FILE)["path"] = "world/data/" + A.LOOKUP + ".dat"
                disk["entries"].sort(key=lambda entry: entry["path"])
        self.reject_original(wrong_filename, "complete exact file set")

    def test_named_seven_typed_fields_bind_to_current_value(self):
        plain = zlib.decompress(self.named_raw, 31)
        for name, kind, size in (("dimension", 1, 1), ("xCenter", 3, 4), ("zCenter", 3, 4),
                                 ("scale", 1, 1), ("width", 2, 2), ("height", 2, 2)):
            changed = change_scalar(plain, name, kind, size)
            with self.subTest(field=name), self.assertRaisesRegex(ValueError, "exact seven typed MapData fields"):
                A.named_nbt(zlib.compress(changed, wbits=31), self.current_map)
        start, header = tag_offset(plain, "colors", 7)
        changed = bytearray(plain)
        changed[start + header + 4 + 16383] ^= 1
        with self.assertRaisesRegex(ValueError, "exact seven typed MapData fields"):
            A.named_nbt(zlib.compress(bytes(changed), wbits=31), self.current_map)
        for name, old_kind, new_kind, extra in (("dimension", 1, 2, 1), ("width", 2, 3, 2)):
            start, header = tag_offset(plain, name, old_kind)
            changed = plain[:start] + bytes((new_kind,)) + plain[start + 1:start + header] + bytes(extra) + plain[start + header:]
            with self.subTest(wrong_type=name), self.assertRaisesRegex(ValueError, "exact seven typed MapData fields"):
                A.named_nbt(zlib.compress(changed, wbits=31), self.current_map)
        extra_tag = b"\x01\x00\x05extra\x00"
        self.assertEqual(plain[-2:], b"\x00\x00")
        with self.assertRaisesRegex(ValueError, "exact seven typed MapData fields"):
            A.named_nbt(zlib.compress(plain[:-2] + extra_tag + plain[-2:], wbits=31), self.current_map)

    def test_named_stream_is_complete_and_root_is_case_sensitive(self):
        for damaged in (self.named_raw[:-1], self.named_raw + b"x", self.named_raw + self.named_raw):
            with self.subTest(stream_bytes=len(damaged)), self.assertRaises(NBT_FAILURES):
                A.named_nbt(damaged, self.current_map)
        plain = zlib.decompress(self.named_raw, 31)
        for changed in (plain + b"\x00", plain.replace(b"\x0a\x00\x04data", b"\x0a\x00\x04Data", 1)):
            with self.subTest(plain_bytes=len(changed)), self.assertRaises(NBT_FAILURES):
                A.named_nbt(zlib.compress(changed, wbits=31), self.current_map)

    def test_all_saved_chunk_arrays_and_empty_list_types(self):
        raw = bytes.fromhex(M.file_entry(self.saved_disk, P.REGION)["hex"])
        cached = self.expected["boundaries"][9]["cache"]
        for name in M.NBT_ARRAYS:
            changed = M.rewrite_record(raw, plain_edit=lambda plain, field=name: M.change_named_array(plain, field))
            disk = copy.deepcopy(self.saved_disk)
            M.replace_file(disk, P.REGION, changed)
            with self.subTest(array=name), self.assertRaisesRegex(ValueError, "complete source-bound saved Chunk field " + name):
                A.saved_chunks(disk, self.interval, cached)
        for name in ("Entities", "TileEntities"):
            changed = M.rewrite_record(raw, plain_edit=lambda plain, field=name: M.change_empty_list(plain, field))
            disk = copy.deepcopy(self.saved_disk)
            M.replace_file(disk, P.REGION, changed)
            with self.subTest(empty_list=name), self.assertRaisesRegex(ValueError, "complete source-bound saved Chunk field " + name):
                A.saved_chunks(disk, self.interval, cached)

    def test_metadata_thirteen_tags_and_clock(self):
        raw = bytes.fromhex(M.file_entry(self.saved_disk, W.LEVEL)["hex"])
        plain = zlib.decompress(raw, 31)
        before = self.expected["boundaries"][9]["info"]
        for name, kind, size in (("RandomSeed", 4, 8), ("SpawnX", 3, 4), ("SpawnY", 3, 4), ("SpawnZ", 3, 4),
                                 ("Time", 4, 8), ("SizeOnDisk", 4, 8), ("version", 3, 4), ("raining", 1, 1),
                                 ("rainTime", 3, 4), ("thundering", 1, 1), ("thunderTime", 3, 4)):
            with self.subTest(field=name), self.assertRaises(ValueError):
                W.metadata(zlib.compress(change_scalar(plain, name, kind, size), wbits=31), before, self.interval)
        start, header = tag_offset(plain, "LevelName", 8)
        size = struct.unpack_from(">H", plain, start + header)[0]
        changed = bytearray(plain)
        changed[start + header + 2 + size - 1] ^= 1
        with self.assertRaises(ValueError):
            W.metadata(zlib.compress(bytes(changed), wbits=31), before, self.interval)
        start, header = tag_offset(plain, "LastPlayed", 4)
        changed = plain[:start + header] + struct.pack(">q", self.interval[1] + 1) + plain[start + header + 8:]
        with self.assertRaisesRegex(ValueError, "clock sample"):
            W.metadata(zlib.compress(changed, wbits=31), before, self.interval)
        for damaged in (raw[:-1], raw + b"x", raw + raw):
            with self.subTest(stream_bytes=len(damaged)), self.assertRaises(NBT_FAILURES):
                W.metadata(damaged, before, self.interval)

    def test_complete_disk_transport_and_measured_session(self):
        mutations = {
            "digest": lambda value: M.file_entry(value, A.NAMED_FILE).update(sha256="0" * 64),
            "truncated_hex": lambda value: M.file_entry(value, A.NAMED_FILE).update(hex="00"),
            "count_type": lambda value: value.update(files=True),
            "unsafe_path": lambda value: M.file_entry(value, A.NAMED_FILE).update(path="world/data/../map.dat"),
            "missing_file": lambda value: value["entries"].remove(M.file_entry(value, A.NAMED_FILE)),
        }
        for name, edit in mutations.items():
            disk = copy.deepcopy(self.saved_disk)
            edit(disk)
            with self.subTest(name=name), self.assertRaises(ValueError):
                P.disk(disk, original=True)
        for clock in (self.original["construction_epoch_ms"][0] - 1, self.original["construction_epoch_ms"][1] + 1):
            disk = copy.deepcopy(self.saved_disk)
            M.replace_file(disk, P.SESSION, struct.pack(">q", clock))
            with self.subTest(session=clock), self.assertRaisesRegex(ValueError, "constructor clock input"):
                P.session(P.disk(disk, original=True), self.original["construction_epoch_ms"])
        disk = copy.deepcopy(self.saved_disk)
        raw = bytearray.fromhex(M.file_entry(disk, P.REGION)["hex"])
        struct.pack_into(">I", raw, 4096, self.interval[1] // 1000 + 1)
        M.replace_file(disk, P.REGION, raw)
        with self.assertRaisesRegex(ValueError, "timestamp in actual save interval"):
            P.region_records(W.filtered_region_disk(disk), self.interval, saved=True)

    def test_complete_inventory_and_exact_metadata_rotation(self):
        def missing_named_file(value):
            rows = A.all_boundaries(value["named_data"])
            for row in rows[10:]:
                disk = row["state"]["disk"]
                disk["entries"].remove(M.file_entry(disk, A.NAMED_FILE))
                M.recount(disk)
            value["named_data"]["limits"]["actual_retained_file_bytes"] = sum(row["state"]["disk"]["bytes"] for row in rows)
        self.reject_original(missing_named_file, "complete exact file set")
        def wrong_rotation(value):
            rows = A.all_boundaries(value["named_data"])
            second = bytes.fromhex(M.file_entry(rows[12]["state"]["disk"], W.LEVEL)["hex"])
            for row in rows[12:]:
                M.replace_file(row["state"]["disk"], W.OLD_LEVEL, second)
            value["named_data"]["limits"]["actual_retained_file_bytes"] = sum(row["state"]["disk"]["bytes"] for row in rows)
        self.reject_original(wrong_rotation, "exact first metadata rotates")

    def test_coherent_copied_world_random_corruption(self):
        captured = self.original["population"]["states"][0]["aux"]["world_random"]
        P.copied_random(captured, original=True)
        states = self.numerical_states()
        for state in states:
            record = state["aux"]["world_random"]
            record["next_longs"][-1] = M.flip_hex(record["next_longs"][-1])
        raw_world = self.original["population"]["callbacks"]["operations"][-1]["after"]["world"]
        with self.assertRaisesRegex(ValueError, "copied nextLong continuation"):
            A.numerical(states, "client", raw_world)
        native_form = {"state": captured["seed48"], **{key: copy.deepcopy(value) for key, value in captured.items() if key != "seed48"}}
        P.copied_random(native_form, original=False)
        native_form["next_longs"][-1] = M.flip_hex(native_form["next_longs"][-1])
        with self.assertRaisesRegex(ValueError, "copied nextLong continuation"):
            P.copied_random(native_form, original=False)

    def test_numerical_aliases_bind_complete_backing_contents(self):
        states = self.numerical_states()
        provider = states[-1]["provider"]
        names = [name for name, value in provider["scratch"].items() if value is not None]
        first, second = next((left, right) for left in names for right in names
                             if provider["scratch"][left] != provider["scratch"][right])
        provider["identities"][first] = copy.deepcopy(provider["identities"][second])
        raw_world = self.original["population"]["callbacks"]["operations"][-1]["after"]["world"]
        with self.assertRaisesRegex(ValueError, "same actual numerical backing"):
            A.numerical(states, "client", raw_world)

    def test_package_source_pins_profile_and_original_payload_bounds(self):
        source = F.SOURCES[-1]
        mutations = {
            "profile": lambda value: value.update(profile="full-world-save"),
            "schema_type": lambda value: value.update(schema=True),
            "archive": lambda value: value["jar_hashes"].update(client="0" * 64),
            "missing_side": lambda value: value["sides"].pop("server"),
            "source_digest": lambda value: value["provenance"]["sources"][source].update(sha256="0" * 64),
            "source_size_type": lambda value: value["provenance"]["sources"][source].update(bytes=True),
            "source_omitted": lambda value: value["provenance"]["sources"].pop(source),
        }
        for name, edit in mutations.items():
            with self.subTest(name=name):
                self.reject_package(edit)
        packed = self.package["sides"]["client"]["original"]
        compressed = base64.b64decode(packed["zlib_base64"], validate=True)
        for name, change in {
            "digest": {"sha256": "0" * 64}, "count_type": {"bytes": True},
            "base64": {"zlib_base64": "!"},
            "truncated": {"zlib_base64": base64.b64encode(compressed[:-1]).decode("ascii")},
            "trailing": {"zlib_base64": base64.b64encode(compressed + b"x").decode("ascii")},
        }.items():
            value = copy.deepcopy(packed)
            value.update(change)
            with self.subTest(packed=name), self.assertRaises(ValueError):
                D.C.PUBLIC.unpack(value, F.MAX_RECORD)

    def test_expected_record_is_rederived_from_original(self):
        expected = copy.deepcopy(self.expected)
        data = P.STORAGE.level.child(expected["named_record"]["tree"], "data")
        colors = P.STORAGE.level.child(data, "colors")
        colors["value"] = M.flip_hex(colors["value"])
        packed = D.C.PUBLIC.packed(F._json_bytes(expected, F.MAX_RECORD))
        self.reject_package(lambda value: value["sides"]["client"].update(expected=packed), "stored projection")

    def test_both_archive_maps_and_public_paths(self):
        for path in (("population", "callbacks", "class_origins"), ("named_data", "class_origins")):
            for origin in ("supplied:server", "file:/private/original.jar"):
                def edit(value, keys=path, replacement=origin):
                    origins = value
                    for key in keys:
                        origins = origins[key]
                    origins[next(iter(origins))] = replacement
                with self.subTest(path=path, origin=origin):
                    self.reject_logical(edit)
        for path in ("file:/private/original.jar", "C:\\private\\capture", "/private/capture", "\\\\host\\share\\capture"):
            with self.subTest(host_path=path), self.assertRaises(ValueError):
                F._public_values({"proof": [{"path": path}]})
        self.reject_package(lambda value: value["provenance"]["capture"].update(private_path="C:\\private\\capture"))

    def test_dirty_provenance_cannot_claim_a_committed_source(self):
        for proof in ({"source_dirty": True, "tested_commit": "0" * 40},
                      {"dirty": True, "child": {"source_commit": "0" * 40}},
                      {"source_dirty": True, "children": [{"source_dirty": False, "committed_source": "0" * 40}]},
                      {"source_dirty": 1}):
            with self.subTest(proof=proof), self.assertRaises(ValueError):
                F._capture_proof(proof)
        F._capture_proof({"source_dirty": True, "baseline": "0" * 40, "tested_commit": None})


class NamedReferenceGraphs(unittest.TestCase):
    @staticmethod
    def manager():
        return {"object": reference("n0", "hc"), "handler_store": reference("n1", "mx"),
                "native_storage_binding": reference("n2", "native.path_binding"),
                "named_cache": {"object": reference("n3", "java.util.HashMap"), "size": 0, "entries": []},
                "ordered_list": {"object": reference("n4", "java.util.ArrayList"), "size": 0, "entries": []},
                "id_map": {"object": reference("n5", "java.util.HashMap"), "size": 0, "entries": []}}

    def test_bijection_preserves_equality_and_inequality_across_calls(self):
        pairs = D.References()
        for native, original in (("n0", "r10"), ("n1", "r11"), ("n0", "r10")):
            pairs.compare(reference(native, "java.util.ArrayList"), reference(original, "java.util.ArrayList"), "successive observation")
        self.assertEqual(len(pairs.summary()), 2)
        for native, original in (("n2", "r10"), ("n0", "r12")):
            with self.subTest(native=native, original=original), self.assertRaises(ValueError):
                pairs.compare(reference(native, "java.util.ArrayList"), reference(original, "java.util.ArrayList"), "later observation")

    def test_reference_class_and_descriptor_are_strict(self):
        for native in (reference("n00", "java.util.ArrayList"), reference("n128", "java.util.ArrayList"),
                       reference("n0", "java.util.HashMap"), {"ref": "n0"}):
            with self.subTest(native=native), self.assertRaises(ValueError):
                D.References().compare(native, reference("r10", "java.util.ArrayList"), "descriptor")
        pairs = D.References()
        pairs.compare(reference("n0", "java.util.ArrayList"), reference("r10", "java.util.ArrayList"), "initial")
        with self.assertRaises(ValueError):
            pairs.compare(reference("n0", "java.util.HashMap"), reference("r10", "java.util.HashMap"), "coherent class replacement")
        with self.assertRaises(ValueError):
            D.References().compare({"size": True}, {"size": 1}, "typed scalar")

    def test_native_private_binding_retention_and_uniqueness(self):
        pairs = D.References()
        manager = self.manager()
        self.assertEqual(pairs.manager(manager, "first")["storage"], manager["handler_store"])
        pairs.manager(copy.deepcopy(manager), "same later owner")
        changed = copy.deepcopy(manager)
        changed["native_storage_binding"]["ref"] = "n6"
        with self.assertRaisesRegex(ValueError, "binding retained"):
            pairs.manager(changed, "replacement")
        second = copy.deepcopy(manager)
        second["object"]["ref"] = "n7"
        with self.assertRaisesRegex(ValueError, "distinct managers"):
            pairs.manager(second, "shared binding")
        second["native_storage_binding"]["ref"] = "n8"
        pairs.manager(second, "distinct actual binding")
        self.assertEqual(pairs.native_bindings, {"n0": "n2", "n7": "n8"})
        self.assertEqual(pairs.summary(), [])

    def test_native_binding_cannot_be_a_semantic_owner_in_either_order(self):
        pairs = D.References()
        pairs.manager(self.manager(), "native binding first")
        with self.assertRaisesRegex(ValueError, "not a native path binding"):
            pairs.compare(reference("n2", "java.util.ArrayList"), reference("r10", "java.util.ArrayList"), "semantic collision")
        pairs = D.References()
        pairs.compare(reference("n2", "java.util.ArrayList"), reference("r10", "java.util.ArrayList"), "semantic first")
        with self.assertRaisesRegex(ValueError, "distinct from every paired semantic owner"):
            pairs.manager(self.manager(), "native collision")

    def test_native_binding_schema_and_self_alias_are_rejected(self):
        mutations = {
            "old_storage_role": lambda value: value.update(storage=value.pop("handler_store")),
            "missing_binding": lambda value: value.pop("native_storage_binding"),
            "wrong_binding_class": lambda value: value["native_storage_binding"].update(**{"class": "mx"}),
            "self_alias": lambda value: value["native_storage_binding"].update(ref="n0"),
        }
        for name, edit in mutations.items():
            value = self.manager()
            edit(value)
            with self.subTest(name=name), self.assertRaises(ValueError):
                D.References().manager(value, "bad native binding")


if __name__ == "__main__":
    unittest.main()
