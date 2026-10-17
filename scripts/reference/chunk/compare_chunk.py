#!/usr/bin/env python3
"""Compare chunk arrays against complete finishing calls in fresh original worlds.

The class-global light-query latch is retained in each world snapshot and checked
for preservation within that context. Genuine setup may leave different latch
values across sides; it is not a field of the measured chunk instance.
"""

import argparse
from dataclasses import asdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import types
import uuid
import zipfile


OPACITY_SHA = "f15de154e2937ae549a09e588e04ae8b4e5c21a99b3649cbf45aa2e067fe63ce"
JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
NATURAL_MANIFEST = "bab5f6aad4890ba7bbdcc74384f9274a60855bcbd47230199e2b6188e79d14f8"
NATURAL = {
    2: (1, (0, 0), "07520e66a906c142aa8556d6f6bf288b9f754f8eae20b4fcb3d2ce39eebefbe6",
        "6abfcc4858e613eec7240d19eaca3c3cf338745eca01b0420942fbf2af40e430"),
    6: (0, (-1099, 279), "f4495ce680feb462c5451b50ed2427aa5d6fa54291f24f23b7a2c07a6fd56dfc",
        "79a33c11c54819b4d596d45dfd05833f99f90fb0d1954af2bef48a93bb65d347"),
}
STAGES = ("before", "after", "repeat_before", "repeat_after")
ARRAY_LENGTHS = dict(blocks=32768, metadata=16384, skylight=16384, block_light=16384, heightmap=256)
HEX = re.compile(r"[0-9a-f]*\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
MAX_RECORD = 8 * 1024 * 1024
MAX_ARTIFACTS = 192 * 1024 * 1024
CLASS_NAMES = {"ChunkObservation.class", "ChunkObservation$Model.class", "ChunkObservation$SetupUnavailable.class"}


class Failure(Exception):
    pass


class Unavailable(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def outcome(failures, unavailable):
    return 1 if failures else 77 if unavailable else 0


def record(path, maximum=None):
    path = Path(path).resolve(strict=True)
    before = path.stat()
    require(stat.S_ISREG(before.st_mode), "not a regular input/artifact: " + str(path))
    bound = before.st_size if maximum is None else maximum
    require(before.st_size <= bound, "hashing size bound: " + str(path))
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        remaining = bound
        while remaining:
            data = stream.read(min(1024 * 1024, remaining))
            if not data:
                break
            digest.update(data)
            remaining -= len(data)
        require(not stream.read(1), "input grew past hashing bound")
    after = path.stat()
    require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) ==
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns), "input changed while hashing")
    return {"path": str(path), "bytes": after.st_size, "sha256": digest.hexdigest()}


def source_module(path, name):
    path = Path(path).resolve(strict=True)
    attributed = record(path, 2 * 1024 * 1024)
    data = path.read_bytes()
    require(len(data) == attributed["bytes"] and hashlib.sha256(data).hexdigest() == attributed["sha256"],
            "loaded helper source changed")
    module = types.ModuleType(name)
    module.__file__ = str(path)
    old = sys.modules.get(name)
    sys.modules[name] = module
    try:
        exec(compile(data, str(path), "exec"), module.__dict__)
    finally:
        if old is None:
            sys.modules.pop(name, None)
        else:
            sys.modules[name] = old
    return module, attributed


def shared_helper(root):
    directory = root / "scripts/reference/caves"
    fixture, fixture_record = source_module(directory / "fixtures.py", "_chunk_natural_fixtures")
    old = sys.modules.get("fixtures")
    sys.modules["fixtures"] = fixture
    try:
        helper, helper_record = source_module(directory / "compare_caves.py", "_chunk_process_helper")
    finally:
        if old is None:
            sys.modules.pop("fixtures", None)
        else:
            sys.modules["fixtures"] = old
    inventory = helper.world_inventory

    def bounded_inventory(path, hashes=False):
        return inventory(path, hashes=hashes, max_entries=256, max_bytes=32 * 1024 * 1024)

    # Adapt bounds only in this isolated copy of the reviewed helper module.
    helper.world_inventory = bounded_inventory
    return helper, [helper_record, fixture_record]


def pairs(values):
    result = {}
    for key, value in values:
        require(key not in result, "duplicate JSON key: " + key)
        result[key] = value
    return result


def strict_json(data):
    try:
        return json.loads(data, object_pairs_hook=pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(Failure("nonfinite JSON value")))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise Failure("invalid JSON: " + str(error)) from error


def keys(value, expected, context):
    require(type(value) is dict and set(value) == set(expected), context + ": complete object keys")


def integer(value, low, high, context):
    require(type(value) is int and low <= value <= high, context + ": signed integer range")


