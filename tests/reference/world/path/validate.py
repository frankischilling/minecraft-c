"""Check original path results, object graphs, source failures and separate entropy contexts."""
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
MULT = 0x5deece66d
MASK = (1 << 48) - 1


def need(ok, message):
    if not ok:
        raise ValueError(message)


def rows(path):
    need(path.stat().st_size < 256 * 1024 * 1024, "bounded complete source journal")
    return [json.loads(line) for line in path.read_text().splitlines()]


def token(value):
    if isinstance(value, dict) and "class" in value:
        return value.get("identity", value.get("oid"))
    return None


def canonical_graph(graph, selected=None, identity_map=None):
    """Preserve every reachable class, value, array slot and alias, excluding VM-local IDs."""
    objects = {token(node): node for node in graph["objects"]}
    identities = {} if identity_map is None else identity_map
    queue, nodes, enqueued = [], [], set()

    def visit(value):
        if isinstance(value, list):
            return [visit(cell) for cell in value]
        if isinstance(value, dict):
            if "class" in value:
                key = token(value)
                if key not in identities:
                    identities[key] = len(identities)
                if key not in enqueued:
                    enqueued.add(key)
                    queue.append(key)
                return {"class": value["class"], "reference": identities[key]}
            return {key: visit(cell) for key, cell in sorted(value.items())}
        return value

    roots = {key: visit(value) for key, value in sorted(graph["roots"].items())
             if selected is None or key in selected}
    while queue:
        key = queue.pop(0)
        if key not in objects:
            continue
        node = objects[key]
        nodes.append({"class": node["class"], "reference": identities[key],
                      **{name: visit(value) for name, value in sorted(node.items())
                         if name not in ("class", "identity", "oid")}})
    return {"roots": roots, "objects": nodes}


def internal(seed, bits):
    seed = (seed * MULT + 11) & MASK
    return seed, seed >> (48 - bits)


def draw(seed, method, args):
    if method == "nextDouble":
        seed, a = internal(seed, 26)
        seed, b = internal(seed, 27)
        return seed, {"D": struct.pack(">d", ((a << 27) + b) / (1 << 53)).hex()}
    if method == "nextFloat":
        seed, value = internal(seed, 24)
        return seed, {"F": struct.pack(">f", value / (1 << 24)).hex()}
    if method == "nextInt":
        bound = args[0]
        while True:
            seed, value = internal(seed, 31)
            result = (bound * value) >> 31 if bound & (bound - 1) == 0 else value % bound
            if bound & (bound - 1) == 0 or value - result + bound - 1 < 1 << 31:
                return seed, result
    raise ValueError("Unqualified reached Random method " + method)


