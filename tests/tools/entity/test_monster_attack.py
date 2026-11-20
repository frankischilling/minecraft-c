"""Twelve current GZ contracts; historical original producers remain unchanged."""
import argparse
import csv
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import struct
import tempfile
import time
import traceback
from urllib.parse import urlparse
from urllib.request import url2pathname
import zipfile

sys.dont_write_bytecode = True
ROOT = next(p for p in Path(__file__).resolve().parents if (p / "CMakeLists.txt").is_file())
REFERENCE = ROOT / "tests/reference/entity/monster_attack"
ORIGINAL_CAPTURE_SHA = "1a7f6d74c428e6dfca6b624117c2ecb94cc55854327e4d4cfbb82aa49b38097b"
RETURNS = {0, 1, 2, 3, 4, 5, 11}
HURTS = {6, 7}
NULLS = {8: 14, 9: 17, 10: 24}


def need(ok, message):
    if not ok:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def local(name):
    # Receipts may contain either one or repeated Windows separators.
    value = str(name).replace("\\", "/")
    drive = re.match(r"^([A-Za-z]):/+(.*)$", value)
    if os.name != "nt" and drive:
        return Path("/mnt") / drive[1].lower() / drive[2]
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", value):
        return Path(value[5].upper() + ":/" + value[7:])
    p = Path(value)
    return p if p.is_absolute() else ROOT / p


def file_origin(name):
    parsed = urlparse(name)
    need(parsed.scheme == "file" and not parsed.netloc, "local recorded class origin")
    value = url2pathname(parsed.path)
    if re.match(r"^/[A-Za-z]:/", value):
        value = value[1:]
    return local(value).resolve()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def inventory(paths):
    return {str(p.resolve()): digest(p) for p in sorted(set(paths))}


def original_code_bindings(directory, data):
    jars = [local(n) for n in data["input_before_sha256"]
            if str(n).replace("\\", "/").endswith("/b1.7.3.jar")]
    need(len(jars) == 1, "one actual captured Original archive")
    parser_path = ROOT / "tests/reference/client/controller/classfile.py"
    classfile = module("monster_classfile", parser_path)
    expected_helpers = data["helper_before_sha256"]
    actual_helpers = {p.name: digest(p) for p in (directory / "classes").glob("*.class")}
    need(expected_helpers == actual_helpers == data["helper_after_sha256"],
         "actual compiled helper binary hashes")
    helper_code = {p.name: classfile.inspect_class(p.read_bytes())
                   for p in (directory / "classes").glob("*.class")}
    need(helper_code == data["helper_code_before"] == data["helper_code_after"],
         "actual compiled helper whole Code records")
    binding = json.loads((directory / "runtime-bindings.json").read_text())
    with zipfile.ZipFile(jars[0]) as archive:
        inventory = {n: hashlib.sha256(archive.read(n)).hexdigest()
                     for n in archive.namelist() if n.endswith(".class")}
        need(inventory == data["original_class_inventory"], "actual complete Original class inventory")
        pins = binding["original_class_pins"]
        for name, pin in pins.items():
            need(classfile.inspect_class(archive.read(name.replace(".", "/") + ".class")) == pin,
                 "actual whole Original class/every-Code pin: " + name)
        for mode in ("normal", "debugger"):
            rows = binding["bindings"][mode]
            need(len(rows) == len(pins) and {r["class"] for r in rows} == set(pins),
                 "complete distinct runtime class binding closure")
            for row in rows:
                need(file_origin(row["origin"]) == jars[0].resolve(), "actual unchanged archive origin")
    events = [json.loads(x) for x in (directory / "debugger/checkpoints.ndjson").read_text().splitlines()]
    live = set()
    for event in events:
        if event.get("event") != "code-pin" or event["class"] not in pins:
            continue
        key = (event["class"], event["method"], event["descriptor"])
        need(key not in live, "distinct actual live Code pin")
        method = next(m for m in pins[event["class"]]["methods"]
                      if m["name"] == event["method"] and m["descriptor"] == event["descriptor"])
        need((event["bytes"], event["sha256"]) == (method["code_bytes"], method["code_sha256"]),
             "actual live Code versus unchanged archive")
        live.add(key)
    need(("gz", "a", "(Lsn;F)V") in live and
         len(live) == binding["live_original_code_matches"], "actual target/live Code closure")
    return dict(helpers=len(helper_code), original_classes=len(pins), live_original_code=len(live))