def boolean(value, context):
    require(type(value) is bool, context + ": boolean")


def blob(value, length, context):
    keys(value, ("length", "values"), context)
    require(type(value["length"]) is int and value["length"] == length, context + ": byte count")
    text = value["values"]
    require(type(text) is str and len(text) == length * 2 and HEX.fullmatch(text), context + ": raw bytes")
    return bytes.fromhex(text)


def validate_data(value, context):
    keys(value, (*ARRAY_LENGTHS, "minimum_height"), context)
    for name, length in ARRAY_LENGTHS.items():
        blob(value[name], length, context + "." + name)
    integer(value["minimum_height"], 0, 127, context)


def validate_ref(value, context):
    keys(value, ("ref", "class"), context)
    if value["ref"] is None:
        require(value["class"] is None, context + ": null reference type")
    else:
        require(type(value["ref"]) is str and re.fullmatch(r"r[0-9]+", value["ref"]), context + ": reference tag")
        require(type(value["class"]) is str and 1 <= len(value["class"]) <= 200, context + ": class")


def validate_flags(value, context):
    keys(value, ("c", "n", "o", "p", "q"), context)
    for item in value.values():
        boolean(item, context)


def validate_world(value, limit, context):
    references = ("world", "provider", "storage", "cache", "provider_world", "manager", "cache_world", "generator", "loader")
    keys(value, (*references, "no_sky", "map", "insertion_list", "pending_set", "fallback", "chunks", "presence",
                 "queue", "enqueue_depth", "drain_depth", "chunk_static_a"), context)
    for name in references:
        validate_ref(value[name], context + "." + name)
    boolean(value["no_sky"], context)
    boolean(value["chunk_static_a"], context + ": class-global light-query latch")
    require(not value["no_sky"], "normal provider must have sky")
    for name in ("enqueue_depth", "drain_depth"):
        integer(value[name], -(2 ** 31), 2 ** 31 - 1, context + "." + name)
    for name, maximum in (("map", 512), ("insertion_list", 512), ("pending_set", 512), ("queue", limit)):
        container = value[name]
        keys(container, ("object", "entries"), context + "." + name)
        validate_ref(container["object"], context)
        entries = container["entries"]
        require(type(entries) is list and len(entries) <= maximum, context + ": collection budget")
        for ordinal, item in enumerate(entries):
            require(type(item) is dict and type(item.get("ordinal")) is int and item["ordinal"] == ordinal,
                    context + ": complete collection order")
            if name == "queue":
                keys(item, ("ordinal", "object", "kind", "bounds"), context + ": region")
                validate_ref(item["object"], context)
                keys(item["kind"], ("identity", "name", "ordinal", "default"), context + ": kind")
                integer(item["kind"]["ordinal"], 0, 1, context + ": enum ordinal")
                integer(item["kind"]["default"], 0, 15, context + ": enum default")
                require(item["kind"] in ({"identity": "a", "name": "Sky", "ordinal": 0, "default": 15},
                                         {"identity": "b", "name": "Block", "ordinal": 1, "default": 0}),
                        context + ": actual light kind")
                require(type(item["bounds"]) is list and len(item["bounds"]) == 6, context + ": six bounds")
                for bound in item["bounds"]:
                    integer(bound, -(2 ** 31), 2 ** 31 - 1, context + ": full signed queue bound")
            else:
                expected = ("ordinal", "key", "value") if name == "map" else (("ordinal", "value") if name == "insertion_list" else ("ordinal", "key"))
                keys(item, expected, context)
                if "key" in item:
                    integer(item["key"], -(2 ** 31), 2 ** 31 - 1, context)
    chunks = value["chunks"]
    require(type(chunks) is list and len(chunks) <= 1025, context + ": cached table budget")
    tags = set()
    for item in chunks:
        keys(item, ("object", "coordinates", "heightmap", "minimum_height", "flags", "time", "measured", "fallback",
                    "empty", "array_references", "blocks", "metadata", "skylight", "block_light"), context + ": cached chunk")
        validate_ref(item["object"], context)
        tag = item["object"]["ref"]
        require(tag is not None and tag not in tags, context + ": deduplicated cache table")
        tags.add(tag)
        require(type(item["coordinates"]) is list and len(item["coordinates"]) == 2, context + ": cached coordinates")
        for coordinate in item["coordinates"]:
            integer(coordinate, -(2 ** 31), 2 ** 31 - 1, context)
        blob(item["heightmap"], 256, context + ": cached height bytes")
        integer(item["minimum_height"], -(2 ** 31), 2 ** 31 - 1, context + ": cached minimum")
        validate_flags(item["flags"], context)
        require(type(item["time"]) is str and re.fullmatch(r"[0-9a-f]{16}", item["time"]), context + ": cached time")
        for name in ("measured", "fallback", "empty"):
            boolean(item[name], context)
        require(type(item["array_references"]) is list and len(item["array_references"]) == 8, context + ": cached array identities")
        for reference in item["array_references"]:
            validate_ref(reference, context)
        for name in ("blocks", "metadata", "skylight", "block_light"):
            fingerprint = item[name]
            keys(fingerprint, ("length", "sha256"), context)
            if fingerprint["length"] is None:
                require(fingerprint["sha256"] is None, context + ": null array hash")
            else:
                integer(fingerprint["length"], 0, 32768, context)
                require(type(fingerprint["sha256"]) is str and HEX64.fullmatch(fingerprint["sha256"]), context + ": array hash")
    require(value["fallback"] in tags, context + ": fallback retained in table")
    for name in ("map", "insertion_list"):
        for item in value[name]["entries"]:
            require(item["value"] is None or item["value"] in tags, context + ": cached reference missing from full table")
    require(type(value["presence"]) is list and len(value["presence"]) == 5, context + ": all five presence queries")
    for item in value["presence"]:
        keys(item, ("coordinates", "key", "present", "contains", "value"), context)
        require(type(item["coordinates"]) is list and len(item["coordinates"]) == 2, context + ": presence coordinates")
        for coordinate in item["coordinates"]:
            integer(coordinate, -(2 ** 31), 2 ** 31 - 1, context)
        integer(item["key"], -(2 ** 31), 2 ** 31 - 1, context)
        boolean(item["present"], context)
        boolean(item["contains"], context)
        require(item["present"] == item["contains"], context + ": actual presence differs from map")
        require(item["value"] is None or item["value"] in tags, context + ": queried reference absent")


