"""Twelve real Zombie damage contracts; optional audited Original selected-prefix comparison.

Bare mode executes only authored Worlds/profiles. Optional original records are parsed
before Native for pinned admission, then used solely for comparison after execution.
Captured entropy/field values never become Native execution inputs.
"""
import argparse
import copy
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import traceback
import zipfile
from urllib.parse import urlparse
from urllib.request import url2pathname

sys.dont_write_bytecode = True
ROOT = next(p for p in Path(__file__).resolve().parents if (p / "CMakeLists.txt").is_file())
REFERENCE = Path(__file__).resolve().parents[2] / "reference/entity/zombie_null_damage"
NATIVE = Path(__file__).resolve().parents[2] / "entity/test_zombie_null_damage.c"
RANDOM_CONSTRUCTOR_FIELDS = frozenset(("ls.F", "ls.G", "ls.aj"))
FRESH = frozenset((0, 6, 7, 9, 10, 11))
RETURNED = frozenset((0, 1, 2, 3, 4, 11))
MASK = (1 << 48) - 1


def need(value, message):
    if not value:
        raise AssertionError(message)


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def local(name):
    value = str(name).replace("\\", "/")
    if os.name != "nt" and re.match(r"^[A-Za-z]:/", value):
        return Path("/mnt") / value[0].lower() / value[3:]
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", value):
        return Path(value[5].upper() + ":/" + value[7:])
    p = Path(value)
    return p if p.is_absolute() else ROOT / p


def inventory(paths):
    return {str(p.resolve()): digest(p) for p in sorted(set(paths))}


def verify_expected(pins, label):
    for path, expected in pins.items():
        need(path.is_file() and digest(path) == expected, f"{label}: {path}")


def f32(v):
    return struct.unpack("!f", struct.pack("!f", v))[0]


def fword(v):
    return {"F": struct.pack("!f", v).hex()}


def dword(v):
    return {"D": struct.pack("!d", v).hex()}


def lcg(seed, count):
    return (seed * 0x5deece66d + 11) & MASK


def double_draw(seed):
    seed = lcg(seed, 1)
    high = seed >> 22
    seed = lcg(seed, 1)
    return seed, ((high << 27) | (seed >> 21)) / 2**53


def float_draw(seed):
    seed = lcg(seed, 1)
    return seed, (seed >> 24) / 2**24


def pitch(a, b):
    return f32(f32(f32(a-b) * f32(.2)) + f32(1))


