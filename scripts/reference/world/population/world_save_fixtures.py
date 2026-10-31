"""Lossless public full World save records derived only from original observations.

Capture proof is caller-supplied attribution. Expected projections are always
rederived by project_original; native results are never inputs to this package.
"""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import tempfile


ROOT = Path(__file__).resolve().parents[4]
MAX_PACKAGE = 16 * 1024 * 1024
MAX_RECORD = 64 * 1024 * 1024
MAX_SOURCE = 2 * 1024 * 1024
SEED = "0000000000000017"
PROFILE = "full-world-save"
SCOPE = "original-game-population-full-world-save-fixture"
JDK_PROFILE = "Java17 --release 17; -Xint -Xmx512m; java.base/java.util=ALL-UNNAMED"
SOURCES = (
    "tests/reference/chunk/ChunkObservation.java",
    "tests/reference/cache/CacheCallbacksObservation.java",
    "tests/reference/world/population/NaturalPopulationObservation.java",
    "tests/reference/world/population/PopulationPersistenceObservation.java",
    "tests/reference/world/population/PopulationWorldSaveObservation.java",
)
HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}


def require(value, message):
    if not value:
        raise ValueError(message)


def _keys(value, names, context):
    require(type(value) is dict and set(value) == set(names), context + ": complete keys")


def _comparison():
    path = ROOT / "scripts/reference/world/population/compare_world_save.py"
    spec = importlib.util.spec_from_file_location("_population_world_save_fixture_comparison", path)
    require(spec is not None and spec.loader is not None, "original persistence comparison support")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _support(comparison):
    require(comparison.HASHES == HASHES and comparison.PROFILE == PROFILE and
            comparison.C.HASHES == HASHES and comparison.C.SEED == SEED,
            "same pinned original archives and full World save profile")
    return comparison.C.PUBLIC


def _json_bytes(value, limit):
    raw = (json.dumps(value, ensure_ascii=True, separators=(",", ":"),
                      allow_nan=False) + "\n").encode("ascii")
    require(0 < len(raw) <= limit, "bounded complete JSON record")
    return raw


def _public_values(value):
    """Reject host paths and non-JSON values, including inside capture proof."""
    count = 0
    stack = [(value, 0)]
    while stack:
        item, depth = stack.pop()
        count += 1
        require(count <= 4_000_000 and depth <= 64, "bounded public JSON structure")
        kind = type(item)
        if kind is dict:
            require(all(type(key) is str for key in item), "string public object keys")
            stack.extend((key, depth + 1) for key in item)
            stack.extend((entry, depth + 1) for entry in item.values())
        elif kind is list:
            stack.extend((entry, depth + 1) for entry in item)
        elif kind is str:
            require("file:/" not in item.lower() and not item.startswith("/") and
                    re.search(r"[A-Za-z]:[\\/]", item) is None and
                    re.search(r"\\\\[^\\]+\\[^\\]+", item) is None,
                    "public record retains host path")
        else:
            require(item is None or kind in (bool, int, float), "ordinary JSON value")


def _source_records():
    records = {}
    for name in SOURCES:
        path = ROOT / name
        require(path.is_file() and not path.is_symlink() and
                0 < path.stat().st_size <= MAX_SOURCE, "ordinary observer source: " + name)
        with path.open("rb") as stream:
            raw = stream.read(MAX_SOURCE + 1)
        require(0 < len(raw) <= MAX_SOURCE, "bounded observer source: " + name)
        records[name] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
    return records


def _capture_proof(value):
    require(type(value) is dict and value, "nonempty fresh original capture attribution")
    _public_values(value)
    _json_bytes(value, MAX_SOURCE)
    stack = [(value, False)]
    while stack:
        item, dirty = stack.pop()
        if type(item) is dict:
            for name in ("source_dirty", "dirty"):
                if name in item:
                    require(type(item[name]) is bool, "typed source dirtiness")
                    dirty = dirty or item[name]
            if dirty:
                require(all(item.get(key) in (None, "") for key in
                            ("tested_commit", "source_commit", "committed_source")),
                        "dirty capture cannot claim an exact source commit")
            stack.extend((entry, dirty) for entry in item.values())
        elif type(item) is list:
            stack.extend((entry, dirty) for entry in item)


def synthetic_jar(side):
    """Ordinary Path used only to rebind logical archive identity, never opened."""
    require(type(side) is str and side in HASHES, "known original side")
    return (Path(tempfile.gettempdir()).resolve() / "mc-population-full-world-save-public-proof" /
            (side + ".jar"))


def _original_shape(value, side):
    _keys(value, ("schema", "scope", "profile", "side", "seed_hex", "jar_sha256",
                  "construction_epoch_ms", "population", "world_save"),
          "complete original full World save observation")
    require(type(value["schema"]) is int and value["schema"] == 1 and
            value["scope"] == "original-game-population-full-world-save" and
            value["profile"] == PROFILE and value["side"] == side and
            value["seed_hex"] == SEED and value["jar_sha256"] == HASHES[side],
            "fixed original full World save identity")
    epoch = value["construction_epoch_ms"]
    require(type(epoch) is list and len(epoch) == 2 and
            all(type(item) is int and 0 <= item <= (1 << 63) - 1 for item in epoch) and
            epoch[0] <= epoch[1], "actual construction epoch interval")
    population = value["population"]
    _keys(population, ("schema", "scope", "side", "seed_hex", "jar_sha256", "callbacks", "states"),
          "complete unchanged origin population record")
    require(type(population["schema"]) is int and population["schema"] == 1 and
            population["scope"] == "original-game-normal-population" and
            population["side"] == side and population["seed_hex"] == SEED and
            population["jar_sha256"] == HASHES[side] and
            type(population["callbacks"]) is dict and
            type(population["states"]) is list and len(population["states"]) == 5,
            "unchanged five-boundary original origin profile")
    saved = value["world_save"]
    _keys(saved, ("schema", "scope", "full_world_save", "unload", "world_restart",
                  "bindings", "field_bindings", "class_origins", "initial", "time_api", "saves", "limits"),
          "complete original full World save record")
    require(type(saved["schema"]) is int and saved["schema"] == 1 and
            saved["scope"] == "original-game-held-world-full-save" and
            saved["full_world_save"] is True and saved["unload"] is False and
            saved["world_restart"] is False and type(saved["saves"]) is list and len(saved["saves"]) == 2,
            "held World two-full-save profile")
    maps = (population["callbacks"].get("class_origins"), saved["class_origins"])
    for origins in maps:
        require(type(origins) is dict and origins and
                all(type(name) is str and name and type(origin) is str
                    for name, origin in origins.items()), "complete original class origins")
    return maps