def validate(directory, pins):
    directory = Path(directory)
    normal = rows(directory / "normal/target.ndjson")
    debug = rows(directory / "debugger/target.ndjson")
    journal = rows(directory / "debugger/checkpoints.ndjson")
    profiles = [line.split("\t") for line in (HERE / "profiles.tsv").read_text().splitlines()[1:]]
    need(len(normal) == len(debug) == len(profiles), "complete authored profile transport")
    entries = [r for r in journal if r["event"] == "entry"]
    returns = [r for r in journal if r["event"] == "return"]
    codes = [r for r in journal if r["event"] == "code-pin"]
    exceptions = [r for r in journal if r["event"] == "exception"]
    starts = {r["case"]: r for r in journal if r["event"] == "active-entry"}
    ends = {r["case"]: r for r in journal if r["event"] == "active-return"}
    need(set(starts) == set(ends) == set(range(len(profiles))), "all original call lifetimes")
    for row in codes:
        method = next(m for m in pins["classes"][row["class"]]["methods"]
                      if (m["name"], m["descriptor"]) == (row["method"], row["descriptor"]))
        need((row["bytes"], row["sha256"]) == (method["code_bytes"], method["code_sha256"]),
             "actual reached original Code identity")
    need(len(codes) >= 50, "actual complete path/heap/map/vector source closure reached")
    known, last_u, last_nano = {}, None, None
    defaults = rng_count = 0
    for row in journal:
        event = row["event"]
        if event == "clock":
            if row["kind"] == "seedUniquifier":
                last_u, last_nano = int(row["value"], 16), None
            else:
                last_nano = int(row["value"], 16)
        elif event == "default-random":
            need(last_u is not None and last_nano is not None, "actual default Random U/nano pair")
            seed = (last_u ^ last_nano ^ MULT) & MASK
            need(row["seed"] == f"{seed:012x}", "actual default seed formula")
            known[row["oid"]] = seed
            last_u = last_nano = None
            defaults += 1
        elif event == "random":
            need(row["oid"] in known, "actual observed Random start state")
            seed, result = draw(known[row["oid"]], row["method"], row["arguments"])
            need(result == row["result"] and row["seed"] == f"{seed:012x}",
                 "independent actual constructor/active Random replay")
            known[row["oid"]] = seed
            rng_count += 1
    need(not [r for r in journal if r["event"] == "random" and r["phase"] == "path"],
         "these actual prepared path calls consume no Random stream")
    for index, (n, d, profile) in enumerate(zip(normal, debug, profiles)):
        need(n["case"] == d["case"] == index and n["input"] == d["input"] == profile,
             "exact authored input identity")
        need((n["outcome"], n["failure"]) == (d["outcome"], d["failure"]),
             "ordinary and debugged original outcome")
        for record in (n, d):
            need(not record["default_constructor_entropy_rebound"] and record["constructor_contexts"],
                 "raw original constructor context retained separately")
            for name in ("actor_fields", "actor_bounds", "target_fields", "target_bounds",
                         "own_seed", "target_seed", "world_seed", "math_seed", "box_cursor",
                         "loaded_entities"):
                need(record["before"][name] == record["after"][name],
                     "original path does not rewrite its actor/target/entropy inputs: " + name)
        for phase in ("before", "after"):
            need(canonical_graph(n[phase]["graph"]) == canonical_graph(d[phase]["graph"]),
                 "ordinary/JDI graph classes, fields, arrays and alias partition")
            actual = starts[index]["state"] if phase == "before" else ends[index]["state"]
            selected = ("world", "actor", "target", "provider_fallback", "result", "extra") if profile[1].startswith("FD_") else None
            need(canonical_graph(d[phase]["graph"], selected) == canonical_graph(actual["graph"], selected),
                 "actual suspended graph agrees with ordinary target observation")
            for name in ("own_seed", "target_seed", "world_seed", "math_seed", "box_cursor",
                         "vector_cursor", "loaded_entities", "cache_size"):
                need(d[phase][name] == actual[name], "actual suspended original state " + name)
        need(len(n["steps"]) == len(d["steps"]), "complete actual component operation steps")
        for a, b in zip(n["steps"], d["steps"]):
            need(a["step"] == b["step"], "component invocation order")
            if "graph" in a:
                need(canonical_graph(a["graph"]) == canonical_graph(b["graph"]),
                     "component intermediate source fields and arrays")
            else:
                left = {"roots": {"step": a}, "objects": n["after"]["graph"]["objects"]}
                right = {"roots": {"step": b}, "objects": d["after"]["graph"]["objects"]}
                need(canonical_graph(left) == canonical_graph(right), "actual popped/waypoint/finished step values")
        normal_graphs = [n["before"]["graph"], *[step["graph"] for step in n["steps"] if "graph" in step], n["after"]["graph"]]
        debug_graphs = [d["before"]["graph"], *[step["graph"] for step in d["steps"] if "graph" in step], d["after"]["graph"]]
        normal_ids, debug_ids = {}, {}
        need([canonical_graph(graph, identity_map=normal_ids) for graph in normal_graphs] ==
             [canonical_graph(graph, identity_map=debug_ids) for graph in debug_graphs],
             "ordinary/JDI cross-step object and backing-array identity continuity")
    for mode in ("normal", "debugger"):
        initialized = set(re.findall(r"Initializing '([^']+)'", (directory / mode / "class-init.log").read_text()))
        actual = initialized & set(pins["jar_class_names"])
        need(actual == set(pins["initialized_original_classes"]), "exact initialized original closure")
        bindings = [line.split() for line in (directory / mode / "target.stdout").read_text().splitlines()
                    if line.startswith("BIND ")]
        need(len(bindings) == len(pins["classes"]), "all actual class origins and resources observed")
        for _, name, length, digest in bindings:
            expected = pins["classes"][name]
            need((int(length), digest) == (expected["class_bytes"], expected["class_sha256"]),
                 "actual original runtime class resource")
        need(not {"net/minecraft/client/Minecraft", "yo", "vy", "px"} & actual, "no client device bootstrap")
    queries = [r for r in returns if r["location"]["class"] == "ew"
               and r["location"]["method"] in ("a", "e") and r["location"]["descriptor"] == "(III)I"]
    failures = [r for r in normal if r["outcome"] == -1]
    need({r["case"] for r in failures} == {13, 14, 15, 16, 45, 56, 57, 58, 59, 60, 63},
         "actual nullable/array/duplicate source controls")
    need({r["case"] for r in exceptions} == {r["case"] for r in failures},
         "actual source exception event for every failed profile")
    sites = {
        13: ("java.lang.NullPointerException", "fw", "a", "(Lsn;Lsn;F)Ldh;", 3),
        14: ("java.lang.NullPointerException", "fd", "a", "(Lsn;IIIF)Ldh;", 1),
        15: ("java.lang.NullPointerException", "fw", "a", "(Lsn;DDDF)Ldh;", 19),
        16: ("java.lang.NullPointerException", "fw", "a", "(Lsn;Lsn;F)Ldh;", 10),
        45: ("java.lang.ArrayIndexOutOfBoundsException", "ew", "e", "(III)I", 41),
        56: ("java.lang.ArrayIndexOutOfBoundsException", "dh", "a", "(Lsn;)Lbt;", 8),
        57: ("java.lang.NullPointerException", "dh", "a", "(Lsn;)Lbt;", 14),
        58: ("java.lang.NullPointerException", "dh", "<init>", "([Ld;)V", 11),
        59: ("java.lang.NullPointerException", "fw", "a", "(Lsn;IIILd;)I", 57),
        60: ("java.lang.IllegalStateException", "lf", "a", "(Ld;)Ld;", 16),
        63: ("java.lang.NegativeArraySizeException", "ew", "<init>", "(Lfd;IIIIII)V", 55),
    }
    for index, expected in sites.items():
        matched = [r for r in exceptions if r["case"] == index]
        need(len(matched) == 1, "one actual reached source failure")
        row = matched[0]
        need((row["class"], row["location"]["class"], row["location"]["method"],
              row["location"]["descriptor"], row["location"]["pc"]) == expected,
             "exact actual source exception class and bytecode instruction")
    coverage = {"profiles": len(profiles), "ordinary_returns": len(normal) - len(failures),
                "source_failures": len(failures), "class_pins": len(pins["classes"]),
                "initialized_original_classes": len(pins["initialized_original_classes"]),
                "selected_Code_pins": sum("code_bytes" in next(m for m in pins["classes"][r["class"]]["methods"]
                    if (m["name"], m["descriptor"]) == (r["method"], r["descriptor"])) for r in pins["selected_methods"]),
                "live_method_pins": len(codes), "original_entries": len(entries),
                "original_returns": len(returns), "EW_query_returns": len(queries),
                "default_Random_occurrences": defaults, "RNG_returns": rng_count,
                "constructor_contexts": sum(len(r["constructor_contexts"]) for r in normal),
                "exception_events": len(exceptions), "D_constructor_records": sum(r["location"]["class"] == "d" and r["location"]["method"] == "<init>" and "fields" in r for r in returns)}
    observed = {"path_records": normal, "debugger_path_records": debug,
                "finder_records": [ends[i] for i in range(len(profiles))],
                "query_records": queries, "source_records": sorted(entries + returns, key=lambda r: r["sequence"]),
                "exception_records": exceptions,
                "entropy_records": [r for r in journal if r["event"] in
                                    ("clock", "default-random", "construction-return", "random")],
                "coverage": coverage,
                "limits": ["Raw constructor/default and Math entropy remains unnormalized and separately observed.",
                           "These finite prepared searches use actual cached normal/Blank Chunks; no terrain-generation equivalence.",
                           "Ordinary FD wrapper internals are observed only through actual read-only JDI receiver fields.",
                           "Component calls enter genuine original private methods with authored operands; they do not establish every World wrapper branch.",
                           "Native ceilings, lifetime failures and unsupported descendants are separate from original null/partial path outcomes."]}
    (directory / "observations.json").write_bytes((json.dumps(observed, indent=2) + "\n").encode())
    return coverage