def audit_original(directory, replacement=None):
    receipt = directory / "capture.json"
    need(digest(receipt) == ORIGINAL_CAPTURE_SHA, "frozen private Original b receipt")
    data = json.loads(receipt.read_text()) if replacement is None else replacement
    need(data["status"] == "PROTOTYPE_RECORDED_UNQUALIFIED" and data["inputs_unchanged"] and
         data["helpers_unchanged"], "honest unqualified Original status and stable inputs/helpers")
    need(data["input_before_sha256"] == data["input_after_sha256"] and
         data["helper_before_sha256"] == data["helper_after_sha256"] and
         data["helper_code_before"] == data["helper_code_after"], "complete recorded before/after")
    paths = [receipt]
    for name, expected in data["input_before_sha256"].items():
        path = local(name)
        need(path.is_file() and digest(path) == expected, "current Original input: " + name)
        paths.append(path)
    need(set(data["output_artifacts"]) == {p.relative_to(directory).as_posix()
         for p in directory.rglob("*") if p.is_file() and p != receipt}, "complete raw artifact inventory")
    for name, pin in data["output_artifacts"].items():
        relative = PurePosixPath(name)
        need(not relative.is_absolute() and ".." not in relative.parts and "\\" not in name,
             "relative artifact path")
        path = directory.joinpath(*relative.parts)
        need(path.is_file() and path.stat().st_size == pin["bytes"] and digest(path) == pin["sha256"],
             "recorded raw Original artifact: " + name)
        paths.append(path)
    original_code_bindings(directory, data)
    normal = [json.loads(x) for x in (directory / "normal/target.ndjson").read_text().splitlines()]
    debugger = [json.loads(x) for x in (directory / "debugger/target.ndjson").read_text().splitlines()]
    trace = [json.loads(x) for x in (directory / "debugger/checkpoints.ndjson").read_text().splitlines()]
    need([r["case"] for r in normal] == list(range(12)) == [r["case"] for r in debugger],
         "all actual Original profiles")
    rows = []
    for a, b in zip(normal, debugger):
        i = a["case"]
        events = [r for r in trace if r.get("case") == i]
        entry = next(r["state"] for r in events if r["event"] == "component-entry")
        finish = next(r["state"] for r in events if r["event"] == "component-finished")
        need(a["input"] == b["input"] and not a["default_entropy_rebound"] and
             not b["default_entropy_rebound"], "actual independent constructors and same operands")
        need(a["world_access_list_size"] == b["world_access_list_size"] == 0, "actual empty listener List")
        prefixes = [r for r in events if r["event"] == "hurt-prefix"]
        returns = [r for r in events if r["event"] == "return" and
                   r["site"]["class"] == "gz" and r["site"]["descriptor"] == "(Lsn;F)V"]
        if i in HURTS:
            need(len(prefixes) == 1, "one actual hurt entry")
            prefix = prefixes[0]["state"]
            selected = a["after"]
            need(prefixes[0]["frames"][1] == {"class": "gz", "method": "a", "descriptor": "(Lsn;F)V", "pc": 61},
                 "actual GZ61 caller")
            need(prefixes[0]["receiver"] == {"class": "wh", "role": "target"} and
                 prefixes[0]["arguments"] == [{"class": "uz", "role": "actor"}, 5],
                 "actual hurt receiver/source/amount")
            need(prefix["actor"]["ls.ae"] == 20 and prefix["target"]["ls.Y"] == 10,
                 "actual pre-hurt state")
            need(len(returns) == 1 and returns[0]["phase"] == "after-native-frontier" and
                 a["after"]["target"]["ls.Y"] == b["after"]["target"]["ls.Y"] == 5 and
                 returns[0]["site"]["pc"] == 65,
                 "historical original actual whole hurt/return observation")
        else:
            selected = a["after"]
            need(a["before"] == a["after"] and b["before"] == b["after"] and entry == finish,
                 "returned/failed gate preserves all actual fields/streams/pools")
            need(not prefixes, "no hurt call in quiet/NULL profile")
            if i in RETURNS:
                need(a["returned"] and b["returned"] and len(returns) == 1 and
                     returns[0]["site"]["pc"] == 65, "real Original return65")
            else:
                errors = [r for r in events if r["event"] == "exception" and
                          r["class"] == "java.lang.NullPointerException"]
                need(not a["returned"] and not b["returned"] and len(errors) == 1 and
                     errors[0]["site"]["class"] == "gz" and errors[0]["site"]["pc"] == NULLS[i],
                     "actual Original source-null site")
        rows.append((a, selected))
    profiles = [p for p in paths if p.name == "profiles.tsv"]
    need(len(profiles) == 1, "one actual consumed Source profile table")
    return data, rows, profiles[0], paths


def bounds(value):
    return None if value is None else [value["eq." + n]["D"] for n in "abcdef"]


def projection(value):
    a, t = value["actor"], value["target"]
    return dict(cooldown=a["ls.ae"], damage=a["gz.c"], actor_health=a["ls.Y"],
                target_health=t["ls.Y"], attacking=a["ii.e"], actor_dead=a["sn.be"],
                target_dead=t["sn.be"], actor_bounds=bounds(value["actor_bounds"]),
                target_bounds=bounds(value["target_bounds"]))


