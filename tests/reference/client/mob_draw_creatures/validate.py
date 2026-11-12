"""Validate actual unchanged-original creature observations without execution replay."""
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


def creature_controls(events, draws):
    """Check entered source bodies separately from pre-draw observer getter calls."""
    def match(row, event, cls, name, descriptor=None):
        loc = row.get("location", {})
        return (row["event"] == event and loc.get("class") == cls and loc.get("method") == name and
                (descriptor is None or loc.get("descriptor") == descriptor))

    def model_fields(state, root):
        ref = state["graph"]["roots"][root]
        return None if ref is None else next(node["fields"] for node in state["graph"]["objects"]
                                             if node["identity"] == ref["identity"])

    hooks, work = [], []
    for case, draw in enumerate(draws["debugger"]):
        selected = [row for row in events if row.get("case") == case]
        skin = [row for row in selected if match(row, "return", "ls", "q_", "()Ljava/lang/String;")]
        require(len(skin) == 1 and skin[0]["result"] == draw["declared_getters"]["actual_skin"],
                "actual in-draw skin getter return, distinct from declared observer getter")
        entity_events = [row for row in selected if match(row, "entry", "ko", "a", "(Lls;FFF)V")]
        require(len(entity_events) == 1, "one actual empty KO Entity event on primary only")
        record = {"case": case, "skin_returns": skin, "entity_events": entity_events}
        kind = draw["kind"]
        if kind == "cn":
            colors = [row for row in selected if row["event"] == "gl-entry" and row["method"] == "glColor4f" and
                      any(frame["class"] == "yx" for frame in row["source_frames"])]
            require(len(colors) == 1 and colors[0]["arguments"] == [{"F": "3f800000"}] * 3 + [{"F": "00000000"}],
                    "actual YX in-pass color alpha +0")
            passes = [row for row in selected if match(row, "return", "yx", "a", "(Lcn;IF)Z")]
            require(len(passes) == 4 and [row["result"] for row in passes] == [True, False, False, False],
                    "actual Spider four typed pass returns")
            record.update(eyes_colors=colors, pass_returns=passes)
        elif kind == "uw":
            sizes = [row for row in selected if match(row, "return", "uw", "v", "()I")]
            require(sizes and all(row["result"] == int(draw["input"][4]) for row in sizes), "actual Slime size getter")
            stores = [row for row in selected if match(row, "entry", "gv", "a", "(Lko;)V")]
            require(len(stores) == 1, "one actual Slime secondary setter per pass0")
            for mode in ("normal", "debugger"):
                actual = draws[mode][case]
                require(actual["after"]["graph"]["roots"]["fur"] == actual["after"]["graph"]["roots"]["own_model"],
                        "actual retained GV.f aliases MJ own outer")
                primary, outer = model_fields(actual["after"], "primary"), model_fields(actual["after"], "own_model")
                if case == 2:
                    require(actual["before"]["graph"]["roots"]["fur"] is None and outer["ko.m"] == {"F": "00000000"} and
                            primary["ko.m"] != outer["ko.m"], "first Slime early KO copies skip actual NULL secondary")
                else:
                    require(primary["ko.m"] == outer["ko.m"] and primary["ko.n"] == outer["ko.n"],
                            "actual Slime secondary receives early flags on reuse")
            record.update(size_returns=sizes, secondary_stores=stores)
        elif kind == "xt":
            age = [row for row in selected if match(row, "return", "es", "b", "(Lxt;F)F")]
            require(len(age) == 1 and age[0]["result"] == draw["declared_getters"]["ES.b(XT,F)F"], "actual ES alternate age return")
            rotations = [row for row in selected if row["event"] == "gl-entry" and any(
                frame["class"] == "es" and frame["descriptor"] == "(Lxt;FFF)V" for frame in row["source_frames"])]
            require([row["method"] for row in rotations] == ["glTranslatef", "glRotatef", "glRotatef", "glRotatef", "glTranslatef"],
                    "actual ES alternate transform order")
            record.update(age_returns=age, rotation_gl=rotations)
        else:
            powered = [row for row in selected if match(row, "return", "gb", "s", "()Z")]
            swell = [row for row in selected if match(row, "return", "gb", "a_", "(F)F")]
            sine = [row for row in selected if match(row, "entry", "in", "a", "(F)F") and any(
                frame["class"] == "m" for frame in row["source_frames"])]
            color = [row for row in selected if match(row, "return", "m", "a", "(Lgb;FF)I")]
            require(len(powered) == 4 and all(row["result"] is False for row in powered), "four actual powered-false watcher getter returns")
            require(len(swell) == 2 and all(row["result"] == {"F": "00000000"} for row in swell), "two distinct actual zero-swell calls")
            require(len(sine) == 1 and sine[0]["arguments"] == [{"F": "00000000"}], "actual Creeper zero-swell IN sine still entered")
            require(len(color) == 1 and color[0]["result"] == 0, "actual Creeper zero color multiplier")
            record.update(powered_returns=powered, swell_returns=swell, sine_entries=sine, color_returns=color)
        require(not any(row["event"] == "gl-entry" and row["method"] == "glDisable" and row["arguments"] == [2977]
                        for row in selected), "source draw does not normalize away NORMALIZE carry")
        hooks.append(record)
        stack = []
        for row in selected:
            if row["event"] == "entry":
                stack.append(row)
            elif row["event"] == "return":
                entered = stack.pop()
                loc = entered["location"]
                if loc["class"] not in ("sc", "no", "wn", "il") or loc["method"] != "a" or loc["descriptor"] != "(FFFFFF)V":
                    continue
                inside = [event for event in selected if entered["sequence"] < event["sequence"] < row["sequence"]]
                pose = [event for event in inside if match(event, "entry", loc["class"], "b", "(FFFFFF)V") and event["receiver"] == entered["receiver"]]
                parts = [event for event in inside if match(event, "entry", "ps", "a", "(F)V")]
                cosine = [event for event in inside if match(event, "entry", "in", "b", "(F)F")]
                sine = [event for event in inside if match(event, "entry", "in", "a", "(F)F")]
                absolute = [event for event in inside if match(event, "math-entry", "java.lang.Math", "abs", "(F)F")]
                counts = (len(cosine), len(sine), len(absolute))
                require(len(pose) == 1 and counts == {"sc": (4, 4, 4), "no": (0, 0, 0), "wn": (0, 0, 0), "il": (4, 0, 0)}[loc["class"]],
                        "actual selected render has one virtual pose and exact entered math operations")
                require(len(parts) in ({"sc": (11,), "no": (1, 4), "wn": (9,), "il": (6,)}[loc["class"]]), "actual concrete part render count")
                work.append({"case": case, "render_entry": entered, "render_return": row, "pose_entries": pose,
                             "part_entries": parts, "cosine_entries": cosine, "sine_entries": sine, "absolute_entries": absolute})
    allocations = [row for row in events if row["event"] == "entry" and match(row, "entry", "ps", "<init>", "(II)V") and
                   any(frame["class"] == "no" for frame in row["source_frames"])]
    rendered = {row["receiver"]["oid"] for row in events if match(row, "entry", "ps", "a", "(F)V")}
    displaced = [row for row in allocations if row["receiver"]["oid"] not in rendered]
    require(len(allocations) == 6 and len(displaced) == 1, "actual NO constructor six PS allocations include one replaced unrendered cube")
    require(len(work) == 22, "all actual concrete primary/true-pass render work records")
    return hooks, work, {"allocations": allocations, "displaced_unrendered": displaced}


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
                "CreatureDrawTarget" in warnings[1] and "maintainers" in warnings[2] and "removed" in warnings[3],
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
                    r"\d+ (recipes|achievements)|CREATURE_DRAW (?:[0-9]|1[0-2]) 1 pool=\d+ nw=\d+|ORIGINAL_CREATURE_DRAW complete blocked_network=\d+", line),
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
        require(len(resources) == 7, "all seven authored resources")
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
    require(len(draws["normal"]) == len(draws["debugger"]) == 13, "all thirteen genuine draws in both VMs")
    require(textures["normal"] == textures["debugger"] and len(textures["normal"]) == 6, "six exact real upload misses/hits")
    for index, record in enumerate(textures["normal"]):
        require(record["index"] == index and record["name"] == record["cached_name"] and
                record["after"] == record["after_hit"] and record["gl_error"] == 0, "genuine cache hit preserves source state")
        after = record["after"]
        require(after["cache_size"] == index + 1 and not after["clamp"] and not after["blur"] and
                after["min"] == after["mag"] == 9728 and after["wrap_s"] == after["wrap_t"] ==
                (10496 if index == 5 else 10497), "actual sampler/flag/cache effects")
        require(len(bytes.fromhex(record["transfer_hex"])) == after["width"] * after["height"] * 4, "complete decoded source transfer")
    expected_pools = [528, 528, 648, 648, 864, 864, 864, 864, 864, 864, 864, 1008, 1008]
    expected_parts = [22, 22, 5, 5, 9, 9, 5, 5, 5, 5, 22, 14, 14]
    compiled = [22, 22, 5, 5, 9, 9, 5, 5, 5, 5, 22, 6, 6]
    expected_kinds = ["cn", "cn", "uw", "uw", "xt", "xt", "uw", "uw", "uw", "uw", "cn", "gb", "gb"]
    checked = []
    for case, (a, b) in enumerate(zip(draws["normal"], draws["debugger"])):
        require(a["index"] == b["index"] == case and a["kind"] == b["kind"] == expected_kinds[case], "authored source sequence")
        for key in ("input", "declared_getters", "outcome", "failure", "baseline_pixels", "pixels", "nw_position", "nw_words", "gl_error"):
            require(a[key] == b[key], "ordinary/JDI exact complete return/geometry/pixels: " + key)
        require(a["outcome"] == 1 and a["failure"] is None and a["gl_error"] == 0 and
                a["nw_position"] == (48 if expected_kinds[case] == "uw" else 192 if expected_kinds[case] == "gb" else 432), "actual complete draw and shadow buffer")
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
    require(draws["normal"][0]["after"]["parts"] == draws["normal"][10]["after"]["parts"],
            "persistent Spider list/part identities after mixed families")
    for case in (4, 10, 11):
        for record in (draws["normal"][case], draws["debugger"][case]):
            require(all(next(cap["enabled"] for cap in record[phase]["gl"]["caps"] if cap["cap"] == 2977)
                        for phase in ("before", "after")), "actual NORMALIZE cross-family carry")
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
    for case in range(13):
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
        direct = 1 if expected_kinds[case] == "cn" else 0
        require(len(textures_for_case) == direct and all(row["lookup"] and row["bind"] and row["returned"] for row in textures_for_case),
                "actual direct eyes living texture helper and return order")
        if direct:
            require(textures_for_case[0]["lookup"]["arguments"] == ["/mob/spider_eyes.png"], "actual eyes texture key")
        case_records.append({"case": case, "entry_bridge": start, "after_bridge": end, "batch_records": batches,
                             "direct_living_texture_calls": textures_for_case})
    require(len(lists) == 42 and all(len(row["batches"]) == 6 for row in lists), "all42 actual compiled parts/252 faces")
    require([len(row["batch_records"]) for row in case_records] == [133, 1, 31, 1, 55, 1, 1, 1, 1, 1, 1, 37, 1], "actual first/reuse batch counts")
    devices = {mode: next(row for row in values if row["event"] == "device") for mode, values in modes.items()}
    require(devices["normal"] == devices["debugger"], "actual same source device")
    hooks, model_work, no_constructor = creature_controls(events, draws)
    result = {"scope": "Finite genuine unchanged-original Spider/Slime/Squid/Creeper first/reuse/mixed draws; no native parity.",
              "draw_records": draws["normal"], "debugger_draw_records": draws["debugger"], "texture_records": textures["normal"],
              "constructor_contexts": {mode: [row for row in values if row["event"] in ("constructor", "constructor_before")] for mode, values in modes.items()},
              "source_events": events, "list_records": lists, "case_records": case_records, "devices": devices, "runtime": runtime,
              "hook_records": hooks, "model_work_records": model_work, "no_constructor_records": no_constructor,
              "ordinary_debugger_checks": checked, "counts": dict(collections.Counter(row["event"] for row in events))}
    (out / "observations.json").write_text(json.dumps(result, indent=2) + "\n", newline="\n")
    return {"sha256": sha(out / "observations.json"), "draws": 13, "textures": 6, "lists": 42, "batches": 265,
            "live_Code": len(live), "event_counts": result["counts"], "initialized_classes": {mode: len(data["initialized_original_classes"]) for mode, data in runtime.items()}}
