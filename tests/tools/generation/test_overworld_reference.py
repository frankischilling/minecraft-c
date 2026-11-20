"""Public Overworld-generation protocol guards; synthetic records are not parity data."""

import copy
import hashlib
import importlib.util
from pathlib import Path
import struct
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]


def load(path, name):
    specification = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


OVERWORLD = load("scripts/reference/generation/compare_overworld.py", "overworld_reference_subject")
CHUNK_GUARDS = load("tests/tools/chunk/test_data_reference.py", "overworld_synthetic_chunk_helpers")
_, OLD, FINISH, _ = OVERWORLD.helpers()
MASK = (1 << 48) - 1
SERIAL_PREFIX = ("aced0005737200106a6176612e7574696c2e52616e646f6d363296344bf00a53"
                 "0300035a0014686176654e6578744e657874476175737369616e4400106e6578"
                 "744e657874476175737369616e4a0004736565647870" + "00" * 9)


def doubles(length):
    return {"length": length, "values": "0" * (length * 16)}


def random_record(seed):
    return {"serialized_hex": SERIAL_PREFIX + f"{seed:016x}78", "next_longs": OVERWORLD.next_longs(seed)}


def presence(world, coordinates):
    result = copy.deepcopy(world)
    mappings = {item["key"]: item["value"] for item in world["map"]["entries"]}
    result["presence"] = []
    for dx, dz in ((0, 0), (-1, 0), (1, 0), (0, -1), (0, 1)):
        x, z = FINISH.i32(coordinates[0] + dx), FINISH.i32(coordinates[1] + dz)
        key = FINISH.cache_key(x, z)
        result["presence"].append({"coordinates": [x, z], "key": key, "present": key in mappings,
                                  "contains": key in mappings, "value": mappings.get(key)})
    return result


def synthetic_replay_record():
    world = presence(CHUNK_GUARDS.synthetic_world(), (0, 0))
    return {"seed_hex": "0000000000000017", "setup_world": world,
            "constructor": {"materials": [{"field": name, "top": i, "filler": 255 - i}
                                           for i, name in enumerate("abcdefghijklm")],
                            "manager": {"a": doubles(256), "b": doubles(257), "c": doubles(256),
                                        "d": {"length": 2, "values": "am"}}},
            "calls": [{"after": {"opacity": CHUNK_GUARDS.descriptor(bytes(1024))}}]}


def marker_inventory(marker, index):
    frame = {"ordinal": 0, "class": "OverworldGenerationObservation", "method": marker,
             "signature": "()V", "code_index": 0, "class_loader_id": 5,
             "class_loader_class": "jdk.internal.loader.ClassLoaders$AppClassLoader", "original_frame": False}
    thread = {"ordinal": 0, "id": 1, "name": "main", "object_class": "java.lang.Thread", "status": 1,
              "daemon": False, "measured": True, "suspended": True, "frame_count": 1,
              "original_frame": False, "frames": [frame]}
    return {"marker": marker, "index": index, "suspend_policy": 2, "measured_thread_id": 1,
            "original_loader_id": 4, "thread_limit": 64, "frames_per_thread_limit": 64,
            "daemon_layout": "thread-daemon", "daemon_field": {"owner": "java.lang.Thread", "signature": "Z", "modifiers": 2},
            "holder_field": None, "thread_count": 1, "measured_main_thread": True, "all_threads_suspended": True,
            "all_other_threads_daemon": True, "no_other_original_frames": True, "threads": [thread]}


