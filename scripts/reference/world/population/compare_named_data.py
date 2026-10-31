"""Compare fresh native named MapData effects with complete original-only records.

Expected state is derived independently from the source observations. The
successful seed23 schedule covers two saves and a separate manager readback;
tracker updates, nondefault maps, source failure policies and restart remain open.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import zlib

from importlib.util import module_from_spec, spec_from_file_location

ROOT = Path(__file__).resolve().parents[4]
SPEC = spec_from_file_location("_named_data_original_comparison", ROOT / "scripts/reference/world/population/named_data_original.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing original-only named MapData projection")
A = module_from_spec(SPEC)
sys.modules[SPEC.name] = A
SPEC.loader.exec_module(A)
W, P, C = A.W, A.P, A.C
R = W.TRANSPORT.REPLAY
require, equal = A.require, A.equal
HASHES, SEED, PROFILE, SOURCES = A.HASHES, A.SEED, A.PROFILE, A.SOURCES
TRANSPORT = W.TRANSPORT
project_original = A.project_original
actual_disk = W.actual_disk
load, keys = P.load, P.keys
disk = P.disk
all_boundaries = A.all_boundaries
pin = C.CAPTURE.file_fingerprint


def operations(value):
    require(type(value["saves"]) is list and len(value["saves"]) == 2, "complete two-save named schedule")
    return [value["time_api"], value["map_constructor"], value["dirty_mark"], value["put"],
            *value["saves"], value["reader_constructor"], value["reader_load"], value["reader_cache_hit"]]


def boundaries(value):
    result = [value["initial"], *[row[which] for row in operations(value) for which in ("before", "after")]]
    equal(len(result), 19, "complete nineteen named boundaries")
    return result


class References:
    """One bijection for the complete reported named-object trajectory."""

    def __init__(self):
        self.native_to_original = {}
        self.original_to_native = {}
        self.classes = {}
        self.native_bindings = {}
        self.binding_owners = {}

    def manager(self, native, label):
        """Inspect the copy-owned native binding separately from handler roles."""
        P.keys(native, ("object", "handler_store", "native_storage_binding", "named_cache",
                        "ordered_list", "id_map"), label + ": actual native manager")
        binding, owner = native["native_storage_binding"], native["object"]
        P.keys(binding, ("ref", "class"), label + ": actual native path binding")
        P.keys(owner, ("ref", "class"), label + ": actual native manager owner")
        require(type(binding["ref"]) is str and
                re.fullmatch(r"n(?:0|[1-9][0-9]*)", binding["ref"]) is not None,
                label + ": native binding identity")
        P.integer(int(binding["ref"][1:]), 0, 127, "native path binding capacity")
        equal(binding["class"], "native.path_binding", "explicit native-only descriptor role")
        require(binding["ref"] not in self.native_to_original and binding["ref"] != owner["ref"],
                "native binding is distinct from every paired semantic owner")
        if owner["ref"] in self.native_bindings:
            equal(binding["ref"], self.native_bindings[owner["ref"]], "actual native binding retained")
        if binding["ref"] in self.binding_owners:
            equal(owner["ref"], self.binding_owners[binding["ref"]], "distinct managers own distinct native bindings")
        self.native_bindings[owner["ref"]] = binding["ref"]
        self.binding_owners[binding["ref"]] = owner["ref"]
        result = {key: value for key, value in native.items()
                  if key not in ("handler_store", "native_storage_binding")}
        # This is a checked source-handler correspondence, not the native a field.
        result["storage"] = native["handler_store"]
        return result

    def compare(self, native, original, label):
        if type(original) is dict and set(original) == {
                "object", "storage", "named_cache", "ordered_list", "id_map"}:
            native = self.manager(native, label)
        if type(original) is dict and set(original) == {"ref", "class"}:
            if original["ref"] is None:
                equal(original, {"ref": None, "class": None}, "source null reference")
                equal(native, None, label + ": native null reference")
                return
            P.keys(native, ("ref", "class"), label + ": complete actual native reference")
            require(type(native["ref"]) is str and re.fullmatch(r"n(?:0|[1-9][0-9]*)", native["ref"]) is not None,
                    label + ": canonical native observer identity")
            P.integer(int(native["ref"][1:]), 0, 127, "native named reference capacity")
            A.ref(original, original["class"])
            equal(native["class"], original["class"], label + ": corresponding actual class")
            left, right = native["ref"], original["ref"]
            require(left not in self.binding_owners, "paired semantic owner is not a native path binding")
            if left in self.native_to_original:
                equal(right, self.native_to_original[left], label + ": native owner equality retained globally")
                equal(native["class"], self.classes[left], label + ": native owner class retained globally")
            if right in self.original_to_native:
                equal(left, self.original_to_native[right], label + ": source owner equality retained globally")
            self.native_to_original[left] = right
            self.original_to_native[right] = left
            self.classes[left] = native["class"]
            return
        if type(original) is dict:
            P.keys(native, original, label)
            for key in original:
                self.compare(native[key], original[key], label + "." + key)
        elif type(original) is list:
            require(type(native) is list and len(native) == len(original), label + ": complete ordered entries")
            for index, (left, right) in enumerate(zip(native, original)):
                self.compare(left, right, label + "[" + str(index) + "]")
        else:
            equal(native, original, label)

    def summary(self):
        require(len(self.native_to_original) == len(self.original_to_native) <= 128,
                "one bounded global named-owner bijection")
        return [{"native": native, "original": original, "class": self.classes[native]}
                for native, original in sorted(self.native_to_original.items(), key=lambda item: int(item[0][1:]))]


def map_transport(value):
    P.keys(value, ("object", "name", "dirty", "x_center", "z_center", "dimension", "scale", "update_counter",
                   "colors", "trackers", "decorations", "tracker_cache"), "complete native MapData")
    colors = value["colors"]
    P.keys(colors, ("object", "data"), "complete native colors owner and data")
    raw = P.byte_array(colors["data"], 16384, "complete native MapData colors")
    result = dict(value)
    result["colors"] = {"object": colors["object"], "bytes": len(raw),
                        "sha256": hashlib.sha256(raw).hexdigest(), "hex": raw.hex()}
    return result




def gzip_tree(raw, label):
    require(type(raw) is bytes and 0 < len(raw) <= 4 * 1024 * 1024, label + ": complete bounded gzip bytes")
    stream = zlib.decompressobj(31)
    plain = stream.decompress(raw, 4 * 1024 * 1024 + 1)
    require(stream.eof and not stream.unused_data and not stream.unconsumed_tail
            and 0 < len(plain) <= 4 * 1024 * 1024, label + ": one complete bounded gzip member")
    return P.canonical_nbt(P.STORAGE.nbt(plain))


def compare_events(native, original, pairs, construction):
    left, right = operations(native), operations(original)
    common = {"before", "after", "epoch_ms", "status", "native_status"}
    extras = ({"value", "previous"}, {"name", "result"}, {"returned_void"},
              {"key", "value", "returned_void"},
              {"index", "force", "progress", "returned_void", "error"},
              {"index", "force", "progress", "returned_void", "error"},
              {"storage", "result"}, {"name", "requested_class", "requested_class_object", "result"},
              {"name", "requested_class", "requested_class_object", "result"})
    for index, (event, expected, fields) in enumerate(zip(left, right, extras)):
        P.keys(event, common | fields, "complete native event " + str(index))
        equal({key: event[key] for key in ("status", "native_status")},
              {"status": "ok", "native_status": 0}, "actual successful native API event")
        low, high = P.epoch(event["epoch_ms"])
        require(low >= construction[1], "native event follows its actual constructor interval")
        if index:
            require(low >= left[index - 1]["epoch_ms"][1], "chronological native measured events")
        equal(event["before"], native["initial"] if index == 0 else left[index - 1]["after"],
              "complete native chronological before/after continuity")
        for field in fields:
            if field == "error":
                equal(event[field], None, "native successful save has no source failure diagnostic")
            else:
                pairs.compare(event[field], expected[field], "event" + str(index) + "." + field)
    equal(left[-1]["after"], left[-1]["before"], "native cache hit preserves every captured field and byte")


def compare_original(native, original, public, jar):
    side = original["side"]
    P.keys(native, ("schema", "scope", "profile", "side", "seed_hex", "construction_epoch_ms", "population", "named_data"),
           "complete native named wrapper")
    equal({key: native[key] for key in ("schema", "scope", "profile", "side", "seed_hex")},
          {"schema": 1, "scope": "native-game-population-named-map-data", "profile": PROFILE,
           "side": side, "seed_hex": C.SEED}, "finite native named profile")
    P.epoch(native["construction_epoch_ms"])
    R.validate_native(native["population"], side, public, profile="origin")
    C.compare_original(native["population"], original["population"], public, jar, profile="origin")
    value, expected_value = native["named_data"], original["named_data"]
    P.keys(value, ("session_token", "initial", "time_api", "map_constructor", "dirty_mark", "put", "saves",
                   "reader_constructor", "reader_load", "reader_cache_hit", "failure"), "complete native named record")
    equal(value["failure"], None, "native producer completed without a reached unsupported/error path")
    P.raw_hex(value["session_token"], 8, "complete native held session token")
    actual_boundaries, original_boundaries = boundaries(value), boundaries(expected_value)
    expected = project_original(original, side, jar)["boundaries"]
    snapshots = [native["population"]["initial"], *[row["snapshot"] for row in native["population"]["operations"]]]
    states = list(original["population"]["states"])
    pairs, disks, infos, converted = References(), [], [], []
    retained = 0
    for index, (boundary, source, projection) in enumerate(zip(actual_boundaries, original_boundaries, expected)):
        P.keys(boundary, ("state", "collections", "map_data"), "complete native named boundary")
        state = boundary["state"]
        P.keys(state, ("snapshot", "info", "disk"), "complete native actual base boundary")
        snapshot = state["snapshot"]
        equal({key: snapshot[key] for key in ("context", "world")}, projection["cache"],
              "complete source/native cache, arrays, flags and queue boundary " + str(index))
        info = P.info(state["info"], side, original=False)
        equal(info, projection["info"], "all fifteen source/native live Info fields boundary " + str(index))
        equal(state["info"]["owner"], 0, "native producer's held Info logical owner tag")
        equal(state["info"]["owner"], actual_boundaries[0]["state"]["info"]["owner"], "actual native held Info owner retained")
        observed_disk = P.disk(state["disk"], original=False)
        P.session(observed_disk, native["construction_epoch_ms"], value["session_token"])
        retained += observed_disk["bytes"]
        require(retained <= 8 * 1024 * 1024, "complete cumulative native raw-file budget")
        equal([{key: entry[key] for key in ("path", "kind")} for entry in observed_disk["entries"]],
              [{key: entry[key] for key in ("path", "kind")} for entry in projection["disk"]["entries"]],
              "complete source/native file and directory inventory boundary " + str(index))
        equal({key: observed_disk[key] for key in ("files", "directories")},
              {key: projection["disk"][key] for key in ("files", "directories")}, "actual file and directory counts")
        require(type(boundary["map_data"]) is list and len(boundary["map_data"]) <= 256, "complete bounded native MapData inventory")
        maps = [map_transport(row) for row in boundary["map_data"]]
        pairs.compare({"collections": boundary["collections"], "map_data": maps},
                      {"collections": source["collections"], "map_data": source["map_data"]},
                      "complete named owner graph and MapData boundary " + str(index))
        converted.append(maps)
        snapshots.append(snapshot)
        states.append(projection["numerical"])
        disks.append(observed_disk)
        infos.append(info)
    transport = W.TRANSPORT.validate_transport(snapshots)
    equal({key: transport[key] for key in ("boundaries", "random_records", "array_records")},
          {"boundaries": 24, "random_records": 72, "array_records": 360}, "complete native numerical transport trajectory")
    C.compare_numerical_snapshots(snapshots, states)
    for snapshot in snapshots:
        for name in R.RANDOMS:
            P.copied_random(snapshot["provider"][name], original=False)
    compare_events(value, expected_value, pairs, native["construction_epoch_ms"])
    equal(len(pairs.native_bindings), 2, "both actual native manager path bindings observed")
    files = [P.disk_files(row) for row in disks]
    source_files = [P.disk_files(row["disk"]) for row in expected]
    for index in range(19):
        equal(set(files[index]), set(source_files[index]), "complete exact source/native filenames, including name/lookup distinction")
        equal(files[index][P.SESSION], files[0][P.SESSION], "native actual session bytes unchanged")
        if index < 10:
            equal(disks[index], disks[0], "native constructor/dirty/put/time preserve all disk bytes")
        else:
            equal(files[index][P.REGION], files[10][P.REGION], "native clean save and separate-reader steps preserve complete region")
            equal(files[index][A.NAMED_FILE], files[10][A.NAMED_FILE], "native clean save and readback preserve complete named gzip")
        if index >= 12:
            equal(disks[index], disks[12], "native separate reader construction/load/hit preserve every actual disk byte")
    equal(disks[11], disks[10], "continuous native second-save disk entry")
    equal(files[12][W.OLD_LEVEL], files[10][W.LEVEL], "native exact first metadata rotates to level.dat_old")
    P.region_records(W.filtered_region_disk(disks[0]), value["time_api"]["epoch_ms"], saved=False)
    native_records = P.region_records(W.filtered_region_disk(disks[10]), value["saves"][0]["epoch_ms"], saved=True)
    original_records = P.region_records(W.filtered_region_disk(expected[10]["disk"]), expected_value["saves"][0]["epoch_ms"], saved=True)
    equal(native_records, original_records, "complete source/native region sectors, compression and typed saved Chunk NBT")
    chunks = A.saved_chunks(disks[10], value["saves"][0]["epoch_ms"],
                            {key: actual_boundaries[9]["state"]["snapshot"][key] for key in ("context", "world")})
    native_named = A.named_nbt(files[10][A.NAMED_FILE], converted[9][0])
    equal(gzip_tree(files[10][A.NAMED_FILE], "native named file"),
          gzip_tree(source_files[10][A.NAMED_FILE], "original named file"), "complete seven typed source/native MapData NBT fields")
    metadata = []
    for save_index, (before, after) in enumerate(((9, 10), (11, 12))):
        observed = W.metadata(files[after][W.LEVEL], infos[before], value["saves"][save_index]["epoch_ms"])
        source = W.metadata(source_files[after][W.LEVEL], expected[before]["info"], expected_value["saves"][save_index]["epoch_ms"])
        equal(W.metadata_payload(observed), W.metadata_payload(source), "all thirteen typed metadata fields with independently measured clock")
        metadata.append({"native_last_played": observed["last_played"], "original_last_played": source["last_played"],
                         "size_on_disk": observed["size_on_disk"], "tags": observed["tags"],
                         "native_gzip": observed["gzip"], "original_gzip": source["gzip"]})
    return {"passed": True, "origin_states": 5, "named_boundaries": 19, "numerical_transport": transport,
            "global_named_owner_bijection": pairs.summary(),
            "native_path_bindings": dict(pairs.native_bindings),
            "handler_scope": "Source handler identity corresponds to the supplying World Store; each native manager owns its separately checked path binding.",
            "original_public_cache_match": True,
            "all_cache_info_and_numerical_fields_match": True, "complete_MapData_colors": True,
            "actual_default_MapData_values": 2, "full_world_saves": 2, "complete_saved_chunks": len(chunks),
            "typed_saved_chunk_arrays": 20, "native_named_file": native_named, "metadata": metadata,
            "within_run_rotation_and_clean_file_preservation": True, "captured_file_effects_verified": True,
            "native_retained_file_bytes": retained}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client", "server", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    work = None
    report = {"schema": 1, "scope": "seed23 genuine held GAME World named MapData, two full saves and separate manager readback",
              "profile": PROFILE, "passed": False, "tested_commit": args.tested_commit or None,
              "full_world_save": True, "world_restart": False, "unload": False,
              "nonempty_players": False, "nonempty_named_data": True,
              "whole_manager_equivalence": False, "nondefault_maps_qualified": False,
              "source_io_failure_policy_qualified": False, "whole_population_equivalence": False,
              "source_attribution_limit": "A tested commit requires clean matching source; selected executable/source/tool fingerprints do not attest the build or transitive libraries. Exact build-to-source qualification belongs to the external compiler audit.",
              "processes": [], "sides": {}}
    try:
        require(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit), "complete tested commit")
        require(not any(os.environ.get(name) for name in
                        ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")),
                "uninjected original Java environment")
        tools = {name: getattr(args, name).resolve() for name in ("executable", "java", "javac")}
        jars = {side: getattr(args, side).resolve() for side in HASHES}
        require(all(path.is_file() and not path.is_symlink() for path in (*tools.values(), *jars.values())),
                "ordinary selected tools and original JARs")
        for side, jar in jars.items():
            equal(C.CAPTURE.file_fingerprint(jar)["sha256"], HASHES[side], "pinned supplied original " + side)
        work = C.CAPTURE.capture_root(args.work_dir, [sys.executable, tools["java"], tools["javac"]])
        require(not tools["executable"].is_relative_to(work), "capture excludes selected executable")
        report.update(C.source_context(work, report["processes"]))
        require(not args.tested_commit or args.tested_commit == report["source_head"] and report["source_dirty"] is False,
                "tested commit requires matching clean source")
        paths = C.source_inputs(**tools, jars=jars)
        paths.update(named_data_driver=Path(__file__).resolve(),
                     named_original=Path(A.__file__).resolve(),
                     named_fixture_module=ROOT / "scripts/reference/world/population/named_data_fixtures.py",
                     persistence_driver=Path(P.__file__).resolve(),
                     strict_transport=Path(TRANSPORT.__file__).resolve(),
                     numerical_replay=Path(TRANSPORT.REPLAY.__file__).resolve(),
                     storage_transport_validator=ROOT / "tests/tools/storage/test_store.py")
        paths.update({name: ROOT / name for name in SOURCES})
        paths["named_probe"] = ROOT / "tests/world/runtime/population/named_data_probe.h"
        report["inputs"] = {name: {"path": str(path), **C.CAPTURE.file_fingerprint(path)} for name, path in paths.items()}
        report["python_version"] = sys.version
        classes = work / "classes"
        classes.mkdir()
        C.run([str(tools["javac"]), "--release", "17", "-d", str(classes),
               *[str(ROOT / source) for source in SOURCES]], work, "compile", report["processes"])
        report["helper_classes"] = C.file_inventory(classes)
        public = W.public_origins()
        for side, jar in jars.items():
            directory = work / side
            directory.mkdir()
            resources = directory / "resources"
            row = {"resources": C.extract_resources(jar, resources), "passed": False}
            report["sides"][side] = row
            original_path, output = directory / "original.json", directory / "native.json"
            C.run([str(tools["java"]), "-Xint", "-Xmx512m", "--add-opens", "java.base/java.util=ALL-UNNAMED",
                   "-cp", str(classes), "PopulationNamedDataObservation", str(jar), side, SEED,
                   str(directory / "original-world"), str(original_path), PROFILE],
                  work, side + "-original", report["processes"])
            original = C.read_json(original_path)
            expected = project_original(original, side, jar)
            C.run([str(tools["executable"]), side, str(resources).encode("utf-8").hex(),
                   str(directory / "native-world").encode("utf-8").hex(), str(output), PROFILE],
                  work, side + "-native", report["processes"])
            native = C.read_json(output)
            equal((work / (side + "-original.stdout")).read_bytes(), b"", "original stdout")
            equal((work / (side + "-native.stderr")).read_bytes(), b"", "native stderr")
            equal((work / (side + "-native.stdout")).read_bytes().replace(b"\r\n", b"\n"),
                  (work / (side + "-original.stderr")).read_bytes().replace(b"\r\n", b"\n"),
                  "complete original/native bootstrap console")
            comparison = compare_original(native, original, public[side], jar)
            equal(actual_disk(directory / "original-world"), expected["boundaries"][-1]["disk"],
                  "complete actual final original disk equals its captured bytes")
            final_native = all_boundaries(native["named_data"])[-1]["state"]["disk"]
            equal(actual_disk(directory / "native-world"), P.disk(final_native, original=False),
                  "complete actual final native disk equals its captured bytes")
            for name, fingerprint in row["resources"].items():
                equal(C.CAPTURE.file_fingerprint(resources / name), fingerprint, "original resource preserved: " + name)
            row.update(passed=True, full_world_saves=2, boundaries=24, named_boundaries=19,
                       comparison=comparison,
                       complete_typed_nbt=True, complete_metadata=True, exact_within_run_rotation=True,
                       actual_final_disks_verified=True,
                       original_output=C.CAPTURE.file_fingerprint(original_path),
                       native_output=C.CAPTURE.file_fingerprint(output))
            print("PASS: " + side + " populated held GAME World named MapData saves/readback", flush=True)
        equal(C.file_inventory(classes), report["helper_classes"], "compiled helper classes preserved")
        for name, path in paths.items():
            equal({"path": str(path), **C.CAPTURE.file_fingerprint(path)}, report["inputs"][name],
                  "comparison input preserved: " + name)
        report["inputs_unchanged"] = True
        report["files"] = C.file_inventory(work)
        report["passed"] = True
        return 0
    except Exception as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error), file=sys.stderr)
        return 1
    finally:
        if work is not None:
            if "files" not in report:
                try:
                    report["files"] = C.file_inventory(work)
                except Exception as error:
                    report["inventory_failure"] = str(error)
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("Capture: " + str(work), file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
