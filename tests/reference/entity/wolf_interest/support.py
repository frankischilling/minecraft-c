"""Audit retained originals and compare only observed Wolf interest leaf values."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlparse
import zipfile

sys.dont_write_bytecode = True


def need(value, reason):
    if not value:
        raise AssertionError(reason)


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def local(name, root):
    value = str(name).replace("\\", "/")
    drive = re.match(r"^([A-Za-z]):/+(.*)$", value)
    if os.name != "nt" and drive:
        return Path("/mnt") / drive[1].lower() / drive[2]
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", value):
        return Path(value[5].upper() + ":/" + value[7:])
    path = Path(value)
    return path if path.is_absolute() else root / path


def check_pins(pins, reason):
    actual = {}
    for path, expected in pins.items():
        need(path.is_file() and sha(path) == expected, reason + ": " + str(path))
        actual[str(path)] = expected
    return actual


def public_inputs(reference):
    provenance = json.loads((reference / "provenance.json").read_text())
    need(provenance["schema"] == 1 and provenance["chronology"]["public_paths_were_capture_start_inputs"] is False,
         "post-capture export chronology is explicit")
    expected = provenance["reference_files"]
    need(expected and set(expected) == {p.name for p in reference.iterdir() if p.is_file()} - {"provenance.json"},
         "complete exact public reference inventory")
    for name, digest in expected.items():
        raw = (reference / name).read_bytes()
        need(b"\r" not in raw and hashlib.sha256(raw).hexdigest() == digest,
             "current LF reference input: " + name)
    return provenance, [reference / "provenance.json", *(reference / n for n in expected)]


def audit_original(packet, reference, root):
    provenance, public_paths = public_inputs(reference)
    pins = {p.resolve(): sha(p) for p in public_paths}
    meta = provenance["packet"]
    audit_path = packet / "final-current-audit.json"
    need(sha(audit_path) == meta["final_audit_sha256"], "immutable complete Original packet audit")
    audit = json.loads(audit_path.read_text())
    need(audit["status"] == "PRIVATE_ORIGINAL_WOLF_INTEREST_CURRENT_AUDIT_PASS", "actual Original current audit status")
    manifest = audit["file_sha256"]
    need(len(manifest) == meta["whole_packet_files"] == 76 and
         {p.relative_to(packet).as_posix() for p in packet.rglob("*") if p.is_file()} == set(manifest) | {"final-current-audit.json"},
         "exact complete Original76 files plus audit, no omitted consumed artifact")
    for name, digest in manifest.items():
        path = (packet / name).resolve()
        need(path.is_relative_to(packet.resolve()) and path.is_file() and sha(path) == digest,
             "whole retained Original artifact: " + name)
        pins[path] = digest
    pins[audit_path.resolve()] = meta["final_audit_sha256"]
    need(sha(packet / "analysis-b.json") == meta["analysis_b_sha256"], "unchanged original analysis")
    receipts = {}
    for mode in ("a", "b"):
        base = packet / ("attempt-" + mode)
        path = base / "receipt.json"
        expected = meta["captures"][mode]
        need(sha(path) == expected["receipt_sha256"], "immutable original actual capture " + mode)
        data = json.loads(path.read_text())
        need(data["status"] == "OBSERVED_NINE_WOLF_INTEREST_CPU_CASES" and data["inputs_unchanged"] and data["helpers_unchanged"],
             "actual ordinary/read-only-JDI completion " + mode)
        before, after = data["input_before_sha256"], data["input_after_sha256"]
        need(len(before) == expected["current_inputs"] == 18 and before == after,
             "exact original eighteen input schema " + mode)
        for name, digest in before.items():
            actual = local(name, root).resolve()
            need(actual.is_file() and sha(actual) == digest, "recorded current Original input: " + name)
            need(actual not in pins or pins[actual] == digest, "consistent recorded input pin")
            pins[actual] = digest
        helpers = data["helper_before_sha256"]
        need(len(helpers) == expected["current_helpers"] == 5 and helpers == data["helper_after_sha256"],
             "actual compiled helper fingerprints " + mode)
        if mode == "b":
            need(helpers == provenance["compiled_helpers_b_sha256"], "exact qualified b compiled helper inventory")
        for name, digest in helpers.items():
            path = base / "classes" / name
            need(sha(path) == digest, "current actual compiled helper " + mode + ":" + name)
            pins[path.resolve()] = digest
        artifacts = data["artifacts_sha256"]
        need(len(artifacts) == expected["capture_artifacts"] == 24 and
             {p.relative_to(base).as_posix() for p in base.rglob("*") if p.is_file()} == set(artifacts) | {"receipt.json"},
             "exact actual capture artifact inventory " + mode)
        for name, observed in artifacts.items():
            path = base / name
            need(sha(path) == observed["sha256"] and path.stat().st_size == observed["bytes"],
                 "actual capture artifact bytes " + mode + ":" + name)
        receipts[mode] = data
    for name, binding in provenance["captured_export_mapping"].items():
        raw = packet / binding["captured_path"]
        normalized = raw.read_bytes().replace(b"\r\n", b"\n")
        need(sha(raw) == binding["captured_sha256"] and (reference / name).read_bytes() == normalized and
             sha(reference / name) == binding["export_sha256"], "exact sole CRLF-to-LF export: " + name)
    spec = importlib.util.spec_from_file_location("wolf_classfile_audit", reference / "classfile.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    authority = json.loads((reference / "source-pins.json").read_text())
    need(len(authority["classes"]) == 747 and sum("code_sha256" in m for c in authority["classes"].values() for m in c["methods"]) == 5709,
         "whole archive class/every-Code metadata")
    b = receipts["b"]
    jar_names = [local(n, root) for n in b["input_after_sha256"] if local(n, root).name == "b1.7.3.jar"]
    need(len(jar_names) == 1 and sha(jar_names[0]) == authority["jar_sha256"], "actual pinned original archive")
    with zipfile.ZipFile(jar_names[0]) as jar:
        names = {n[:-6] for n in jar.namelist() if n.endswith(".class")}
        need(names == set(authority["classes"]), "exact archive class inventory")
        for name, expected in authority["classes"].items():
            need(inspector.inspect_class(jar.read(name + ".class")) == expected, "actual original every-Code: " + name)
    helpers_code = json.loads((reference / "compiled-helper-Code-pins.json").read_text())
    need(set(helpers_code) == set(provenance["compiled_helpers_b_sha256"]), "complete authored helper Code schema")
    for name, expected in helpers_code.items():
        need(inspector.inspect_class((packet / "attempt-b/classes" / name).read_bytes()) == expected,
             "actual unchanged compiled helper Code " + name)
    runtime = json.loads((reference / "runtime-class-Code-pins.json").read_text())
    events = [json.loads(line) for line in (packet / "attempt-b/debugger/checkpoints.ndjson").read_text().splitlines()]
    live = [{k: e[k] for k in ("class", "method", "descriptor", "bytes", "sha256")} for e in events if e["event"] == "code-pin"]
    need(len(live) == 1945 and live == runtime["live_Code"], "exact actually prepared live Code metadata")
    for row in live:
        owner = row["class"].replace(".", "/")
        method = [m for m in authority["classes"][owner]["methods"] if m["name"] == row["method"] and m["descriptor"] == row["descriptor"]]
        need(len(method) == 1 and method[0].get("code_bytes") == row["bytes"] and method[0].get("code_sha256") == row["sha256"],
             "actual live/archived Code equality")
    for mode in ("normal", "debugger"):
        observed = b["modes"][mode]
        need(len(observed["bindings"]) == 747 and len(observed["initialized_original_classes"]) == 192 and
             runtime["class_bindings"][mode] == observed["bindings"] and
             runtime["initialized_original_classes"][mode] == observed["initialized_original_classes"],
             "complete actual class-origin/initialized closure " + mode)
        for name, value in observed["bindings"].items():
            origin = urlparse(value["origin"])
            path = unquote(origin.path)
            if re.match(r"^/[A-Za-z]:/", path):
                path = path[1:]
            expected = authority["classes"][name]
            need(origin.scheme == "file" and not origin.netloc and local(path, root).resolve() == jar_names[0].resolve() and
                 value["bytes"] == expected["class_bytes"] and value["sha256"] == expected["class_sha256"],
                 "actual original archive runtime origin " + name)
    cases = []
    for i in range(9):
        e = [r for r in events if r.get("case") == i]
        boundary = [r for r in e if r["event"] == "interest-boundary"]
        need([r["location"]["pc"] for r in boundary] == ([51, 126] if i < 7 else [51]), "selected actual leaf boundaries")
        after = boundary[1] if i < 7 else next(r for r in e if r["event"] == "exception")
        selected = [r for r in e if boundary[0]["sequence"] <= r["sequence"] <= after["sequence"]]
        parent = [r for r in e if r["event"] == "return" and r["location"] == {"class": "ls", "method": "o", "descriptor": "()V", "pc": 545}]
        need(len(parent) == 1 and parent[0]["sequence"] < boundary[0]["sequence"], "actual superclass return stays Source-only")
        need(not [r for r in selected if r["event"] == "random" or (r["event"] == "entry" and r["location"]["class"] == "in")],
             "no Original leaf RNG/IN calls")
        cases.append(dict(case=i, before=boundary[0], after=after, events=e, leaf=selected,
                          row=b["modes"]["debugger"]["rows"][i], parent=parent[0]))
    return cases, pins, dict(captures=2, inputs_each=18, helpers_each=5, artifacts_each=24,
                           whole_packet_files=76, classes_each_VM=747, initialized_each_VM=192, live_Code=1945,
                           current_export_line_endings_changed=provenance["chronology"]["changed_captured_export_files"])


def _ref(value):
    return None if value is None else (value["class"], value["oid"])


def compare(rows, cases):
    profiles = [r for r in rows if r.get("record") == "profile"]
    need(len(profiles) == len(cases) == 9, "complete actual leaf record set")
    comparisons, aliases = 0, 0
    for n, s in zip(profiles, cases):
        i = s["case"]
        need(all(type(n[k]) is bool for k in ("interested", "selected_read", "food_type", "food_returned",
                 "stack_nonnull", "range_returned", "interest_returned", "source_exception", "native_policy", "parent_return_credit")),
             "typed emitted boolean receipts")
        need(all(type(n[k]) is int for k in ("case", "pc", "child_pc", "selected", "tame_calls", "id_reads", "array_reads", "bone_id")) and
             len(n["ids"]) == 3 and all(type(v) is int for v in n["ids"]) and len(n["arrays"]) == len(n["items"]) == 2,
             "typed emitted scalar and fixed receipt arrays")
        events, leaf = s["events"], s["leaf"]
        reads = [r for r in events if r["event"] == "field-read"]
        field = lambda name: [r for r in reads if r["field"] == name]
        method = lambda c, name, descriptor: [r for r in leaf if r["event"] == "return" and r["location"]["class"] == c and r["location"]["method"] == name and r["location"]["descriptor"] == descriptor]
        tame, food = method("gi", "D", "()Z"), method("yw", "m", "()Z")
        current = method("ix", "b", "()Liz;")
        ids, arrays, bone = field("iz.c"), field("gm.c"), field("gm.aV")
        selected = field("ix.c")
        need(n["case"] == i and n["scene"] == s["row"]["scene"], "same authored case/scenario")
        need(n["scope"] == "GI.o4..186-only" and n["parent_return_credit"] is False,
             "native entered range never earns full GI/super/tick return")
        need(s["before"]["state"]["fields"]["gi.a"] is False, "actual Original before-interest clear")
        need(n["interested"] == s["after"]["state"]["fields"]["gi.a"], "actual selected interested result")
        need(n["tame_calls"] == len(tame) and n["id_reads"] == len(ids) and n["ids"][:len(ids)] == [r["value"] for r in ids] and
             n["array_reads"] == len(arrays), "actual fresh getter/id/array counts and values")
        need(n["selected_read"] == bool(selected), "actual selected-index reachability")
        if selected:
            need(n["selected"] == selected[0]["value"], "actual selected slot value")
        need(n["food_returned"] == bool(food) and n["food_type"] == bool(food), "actual selected food branch/return")
        if food:
            need(n["interested"] == food[0]["result"], "actual Source food result to Native stored result")
        if bone:
            need(n["bone_id"] == field("gm.bf")[0]["value"], "actual canonical Bone id")
        if i < 7:
            need(n["range_returned"] and n["interest_returned"] and n["pc"] == 186 and not n["source_exception"],
                 "native range186 return pairs selected leaf only")
        else:
            ex = s["after"]
            need(not n["range_returned"] and not n["interest_returned"] and n["source_exception"] and not n["native_policy"] and
                 n["exception"].replace("/", ".") == ex["class"] and n["child_class"] == ex["location"]["class"] and
                 n["child_pc"] == ex["location"]["pc"] and n["pc"] == 51, "exact Original NULL class/callee/caller BCI")
        source_roles = {"player": _ref(s["before"]["state"]["fields"]["ls.b"]),
                        "inventory": _ref(field("gs.c")[0]["value"])}
        if current:
            source_roles["stack"] = _ref(current[0]["result"])
            need(n["stack_nonnull"] == (current[0]["result"] is not None), "actual returned current Stack nullability")
        else:
            need(not n["stack_nonnull"] and n["stack"] == "0" * 16, "Native unreached Stack receipt stays empty, no Source NULL claim")
        native_roles = {k: n[k] for k in source_roles}
        if bone:
            source_roles["bone"] = _ref(bone[0]["value"])
            native_roles["bone"] = n["bone"]
        for ordinal, read in enumerate(arrays):
            name = "array" + str(ordinal)
            source_roles[name] = _ref(read["value"])
            native_roles[name] = n["arrays"][ordinal]
        if food:
            actual = next(r for r in leaf if r["event"] == "entry" and r["location"]["class"] == "yw" and r["location"]["method"] == "m")
            source_roles["food_second_item"] = _ref(actual["receiver"])
            native_roles["food_second_item"] = n["items"][1]
        for role, value in source_roles.items():
            need(re.fullmatch(r"[0-9a-f]{16}", native_roles[role]) is not None, "actual identity token shape " + role)
            need((native_roles[role] == "0" * 16) == (value is None), "actual nullable role " + role)
        for a in source_roles:
            for b in source_roles:
                need((source_roles[a] == source_roles[b]) == (native_roles[a] == native_roles[b]),
                     "actual per-profile role alias partition " + a + "/" + b)
                aliases += 1
        comparisons += 12 + len(ids) + len(arrays)
    return dict(selected_value_checks=comparisons, per_profile_alias_relations=aliases,
                paired_roles="selected Player/Inventory/current Stack/Bone/arrays; actual food second-element receiver",
                unpaired=["Native dynamic GM/Item class", "complete temporal/cross-profile identity inventory",
                          "Native before51 field snapshot", "first nonfood element object identity",
                          "first food element identity (Native repeated-item alias is its own C control)",
                          "Source full LS.o/superclass/GI method return and parent fields", "raw independent constructor/entropy values"])


def successful_build(data, executable, root):
    need(data.get("status") == "PASS", "explicit actual successful build")
    phases = data.get("phases")
    need(isinstance(phases, list) and phases, "actual nonempty build phases")
    target, found = "mc_test_wolf_interest", False
    for phase in phases:
        need(type(phase.get("exit")) is int and phase["exit"] == 0, "every actual phase succeeded")
        command = phase.get("command")
        need(isinstance(command, list) and command and all(isinstance(v, str) and v for v in command), "actual argv command")
        words = command
        program = words[0].replace("\\", "/").rsplit("/", 1)[-1].lower()
        if program in ("wsl", "wsl.exe"):
            need(len(words) > 5 and words[1:5] == ["-d", "Ubuntu", "--exec", "cmake"], "exact admitted WSL CMake argv")
            words = words[4:]
            program = "cmake"
        if program in ("cmake", "cmake.exe"):
            if len(words) == 5 and words[1] == "-S" and words[3] == "-B":
                need(local(words[2], root).resolve() == root.resolve() and (root / "CMakeLists.txt").is_file(),
                     "actual CMake configure Source root")
                need(local(words[4], root).resolve().is_dir(), "actual CMake configure build directory")
                continue
            need(len(words) > 4 and words[1] == "--build" and "--target" in words, "actual CMake build/target form")
            index = words.index("--target") + 1
            targets = []
            while index < len(words) and not words[index].startswith("-"):
                targets.append(words[index])
                index += 1
            need(targets and len(targets) == len(set(targets)) and
                 all(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.-]*", t) for t in targets), "strict CMake target list")
            if target in targets:
                build = local(words[2], root).resolve()
                need(executable.name in (target, target + ".exe") and executable.is_relative_to(build), "selected exact target/tree executable")
                found = True
        elif program == "msbuild.exe":
            solutions = [v for v in words[1:] if v.lower().endswith((".slnx", ".sln"))]
            flags = [v for v in words[1:] if v.lower().startswith("/t:")]
            need(len(solutions) == len(flags) == 1, "actual one MSBuild solution and target list")
            targets = flags[0][3:].split(";")
            need(targets and len(targets) == len(set(targets)) and
                 all(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.-]*", t) for t in targets), "strict MSBuild target list")
            need({"/p:configuration=debug", "/p:platform=x64", "/m:6"} <= {v.lower() for v in words[1:]}, "actual Debug/x64/m6 MSBuild form")
            solution = local(solutions[0], root).resolve()
            need(solution.name in ("minecraft_c.sln", "minecraft_c.slnx") and solution.is_file(), "actual generated solution")
            if target in targets:
                need(executable.name == target + ".exe" and executable.parent == solution.parent / "Debug", "actual selected MSBuild executable/tree")
                found = True
        else:
            raise AssertionError("unadmitted build program; no shell/echo command masquerade")
    need(found, "actual mc_test_wolf_interest target invocation")


def build_inputs(path, executable, root, replacement=None):
    data = json.loads(path.read_text()) if replacement is None else replacement
    need(data.get("schema") == "native-target-build-output-binding-v1" and data.get("status") == "PASS" and
         data.get("target") == "mc_test_wolf_interest" and data.get("binding_scope") == "post-build/pre-paired-execution",
         "actual post-build/pre-paired output companion, no retroactive first-Native chronology")
    original = local(data["source_receipt"]["path"], root).resolve()
    need(sha(original) == data["source_receipt"]["sha256"], "immutable actual build receipt")
    source = json.loads(original.read_text())
    for field in ("phases", "input_before_sha256", "input_after_sha256", "inputs_unchanged"):
        need(data[field] == source[field], "exact actual copied build field " + field)
    successful_build(source, executable, root)
    before, after = source["input_before_sha256"], source["input_after_sha256"]
    need(source["inputs_unchanged"] is True and before and before == after, "stable actual compiled inputs")
    needed = {"CMakeLists.txt", "cmake/mob_tick.cmake", "tests/entity/test_wolf_interest.c",
              "tests/world/runtime/test_world_random_tick.c", "src/entity/mob/tick/tick.c",
              "src/entity/mob/tick/wolf.c", "src/entity/mob/tick/wolf_setter.c",
              "src/entity/mob/tick/internal.h", "include/mc/entity/mob/tick.h",
              "src/item/registry/runtime.c", "include/mc/item/registry/runtime.h",
              "src/inventory/player_current.c", "include/mc/inventory/player_current.h"}
    pins = {path.resolve(): sha(path), original: data["source_receipt"]["sha256"]}
    actual_inputs = set()
    for name, digest in before.items():
        actual = local(name, root).resolve()
        need(actual.is_file() and sha(actual) == digest, "actual current compiled input " + name)
        pins[actual] = digest
        actual_inputs.add(actual)
    need({(root / name).resolve() for name in needed} <= actual_inputs, "compiled component/fixture/inventory dependency closure")
    output = data.get("output_sha256")
    need(isinstance(output, dict) and output, "actual expected executable output pins")
    selected = [(local(name, root).resolve(), digest) for name, digest in output.items() if local(name, root).resolve() == executable]
    need(len(selected) == 1 and sha(executable) == selected[0][1], "actual selected compiled executable bytes")
    pins[executable] = selected[0][1]
    for phase in source["phases"]:
        need(isinstance(phase.get("log"), str) and phase["log"], "retained actual build log")
        log = original.parent / phase["log"]
        need(log.is_file(), "actual build log exists")
        pins[log.resolve()] = sha(log)
    return pins, dict(source_receipt=str(original), source_sha256=sha(original), output_sha256=selected[0][1],
                      binding_scope="post-build/pre-paired-execution", before_first_Native_execution_claimed=False), source, data


def comparator_negatives(rows, cases):
    profiles = [r for r in rows if r.get("record") == "profile"]
    mutations = [
        ("altered_source_result", 1, "interested", False),
        ("wrong_fresh_tame_count", 2, "tame_calls", 1),
        ("wrong_second_id", 3, "ids", [319, 260, 0]),
        ("wrong_selected_index", 0, "selected", 8),
        ("wrong_null_callee_site", 8, "child_pc", 51),
        ("aliased_Player_Inventory", 1, "inventory", profiles[1]["player"]),
        ("aliased_Inventory_Stack", 1, "stack", profiles[1]["inventory"]),
        ("split_actual_array_alias", 3, "arrays", [profiles[3]["arrays"][0], profiles[3]["inventory"]]),
        ("wrong_Bone_Stack_alias", 1, "bone", profiles[1]["stack"]),
        ("food_result_mismatch", 4, "interested", True),
    ]
    accepted = []
    for name, index, key, value in mutations:
        wrong = copy.deepcopy(rows)
        profiles = [r for r in wrong if r.get("record") == "profile"]
        profiles[index][key] = value
        try:
            compare(wrong, cases)
        except AssertionError:
            accepted.append(name)
        else:
            raise AssertionError("Source comparator accepted mutation: " + name)
    return accepted


def manifest_negatives(packet, provenance):
    actual = {p.relative_to(packet).as_posix() for p in packet.rglob("*") if p.is_file()}
    original = json.loads((packet / "final-current-audit.json").read_text())
    results = []
    for key in ("attempt-b/debugger/checkpoints.ndjson", "attempt-b/normal/target.ndjson", "inputs-b/WolfInterestJdi.java"):
        wrong = copy.deepcopy(original["file_sha256"])
        wrong.pop(key)
        try:
            need(len(wrong) == provenance["packet"]["whole_packet_files"] == 76 and actual == set(wrong) | {"final-current-audit.json"},
                 "complete immutable consumed artifact manifest")
        except AssertionError:
            results.append("omitted_" + key)
        else:
            raise AssertionError("manifest accepted omission " + key)
    return results


def drift_negative(pins, suffix, reason):
    matches = [p for p in pins if p.as_posix().endswith(suffix)]
    need(len(matches) == 1, "exact recorded drift-control path " + suffix)
    wrong = dict(pins)
    wrong[matches[0]] = "0" * 64
    try:
        check_pins(wrong, reason)
    except AssertionError:
        return dict(name=reason, rejected=True, path=str(matches[0]), scope="altered expected pin only; retained input never mutated")
    raise AssertionError("expected-pin drift control was accepted")


def build_negatives(source, companion, executable, root):
    controls = []
    for name in ("failed_status", "failed_phase", "absent_target", "empty_target", "echo_masquerade"):
        bad = copy.deepcopy(source)
        if name == "failed_status":
            bad["status"] = "FAIL"
        elif name == "failed_phase":
            bad["phases"][-1]["exit"] = 1
        else:
            for phase in bad["phases"]:
                command = phase["command"]
                if "--target" in command:
                    for i, word in enumerate(command):
                        if word == "mc_test_wolf_interest":
                            command[i] = "" if name == "empty_target" else "mc_test_wolf_interest_absent"
                    if name == "echo_masquerade":
                        phase["command"] = ["echo", "--build", str(executable.parent), "--target", "mc_test_wolf_interest"]
                elif any(v.lower().startswith("/t:") for v in command):
                    for i, word in enumerate(command):
                        if word.lower().startswith("/t:"):
                            command[i] = word.replace("mc_test_wolf_interest", "" if name == "empty_target" else "mc_test_wolf_interest_absent")
                    if name == "echo_masquerade":
                        command[0] = "echo"
        try:
            successful_build(bad, executable, root)
        except AssertionError:
            controls.append(name)
        else:
            raise AssertionError("build parser accepted " + name)
    output = copy.deepcopy(companion["output_sha256"])
    selected = [name for name in output if local(name, root).resolve() == executable]
    need(len(selected) == 1, "exact selected expected executable negative")
    output[selected[0]] = "0" * 64
    try:
        need(sha(executable) == output[selected[0]], "actual executable pin")
    except AssertionError:
        controls.append("wrong_executable_output_pin")
    else:
        raise AssertionError("wrong executable pin accepted")
    return controls