def synthetic_original():
    ref = CHUNK_GUARDS.reference
    world = CHUNK_GUARDS.synthetic_world()
    world.update(world=ref("r0", "fd"), provider=ref("r1", "rh"), storage=ref("r2", "wy"),
                 cache=ref("r3", "ok"), provider_world=ref("r0", "fd"), manager=ref("r17", "xv"),
                 cache_world=ref("r0", "fd"), generator=ref("r24", "yf"), loader=ref(None, None))
    cached = copy.deepcopy(world["chunks"][0])
    cached.update(object=ref("r20", "lm"), fallback=False, empty=False)
    world["chunks"].append(cached)
    world["map"]["entries"] = [{"ordinal": 0, "key": 0, "value": "r20"}]
    world["insertion_list"]["entries"] = [{"ordinal": 0, "value": "r20"}]
    world = presence(world, (0, 0))
    manager = {name: doubles(256) for name in "abc"}
    manager.update(d=None, identities={name: ref(f"r{400 + n}", "[D") for n, name in enumerate("abc")})
    manager["identities"].update(d=ref(None, None), manager=ref("r17", "xv"))
    noise = {name: [{"offsets": ["0" * 16] * 3, "permutation": list(range(256)) * 2} for _ in range(count)]
             for name, count in zip(("k", "l", "m", "n", "o", "a", "b", "c"), (16, 16, 8, 4, 4, 10, 16, 8))}
    identities = {name: ref(f"r{100 + n}", "[D") for n, name in enumerate(OVERWORLD.SCRATCH)}
    identities.update(v=ref("r200", "[Lkd;"), i=ref("r201", "[[I"),
                      i_rows=[ref(f"r{202 + n}", "[I") for n in range(32)], w=ref(None, None),
                      generator=ref("r30", "yf"), world=ref("r0", "fd"), random=ref("r31", "java.util.Random"),
                      cave=ref("r32", "sq"), cave_random=ref("r33", "java.util.Random"))
    fresh = {"noise": noise, "scratch": {name: doubles(length) if name in "rst" else None
                                       for name, length in OVERWORLD.SCRATCH.items()},
             "biomes": None, "materials": None, "generator_random": random_record(303),
             "cave_random": {"omitted": "unconsumed default-seeded state before public cave reseed"},
             "cave_range": 8, "original_only": {"i": [[0] * 32 for _ in range(32)], "w": None},
             "identities": copy.deepcopy(identities)}
    for name in ("q", "d", "e", "f", "g", "h", "v"):
        fresh["identities"][name] = ref(None, None)
    required = {"fd", "lm", "ok", "xa", "rh", "wt", "wi", "eb", "st", "ei", "cl", "bf", "uu", "yy", "wy",
                "yf", "xv", "kd", "uf", "am", "fv", "sq", "fs", "g", "rb", "ry", "t", "tf", "yj"}
    materials = [{"field": name, "top": 2, "filler": 3} for name in "abcdefghijklm"]
    value = {"schema": 1, "side": "client", "seed_hex": "0000000000000017", "jar_sha256": FINISH.JAR_HASHES["client"],
             "platform_parent": True, "class_origins": {name: "file:/synthetic-original.jar" for name in required},
             "bindings": {"world": "fd", "chunk": "lm", "generator": "yf", "generate": "b", "finish": "c"},
             "controls": {name: True for name in OVERWORLD.CONTROL_KEYS}, "setup_world": world,
             "constructor": {"generator": fresh, "manager": manager, "materials": materials}, "calls": []}
    trace = {"schema": 1, "side": "client", "seed_hex": value["seed_hex"], "jar_sha256": value["jar_sha256"],
             "listen_address": "127.0.0.1", "can_get_method_return_values": True, "can_get_bytecodes": True,
             "child_exit": 0, "constructor_random": {"generator_seed": 303, "cave_seed": 101}, "calls": []}
    signatures = {"generator": ("yf", "b", "(II)Llm;"), "constructor": ("lm", "<init>", "(Lfd;[BII)V"),
                  "base": ("yf", "a", "(II[B[Lkd;[D)V"), "surface": ("yf", "a", "(II[B[Lkd;)V"),
                  "finish": ("lm", "c", "()V")}
    prior = []
    previous_generator, previous_cave = 303, 101
    for index, coordinates in enumerate(OVERWORLD.COORDINATES):
        before = presence(world, coordinates)
        local = {"data": CHUNK_GUARDS.local_data(bytes(32768)), "coordinates": list(coordinates),
                 "flags": {"c": False, "n": False, "o": True, "p": False, "q": False}, "time": "0000000000000000",
                 "map": {"class": "java.util.HashMap", "size": 0}, "entity_slices": [{"class": "java.util.ArrayList", "size": 0}] * 8,
                 "identities": {"roles_retained": [True] * 19, "roles_pairwise_distinct": True, "input_retained": True, "world_retained": True}}
        reseeded = ((coordinates[0] * 341873128712 + coordinates[1] * 132897987541) ^ 25214903917) & MASK
        final_generator, final_cave = reseeded ^ 12345, previous_cave + 1
        generated = {"noise": noise, "scratch": {name: doubles(length) for name, length in OVERWORLD.SCRATCH.items()},
                     "biomes": {"length": 256, "values": "a" * 256}, "materials": "0203" * 256,
                     "generator_random": random_record(final_generator), "cave_random": random_record(final_cave),
                     "cave_range": 8, "original_only": fresh["original_only"], "identities": copy.deepcopy(identities)}
        retained = copy.deepcopy(prior) + [{"index": index, "local": copy.deepcopy(local)}]
        after = {"local": local, "world": presence(world, coordinates), "generator": generated,
                 "manager": copy.deepcopy(manager), "materials": copy.deepcopy(materials), "retained": retained,
                 "opacity": CHUNK_GUARDS.descriptor(bytes(1024)), "opacity_identity_retained": True}
        value["calls"].append({"index": index, "coordinates": list(coordinates), "before_world": before,
                               "prior_retained": copy.deepcopy(prior), "after": after})
        native_id = 10000 + index
        stages = []
        for ordinal, (kind, code) in enumerate(OVERWORLD.STAGES):
            owner, method, signature = signatures[kind]
            stages.append({"ordinal": ordinal, "kind": kind, "code_index": code, "class": owner, "method": method,
                           "signature": signature, "chunk_object_id": None if ordinal < 2 else native_id,
                           "generator_seed": previous_generator if ordinal == 0 else reseeded if ordinal < 10 else final_generator,
                           "cave_seed": previous_cave if ordinal < 12 else final_cave})
        queries = []
        for ordinal in range(1024):
            x, z, direction = ordinal // 64, ordinal // 4 % 16, ordinal % 4
            qx = FINISH.i32(coordinates[0] * 16 + x + (-1, 1, 0, 0)[direction])
            qz = FINISH.i32(coordinates[1] * 16 + z + (0, 0, -1, 1)[direction])
            queries.append({"ordinal": ordinal, "x": qx, "z": qz, "local_height": 0, "result": 0})
        identity = {"thread_id": 1, "thread_name": "main", "world_object_id": 2, "generator_object_id": 3,
                    "chunk_object_id": native_id, "world_class": "fd", "generator_class": "yf", "chunk_class": "lm",
                    "world_loader_id": 4, "generator_loader_id": 4}
        trace["calls"].append({"index": index, "coordinates": list(coordinates), "identity": identity,
                               "begin_marker": True, "end_marker": True, "stages": stages, "direct_call_count": 1024,
                               "ignored_entries": 0, "records": queries,
                               "begin_threads": marker_inventory("begin", index), "end_threads": marker_inventory("end", index)})
        prior = retained
        previous_generator, previous_cave = final_generator, final_cave
    return value, trace