def _logical_original(value, side):
    value = copy.deepcopy(value)
    maps = _original_shape(value, side)
    urls = {origin for origins in maps for origin in origins.values()}
    require(len(urls) == 1 and all(origin.startswith("file:/") for origin in urls),
            "one exact supplied archive URL for every original class")
    for origins in maps:
        for name in origins:
            origins[name] = "supplied:" + side
    _public_values(value)
    return value


def _validated_original(value, side, comparison):
    maps = _original_shape(value, side)
    require(all(set(origins.values()) == {"supplied:" + side} for origins in maps),
            "logical side-specific archive identity in both original origin maps")
    _public_values(value)
    rebound = copy.deepcopy(value)
    jar = synthetic_jar(side)
    url = jar.resolve().as_uri().replace("file:///", "file:/", 1)
    for origins in _original_shape(rebound, side):
        for name in origins:
            origins[name] = url
    # Preserve the lossless observation even if a projection implementation
    # uses temporary edits while deriving its independent expected value.
    expected = comparison.project_original(copy.deepcopy(rebound), side, jar)
    _public_values(expected)
    _json_bytes(expected, MAX_RECORD)
    return rebound, expected


def export_package(originals, provenance):
    """Return an original-only package; caller owns capture proof and writing."""
    _keys(originals, HASHES, "both complete supplied-original sides")
    _capture_proof(provenance)
    comparison = _comparison()
    public = _support(comparison)
    sides = {}
    for side in HASHES:
        original = _logical_original(originals[side], side)
        _, expected = _validated_original(original, side, comparison)
        sides[side] = {"jar_sha256": HASHES[side],
                       "original": public.packed(_json_bytes(original, MAX_RECORD)),
                       "expected": public.packed(_json_bytes(expected, MAX_RECORD))}
        for name in ("original", "expected"):
            public.unpack(sides[side][name], MAX_RECORD)
    package = {"schema": 1, "scope": SCOPE, "profile": PROFILE, "seed_hex": SEED,
               "jar_hashes": dict(HASHES), "provenance": {
                   "seed_hex": SEED, "jdk_profile": JDK_PROFILE,
                   "sources": _source_records(), "capture": copy.deepcopy(provenance)},
               "sides": sides}
    serialize_package(package)
    return package


def serialize_package(package):
    """Return canonical bounded bytes without creating or replacing a file."""
    _public_values(package)
    return _json_bytes(package, MAX_PACKAGE)


def load_package(path, comparison):
    """Return {side: {original, expected, jar}} after original-only rederivation."""
    public = _support(comparison)
    path = Path(path)
    require(path.is_file() and not path.is_symlink() and
            0 < path.stat().st_size <= MAX_PACKAGE, "bounded ordinary full World save fixture")
    with path.open("rb") as stream:
        raw = stream.read(MAX_PACKAGE + 1)
    require(0 < len(raw) <= MAX_PACKAGE, "bounded complete full World save fixture bytes")
    package = public.strict_json(raw)
    _keys(package, ("schema", "scope", "profile", "seed_hex", "jar_hashes", "provenance", "sides"),
          "complete full World save fixture package")
    require(type(package["schema"]) is int and package["schema"] == 1 and
            package["scope"] == SCOPE and package["profile"] == PROFILE and
            package["seed_hex"] == SEED and package["jar_hashes"] == HASHES,
            "fixed public full World save profile and archive hashes")
    _public_values(package)
    provenance = package["provenance"]
    _keys(provenance, ("seed_hex", "jdk_profile", "sources", "capture"), "complete provenance")
    require(provenance["seed_hex"] == SEED and provenance["jdk_profile"] == JDK_PROFILE,
            "seed23 Java17 observer profile")
    _keys(provenance["sources"], SOURCES, "five original observer sources")
    for record in provenance["sources"].values():
        public.fingerprint(record, MAX_SOURCE)
    require(public.json_equal(provenance["sources"], _source_records()),
            "original observer source bytes/SHA256 differ from fixture attribution")
    _capture_proof(provenance["capture"])
    _keys(package["sides"], HASHES, "exact client and server records")
    result = {}
    for side, row in package["sides"].items():
        _keys(row, ("jar_sha256", "original", "expected"), "complete original side record")
        require(row["jar_sha256"] == HASHES[side], "pinned side archive hash")
        original = public.strict_json(public.unpack(row["original"], MAX_RECORD))
        rebound, expected = _validated_original(original, side, comparison)
        stored = public.strict_json(public.unpack(row["expected"], MAX_RECORD))
        _public_values(stored)
        require(public.json_equal(stored, expected),
                "stored projection does not derive exactly from original observation: " + side)
        result[side] = {"original": rebound, "expected": expected, "jar": synthetic_jar(side)}
    return result
