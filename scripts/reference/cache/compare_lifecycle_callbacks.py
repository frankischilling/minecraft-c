#!/usr/bin/env python3
"""Compare finite original detach calls with the generic C callback coordinator.

Decoded Boat/Sign data and all feature/serializer bodies are external. Only C
publication, activation, detachment, pending removal and callback order compute
the compared transitions. The original-only validator checks the full graphs.
"""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
COMMON_PATH = Path(__file__).with_name("compare_lifecycle.py").resolve()
COMMON_RAW = COMMON_PATH.read_bytes()
if len(COMMON_RAW) > 2 * 1024 * 1024:
    raise ValueError("bounded executing native support source")
COMMON_SPEC = importlib.util.spec_from_file_location("_lifecycle_callbacks_native", COMMON_PATH)
common = importlib.util.module_from_spec(COMMON_SPEC)
sys.modules[COMMON_SPEC.name] = common
exec(compile(COMMON_RAW, str(COMMON_PATH), "exec"), common.__dict__)

FIXTURE = ROOT / "tests/reference/cache/lifecycle_callbacks_fixtures.json"
FIXTURE_SHA256 = "763305cbd70129632ec079ae336a8d29dc5b721321e634898627d9bf088e9cc8"
MAX_OUTPUT = 32 * 1024 * 1024
SEAMS = {"load": "load", "register_tiles": "register_tiles", "register_entities": "register_entities",
         "external_tile_invalidate": "invalidate_tiles", "defer_entities": "defer_entities",
         "write": "save_main", "loader_extra": "save_extra", "loader_maintenance": "loader_maintenance",
         "provider_maintenance": "provider_maintenance"}
EXCLUSIONS = [
    "Boat/Sign decoding, registration, invalidation, serialization and disk writes are external bodies.",
    "The one Sign invalidation represents the finite whole tile callback; generic tile traversal is external.",
    "The raw loader result is imported before C acquisition; it is compared from its actual return onward.",
    "Blank fallback arrays are virtual in C; complete original arrays remain in the controlled capture.",
    "C local lease counts are constructed ownership checks, separate from original Java object retention.",
    "World lists/deferred lists and the dedicated entity registry still retain objects after unload."]
require = common.require