def compare(native, original):
    need(len(native) == 12, "all twelve actual Native rows")
    checks = 0
    for row, (source, selected) in zip(native, original):
        i = source["case"]
        need(row["case"] == i and row["scenario"] == source["input"][1], "profile identity/order")
        need(row["actor_class"] == "uz" and row["target_class"] == "wh", "genuine canonical class roles")
        need(row["constructor"]["rebound"] is False, "Native raw entropy context")
        for phase, values in [("before", source["before"]), ("after", selected)]:
            expect = projection(values)
            need({k: row[phase][k] for k in expect} == expect, "actual selected Source state: " + str(i))
            checks += len(expect)
        need(row["returned"] == (i in RETURNS | HURTS) and row["pc"] == (65 if i in RETURNS | HURTS else NULLS[i]),
             "actual current whole return versus source failure")
        need(row["guard_released"] and row["cleanup_returned"] and row["null_fields_preserved"] and
             row["entropy_unchanged"], "actual checked cleanup/prefix receipts")
        for key in ["actor_seed", "target_seed", "world_seed", "math_seed", "U"]:
            expected_seed = advance(row["before"][key], 2) if i in HURTS and key == "target_seed" else row["before"][key]
            need(expected_seed == row["after"][key], "actual native private consumption/preservation: " + key)
        actor = row["attacker_identity"]
        need(actor != "0000000000000000" and row["target_identity"] != actor,
             "actual distinct actor/target identities")
        need(row["argument_identity"] == ("0000000000000000" if source["input"][4] == "1" else row["target_identity"]),
             "actual nullable argument role")
        if i in HURTS:
            need(row["hurt"] and row["damage"] == 5 and not row["native_policy"] and not row["source_exception"] and
                 row["effect_source_identity"] == actor and row["returned"], "actual completed hurt61 and parent65")
        elif i in NULLS:
            need(not row["hurt"] and row["source_exception"] and not row["native_policy"], "actual Source NULL")
        else:
            need(not row["hurt"] and not row["source_exception"], "quiet returned body")
        checks += 12
    return checks


def successful_build(data, executable):
    need(data.get("status") == "PASS", "explicit successful actual build status")
    phases = data.get("phases")
    need(isinstance(phases, list) and phases, "nonempty actual build phases")
    target = "mc_test_monster_attack"
    found = False
    for phase in phases:
        need(type(phase.get("exit")) is int and phase["exit"] == 0, "every actual build phase succeeded")
        command = phase.get("command")
        need(isinstance(command, list) and command and all(isinstance(w, str) for w in command),
             "actual argv build command")
        targets = []
        if "--target" in command:
            for word in command[command.index("--target") + 1:]:
                if word.startswith("-"):
                    break
                targets.append(word)
        if "--build" in command and target in targets:
            build = local(command[command.index("--build") + 1]).resolve()
            need(executable.name in (target, target + ".exe") and executable.is_relative_to(build),
                 "selected executable belongs to the actual target build tree")
            found = True
        program = command[0].replace("\\", "/").rsplit("/", 1)[-1].lower()
        if program == "msbuild.exe":
            solutions = [w for w in command[1:] if w.lower().endswith(".slnx")]
            target_flags = [w for w in command[1:] if w.lower().startswith("/t:")]
            need(len(solutions) == len(target_flags) == 1, "actual single MSBuild solution/target list")
            targets = target_flags[0][3:].split(";")
            need(targets and len(targets) == len(set(targets)) and
                 all(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.-]*", w) for w in targets),
                 "well-formed actual MSBuild target list")
            flags = {w.lower() for w in command[1:]}
            need({"/p:configuration=debug", "/p:platform=x64", "/m:6"} <= flags,
                 "actual admitted MSBuild Debug/x64/m6 form")
            solution = local(solutions[0]).resolve()
            need(solution.name == "minecraft_c.slnx" and solution.is_file(), "actual generated Minecraft solution")
            if target in targets:
                need(executable.name == target + ".exe" and executable.parent == solution.parent / "Debug",
                     "selected actual MSBuild Debug executable/tree")
                found = True
    need(found, "actual Monster target build invocation")


def malformed_build(data, malformed):
    changed = copy.deepcopy(data)
    touched = False
    for phase in changed["phases"]:
        command = phase["command"]
        if "--build" in command and "--target" in command:
            for i in range(command.index("--target") + 1, len(command)):
                if command[i] == "mc_test_monster_attack":
                    command[i] = "" if malformed else "mc_test_monster_attack_absent"
                    touched = True
        if command[0].replace("\\", "/").rsplit("/", 1)[-1].lower() == "msbuild.exe":
            for i, word in enumerate(command):
                if word.lower().startswith("/t:"):
                    command[i] = word + ";;" if malformed else word.replace("mc_test_monster_attack", "mc_test_monster_attack_absent")
                    touched = True
    need(touched, "actual build target form for negative control")
    return changed


