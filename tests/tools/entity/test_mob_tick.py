"""Genuine published Mob ticks and optional unchanged-original typed-state comparison."""
import argparse
import csv
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib

from test_mob_construction import RESOURCES


ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "tests/reference/entity/mob_tick"
KINDS = ("cn", "uz", "fr", "gb", "uw", "wh", "dl", "bx", "ww", "gi", "xt")


def need(value, message):
    if not value:
        raise AssertionError(message)


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1048576), b""):
            result.update(block)
    return result.hexdigest()


def tag(kind, name, payload):
    text = name.encode("utf-8")
    return bytes([kind]) + struct.pack(">H", len(text)) + text + payload


def authored_world(path):
    """Twenty-five actual stored normal Chunks, with the declared Stone plane."""
    region = path / "region"
    region.mkdir(parents=True)
    blocks = bytearray(32768)
    for x in range(16):
        for z in range(16):
            blocks[(x << 11) | (z << 7) | 63] = 1
    arrays = (("Blocks", blocks), ("Data", bytes(16384)), ("SkyLight", bytes(16384)),
              ("BlockLight", bytes(16384)), ("HeightMap", bytes([64]) * 256))
    grouped = {}
    for x in range(-2, 3):
        for z in range(-2, 3):
            grouped.setdefault((x // 32, z // 32), []).append((x, z))
    for (rx, rz), points in grouped.items():
        header = bytearray(8192)
        sectors = []
        for x, z in points:
            fields = [tag(3, "xPos", struct.pack(">i", x)), tag(3, "zPos", struct.pack(">i", z)),
                      tag(4, "LastUpdate", bytes(8))]
            fields += [tag(7, name, struct.pack(">i", len(data)) + data) for name, data in arrays]
            fields += [tag(1, "TerrainPopulated", b"\1"), tag(9, "Entities", b"\x0a\0\0\0\0"),
                       tag(9, "TileEntities", b"\x0a\0\0\0\0")]
            raw = tag(10, "", tag(10, "Level", b"".join(fields) + b"\0") + b"\0")
            payload = b"\2" + zlib.compress(raw)
            record = struct.pack(">i", len(payload)) + payload
            need(len(record) <= 4096, "authored Mob Chunk fits its actual Region sector")
            struct.pack_into(">I", header, 4 * ((x & 31) + 32 * (z & 31)),
                             ((2 + len(sectors)) << 8) | 1)
            sectors.append(record + bytes(4096 - len(record)))
        (region / f"r.{rx}.{rz}.mcr").write_bytes(header + b"".join(sectors))


def read_profiles():
    with (REFERENCE / "profiles.tsv").open(encoding="ascii", newline="") as source:
        rows = csv.DictReader(source, delimiter="\t")
        need(rows.fieldnames == "case kind scene own_seed world_seed callback".split(),
             "authored source caller profile schema")
        profiles = list(rows)
    need([int(row["case"]) for row in profiles] == list(range(len(profiles))) and
         set(row["kind"] for row in profiles) == set(KINDS), "all eleven real Mob profile identities")
    return profiles


def source_records(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    need(isinstance(data, dict) and isinstance(data.get("prefix_records"), list),
         "complete original full-tick and actual invocation-prefix transport required")
    values = data.get("tick_records", data.get("original_rows", data.get("records")))
    need(isinstance(values, list), "complete original Mob tick records required")
    rows = [row for row in values if row.get("event") == "tick"]
    need(rows and [row["case"] for row in rows] == list(range(len(rows))), "ordered original Mob tick records")
    return data, rows


def qualification(count):
    with (REFERENCE / "qualification.tsv").open(encoding="ascii", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        need(reader.fieldnames == "case comparison class method descriptor pc caller".split(),
             "authored source qualification schema")
        rows = list(reader)
    need([int(row["case"]) for row in rows] == list(range(count)) and
         all(row["comparison"] in ("full", "prefix", "source_failure", "callback_failure") for row in rows),
         "every original scenario has an explicit full, failure or child-prefix scope")
    return rows


def original_inputs(path):
    capture = path.with_name("capture.json")
    data = json.loads(capture.read_text(encoding="utf-8"))
    need(data["status"] == "PASS" and data["input_before_sha256"] == data["input_after_sha256"] and
         data["helper_before_sha256"] == data["helper_after_sha256"],
         "audited original source/helper inputs must be unchanged")
    pins = {}
    for name, value in data["input_after_sha256"].items():
        actual = Path(name)
        if os.name != "nt" and re.match(r"^[A-Za-z]:\\", name):
            actual = Path("/mnt") / name[0].lower() / name[3:].replace("\\", "/")
        pins[actual] = value
    pins.update({capture.parent / "classes" / name: value
                 for name, value in data["helper_after_sha256"].items()})
    for actual, expected in pins.items():
        need(actual.is_file() and digest(actual) == expected, f"current original source/helper pin: {actual}")
    return [capture, *pins]


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
    sb = reference_locations(source_before, label + "/source-before")
    sa = reference_locations(source_after, label + "/source-after")
    need(set(nb) == set(sb) and set(na) == set(sa), f"complete cross-phase reference locations: {label}")
    count = 0
    for before in sorted(nb):
        for after in sorted(na):
            need((nb[before] == na[after]) == (sb[before] == sa[after]),
                 f"reference identity continuity differs: {label}/{before}->{after}")
            count += 1
    return count


def source_state(state):
    result = dict(state)
    if result["own_seed"] == "-":
        result["own_seed"] = None  # Original text transport's nullable Random sentinel.
    result["fields"] = {key: scalar(value) for key, value in state["fields"].items()}
    watcher = dict(state["watcher"]) if state["watcher"] is not None else None
    if watcher is not None:
        watcher["records"] = [dict(row, value=scalar(row["value"])) for row in watcher["records"]]
        watcher["size"] = len(watcher["records"])
    result["watcher"] = watcher
    return result


def compare_entity_fields(actual, expected, actual_model, expected_model, label):
    expected_fields = {key: scalar(value) for key, value in expected["fields"].items()}
    need(set(actual["fields"]) == set(expected_fields), f"complete declared field inventory: {label}")
    count = 0
    for key, value in expected_fields.items():
        if key == "sn.aD":
            continue  # Namespace-local numerical Entity ID, independently checked stable.
        candidate = actual["fields"][key]
        if key in REFERENCE_FIELDS or (isinstance(value, dict) and "class" in value):
            need(reference_symbol(candidate, actual_model, label + "/native/" + key) ==
                 reference_symbol(value, expected_model, label + "/original/" + key),
                 f"actual reference class/role/alias differs: {label}/{key}")
        else:
            need(candidate == value, f"actual original {label}/{key}: {candidate} != {value}")
        count += 1
    watcher = dict(expected["watcher"]) if expected["watcher"] is not None else None
    if watcher is not None:
        watcher["records"] = [dict(row, value=scalar(row["value"])) for row in watcher["records"]]
        watcher["size"] = len(watcher["records"])
    need(actual["bounds"] == expected["bounds"] and actual["watcher"] == watcher,
         f"actual bounds/watcher differs: {label}")
    return count + 2


def compare_states(native, original, label):
    original = source_state(original)
    actual_model, expected_model, count = compare_role_models(native, original, label)
    count += compare_entity_fields(native, original, actual_model, expected_model, label)
    for key in ("own_seed", "world_seed", "box_cursor", "vector_cursor", "loaded_entities", "players", "cache_size"):
        need(native[key] == original[key], f"actual original {label}/{key} differs")
        count += 1
    others = original.get("secondary_entities", [])
    need(len(native.get("secondary_entities", [])) == len(others), f"actual secondary owner inventory: {label}")
    for index, (actual, expected) in enumerate(zip(native.get("secondary_entities", []), others)):
        count += compare_entity_fields(actual, expected, actual_model, expected_model, f"secondary{index}/{label}")
        need(actual["own_seed"] == expected["own_seed"], f"actual secondary stream{index}/{label}")
        count += 1
    return count


def comparator_controls(states):
    # These copied observations exercise only rejection mechanics. They are not
    # runtime inputs or substitute expected answers for any paired body.
    passed = []
    def reject(name, function):
        try:
            function()
        except AssertionError:
            passed.append(name)
        else:
            raise AssertionError("reference comparator accepted mutation: " + name)
    expected = states[46]["after"]
    compare_states(expected, expected, "comparator-control-baseline")
    wrong = copy.deepcopy(expected)
    wrong["fields"]["ls.b"] = copy.deepcopy(wrong["reference_roles"]["actor"])
    reject("wrong_same_class_target", lambda: compare_states(wrong, expected, "wrong-target"))
    wrong_class = copy.deepcopy(expected)
    wrong_class["reference_roles"]["secondary0"]["class"] = "gi"
    wrong_class["fields"]["ls.b"]["class"] = "gi"
    reject("wrong_reference_class", lambda: compare_states(wrong_class, expected, "wrong-class"))
    alias = states[48]["after"]
    need(object_token(alias["fields"]["ls.b"], "control-look") == object_token(alias["fields"]["ii.d"], "control-target"),
         "declared aliased look/creature inputs remain available to the rejection control")
    split = copy.deepcopy(alias)
    split["fields"]["ls.b"] = copy.deepcopy(split["reference_roles"]["actor"])
    reject("split_look_creature_alias", lambda: compare_states(split, alias, "alias-split"))
    before, after = states[0]["before"], states[0]["after"]
    compare_identity_transition(before, after, before, after, "temporal-control-baseline")
    for suffix, field in (("bounds", "sn.aW"), ("random", "sn.bs"), ("watcher", "sn.bD")):
        replacement = copy.deepcopy(after)
        value = replacement["reference_roles"]["actor." + suffix]
        actual = int(value["identity"], 16) + 4096
        used = {cell["identity"] for cell in replacement["reference_roles"].values()}
        while f"{actual:016x}" in used:
            actual += 4096
        value["identity"] = f"{actual:016x}"
        replacement["fields"][field] = copy.deepcopy(value)
        # Same class, value and per-state role graph alone intentionally pass.
        compare_states(replacement, after, "fresh-same-value-" + suffix)
        reject("replaced_" + suffix + "_between_states", lambda candidate=replacement:
               compare_identity_transition(before, candidate, before, after, "replaced-" + suffix))
    return {"rejected": passed, "count": len(passed),
            "scope": "Copied observation mutations only: wrong same-class target, wrong class, split aliases and fresh same-class/same-value bounds/Random/watcher replacements must fail the actual comparison path; no mutated evidence executes a Mob body."}


def observer_rows(values):
    return ["MOB_" + value for value in values]


def caller_matches(site, text):
    match = re.fullmatch(r"([^.]+)\.(.*?)([0-9]+)", text)
    return match is not None and (site["class"], site["method"], int(site["pc"])) == (
        match.group(1), match.group(2), int(match.group(3)))


def choose_prefix(records, index, spec):
    selected = [row for row in records if row["case"] == index and all(
        str(row["site"][key]) == spec[key] for key in ("class", "method", "descriptor", "pc")) and
        any(caller_matches(site, spec["caller"]) for site in row["source_frames"])]
    need(selected, f"actual source child entry for case{index}/{spec['caller']}")
    return selected[0]  # First reached entry, including first lava hurt before later damage.


def compare_original(data, original, profiles, states, results, events):
    need(len(original) == len(profiles), "matching complete original Mob profile matrix")
    modes = qualification(len(profiles))
    count = 0
    identity_count = 0
    paired = {name: [] for name in ("full_return", "callback_failure", "source_failure", "child_prefix")}
    source_after = {}
    selected = {}
    for index, row in enumerate(original):
        need(row["kind"] == profiles[index]["kind"] and
             row.get("scenario", row.get("scene")) == profiles[index]["scene"] and
             not row["default_constructor_entropy_rebound"] and row["actual_entity_published"],
             "unchanged original Mob/caller/publication identity")
        before, after, result = states[index]["before"], states[index]["after"], results[index]
        count += compare_states(before, row["before"], f"before{index}")
        mode = modes[index]["comparison"]
        if mode == "prefix":
            prefix = choose_prefix(data["prefix_records"], index, modes[index])
            need(result["outcome"] == -1 and not result["returned"] and result["native_policy"] and
                 caller_matches(result["site"], modes[index]["caller"]),
                 f"native must stop at the actual unimplemented invocation{index}")
            count += compare_states(after, prefix["state"], f"child-prefix{index}")
            entries = [phase for phase in data["source_phase_records"] if phase["case"] == index and
                       phase["event"] == "entry" and phase["location"]["method"] == "w_" and
                       phase["location"]["descriptor"] == "()V"]
            need(entries, "actual source tick entry in the same JDI identity namespace as its prefix")
            jdi_before = min(entries, key=lambda phase: phase["sequence"])["state"]
            compare_role_models(row["before"], jdi_before, f"source-VM-role-bridge{index}")
            identity_count += compare_identity_transition(before, after, jdi_before, prefix["state"],
                                                         f"child-prefix-identities{index}")
            need(events[index] == observer_rows(prefix["events"]), "source events before missing child entry")
            need(before["math_seed"] == after["math_seed"] and
                 data["debugger_tick_records"][index]["before"]["math_seed"] == prefix["state"]["math_seed"],
                 "missing-child prefix cannot equate independent constructor or later child entropy")
            selected[index] = prefix
            args = prefix["arguments"]
            if index == 34:
                need(result["drop"]["stored"] and [result["drop"]["item_id"], result["drop"]["count"]] == args,
                     "actual Egg field/count arguments before the missing drop child")
            elif index in (40, 42, 44):
                need(args[0] is None and result["damage"]["stored"] and result["damage"]["source_null"] and
                     result["damage"]["amount"] == args[1], "actual damage null-source and amount arguments")
            else:
                path = result["path"]
                roles = prefix["state"]["reference_roles"]
                need(path["stored"] and path["receiver_is_world"] and args[0] == roles["actor"],
                     "actual path receiver and source actor operands")
                if index in (47, 48):
                    need(path["kind"] == 2 and path["target_is_secondary"] and
                         args[1] == roles["secondary0"] and path["range"] == args[2],
                         "actual target-path Entity and range arguments")
                else:
                    need(path["kind"] == 1 and [path["x"], path["y"], path["z"], path["range"]] == args[1:],
                         "actual winning wander coordinates and range before path invocation")
            paired["child_prefix"].append(index)
        else:
            count += compare_states(after, row["after"], f"after{index}")
            identity_count += compare_identity_transition(before, after, row["before"], row["after"],
                                                         f"tick-identities{index}")
            need(result["outcome"] == row["outcome"] and events[index] == observer_rows(row["events"]),
                 f"actual source completion/failure and event words{index}")
            if mode == "source_failure":
                need(row["failure"] == "java.lang.NullPointerException" and result["source_exception"] and
                     not result["native_policy"] and result["exception_class"] == "java/lang/NullPointerException",
                     "native source-null classification and complete reached prefix")
                paired["source_failure"].append(index)
            elif mode == "callback_failure":
                need(result["outcome"] == -1, "declared native observer must fail after recording its event")
                paired["callback_failure"].append(index)
            else:
                need(result["outcome"] == 1 and result["returned"] and result["living_pc"] == 528,
                     "full source tick completion includes final Living animation")
                paired["full_return"].append(index)
        source_after[index] = {"outcome": row["outcome"], "after": row["after"], "events": row["events"]}
    # A completed real World ray precedes the later path boundary. Keep and
    # compare its entry arguments independently of their later pooled values.
    for index in (47, 48):
        ray = [row for row in data["prefix_records"] if row["case"] == index and
               row["site"]["class"] == "fd" and row["site"]["descriptor"] == "(Lbt;Lbt;)Lvf;"]
        need(ray and len(ray[0]["argument_vectors"]) == 2, "two actual original line-of-sight vectors")
        values = [[vector["bt." + axis]["D"] for axis in ("a", "b", "c")]
                  for vector in ray[0]["argument_vectors"]]
        need(results[index]["ray_calls"] == 1 and results[index]["ray_returned"] and
             [results[index]["ray_start"], results[index]["ray_end"]] == values,
             "actual saved native World ray arguments and completed child receipt")
    summary = {"profiles": len(original), "typed_state_comparisons": count,
        "reference_identity_relations": identity_count,
        **{key + "_profiles": len(value) for key, value in paired.items()}, "cases": paired,
        "events": sum(map(len, events.values())), "actual_ray_argument_words": 12, "child_argument_profiles": 8,
        "scope": "Full-return and source/controlled-callback failures compare their actual complete states. Missing egg/hurt/path descendants compare the native failed after-state to actual suspended original invocation-entry fields, bounds, watchers, streams, pools, frames and events. Every actor/secondary object reference retains its class and is compared through complete logical roles, per-state aliases and before/after identity relations inside each actual process. JDI prefix continuity uses the same JDI VM's actual tick-entry state. Full original after-states/outcomes/events remain separate and are never normalized into native success or failure. Constructor entropy is independently observed before declared caller fields/setSeed inputs; absolute namespace-local Entity IDs are not equated. Controlled native callback IO pairs reached effects without Java Throwable equivalence."}
    return summary, source_after, selected


def read_native(path, profiles):
    rows = [json.loads(line) for line in path.read_text(encoding="ascii").splitlines()]
    states = {index: {} for index in range(len(profiles))}
    results = {}
    controls = []
    for row in rows:
        if row["event"] == "state":
            need(row["case"] in states and row["phase"] not in states[row["case"]], "unique actual state phase")
            states[row["case"]][row["phase"]] = row
        elif row["event"] == "result":
            need(row["case"] not in results, "unique actual tick result")
            results[row["case"]] = row
        else:
            need(row["event"] in ("native_control", "native_state"), "complete native Mob observation inventory")
            controls.append(row)
    need(set(results) == set(states), "every declared Mob tick must return a result")
    for index, phases in states.items():
        need(set(phases) == {"constructor", "before", "after"}, "actual constructor and caller/tick states retained")
        before, after = phases["before"], phases["after"]
        for phase in phases.values():
            need(phase["kind"] == profiles[index]["kind"] and phase["scene"] == profiles[index]["scene"],
                 "actual Mob kind/profile identity")
            reference_locations(phase, f"actual-native{index}/{phase['phase']}")
        need(before["fields"]["sn.aD"] == after["fields"]["sn.aD"] and
             before["math_seed"] == after["math_seed"], "active tick cannot recreate its Entity or consume shared Math")
        need(role_model(before, "native-before")[0] == role_model(after, "native-after")[0],
             "actual actor, World, secondary, permanent bounds, Random and watcher identities remain stable through tick")
        need(before["loaded_entities"] >= 1 and before["players"] == 0 and before["cache_size"] == 25,
             "actual ordinary World publication and stored Chunk context")
        need(before["own_seed"] == (None if profiles[index]["scene"] == "null_own" else
             f"{(int(profiles[index]['own_seed']) ^ 0x5deece66d) & ((1 << 48) - 1):012x}"),
             "declared source own Random.setSeed input")
        if profiles[index]["scene"] == "baseline":
            kind = profiles[index]["kind"]
            # Pinned source branches: environment1000, two distinct II80 draws,
            # then two LS steering floats; UW only environment, XT adds50+2floats.
            own_steps = 1 if kind == "uw" else 4 if kind == "xt" else 5
            need(after["own_seed"] == random_word(0, own_steps) and
                 after["world_seed"] == random_word(89, int(kind == "gi")),
                 "independent source baseline own/World stream progression")
            need(results[index]["outcome"] == 1 and after["fields"]["sn.bt"] == 1 and
                 after["fields"]["sn.c"] is False and after["fields"]["ls.a"] == 1,
                 "complete baseline source environment/counter prefix")
            need(before["fields"]["ls.G"] == {"F": "3c800000"} and
                 before["fields"]["ls.F"] == {"F": "3e800000"} and
                 before["fields"]["ls.aj"] == {"F": "3f000000"},
                 "explicit declared caller fields without constructor entropy rebinding")
        if profiles[index]["scene"] in ("ambient", "ambient_tamed_hurt", "ambient_angry", "peaceful_ambient"):
            kind = profiles[index]["kind"]
            interval = 120 if kind in ("wh", "dl", "bx", "ww", "gi", "xt") else 80
            need(after["fields"]["ls.a"] == -interval, "actual LS/BG/AR ambient reset interval")
        if profiles[index]["scene"] == "peaceful":
            need(after["fields"]["sn.be"] is True and after["fields"]["sn.bt"] == 1 and
                 results[index]["living_pc"] == 528, "peaceful removal follows full super tick")
        if profiles[index]["scene"] in ("peaceful_remote", "peaceful_ambient"):
            need(after["fields"]["sn.be"] is False, "remote guard or prior ambient failure prevents later removal")
    native_results = [row for row in controls if row["event"] == "native_control"]
    need(len(native_results) == 12, "all twelve native ownership/null/limit/eye-height/deep-chain/Piston/class/Math/facing controls")
    return states, results, controls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    args = parser.parse_args()
    profiles = read_profiles()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="mob-tick-", dir=args.work_dir.resolve()))
    resources = out / "resources"
    for name, data in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    worlds = out / "worlds"
    for index in range(len(profiles) + 12):
        authored_world(worlds / f"case{index}")
    executable = args.executable.resolve(strict=True)
    paths = {executable, Path(__file__).resolve(), Path(__file__).with_name("test_mob_construction.py"),
             ROOT / "tests/entity/test_mob_tick.c", ROOT / "tests/world/runtime/test_world_random_tick.c",
             *resources.rglob("*.*"), *worlds.rglob("*.mcr"),
             *(path for path in REFERENCE.iterdir() if path.is_file())}
    for directory in (ROOT / "include", ROOT / "src", ROOT / "tests/support"):
        paths.update(path for path in directory.rglob("*") if path.suffix in (".c", ".h"))
    if args.observations:
        observation = args.observations.resolve(strict=True)
        paths.update([observation, *original_inputs(observation)])
    paths = sorted(path.resolve(strict=True) for path in paths)
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before, "original_comparison": None}
    try:
        command = [str(executable), str(resources), str(worlds), str(out / "native.ndjson"),
                   str(out / "events.txt"), str(REFERENCE / "profiles.tsv"),
                   str(REFERENCE / "scenario-preparation.tsv")]
        report["command"] = command
        result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=270)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        lines = result.stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"Mob tick: ([1-9][0-9]*) checks;([1-9][0-9]*) paired profiles", lines[-1]) if lines else None
        need(result.returncode == 0 and not result.stderr and footer is not None and
             int(footer.group(2)) == len(profiles) and all(line in ("151 recipes", "16 achievements") for line in lines[:-1]),
             f"native genuine Mob tick failed ({result.returncode}); inspect {out}")
        states, results, controls = read_native(out / "native.ndjson", profiles)
        report["comparator_negative_controls"] = comparator_controls(states)
        events = {index: [] for index in range(len(profiles))}
        for line in (out / "events.txt").read_text(encoding="ascii").splitlines():
            values = line.split()
            need(values[0] in ("MOB_SOUND", "MOB_PARTICLE") and int(values[1]) in events and
                 int(values[2]) == len(events[int(values[1])]), "actual ordered Mob presentation event")
            events[int(values[1])].append(line)
        if args.observations:
            data, original = source_records(args.observations)
            summary, full_after, prefixes = compare_original(data, original, profiles, states, results, events)
            report["original_comparison"] = summary
            (out / "original-full-after.json").write_text(json.dumps(full_after, indent=2) + "\n", encoding="utf-8", newline="\n")
            (out / "original-selected-prefixes.json").write_text(json.dumps(prefixes, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", checks=int(footer.group(1)), profiles=len(profiles), native_controls=controls,
                      scope="Eleven genuine concrete Mob owners in actual GAME Worlds with25stored normal Chunks and Stone plane, real Registry/Entity/Math owners, ordinary FD.b publication, typed fields/bounds/watchers and actual private/World streams. No replacement World queries or successful missing path/combat descendants.")
        print(f"Mob ticks: {report['checks']} checks; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        need(unchanged, "actual Mob tick executable/source/helper/fixture inputs changed")


if __name__ == "__main__":
    main()