class OverworldReferenceGuards(unittest.TestCase):
    def validate(self, value, trace):
        with patch.object(OLD, "OPACITY_SHA", hashlib.sha256(bytes(1024)).hexdigest()):
            return OVERWORLD.validate_original(value, trace, 23, "client", OLD, FINISH)

    def test_complete_synthetic_calls_allow_presence_origin_changes(self):
        value, trace = synthetic_original()
        self.validate(value, trace)
        projection = OVERWORLD.c_projection(value, trace, FINISH)
        self.assertEqual(len(projection["calls"]), 9)
        self.assertEqual(sum(len(call["trace"]) for call in projection["calls"]), 9216)
        self.assertFalse(projection["calls"][0]["biomes_reused"])
        self.assertTrue(all(call["biomes_reused"] for call in projection["calls"][1:]))

    def test_stage_binding_random_identity_and_thread_corruptions_fail(self):
        value, trace = synthetic_original()
        self.validate(value, trace)
        for mutation in ("missing_call", "coordinate", "stage_binding", "stage_random", "stage_chunk",
                         "world_identity", "thread_role", "original_frame", "query_order", "prior_return", "manager_d"):
            changed, measured = copy.deepcopy(value), copy.deepcopy(trace)
            if mutation == "missing_call":
                changed["calls"].pop()
            elif mutation == "coordinate":
                changed["calls"][1]["coordinates"] = [8, 8]
            elif mutation == "stage_binding":
                measured["calls"][0]["stages"][6]["method"] = "b"
            elif mutation == "stage_random":
                measured["calls"][0]["stages"][3]["generator_seed"] ^= 1
            elif mutation == "stage_chunk":
                measured["calls"][0]["stages"][2]["chunk_object_id"] = None
            elif mutation == "world_identity":
                measured["calls"][1]["identity"]["world_object_id"] += 1
            elif mutation == "thread_role":
                measured["calls"][0]["begin_threads"]["threads"][0]["daemon"] = True
            elif mutation == "original_frame":
                measured["calls"][0]["begin_threads"]["threads"][0]["frames"][0]["original_frame"] = True
            elif mutation == "query_order":
                measured["calls"][0]["records"][0]["ordinal"] = 1
            elif mutation == "prior_return":
                changed["calls"][1]["prior_retained"][0]["local"]["data"]["minimum_height"] = 1
            else:
                changed["calls"][0]["after"]["manager"]["identities"]["d"] = CHUNK_GUARDS.reference("r999", "[Lkd;")
            with self.subTest(mutation=mutation), self.assertRaises(OVERWORLD.Failure):
                self.validate(changed, measured)

    def test_raw_48_bit_continuation_matches_existing_java_oracle(self):
        # The same Temurin17 oracle values are retained in tests/core/java/test_java.c.
        longs = (-4962768465676381896, 4437113781045784766, -6688467811848818630,
                 -8292973307042192125, -7423979211207825555, 6146794652083548235,
                 7105486291024734541, -279624296851435688)
        expected = [f"{value & ((1 << 64) - 1):016x}" for value in longs]
        self.assertEqual(OVERWORLD.next_longs(25214903917), expected)
        for seed in (True, -1, 1 << 48, 1.0):
            with self.subTest(seed=seed), self.assertRaises(OVERWORLD.Failure):
                OVERWORLD.next_longs(seed)
        record = random_record(25214903917)
        self.assertEqual(OVERWORLD.random_projection(record, 25214903917),
                         {"state": "0005deece66d", "next_longs": expected})
        for changed in (dict(record, serialized_hex=record["serialized_hex"][:-2]),
                        dict(record, next_longs=expected[:-1]),
                        dict(record, next_longs=["0" * 16] + expected[1:])):
            with self.subTest(record=changed), self.assertRaises(OVERWORLD.Failure):
                OVERWORLD.random_projection(changed, 25214903917)

    def test_descriptors_require_complete_raw_shape_and_integer_lengths(self):
        OVERWORLD.raw_descriptor(doubles(2), length=2)
        OVERWORLD.raw_descriptor({"length": 2, "values": "am"}, "biome", 2)
        for value in (None, {"length": True, "values": "0" * 16},
                      {"length": -1, "values": ""}, {"length": 4097, "values": ""},
                      {"length": 2, "values": "0" * 31}, {"length": 2, "values": "G" * 32}):
            with self.subTest(value=value), self.assertRaises(OVERWORLD.Failure):
                OVERWORLD.raw_descriptor(value, length=2)
        for text in ("a-", "an", "A", "a"):
            with self.subTest(text=text), self.assertRaises(OVERWORLD.Failure):
                OVERWORLD.raw_descriptor({"length": 2, "values": text}, "biome", 2)

    def test_noise_requires_all_82_complete_layers(self):
        layer = {"offsets": ["0" * 16] * 3, "permutation": list(range(256)) * 2}
        noise = {name: [copy.deepcopy(layer) for _ in range(count)] for name, count in
                 zip(("k", "l", "m", "n", "o", "a", "b", "c"), (16, 16, 8, 4, 4, 10, 16, 8))}
        OVERWORLD.validate_noise(noise)
        for mutation in ("channel", "layer", "offset", "permutation_length", "permutation_type", "permutation_range"):
            changed = copy.deepcopy(noise)
            if mutation == "channel":
                del changed["c"]
            elif mutation == "layer":
                changed["k"].pop()
            elif mutation == "offset":
                changed["k"][0]["offsets"][0] = "0" * 15
            elif mutation == "permutation_length":
                changed["k"][0]["permutation"].pop()
            else:
                changed["k"][0]["permutation"][0] = True if mutation == "permutation_type" else 256
            with self.subTest(mutation=mutation), self.assertRaises(OVERWORLD.Failure):
                OVERWORLD.validate_noise(changed)

    def test_presence_descriptors_follow_each_current_query_origin(self):
        baseline = presence(CHUNK_GUARDS.synthetic_world(), (0, 0))
        changed_origin = presence(baseline, (1, 0))
        self.assertNotEqual(baseline, changed_origin)
        self.assertEqual(OVERWORLD.world_projection(baseline), OVERWORLD.world_projection(changed_origin))
        OVERWORLD.validate_presence(changed_origin, (1, 0), FINISH)
        changed = copy.deepcopy(changed_origin)
        changed["presence"][0]["key"] ^= 1
        with self.assertRaises(OVERWORLD.Failure):
            OVERWORLD.validate_presence(changed, (1, 0), FINISH)
        changed = copy.deepcopy(changed_origin)
        changed["queue"]["entries"].append("changed")
        self.assertNotEqual(OVERWORLD.world_projection(baseline), OVERWORLD.world_projection(changed))

    def test_replay_preserves_exact_manager_lengths_and_all_payload_bytes(self):
        value = synthetic_replay_record()
        encoded = OVERWORLD.replay_input(value, FINISH)
        expected = bytearray(b"MCG1")
        expected += struct.pack(">q?iIIII", 23, False, 0, 1, 0, 0, 0)
        expected += bytes(1024)
        expected += bytes(item for i in range(13) for item in (i, 255 - i))
        for length in (256, 257, 256):
            expected += struct.pack(">i", length) + bytes(length * 8)
        expected += struct.pack(">i", 2) + b"am"
        expected += struct.pack(">ii??", 0, 0, True, False) + bytes(256)
        self.assertEqual(encoded, bytes(expected))

    def test_replay_rejects_ambiguous_null_and_overbudget_cache(self):
        for mutation in ("duplicate_identity", "null_map", "null_insertion", "queue_budget"):
            value = synthetic_replay_record()
            world = value["setup_world"]
            if mutation == "duplicate_identity":
                world["chunks"].append(copy.deepcopy(world["chunks"][0]))
            elif mutation == "null_map":
                world["map"]["entries"] = [{"key": 0, "value": None}]
            elif mutation == "null_insertion":
                world["insertion_list"]["entries"] = [{"value": None}]
            else:
                world["queue"]["entries"] = [{"kind": {"ordinal": 0}, "bounds": [0] * 6}] * 23553
            with self.subTest(mutation=mutation), self.assertRaises(OVERWORLD.Failure):
                OVERWORLD.replay_input(value, FINISH)

    def test_fixed_acceptance_corpus_retains_four_seeds_and_nine_ordered_calls(self):
        self.assertEqual(OVERWORLD.SEEDS, (23, 24, 26, 51))
        self.assertEqual(OVERWORLD.COORDINATES,
                         ((1, 0), (0, 0), (1, 0), (-1, -1), (32768, 0), (2000000, 0),
                          (-2000000, 0), (-2147483640, 0), (2147483638, 0)))
        self.assertEqual(len(OVERWORLD.STAGES), 17)


if __name__ == "__main__":
    unittest.main()