def build_inputs(path, executable, replacement=None):
    data = json.loads(path.read_text()) if replacement is None else replacement
    paths = [path]
    source = data
    source_path = path
    if data.get("schema") == "monster-build-output-binding-v1":
        need(data.get("status") == "PASS" and
             data.get("binding_scope") == "post-build/pre-first-native-execution",
             "explicit post-build/pre-first-execution output companion")
        source_path = local(data["source_receipt"]["path"]).resolve()
        need(digest(source_path) == data["source_receipt"]["sha256"], "immutable actual build receipt")
        source = json.loads(source_path.read_text())
        need(data["phases"] == source["phases"] and
             data["input_before_sha256"] == source["input_before_sha256"] and
             data["input_after_sha256"] == source["input_after_sha256"], "exact copied actual build record")
        paths.append(source_path)
    successful_build(source, executable)
    before, after = source["input_before_sha256"], source["input_after_sha256"]
    need(source.get("inputs_unchanged") is True and data.get("inputs_unchanged") is True and
         before and before == after, "stable actual compiled input receipt")
    needed = {"src/entity/mob/tick/tick.c", "src/entity/mob/tick/monster_attack.c",
              "src/entity/mob/tick/behavior.c", "src/entity/mob/tick/internal.h",
              "include/mc/entity/mob/tick.h", "tests/entity/test_monster_attack.c",
              "tests/world/runtime/test_world_random_tick.c"}
    needed_resolved = {(ROOT / name).resolve(): name for name in needed}
    found = set()
    for name, expected in before.items():
        p = local(name)
        need(p.is_file() and digest(p) == expected, "actual current compiled input: " + name)
        paths.append(p)
        actual = p.resolve()
        if actual in needed_resolved:
            found.add(needed_resolved[actual])
    need(found == needed, "compiled receipt includes actual component/body/fixture")
    outputs = data.get("output_sha256")
    need(isinstance(outputs, dict) and outputs, "explicit actual build executable output pin")
    selected = [(name, pin) for name, pin in outputs.items() if local(name).resolve() == executable]
    need(len(selected) == 1 and digest(executable) == selected[0][1],
         "same selected actual executable output")
    for phase in source["phases"]:
        if phase.get("log"):
            log = source_path.parent / phase["log"]
            need(log.is_file(), "actual build phase log")
            paths.append(log)
    info = dict(source_receipt=str(source_path), source_sha256=digest(source_path),
                output_binding_scope=data.get("binding_scope", "recorded build output"),
                executable=str(executable), executable_sha256=selected[0][1])
    return paths, info, source, data



def public_inputs():
    data = json.loads((REFERENCE / "provenance.json").read_text())
    need(data["schema"] == 1 and data["chronology"]["public_paths_were_capture_start_inputs"] is False,
         "explicit post-capture public rehoming")
    for name, expected in data["public_normalized_authored_inputs_sha256"].items():
        need(name in {"ConstructorJson.java", "MonsterTarget.java", "MonsterJdi.java", "profiles.tsv", "source-pins.json"} and
             digest(REFERENCE / name) == expected, "current public authored input: " + name)
    need(len(data["public_normalized_authored_inputs_sha256"]) == 5 and
         set(data["public_normalized_authored_inputs_sha256"]) == set(data["captured_authored_inputs_sha256"]) and
         len(data["compiled_helpers_sha256"]) == 3 and
         digest(REFERENCE / "runtime-class-pins.json") == data["runtime_class_pins_sha256"],
         "complete authored/static reference bindings")
    with (REFERENCE / "profiles.tsv").open(newline="", encoding="utf-8") as table:
        profiles = list(csv.DictReader(table, delimiter="\t"))
    need(len(profiles) == 12 and [int(r["id"]) for r in profiles] == list(range(12)),
         "complete twelve authored profiles")
    return data, profiles, [p for p in REFERENCE.iterdir() if p.is_file()]


def archived_public_binding(data, provenance):
    """Prove exactly the declared post-capture line-ending transform."""
    for name, expected in provenance["captured_authored_inputs_sha256"].items():
        paths = [local(n) for n in data["input_before_sha256"] if local(n).name == name]
        need(len(paths) == 1 and digest(paths[0]) == expected,
             "actual raw archived authored input: " + name)
        need((REFERENCE / name).read_bytes() == paths[0].read_bytes().replace(b"\r\n", b"\n"),
             "public input is exact archived CRLF-to-LF transform: " + name)