def project_capture(directory, controls):
    """Read original-only records; never read a native executable or response."""
    proof = controls.validate_capture(directory)
    require(proof["status"] == "pass" and proof["scenario"] == "detach-owned", "strict finite original profile")
    observed = common.strict_json(common.read(directory / "observed.json"))
    trace = common.strict_json(common.read(directory / "trace.json"))
    identities = {row["reference"]: row["object_id"] for row in trace["identity_ids"]}
    chunks = {row["id"]: row for row in observed["chunk_states"]}
    fallback = next(row for row in chunks.values() if row["blank"])
    normal = next(row for row in chunks.values() if not row["blank"])
    require(len(chunks) == 2 and normal["coordinates"] == [16, 0], "finite original retained owners")
    canonical = {identities[fallback["id"]]: 0, identities[normal["id"]]: 1}
    blobs = {}

    def header(reference):
        require(set(reference) == {"header_reference"}, "indexed original header")
        return trace["chunk_headers"][reference["header_reference"]]

    def full(reference):
        require(set(reference) == {"chunk_reference"}, "indexed original full payload")
        return trace["chunk_states"][reference["chunk_reference"]]

    def state(value):
        require(value["snapshot_kind"] == "all-retained-chunks-full", "complete finite event payloads")
        result = []
        for reference in value["chunks"]:
            payload = full(reference)
            row = header(payload["chunk"])
            identity = canonical[row["id"]]
            arrays = None
            if identity:
                arrays = {}
                for name, size in common.ARRAYS.items():
                    array = payload["arrays"][name]["data"]
                    blob = trace["blobs"][array["sha256"]]
                    raw = common.hex_bytes(blob["hex"], size)
                    require(common.fingerprint(raw) == {"bytes": array["bytes"], "sha256": array["sha256"]},
                            "original full array binding")
                    blobs[array["sha256"]] = blob
                    arrays[name] = {"bytes": size, "sha256": array["sha256"]}
            result.append({"id": identity, "coordinates": row["coordinates"], "blank": identity == 0,
                           "flags": row["flags"], "last_save_time": row["last_save"],
                           "minimum_height": row["minimum"] if identity else None, "arrays": arrays})
        return {"world_time": value["metadata"]["e"], "spawn": [value["metadata"]["b"], value["metadata"]["d"]],
                "world_force_load": value["world_force_load"], "provider_force_load": value["provider_force_load"],
                "saving_disabled": value["save_off"], "chunks": sorted(result, key=lambda row: row["id"]),
                "map": sorted([[row["key"], canonical[header(row["chunk"])["id"]]] for row in value["map"]["entries"]]),
                "insertions": [canonical[header(row)["id"]] for row in value["insertions"]["values"]],
                "pending": value["pending"]["values"]}

    slices = [identities[row["id"]] for row in normal["entities"]]
    operations = []
    raw_loader = None
    for operation in trace["operations"]:
        stack, callbacks = [], []
        ordinal = {"register_entities": 0, "defer_entities": 0}
        for event in operation["events"]:
            role = event["role"]
            if role not in SEAMS:
                continue
            if event["edge"] == "breakpoint" and event["code_index"] == 0:
                stack.append(event)
            elif event["edge"] == "return":
                require(stack and stack[-1]["role"] == role, "matched nested original callback")
                entry = stack.pop()
                part = None
                if role in ordinal:
                    part = ordinal[role]
                    ordinal[role] += 1
                    require(part < 8 and entry["arguments"] == [{"object_id": slices[part], "class": "java.util.ArrayList"}],
                            "exact original slice owner and ordered index")
                row = {"kind": SEAMS[role], "slice": part, "entry": state(entry["state"]),
                       "return": state(event["state"]),
                       "source": {"role": role, "entry_event": entry["ordinal"], "return_event": event["ordinal"],
                                  "entry_pc": entry["code_index"], "return_pc": event["code_index"]}}
                callbacks.append(row)
                if role == "load":
                    require(event["result"]["object_id"] == identities[normal["id"]], "raw result owner")
                    raw_loader = next(chunk for chunk in row["return"]["chunks"] if chunk["id"] == 1)
        require(not stack, "all finite external callbacks returned")
        operations.append({"action": operation["action"],
                           "before": state(operation["before"]["state"]), "after": state(operation["after"]["state"]),
                           "result": False if operation["action"] == "unload_external" else
                                     1 if operation["action"] == "acquire" else None,
                           "callbacks": callbacks})
    require(raw_loader is not None and raw_loader["last_save_time"] == "0000000000000000" and
            raw_loader["flags"]["c"] is False, "actual decoder result before stamping and activation")
    require([row["kind"] for row in operations[1]["callbacks"]] ==
            ["load", "register_tiles"] + ["register_entities"] * 8, "complete activation callback schedule")
    expected = ["invalidate_tiles"] + ["defer_entities"] * 8
    if trace["context"] == "dedicated":
        expected += ["save_main", "save_extra", "loader_maintenance", "provider_maintenance"]
    require([row["kind"] for row in operations[-1]["callbacks"]] == expected, "complete detachment callback schedule")
    return {"context": trace["context"], "jar_sha256": trace["jar_sha256"], "seed_hex": trace["seed_hex"],
            "source": {name: common.file_fingerprint(directory / name) for name in
                       ("manifest.json", "preparation.json", "observed.json", "control.json", "trace.json")},
            "control_sections": proof["sections"], "raw_loader": raw_loader, "operations": operations}, blobs


def words_flags(value):
    return "".join("1" if value[name] else "0" for name in "cnopq")


