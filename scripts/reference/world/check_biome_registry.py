#!/usr/bin/env python3
"""Replay finite original Biome graph/query observations through actual C owners."""

import argparse
import importlib.util
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = ROOT / "tests/reference/world"
ORIGINAL_POLICY = b"POLICY\tUNTOUCHED_ORIGINAL_BOOTSTRAP\tEXPLICIT_MUTATION_CONTROLS\tNOT_FULL_WORLD_OR_GAME_STARTUP\n"
NATIVE_POLICY = b"POLICY\tNATIVE_TYPED_BIOME_BOOTSTRAP\tEXPLICIT_MUTATION_CONTROLS\tNOT_FULL_WORLD_OR_GAME_STARTUP\n"
# Pins are bound only after exporting an actual successful original capture.
EXPECTED = {
    "biome_registry_client.tsv": "6e1b81c2943b656c9b77de1a8ecf29a2add00b51c748431c6cb6d37f66bd1d25",
    "biome_registry_server.tsv": "dbfe09cef6fefec1679fabad7ce91529af348b69af753357908e5f0b7711ed00",
    "biome_registry_provenance.json": "e5ea91a280a46ff48795823855499afe413b51022a78ffb9c5054c91a89e006f",
    "biome_registry_requests.tsv": "8690023ba17bcc37b9499bb91172289d9b4e55bcb1d08f4f9923317a7ce66171",
}


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def compare(actual, expected, side, operations, recorder):
    require(actual.count(NATIVE_POLICY) == 1 and expected.count(ORIGINAL_POLICY) == 1,
            "exact distinct native/original producer policies")
    normalized = actual.replace(NATIVE_POLICY, ORIGINAL_POLICY, 1)
    counts = recorder.protocol(normalized, side, operations)
    require(normalized == expected, "complete native Biome graph/query stream differs from original")
    return counts


