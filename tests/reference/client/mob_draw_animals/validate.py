"""Validate actual unchanged-original animal observations without execution replay."""
import collections
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlparse

from support import require


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    require(path.stat().st_size < 536870912, "bounded complete authored observation file")
    return [json.loads(line) for line in path.read_text().splitlines()]


def refs(value):
    if isinstance(value, dict):
        if set(value) == {"class", "identity"}:
            yield value
        else:
            for child in value.values():
                yield from refs(child)
    elif isinstance(value, list):
        for child in value:
            yield from refs(child)


def bridge(event):
    result = {}
    actual = set()
    for row in event["identity_bridge"]:
        require(row["identity"] not in result and row["actual"]["oid"] not in actual, "one-to-one actual source identity bridge")
        result[row["identity"]] = row["actual"]
        actual.add(row["actual"]["oid"])
    return result


def same_class(binary, source):
    dimensions = 0
    while source.endswith("[]"):
        dimensions += 1
        source = source[:-2]
    if not dimensions:
        return binary == source
    primitive = {"boolean": "Z", "byte": "B", "char": "C", "short": "S", "int": "I", "long": "J", "float": "F", "double": "D"}
    return binary == "[" * dimensions + primitive.get(source, "L" + source + ";")


def validate(out, pins, jar):
    modes, runtime = {}, {}
    for mode in ("normal", "debugger"):
        directory = out / mode
        modes[mode] = rows(directory / "target.ndjson")
        stdout = directory / ("target.stdout" if mode == "debugger" else "process.stdout")
        stderr = directory / ("target.stderr" if mode == "debugger" else "process.stderr")
        lines = stdout.read_text().splitlines()
        errors = stderr.read_text().splitlines()
        warnings = [line for line in lines + errors if line.startswith("WARNING: ")]
        require(len(warnings) == 4 and "terminally deprecated" in warnings[0] and
                "AnimalDrawTarget" in warnings[1] and "maintainers" in warnings[2] and "removed" in warnings[3],
                "exact declared JDK17 SecurityManager warning family")
        require(all(line.startswith("WARNING: ") for line in errors), "no undeclared source stderr")
        bindings = {}
        for line in lines:
            if line.startswith("BIND "):
                _, name, origin, size, digest = line.split()
                expected = pins["classes"][name]
                require(name not in bindings and int(size) == expected["class_bytes"] and
                        digest == expected["class_sha256"] and
                        Path(unquote(urlparse(origin).path)).resolve() == jar,
                        "actual original class bytes/JAR runtime origin")
                bindings[name] = {"origin": origin, "bytes": int(size), "sha256": digest}
            else:
                require(line.startswith("WARNING: ") or re.fullmatch(
                    r"\d+ (recipes|achievements)|ANIMAL_DRAW [0-7] 1 pool=\d+ nw=\d+|ORIGINAL_ANIMAL_DRAW complete blocked_network=\d+", line),
                    "only declared original stdout")
        require(set(bindings) == set(pins["classes"]), "complete selected runtime source bindings")
        initialized = set()
        for line in (directory / "class-init.log").read_text().splitlines():
            match = re.search(r"Initializing '([^']+)'", line)
            if match and match[1] in pins["jar_class_names"]:
                initialized.add(match[1])
        require(initialized <= set(pins["classes"]), "actual initialization closure is wholly pinned")
        runtime[mode] = {"bindings": bindings, "initialized_original_classes": sorted(initialized)}
        resources = [row for row in modes[mode] if row["event"] == "resource_input"]
        require(len(resources) == 8, "all eight authored resources")
        for row in resources:
            path = directory / "resources" / row["resource"]
            require(sha(path) == row["sha256"] and path.stat().st_size == row["bytes"] and
                    Path(unquote(urlparse(row["runtime_url"]).path)).resolve() == path.resolve(),
                    "actual selected authored class-resource bytes")
        require(not list((directory / "resources").rglob("*.class")), "no replacement source classes")
        guard = [row for row in modes[mode] if row["event"] == "network_guard"]
        require(len(guard) == 1 and all("java.net.InetAddress.getLocalHost" in item and "sun.font." in item and
                "ji.<init>" in item for item in guard[0]["blocked_attempts"]), "only declared Fontconfig hostname guard")
    draws = {mode: [row for row in values if row["event"] == "draw"] for mode, values in modes.items()}
    textures = {mode: [row for row in values if row["event"] == "texture_lookup"] for mode, values in modes.items()}
    require(len(draws["normal"]) == len(draws["debugger"]) == 8, "all eight genuine draws in both VMs")
    require(textures["normal"] == textures["debugger"] and len(textures["normal"]) == 7, "seven exact real upload misses/hits")
    for index, record in enumerate(textures["normal"]):
        require(record["index"] == index and record["name"] == record["cached_name"] and
                record["after"] == record["after_hit"] and record["gl_error"] == 0, "genuine cache hit preserves source state")
        after = record["after"]
        require(after["cache_size"] == index + 1 and not after["clamp"] and not after["blur"] and
                after["min"] == after["mag"] == 9728 and after["wrap_s"] == after["wrap_t"] ==
                (10496 if index == 2 else 10497), "actual sampler/flag/cache effects")
        require(len(bytes.fromhex(record["transfer_hex"])) == after["width"] * after["height"] * 4, "complete decoded source transfer")
    expected_pools = [288, 432, 432, 648, 648, 840, 840, 840]
    expected_parts = [12, 12, 12, 9, 9, 8, 8, 12]
    compiled = [12, 6, 6, 9, 9, 8, 8, 12]
    expected_kinds = ["dl", "wh", "wh", "bx", "bx", "ww", "ww", "dl"]
    checked = []
    for case, (a, b) in enumerate(zip(draws["normal"], draws["debugger"])):
        require(a["index"] == b["index"] == case and a["kind"] == b["kind"] == expected_kinds[case], "authored source sequence")
        for key in ("input", "outcome", "failure", "baseline_pixels", "pixels", "nw_position", "nw_words", "gl_error"):
            require(a[key] == b[key], "ordinary/JDI exact complete return/geometry/pixels: " + key)
        require(a["outcome"] == 1 and a["failure"] is None and a["gl_error"] == 0 and
                a["nw_position"] == (48 if case in (5, 6) else 432), "actual complete draw and shadow buffer")
        for phase in ("before", "after"):
            for key in ("graph", "parts", "pool", "pool_storage", "gl"):
                require(a[phase][key] == b[phase][key], "ordinary/JDI exact selected " + phase + "." + key)
        for record in (a, b):
            require(record["before"]["entropy"] == record["after"]["entropy"] and
                    record["before"]["actor"] == record["after"]["actor"] and record["before"]["camera"] == record["after"]["camera"],
                    "actual within-VM fields/streams unchanged; independent constructor contexts not equated")
            require(record["after"]["pool"] == expected_pools[case] and
                    len(record["after"]["parts"]) == expected_parts[case] and
                    sum(part["is_list"] for part in record["after"]["parts"]) == compiled[case], "actual visible-part compilation/cursor")
            for image in ("baseline_pixels", "pixels"):
                path = out / ("normal" if record is a else "debugger") / record[image]["file"]
                require(path.stat().st_size == 262144 and sha(path) == record[image]["sha256"], "actual complete RGBA artifact")
        checked.append({"case": case, "kind": a["kind"], "ordinary_JDI_selected_state_exact": True})
    require(draws["normal"][0]["after"]["parts"] == draws["normal"][7]["before"]["parts"] ==
            draws["normal"][7]["after"]["parts"], "same persistent Sheep model/list graph after other families")
    require(draws["normal"][1]["pixels"]["sha256"] == draws["normal"][2]["pixels"]["sha256"] and
            draws["normal"][3]["pixels"]["sha256"] == draws["normal"][4]["pixels"]["sha256"] and
            draws["normal"][0]["pixels"]["sha256"] == draws["normal"][7]["pixels"]["sha256"], "actual stable reuse/revisit rasters")
    require(draws["normal"][5]["pixels"]["sha256"] != draws["normal"][6]["pixels"]["sha256"], "actual changed-partial Chicken flap visible")
    events = rows(out / "debugger/checkpoints.ndjson")
    require([row["sequence"] for row in events] == list(range(len(events))), "complete debugger transport order")
    live = {}
    for event in events:
        if event["event"] == "code-pin":
            name = event["class"].replace(".", "/")
            method = next(m for m in pins["classes"][name]["methods"] if m["name"] == event["method"] and
                          m["descriptor"] == event["descriptor"])
            require(method["code_bytes"] == event["bytes"] and method["code_sha256"] == event["sha256"], "actual live unchanged original Code")
            live[(name, event["method"], event["descriptor"])] = event
    lists, case_records = [], []
    prior = {}
    for case in range(8):
        selected = [row for row in events if row.get("case") == case]
        require(not any(row["event"] in ("exception", "random", "random-constructor", "clock") for row in selected),
                "no active draw Throwable or entropy draws")
        start = bridge(next(row for row in selected if row["event"] == "draw-entry"))
        end = bridge(next(row for row in selected if row["event"] == "state-complete"))
        require(all(end.get(k) == v for k, v in start.items()) and all(start.get(k) == v for k, v in prior.items()),
                "actual JVM alias and temporal continuity across all species")
        prior = end
        for phase, actual in (("before", start), ("after", end)):
            for ref in refs(draws["debugger"][case][phase]):
                require(ref["identity"] in actual and same_class(ref["class"], actual[ref["identity"]]["class"]),
                        "every selected reference resolves to actual class/JVM identity")
        stack, batches, textures_for_case = [], [], []
        current = None
        for event in selected:
            if event["event"] == "entry":
                stack.append(event)
                if "batch" in event:
                    require(len(event["batch"]["words"]) == event["batch"]["nw.o"] == 8 * event["batch"]["nw.h"],
                            "complete actual model/shadow raw batch")
                    batches.append(event)
                    if current is not None:
                        current["batches"].append(event)
                location = event["location"]
                if location["class"] == "bw" and location["method"] == "a" and location["descriptor"] == "(Ljava/lang/String;)V":
                    textures_for_case.append({"helper": event, "lookup": None, "bind": None, "returned": False})
                if location["class"] == "ji" and location["method"] == "b" and location["descriptor"] in ("(Ljava/lang/String;)I", "(I)V"):
                    frames = event["source_frames"]
                    if len(frames) > 1 and frames[1]["class"] == "bw" and frames[1]["descriptor"] == "(Ljava/lang/String;)V":
                        require(textures_for_case, "real texture helper enclosing lookup/bind")
                        textures_for_case[-1]["lookup" if location["descriptor"] == "(Ljava/lang/String;)I" else "bind"] = event
            elif event["event"] == "return":
                require(stack and all(stack[-1]["location"][key] == event["location"][key] for key in ("class", "method", "descriptor")),
                        "actual nested source return order")
                entered = stack.pop()
                if entered["location"]["class"] == "bw" and entered["location"]["descriptor"] == "(Ljava/lang/String;)V":
                    textures_for_case[-1]["returned"] = True
            elif event["event"] == "gl-entry":
                if event["method"] == "glNewList":
                    require(current is None, "one actual open compiled list")
                    owner = next(s["receiver"] for s in reversed(stack) if s["location"]["class"] == "ps" and s["location"]["method"] == "d")
                    current = {"case": case, "list": event["arguments"][0], "mode": event["arguments"][1],
                               "part": owner, "gl_commands": [], "batches": []}
                if current is not None:
                    current["gl_commands"].append(event)
            elif event["event"] == "gl-return" and event["method"] == "glEndList":
                require(current is not None, "actual list publication")
                lists.append(current)
                current = None
        require(not stack and current is None, "complete source/list nesting at return")
        require(batches[-1]["batch"]["words"] == draws["normal"][case]["nw_words"], "actual final shadow batch")
        direct = 4 if case in (1, 2) else 1 if case in (0, 7) else 0
        require(len(textures_for_case) == direct and all(row["lookup"] and row["bind"] and row["returned"] for row in textures_for_case),
                "all actual direct living texture helpers, including false saddle passes")
        if case in (1, 2):
            require(all(row["lookup"]["arguments"] == ["/mob/saddle.png"] for row in textures_for_case),
                    "all four actual saddle lookups")
            references = [row["lookup"]["argument_references"][0] for row in textures_for_case]
            require(all(item == references[0] and item["class"] == "java.lang.String" for item in references),
                    "actual same interned saddle String identity")
        case_records.append({"case": case, "entry_bridge": start, "after_bridge": end, "batch_records": batches,
                             "direct_living_texture_calls": textures_for_case})
    require(len(lists) == 35 and all(len(row["batches"]) == 6 for row in lists), "all35 actual compiled parts/210 faces")
    require([len(row["batch_records"]) for row in case_records] == [73, 37, 1, 55, 1, 49, 1, 1], "actual first/reuse batch counts")
    devices = {mode: next(row for row in values if row["event"] == "device") for mode, values in modes.items()}
    require(devices["normal"] == devices["debugger"], "actual same source device")
    result = {"scope": "Finite genuine unchanged-original Sheep/Pig/Cow/Chicken first/reuse/revisit; no native parity.",
              "draw_records": draws["normal"], "debugger_draw_records": draws["debugger"], "texture_records": textures["normal"],
              "constructor_contexts": {mode: [row for row in values if row["event"] == "constructor"] for mode, values in modes.items()},
              "source_events": events, "list_records": lists, "case_records": case_records, "devices": devices, "runtime": runtime,
              "ordinary_debugger_checks": checked, "counts": dict(collections.Counter(row["event"] for row in events))}
    (out / "observations.json").write_text(json.dumps(result, indent=2) + "\n", newline="\n")
    return {"sha256": sha(out / "observations.json"), "draws": 8, "textures": 7, "lists": 35, "batches": 218,
            "live_Code": len(live), "event_counts": result["counts"], "initialized_classes": {mode: len(data["initialized_original_classes"]) for mode, data in runtime.items()}}