def requests(case, blobs):
    initial = case["operations"][0]["before"]
    blank = initial["chunks"][0]
    normal = case["raw_loader"]
    rows = [["OPEN", "dedicated" if case["context"] == "dedicated" else "base", initial["world_time"],
             *initial["spawn"], int(initial["world_force_load"]), int(initial["provider_force_load"] is True),
             int(initial["saving_disabled"]), *blank["coordinates"], words_flags(blank["flags"]), blank["last_save_time"]],
            ["CHUNK", *normal["coordinates"], words_flags(normal["flags"]), normal["last_save_time"],
             normal["minimum_height"], *[blobs[normal["arrays"][name]["sha256"]]["hex"] for name in common.ARRAYS]]]
    for operation in case["operations"]:
        action = operation["action"]
        if action == "time":
            rows.append(["TIME", operation["after"]["world_time"]])
        elif action == "acquire":
            rows.append(["ACQUIRE", 16, 0])
        elif action == "direct_detach_external":
            rows.append(["DETACH"])
        elif action == "drop_external":
            rows.append(["DROP", 16, 0])
        elif action == "unload_external":
            # Generic callbacks keep q unchanged for this actual one-Boat case.
            rows.append(["MAINTAIN"])
        else:
            raise common.Failure("unsupported finite operation: " + action)
    rows.append(["QUIT"])
    return b"".join(("\t".join(map(str, row)) + "\n").encode("ascii") for row in rows), rows


def run(executable, request, work, processes, *, rejection=False):
    require(len(request) <= 256 * 1024, "bounded complete finite native input")
    (work / "native.stdin").write_bytes(request)
    record = {"command": [str(executable)], "exit": None, "reaped": False,
              "timeout": False, "output_limited": False, "seconds_limit": 30, "stream_limit": MAX_OUTPUT}
    processes.append(record)
    started = time.monotonic()
    with (work / "native.stdin").open("rb") as incoming, (work / "native.stdout").open("w+b") as outgoing, \
            (work / "native.stderr").open("w+b") as errors:
        child = subprocess.Popen([str(executable)], stdin=incoming, stdout=outgoing, stderr=errors, cwd=work)
        record["pid"] = child.pid
        try:
            while child.poll() is None:
                if time.monotonic() - started >= 30:
                    record["timeout"] = True
                    break
                if max(os.fstat(outgoing.fileno()).st_size, os.fstat(errors.fileno()).st_size) > MAX_OUTPUT:
                    record["output_limited"] = True
                    break
                time.sleep(0.01)
        finally:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=10)
            record["exit"], record["reaped"] = child.returncode, True
            for name, stream in (("stdout", outgoing), ("stderr", errors)):
                size = os.fstat(stream.fileno()).st_size
                record[name + "_total_bytes"] = size
                record["output_limited"] |= size > MAX_OUTPUT
                stream.truncate(min(size, MAX_OUTPUT))
            record["seconds"] = round(time.monotonic() - started, 6)
    for name in ("stdin", "stdout", "stderr"):
        record[name] = common.file_fingerprint(work / ("native." + name))
    require(not record["timeout"] and not record["output_limited"], "bounded native callback process")
    if rejection:
        newline = b"\r\n" if os.name == "nt" else b"\n"
        require(record["exit"] == 1 and common.read(work / "native.stderr") ==
                b"lifecycle callback probe: invalid/unsupported input or bounded adapter failure" + newline,
                "native parser rejects unsupported input without sanitizer failure")
        return None
    require(record["exit"] == 0 and record["stderr"]["bytes"] == 0, "clean native callback process")
    output = common.read(work / "native.stdout", MAX_OUTPUT)
    lines = output.splitlines(keepends=True)
    require(all(len(line) <= 8 * 1024 * 1024 and line.endswith(b"\n") for line in lines), "complete bounded JSON lines")
    return [common.strict_json(line) for line in lines]


