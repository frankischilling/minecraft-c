"""Corruption controls for original-derived population save/readback evidence."""

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
    "population_persistence_comparison", ROOT / "scripts/reference/world/population/compare_persistence.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing population persistence comparison")
DRIVER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = DRIVER
SPEC.loader.exec_module(DRIVER)
FIXTURES = DRIVER.load("population_persistence_fixture_controls",
                       "scripts/reference/world/population/persistence_fixtures.py")
NBT_ARRAYS = {"Blocks": "blocks", "Data": "metadata", "SkyLight": "skylight",
              "BlockLight": "block_light", "HeightMap": "heightmap"}


def flip_hex(value):
    """Change the final byte while preserving canonical length and hex syntax."""
    return value[:-2] + format(int(value[-2:], 16) ^ 1, "02x")


def file_entry(inventory, path):
    return next(row for row in inventory["entries"] if row["path"] == path)


def recount(inventory):
    inventory["files"] = sum(row["kind"] == "file" for row in inventory["entries"])
    inventory["directories"] = sum(row["kind"] == "directory" for row in inventory["entries"])
    inventory["bytes"] = sum(row.get("bytes", 0) for row in inventory["entries"])


def replace_file(inventory, path, raw):
    file_entry(inventory, path).update(bytes=len(raw), hex=raw.hex(),
                                       sha256=hashlib.sha256(raw).hexdigest())
    recount(inventory)


def rewrite_record(raw, *, plain_edit=None, stream_edit=None):
    """Retain the real region layout and rewrite only occupied slot zero."""
    result = bytearray(raw)
    location = struct.unpack_from(">I", result, 0)[0]
    start, capacity = (location >> 8) * 4096, (location & 255) * 4096
    length = struct.unpack_from(">I", result, start)[0]
    kind = result[start + 4]
    if kind not in (1, 2):
        raise AssertionError("fixture record must use a source-supported compression method")
    payload = bytes(result[start + 5:start + 4 + length])
    if plain_edit is not None:
        plain = zlib.decompress(payload, 31 if kind == 1 else 15)
        plain = plain_edit(plain)
        encoder = zlib.compressobj(wbits=31 if kind == 1 else 15)
        payload = encoder.compress(plain) + encoder.flush()
    if stream_edit is not None:
        payload = stream_edit(payload)
    record = struct.pack(">I", len(payload) + 1) + bytes((kind,)) + payload
    if len(record) > capacity:
        raise AssertionError("bounded mutation must fit the existing source allocation")
    result[start:start + capacity] = record + bytes(capacity - len(record))
    return bytes(result)


def change_named_array(plain, name):
    encoded = name.encode("ascii")
    prefix = b"\x07" + struct.pack(">H", len(encoded)) + encoded
    prefix += struct.pack(">I", DRIVER.ARRAYS[NBT_ARRAYS[name]])
    if plain.count(prefix) != 1:
        raise AssertionError("one complete named source byte array is required")
    offset = plain.index(prefix) + len(prefix)
    result = bytearray(plain)
    result[offset + DRIVER.ARRAYS[NBT_ARRAYS[name]] - 1] ^= 1
    return bytes(result)


def change_empty_list(plain, name):
    encoded = name.encode("ascii")
    prefix = b"\x09" + struct.pack(">H", len(encoded)) + encoded
    needle = prefix + b"\x01\x00\x00\x00\x00"
    if plain.count(needle) != 1:
        raise AssertionError("one genuine BYTE-typed empty source list is required")
    offset = plain.index(needle) + len(prefix)
    return plain[:offset] + b"\x0a" + plain[offset + 1:]


class PersistenceEvidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = ROOT / "tests/reference/world/population/persistence_fixtures.json"
        cls.package = DRIVER.C.PUBLIC.strict_json(cls.path.read_bytes())
        # Decode and independently rederive both sides once, never from native output.
        cls.cases = FIXTURES.load_package(cls.path, DRIVER)
        cls.original = cls.cases["client"]["original"]
        cls.jar = cls.cases["client"]["jar"]
        cls.saved_disk = cls.original["persistence"]["cache_saves"][0]["after"]["disk"]
        cls.interval = cls.original["persistence"]["cache_saves"][0]["epoch_ms"]
        cls.logical = DRIVER.C.PUBLIC.strict_json(
            DRIVER.C.PUBLIC.unpack(cls.package["sides"]["client"]["original"]))

    def reject_original(self, mutate, message=None):
        value = copy.deepcopy(self.original)
        mutate(value)
        failures = (ValueError, DRIVER.C.PUBLIC.driver.Failure)
        assertion = self.assertRaisesRegex(failures, message) if message else self.assertRaises(failures)
        with assertion:
            DRIVER.project_original(value, "client", self.jar)

    def reject_package(self, mutate, message=None):
        value = copy.deepcopy(self.package)
        mutate(value)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fixture.json"
            # Use ordinary JSON here so the loader, rather than the writer's
            # public-value precheck, is responsible for rejecting the record.
            path.write_text(json.dumps(value, ensure_ascii=True), encoding="ascii")
            assertion = self.assertRaisesRegex(ValueError, message) if message else self.assertRaises(ValueError)
            with assertion:
                FIXTURES.load_package(path, DRIVER)

    def reject_logical(self, mutate, message=None):
        original = copy.deepcopy(self.logical)
        mutate(original)
        packed = DRIVER.C.PUBLIC.packed(json.dumps(original, separators=(",", ":")).encode("ascii"))
        self.reject_package(lambda value: value["sides"]["client"].update(original=packed), message)

    @staticmethod
    def install_saved_region(value, raw):
        # Every post-save copy must agree, or a continuity check would hide a
        # missing semantic check in the saved NBT itself.
        for boundary in DRIVER.all_boundaries(value["persistence"])[4:]:
            replace_file(boundary["disk"], DRIVER.REGION, raw)

    def test_complete_original_only_fixture(self):
        self.assertEqual(set(self.cases), {"client", "server"})
        for side, case in self.cases.items():
            with self.subTest(side=side):
                self.assertEqual(case["jar"], FIXTURES.synthetic_jar(side))
                self.assertEqual(len(case["expected"]["boundaries"]), 15)
                self.assertEqual(len(case["expected"]["direct_loads"]), 4)
                self.assertEqual([row["slot"] for row in case["expected"]["records"]], [0, 1, 32, 33])
                self.assertEqual(case["expected"]["block_map"]["length"], 256)
                self.assertTrue(all(case["original"]["persistence"][key] is False
                                    for key in ("full_world_save", "unload", "world_restart")))

    def test_complete_profile_and_call_schedule(self):
        mutations = {
            "profile": lambda v: v.update(profile="origin"),
            "scope": lambda v: v.update(scope="original-game-normal-population"),
            "schema_type": lambda v: v.update(schema=True),
            "population_truncated": lambda v: v["population"]["callbacks"]["operations"].pop(),
            "population_coordinate": lambda v: v["population"]["callbacks"]["operations"][0].update(coordinates=[0, 1]),
            "save_truncated": lambda v: v["persistence"]["cache_saves"].pop(),
            "save_order": lambda v: v["persistence"]["cache_saves"].reverse(),
            "save_not_forced": lambda v: v["persistence"]["cache_saves"][0].update(force=False),
            "save_incomplete": lambda v: v["persistence"]["cache_saves"][0].update(completed=False),
            "load_truncated": lambda v: v["persistence"]["direct_loads"].pop(),
            "load_order": lambda v: v["persistence"]["direct_loads"].reverse(),
            "load_coordinate": lambda v: v["persistence"]["direct_loads"][3].update(coordinates=[-1, 1]),
            "full_world_claim": lambda v: v["persistence"].update(full_world_save=True),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_original(mutate)
        with self.assertRaises(ValueError):
            DRIVER.project_original(self.original["population"], "client", self.jar)

    def test_every_complete_readback_array(self):
        for role in DRIVER.ARRAYS:
            def mutate(value, field=role):
                array = value["persistence"]["direct_loads"][3]["chunk"]["data"][field]
                array["values"] = flip_hex(array["values"])
            with self.subTest(role=role):
                self.reject_original(mutate, "actual normalized direct-load Chunk 3")

    def test_every_saved_array_and_typed_empty_list(self):
        raw = bytes.fromhex(file_entry(self.saved_disk, DRIVER.REGION)["hex"])
        for name in NBT_ARRAYS:
            changed = rewrite_record(raw, plain_edit=lambda plain, field=name: change_named_array(plain, field))
            with self.subTest(array=name):
                self.reject_original(lambda value, data=changed: self.install_saved_region(value, data),
                                     "complete saved " + name)
        for name in ("Entities", "TileEntities"):
            changed = rewrite_record(raw, plain_edit=lambda plain, field=name: change_empty_list(plain, field))
            with self.subTest(empty_list=name):
                self.reject_original(lambda value, data=changed: self.install_saved_region(value, data),
                                     "actual empty on-disk " + name)

    def test_transient_and_persisted_chunk_flags(self):
        for flag in ("c", "n", "o", "p", "q"):
            def saved(value, field=flag):
                cache = value["persistence"]["cache_saves"][0]["after"]["cache"]
                for chunk in (cache["cached_locals"][0], cache["world"]["chunks"][1]):
                    chunk["flags"][field] = not chunk["flags"][field]
            def loaded(value, field=flag):
                flags = value["persistence"]["direct_loads"][0]["chunk"]["flags"]
                flags[field] = not flags[field]
            with self.subTest(saved_flag=flag):
                self.reject_original(saved, "complete first-save effects")
            with self.subTest(loaded_flag=flag):
                self.reject_original(loaded, "actual normalized direct-load Chunk 0")

    def test_context_backing_and_fresh_load_owner_continuity(self):
        def backing(value):
            cache = value["persistence"]["direct_loads"][-1]["after"]["cache"]
            for chunk in (cache["cached_locals"][0], cache["world"]["chunks"][1]):
                chunk["array_references"][0]["ref"] = "r999999"
        def reused_result(value):
            row = value["persistence"]["direct_loads"][0]
            row["result"] = copy.deepcopy(value["persistence"]["initial"]["cache"]["cached_locals"][0]["object"])
        def reused_loaded_backing(value):
            row = value["persistence"]["direct_loads"][1]["chunk"]
            row["array_references"][0] = copy.deepcopy(value["persistence"]["direct_loads"][0]["chunk"]["array_references"][0])
        mutations = {
            "held_world": lambda v: v["persistence"]["initial"]["cache"]["world"]["world"].update(ref="r999999"),
            "cache_collection": lambda v: v["persistence"]["initial"]["cache"]["world"]["map"]["object"].update(ref="r999999"),
            "cached_backing": backing,
            "load_world": lambda v: v["persistence"]["direct_loads"][0]["world"].update(ref="r999999"),
            "cached_result": reused_result,
            "reused_loaded_backing": reused_loaded_backing,
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_original(mutate)

    def test_all_info_fields_and_owner_remain_observed(self):
        def field(value, role, replacement):
            info = value["persistence"]["cache_saves"][0]["after"]["info"]
            next(row for row in info["fields"] if row["role"] == role)["value"] = replacement
        mutations = {
            "owner": lambda v: v["persistence"]["cache_saves"][0]["after"]["info"]["owner"].update(ref="r999999"),
            "time": lambda v: field(v, "time", "00000000000004d3"),
            "size": lambda v: field(v, "size_on_disk", "0000000000005000"),
            "version": lambda v: field(v, "version", 19132),
            "version_type": lambda v: field(v, "version", True),
            "omitted_field": lambda v: v["persistence"]["initial"]["info"]["fields"].pop(),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_original(mutate)

    def test_random_values_and_aliases_cannot_change_during_persistence(self):
        def numerical(value):
            return value["persistence"]["direct_loads"][-1]["after"]["numerical"]
        mutations = {
            "generator_seed": lambda v: numerical(v)["provider"]["generator_random"].update(seed48="000000000000"),
            "cave_continuation": lambda v: numerical(v)["provider"]["cave_random"]["next_longs"].pop(),
            "world_gaussian": lambda v: numerical(v)["aux"]["world_random"].update(have_next_next_gaussian=True),
            "world_owner": lambda v: numerical(v)["aux"]["world_random_identity"].update(ref="r999999"),
            "provider_alias": lambda v: numerical(v)["provider"]["identities"].update(
                w=copy.deepcopy(numerical(v)["provider"]["identities"]["q"])),
            "manager_alias": lambda v: numerical(v)["aux"]["manager"]["identities"].update(
                b=copy.deepcopy(numerical(v)["aux"]["manager"]["identities"]["a"])),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_original(mutate, "original numerical state retained boundary 14")

    def test_captured_world_random_continuations_and_native_form(self):
        for side, case in self.cases.items():
            captured = case["original"]["population"]["states"][0]["aux"]["world_random"]
            native_form = copy.deepcopy(captured)
            native_form["state"] = native_form.pop("seed48")
            with self.subTest(side=side):
                self.assertEqual(DRIVER.copied_random(captured, original=True), captured)
                self.assertEqual(DRIVER.copied_random(native_form, original=False), captured)
                native_form["next_longs"][-1] = flip_hex(native_form["next_longs"][-1])
                with self.assertRaisesRegex(ValueError, "copied nextLong continuation derives from actual captured seed"):
                    DRIVER.copied_random(native_form, original=False)

    def test_coherent_world_random_copy_corruption_cannot_override_its_seed(self):
        def mutate(value):
            states = [*value["population"]["states"],
                      *[boundary["numerical"] for boundary in DRIVER.all_boundaries(value["persistence"])]]
            self.assertEqual(len(states), 20)
            continuation = list(states[0]["aux"]["world_random"]["next_longs"])
            continuation[-1] = flip_hex(continuation[-1])
            for state in states:
                state["aux"]["world_random"]["next_longs"] = list(continuation)
        self.reject_original(mutate, "copied nextLong continuation derives from actual captured seed")

    def test_complete_late_normalization_map(self):
        def map_value(value):
            return value["persistence"]["block_map"]
        def changed_live_mapping(value):
            mapping = map_value(value)
            raw = bytearray.fromhex(mapping["values"])
            blocks = bytes.fromhex(value["persistence"]["cache_saves"][0]["before"]["cache"]["cached_locals"][0]["data"]["blocks"]["values"])
            index = next(item for item in blocks if item and raw[item] == item)
            raw[index] = 0
            mapping.update(values=raw.hex(), sha256=hashlib.sha256(raw).hexdigest())
            for row in value["persistence"]["direct_loads"]:
                row["normalization_map"] = copy.deepcopy(mapping)
        mutations = {
            "last_byte": lambda v: map_value(v).update(values=flip_hex(map_value(v)["values"])),
            "digest": lambda v: map_value(v).update(sha256="0" * 64),
            "truncated": lambda v: map_value(v).update(values=map_value(v)["values"][:-2]),
            "owner": lambda v: map_value(v)["owner"].update(ref=None, **{"class": None}),
            "load_owner": lambda v: v["persistence"]["direct_loads"][-1]["normalization_map"]["owner"].update(ref="r999999"),
            "live_bytes_with_valid_digest": changed_live_mapping,
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_original(mutate)

    def test_raw_disk_transport_is_strict(self):
        def region(value):
            return file_entry(value, DRIVER.REGION)
        mutations = {
            "incomplete": lambda v: v.update(complete=False),
            "kind": lambda v: region(v).update(kind="symlink"),
            "file_count": lambda v: v.update(files=v["files"] - 1),
            "directory_count_type": lambda v: v.update(directories=True),
            "byte_count": lambda v: v.update(bytes=v["bytes"] + 1),
            "file_size_type": lambda v: region(v).update(bytes=True),
            "file_size": lambda v: region(v).update(bytes=region(v)["bytes"] - 1),
            "digest": lambda v: region(v).update(sha256="0" * 64),
            "hex_truncated": lambda v: region(v).update(hex=region(v)["hex"][:-2]),
            "hex_uppercase": lambda v: region(v).update(hex="A" + region(v)["hex"][1:]),
            "hex_nonhex": lambda v: region(v).update(hex="g" + region(v)["hex"][1:]),
            "parent_traversal": lambda v: region(v).update(path="world/../r.0.0.mcr"),
            "absolute_path": lambda v: region(v).update(path="/world/r.0.0.mcr"),
            "windows_path": lambda v: region(v).update(path="C:\\world\\r.0.0.mcr"),
            "duplicate_path": lambda v: v["entries"].append(copy.deepcopy(v["entries"][-1])),
            "missing_root": lambda v: v["entries"].pop(0),
            "unsorted": lambda v: v["entries"].reverse(),
        }
        for name, mutate in mutations.items():
            value = copy.deepcopy(self.saved_disk)
            mutate(value)
            with self.subTest(name=name), self.assertRaises(ValueError):
                DRIVER.disk(value, original=True)

    def test_complete_file_inventory(self):
        for omitted in (DRIVER.REGION, DRIVER.SESSION):
            value = copy.deepcopy(self.saved_disk)
            value["entries"] = [row for row in value["entries"] if row["path"] != omitted]
            recount(value)
            with self.subTest(omitted=omitted), self.assertRaisesRegex(ValueError, "complete constructor/save file set"):
                DRIVER.region_records(DRIVER.disk(value, original=True), self.interval, saved=True)
        value = copy.deepcopy(self.saved_disk)
        value["entries"].append({"path": "world/unobserved.dat", "kind": "file", "bytes": 1,
                                 "hex": "00", "sha256": hashlib.sha256(b"\0").hexdigest()})
        recount(value)
        with self.assertRaisesRegex(ValueError, "complete constructor/save file set"):
            DRIVER.region_records(DRIVER.disk(value, original=True), self.interval, saved=True)

    def test_clock_inputs_are_measured_not_normalized(self):
        construction = self.original["construction_epoch_ms"]
        for clock in (construction[0] - 1, construction[1] + 1):
            value = copy.deepcopy(self.saved_disk)
            replace_file(value, DRIVER.SESSION, struct.pack(">q", clock))
            with self.subTest(session=clock), self.assertRaisesRegex(ValueError, "constructor clock input"):
                DRIVER.session(DRIVER.disk(value, original=True), construction)
        value = copy.deepcopy(self.saved_disk)
        raw = bytearray.fromhex(file_entry(value, DRIVER.REGION)["hex"])
        struct.pack_into(">I", raw, 4096, self.interval[1] // 1000 + 1)
        replace_file(value, DRIVER.REGION, raw)
        with self.assertRaisesRegex(ValueError, "timestamp in actual save interval"):
            DRIVER.region_records(DRIVER.disk(value, original=True), self.interval, saved=True)
        for interval in ([True, 1], [2, 1], [0], [0, 1 << 63]):
            with self.subTest(interval=interval), self.assertRaises(ValueError):
                DRIVER.epoch(interval)

    def test_compressed_records_are_complete_and_fully_allocated(self):
        raw = bytes.fromhex(file_entry(self.saved_disk, DRIVER.REGION)["hex"])
        changes = {
            "truncated_stream": rewrite_record(raw, stream_edit=lambda stream: stream[:-1]),
            "trailing_stream_bytes": rewrite_record(raw, stream_edit=lambda stream: stream + b"\0"),
        }
        start = (struct.unpack_from(">I", raw, 0)[0] >> 8) * 4096
        for name, offset, byte in (("compression_method", start + 4, 3),
                                   ("sector_padding", start + 4095, 1)):
            changed = bytearray(raw)
            changed[offset] = byte
            changes[name] = bytes(changed)
        changed = bytearray(raw)
        changed[4:8] = changed[:4]
        changes["overlapping_sectors"] = bytes(changed)
        for name, changed in changes.items():
            value = copy.deepcopy(self.saved_disk)
            replace_file(value, DRIVER.REGION, changed)
            with self.subTest(name=name), self.assertRaises(ValueError):
                DRIVER.region_records(DRIVER.disk(value, original=True), self.interval, saved=True)

    def test_package_profile_sources_and_archive_pins(self):
        source = FIXTURES.SOURCES[-1]
        mutations = {
            "profile": lambda v: v.update(profile="origin"),
            "schema_type": lambda v: v.update(schema=True),
            "archive": lambda v: v["sides"]["client"].update(jar_sha256="0" * 64),
            "missing_side": lambda v: v["sides"].pop("server"),
            "source_digest": lambda v: v["provenance"]["sources"][source].update(sha256="0" * 64),
            "source_size_type": lambda v: v["provenance"]["sources"][source].update(bytes=True),
            "source_omitted": lambda v: v["provenance"]["sources"].pop(source),
            "jdk_profile": lambda v: v["provenance"].update(jdk_profile="Java21"),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_package(mutate)

    def test_packed_original_transport(self):
        def packed(value):
            return value["sides"]["client"]["original"]
        def compressed(value, change):
            record = packed(value)
            raw = base64.b64decode(record["zlib_base64"], validate=True)
            record["zlib_base64"] = base64.b64encode(change(raw)).decode("ascii")
        mutations = {
            "digest": lambda v: packed(v).update(sha256="0" * 64),
            "count": lambda v: packed(v).update(bytes=packed(v)["bytes"] - 1),
            "count_type": lambda v: packed(v).update(bytes=True),
            "base64": lambda v: packed(v).update(zlib_base64="!"),
            "truncated": lambda v: compressed(v, lambda raw: raw[:-1]),
            "trailing": lambda v: compressed(v, lambda raw: raw + b"\0"),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_package(mutate)

    def test_dirty_capture_cannot_claim_an_exact_commit(self):
        mutations = {
            "direct": lambda v: v["provenance"].update(capture={"source_dirty": True, "tested_commit": "0" * 40}),
            "nested": lambda v: v["provenance"].update(capture={"dirty": True, "attribution": {"source_commit": "0" * 40}}),
            "list_override": lambda v: v["provenance"].update(capture={"dirty": True, "attribution": [
                {"source_dirty": False, "committed_source": "0" * 40}]}),
            "dirty_type": lambda v: v["provenance"].update(capture={"source_dirty": 1}),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.reject_package(mutate)

    def test_both_origin_maps_are_logical_and_host_paths_are_rejected(self):
        for path in (("population", "callbacks", "class_origins"), ("persistence", "class_origins")):
            def mutate(value, keys=path):
                origins = value
                for key in keys:
                    origins = origins[key]
                origins[next(iter(origins))] = "supplied:server"
            with self.subTest(origin_map=path):
                self.reject_logical(mutate)
        for host_path in ("file:/private/reference.jar", "C:\\private\\capture", "/private/capture"):
            with self.subTest(host_path=host_path):
                self.reject_package(lambda value, path=host_path: value["provenance"]["capture"].update(private_path=path))

    def test_expected_is_rederived_from_complete_original_observation(self):
        value = copy.deepcopy(self.cases["client"]["expected"])
        value["direct_loads"][-1]["data"]["heightmap"]["values"] = flip_hex(
            value["direct_loads"][-1]["data"]["heightmap"]["values"])
        packed = DRIVER.C.PUBLIC.packed(json.dumps(value, separators=(",", ":")).encode("ascii"))
        self.reject_package(lambda package: package["sides"]["client"].update(expected=packed), "stored projection")


if __name__ == "__main__":
    unittest.main()