def cache_projection(world):
    return {key: value for key, value in world.items() if key != "queue"}


def validate_original(value, case, side):
    keys(value, ("schema", "side", "seed", "coordinates", "bindings", "input_sha256", "repeat_input_sha256", "setup_world",
                 *STAGES, "controls"), side)
    require(type(value["schema"]) is int and value["schema"] == 2 and value["side"] == side, "original schema/side")
    require(type(value["coordinates"]) is list and len(value["coordinates"]) == 2, "original coordinate shape")
    for coordinate in value["coordinates"]:
        integer(coordinate, -2048, 2048, "original coordinates")
    require(value["seed"] == f'{case["seed"]:016x}' and value["coordinates"] == list(case["coordinates"]), "original input coordinates/seed")
    require(value["input_sha256"] == hashlib.sha256(case["initial"]).hexdigest() and
            value["repeat_input_sha256"] == hashlib.sha256(case["repeat"]).hexdigest(), "original complete input hashes")
    bindings = dict(world="fd", chunk="lm", finish="c", nibble="wi", provider="rh", cache="ok") if side == "client" else dict(world="dj", chunk="hi", finish="b", nibble="ob", provider="kp", cache="jb")
    require(value["bindings"] == bindings, "reviewed original bindings")
    keys(value["controls"], ("initial_blocks_unchanged", "repeat_blocks_unchanged", "opacity_unchanged",
                            "chunk_static_a_unchanged"), "original controls")
    require(all(item is True for item in value["controls"].values()), "original control failure")
    validate_world(value["setup_world"], 4096, "setup world")
    for stage, limit in zip(STAGES, (4096, 5120, 5120, 6144)):
        snapshot = value[stage]
        keys(snapshot, ("local", "world", "opacity", "opacity_identity_retained"), stage)
        validate_world(snapshot["world"], limit, stage + ": world")
        opacity = blob(snapshot["opacity"], 1024, stage + ": opacity")
        require(hashlib.sha256(opacity).hexdigest() == OPACITY_SHA, "actual complete opacity changed")
        require(snapshot["opacity_identity_retained"] is True, "opacity identity changed")
        local = snapshot["local"]
        keys(local, ("data", "coordinates", "flags", "time", "map", "entity_slices", "identities"), stage + ": local")
        validate_data(local["data"], stage)
        require(type(local["coordinates"]) is list and len(local["coordinates"]) == 2, "local coordinate shape")
        for coordinate in local["coordinates"]:
            integer(coordinate, -2048, 2048, "local coordinates")
        require(local["coordinates"] == list(case["coordinates"]), "retained original coordinates")
        validate_flags(local["flags"], stage)
        require(local["time"] == "0000000000000000", "fresh original time changed")
        keys(local["map"], ("class", "size"), "local map")
        integer(local["map"]["size"], 0, 0, "local map size")
        require(local["map"] == {"class": "java.util.HashMap", "size": 0}, "fresh local map")
        require(type(local["entity_slices"]) is list and len(local["entity_slices"]) == 8, "local entity slice count")
        for entry in local["entity_slices"]:
            keys(entry, ("class", "size"), "local entity slice")
            integer(entry["size"], 0, 0, "local entity slice size")
        require(local["entity_slices"] == [{"class": "java.util.ArrayList", "size": 0}] * 8, "fresh local entity lists")
        identity = local["identities"]
        keys(identity, ("roles_retained", "roles_pairwise_distinct", "input_retained", "world_retained"), stage)
        require(type(identity["roles_retained"]) is list and len(identity["roles_retained"]) == 19 and
                all(item is True for item in identity["roles_retained"]), "original role identity changed")
        require(all(identity[name] is True for name in ("roles_pairwise_distinct", "input_retained", "world_retained")), "original alias control")
        expected = case["initial"] if stage in ("before", "after") else case["repeat"]
        require(blob(local["data"]["blocks"], 32768, stage) == expected, "original borrowed bytes changed")
    before, after, repeat_before, repeat_after = [value[stage]["local"] for stage in STAGES]
    require(value["setup_world"] == value["before"]["world"], "constructor changed original world")
    for stage in STAGES[1:]:
        require(cache_projection(value[stage]["world"]) == cache_projection(value["before"]["world"]),
                "original cache boundary changed; full diagnostic retained")
        require(value[stage]["world"]["queue"]["object"] == value["before"]["world"]["queue"]["object"], "queue identity changed")
    for name in ("metadata", "skylight", "block_light", "heightmap"):
        require(blob(before["data"][name], ARRAY_LENGTHS[name], "constructor") == bytes(ARRAY_LENGTHS[name]), "fresh array not zero")
    require(before["data"]["minimum_height"] == 0 and not any(before["flags"].values()), "fresh constructor primitives")
    for local in (after, repeat_before, repeat_after):
        require(local["flags"] == dict(c=False, n=False, o=True, p=False, q=False), "actual finishing dirty state")
        for name in ("metadata", "block_light"):
            require(local["data"][name] == before["data"][name], "original unaffected packed bytes changed")
    require({key: data for key, data in after["data"].items() if key != "blocks"} ==
            {key: data for key, data in repeat_before["data"].items() if key != "blocks"}, "repeat edit changed other local data")
    return {stage: value[stage]["local"]["data"] for stage in STAGES}