def compare_state(case, blobs, expected, actual, *, imported, available, retained, released, depth):
    common.keys(actual, ("policy", "time", "spawn", "world_force_load", "provider_force_load", "saving_disabled",
                         "imported", "load_available", "main_q_effect", "normal", "fallback", "map", "list",
                         "pending", "pending_membership", "leases"))
    require(actual["policy"] == ("dedicated" if case["context"] == "dedicated" else "base") and
            actual["imported"] is imported and actual["load_available"] is available and
            actual["main_q_effect"] is None, "exact native adapter mode/decoder seam")
    require(common.exact(actual["leases"], {"normal": depth, "fallback": 0, "retains": retained, "releases": released}),
            "balanced explicit local retention at callback boundary")
    require((actual["normal"] is not None) is imported, "exact imported external owner")
    wanted = {row["id"]: row for row in expected["chunks"]}
    for identity, key in ((0, "fallback"), (1, "normal")):
        if identity and not imported:
            continue
        row = actual[key]
        common.keys(row, ("id", "coordinates", "blank", "flags", "last_save_time", "minimum_height", "arrays"))
        original = wanted.get(identity, case["raw_loader"])
        require(common.exact({name: row[name] for name in row if name != "arrays"},
                             {name: original[name] for name in original if name != "arrays"}),
                "complete original/C chunk header at callback boundary")
        if identity:
            common.keys(row["arrays"], common.ARRAYS)
            for name in common.ARRAYS:
                require(type(row["arrays"][name]) is str and
                        row["arrays"][name] == blobs[original["arrays"][name]["sha256"]]["hex"],
                        "every original/C payload byte at callback boundary: " + name)
        else:
            require(row["arrays"] is None, "virtual blank array projection")
    if expected["provider_force_load"] is None:
        require(case["context"] != "dedicated" and actual["provider_force_load"] is False,
                "absent base-provider force field uses an ignored false C binding")
    projection = {"world_time": actual["time"], "spawn": actual["spawn"],
                  "world_force_load": actual["world_force_load"], "provider_force_load": actual["provider_force_load"],
                  "saving_disabled": actual["saving_disabled"], "map": actual["map"],
                  "insertions": actual["list"], "pending": actual["pending"]}
    if expected["provider_force_load"] is None:
        projection["provider_force_load"] = None
    require(common.exact(projection, {key: value for key, value in expected.items() if key != "chunks"}),
            "complete original/C graph and live values at callback boundary")
    require(type(actual["pending_membership"]) is list and
            common.exact(sorted(actual["pending_membership"]), sorted(expected["pending"])),
            "complete native pending membership and actual original iteration")


