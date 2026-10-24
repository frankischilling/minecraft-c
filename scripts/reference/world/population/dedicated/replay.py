#!/usr/bin/env python3
"""Run the production dedicated probe against lossless original-only records.

The child receives project-authored resources and measured constructor inputs.
It never receives the original graph, trace, expected arrays or saved files.
The fixture's source/classfile audit remains inherited evidence.
"""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
from pathlib import Path
import re
import sys
import traceback
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[5]
MAX_NATIVE = 64 * 1024 * 1024
RESOURCES = {
    "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
    "lang/stats_US.lang": b"", "achievement/map.txt": b"",
}


def load(name, filename):
    if name not in sys.modules:
        spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        spec.loader.exec_module(module)
    return sys.modules[name]


F = load("_dedicated_replay_fixtures", "fixtures.py")
C = F.comparison()
P = load("_dedicated_replay_process", "process.py")


def input_paths(executable):
    """Selected immutable replay inputs; this is not build-to-source proof."""
    paths = {"executable": P.ordinary(executable).resolve(strict=True),
             "python": Path(sys.executable).resolve(strict=True),
             "fixture": F.DEFAULT, "driver": Path(__file__).resolve(strict=True)}
    for directory in (ROOT / "scripts/reference", ROOT / "tests/tools/storage"):
        for path in sorted(directory.rglob("*.py")):
            paths[path.relative_to(ROOT).as_posix()] = path
    for name in ("tests/world/runtime/population/natural_probe.c",
                 "tests/world/runtime/population/dedicated_probe.h",
                 "tests/world/runtime/population/dedicated_state_probe.h",
                 "src/world/runtime/construction.c", "src/world/runtime/chunk_provider.c",
                 "src/core/java/random_profile.c"):
        paths[name] = ROOT / name
    manifest = ROOT / ".source-manifest.json"
    if manifest.is_file():
        paths["source_export_manifest"] = manifest
        exported = C.strict_json(manifest.read_bytes())
        for name, path in paths.items():
            if name not in ("executable", "python", "source_export_manifest") and path.is_relative_to(ROOT):
                relative = path.relative_to(ROOT).as_posix()
                C.need(relative in exported["files"] and P.fingerprint(path) == exported["files"][relative],
                       "selected replay input matches source export: " + relative)
    return paths


def write_resources(directory):
    directory = P.ordinary(directory)
    C.need(not directory.exists(), "new minimal resource directory")
    directory.mkdir()
    for name, raw in RESOURCES.items():
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as outgoing:
            outgoing.write(raw)
    return {name: F.fingerprint(raw) for name, raw in RESOURCES.items()}


def validate_final_disk(native, world_root):
    """Check complete actual post-close files, including the real session lock."""
    C.need(native["region_close_completed"] is True, "native region owners closed")
    files, directories = C.Comparison.disk(None, native["disk_after_close"], False)
    projected = {name: {"kind": "directory"} for name in directories}
    projected.update({name: {"kind": "file", **F.fingerprint(raw)} for name, raw in files.items()})
    actual = P.inventory(world_root)
    C.equal(actual, projected, "all actual post-close native files/directories match complete projection")
    C.equal(files[C.SESSION].hex(), native["live_session_token"], "actual final session file token")
    return actual


def run_native(executable, resources_dir, world_root, tape_path, output_path, work_dir, records,
               *, environment=None, label="native"):
    """Run only the real producer; callers supply resource and oracle provenance."""
    executable = P.ordinary(executable).resolve(strict=True)
    resources_dir = P.ordinary(resources_dir).resolve(strict=True)
    world_root, output_path, tape_path, work_dir = map(P.ordinary,
        (world_root, output_path, tape_path, work_dir))
    C.need(executable.is_file() and resources_dir.is_dir() and work_dir.is_dir(), "ordinary production inputs/work directory")
    C.need(not world_root.exists() and not output_path.exists(), "fresh native root and output")
    C.need(world_root.parent.is_dir() and output_path.parent.is_dir(), "existing owned native output parents")
    tape = C.bounded_read(tape_path, 65536)
    C.parse_tape(tape)
    outgoing, errors = P.run([executable, "server", str(resources_dir).encode("utf-8").hex(),
        str(world_root).encode("utf-8").hex(), output_path, F.PROFILE,
        str(tape_path).encode("utf-8").hex()], work_dir, label, records,
        P.environment() if environment is None else environment, seconds=120,
        stdout_limit=4 * 1024 * 1024, stderr_limit=4 * 1024 * 1024,
        watched_outputs={output_path: MAX_NATIVE})
    C.equal(outgoing.replace(b"\r\n", b"\n"), b"16 achievements\n151 recipes\n", "complete native bootstrap stream")
    C.equal(errors, b"", "empty native diagnostic stream")
    native = C.strict_json(C.bounded_read(output_path, MAX_NATIVE))
    C.equal((native["schema"], native["profile"], native["side"], native["passed"], native["failure"], native["region_close_completed"]),
            (2, F.PROFILE, "server", True, None, True), "successful schema2 production observation")
    C.need(type(native["operations"]) is list and len(native["operations"]) == 17, "complete native operation schedule")
    boundaries = [native["initial"], *[row["after"] for row in native["operations"]]]
    C.equal([row["ordinal"] for row in boundaries], list(range(18)), "18 actual native boundaries")
    C.equal(native["constructor_inputs"], {"bound": True, "count": 9, "consumed": 9, "failure": 0}, "actual constructor tape fully consumed")
    C.equal(native["reload_transport_guard"], {"identity_map": True, "complete": True,
            "saved_block_bytes_checked": 5 * 32768}, "complete actual reload transport guard")
    for state in boundaries:
        token = state["native_private"]["store_session_token"]
        C.need(type(token) is str and re.fullmatch(r"[0-9a-f]{16}", token) is not None, "actual inspected Store token")
        C.equal(token, native["live_session_token"], "same actual held Store token at every boundary")
    validate_final_disk(native, world_root)
    return native