def validate_c(value, case):
    keys(value, ("schema", *STAGES, "controls"), "C")
    require(type(value["schema"]) is int and value["schema"] == 1, "C schema")
    keys(value["controls"], ("initial_blocks_unchanged", "repeat_blocks_unchanged", "metadata_unchanged",
                            "block_light_unchanged", "opacity_unchanged", "input_retained"), "C controls")
    require(all(item is True for item in value["controls"].values()), "C preservation control")
    for stage in STAGES:
        validate_data(value[stage], "C " + stage)
        expected = case["initial"] if stage in ("before", "after") else case["repeat"]
        require(blob(value[stage]["blocks"], 32768, stage) == expected, "C complete borrowed input")
    return {stage: value[stage] for stage in STAGES}


def first_difference(left, right, path="$"):
    """Keep the first exact path and small values, never a whole array dump."""
    if type(left) is not type(right):
        return dict(path=path, reason="type", left_type=type(left).__name__, right_type=type(right).__name__)
    if isinstance(left, dict):
        if set(left) != set(right):
            return dict(path=path, reason="keys", left_keys=sorted(left), right_keys=sorted(right))
        for key in left:
            result = first_difference(left[key], right[key], path + "." + key)
            if result is not None:
                return result
        return None
    if isinstance(left, list):
        if len(left) != len(right):
            return dict(path=path, reason="length", left_length=len(left), right_length=len(right))
        for index, (a, b) in enumerate(zip(left, right)):
            result = first_difference(a, b, f"{path}[{index}]")
            if result is not None:
                return result
        return None
    if left == right:
        return None
    if isinstance(left, str) and len(left) == len(right) and len(left) > 64 and len(left) % 2 == 0 and HEX.fullmatch(left) and HEX.fullmatch(right):
        index = next(index for index in range(0, len(left), 2) if left[index:index + 2] != right[index:index + 2])
        return dict(path=path, reason="byte", byte_index=index // 2, left=left[index:index + 2], right=right[index:index + 2])
    return dict(path=path, reason="value", left=left, right=right)


def no_links(path):
    for item in reversed((Path(path), *Path(path).parents)):
        try:
            info = item.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(info.st_mode) and not (getattr(info, "st_file_attributes", 0) & 0x400),
                "link/reparse output component: " + str(item))