def compare(case, blobs, commands, responses):
    require(len(commands) == len(responses) == len(case["operations"]) + 3, "one complete response per request")
    retains, releases, callbacks, states = 0, 0, 0, 0
    for index, (command, response) in enumerate(zip(commands, responses)):
        common.keys(response, ("schema", "kind", "index", "command", "category", "status", "result", "returned_id",
                               "callbacks", "state"))
        verb = command[0]
        category = "acquire" if verb in ("ACQUIRE", "DROP") else "lifecycle" if verb in ("DETACH", "MAINTAIN") else "adapter"
        require(common.exact({name: response[name] for name in ("schema", "kind", "index", "command", "category", "status")},
                             {"schema": 1, "kind": "RESPONSE", "index": index + 1, "command": verb,
                              "category": category, "status": 0}), "exact successful native command correspondence")
        require(common.exact(response["result"], False if verb == "MAINTAIN" else None) and
                common.exact(response["returned_id"], 1 if verb == "ACQUIRE" else None), "exact native API outputs")
        require(type(response["callbacks"]) is list, "native complete callback list")
        if verb in ("OPEN", "CHUNK"):
            expected = case["operations"][0]["before"]
            wanted_callbacks = []
        elif verb == "QUIT":
            expected = case["operations"][-1]["after"]
            wanted_callbacks = []
        else:
            operation = case["operations"][index - 2]
            expected, wanted_callbacks = operation["after"], operation["callbacks"]
        require(len(response["callbacks"]) == 2 * len(wanted_callbacks), "every actual callback entry/return")
        lease_operation = verb in ("ACQUIRE", "DETACH", "MAINTAIN")
        for event_index, event in enumerate(response["callbacks"]):
            wanted = wanted_callbacks[event_index // 2]
            edge = "entry" if event_index % 2 == 0 else "return"
            kind = wanted["kind"]
            common.keys(event, ("index", "kind", "edge", "outcome", "chunk_id", "slice", "owner_matched",
                                "context_matched", "request_coordinates", "state"))
            outcome = None if edge == "entry" else "loaded" if kind == "load" else \
                      "false" if kind == "provider_maintenance" else "ok"
            require(common.exact({key: value for key, value in event.items() if key != "state"},
                                 {"index": event_index, "kind": kind, "edge": edge, "outcome": outcome,
                                  "chunk_id": None if kind in ("loader_maintenance", "provider_maintenance") else 1,
                                  "slice": wanted["slice"], "owner_matched": True, "context_matched": True,
                                  "request_coordinates": [16, 0] if kind == "load" else None}),
                    "actual original callback order/slice and stable C bindings")
            local = kind not in ("load", "loader_maintenance", "provider_maintenance")
            count = retains + (1 if local or verb == "MAINTAIN" and kind != "load" else 0)
            done = releases + (1 if verb == "MAINTAIN" and not local else 0)
            compare_state(case, blobs, wanted[edge], event["state"], imported=True,
                          available=kind == "load" and edge == "entry", retained=count, released=done, depth=int(local))
            states += 1
        if lease_operation:
            retains += 1
            releases += 1
        compare_state(case, blobs, expected, response["state"], imported=verb != "OPEN",
                      available=index in (1, 2), retained=retains, released=releases, depth=0)
        callbacks += len(wanted_callbacks)
        states += 1
    return {"responses": len(responses), "callback_pairs": callbacks, "full_states": states,
            "full_normal_arrays": 5 * (states - 1), "balanced_local_leases": retains}


def negative_controls(case, blobs, commands, responses):
    """Discriminating response corruptions; none supplies oracle expectations."""
    acquire = next(i for i, row in enumerate(responses) if row["command"] == "ACQUIRE")
    detach = next(i for i, row in enumerate(responses) if row["command"] in ("DETACH", "MAINTAIN"))
    mutations = {
        "callback-order": lambda rows: rows[acquire]["callbacks"].reverse(),
        "missing-empty-slice": lambda rows: rows[detach]["callbacks"].pop(2),
        "active-before-invalidation": lambda rows: rows[detach]["callbacks"][0]["state"]["normal"]["flags"].update(c=True),
        "local-lease": lambda rows: rows[detach]["callbacks"][0]["state"]["leases"].update(normal=0),
        "lease-total": lambda rows: rows[detach]["state"]["leases"].update(retains=99),
        "loader-stamp": lambda rows: rows[acquire]["callbacks"][1]["state"]["normal"].update(last_save_time="0000000000001770"),
        "slice-owner-binding": lambda rows: rows[acquire]["callbacks"][2].update(owner_matched=False),
        "payload-byte": lambda rows: rows[detach]["callbacks"][0]["state"]["normal"]["arrays"].update(heightmap="00" * 256),
        "unexpected-map-owner": lambda rows: rows[detach]["state"]["map"].append([123, 1]),
        "missing-array": lambda rows: rows[acquire]["state"]["normal"]["arrays"].pop("blocks"),
        "typed-status": lambda rows: rows[acquire].update(status=False),
        "retained-world-time": lambda rows: rows[detach]["callbacks"][0]["state"].update(time="0000000000000000"),
    }
    if case["context"] == "dedicated":
        mutations["typed-provider-result"] = lambda rows: rows[detach].update(result=0)
        drop = next(i for i, row in enumerate(responses) if row["command"] == "DROP")
        mutations["pending-membership"] = lambda rows: rows[drop]["state"].update(pending_membership=[])
        mutations["unload-map-removal"] = lambda rows: rows[detach]["state"]["map"].append([16, 1])
    rejected = []
    for name, mutate in mutations.items():
        candidate = copy.deepcopy(responses)
        mutate(candidate)
        try:
            compare(case, blobs, commands, candidate)
        except common.Failure:
            rejected.append(name)
        else:
            raise common.Failure("accepted native callback corruption: " + name)
    return rejected


def input_controls(executable, request, work, processes):
    rows = request.splitlines(keepends=True)
    variants = {"missing-chunk": rows[0] + b"QUIT\n", "trailing-command": request + b"TIME\t0\n",
                "trailing-whitespace": request + b" \n", "incomplete-final-line": b"".join(rows[:-1]) + b"QUIT",
                "missing-quit": b"".join(rows[:-1]),
                "missing-array": rows[0] + b"CHUNK\t16\t0\t00000\t0000000000000000\t0\nQUIT\n",
                "unknown-command": b"".join(rows[:2]) + b"UNKNOWN\nQUIT\n"}
    for name, raw in variants.items():
        directory = work / ("reject-" + name)
        directory.mkdir()
        run(executable, raw, directory, processes, rejection=True)
    return list(variants)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, action="append")
    parser.add_argument("--fixture", type=Path, default=FIXTURE)
    parser.add_argument("--export-fixture", type=Path)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args(argv)
    report = {"schema": 1, "scope": "finite-original-generic-lifecycle-callbacks", "status": "running",
              "tested_commit": None, "processes": [], "cases": [], "errors": [], "limitations": EXCLUSIONS}
    work, inputs = None, {}
    try:
        require(common.file_fingerprint(COMMON_PATH) == common.fingerprint(COMMON_RAW), "raw executing native support binding")
        if args.capture:
            path = Path(__file__).with_name("lifecycle_controls.py")
            controls = common.load("_lifecycle_callback_controls", path, expected=common.file_fingerprint(path))
            package = {"schema": 1, "scope": report["scope"], "C_executed_to_create_expectations": False,
                       "limitations": EXCLUSIONS, "cases": [], "blobs": {}}
            for directory in args.capture:
                case, blobs = project_capture(directory.resolve(), controls)
                package["cases"].append(case)
                package["blobs"].update(blobs)
        else:
            raw = common.read(args.fixture.resolve(), 8 * 1024 * 1024)
            require(common.fingerprint(raw)["sha256"] == FIXTURE_SHA256, "pinned original-only fixture")
            package = common.strict_json(raw)
        if args.export_fixture:
            require(args.capture and args.executable is None, "original-only export never executes C")
            with args.export_fixture.open("x", encoding="utf-8", newline="\n") as stream:
                stream.write(json.dumps(package, indent=2) + "\n")
            print(json.dumps({"status": "pass", "fixture": str(args.export_fixture),
                              **common.file_fingerprint(args.export_fixture)}))
            return 0
        require(args.executable is not None and args.work_dir is not None, "native executable and output required")
        executable = args.executable.resolve()
        work = common.fresh_work(args.work_dir, (executable, Path(sys.executable).resolve(), args.fixture.resolve()))
        sources = common.source_paths(True) + [Path(__file__).resolve(), ROOT / "tests/world/cache/lifecycle_callbacks_probe.c"]
        paths = {*sources, executable, Path(sys.executable).resolve()}
        if not args.capture:
            paths.add(args.fixture.resolve())
        else:
            paths.update(directory.resolve() / name for directory in args.capture for name in
                         ("manifest.json", "preparation.json", "observed.json", "control.json", "trace.json"))
        inputs = {str(path): common.file_fingerprint(path) for path in sorted(paths)}
        report["inputs_before"] = inputs
        require(inputs[str(COMMON_PATH)] == common.fingerprint(COMMON_RAW), "archived native support matches executed raw bytes")
        for path in sources:
            target = work / "source" / path.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(common.read(path, 2 * 1024 * 1024))
            require(common.file_fingerprint(target) == inputs[str(path)], "exact callback replay source archive")
        for index, case in enumerate(package["cases"]):
            directory = work / ("case-" + str(index))
            directory.mkdir()
            request, commands = requests(case, package["blobs"])
            responses = run(executable, request, directory, report["processes"])
            result = compare(case, package["blobs"], commands, responses)
            result["negative_controls"] = negative_controls(case, package["blobs"], commands, responses)
            report["cases"].append({"context": case["context"], **result})
            if index == 0:
                report["input_rejections"] = input_controls(executable, request, work, report["processes"])
        report["status"] = "pass"
    except Exception as error:
        report["status"] = "fail"
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        if work is not None:
            report["inputs_after"] = {}
            for path, expected in inputs.items():
                try:
                    actual = common.file_fingerprint(Path(path))
                    report["inputs_after"][path] = actual
                    require(actual == expected, "callback replay input preserved: " + path)
                except Exception as error:
                    report["status"] = "fail"
                    report["errors"].append(str(error))
            path = work / "report.json"
            path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print(json.dumps({"status": report["status"], "report": str(path), "errors": report["errors"]}))
        elif report["status"] == "fail":
            print(json.dumps({"status": "fail", "errors": report["errors"]}), file=sys.stderr)
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