def native_c_inputs():
    """Bounded actual GZ source group and literal fixture/header include closure.

    This is not a build receipt or a recursive inventory of unrelated tests or
    ignored files. Optional build admission independently binds every recorded
    actual compiled input. Standard SDK headers are external to this source view.
    """
    cmake = ROOT / "cmake/mob_tick.cmake"
    body = re.search(r"target_sources\(mc_simulation PRIVATE(.*?)\)", cmake.read_text(encoding="utf-8"), re.S)
    need(body is not None, "actual Monster component source group")
    names = re.findall(r"src/[A-Za-z0-9_./-]+\.c", body[1])
    need("src/entity/mob/tick/monster_attack.c" in names, "actual protected body in component group")
    pending = [ROOT / n for n in names] + [ROOT / "tests/entity/test_monster_attack.c", cmake]
    result = set()
    while pending:
        path = pending.pop().resolve()
        need(path.is_relative_to(ROOT) and path.is_file(), "actual bounded C input: " + str(path))
        if path in result:
            continue
        result.add(path)
        if path.suffix not in (".c", ".h", ".inc"):
            continue
        for match in re.finditer(r'^\s*#\s*include\s*(["<])([^">]+)[">]', path.read_text(encoding="utf-8"), re.M):
            spelling = match[2]
            candidates = [path.parent / spelling, ROOT / "src" / spelling,
                          ROOT / "src/core/java" / spelling, ROOT / "include" / spelling]
            found = next((p.resolve() for p in candidates if p.is_file()), None)
            if found is not None:
                need(found.is_relative_to(ROOT), "local include stays in repository")
                pending.append(found)
            else:
                need(match[1] == "<", "unresolved literal repository include: " + spelling)
    return sorted(result)


def expect_pins(actual, expected, label):
    need(actual == expected, label)


def recorded_original_pins(directory, data):
    result = {str((directory / "capture.json").resolve()): ORIGINAL_CAPTURE_SHA}
    for name, expected in data["input_before_sha256"].items():
        key = str(local(name).resolve())
        need(key not in result or result[key] == expected, "consistent captured input alias")
        result[key] = expected
    for name, pin in data["output_artifacts"].items():
        key = str((directory / name).resolve())
        need(key not in result or result[key] == pin["sha256"], "consistent captured artifact alias")
        result[key] = pin["sha256"]
    return result


def recorded_build_pins(source, companion, path):
    result = {str(local(name).resolve()): expected for name, expected in source["input_before_sha256"].items()}
    for name, expected in companion["output_sha256"].items():
        key = str(local(name).resolve())
        need(key not in result or result[key] == expected, "consistent compiled/output alias")
        result[key] = expected
    result[str(path.resolve())] = digest(path)
    if companion.get("schema") == "monster-build-output-binding-v1":
        row = companion["source_receipt"]
        result[str(local(row["path"]).resolve())] = row["sha256"]
    return result


def check_recorded(pins, label):
    current = {name: digest(Path(name)) for name in pins}
    expect_pins(current, pins, label)
    return current


def expected_pin_negative(pins, label, suffix):
    need(pins, "nonempty actual expected-pin negative")
    # A stable but wrong baseline must still fail the historical pin check.
    key = next((name for name in pins if name.replace("\\", "/").endswith(suffix)), None)
    need(key is not None, "actual reached input exists for expected-pin negative")
    stable_wrong_before = dict(pins)
    stable_wrong_before[key] = "0" * 64 if pins[key] != "0" * 64 else "1" * 64
    stable_wrong_after = dict(stable_wrong_before)
    need(stable_wrong_before == stable_wrong_after, "negative has a stable intervening baseline")
    try:
        expect_pins(stable_wrong_after, pins, label)
    except AssertionError as error:
        need(str(error) == label, "intended historical-pin negative category")
        return dict(name=label, rejected=True, stable_before_after=True,
                    reason="recorded pin rejects a stable intervening wrong baseline")
    raise AssertionError("intervening drift was accepted")


def advance(seed, count):
    state = int(seed, 16)
    for _ in range(count):
        state = (state * 0x5DEECE66D + 11) & ((1 << 48) - 1)
    return f"{state:012x}"