def guard_work(work, protected):
    no_links(work)
    canonical = work.resolve()
    for path in protected:
        path = Path(path).resolve()
        require(canonical != path and canonical not in path.parents, "work directory contains supplied source/input")
    manifest = work / "manifest.json"
    if os.path.lexists(manifest):
        no_links(manifest)
        require(manifest.is_file() and manifest.stat().st_nlink == 1, "manifest is not independent regular storage")
        require(all(not os.path.samefile(manifest, path) for path in protected if Path(path).exists()), "manifest aliases input")


def write_json(path, value):
    data = (json.dumps(value, indent=2, allow_nan=False) + "\n").encode()
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    with temporary.open("xb") as stream:
        stream.write(data)
    os.replace(temporary, path)


def roof(initial):
    result = bytearray(initial)
    for x in range(16):
        for z in range(16):
            result[x * 2048 + z * 128 + 127] = 1
    return bytes(result)


def natural_paths(root, override=None):
    frozen = Path(override) if override else root / ".local/reference/caves-setup/capture-ea3e2caa3e134ed998a7394406a633e5"
    return [frozen / "manifest.json", *[
        frozen / f"{side}-case-{number:03d}.json"
        for number in NATURAL for side in ("client", "server")]]


def cases(root, smoke=False, natural_root=None):
    result = [dict(label="air_to_stone", seed=0, coordinates=(0, 0), initial=bytes(32768), repeat=bytes([1]) * 32768,
                   authority="owned air/stone inputs; observed initialized opacity hash pinned")]
    dependencies = []
    if smoke:
        return result, dependencies
    result.append(dict(label="stone_to_air", seed=0, coordinates=(-1, 1), initial=bytes([1]) * 32768, repeat=bytes(32768),
                       authority="owned valid stone/air inputs"))
    mixed = bytearray(32768)
    # Input layout only. No expected height, light, or queue state is computed.
    # Actual captured IDs: stone 1->255; glass 20->0; leaves 18->1;
    # water 8/9 and ice 79->3. Vary X/Z and both packed Y parities.
    for x in range(16):
        for z in range(16):
            base = x * 2048 + z * 128
            mode = (x + z * 3) % 8
            if mode == 0:
                mixed[base] = 1
            elif mode == 1:
                mixed[base + 126] = 1
            elif mode == 2:
                mixed[base + 127] = 1
            elif mode == 3:
                mixed[base:base + 128] = bytes([20]) * 128
            else:
                kinds = (18, 8, 9, 79)
                for y in range(111 + mode % 2, 128):
                    mixed[base + y] = kinds[(y + x + z) % 4]
                mixed[base + x + z] = 20
    result.append(dict(label="mixed_to_roof", seed=1, coordinates=(2, -3), initial=bytes(mixed), repeat=roof(mixed),
                       authority="owned asymmetric input using observed registered zero/partial opacity IDs"))
    manifest = natural_paths(root, natural_root)[0]
    frozen = manifest.parent
    if not manifest.is_file():
        raise Unavailable("frozen original post-cave manifest missing: " + str(manifest))
    require(record(manifest, 4 * 1024 * 1024)["sha256"] == NATURAL_MANIFEST, "frozen natural manifest hash")
    dependencies.append(manifest)
    for number, (seed, coordinates, file_sha, input_sha) in NATURAL.items():
        observed = []
        for side in ("client", "server"):
            path = frozen / f"{side}-case-{number:03d}.json"
            require(record(path, 2 * 1024 * 1024)["sha256"] == file_sha, "frozen natural full record hash")
            dependencies.append(path)
            capture = strict_json(path.read_bytes())
            block_data = blob(capture["passes"][0]["after"]["blocks"], 32771, "frozen post-cave input")
            require(block_data[-3:] == bytes.fromhex("8a8b8c"), "frozen transport suffix")
            selected = block_data[:32768]
            require(hashlib.sha256(selected).hexdigest() == input_sha, "frozen original prefix hash")
            observed.append(selected)
        require(observed[0] == observed[1], "frozen original input pair disagrees")
        result.append(dict(label=f"natural_{number}_to_roof", seed=seed, coordinates=coordinates, initial=observed[0],
                           repeat=roof(observed[0]), authority=f"both pinned original cave case {number}; exact 32768-byte prefix"))
    return result, dependencies


def class_inventory(path):
    require({item.name for item in path.iterdir()} == CLASS_NAMES, "fresh observer class set")
    result = {}
    for name in sorted(CLASS_NAMES):
        target = path / name
        require(target.is_file() and target.stat().st_nlink == 1 and not target.is_symlink(), "fresh class independent storage")
        require(target.read_bytes()[:8] == b"\xca\xfe\xba\xbe\x00\x00\x00\x37", "strict Java11 helper bytecode")
        result[name] = record(target, 1024 * 1024)
    return result