def utf16(value):
    encoded = value.encode("utf-16-be", "surrogatepass")
    return {"utf16": list(struct.unpack(">" + "H" * (len(encoded) // 2), encoded))}


def random_word(seed_input, advances):
    state = (seed_input ^ 0x5deece66d) & ((1 << 48) - 1)
    for _ in range(advances):
        state = (state * 0x5deece66d + 11) & ((1 << 48) - 1)
    return f"{state:012x}"


REFERENCE_FIELDS = frozenset(("sn.aG", "sn.aH", "sn.aI", "sn.aW", "sn.bs", "sn.bD", "ls.b", "ii.a", "ii.d"))


def scalar(value):
    if isinstance(value, str):
        return utf16(value)
    if isinstance(value, dict) and "class" in value and "string" in value:
        return utf16(value["string"])
    return value


def object_token(value, label):
    if value is None:
        return None
    need(isinstance(value, dict) and isinstance(value.get("class"), str), f"typed object reference required: {label}")
    keys = [key for key in ("identity", "oid") if key in value]
    need(len(keys) == 1 and type(value[keys[0]]) in (int, str), f"one actual process-local reference identity: {label}")
    return value["class"], value[keys[0]]


def role_model(state, label):
    roles = state.get("reference_roles")
    need(isinstance(roles, dict) and "actor" in roles and "world" in roles, f"complete native/source reference roles: {label}")
    tokens = {name: object_token(value, label + "/role/" + name) for name, value in roles.items()}
    need(all(value is not None for value in tokens.values()), f"absent source roles must be omitted: {label}")
    by_identity = {}
    for name, (kind, identity) in tokens.items():
        if identity not in by_identity:
            by_identity[identity] = (kind, [])
        need(by_identity[identity][0] == kind, f"one class per actual reference identity: {label}/{name}")
        by_identity[identity][1].append(name)
    return tokens, by_identity


def reference_symbol(value, model, label):
    token = object_token(value, label)
    if token is None:
        return None
    kind, identity = token
    need(identity in model[1], f"unrepresented nonnull reference: {label}/{kind}")
    actual_class, names = model[1][identity]
    need(kind == actual_class, f"reference class/role inconsistency: {label}")
    return kind, tuple(sorted(names))


def reference_locations(state, label):
    model = role_model(state, label)
    locations = {"role/" + name: value for name, value in model[0].items()}
    owners = [("actor", state)] + [(f"secondary{i}", entity) for i, entity in enumerate(state.get("secondary_entities", []))]
    for owner, entity in owners:
        for name, value in entity["fields"].items():
            if name in REFERENCE_FIELDS or (isinstance(value, dict) and "class" in value and "string" not in value):
                reference_symbol(value, model, label + "/" + owner + "/" + name)
                locations[owner + "/" + name] = object_token(value, label)
    return locations


def compare_role_models(native, original, label):
    actual, expected = role_model(native, label + "/native"), role_model(original, label + "/original")
    need(set(actual[0]) == set(expected[0]), f"complete logical-role inventory: {label}")
    count = 0
    names = sorted(actual[0])
    for index, name in enumerate(names):
        need(actual[0][name][0] == expected[0][name][0], f"actual role class differs: {label}/{name}")
        count += 1
        for other in names[index + 1:]:
            need((actual[0][name][1] == actual[0][other][1]) ==
                 (expected[0][name][1] == expected[0][other][1]), f"actual role alias partition differs: {label}/{name}/{other}")
            count += 1
    return actual, expected, count


def compare_identity_transition(native_before, native_after, source_before, source_after, label):
    # Compare equality relations inside each process, never native pointer values
    # to unrelated JVM IDs. Cross-phase relations catch fresh same-value objects
    # even if the after-state recomputes the same logical role name.
    nb = reference_locations(native_before, label + "/native-before")
    na = reference_locations(native_after, label + "/native-after")
    sb = reference_locations(path_roles(source_before), label + "/source-before")
    sa = reference_locations(path_roles(source_after), label + "/source-after")
    need(set(nb) == set(sb) and set(na) == set(sa), f"complete cross-phase reference locations: {label}")
    count = 0
    for before in sorted(nb):
        for after in sorted(na):
            need((nb[before] == na[after]) == (sb[before] == sa[after]),
                 f"reference identity continuity differs: {label}/{before}->{after}")
            count += 1
    return count


def path_roles(state):
    # The frozen source observer retained the actual DH class/identity in II.a.
    # Naming that observed field adds a logical role, without adding a field,
    # inventing Path contents, replacing an identity or changing evidence.
    result = dict(state)
    roles = dict(state["reference_roles"])
    owners = [("actor", state)] + [(f"secondary{i}", value)
                                    for i, value in enumerate(state.get("secondary_entities", []))]
    for name, value in owners:
        actual = value["fields"].get("ii.a")
        if actual is not None:
            need(object_token(actual, name + "/actual-II.a")[0] == "dh", "actual observed Path class")
            key = name + ".path"
            need(key not in roles or roles[key] == actual, "Path role preserves the observed field identity")
            roles[key] = actual
    result["reference_roles"] = roles
    return result


def source_state(state):
    result = path_roles(state)
    if result["own_seed"] == "-":
        result["own_seed"] = None  # Original text transport's nullable Random sentinel.
    result["fields"] = {key: scalar(value) for key, value in state["fields"].items()}
    watcher = dict(state["watcher"]) if state["watcher"] is not None else None
    if watcher is not None:
        watcher["records"] = [dict(row, value=scalar(row["value"])) for row in watcher["records"]]
        watcher["size"] = len(watcher["records"])
    result["watcher"] = watcher
    return result



def successful_build(data, executable):
    need(data.get("status") == "PASS", "explicit successful actual build status")
    phases = data.get("phases")
    need(isinstance(phases, list) and phases, "nonempty actual build phases")
    target = "mc_test_zombie_null_damage"
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
    need(found, "actual Zombie damage target build invocation")


def malformed_build(data, malformed):
    changed = copy.deepcopy(data)
    touched = False
    for phase in changed["phases"]:
        command = phase["command"]
        if "--build" in command and "--target" in command:
            for i in range(command.index("--target") + 1, len(command)):
                if command[i] == "mc_test_zombie_null_damage":
                    command[i] = "" if malformed else "mc_test_zombie_null_damage_absent"
                    touched = True
        if command[0].replace("\\", "/").rsplit("/", 1)[-1].lower() == "msbuild.exe":
            for i, word in enumerate(command):
                if word.lower().startswith("/t:"):
                    command[i] = word + ";;" if malformed else word.replace("mc_test_zombie_null_damage", "mc_test_zombie_null_damage_absent")
                    touched = True
    need(touched, "actual build target form for negative control")
    return changed


def build_inputs(path, executable, replacement=None):
    data = json.loads(path.read_text()) if replacement is None else replacement
    paths = [path]
    source = data
    source_path = path
    if data.get("schema") == "zombie-damage-build-output-binding-v1":
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
    needed = {"src/entity/mob/damage.c", "src/entity/movement/effects.c", "src/entity/tick/common.c",
              "include/mc/entity/movement/effects.h", "tests/entity/test_zombie_null_damage.c",
              "tests/entity/zombie_damage_transport.inc", "tests/world/runtime/test_world_random_tick.c"}
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
    provenance = json.loads((REFERENCE / "provenance.json").read_text())
    need(provenance["schema"] == 1 and provenance["chronology"]["public_paths_were_capture_start_inputs"] is False,
         "explicit post-capture public binding chronology")
    expected = provenance["public_authored_sha256"]
    need(set(expected) == {"ConstructorJson.java", "MobTickTarget.java", "ZombieDamageTarget.java",
                           "ZombieDamageJdi.java", "profiles.tsv", "preparation.tsv", "protocol.md"},
         "exact seven authored input declarations")
    for name, pin in expected.items():
        need(digest(REFERENCE / name) == pin and b"\r" not in (REFERENCE / name).read_bytes(),
             "actual frozen prospective LF input: " + name)
    for name, key in (("source-code-pins.json", "source_code_pins_sha256"),
                      ("helper-code-pins.json", "helper_code_pins_sha256")):
        need(digest(REFERENCE / name) == provenance[key], "actual static Code pin file: " + name)
    with (REFERENCE / "profiles.tsv").open(newline="", encoding="ascii") as source:
        reader = csv.DictReader(source, delimiter="\t")
        need(reader.fieldnames == "case entry scene amount world Y by au attacker callback scope".split(),
             "declared twelve-profile schema")
        rows = list(reader)
    need(len(rows) == 12 and [int(r["case"]) for r in rows] == list(range(12)), "complete authored profile order")
    return provenance, rows


def source_manifest(receipt, provenance):
    need(receipt["status"] == "OBSERVED_TWELVE_ZOMBIE_DAMAGE_CPU_CASES" and
         receipt["inputs_unchanged"] is True and receipt["helpers_unchanged"] is True and
         receipt["input_before_sha256"] == receipt["input_after_sha256"] == provenance["captured_inputs_sha256"] and
         receipt["helper_before_sha256"] == receipt["helper_after_sha256"] == provenance["compiled_helpers_sha256"] and
         len(receipt["input_before_sha256"]) == 18 and len(receipt["helper_before_sha256"]) == 5,
         "actual complete captured input/helper schema")
    need(receipt["artifacts_sha256"] == provenance["captured_artifacts"] and
         len(receipt["artifacts_sha256"]) == 24 and
         {"normal/target.ndjson", "debugger/target.ndjson", "debugger/checkpoints.ndjson", "authority.json",
          "helper-Code.json"} <= set(receipt["artifacts_sha256"]), "exact consumed Source artifact manifest")
    need(receipt["default_constructor_entropy_rebound"] is False and receipt["no_owner_seed_replay"] is True and
         receipt["read_only_JDI"] is True and receipt["ordinary_only"] is False,
         "actual independent default entropy and read-only JDI scope")


def original_audit(directory, provenance):
    directory = directory.resolve(strict=True)
    receipt_path = directory / "attempt-a/receipt.json"
    analysis_path = directory / "analysis-a.json"
    audit_path = directory / "final-current-audit.json"
    expected = {receipt_path: provenance["capture_receipt_sha256"],
                analysis_path: provenance["analysis_sha256"], audit_path: provenance["final_audit_sha256"],
                directory / "input-freeze-a.json": provenance["input_freeze_sha256"]}
    verify_expected(expected, "immutable original top-level admission")
    receipt = json.loads(receipt_path.read_text())
    source_manifest(receipt, provenance)
    audit = json.loads(audit_path.read_text())
    need(audit["status"] == "PRIVATE_ORIGINAL_ZOMBIE_DAMAGE_CURRENT_AUDIT_PASS" and
         audit["capture_receipt_sha256"] == provenance["capture_receipt_sha256"] and
         audit["analysis_sha256"] == provenance["analysis_sha256"] and
         audit["file_sha256"] == provenance["post_capture_artifacts_sha256"] and
         audit["all_current_fingerprints_match"] is True,
         "separate actual post-capture current audit")
    expected.update({directory / name: pin for name, pin in audit["file_sha256"].items()})
    expected.update({local(name): pin for name, pin in receipt["input_after_sha256"].items()})
    expected.update({directory / "attempt-a/classes" / name: pin
                     for name, pin in receipt["helper_after_sha256"].items()})
    for name, metadata in receipt["artifacts_sha256"].items():
        path = directory / "attempt-a" / name
        need(path.is_file() and path.stat().st_size == metadata["bytes"], "actual captured output byte count: " + name)
        expected[path] = metadata["sha256"]
    verify_expected(expected, "actual current captured input/helper/output pins")
    # Only the exact documented CRLF->LF transform is allowed. It is the identity
    # transform for the current seven inputs; no whitespace/source rewrite is accepted.
    for name, pin in provenance["captured_authored_sha256"].items():
        archived = directory / "inputs-a" / name
        need(digest(archived) == pin and
             (REFERENCE / name).read_bytes() == archived.read_bytes().replace(b"\r\n", b"\n"),
             "exact archived/public authored binding: " + name)
    parser_path = ROOT / "tests/reference/client/controller/classfile.py"
    classfile = load_module("zombie_original_classfile", parser_path)
    authority = json.loads((directory / "attempt-a/authority.json").read_text())
    need(authority == json.loads((REFERENCE / "source-code-pins.json").read_text()) and
         len(authority["classes"]) == 747 and
         sum("code_sha256" in method for cls in authority["classes"].values() for method in cls["methods"]) == 5709,
         "whole Original class/all-Code authority")
    jars = [local(name) for name in receipt["input_before_sha256"]
            if str(name).replace("\\", "/").endswith("/b1.7.3.jar")]
    need(len(jars) == 1 and digest(jars[0]) == authority["jar_sha256"], "exact unchanged Original archive")
    with zipfile.ZipFile(jars[0]) as archive:
        actual = {name[:-6]: classfile.inspect_class(archive.read(name))
                  for name in archive.namelist() if name.endswith(".class")}
    need(actual == authority["classes"], "all747 actual class and5709 Code identities")
    helpers = {p.name: classfile.inspect_class(p.read_bytes())
               for p in (directory / "attempt-a/classes").glob("*.class")}
    need(helpers == json.loads((REFERENCE / "helper-code-pins.json").read_text()) ==
         json.loads((directory / "attempt-a/helper-Code.json").read_text()), "all five actual helper Code identities")
    events = [json.loads(line) for line in (directory / "attempt-a/debugger/checkpoints.ndjson").read_text().splitlines()]
    code = [e for e in events if e["event"] == "code-pin"]
    need(len(code) == receipt["live_Code"] == 1754 and len(events) == receipt["JDI_events"] == 2588,
         "complete live Code and raw event inventories")
    for row in code:
        methods = [m for m in actual[row["class"].replace(".", "/")]["methods"]
                   if m["name"] == row["method"] and m["descriptor"] == row["descriptor"]]
        need(len(methods) == 1 and methods[0]["code_bytes"] == row["bytes"] and
             methods[0]["code_sha256"] == row["sha256"], "actual live Original Code identity")
    for mode in ("normal", "debugger"):
        recorded = receipt["modes"][mode]
        need(set(recorded["bindings"]) == set(actual) and not recorded["post_observation_binding_unavailable"],
             "all747 distinct actual Original runtime bindings")
        for name, bound in recorded["bindings"].items():
            url = urlparse(bound["origin"])
            path = url2pathname(url.path)
            if re.match(r"^/[A-Za-z]:/", path): path = path[1:]
            need(url.scheme == "file" and not url.netloc and local(path).resolve() == jars[0].resolve() and
                 bound["bytes"] == actual[name]["class_bytes"] and bound["sha256"] == actual[name]["class_sha256"],
                 "actual original runtime origin/whole-class pin")
        initialized = {n for n in re.findall(r"Initializing '([^']+)'",
                       (directory / f"attempt-a/{mode}/class-init.log").read_text()) if n in actual}
        need(sorted(initialized) == recorded["initialized_original_classes"] and len(initialized) == 183,
             "actual complete initialized Original closure")
        stdout = (directory / f"attempt-a/{mode}/target.stdout").read_text()
        need("ORIGINAL_ZOMBIE_DAMAGE_COMPLETE" in stdout and "BIND_UNAVAILABLE " not in stdout,
             "actual original completion and complete class bindings")
        bindings = {}
        for line in stdout.splitlines():
            if line.startswith("BIND "):
                _, name, origin, size, pin = line.split()
                need(name not in bindings,"one actual original class binding")
                bindings[name] = {"origin":origin,"bytes":int(size),"sha256":pin}
        need(bindings == recorded["bindings"],"raw actual runtime class bindings agree with captured receipt")
        rows = [json.loads(line) for line in (directory / f"attempt-a/{mode}/target.ndjson").read_text().splitlines()]
        need(rows == recorded["rows"] and len(rows) == 12 and [r["case"] for r in rows] == list(range(12)),
             "actual twelve consumed source records")
    analysis = json.loads(analysis_path.read_text())
    need(analysis["status"] == "PRIVATE_ORIGINAL_ZOMBIE_DAMAGE_ANALYSIS_PASS" and
         analysis["receipt_sha256"] == provenance["capture_receipt_sha256"] and analysis["same_VM_scalar_checks"] == 188,
         "independent same-VM source analysis remains explicit")
    return {"expected": expected, "receipt": receipt, "events": events, "analysis": analysis,
            "scope": "18 captured inputs/5 compiled helpers/24 captured outputs; separate post-capture current audit; all747 class origins,5709 Code methods,1754 live Code identities and183 initialized classes perVM. No new execution."}


def compare_entity(actual, expected, am, sm, label, exclude_direction):
    fields = {k: scalar(v) for k, v in expected["fields"].items()}
    need(set(actual["fields"]) == set(fields), "complete declared fields: " + label)
    count = 0
    for name, value in fields.items():
        if name in RANDOM_CONSTRUCTOR_FIELDS or name == "sn.aD" or (name == "ls.ac" and exclude_direction):
            continue
        observed = actual["fields"][name]
        if name in REFERENCE_FIELDS or (isinstance(value, dict) and "class" in value):
            need(reference_symbol(observed, am, label+"/native/"+name) ==
                 reference_symbol(value, sm, label+"/source/"+name), "actual ref class/role/alias: " + label+"/"+name)
        else:
            need(observed == value, f"actual typed field {label}/{name}: {observed!r} != {value!r}")
        count += 1
    watcher = copy.deepcopy(expected["watcher"])
    if watcher is not None:
        watcher["size"] = len(watcher["records"])
        for row in watcher["records"]: row["value"] = scalar(row["value"])
    need(actual["bounds"] == expected["bounds"] and actual["watcher"] == watcher,
         "actual permanent bounds/watcher scalar records: " + label)
    return count + 2


def compare_state(native, source, label, exclude_direction=False):
    am, sm, identities = compare_role_models(native, source, label)
    count = compare_entity(native, source, am, sm, label, exclude_direction)
    need(len(native["secondary_entities"]) == len(source["secondary_entities"]), "all secondary actors: " + label)
    for i, (a, b) in enumerate(zip(native["secondary_entities"], source["secondary_entities"])):
        count += compare_entity(a, b, am, sm, f"{label}/secondary{i}", False)
    for key in ("box_cursor", "vector_cursor", "loaded_entities", "players", "cache_size"):
        need(native[key] == source[key], "actual query/pool/publication state: " + label+"/"+key)
        count += 1
    # Independent constructor/private/World/shared seeds are retained raw. Their
    # per-call progress and values are checked within each process, never rebased.
    return count, identities


def exact_cleanup(before, after):
    names = ("fields", "bounds", "watcher", "own_seed", "world_seed", "math_seed", "uniquifier", "box_cursor",
             "vector_cursor", "loaded_entities", "players", "cache_size", "secondary_entities", "reference_roles")
    need(all(before[n] == after[n] for n in names), "checked no-op cleanup preserves the complete failed/returned prefix")


def native_oracles(index, phases, result):
    before, after = phases["before"], phases["after"]
    need(before["fields"]["sn.aD"] == after["fields"]["sn.aD"], "namespace-local Entity id stable")
    for name in RANDOM_CONSTRUCTOR_FIELDS:
        need(before["fields"][name] == after["fields"][name], "independent raw constructor field preserved: " + name)
    need(before["world_seed"] == after["world_seed"] and before["uniquifier"] == after["uniquifier"],
         "actual World stream and default-entropy namespace unchanged")
    roles_before = {k:v for k,v in before["reference_roles"].items() if k != "actor.path"}
    roles_after = {k:v for k,v in after["reference_roles"].items() if k != "actor.path"}
    need(roles_before == roles_after, "actual permanent owner/alias identities unchanged")
    if index != 9:
        need(before["reference_roles"] == after["reference_roles"], "direct damage does not construct or replace a Path")
    exact_cleanup(after, phases["after_cleanup"])
    if index == 9:
        pending = phases["sound_callback"]
        m, value = double_draw(int(before["math_seed"], 16))
        r, _ = float_draw(int(before["own_seed"], 16)); r, _ = float_draw(r)
        need(pending["math_seed"] == f"{m:012x}" and pending["own_seed"] == f"{r:012x}" and
             pending["fields"]["ls.ac"] == fword(int(value*2)*180), "actual nested SN501 Math251/private-float prefix")
        need(pending["fields"]["sn.bv"] == 20 and pending["fields"]["ls.aa"] == pending["fields"]["ls.ab"] == 10 and
             pending["fields"]["sn.by"] == 20 and after["fields"]["sn.bv"] == 19 and
             after["fields"]["ls.aa"] == 9 and after["fields"]["sn.by"] == 19 and
             result["tick_returned"] and result["environment_returned"], "actual fire child returns before decrement and later timers")
        return 13
    e = result["effect"]
    returned = index in RETURNED
    need(e["entered"] == (index != 8) and e["returned"] == returned and e["result_stored"] == returned and
         e["living_returned"] == returned and ((result["status"] == 0) == returned), "distinct full return/native prefix/failure outcomes")
    if returned:
        need(result["answer"] == (index not in (1, 2, 3)) == e["result"], "actual GZ boolean result")
        need(e["wrapper_pc"] == (40 if index in (1, 2, 3) else 26) and
             e["living_pc"] == {1:11, 2:25, 3:59}.get(index, 363), "actual LS/GZ return PCs")
    else:
        need(result["answer"] == result["answer_before"], "unreturned source invocation does not invent caller result")
    m = int(before["math_seed"], 16)
    r = None if before["own_seed"] == "-" else int(before["own_seed"], 16)
    if index in FRESH:
        m, shared = double_draw(m)
        need(e["shared_attempts"] == e["shared_calls"] == 1 and e["shared_value"] == dword(shared) and
             after["fields"]["ls.ac"] == fword(int(shared*2)*180), "actual same-process Math251/value/ac264 relation")
        if index != 6:
            need(e["sound_world"] == before["reference_roles"]["world"]["identity"] and
                 e["sound_name"] != "0000000000000000" and e["sound_world_captured"] and
                 e["sound_name_returned"] and e["volume_returned"], "actual captured World/name identity operands")
            r, a = float_draw(r); r, b = float_draw(r)
            need(e["private_attempts"] == e["private_calls"] == 2 and
                 e["private_values"] == [fword(a), fword(b)] and e["pitch"] == fword(pitch(a,b)),
                 "actual private LCG floats343/350 or295/302 and strict pitch grouping")
            need(e["volume"] == fword(1) and e["sound_literal"] == ("mob.zombiedeath" if index == 7 else "mob.zombiehurt"),
                 "actual name and inherited volume leaves")
        else:
            need(e["private_attempts"] == 1 and e["private_calls"] == 0, "Source NULL private stream before first float")
    else:
        need(e["shared_calls"] == e["shared_attempts"] == e["private_calls"] == e["private_attempts"] == 0,
             "early/repeat/entry-only has no new entropy draws")
    need(after["math_seed"] == f"{m:012x}" and
         after["own_seed"] == ("-" if r is None else f"{r:012x}"), "actual per-process stream progress; no cross-VM seed equality")
    if index in (5, 6):
        need(e["source_exception"] and not e["native_policy"] and
             e["exception_class"] == "java/lang/NullPointerException" and e["living_pc"] == (4 if index == 5 else 343),
             "precise Source NULL failure classification")
    if index == 7:
        need(e["native_policy"] and not e["source_exception"] and e["death_invoke_reached"] and
             e["sound_returned"] and e["living_pc"] == 316 and
             e["child_site"] == {"class":"ls","method":"b","descriptor":"(Lsn;)V","pc":0},
             "actual sound then unreturned death316 dependency")
    if index == 8:
        need(result["effect_source"] == before["reference_roles"]["secondary0"]["identity"] and
             result["effect_amount"] == 1 and result["effect_damage_pc"] == 3, "actual nonNULL GZ3 invocation operands")
    if index == 10:
        need(e["native_policy"] and not e["source_exception"] and e["living_pc"] == 359 and
             not e["sound_returned"] and result["world_pc"] == result["world_access_pc"] == 53 and
             result["world_status"] == result["world_cause_status"] == 3 and result["world_system_error"] == 0 and
             result["world_cause_operation"] == "authored native Zombie sound observer failure" and
             result["effect_operation"] == "Living damage actual World Entity sound child" and
             result["world_failure_receiver"] == "0000000000000000" and
             e["child_site"] == {"class":"fd","method":"a","descriptor":"(Lsn;Ljava/lang/String;FF)V","pc":53} and
             result["world_failure_receipt"] == "0000000000000000", "native callback IO full prefix; no invented Source Throwable receipt")
    return 18

def read_native(path):
    rows = [json.loads(line) for line in path.read_text(encoding="ascii").splitlines()]
    phases, results, cleanup, constructors, sounds = ({i:{} for i in range(12)}, {}, {}, {i:[] for i in range(12)}, {})
    for row in rows:
        index = row["case"]
        need(type(index) is int and index in phases, "declared native case identifier")
        if row["event"] == "state":
            need(row["phase"] not in phases[index], "unique actual native state phase")
            phases[index][row["phase"]] = row
        elif row["event"] == "result":
            need(index not in results, "unique actual result")
            results[index] = row
        elif row["event"] == "cleanup":
            need(index not in cleanup, "unique checked cleanup")
            cleanup[index] = row
        elif row["event"] == "constructor":
            need(row["ordinal"] == len(constructors[index]) and row["class"] ==
                 ("uz" if row["ordinal"] == 0 else "bx") and row["default_profile_rebound"] is False,
                 "actual independent raw Zombie/Cow construction context")
            constructors[index].append(row)
        elif row["event"] == "sound":
            need(index in (9, 10) and index not in sounds and row["ordinal"] == 0,
                 "exact extra native observer event inventory")
            sounds[index] = row
        else:
            raise AssertionError("undeclared native transport event: " + str(row["event"]))
    need(set(results) == set(cleanup) == set(phases) and set(sounds) == {9,10}, "no omitted result, cleanup or observer group")
    checks = 0
    for index in range(12):
        expected_phases = {"before", "after", "after_cleanup"} | ({"sound_callback"} if index in (9,10) else set())
        need(set(phases[index]) == expected_phases and len(constructors[index]) == (2 if index in (8,11) else 1),
             "complete declared state/constructor phases")
        for phase in phases[index].values():
            reference_locations(phase, f"native{index}/{phase['phase']}")
        check = cleanup[index]
        need(check["native_returned"] is True and check["source_prefix_preserved"] is True and
             check["ctor_diagnostic_preserved"] is True and check["actual_target_preserved"] == (index == 11),
             "actual native cleanup preserves source failure and construction diagnostics")
        checks += native_oracles(index, phases[index], results[index])
        for state in phases[index].values():
            need(state["loaded_entities"] == (2 if index in (8,11) else 1) and state["players"] == 0 and state["cache_size"] == 25,
                 "real ordinary Entity publication/25 stored Chunks/no synthetic Player")
    return {"phases": phases, "results": results, "constructors": constructors, "sounds": sounds,
            "records": len(rows), "native_scalar_checks": checks}


def location(row, cls, method, descriptor, pc=None, member="location"):
    site = row.get(member, {})
    return site.get("class") == cls and site.get("method") == method and site.get("descriptor") == descriptor and (pc is None or site.get("pc") == pc)


def one(rows, predicate, label):
    matches = [row for row in rows if predicate(row)]
    need(len(matches) == 1, "one actual source observation: " + label)
    return matches[0]


def select_source(original, index):
    events = [row for row in original["events"] if row.get("case") == index]
    before = one(events, lambda row: row["event"] == "active-entry", f"{index}/before")
    if index == 7:
        selected = one(events, lambda row: row["event"] == "prefix" and
                       location(row,"ls","b","(Lsn;)V",0,"site"), "fatal actual death entry")
        need(any(f["class"] == "ls" and f["descriptor"] == "(Lsn;I)Z" and f["pc"] == 316
                 for f in selected["source_frames"]), "actual missing death callsite316")
        mode = "native_prefix_at_actual_Source_death_entry316"
    elif index == 8:
        selected = one(events, lambda row: row["event"] == "prefix" and
                       location(row,"ls","a","(Lsn;I)Z",0,"site"), "nonNULL actual damage entry")
        need(selected["source_frames"][1]["class"] == "gz" and selected["source_frames"][1]["pc"] == 3,
             "actual Source invokespecial resolution LS <- GZ3")
        mode = "native_prefix_at_actual_Source_LS_entry_GZ3"
    elif index in (5,6,10):
        selected = one(events, lambda row: row["event"] == "exception", "actual reached Source failure")
        if index in (5,6):
            need(selected["class"] == "java.lang.NullPointerException" and
                 location(selected,"ls","a","(Lsn;I)Z",4 if index == 5 else 343), "actual Source NULL receiver BCI")
            mode = "Source_NULL_failure_prefix"
        else:
            need(selected["class"] == "java.lang.IllegalStateException" and
                 selected["source_frames"][0]["class"] == "fd" and selected["source_frames"][0]["pc"] == 53 and
                 selected["source_frames"][1]["pc"] == 359, "actual authored Source observer Throwable")
            mode = "field_prefix_only;native_IO_not_Source_Throwable_identity"
    elif index == 9:
        selected = one(events, lambda row: row["event"] == "entry" and
                       location(row,"fd","a","(Lsn;Ljava/lang/String;FF)V"), "nested fire actual sound entry")
        mode = "actual_SN501_nested_damage_sound_prefix;later_Native_Mob_body_unpaired"
    else:
        selected = one(events, lambda row: row["event"] == "active-return", "actual full direct GZ return")
        mode = "full_direct_damage_return"
    return before, selected, mode


def process_prefix_streams(index, before, selected):
    """Check one process only; neither seed nor value crosses an execution."""
    m = int(before["math_seed"], 16)
    r = None if before["own_seed"] == "-" else int(before["own_seed"], 16)
    count = 0
    if index in FRESH:
        m, value = double_draw(m)
        need(selected["fields"]["ls.ac"] == fword(int(value*2)*180), "actual same-process selected-prefix direction relation")
        count += 1
        if index != 6:
            r, _ = float_draw(r); r, _ = float_draw(r)
    need(selected["math_seed"] == f"{m:012x}" and
         selected["own_seed"] == ("-" if r is None else f"{r:012x}") and
         selected["world_seed"] == before["world_seed"], "actual selected-prefix per-process shared/private/World progress")
    return count + 3


def compare_original(native, original):
    checks = identities = stream_checks = 0
    selections = []
    for index in range(12):
        sb, selected, mode = select_source(original, index)
        phases = native["phases"][index]
        nb = phases["before"]
        na = phases["sound_callback"] if index == 9 else phases["after"]
        for phase, a, b in (("before",nb,sb["state"]),("selected",na,selected["state"])):
            c, r = compare_state(a,b,f"{index}/{phase}",index in FRESH and phase == "selected")
            checks += c; identities += r
        identities += compare_identity_transition(nb,na,sb["state"],selected["state"],f"{index}/actual-lifetime")
        stream_checks += process_prefix_streams(index,nb,na)
        stream_checks += process_prefix_streams(index,sb["state"],selected["state"])
        if index in RETURNED:
            source_row = original["receipt"]["modes"]["debugger"]["rows"][index]
            need(source_row["failure"] is None and source_row["result"] == native["results"][index]["answer"],
                 "actual full Source/direct Native boolean return")
            checks += 1
        if index == 9:
            events = [row for row in original["events"] if row.get("case") == 9]
            gz_return = one(events, lambda e:e["event"] == "return" and location(e,"gz","a","(Lsn;I)Z",26), "actual nested damage returned26")
            decrement = one(events, lambda e:e["event"] == "field-store" and e["field"] == "sn.bv" and
                            location(e,"sn","U","()V",512), "actual Source fire store512")
            need(gz_return["state"]["fields"]["sn.bv"] == 20 and decrement["next"] == 19 and
                 decrement["sequence"] > gz_return["sequence"], "actual Source fire decrement follows damage return")
            full = original["receipt"]["modes"]["debugger"]["rows"][9]["after"]
            for field in ("sn.bv","ls.Y","ls.aa","ls.ab","sn.by"):
                need(phases["after"]["fields"][field] == full["fields"][field], "only actual post-environment timer/health field: " + field)
                checks += 1
            sound = native["sounds"][9]
            need(sound["name"] == selected["arguments"][1] and sound["volume"] == selected["arguments"][2] and
                 sound["xyz"] == [selected["state"]["fields"][field]["D"] for field in ("sn.aM","sn.aN","sn.aO")],
                 "actual callback deterministic sound operands; offset0 declared; private pitch independently checked")
            checks += 5
        result = native["results"][index]
        if index != 9:
            entry = one([e for e in original["events"] if e.get("case") == index],
                        lambda e:e["event"] == "prefix" and location(e,"ls","a","(Lsn;I)Z",0,"site"),
                        "actual source evaluated damage operands")
            need(entry["arguments"][1] == result["effect_amount"] == (3 if index == 4 else 1),
                 "actual reached damage amount")
            if index == 8:
                need(object_token(entry["arguments"][0],"original/source-operand") ==
                     object_token(entry["state"]["reference_roles"]["secondary0"],"original/Cow-role"),
                     "actual nonNULL Source attacker is the same real Cow")
            else:
                need(entry["arguments"][0] is None and result["effect_source"] == "0000000000000000",
                     "actual NULL attacker operand")
            checks += 2
        if index in FRESH and index != 9:
            sounds = [e for e in original["events"] if e.get("case") == index and e["event"] == "entry" and
                      location(e,"fd","a","(Lsn;Ljava/lang/String;FF)V")]
            if index == 6:
                need(not sounds, "Source NULL stream before sound call")
            else:
                need(len(sounds) == 1 and result["effect"]["sound_literal"] == sounds[0]["arguments"][1] and
                     result["effect"]["volume"] == sounds[0]["arguments"][2], "actual deterministic literal/volume operands")
                checks += 2
        selections.append({"case":index,"scope":mode,"before":sb,"selected":selected})
    return {"typed_checks":checks,"identity_relations":identities,"separate_process_stream_checks":stream_checks,"profiles":12,
            "full_direct_returns":6,"Source_NULL_failures":2,"native_child_entry_prefixes":2,
            "native_IO_vs_Source_Throwable_field_prefixes":1,"actual_nested_SN501_sound_prefixes":1,
            "Source_same_VM_scalar_analysis_checks_retained":188,"Native_same_process_scalar_checks":native["native_scalar_checks"],
            "independent_entropy_rebound":False,"scope_limits":[
                "Constructor-derived LS.F/G/aj and namespace-local numeric Entity IDs are raw retained, excluded from cross-process field equality; native originals must remain unchanged through each call.",
                "Shared/private/World seeds and stochastic ac/pitch are raw retained, never cross-process equated. Native same-process LCG, pitch and Math251/ac relations are checked separately; frozen Source analysis retains188 same-VM checks.",
                "Case7 Original actually continues death/drop and returns; Native is compared at actual death-entry316 only. Case8 Original continues nonNULL knockback; Native is compared at actual LS entry GZ3 only.",
                "Case9 Native executes a full real Mob tick with ordinary sunlight false; Original executes virtual LS.U. Pair only actual SN501 sound prefix and explicit fire/health/timer after fields; no whole-body equality or source skip.",
                "Case10 native adapter IO and Original authored IllegalStateException retain distinct classifications. Pair deterministic actor prefix, not Source Throwable/receipt identity.",
                "Watcher record index/type/scalar/dirty and watcher object identity are paired; no boxed Byte or internal Map/record object identities where Source exposes scalars only. Strings compare UTF16 values, not identity.",
                "No full GL/audio-device/whole-frame, item-drop graph or playable-game claim."
            ]}, selections


def reject(name, call, output):
    try:
        call()
    except (AssertionError, KeyError, ValueError, FileNotFoundError):
        output.append(name)
    else:
        raise AssertionError("intended rejection accepted: " + name)


def comparator_negatives(native, original):
    passed = []
    _, source, _ = select_source(original,11)
    expected = native["phases"][11]["after"]
    def compare(value): return compare_state(value,source["state"],"negative/target",True)
    wrong = copy.deepcopy(expected); wrong["fields"]["ls.Y"] += 1
    reject("wrong_health", lambda:compare(wrong),passed)
    wrong = copy.deepcopy(expected); wrong["reference_roles"]["secondary0"]["class"] = "dl"; wrong["fields"]["ii.d"]["class"] = "dl"
    reject("wrong_concrete_target_class",lambda:compare(wrong),passed)
    wrong = copy.deepcopy(expected); wrong["fields"]["ii.d"] = copy.deepcopy(wrong["reference_roles"]["actor"])
    reject("lost_existing_Cow_target_alias",lambda:compare(wrong),passed)
    sb,sa,_=select_source(original,0); nb=native["phases"][0]["before"];na=native["phases"][0]["after"]
    wrong=copy.deepcopy(na); used={v["identity"] for v in wrong["reference_roles"].values()};value=int(wrong["reference_roles"]["actor.watcher"]["identity"],16)+4096
    while f"{value:016x}" in used:value+=4096
    wrong["reference_roles"]["actor.watcher"]["identity"]=f"{value:016x}";wrong["fields"]["sn.bD"]=copy.deepcopy(wrong["reference_roles"]["actor.watcher"])
    compare_state(wrong,sa["state"],"negative/per-state-same-value",True)
    reject("replaced_same_class_watcher_across_phases",lambda:compare_identity_transition(nb,wrong,sb["state"],sa["state"],"negative/continuity"),passed)
    for name, mutate in (("wrong_same_process_ac",lambda p,r:p["after"]["fields"].__setitem__("ls.ac",fword(90))),
                         ("wrong_actual_Math251_receipt",lambda p,r:r["effect"].__setitem__("shared_value",dword(-1))),
                         ("wrong_private_pitch",lambda p,r:r["effect"].__setitem__("pitch",fword(2)))):
        phases=copy.deepcopy(native["phases"][0]);result=copy.deepcopy(native["results"][0]);mutate(phases,result)
        phases["after_cleanup"]=copy.deepcopy(phases["after"])
        reject(name,lambda p=phases,r=result:native_oracles(0,p,r),passed)
    result=copy.deepcopy(native["results"][7]);result["effect"]["returned"]=True
    reject("missing_death_child_forged_return",lambda:native_oracles(7,native["phases"][7],result),passed)
    return {"count":len(passed),"rejected":passed,"scope":"Pure copied-observation corruption controls exercising the actual field/class/role/alias/temporal/scalar/outcome comparators; no mutation executes a game body."}


def manifest_negatives(receipt, provenance):
    passed=[]
    wrong=copy.deepcopy(receipt);del wrong["artifacts_sha256"]["debugger/checkpoints.ndjson"]
    reject("omitted_consumed_JDI_artifact",lambda:source_manifest(wrong,provenance),passed)
    wrong=copy.deepcopy(receipt);wrong["artifacts_sha256"]["normal/target.ndjson"]["sha256"]="0"*64
    reject("changed_consumed_source_output",lambda:source_manifest(wrong,provenance),passed)
    wrong=copy.deepcopy(receipt);del wrong["input_before_sha256"][next(iter(wrong["input_before_sha256"]))]
    reject("omitted_captured_input",lambda:source_manifest(wrong,provenance),passed)
    return {"count":len(passed),"rejected":passed}


def validate_stdout(data, status, stderr):
    need(status == 0 and not stderr,
         f"actual native failure: exit={status}; stderr={stderr.decode('utf-8', errors='replace')}")
    lines=data.decode("ascii").splitlines()
    need(len(lines)==25 and lines[:-1]==["151 recipes","16 achievements"]*12,
         "exact declared setup stdout order/count; no ignored garbage")
    footer=re.fullmatch(r"Zombie NULL damage: ([1-9][0-9]*) checks;12 genuine profiles",lines[-1])
    need(status==0 and not stderr and footer is not None,"actual native exit/stderr/footer")
    return int(footer[1])


def source_closure(start):
    # Exact quoted includes of the compiled test amalgamation, including every
    # included C/helper/inc body. Never recursively inventory ignored .local or
    # unrelated Original source trees in authored-native-only mode.
    todo=list(start);found=set()
    search=[ROOT/"include",ROOT/"src",ROOT/"src/core/java",ROOT/"tests/entity"]
    while todo:
        path=todo.pop().resolve(strict=True)
        if path in found:continue
        found.add(path)
        for name in re.findall(r'^\s*#\s*include\s+"([^"\n]+)"',path.read_text(errors="strict"),re.M):
            candidates=[path.parent/name,*[p/name for p in search]]
            actual=next((p for p in candidates if p.is_file()),None)
            need(actual is not None,"actual included test/header input: "+name)
            todo.append(actual)
    return found


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--work-dir",type=Path,required=True)
    parser.add_argument("--build-receipt",type=Path,help="Actual successful target build+compiled input map+selected output pins; required for Original comparison")
    parser.add_argument("--original",type=Path,help="Explicit external frozen Zombie packet root containing attempt-a, analysis-a and separate current audit")
    args=parser.parse_args()
    args.work_dir.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix="zombie-null-damage-",dir=args.work_dir.resolve()))
    report={"status":"FAIL","native_execution_started":False,"comparison_scope":"AUTHORED_NATIVE_ONLY","original_comparison":None,
            "scope":"Private proposed twelve-profile fixture; Native execution credit exists only if the selected actual executable returns all mandatory groups and final audits pass."}
    before=None;paths=set();expected={};failure=None
    try:
        provenance,profiles=public_inputs()
        executable=args.executable.resolve(strict=True)
        world_helper=ROOT/"tests/tools/entity/test_mob_tick.py"
        construction_helper=world_helper.with_name("test_mob_construction.py")
        sys.path.insert(0,str(world_helper.parent))
        authored=load_module("zombie_authored_world",world_helper)
        resources=out/"resources"
        for name,data in authored.RESOURCES.items():
            p=resources/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
        worlds=out/"worlds"
        for index in range(12):authored.authored_world(worlds/f"case{index}")
        paths={executable,Path(__file__).resolve(),world_helper,construction_helper,
               *source_closure([NATIVE]),*(p for p in REFERENCE.iterdir() if p.is_file()),
               *(p for p in resources.rglob("*") if p.is_file()),*worlds.rglob("*.mcr")}
        original=None
        need(not args.original or args.build_receipt is not None, "Original comparison requires an explicit actual Native build receipt")
        if args.original:
            original=original_audit(args.original,provenance)
            expected=original["expected"]
            paths.update(expected)
            report["original_manifest_negative_controls"]=manifest_negatives(original["receipt"],provenance)
            report["original_audit_scope"]=original["scope"]
        if args.build_receipt:
            build_pin = digest(args.build_receipt)
            compiled_paths, binding, compiled_source, compiled_data = build_inputs(args.build_receipt.resolve(strict=True),executable)
            paths.update(compiled_paths)
            expected.update({local(name):pin for name,pin in compiled_source["input_before_sha256"].items()})
            expected[executable]=binding["executable_sha256"]
            expected[args.build_receipt.resolve()]=build_pin
            expected[local(binding["source_receipt"])]=binding["source_sha256"]
            report["native_build_binding"]=binding
            rejected=[]
            reject("missing_build_target",lambda:successful_build(malformed_build(compiled_source,False),executable),rejected)
            reject("malformed_build_target",lambda:successful_build(malformed_build(compiled_source,True),executable),rejected)
            bad=copy.deepcopy(compiled_source);bad["phases"][0]["exit"]=1
            reject("failed_build_phase",lambda:successful_build(bad,executable),rejected)
            bad=copy.deepcopy(compiled_data);bad["output_sha256"]={}
            reject("missing_selected_executable_output",lambda:build_inputs(args.build_receipt.resolve(),executable,bad),rejected)
            report["build_negative_controls"]={"count":len(rejected),"rejected":rejected}
        before=inventory(paths)
        report["input_before_sha256"]=before
        # Immediate independent expected-pin boundary: new bytes cannot become
        # a stable baseline after a prior original audit passed.
        verify_expected(expected,"immediate pre-execution original expected pins")
        need(digest(executable)==before[str(executable)],"immediate selected executable pin")
        command=[str(executable),str(resources),str(worlds),str(out),str(REFERENCE/"profiles.tsv")]
        report["command"]=command;report["native_execution_started"]=True
        result=subprocess.run(command,stdin=subprocess.DEVNULL,capture_output=True,timeout=240)
        (out/"native.stdout").write_bytes(result.stdout);(out/"native.stderr").write_bytes(result.stderr)
        report["native_exit"]=result.returncode
        report["native_stderr_bytes"]=len(result.stderr)
        checks=validate_stdout(result.stdout,result.returncode,result.stderr)
        native=read_native(out/"native.ndjson")
        report.update(checks=checks,profiles=12,records=native["records"],native_scalar_checks=native["native_scalar_checks"])
        if original:
            summary,selections=compare_original(native,original)
            report["original_comparison"]=summary;report["comparison_scope"]="EXPLICIT_ORIGINAL_SELECTED_RETURNS_AND_PREFIXES"
            report["comparator_negative_controls"]=comparator_negatives(native,original)
            (out/"original-selected.json").write_text(json.dumps(selections,indent=2)+"\n",newline="\n")
            (out/"original-full-outcomes.json").write_text(json.dumps(original["receipt"]["modes"],indent=2)+"\n",newline="\n")
        report["status"]="PASS"
    except Exception as error:
        failure=error
        report["error"]=f"{type(error).__name__}: {error}"
        (out/"driver-error.txt").write_text(traceback.format_exc(),newline="\n")
    finally:
        errors=[]
        try:
            verify_expected(expected,"final original expected pins")
            after=inventory(paths)
            report["input_after_sha256"]=after
            report["inputs_unchanged"]=before is not None and before==after
            need(report["inputs_unchanged"],"actual executable/compiled-helper/fixture/Original input drift")
        except Exception as error:
            errors.append(f"{type(error).__name__}: {error}")
        if errors:
            report["status"]="FAIL";report["final_audit_errors"]=errors
            if failure is None:failure=AssertionError("; ".join(errors))
        report["evidence_chronology"]="Optional Original pre-execution audit and immediate/final expected-pin checks are current post-capture checks; public paths were not capture-start inputs. An explicit successful Native build receipt is required for Original comparison; authored-only runs may omit it and then claim no compile-time source binding."
        (out/"run.json").write_text(json.dumps(report,indent=2)+"\n",newline="\n")
    if failure is not None:
        print(f"Zombie NULL damage FAIL; {out/'run.json'}",file=sys.stderr)
        raise SystemExit(1)
    print(f"Zombie NULL damage: {report['checks']} native checks; {out/'run.json'}")


if __name__ == "__main__":
    main()
