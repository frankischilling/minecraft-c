"""Validate actual ordinary/JDI records; snapshots are observations, never execution inputs."""
import copy
import csv
import json
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tests/reference/entity/player"))
from support import require


def expected(p, after):
    return dict(fuse=int(p["expected_fuse"] if after else p["fuse"]), previous=int(p["previous"]),
                attacking=bool(int(p["expected_attacking"] if after else p["attacking"])),
                dead=False, world_null=bool(int(p["null_world"])),
                direction=int(p["expected_direction"] if after else p["direction"]), powered=int(p["powered"]),
                dirty16=bool(after and int(p["expected_dirty"])), dirty17=False,
                dirty=bool(after and int(p["expected_dirty"])),
                x="4021000000000000", y="4050200000000000", z="4021000000000000")


def ndjson(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


def body_record(record, p):
    require(record["case"] == int(p["case"]) and record["scenario"] == p["scenario"], "actual case identity")
    require(record["before"]["state"] == expected(p, False), "declared original operand state")
    require(not record["default_entropy_rebound"] and not record["constructor"]["default_entropy_rebound"], "unmodified default entropy")
    require(record["world_access_list_size"] == 0, "actual empty WorldAccess List")
    raw = record["constructor"]["raw"]
    require((raw["state"]["fuse"], raw["state"]["previous"], raw["state"]["direction"], raw["state"]["powered"]) == (0, 0, -1, 0), "genuine GB raw constructor fields")
    require(record["constructor"]["math_before"] != record["constructor"]["math_after"], "actual LS constructor Math stream")
    if p["projection"] == "explosion":
        require(record["outcome"] == 1 and record["failure"] is None, "original full explosion genuinely returns")
        require(record["after"]["state"]["dead"] and record["after"]["state"]["attacking"], "original after-explosion stores separately retained")
        return
    require(record["after"]["state"] == expected(p, True), "original actual returned/null field oracle")
    require((record["outcome"], record["failure"]) == ((-1, "java.lang.NullPointerException") if p["projection"] == "null" else (1, None)), "actual source outcome")
    before, after = record["before"], record["after"]
    permitted = {"gb.a", "ii.e"}
    require({k: v for k, v in before["fields"].items() if k not in permitted} ==
            {k: v for k, v in after["fields"].items() if k not in permitted}, "all other actual inherited/concrete fields unchanged")
    for name in ("bounds", "own_seed", "world_seed", "math_seed", "box_cursor", "vector_cursor"):
        require(before[name] == after[name], "no body entropy/pool/bounds changes: " + name)


def selected_record(record, p):
    require(record["case"] == int(p["case"]) and record["before"] == expected(p, False) and
            record["after"] == expected(p, True) and record["sounds"] == int(p["sounds"]) and
            record["pc"] == int(p["pc"]), "selected original comparison receipt")


def validate(out, pins):
    with (HERE / "profiles.tsv").open(newline="") as source:
        profiles = list(csv.DictReader(source, delimiter="\t"))
    require(len(profiles) == 12, "twelve declared cases")
    normal_all = ndjson(out / "normal/target.ndjson")
    debug_all = ndjson(out / "debugger/target.ndjson")
    normal = [r for r in normal_all if r["event"] == "case"]
    debug = [r for r in debug_all if r["event"] == "case"]
    trace = ndjson(out / "debugger/checkpoints.ndjson")
    require(len(normal) == len(debug) == 12, "all unchanged original methods completed")
    for mode, rows in (("normal", normal_all), ("debugger", debug_all)):
        origins = [r for r in rows if r["event"] == "origin"]
        require(len(origins) == len(pins["classes"]), "whole primary class origin inventory")
        for row in origins:
            require(row["class"] in pins["classes"] and row["sha256"] == pins["classes"][row["class"]]["class_sha256"], "actual primary runtime resource pin")
            require(row["location"].endswith("/b1.7.3.jar"), "original archive runtime origin")
        loaded = set()
        for line in (out / mode / "class-init.log").read_text().splitlines():
            found = re.search(r"\[class,load\] (\S+) source: .*b1\.7\.3\.jar", line)
            if found:
                loaded.add(found[1])
        require(loaded == set(pins["classes"]), "every actual original runtime-loaded class pinned")
    methods = {(c, m["name"], m["descriptor"]): m for c, p in pins["classes"].items() for m in p["methods"]}
    live = [r for r in trace if r["event"] == "code-pin"]
    require(len(live) == 16, "sixteen selected original live method bodies")
    for row in live:
        source = methods[(row["class"], row["method"], row["descriptor"])]
        require((row["bytes"], row["sha256"]) == (source["code_bytes"], source["code_sha256"]), "read-only live Method.bytecodes pin")
    selected = []
    for index, p in enumerate(profiles):
        for record in (normal[index], debug[index]):
            body_record(record, p)
        events = [r for r in trace if r.get("case") == index]
        entries = [r for r in events if r["event"] == "component-entry"]
        require(len(entries) == 1 and entries[0]["state"] == expected(p, False), "actual component JDI entry")
        source_entries = [r for r in events if r["event"] == "entry" and r["site"]["class"] == "gb" and r["site"]["descriptor"] == "(Lsn;F)V"]
        require(len(source_entries) == 1, "exact actual protected GB invocation")
        entry = source_entries[0]
        require(entry["site"]["method"] == ("b" if p["unseen"] == "1" else "a"), "selected protected method")
        require(entry["arguments"][0] == (entries[0]["actor"] if p["target_self"] == "1" else None) and
                entry["arguments"][1] == {"F": p["distance_bits"]}, "actual ignored target/distance operand roles")
        sound = [r for r in events if r["event"] == "entry" and r["site"]["class"] == "fd" and r["site"]["descriptor"] == "(Lsn;Ljava/lang/String;FF)V"]
        require(len(sound) == int(p["sounds"]), "actual original fuse sound invocation count")
        if sound:
            require(sound[0]["state"] == expected(p, False), "sound precedes direction/fuse/attacking stores")
            require(sound[0]["arguments"] == [entries[0]["actor"], "random.fuse", {"F": "3f800000"}, {"F": "3f000000"}], "actual sound arguments")
            require(sound[0]["frames"][1] == {"class": "gb", "method": "a", "descriptor": "(Lsn;F)V", "pc": 55}, "sound source caller")
        require(not [r for r in events if r["event"] == "random" and r["phase"] == "body"], "no direct or pre-explosion body Random draw")
        doubles = [r for r in events if r["event"] == "random" and r["phase"] == "construction" and r["role"] == "math" and r["method"] == "nextDouble"]
        require(len(doubles) == 4, "actual four LS constructor Math returns, no replay")
        if p["projection"] == "explosion":
            prefix = [r for r in events if r["event"] == "explosion-prefix"]
            require(len(prefix) == 1, "actual FD explosion entry prefix")
            chosen = prefix[0]
            require(chosen["state"] == expected(p, True), "no K/attacking before explosion return")
            radius = "40c00000" if p["powered"] == "1" else "40400000"
            require(chosen["receiver"] == entries[0]["world"] and chosen["arguments"] ==
                    [entries[0]["actor"], {"D": "4021000000000000"}, {"D": "4050200000000000"}, {"D": "4021000000000000"}, {"F": radius}], "actual captured World/self/XYZ/radius roles")
            require(chosen["frames"][1] == {"class": "gb", "method": "a", "descriptor": "(Lsn;F)V", "pc": int(p["pc"])}, "actual explosion caller PC")
        else:
            chosen = {"state": debug[index]["after"]["state"]}
            if p["projection"] == "null":
                errors = [r for r in events if r["event"] == "exception" and r["site"]["class"] == "gb"]
                require(len(errors) == 1 and errors[0]["class"] == "java.lang.NullPointerException" and errors[0]["site"]["pc"] == 4, "actual first World field NPE")
            else:
                returns = [r for r in events if r["event"] == "return" and r["site"]["class"] == "gb" and r["site"]["descriptor"] == "(Lsn;F)V"]
                require(len(returns) == 1 and returns[0]["site"]["pc"] == int(p["pc"]), "actual selected GB return PC")
        record = dict(case=index, scenario=p["scenario"], before=entries[0]["state"], after=chosen["state"], sounds=len(sound), pc=int(p["pc"]), projection=p["projection"])
        selected_record(record, p)
        selected.append(record)
    # Negative controls exercise the actual comparison contract, not a count-only check.
    for cell, replacement in (("fuse", 999), ("direction", 0), ("attacking", True), ("previous", 999)):
        mutant = copy.deepcopy(selected[0]); mutant["after"][cell] = replacement
        rejected = False
        try:
            selected_record(mutant, profiles[0])
        except ValueError:
            rejected = True
        require(rejected, "negative comparison mutation rejected: " + cell)
    observations = dict(comparison_records=selected, ordinary_records=normal, debugger_records=debug,
                        live_code=live, trace_records=len(trace), negative_controls=4,
                        scope="Nine complete GB protected-body returns, actual NULL-World throwable, two actual FD explosion-entry prefixes. Full original explosion after-states are retained separately, never used as native completion. Default constructor entropy remains independent and unmodified.")
    (out / "observations.json").write_text(json.dumps(observations, indent=2) + "\n")
    return dict(cases=12, returned=9, source_null=1, explosion_prefixes=2, live_code=len(live), trace_records=len(trace), negative_controls=4)