def read_output(path):
    require(path.stat().st_size <= MAX_RECORD, "complete record byte budget")
    data = path.read_bytes()
    require(data.endswith(b"\n") and data.count(b"\n") == 1, "one complete JSON record required")
    return strict_json(data)


def original_status(process, stdout, stderr):
    """Only explicit setup-only exits are unavailable; completed work fails closed."""
    markers = [line for line in stderr.splitlines() if line in
               ("SETUP_BEGIN", "SETUP_DONE", "MEASUREMENT_BEGIN", "MEASUREMENT_DONE")]
    allowed_setup = (
        markers == [] and "UNAVAILABLE: original class initialization" in stderr.splitlines()
    ) or (
        markers == ["SETUP_BEGIN"] and "UNAVAILABLE: genuine original world setup" in stderr.splitlines()
    )
    if process.returncode == 77 and not process.stop_reason and not stdout and allowed_setup:
        return "unavailable"
    require(process.returncode == 0 and not process.stop_reason, "original process failed")
    require(markers == ["SETUP_BEGIN", "SETUP_DONE", "MEASUREMENT_BEGIN", "MEASUREMENT_DONE"],
            "complete original lifecycle required")
    return "completed"


def source_paths(root):
    names = ("CMakeLists.txt", "scripts/reference/chunk/compare_chunk.py", "tests/reference/chunk/ChunkObservation.java",
             "tests/world/chunk/data_probe.c", "tests/tools/chunk/test_data_reference.py", "tests/world/chunk/test_data.c",
             "src/world/chunk/data.c", "src/world/chunk/nibbles.c", "include/mc/world/chunk/data.h",
             "include/mc/world/chunk/nibbles.h")
    return [root / name for name in names]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client", "server", "java", "javac", "work-dir", "tested-commit", "natural-root"):
        parser.add_argument("--" + name, required=name in ("client", "server", "work-dir", "tested-commit"))
    parser.add_argument("--capture-only", action="store_true", help="capture originals without claiming a C comparison")
    parser.add_argument("--smoke", action="store_true", help="run only the first air-to-stone pair")
    args = parser.parse_args(argv)
    require(re.fullmatch(r"[0-9a-f]{40}", args.tested_commit) is not None, "full lowercase tested object ID required")
    require(args.capture_only or args.executable, "--executable required for comparison")
    root = Path(__file__).resolve().parents[3]
    work = Path(args.work_dir).absolute()
    helper, shared_sources = shared_helper(root)
    sources = source_paths(root)
    supplied = [Path(args.client), Path(args.server), *sources]
    supplied.extend(Path(value["path"]) for value in shared_sources)
    supplied.append(Path(sys.executable))
    if not args.smoke:
        supplied.extend(natural_paths(root, args.natural_root))
    # Discover possible runtime inputs before the first output mutation.
    # Missing tools remain explicit unavailable results inside the capture.
    for override, name in ((args.java, "java"), (args.javac, "javac")):
        try:
            tool = helper.find_tool(override, name)
        except helper.Unavailable:
            continue
        supplied.extend((tool, tool.parent.parent / "release", tool.parent.parent / "lib/modules",
                         tool.parent.parent / "lib/ct.sym"))
    if args.executable:
        supplied.append(Path(args.executable))
    guard_work(work, supplied)
    work.mkdir(parents=True, exist_ok=True)
    capture = work / ("capture-" + uuid.uuid4().hex)
    capture.mkdir()
    world_base = root / ".local/reference/chunk-data-setup" / capture.name
    no_links(world_base)
    world_base.mkdir(parents=True)
    report = dict(schema=2, status="running", tested_commit=args.tested_commit, capture_only=args.capture_only,
                  scope="local chunk arrays compared; complete original cache/queue diagnostics retained separately",
                  started_utc=datetime.now(timezone.utc).isoformat(), inputs={}, sources=shared_sources,
                  commands=[], records=[], comparisons=[], errors=[], unavailable=[], worlds=[])
    protected = {}
    compiled = None
    try:
        require(not any(os.environ.get(name) for name in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS")),
                "unattributed injected Java options")
        java = helper.find_tool(args.java, "java")
        javac = helper.find_tool(args.javac, "javac")
        require(java.parent.parent == javac.parent.parent, "java and javac must belong to one JDK")
        jdk = java.parent.parent
        for name, path in (("java", java), ("javac", javac), ("python", Path(sys.executable)),
                           ("jdk_release", jdk / "release"), ("jdk_modules", jdk / "lib/modules"), ("jdk_ct_sym", jdk / "lib/ct.sym")):
            if not path.is_file():
                raise Unavailable("required runtime attribution file missing: " + str(path))
            protected[name] = record(path)
        for name in ("client", "server"):
            path = Path(getattr(args, name))
            if not path.is_file():
                raise Unavailable("original JAR missing: " + str(path))
            path = path.resolve(strict=True)
            protected[name] = record(path, 4 * 1024 * 1024)
            require(protected[name]["sha256"] == JAR_HASHES[name], "original JAR hash: " + name)
            with zipfile.ZipFile(path) as archive:
                names = ("fd", "xa", "rh", "wt", "wy", "lm", "wi", "ok", "li", "st", "eb", "yy", "uu") if name == "client" else ("dj", "os", "kp", "om", "dq", "hi", "ob", "jb", "hg", "lv", "co", "qa", "na")
                report.setdefault("original_members", {})[name] = {
                    member + ".class": hashlib.sha256(archive.read(member + ".class")).hexdigest() for member in names}
        if args.executable:
            if not Path(args.executable).is_file():
                raise Unavailable("C probe executable missing: " + args.executable)
            protected["executable"] = record(Path(args.executable).resolve(strict=True))
        requested, natural_dependencies = cases(root, args.smoke, args.natural_root)
        guard_work(work, [*supplied, *natural_dependencies])
        for index, path in enumerate(natural_dependencies):
            protected["natural_" + str(index)] = record(path, 4 * 1024 * 1024)
        for path in sources:
            report["sources"].append(record(path, 2 * 1024 * 1024))
        protected.update({"source_" + str(index): value for index, value in enumerate(report["sources"])})
        report["inputs"] = dict(protected)
        report["python"] = sys.version
        for name, tool in (("java", java), ("javac", javac)):
            out, err = capture / (name + "-version.stdout"), capture / (name + "-version.stderr")
            process = helper.run_live([str(tool), "-version"], out, err, capture, seconds=15, stdout_limit=65536, stderr_limit=65536)
            report["commands"].append(asdict(process))
            require(process.returncode == 0 and not process.stop_reason, name + " version failed")
            text = (out.read_bytes() + b"\n" + err.read_bytes()).decode("utf-8")
            require(helper.java_major(text, name) >= 11, "Java11 or newer required")
        classes = capture / "classes"
        classes.mkdir()
        empty = capture / "empty"
        empty.mkdir()
        process = helper.run_live([str(javac), "--release", "11", "-Xlint:all", "-Werror", "-encoding", "UTF-8",
                                   "-classpath", str(empty), "-sourcepath", str(empty), "-d", str(classes),
                                   str(root / "tests/reference/chunk/ChunkObservation.java")],
                                  capture / "compile.stdout", capture / "compile.stderr", empty,
                                  seconds=30, stdout_limit=1024 * 1024, stderr_limit=4 * 1024 * 1024)
        report["commands"].append(asdict(process))
        require(process.returncode == 0 and not process.stop_reason, "observer compilation failed")
        compiled = class_inventory(classes)
        report["compiled_classes"] = compiled
        for number, case in enumerate(requested):
            initial_path = capture / f"case-{number:03d}-initial.bin"
            repeated_path = capture / f"case-{number:03d}-repeat.bin"
            initial_path.write_bytes(case["initial"])
            repeated_path.write_bytes(case["repeat"])
            protected[f"case_{number}_initial"] = record(initial_path, 32768)
            protected[f"case_{number}_repeat"] = record(repeated_path, 32768)
            observed = {}
            for side in ("client", "server"):
                label = f"{side}-{number:03d}"
                out, err = capture / (label + ".stdout"), capture / (label + ".stderr")
                owned_world = world_base / label
                process = helper.run_live([str(java), "-Xmx512m", "-Djava.awt.headless=true", "-cp", str(classes),
                                          "ChunkObservation", protected[side]["path"], side, f'{case["seed"]:016x}',
                                          str(case["coordinates"][0]), str(case["coordinates"][1]), str(initial_path),
                                          str(repeated_path), str(owned_world)], out, err, empty, seconds=30,
                                         stdout_limit=MAX_RECORD, stderr_limit=4 * 1024 * 1024, world_root=owned_world)
                report["commands"].append(asdict(process))
                inventory = helper.world_inventory(owned_world, hashes=True)
                report["worlds"].append(inventory)
                try:
                    require(inventory["complete"] and inventory["error"] is None, "owned root complete inventory")
                    stderr = err.read_text(encoding="utf-8", errors="replace")
                    if original_status(process, out.read_bytes(), stderr) == "unavailable":
                        report["unavailable"].append(label + ": original setup unavailable; retained stderr")
                        continue
                    value = read_output(out)
                    # Retain the complete record before judging any control.
                    path = capture / (label + ".json")
                    write_json(path, value)
                    report["records"].append(dict(side=side, case=case["label"], authority=case["authority"], artifact=record(path)))
                    observed[side] = validate_original(value, case, side)
                    observed[side + "_record"] = value
                except (Failure, OSError, ValueError, KeyError, TypeError) as error:
                    report["errors"].append(label + ": " + str(error))
            if "client" in observed and "server" in observed:
                difference = None
                for stage in STAGES:
                    left = observed["client_record"][stage]["local"]
                    right = observed["server_record"][stage]["local"]
                    local_difference = first_difference(left, right, f"$.{stage}.local")
                    if difference is None:
                        difference = local_difference
                report["comparisons"].append(dict(case=case["label"], pair="client/server", equal=difference is None,
                                                  difference=difference))
                if difference is not None:
                    report["errors"].append(case["label"] + ": original local difference " + json.dumps(difference))
            if not args.capture_only and ("client" in observed or "server" in observed):
                authority = observed.get("client_record", observed.get("server_record"))
                opacity_path = capture / f"case-{number:03d}-opacity.i32be"
                opacity_path.write_bytes(blob(authority["before"]["opacity"], 1024, "C observed opacity input"))
                protected[f"case_{number}_opacity"] = record(opacity_path, 1024)
                out, err = capture / f"c-{number:03d}.stdout", capture / f"c-{number:03d}.stderr"
                process = helper.run_live([protected["executable"]["path"], str(initial_path), str(repeated_path), str(opacity_path)],
                                         out, err, empty, seconds=15, stdout_limit=MAX_RECORD, stderr_limit=4 * 1024 * 1024)
                report["commands"].append(asdict(process))
                try:
                    require(process.returncode == 0 and not process.stop_reason, "C probe process failed")
                    value = read_output(out)
                    path = capture / f"c-{number:03d}.json"
                    write_json(path, value)
                    report["records"].append(dict(side="c", case=case["label"], artifact=record(path)))
                    actual = validate_c(value, case)
                    for side in ("client", "server"):
                        if side in observed:
                            difference = first_difference(actual, observed[side])
                            report["comparisons"].append(dict(case=case["label"], pair="c/" + side,
                                                              equal=difference is None, difference=difference))
                            if difference is not None:
                                report["errors"].append(case["label"] + ": C/" + side + " difference " + json.dumps(difference))
                except (Failure, OSError, ValueError, KeyError, TypeError) as error:
                    report["errors"].append(case["label"] + ": " + str(error))
            require(class_inventory(classes) == compiled, "compiled observer classes changed")
            for name, before in protected.items():
                require(record(before["path"], before["bytes"]) == before, "source/input preservation: " + name)
        report["requested_cases"] = len(requested)
    except (Unavailable, helper.Unavailable) as error:
        report["unavailable"].append(str(error))
    except Exception as error:
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        for name, before in protected.items():
            try:
                require(record(before["path"], before["bytes"]) == before, "input changed: " + name)
            except Exception as error:
                report["errors"].append(str(error))
        if compiled is not None:
            try:
                require(class_inventory(capture / "classes") == compiled, "compiled classes changed")
            except Exception as error:
                report["errors"].append(str(error))
        artifacts = []
        total = 0
        for path in sorted(capture.rglob("*")):
            if path.is_file():
                try:
                    item = record(path, MAX_ARTIFACTS)
                    total += item["bytes"]
                    artifacts.append(item)
                except Exception as error:
                    report["errors"].append(str(error))
        if total > MAX_ARTIFACTS:
            report["errors"].append("combined artifact byte budget exceeded")
        report["artifacts"] = artifacts
        report["inputs"] = protected
        top_manifest_safe = True
        try:
            guard_work(work, supplied)
        except (Failure, OSError) as error:
            top_manifest_safe = False
            report["errors"].append("top manifest preservation guard: " + str(error))
        result = outcome(report["errors"], report["unavailable"])
        report["status"] = "fail" if result == 1 else "unavailable" if result == 77 else "captured" if args.capture_only else "pass"
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_json(capture / "manifest.json", report)
        published_manifest = capture / "manifest.json"
        if top_manifest_safe:
            write_json(work / "manifest.json", report)
            published_manifest = work / "manifest.json"
        print(json.dumps({"status": report["status"], "manifest": str(published_manifest),
                          "cases": report.get("requested_cases", 0), "comparisons": len(report["comparisons"]),
                          "errors": report["errors"], "unavailable": report["unavailable"]}))
    return result


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (Failure, OSError) as error:
        print("chunk reference: " + str(error), file=sys.stderr)
        raise SystemExit(1)