def rejection_controls(expected, side, operations, recorder):
    canonical = expected.replace(ORIGINAL_POLICY, NATIVE_POLICY, 1)
    rows = canonical.splitlines(keepends=True)
    controls = {}

    def mutate(kind, column, value):
        changed = rows.copy()
        index = next(i for i, row in enumerate(changed) if row.startswith(kind + b"\t"))
        fields = changed[index].rstrip(b"\n").split(b"\t")
        require(fields[column] != value, "nontrivial mutation premise")
        fields[column] = value
        changed[index] = b"\t".join(fields) + b"\n"
        return b"".join(changed)

    controls["biome_alias"] = mutate(b"NAMED", 3, b"NULL")
    controls["signed_byte"] = mutate(b"BIOME", 6, b"-128")
    controls["spawn_weight"] = mutate(b"RECORD", 4, b"11")
    controls["entity_class"] = mutate(b"ENTITY_CLASS", 3, b"wrongClass")
    controls["table_alias"] = mutate(b"TABLE_SLOT", 3, b"NULL")
    controls["dropped_table_slot"] = b"".join(row for i, row in enumerate(rows)
        if i != next(j for j, candidate in enumerate(rows) if candidate.startswith(b"TABLE_SLOT\t")))
    query = next(i for i, row in enumerate(rows) if row.startswith(b"QUERY\t") and b"\tRETURN\tR:" in row)
    changed = rows.copy()
    changed[query] = changed[query].rsplit(b"\t", 1)[0] + b"\tNULL\n"
    controls["selector_result"] = b"".join(changed)
    thrown = next(i for i, row in enumerate(rows) if row.startswith(b"QUERY\t") and b"\tTHROW\t" in row)
    changed = rows.copy()
    changed[thrown] = changed[thrown].rsplit(b"\t", 1)[0] + b"\tjava.lang.NullPointerException\n"
    controls["exception_kind"] = b"".join(changed)
    controls["missing_footer"] = b"".join(rows[:-1])
    controls["false_original_producer"] = expected
    rejected = []
    for label, candidate in controls.items():
        require(candidate != canonical, "nontrivial rejection control: " + label)
        try:
            compare(candidate, expected, side, operations, recorder)
        except ValueError:
            rejected.append(label)
        else:
            raise ValueError("corrupted result accepted: " + label)
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--resources-dir", type=Path)
    args = parser.parse_args()
    recorder_path = ROOT / "scripts/reference/world/capture_biome_registry.py"
    recorder = load("biome_replay_schema", recorder_path)
    support = load("biome_replay_process", recorder.PROCESS_HELPER)
    report = {"schema": "BIOME_REGISTRY_NATIVE_REPLAY_V1", "passed": False, "processes": [],
              "scope": "Finite live Biome graph, public helper, raw-bit selectors and declared mutations",
              "normalization": "Only exact producer-policy token; NULL/index status kinds use observed exception names",
              "limitations": ["No native Throwable identity or Java heap/GC/OOM equivalence",
                              "No full World startup, generation, population or persistence acceptance"]}
    work = None
    try:
        require(args.executable.is_file(), "native Biome probe missing")
        require(EXPECTED, "actual original fixtures not pinned")
        paths = {"executable": args.executable.resolve(), "checker": Path(__file__).resolve(),
                 "recorder": recorder_path, "observer": recorder.HELPER,
                 "support": recorder.PROCESS_HELPER, "exporter": ROOT / "scripts/reference/world/export_biome_registry.py"}
        paths.update({name: FIXTURES / name for name in EXPECTED})
        for directory in ("include", "src"):
            paths.update({"project/" + path.relative_to(ROOT).as_posix(): path
                          for path in (ROOT / directory).rglob("*")
                          if path.is_file() and path.suffix in (".c", ".h")})
        paths["probe"] = ROOT / "tests/world/biome/registry_probe.c"
        resources = None
        if args.resources_dir:
            resources = args.resources_dir.resolve(strict=True)
            require(resources.is_dir(), "imported resource directory")
            for name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"):
                source = resources / name
                require(source.is_file() and source.stat().st_size <= recorder.MAX_PROTOCOL,
                        "bounded imported resource: " + name)
                paths["resource/" + name] = source
        before = {key: support.file_fingerprint(path) for key, path in paths.items()}
        require(all(before[key]["sha256"] == pin for key, pin in EXPECTED.items()), "immutable original fixture pins")
        provenance = json.loads(paths["biome_registry_provenance.json"].read_text(encoding="ascii"))
        require(provenance["inputs"]["helper"] == before["observer"]
                and provenance["inputs"]["recorder"] == before["recorder"], "actual observer/recorder provenance")
        request = paths["biome_registry_requests.tsv"]
        operations = recorder.declarations(request.read_bytes())
        require(before["biome_registry_requests.tsv"] == provenance["inputs"]["requests"], "actual query declarations")
        work = support.capture_root(args.work_dir, [sys.executable])
        report["inputs_before"] = before
        for key, path in paths.items():
            if key.startswith("resource/"):
                destination = work / key
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(path.read_bytes())
                require(support.file_fingerprint(destination) == before[key], "retained resource bytes: " + key)
        results = {}
        for side in ("client", "server"):
            output = work / (side + ".tsv")
            command = [str(args.executable.resolve()), side, str(request), str(output)]
            if resources:
                command += ["--resources-dir", str(resources)]
            _, stderr = support.run(command, work=work, name=side, records=report["processes"],
                                    deadline=time.monotonic() + 35)
            require(not stderr, "native probe stderr must be empty")
            require(output.is_file() and output.stat().st_size <= recorder.MAX_PROTOCOL, "bounded explicit native protocol")
            raw = output.read_bytes()
            expected = paths["biome_registry_" + side + ".tsv"].read_bytes()
            results[side] = {"counts": compare(raw, expected, side, operations, recorder),
                             "protocol": support.fingerprint(raw),
                             "rejection_controls": rejection_controls(expected, side, operations, recorder)}
        report["results"] = results
        report["inputs_after"] = {key: support.file_fingerprint(path) for key, path in paths.items()}
        require(report["inputs_after"] == before, "native replay inputs unchanged")
        for key in before:
            if key.startswith("resource/"):
                require(support.file_fingerprint(work / key) == before[key], "retained resource unchanged: " + key)
        report["passed"] = True
        print("PASS: both Biome graphs, 6 snapshots, 3824 raw-bit queries and 10 rejection controls per side")
        return 0
    except (ValueError, OSError, KeyError, TypeError, support.Failure) as error:
        report["failure"] = type(error).__name__ + ": " + str(error)
        print("FAIL: " + report["failure"], file=sys.stderr)
        return 1
    finally:
        if work is not None:
            (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="ascii", newline="\n")


if __name__ == "__main__":
    raise SystemExit(main())
