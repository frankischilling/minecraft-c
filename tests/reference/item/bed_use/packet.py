"""Canonical finite LP observations and selected source trace; raw verification is private."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LP = "lp.a(Liz;Lgs;Lfd;IIII)Z@"


def need(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def read_json(path):
    path = Path(path)
    need(path.stat().st_size <= 4 * 1024 * 1024, "bounded JSON bytes")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def read_lines(path, limit=8 * 1024 * 1024):
    path = Path(path)
    need(path.stat().st_size <= limit, "bounded text bytes")
    return path.read_text(encoding="ascii").splitlines()


def profiles():
    finite = read_json(HERE / "finite.json")
    rows = finite["rows"]
    need(finite["schema"] == "PRIVATE_BED_PLACEMENT_FINITE_V1" and 0 < len(rows) <= 96,
         "finite LP input schema/count")
    need(len({r["name"] for r in rows}) == len(rows), "unique row names")
    expected = ["\t".join([r["name"], *map(str, r["values"])]) for r in rows]
    need(read_lines(HERE / "profiles.tsv") == expected, "exact authored profile text")
    return rows


def source_pins(jar):
    need(digest(jar) == JAR, "unchanged original archive")
    spec = importlib.util.spec_from_file_location("bed_placement_source_parser", ROOT / "tests/reference/client/controller/classfile.py")
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    pins = read_json(HERE / "source-pins.json")
    with zipfile.ZipFile(jar) as z:
        actual = {"client_jar_sha256": JAR, "classes": {
            name: reader.inspect_class(z.read(name + ".class")) for name in pins["classes"]}}
    need(pins == actual and len(actual["classes"]) == 15, "all original class and Code hashes")
    return actual


def rows(stdout):
    selected = [line for line in stdout.splitlines() if line.startswith("BED_PLACEMENT\t")]
    expected = profiles()
    need(len(selected) == len(expected), "complete finite row count")
    records = []
    for index, (line, p) in enumerate(zip(selected, expected)):
        r = json.loads(line.split("\t", 1)[1], object_pairs_hook=unique)
        need(type(r["index"]) is int and r["index"] == index and r["name"] == p["name"] and
             r["values"] == p["values"], "ordered actual input values")
        need(r["result"] in ("UNRETURNED", "true", "false") and
             r["exception"] in ("NONE", "java.lang.NullPointerException", "BedPlacementObservation$DeclaredCallbackFailure") and
             r["random_unchanged"] is True, "typed source completion and Random receipt")
        need(type(r["count_after"]) is int and -2147483648 <= r["count_after"] <= 2147483647,
             "signed source count")
        need(re.fullmatch("[0-9a-f]{8}", r["yaw_after"]) is not None, "float field raw word")
        need(len(r["changes"]) <= 8192 and len(r["notifications"]) <= 4096, "bounded full mutation receipt")
        records.append(r)
    return selected, records


def check_events(events, records):
    need(0 < len(events) <= 20000 and [e["seq"] for e in events] == list(range(len(events))),
         "complete passive event sequence")
    groups = [[] for _ in records]
    for e in events:
        need(type(e["profile"]) is int and 0 <= e["profile"] < len(records), "passive profile index")
        groups[e["profile"]].append(e)
    for r, group in zip(records, groups):
        need(group[0]["event"] == "begin" and group[-1]["event"] == "end", "bounded entered/returned call epoch")
        entries = [e for e in group if e["event"] == "entry"]
        need(sum(e["at"] == LP + "0" for e in entries) == 1, "one actual LP entry")
        direct = [e for e in entries if e["caller"].startswith(LP)]
        pcs = [int(e["caller"].split("@")[-1]) for e in direct]
        name = r["name"]
        if name.startswith("face_guard_"):
            need(pcs == [] and not any(e["event"].startswith("field-") for e in group),
                 "face rejection precedes Player/World/Stack reads")
        if name in ("first_air_blocked", "second_air_blocked", "first_support_false", "second_support_false"):
            last = {"first_air_blocked": 92, "second_air_blocked": 111,
                    "first_support_false": 126, "second_support_false": 147}[name]
            need(pcs == [37, *[p for p in (92, 111, 126, 147) if p <= last]], "air/support shortcircuit order")
        setters = [e for e in group if e["event"] == "return" and e["caller"] in (LP + "167", LP + "194")]
        stores = [e for e in group if e["event"] == "field-write"]
        if r["result"] == "true":
            need([e["caller"] for e in setters] == [LP + "167", LP + "194"], "both setters returned before LP success")
            need(len(stores) == 1 and stores[0]["at"] == LP + "205" and stores[0]["after"] == r["count_after"],
                 "actual late count205 store")
            if name in ("height_127", "remote_ceiling_both_false", "bed_id_zero_both_false"):
                need([e["value"] for e in setters] == [False, False], "both ordinary false writes are discarded")
            if name == "bed_id_zero_first_false_head_true":
                need([e["value"] for e in setters] == [False, True], "first ordinary false is discarded")
            if name.startswith("callback_head_"):
                need([e["value"] for e in setters] == [True, False], "head ordinary false is discarded")
        else:
            need(not stores, "failure/rejection does not reach count205")
        if r["exception"] != "NONE":
            exceptions = [e for e in group if e["event"] == "exception"]
            need(exceptions and exceptions[-1]["type"] == r["exception"], "actual source exception event")
            expected_pc = {"null_player":23,"null_all":23,"null_world":92,"null_stack":200,
                           "callback_fail_foot_notice":167,"callback_fail_head_notice":194}.get(name)
            if expected_pc is not None:
                need(any(LP + str(expected_pc) in e["frames"] for e in exceptions), "exact retained exception prefix")
    return groups


def verify_capture(directory):
    directory = Path(directory)
    report = read_json(directory / "report.json")
    need(report["status"] == "PASS" and report["original_fresh_vms"] == 2 and
         report["input_unchanged"] is True and report["helpers_unchanged"] is True,
         "complete two-VM private cohort")
    for path, sha in report["input_before"].items():
        need(digest(path) == sha, "frozen input file changed")
    values = []
    for run in report["runs"]:
        output = directory / run["directory"]
        for filename, pin in run["artifacts"].items():
            need(Path(filename).name == filename and digest(output / filename) == pin, "captured artifact changed")
        selected, records = rows((output / "target.stdout").read_text(encoding="ascii"))
        events = [json.loads(line, object_pairs_hook=unique) for line in read_lines(output / "events.jsonl")]
        check_events(events, records)
        values.append(selected)
    need(len(values) == 2 and values[0] == values[1] == read_lines(directory / "selected.txt"),
         "two complete unchanged-original selected row sets")
    return report


SOURCES = ("BedPlacementObservation.java", "BedPlacementJdi.java", "prepare_inputs.py", "capture.py",
           "packet.py", "test_packet.py", "README.md")
DATA = ("selected.txt", "source-journal.json", "finite.json", "profiles.tsv", "source-pins.json")
SELECTED = "d0e55cc30fce8d452874bfac65dae3ef2d62789f37ce4be14e105cf39b040819"
TRACE = "1236c9c87172e1002c93efa6c3de640e4e34ab684040f388dc31f35d916b8410"
EXECUTED_PRODUCERS = "9c18787d6626f5b9d45268a5fc91d4ac938aa234779c0ae9f765f93ca29e7e64"
EXECUTED_HELPERS = "194379c7773e82b9f60ce1beb9f11aced94cf8f2ca6112e4498b30d90242264f"
BINARY_INPUTS = "f0b6369ad6e29ff530f8bcc6f2915bf6e2c527c6efeec2a171a38b4226927bb9"
BINARY_INVENTORY = "3fefb031d6185338706d3ebbde5202eb716e0042209f56ce5c95c5741ca10fd7"


def map_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode("ascii")).hexdigest()


def binary_inputs(value):
    need(type(value) is dict and len(value) == 98 and
         all(type(n) is str and n.isascii() and n.startswith(("jdk/", "sdk/")) and
             ":" not in n and chr(92) not in n and all(p not in ("", ".", "..") for p in n.split("/")) and
             type(s) is str and re.fullmatch("[0-9a-f]{64}", s) for n, s in value.items()),
         "portable complete historical binary-input map")
    need(sum(n.startswith("jdk/") for n in value) == 94 and sum(n.startswith("sdk/") for n in value) == 4 and
         {"jdk/bin/java.exe", "jdk/bin/javac.exe", "jdk/lib/modules", "jdk/bin/server/jvm.dll"} <= set(value),
         "94JDK and4SDK historical binary roles")
    need(map_digest(sorted(value)) == BINARY_INVENTORY and map_digest(value) == BINARY_INPUTS,
         "immutable complete historical binary inventory and values")


def canonical_journal(events, live_code):
    """Lossless per-call projection; sequence/profile are represented by ordering."""
    calls = [{"index": i, "events": []} for i in range(72)]
    for e in events:
        calls[e["profile"]]["events"].append({k: v for k, v in e.items() if k not in ("seq", "profile")})
    return {"schema": "BED_LP_SELECTED_TRACE_V1", "calls": calls, "live_code": live_code}


def journal_events(value):
    need(type(value) is dict and set(value) == {"schema", "calls", "live_code"} and
         value["schema"] == "BED_LP_SELECTED_TRACE_V1", "canonical LP trace schema")
    calls = value["calls"]
    need(type(calls) is list and len(calls) == 72, "canonical72-call trace")
    shapes = {"begin": {"event"}, "end": {"event"},
              "entry": {"event", "at", "caller", "receiver", "args"},
              "return": {"event", "at", "caller", "value"},
              "field-read": {"event", "at", "field", "receiver", "value"},
              "field-write": {"event", "at", "field", "receiver", "before", "after"},
              "exception": {"event", "at", "type", "catch", "frames"}}
    events = []
    for index, call in enumerate(calls):
        need(type(call) is dict and set(call) == {"index", "events"} and type(call["index"]) is int and
             call["index"] == index and type(call["events"]) is list and 2 <= len(call["events"]) <= 128,
             "ordered bounded LP call trace")
        for e in call["events"]:
            need(type(e) is dict and e.get("event") in shapes and set(e) == shapes[e["event"]],
                 "typed canonical source event")
            events.append({"seq": len(events), "profile": index, **e})
    need(len(events) == 1438, "complete agreed selected trace")
    return events


def read_journal(directory=HERE):
    value = read_json(Path(directory) / "source-journal.json")
    events = journal_events(value)
    live = value["live_code"]
    need(type(live) is list and len(live) == 8, "eight original live Code pins")
    pins = read_json(Path(directory) / "source-pins.json")
    seen = set()
    for code in live:
        need(type(code) is dict and set(code) == {"class", "method", "descriptor", "bytes", "sha256"} and
             type(code["bytes"]) is int, "live Code projection schema")
        key = (code["class"], code["method"], code["descriptor"])
        need(key not in seen, "unique live method")
        seen.add(key)
        matches = [m for m in pins["classes"][code["class"]]["methods"] if
                   (m["name"], m["descriptor"]) == key[1:]]
        need(len(matches) == 1 and matches[0]["code_bytes"] == code["bytes"] and
             matches[0]["code_sha256"] == code["sha256"], "actual original live Code identity")
    return events


def read_reference(directory=HERE):
    """Portable canonical data; no raw attempt/process files or private path needed."""
    directory = Path(directory)
    p = read_json(directory / "provenance.json")
    expected_keys = {"schema", "archive_sha256", "original_fresh_vms", "rows_per_vm", "events_per_vm",
        "input_unchanged", "helpers_unchanged", "jdi_source_writes", "jdi_target_invocations",
        "jdi_redefinitions", "jdi_forced_returns", "source_effect_counters", "exercised_guard_denials",
        "canonical_sha256", "executed_producer_sha256", "current_reproduction_sha256",
        "canonical_exporter_sha256", "private_evidence_sha256", "helper_sha256", "binary_input_sha256", "vm_projections", "scope"}
    need(type(p) is dict and set(p) == expected_keys and p["schema"] == "BED_USE_CANONICAL_V1" and
         p["archive_sha256"] == JAR, "closed canonical LP provenance")
    for key, expected in (("original_fresh_vms", 2), ("rows_per_vm", 72), ("events_per_vm", 1438),
                          ("jdi_source_writes", 0), ("jdi_target_invocations", 0),
                          ("jdi_redefinitions", 0), ("jdi_forced_returns", 0)):
        need(type(p[key]) is int and p[key] == expected, "typed original qualification: " + key)
    need(p["input_unchanged"] is True and p["helpers_unchanged"] is True,
         "source/tool/helper continuity")
    binary_inputs(p["binary_input_sha256"])
    need(p["source_effect_counters"] == {"network": 0, "native": 0, "exec": 0, "denied_writes": 0} and
         all(type(v) is int for v in p["source_effect_counters"].values()) and
         p["exercised_guard_denials"] == {"connect": 1, "listen": 1, "native": 1, "exec": 1, "outside_write": 1} and
         all(type(v) is int for v in p["exercised_guard_denials"].values()), "actual guard/counter attestations")
    need(set(p["canonical_sha256"]) == set(DATA) and
         set(p["current_reproduction_sha256"]) == set(SOURCES), "exact canonical/current-source inventories")
    for group in ("canonical_sha256", "current_reproduction_sha256"):
        for name, sha in p[group].items():
            need(type(sha) is str and re.fullmatch("[0-9a-f]{64}", sha) and digest(directory / name) == sha,
                 "canonical or current reproduction source changed: " + name)
    historical = {"BedPlacementObservation.java", "BedPlacementJdi.java", "prepare_inputs.py", "capture.py",
                  "packet.py", "profiles.tsv", "finite.json", "source-pins.json"}
    need(set(p["executed_producer_sha256"]) == historical and
         all(type(s) is str and re.fullmatch("[0-9a-f]{64}", s) for s in p["executed_producer_sha256"].values()),
         "historical executed producers remain separately pinned")
    need(map_digest(p["executed_producer_sha256"]) == EXECUTED_PRODUCERS,
         "immutable complete executed-producer identity")
    helpers = {"BedPlacementJdi.class", "BedPlacementObservation.class", "BedPlacementObservation$1.class",
               "BedPlacementObservation$Observer.class", "BedPlacementObservation$DeclaredCallbackFailure.class"}
    need(type(p["helper_sha256"]) is dict and set(p["helper_sha256"]) == helpers and
         all(type(s) is str and re.fullmatch("[0-9a-f]{64}", s) for s in p["helper_sha256"].values()) and
         map_digest(p["helper_sha256"]) == EXECUTED_HELPERS, "immutable complete executed-helper identity")
    for name in ("BedPlacementObservation.java", "BedPlacementJdi.java"):
        need(digest(directory / name) == p["executed_producer_sha256"][name], "executed original observer changed")
    need(p["canonical_exporter_sha256"] == {"packet.py": p["current_reproduction_sha256"]["packet.py"]},
         "separate current canonical exporter identity")
    need(type(p["private_evidence_sha256"]) is dict and len(p["private_evidence_sha256"]) == 13 and
         all(type(n) is str and "/" not in n and chr(92) not in n and type(s) is str and
             re.fullmatch("[0-9a-f]{64}", s) for n, s in p["private_evidence_sha256"].items()),
         "private immutable evidence digest inventory")
    need(digest(directory / "selected.txt") == SELECTED and digest(directory / "source-journal.json") == TRACE,
         "exact historical observations and lossless agreed trace")
    selected, records = rows((directory / "selected.txt").read_text(encoding="ascii"))
    need(read_lines(directory / "selected.txt") == selected, "only canonical LP observations")
    events = read_journal(directory)
    check_events(events, records)
    projection = {"observations_sha256": SELECTED, "trace_sha256": TRACE, "rows": 72, "events": 1438,
                  "target_exit": 0, "timed_out": False, "capture_complete": True, "overflow": False,
                  "reader_errors": 0, "original_class_origins": 15, "live_code_methods": 8}
    need(type(p["vm_projections"]) is list and len(p["vm_projections"]) == 2 and
         all(type(v) is dict and v == projection and
             all(type(v[k]) is type(x) for k, x in projection.items()) for v in p["vm_projections"]),
         "two checked private VM projections attest the same canonical data")
    return records


if __name__ == "__main__":
    print("Canonical original LP packet:", len(read_reference()), "rows")