def verify_native(native, profiles):
    need(len(native) == 12, "all twelve actual Native rows")
    checks = 0
    before_keys = {"cooldown", "damage", "actor_health", "target_health", "attacking", "actor_dead",
                   "target_dead", "actor_bounds", "target_bounds", "actor_seed", "target_seed", "world_seed", "math_seed", "U"}
    row_keys = {"case", "scenario", "actor_class", "target_class", "constructor", "before", "after", "returned",
                "native_policy", "source_exception", "pc", "hurt", "damage", "attacker_identity", "target_identity",
                "argument_identity", "effect_source_identity", "guard_released", "cleanup_returned", "null_fields_preserved", "entropy_unchanged"}
    for i, (row, p) in enumerate(zip(native, profiles)):
        need(set(row) == row_keys and row["case"] == i and row["scenario"] == p["name"], "complete ordered actual Native profile")
        need(row["actor_class"] == "uz" and row["target_class"] == "wh", "genuine canonical uz/wh roles")
        ctor = row["constructor"]
        need(ctor["rebound"] is False and type(ctor["holder_before"]) is int and
             re.fullmatch(r"[0-9a-f]{16}", ctor["U_before"]), "actual independent constructor context")
        for owner in ("actor", "target"):
            for name in ("F", "G", "aj", "yaw"):
                need(re.fullmatch(r"[0-9a-f]{8}", ctor[owner + "_" + name]), "raw constructor float word retained")
        a, b = row["before"], row["after"]
        need(set(a) == set(b) == before_keys, "complete current Native state")
        need(a["cooldown"] == int(p["cooldown"]) and a["damage"] == 5 and
             a["actor_health"] == 20 and a["target_health"] == 10 and
             a["attacking"] is a["actor_dead"] is a["target_dead"] is False,
             "actual authored canonical constructor/pre-call fields")
        expected = dict(a)
        if i in HURTS:
            expected["cooldown"] = 20
            expected["target_health"] = 5
            expected["target_seed"] = advance(a["target_seed"], 2)
        need(b == expected, "exact selected current mutation and private stream consumption")
        for name, flag, low, high in (("actor_bounds", "actor_bounds_null", "64", "66"),
                                      ("target_bounds", "target_bounds_null", p["target_min_y"], p["target_max_y"])):
            box = a[name]
            if p[flag] == "1":
                need(box is None and b[name] is None, "declared NULL source field survives call/cleanup")
            else:
                need(isinstance(box, list) and len(box) == 6 and
                     all(re.fullmatch(r"[0-9a-f]{16}", word) for word in box), "actual complete permanent bounds")
                as_bits = lambda v: "7ff8000000000000" if v == "NaN" else struct.pack(">d", float(v)).hex()
                need(box[1] == as_bits(low) and box[4] == as_bits(high), "actual authored vertical operands")
        for name in ("actor_seed", "target_seed", "world_seed", "math_seed", "U"):
            expected_seed = advance(a[name], 2) if i in HURTS and name == "target_seed" else a[name]
            need(re.fullmatch(r"[0-9a-f]{16}" if name == "U" else r"[0-9a-f]{12}", a[name]) and expected_seed == b[name],
                 "actual native private consumption/preservation: " + name)
        expected_pc = 65 if i in RETURNS | HURTS else NULLS[i]
        need(row["returned"] is (i in RETURNS | HURTS) and row["pc"] == expected_pc and
             row["hurt"] is (i in HURTS) and row["native_policy"] is False and
             row["source_exception"] is (i in NULLS), "exact authored current return/failure class")
        need(row["damage"] == (5 if i in HURTS else 0), "reached actual hurt amount")
        for name in ("attacker_identity", "target_identity", "argument_identity", "effect_source_identity"):
            need(re.fullmatch(r"[0-9a-f]{16}", row[name]), "actual value-only identity")
        need(row["attacker_identity"] != "0" * 16 and row["target_identity"] != "0" * 16 and
             row["attacker_identity"] != row["target_identity"], "actual distinct canonical owners")
        need(row["argument_identity"] == ("0" * 16 if p["target_null"] == "1" else row["target_identity"]) and
             row["effect_source_identity"] == (row["attacker_identity"] if i in HURTS else "0" * 16),
             "actual nullable target and reached attacker role")
        need(all(row[k] is True for k in ("guard_released", "cleanup_returned", "null_fields_preserved", "entropy_unchanged")),
             "actual balanced native cleanup without source repair")
        checks += 35
    return checks