def native_corruption_controls(original, native, trace, tape, *, legacy_original):
    """Mutate actual producer observations, never manufacture a native golden."""
    rejected = []
    controls = ("used_world_random", "dedicated_E", "missing_server_role", "relative_native_path",
                "scheduled_order", "saved_metadata_nbt", "actual_store_token", "constructor_input")
    for name in controls:
        changed, changed_tape = copy.deepcopy(native), tape
        if name == "used_world_random":
            later = native["operations"][-1]["after"]["world_random"]["value"]
            C.need(later != native["initial"]["world_random"]["value"], "actual used World Random changed")
            changed["initial"]["world_random"]["value"] = copy.deepcopy(later)
        elif name == "dedicated_E":
            changed["initial"]["dedicated"]["fields"]["E"] = True
        elif name == "missing_server_role":
            del changed["initial"]["server"]
        elif name == "relative_native_path":
            path = changed["initial"]["native_private"]["store_root"]["utf8_bytes"]
            path.update(length=5, values=b"world".hex())
        elif name == "scheduled_order":
            entries = changed["operations"][-1]["after"]["scheduled"]["E"]["entries"]
            C.need(len(entries) >= 2, "actual nonempty ordered queue control")
            entries[0], entries[1] = entries[1], entries[0]
        elif name == "saved_metadata_nbt":
            row = next(row for row in changed["operations"][6]["after"]["disk"]["entries"] if row["path"] == C.LEVEL)
            before_size = row["bytes"]
            plain = bytearray(C.decompress(bytes.fromhex(row["hex"]), True, 65536))
            key = b"\x04\x00\x0aRandomSeed"
            C.need(plain.count(key) == 1, "one actual typed metadata seed")
            plain[plain.index(key) + len(key) + 7] ^= 1
            encoder = zlib.compressobj(wbits=31)
            raw = encoder.compress(bytes(plain)) + encoder.flush()
            row.update(bytes=len(raw), hex=raw.hex())
            changed["operations"][6]["after"]["disk"]["bytes"] += len(raw) - before_size
        elif name == "actual_store_token":
            private = changed["initial"]["native_private"]
            private["store_session_token"] = f"{int(private['store_session_token'], 16) ^ 1:016x}"
        elif name == "constructor_input":
            lines = tape.decode("ascii").splitlines()
            u, sample = lines[2].split("\t")
            lines[2] = u + "\t" + str(int(sample) ^ 1)
            changed_tape = ("\n".join(lines) + "\n").encode("ascii")
        try:
            C.compare(original, changed, trace, changed_tape, legacy_original=legacy_original)
        except ValueError:
            rejected.append(name)
        else:
            raise ValueError("corruption control was accepted: " + name)
    return {"positive_comparisons": 1, "rejected": rejected, "negative_controls": len(rejected),
            "native_source": "actual production output only; mutations are never producer inputs"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    report = {"schema": 1, "scope": "finite original-only dedicated fixture versus fresh production native replay",
              "passed": False, "tested_commit": None, "qualification_complete": False,
              "jar_free": True, "java_free": True, "fixture_source_dirty": True,
              "resource_binding": "project-authored minimal translation resources",
              "source_attribution_limit": "Selected files and executable are pinned; external build/platform attestation remains separate.",
              "processes": []}
    work = None
    code = 1
    try:
        paths = input_paths(args.executable)
        report["inputs_before"] = P.input_snapshot(paths)
        case = F.load_package()
        base = P.ordinary(args.work_dir)
        base.mkdir(parents=True, exist_ok=True)
        work = base / ("capture-" + uuid.uuid4().hex)
        work.mkdir(exist_ok=False)
        C.need(not paths["executable"].is_relative_to(work), "native executable outside capture")
        resources, world, output, tape = work / "resources", work / "native-world", work / "native.json", work / "constructor-inputs.tsv"
        report["resources"] = write_resources(resources)
        with tape.open("xb") as outgoing:
            outgoing.write(case["constructor_inputs"])
        native = run_native(paths["executable"], resources, world, tape, output, work, report["processes"])
        report["comparison"] = C.compare(case["original_value"], native, case["trace"], case["constructor_inputs"], legacy_original=True)
        report["corruption_controls"] = native_corruption_controls(case["original_value"], native, case["trace"], case["constructor_inputs"], legacy_original=True)
        report["final_disk_inventory"] = validate_final_disk(native, world)
        report["native_output"] = P.fingerprint(output)
        report["fixture_payloads"] = F.PAYLOADS
        report["offline_original_validation"] = case["validation"]
        C.equal(P.inventory(resources), {".": {"kind": "directory"}, "achievement": {"kind": "directory"},
            "lang": {"kind": "directory"}, **{name: {"kind": "file", **record} for name, record in report["resources"].items()}}, "minimal resource files unchanged")
        report["inputs_after"] = P.input_snapshot(paths)
        C.equal(report["inputs_after"], report["inputs_before"], "all selected replay inputs unchanged")
        report["passed"], code = True, 0
    except BaseException as error:
        report["failure"] = f"{type(error).__name__}: {error}"
        report["traceback"] = traceback.format_exc()
        traceback.print_exc()
    finally:
        if work is not None:
            report["retained_files"] = P.inventory(work)
            with (work / "report.json").open("x", encoding="utf-8", newline="\n") as outgoing:
                json.dump(report, outgoing, indent=2, allow_nan=False)
                outgoing.write("\n")
            print("Capture:", work, flush=True)
        print("PASS" if report["passed"] else "FAIL", "dedicated original-only fixture replay", flush=True)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