def authored_negatives(native, profiles):
    results = []
    for name, index, key, value in (("wrong_canonical_target", 0, "target_class", "bx"),
                                     ("lost_hurt_return", 6, "returned", False),
                                     ("wrong_effect_role", 6, "effect_source_identity", native[6]["target_identity"]),
                                     ("wrong_null_pc", 9, "pc", 14)):
        changed = copy.deepcopy(native)
        changed[index][key] = value
        try:
            verify_native(changed, profiles)
        except AssertionError:
            results.append(dict(name=name, rejected=True, scope="authored native transport; no Original outcome credit"))
        else:
            raise AssertionError("authored negative accepted: " + name)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--original", type=Path)
    parser.add_argument("--build-receipt", type=Path)
    args = parser.parse_args()
    need(bool(args.original) == bool(args.build_receipt), "Original pairing requires both explicit Original and actual build receipt")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="monster-attack-", dir=args.work_dir.resolve()))
    report = dict(status="FAIL", scope="FINITE_ORIGINAL_SELECTED_PAIR" if args.original else "AUTHORED_NATIVE_ONLY",
                  pre_execution_audits_complete=False, native_execution_started=False)
    before, failed, completed, process = None, None, False, None
    build_pins, source_pins, controls = {}, {}, []
    try:
        executable = args.executable.resolve(strict=True)
        provenance, profiles_data, reference_paths = public_inputs()
        profiles = REFERENCE / "profiles.tsv"
        report["public_source_binding_chronology"] = provenance["chronology"]
        source_paths, compiled, original = [], [], None
        if args.original:
            directory = args.original.resolve()
            data, original, captured_profiles, source_paths = audit_original(directory)
            archived_public_binding(data, provenance)
            need(captured_profiles.read_bytes().replace(b"\r\n", b"\n") == profiles.read_bytes(),
                 "same captured/public authored profiles with exact declared newline transform")
            compiled, binding, build, companion = build_inputs(args.build_receipt.resolve(), executable)
            source_pins = recorded_original_pins(directory, data)
            build_pins = recorded_build_pins(build, companion, args.build_receipt.resolve())
            report["build_binding"] = binding
            report["source_code_binding"] = original_code_bindings(directory, data)
            report["historical_Original_status"] = data["status"]
            report["Original_scope"] = "Historical unqualified capture unchanged; current finite comparator selects7gate returns,3sourceNULLs and2actual whole Pig hurt/return65 after-states"
            for name, mutate in [("failed_build_status", lambda b: b.update(status="FAIL")),
                                 ("failed_build_phase", lambda b: b["phases"][-1].update(exit=1)),
                                 ("missing_target_build", lambda b: b.update(phases=[]))]:
                bad = copy.deepcopy(build)
                mutate(bad)
                try:
                    successful_build(bad, executable)
                except AssertionError:
                    controls.append(name)
                else:
                    raise AssertionError("negative build accepted: " + name)
            for malformed in (False, True):
                try:
                    successful_build(malformed_build(build, malformed), executable)
                except AssertionError:
                    controls.append("malformed_target_form" if malformed else "absent_monster_target")
                else:
                    raise AssertionError("negative target form accepted")
            bad = copy.deepcopy(companion)
            selected = next(k for k in bad["output_sha256"] if local(k).resolve() == executable)
            bad["output_sha256"][selected] = "0" * 64
            try:
                build_inputs(args.build_receipt.resolve(), executable, bad)
            except AssertionError:
                controls.append("wrong_executable_output")
            else:
                raise AssertionError("negative executable output accepted")
            report["expected_pin_controls"] = [expected_pin_negative(build_pins, "intervening_compiled_input_drift", "tests/entity/test_monster_attack.c"),
                                                expected_pin_negative(source_pins, "intervening_Original_input_drift", "normal/target.ndjson")]
        support = ROOT / "tests/tools/entity/test_mob_construction.py"
        resources = module("monster_resources", support).RESOURCES
        c_inputs = native_c_inputs()
        if args.original:
            # Preserve the former full optional source baseline as well as the
            # stronger immediate/final recorded-pin checks. Bare CTest mode
            # uses only the bounded actual component/include closure above.
            c_inputs = sorted(set(c_inputs) | {p for folder in ("src", "include", "tests")
                for p in (ROOT / folder).rglob("*") if p.is_file() and p.suffix in (".c", ".h", ".inc")})
        inputs = [Path(__file__).resolve(), Path(sys.executable).resolve(), support, executable,
                  *reference_paths, *source_paths, *compiled, *c_inputs]
        report["native_source_inventory_scope"] = "Actual Mob component source group plus fixture/literal include closure; no recursive ignored or unrelated test inventory. Optional compiled receipt independently binds all recorded compiler inputs."
        report["bounded_C_input_count"] = len(c_inputs)
        files, worlds = work / "resources", work / "worlds"
        files.mkdir()
        worlds.mkdir()
        for name, content in resources.items():
            target = files / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            inputs.append(target)
        before = inventory(inputs)
        report["input_before_sha256"] = before
        # No newly chosen baseline may replace recorded build/capture pins.
        # Recheck exact historical expectations immediately before dispatch.
        if build_pins:
            report["pre_execution_recorded_build_sha256"] = check_recorded(build_pins, "compiled expected pin drift before Native")
            report["pre_execution_recorded_Original_sha256"] = check_recorded(source_pins, "Original expected pin drift before Native")
        report["pre_execution_audits_complete"] = True
        command = [str(executable), str(files), str(worlds), str(work), str(profiles)]
        stdout, stderr = work / "native.stdout", work / "native.stderr"
        report["command"] = command
        start, reason = time.monotonic(), None
        with stdout.open("wb") as so, stderr.open("wb") as se:
            report["native_execution_started"] = True
            process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=so, stderr=se)
            while process.poll() is None:
                if time.monotonic() - start > 120:
                    reason = "Native timeout"
                elif max(stdout.stat().st_size, stderr.stat().st_size) > 32 * 1024 * 1024:
                    reason = "Native output bound"
                if reason:
                    process.kill()
                    break
                time.sleep(.05)
            code = process.wait(timeout=15)
        report.update(native_exit=code, seconds=time.monotonic() - start, termination_reason=reason)
        error = stderr.read_bytes()
        need(reason is None and code == 0 and not error,
             "Native failed; full stderr " + str(stderr) + ": " + error[:2048].decode(errors="replace"))
        lines = stdout.read_text().splitlines()
        footer = re.fullmatch(r"Monster attack: ([1-9][0-9]*) checks;12 genuine protected-body profiles", lines[-1]) if lines else None
        need(lines[:24] == ["151 recipes", "16 achievements"] * 12 and len(lines) == 25 and footer,
             "exact declared setup lines and actual check footer")
        report["native_checks"] = int(footer[1])
        native = [json.loads(s) for s in (work / "native.ndjson").read_text().splitlines()]
        report["authored_transport_checks"] = verify_native(native, profiles_data)
        report["authored_negative_controls"] = authored_negatives(native, profiles_data)
        if original is not None:
            report["comparisons"] = compare(native, original)
            for name, index, key, value in [("lost_hurt_return", 6, "returned", False),
                                           ("wrong_effect_role", 6, "effect_source_identity", native[6]["target_identity"]),
                                           ("wrong_null_pc", 9, "pc", 14)]:
                bad = copy.deepcopy(native)
                bad[index][key] = value
                try:
                    compare(bad, original)
                except AssertionError:
                    controls.append(name)
                else:
                    raise AssertionError("negative comparator accepted: " + name)
            bad = copy.deepcopy(data)
            bad["output_artifacts"][next(iter(bad["output_artifacts"]))]["sha256"] = "0" * 64
            try:
                audit_original(args.original.resolve(), bad)
            except AssertionError:
                controls.append("bad_original_artifact")
            else:
                raise AssertionError("negative manifest accepted")
        report.update(controls=controls, cases=12, returned=9, source_null=3, completed_hurt=2)
        completed = True
    except BaseException as error:
        failed = error
        report["failure"] = dict(type=type(error).__name__, message=str(error)[:2048])
        (work / "driver.failure.txt").write_text(traceback.format_exc())
    finally:
        if process is not None and process.poll() is None:
            try:
                process.kill()
                report["cleanup_native_exit"] = process.wait(timeout=15)
            except (OSError, subprocess.TimeoutExpired) as error:
                failed = failed or error
                report["native_cleanup_error"] = dict(type=type(error).__name__, message=str(error)[:1024])
        after, errors = {}, []
        if before is not None:
            for name in before:
                try:
                    after[name] = digest(Path(name))
                except OSError as error:
                    errors.append(dict(path=name, type=type(error).__name__, message=str(error)[:1024]))
        report["input_after_sha256"], report["final_inventory_errors"] = after, errors
        same = before is not None and before == after and not errors
        for name, pins in (("build", build_pins), ("Original", source_pins)):
            if pins:
                try:
                    report["final_recorded_" + name + "_sha256"] = check_recorded(pins, name + " expected pin drift after Native")
                except (OSError, AssertionError) as error:
                    same = False
                    failed = failed or error
                    report["final_expected_" + name + "_error"] = dict(type=type(error).__name__, message=str(error)[:2048])
        report["inputs_unchanged"] = same
        if failed is None and not same:
            failed = AssertionError("actual Native/Source input drift or final inventory refusal")
            report["failure"] = dict(type=type(failed).__name__, message=str(failed))
        artifacts, artifact_errors = {}, []
        for path in sorted(work.rglob("*")):
            if path.is_file() and path.name != "run.json":
                try:
                    artifacts[path.relative_to(work).as_posix()] = digest(path)
                except OSError as error:
                    artifact_errors.append(dict(path=str(path), type=type(error).__name__, message=str(error)[:1024]))
        report["output_artifacts"], report["output_inventory_errors"] = artifacts, artifact_errors
        report["status"] = "PASS" if completed and failed is None and same and not artifact_errors else "FAIL"
        (work / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"] + " Monster attack " + report["scope"])
    print(work / "run.json")
    need(report["status"] == "PASS", "verification failed; receipt " + str(work / "run.json"))


if __name__ == "__main__":
    main()
