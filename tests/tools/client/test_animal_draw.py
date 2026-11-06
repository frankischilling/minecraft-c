"""Genuine prepared Sheep/Pig/Cow/Chicken drawing and optional audited original observations.

Authored resources and Chunk bytes are execution inputs. Original observations
are audited independently and compared after native execution; they never
become model, query or image execution inputs.
"""
import argparse
import collections
import copy
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "tests/reference/client/mob_draw_animals"


def need(value, message):
    if not value:
        raise AssertionError(message)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1048576), b""):
            h.update(block)
    return h.hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def authored_assets(path):
    png = module("mob_draw_png", Path(__file__).with_name("test_startup.py")).authored_png
    skin = lambda x, y: (0x30 if x < 32 else 0xe0, 0x90 if y < 16 else 0x40,
                         0xd0 if x % 8 < 4 else 0x40, 255)
    fur = lambda x, y: (0xe8 if x < 32 else 0x70, 0xc0 if y < 16 else 0x40, 0x50,
                        255 if ((x // 4 + y // 4) & 1) == 0 else 0)

    def shadow(x, y):
        distance = math.sqrt(((x + .5) / 64 - 1) ** 2 + ((y + .5) / 64 - 1) ** 2)
        return 0, 0, 0, int(max(0, 1 - distance) * 180)

    pig = lambda x, y: (0xf0 if x < 32 else 0x90, 0x80 if y < 16 else 0x30, 0x70, 255)
    saddle = lambda x, y: (0x80 if x % 8 < 4 else 0x40, 0x40, 0x20, 255)
    cow = lambda x, y: (0xe0, 0xd0, 0xb0, 255) if (x // 4 + y // 4) % 2 == 0 else (0x50, 0x30, 0x20, 255)
    chicken = lambda x, y: (0xe8 if x < 32 else 0xe0, 0xe0 if y < 16 else 0x80, 0x40, 255)
    for name, width, height, pixel in (("pack.png", 64, 32, skin),
                                      ("mob/sheep.png", 64, 32, skin),
                                      ("mob/sheep_fur.png", 64, 32, fur),
                                      ("misc/shadow.png", 128, 128, shadow), ("mob/pig.png", 64, 32, pig), ("mob/saddle.png", 64, 32, saddle), ("mob/cow.png", 64, 32, cow), ("mob/chicken.png", 64, 32, chicken)):
        target = path / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(png(width, height, pixel))


def authored_world(path):
    # Import its authored NBT/Region transport, never any captured state.
    directory = ROOT / "tests/tools/entity"
    sys.path.insert(0, str(directory))
    try:
        module("mob_draw_world", directory / "test_mob_tick.py").authored_world(path)
    finally:
        sys.path.pop(0)


def inventory(paths):
    return {str(path.resolve()): digest(path) for path in sorted(set(paths))}


def local_path(name):
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", name):
        return Path(name[5].upper() + ":/" + name[7:])
    if os.name != "nt" and re.match(r"^[A-Za-z]:[\\/]", name):
        return Path("/mnt") / name[0].lower() / name[3:].replace("\\", "/")
    return Path(name)


def capture_audit(observations):
    receipt = observations.with_name("capture.json")
    data = json.loads(receipt.read_text(encoding="utf-8"))
    need(data["status"] == "PASS" and data["input_before_sha256"] == data["input_after_sha256"]
         and data["helper_before_sha256"] == data["helper_after_sha256"],
         "original ordinary/JDI capture and immutable source/helper fingerprints")
    need(digest(observations) == data["observations"]["sha256"], "complete original observations digest")
    pins = {name: expected for name, expected in data["input_after_sha256"].items()}
    for name, expected in data["helper_after_sha256"].items():
        pins[str(receipt.parent / "classes" / name)] = expected
    need(len(data["input_after_sha256"]) == 28 and len(data["helper_after_sha256"]) == 4,
         "complete original 28 input/four compiled-helper inventory")
    foreign = []
    current = {}
    local = [receipt, observations]
    for name, expected in pins.items():
        actual = local_path(name)
        if actual.is_file():
            current[name] = digest(actual)
            local.append(actual)
        else:
            need(os.name == "nt" and name.startswith("/usr/"), "original input is unavailable: " + name)
            foreign.append(name)
    if foreign:
        # The original context is Linux. Audit its real SDK bytes in that same
        # filesystem from Windows, without interpreting paths as shell code.
        script = ("import hashlib,json,sys;from pathlib import Path;"
                  "print(json.dumps({p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()"
                  " for p in json.load(sys.stdin)}))")
        result = subprocess.run(["wsl.exe", "-d", "Ubuntu", "--exec", "/usr/bin/python3", "-c", script],
                                input=json.dumps(foreign), capture_output=True, text=True, timeout=60)
        need(result.returncode == 0 and not result.stderr, "read-only original Linux SDK fingerprint audit")
        current.update(json.loads(result.stdout))
    need(current == pins, "current original input/helper drift is never admitted")
    return local, {"capture": str(receipt), "capture_sha256": digest(receipt),
                   "observations_sha256": digest(observations), "input_count": 28,
                   "helper_count": 4, "current_sha256": current}


def device_manifest(data, receipt, primary, original_audit):
    """Validate complete consumed evidence before any target NDJSON is read."""
    normalize = lambda name: str(local_path(str(name)).resolve())
    original_capture = Path(original_audit["capture"])
    java = local_path(data["command"][0]).resolve()
    need(java.name == "java.exe" and java.parent.parent.name == "jdk-17.0.17.10-hotspot"
         and "AnimalDrawTarget" in data["command"], "actual declared Windows JDK17/helper invocation")
    sdk_suffixes = ("org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar",
                    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar",
                    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar",
                    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar")
    sdk = [local_path(next(name for name in primary["input_after_sha256"]
                          if name.replace("\\", "/").endswith(suffix))).resolve()
           for suffix in sdk_suffixes]
    archive = sdk[0].parents[2] / "lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    mandatory = [ROOT / ".local/run_animal_draw_windows_original_20261031.py", ROOT / "b1.7.3.jar",
                 archive, java, java.parent / "server/jvm.dll", java.parent.parent / "lib/modules",
                 *sdk, receipt.parent / "natives/lwjgl64.dll", *REFERENCE.iterdir(),
                 original_capture, original_capture.with_name("observations.json"),
                 *(receipt.parent / "classes" / name for name in primary["helper_after_sha256"]),
                 *(original_capture.parent / "classes" / name for name in primary["helper_after_sha256"]),
                 ROOT / "tests/reference/entity/player/support.py",
                 ROOT / "tests/reference/client/controller/classfile.py", receipt.parent / "normal/classes.txt"]
    required = {normalize(path) for path in mandatory}
    actual = {normalize(name) for name in data["input_after_sha256"]}
    need(len(data["input_after_sha256"]) == len(actual) == 36 and len(required) == 35 and
         required <= actual and len(actual - required) == 1 and
         Path(next(iter(actual - required))).name.lower() == "python.exe",
         "exact supplementary36-input inventory including source, tool, SDK and helper evidence")
    names = {f"normal/{phase}{case}.{extension}" for phase in ("baseline", "draw")
             for case in range(8) for extension in ("png", "rgba")}
    names |= {"normal/" + name for name in ("class-init.log", "classes.txt", "process.stderr",
                                           "process.stdout", "target.ndjson")}
    names |= {"normal/resources/" + name for name in ("pack.png", "misc/shadow.png", "mob/sheep.png", "mob/sheep_fur.png", "mob/pig.png", "mob/saddle.png", "mob/cow.png", "mob/chicken.png")}
    artifacts = {name.replace("\\", "/"): expected for name, expected in data["artifacts"].items()}
    need(len(data["artifacts"]) == len(artifacts) == 45 and set(artifacts) == names,
         "exact45-artifact inventory must include every consumed ordinary target file")
    paths = []
    for name, expected in data["input_after_sha256"].items():
        path = local_path(name)
        need(path.is_file() and digest(path) == expected, "current supplementary source/tool input: " + name)
        paths.append(path)
    for name, expected in artifacts.items():
        path = (receipt.parent / name).resolve(strict=True)
        need(path.is_relative_to(receipt.parent.resolve()) and path.stat().st_size == expected["bytes"]
             and digest(path) == expected["sha256"], "complete immutable supplementary ordinary artifact")
        paths.append(path)
    return paths


def device_audit(receipt, source, original_audit):
    """A separate actual ordinary run may supply GL only on that exact GPU.

    Its model/geometry/texture observations must agree with b independently.
    It never replaces b's read-only debugger, list or query evidence.
    """
    data = json.loads(receipt.read_text(encoding="utf-8"))
    need(data["status"] == "PASS" and data["ordinary_only"] and data["no_JDI_credit"]
         and data["inputs_unchanged"] and data["input_before_sha256"] == data["input_after_sha256"]
         and data["helper_before_sha256"] == data["helper_after_sha256"],
         "supplement must be an actual immutable ordinary source run")
    need(data["original_linux_capture_sha256"] == original_audit["capture_sha256"] and
         data["original_linux_observations_sha256"] == original_audit["observations_sha256"],
         "supplement names the same immutable full original authority")
    primary = json.loads(Path(original_audit["capture"]).read_text(encoding="utf-8"))
    need(data["helper_after_sha256"] == primary["helper_after_sha256"],
         "supplement executed all four byte-identical frozen b helpers")
    paths = [receipt, *device_manifest(data, receipt, primary, original_audit)]
    for name, expected in data["helper_after_sha256"].items():
        path = receipt.parent / "classes" / name
        need(path.is_file() and digest(path) == expected, "current supplementary compiled helper")
        paths.append(path)
    rows = [json.loads(line) for line in (receipt.parent / "normal/target.ndjson").read_text().splitlines()]
    draws = [row for row in rows if row["event"] == "draw"]
    devices = [row for row in rows if row["event"] == "device"]
    textures = [row for row in rows if row["event"] == "texture_lookup"]
    need([row["index"] for row in draws] == list(range(8)) and len(devices) == 1
         and devices == data["devices"] and textures == source["texture_records"],
         "actual complete supplementary calls, device and identical uploads/cache state")
    pair = Pair()
    for index, row in enumerate(draws):
        need(row["outcome"] == 1 and row["failure"] is None and row["gl_error"] == 0
             and row["before"]["entropy"] == row["after"]["entropy"],
             "actual full ordinary return and unchanged own-process entropy")
        for key in ("input", "kind", "outcome", "failure", "nw_position", "nw_words", "gl_error"):
            pair.value(row[key], source["draw_records"][index][key], "supplement/" + key)
        for phase in ("before", "after"):
            a, b = row[phase], source["draw_records"][index][phase]
            pair.graph(a["graph"], b["graph"], f"supplement/{index}/{phase}/graph")
            for key in ("parts", "pool", "pool_storage"):
                pair.value(a[key], b[key], f"supplement/{index}/{phase}/{key}")
            # Keep both complete raw GL snapshots. Only the actual same-device
            # source GL below is compared to native; no b row is overwritten.
    return paths, {"draws": draws, "device": devices[0]}, {
        "capture": str(receipt), "sha256": digest(receipt), "ordinary_only": True,
        "JDI_credit": False, "current_inputs": len(data["input_after_sha256"]),
        "current_helpers": 4, "artifacts": len(data["artifacts"]),
        "independent_b_graph_state_checks": pair.checks, "source_device": devices[0]}


def device_manifest_negatives(receipt, original_audit):
    data = json.loads(receipt.read_text(encoding="utf-8"))
    primary = json.loads(Path(original_audit["capture"]).read_text(encoding="utf-8"))
    results = []
    for name in ("target_omitted", "artifacts_empty", "target_hash", "input_omitted"):
        changed = copy.deepcopy(data)
        target = next(key for key in changed["artifacts"] if key.replace("\\", "/") == "normal/target.ndjson")
        if name == "target_omitted":
            del changed["artifacts"][target]
        elif name == "artifacts_empty":
            changed["artifacts"] = {}
        elif name == "target_hash":
            changed["artifacts"][target]["sha256"] = "0" * 64
        else:
            del changed["input_after_sha256"][next(key for key in changed["input_after_sha256"]
                                                 if key.replace("\\", "/").endswith("/b1.7.3.jar"))]
        try:
            device_manifest(changed, receipt, primary, original_audit)
        except AssertionError:
            results.append({"name": name, "rejected": True})
        else:
            raise AssertionError("device manifest admitted evidence corruption: " + name)
    return results


def matching_device(native, original):
    need(native["vendor"] == original["vendor"] and native["renderer"] == original["renderer"],
         "supplement requires the actual same GPU vendor and renderer")
    # This supplementary capture is explicitly the observed NVIDIA581.42
    # compatibility context. Native core and original compatibility GL version
    # prefixes differ; their actual vendor-driver suffix must match exactly.
    driver = lambda text: re.search(r"\bNVIDIA [0-9]+\.[0-9]+$", text)
    a, b = driver(native["version"]), driver(original["version"])
    need(a is not None and b is not None and a[0] == b[0] == "NVIDIA 581.42",
         "no silent unknown device or driver supplement")
    return {"native": native, "ordinary_original": original, "exact_driver": a[0],
            "scope": "same physical GPU/driver, native core and original compatibility contexts"}


def reference(value):
    if not isinstance(value, dict) or "class" not in value:
        return None
    keys = [key for key in ("identity", "oid") if key in value]
    if not keys:
        return None
    need(len(keys) == 1 and type(value[keys[0]]) in (int, str), "one actual process-local identity")
    return value["class"], value[keys[0]]


class Pair:
    """Injective actual class/alias/temporal pairing; no nonnull collapse."""
    def __init__(self):
        self.forward, self.reverse = {}, {}
        self.native_classes, self.source_classes = {}, {}
        self.lists, self.list_reverse = {}, {}
        self.textures, self.texture_reverse = {0: 0}, {0: 0}
        self.checks = 0
        self.matrix_maximum = 0.0
        self.matrix_signed_zeros = []

    def name(self, actual, expected, kind):
        left = self.lists if kind == "list" else self.textures
        right = self.list_reverse if kind == "list" else self.texture_reverse
        if actual == 0 or expected == 0:
            need(actual == expected == 0, "zero source name is not an allocation")
            return
        need(type(actual) is type(expected) is int and actual > 0 and expected > 0,
             "real positive source " + kind + " names")
        need(actual not in left or left[actual] == expected, "source name continuity: " + kind)
        need(expected not in right or right[expected] == actual, "source name alias partition: " + kind)
        left[actual], right[expected] = expected, actual
        self.checks += 2

    def value(self, actual, expected, label):
        a, b = reference(actual), reference(expected)
        if a is not None or b is not None:
            need(a is not None and b is not None and a[0] == b[0], "reference class: " + label)
            need(a[1] not in self.native_classes or self.native_classes[a[1]] == a[0],
                 "one actual class per native identity: " + label)
            need(b[1] not in self.source_classes or self.source_classes[b[1]] == b[0],
                 "one actual class per source identity: " + label)
            self.native_classes[a[1]], self.source_classes[b[1]] = a[0], b[0]
            need(a not in self.forward or self.forward[a] == b, "reference continuity: " + label)
            need(b not in self.reverse or self.reverse[b] == a, "reference alias partition: " + label)
            self.forward[a], self.reverse[b] = b, a
            self.checks += 3
            return
        if label.endswith("/ps.o"):
            self.name(actual, expected, "list")
            return
        need(type(actual) is type(expected), "typed value: " + label)
        if isinstance(expected, dict):
            need(set(actual) == set(expected), "complete field inventory: " + label)
            for key in sorted(expected):
                self.value(actual[key], expected[key], label + "/" + key)
        elif isinstance(expected, list):
            need(len(actual) == len(expected), "complete array extent: " + label)
            for index, (a, b) in enumerate(zip(actual, expected)):
                self.value(a, b, label + "/" + str(index))
        else:
            need(actual == expected, f"exact value {label}: {actual!r} != {expected!r}")
            self.checks += 1

    def graph(self, actual, expected, label):
        need(set(actual) == set(expected) == {"roots", "objects"}, "complete selected graph shape")
        aobjects = {reference(row): row for row in actual["objects"]}
        bobjects = {reference(row): row for row in expected["objects"]}
        need(None not in aobjects and None not in bobjects and len(aobjects) == len(actual["objects"])
             and len(bobjects) == len(expected["objects"]) == len(aobjects),
             "all selected actual graph objects, unique identities")
        self.value(actual["roots"], expected["roots"], label + "/roots")
        visited = set()
        while True:
            pending = [(a, b) for a, b in self.forward.items() if (a, b) not in visited
                       and (a in aobjects or b in bobjects)]
            if not pending:
                break
            for a, b in pending:
                visited.add((a, b))
                need(a in aobjects and b in bobjects, "identical expanded/opaque selected scope")
                left = {k: v for k, v in aobjects[a].items() if k not in ("identity", "oid")}
                right = {k: v for k, v in bobjects[b].items() if k not in ("identity", "oid")}
                self.value(left, right, label + "/" + a[0] + "/" + str(len(visited)))
        need(len(visited) == len(aobjects), "no omitted or unreachable selected object")

    def gl(self, actual, expected, label):
        need(set(actual) == set(expected), "complete selected GL field inventory")
        self.name(actual["texture"], expected["texture"], "texture")
        for key in expected:
            if key in ("modelview", "projection"):
                # Prepared view/projection stores may differ at the final bit
                # between compatibility GL and the core CPU matrix producer.
                # Four ULP is a finite matrix-only allowance. Raw geometry,
                # normals, colors, all model arithmetic and source enums stay exact.
                need(len(actual[key]) == len(expected[key]) == 16, "complete real matrix")
                for index, (a, b) in enumerate(zip(actual[key], expected[key])):
                    af, bf = (struct.unpack(">f", bytes.fromhex(v))[0] for v in (a, b))
                    need(math.isfinite(af) and math.isfinite(bf), "finite prepared matrices")
                    ai, bi = int(a, 16), int(b, 16)
                    if a != b and af == bf == 0:
                        self.matrix_signed_zeros.append({"location": f"{label}/{key}/{index}",
                                                         "native": a, "original": b})
                    need(a == b or af == bf == 0 or (af != 0 and bf != 0 and (ai >> 31) == (bi >> 31)
                                    and abs(ai - bi) <= 4), f"matrix four-ULP bound {label}/{key}/{index}")
                    self.matrix_maximum = max(self.matrix_maximum, abs(af - bf))
                    self.checks += 1
            elif key != "texture":
                self.value(actual[key], expected[key], label + "/" + key)


def parse_native(path):
    rows = [json.loads(line) for line in path.read_text(encoding="ascii").splitlines()]
    groups = {name: [row for row in rows if row["event"] == name] for name in
              ("context", "device", "texture", "state", "draw", "list", "queries", "control", "texture_calls", "animation", "model_work")}
    need(sum(map(len, groups.values())) == len(rows), "complete declared native transport")
    need(len(groups["device"]) == len(groups["context"]) == 1, "actual native device and raw context")
    need(groups["context"][0] == {"event": "context", "RAW_app": True, "namespace_attached": True,
         "canonical_nonnull": True, "static_holds": 1, "default_profile_rebound": False}, "actual RAW/static ownership")
    need([r["index"] for r in groups["texture"]] == list(range(7)), "seven actual upload misses and hits")
    for group in ("draw", "queries", "texture_calls", "animation"):
        need([r["index" if group == "draw" else "case"] for r in groups[group]] == list(range(8)), "all actual calls: " + group)
    need([(r["case"], r["phase"]) for r in groups["state"]] == [(i,p) for i in range(8) for p in ("before","after")], "sixteen actual snapshots")
    need(len(groups["list"]) == 35 and len(groups["control"]) == 12, "35 compiled parts and12 retained/failure controls")
    need([(row["case"],row["phase"]) for row in groups["model_work"]] ==
         [(i,phase) for i in range(8) for phase in (("primary","pass0") if i in (0,7) else ("primary",))],
         "actual selected model work for every real primary/pass invocation")
    need([r["name"] for r in groups["control"]] == ["retained_graph"]*8+["allocation_zero","null_actor","foreign_actor","shadow_limit"], "finite ownership controls")
    for i, r in enumerate(groups["control"][:8]):
        need(r == {"event":"control","name":"retained_graph","case":i,"nested_busy":True,"class_retire_busy":True,"app_retire_busy":True,"render_borrows_restored":True}, "real owner refusal/exact restoration")
    need(groups["control"][8] == {"event":"control","name":"allocation_zero","admission_only":True} and
         groups["control"][9] == {"event":"control","name":"null_actor","exception":"java/lang/NullPointerException","pc":1} and
         groups["control"][10] == {"event":"control","name":"foreign_actor","native_admission":True}, "precise admission/source-null controls")
    limit = groups["control"][11]
    need(limit == {"event":"control","name":"shadow_limit","primary_returned":True,"native_policy":True,"unfinished_NW_preserved":True,
                  "manager_status":"LIMIT","living_status":"LIMIT","quads":0,"vertices":0,
                  "site":{"class":"bw","method":"a","descriptor":"(Luu;DDDIIIFFDDD)V","pc":241},
                  "operation":"Shadow reached native quad ceiling"}, "exact shadow native ceiling and preserved prefix")
    pools = [288,432,432,648,648,840,840,840]
    compiled = [12,6,6,9,9,8,8,12]
    for i, draw in enumerate(groups["draw"]):
        words = 48 if i in (5,6) else 432
        need(draw["outcome"] == 1 and draw["gl_error"] == 0 and draw["nw_position"] == len(draw["nw_words"]) == words, "complete actual final shadow")
        before, after = groups["state"][i*2:i*2+2]
        need(before["entropy"] == after["entropy"] and after["pool"] == pools[i], "actual unchanged streams/real pooled compilation")
        need(sum(p["is_list"] for p in after["parts"]) == compiled[i], "actual visible parts; unused saddle not compiled")
        if i in (2,4,6):
            need(before["parts"] == groups["state"][(i-1)*2+1]["parts"], "real reuse identities")
    need(groups["state"][1]["parts"] == groups["state"][14]["parts"] == groups["state"][15]["parts"], "same Sheep after other species")
    for row in groups["list"]:
        need(len(row["batches"]) == 6 and all(len(b["words"]) == 48 and b["vertices"] == 6 and b["mode"] == 4 for b in row["batches"]), "all210 complete model faces")
    return groups


def source_queries(source, case):
    # Fixed selected direct invocation boundaries, authored from pinned Code.
    # Inner World implementation calls remain retained in source_events and
    # are deliberately outside this direct-call producer's equality claim.
    selected = {("sn", "a", "(F)F", 118), ("sn", "a", "(F)F", 133),
                ("bw", "c", "(Lsn;DDDFF)V", 253), ("bw", "c", "(Lsn;DDDFF)V", 271),
                ("bw", "a", "(Luu;DDDIIIFFDDD)V", 6),
                ("bw", "a", "(Luu;DDDIIIFFDDD)V", 44)}
    result, stack = [], []
    for event in source["source_events"]:
        if event.get("case") != case or event["event"] not in ("entry", "return"):
            continue
        if event["event"] == "entry":
            frames = event["source_frames"]
            caller = frames[1] if len(frames) > 1 else None
            key = tuple(caller[k] for k in ("class", "method", "descriptor", "pc")) if caller else None
            row = None
            if key in selected:
                row = {"receiver": event["receiver"], "method": event["location"],
                       "caller": caller, "arguments": event["arguments"]}
                result.append(row)
            stack.append((event["location"], row))
        else:
            need(stack, "complete observed source nesting")
            location, row = stack.pop()
            need(all(location[k] == event["location"][k] for k in ("class", "method", "descriptor")),
                 "actual source query return pairing")
            if row is not None:
                row["value"] = event["result"]
    need(not stack and all("value" in row for row in result), "complete selected source query returns")
    need(len(result) == ([51,49,49,49,49,9,9,51][case]), "finite actual direct boundary count")
    return result


def compare(groups, source, device_source=None):
    pair = Pair()
    block_receivers, block_reverse = {}, {}
    for actual, expected in zip(groups["texture"], source["texture_records"]):
        need(actual["index"] == expected["index"] and actual["key"] == expected["key"], "real selected resource keys")
        pair.name(actual["name"], expected["name"], "texture")
        pair.name(actual["cached_name"], expected["cached_name"], "texture")
        pair.value(actual["cache_before"], expected["before"]["cache_size"], "texture/cache_before")
        pair.value(actual["cache_after"], expected["after"]["cache_size"], "texture/cache_after")
        for key in ("wrap_s", "wrap_t", "min", "mag"):
            pair.value(actual[key], expected["after"][key], "texture/" + key)
        pair.value(actual["transfer_hex"], expected["transfer_hex"], "texture/decoded_transfer")
    query_count = 0
    for index, expected in enumerate(source["draw_records"]):
        for phase_index, phase in enumerate(("before", "after")):
            actual, original = groups["state"][index * 2 + phase_index], expected[phase]
            pair.graph(actual["graph"], original["graph"], f"case{index}/{phase}/graph")
            for key in ("parts", "pool", "pool_storage"):
                pair.value(actual[key], original[key], f"case{index}/{phase}/{key}")
            gl_original = original["gl"] if device_source is None else device_source["draws"][index][phase]["gl"]
            pair.gl(actual["gl"], gl_original, f"case{index}/{phase}/gl")
        for key in ("outcome", "nw_position", "nw_words", "gl_error"):
            pair.value(groups["draw"][index][key], expected[key], f"case{index}/{key}")
        animation = groups["animation"][index]
        age = [row["result"] for row in source["source_events"] if row.get("case") == index and
               row["event"] == "return" and row["location"]["class"] == ("ip" if index in (5,6) else "gv") and
               row["location"]["method"] == ("a" if index in (5,6) else "c") and
               row["location"]["descriptor"] == ("(Lww;F)F" if index in (5,6) else "(Lls;F)F")]
        need(len(age) == 1, "actual unique source animation return")
        pair.value(animation["age"], age[0], f"case{index}/actual_animation_return")
        need(animation["flap_arguments_read"] == animation["flap_math_returned"] == (index in (5,6)),
             "actual Chicken branch reached and returned")
        f32 = lambda value: struct.unpack(">f", struct.pack(">f", value))[0]
        encoded = lambda value: {"F": struct.pack(">f", value).hex()}
        if index in (5,6):
            partial = .625 if index == 6 else .375
            pair.value(animation["flap_first"], encoded(f32(.25 + f32(f32(.75-.25)*partial))), "independent caller-input flap first")
            pair.value(animation["flap_second"], encoded(f32(.5 + f32(f32(1.25-.5)*partial))), "independent caller-input flap second")
        else:
            pair.value(animation["flap_first"], {"F":"00000000"}, "inactive flap first")
            pair.value(animation["flap_second"], {"F":"00000000"}, "inactive flap second")
        originals = source_queries(source, index)
        actuals = groups["queries"][index]["queries"]
        need(len(actuals) == len(originals), "complete ordered direct query inventory")
        bridge = source["case_records"][index]["entry_bridge"]
        reverse_bridge = {reference(value): int(key) for key, value in bridge.items()}
        native_world = groups["state"][index * 2]["graph"]["roots"]["world"]
        for ordinal, (actual, original) in enumerate(zip(actuals, originals)):
            label = f"case{index}/query{ordinal}"
            for key in ("method", "caller", "arguments", "value"):
                pair.value(actual[key], original[key], label + "/" + key)
            need(actual["invoked"] and actual["returned"] and not actual["failed"] and
                 not actual["native_policy"] and not actual["source_exception"] and
                 actual["exception_class"] is None, "actual direct call completed")
            token = reference(original["receiver"])
            if token[0] == "fd":
                need(actual["receiver_role"] == "world" and actual["receiver"] == native_world["identity"],
                     "actual held World receiver")
                need(token in reverse_bridge, "observed World identity bridge")
                pair.value(native_world, {"class": "fd", "identity": reverse_bridge[token]}, label + "/receiver")
            else:
                # Actual source registry Stone is eo. Native role was obtained
                # by identity search in its real Registry m[], not by query value.
                need(token[0] == "eo" and actual["receiver_role"] == "block:1", "actual canonical Stone role")
                # The query API exposes native Block identity, not its dynamic
                # source class. Pair the real Registry role/alias separately;
                # do not manufacture a native eo class observation.
                pointer = actual["receiver"]
                need(pointer not in block_receivers or block_receivers[pointer] == token,
                     "actual Block receiver continuity")
                need(token not in block_reverse or block_reverse[token] == pointer,
                     "actual Block receiver alias partition")
                block_receivers[pointer], block_reverse[token] = token, pointer
            query_count += 1
    for ordinal, (actual, original) in enumerate(zip(groups["list"], source["list_records"])):
        bridge = source["case_records"][original["case"]]["after_bridge"]
        reverse_bridge = {reference(value): int(key) for key, value in bridge.items()}
        part = reference(original["part"])
        need(part in reverse_bridge, "observed actual compiled PS bridge")
        pair.value(actual["part"], {"class": "ps", "identity": reverse_bridge[part]}, f"list{ordinal}/owner")
        pair.name(actual["list"], original["list"], "list")
        need(actual["commands"] == 6, "six real immutable draw commands in each source list")
        need(len(actual["batches"]) == len(original["batches"]) == 6, "complete list batch extent")
        for face, (a, b) in enumerate(zip(actual["batches"], original["batches"])):
            pair.value(a["vertices"], b["batch"]["nw.h"], f"list{ordinal}/face{face}/vertices")
            pair.value(a["mode"], b["batch"]["nw.p"], f"list{ordinal}/face{face}/mode")
            pair.value(a["words"], b["batch"]["words"], f"list{ordinal}/face{face}/words")
            need(a["arrays"] == 11, "actual vertex/texture/normal arrays retained in list batch")
    texture_count = compare_texture_calls(pair, groups, source)
    model_count = compare_model_work(pair, groups, source)
    return {"typed_graph_state_checks": pair.checks, "expanded_objects": sum(len(r["graph"]["objects"]) for r in groups["state"]),
            "query_attempts": query_count, "compiled_lists": 35, "model_faces": 210,
            "complete_shadow_words": 6 * 432 + 2 * 48, "matrix_maximum_absolute_error": pair.matrix_maximum,
            "matrix_allowance": "four float ULP plus numeric equality of opposite signed zeros, prepared view/projection only",
            "matrix_opposite_signed_zeros": pair.matrix_signed_zeros,
            "GL_authority": "complete Linux ordinary/JDI primary" if device_source is None else "separate audited same-device ordinary source; no supplementary JDI credit",
            "identity_pairs": len(pair.forward), "list_name_pairs": len(pair.lists),
            "texture_name_pairs": len(pair.textures) - 1, "direct_texture_calls": texture_count,
            "actual_animation_returns": 8, "selected_model_work_records": model_count}


def compare_model_work(pair, groups, source):
    for work in groups["model_work"]:
        case=work["case"]
        model=source["draw_records"][case]["after"]["graph"]["roots"]["primary" if work["phase"]=="primary" else "fur"]
        label=f"case{case}/{work['phase']}/model_work"
        pair.value(work["model"],model,label+"/model")
        root=source["case_records"][case]["after_bridge"][str(model["identity"])]
        entries=[row for row in source["source_events"] if row.get("case")==case and row["event"]=="entry" and row["receiver"]==root and row["location"]["descriptor"]=="(FFFFFF)V"]
        counts=collections.Counter((row["location"]["class"],row["location"]["method"]) for row in entries)
        cow=model["class"]=="hh";chicken=model["class"]=="td"
        expected=({("hh","a"):1,("nn","a"):1,("hh","b"):1,("nn","b"):1} if cow else
                  {("td","a"):1,("td","b"):1} if chicken else {("nn","a"):1,("nn","b"):1})
        need(counts==expected,"actual source selected virtual pose/render invocation count and model identity")
        pair.value(work["pose_stores"],11 if cow or chicken else 7,label+"/pose_stores")
        pair.value(work["math_calls"],2 if chicken else 4,label+"/math_calls")
        for key in ("part_calls","part_returns"):
            pair.value(work[key],9 if cow else 8 if chicken else 6,label+"/"+key)
        returns=[row for row in source["source_events"] if row.get("case")==case and row["event"]=="return" and row["receiver"]==root and row["location"]["descriptor"]=="(FFFFFF)V"]
        for key,method in (("pose_site","b"),("render_site","a")):
            declaring="hh" if cow else "td" if chicken else "nn"
            returned=[row["location"] for row in returns if row["location"]["class"]==declaring and row["location"]["method"]==method]
            need(len(returned)==1,"actual selected source method return")
            pair.value(work[key],returned[0],label+"/"+key)
        if cow:
            need(work["nn_pose_returned"] and work["nn_render_returned"],"real nested superclass completion")
    return len(groups["model_work"])


def compare_texture_calls(pair, groups, source):
    total = 0
    for case, record in enumerate(source["case_records"]):
        expected = []
        for direct in record["direct_living_texture_calls"]:
            expected.append(("living", direct["lookup"], direct["bind"], direct["helper"]["source_frames"][1]))
        events = [row for row in source["source_events"] if row.get("case") == case and row["event"] == "entry"]
        shadow_lookup = [row for row in events if row["location"]["class"] == "ji" and row["location"]["method"] == "b" and
                         row["location"]["descriptor"] == "(Ljava/lang/String;)I" and len(row["source_frames"]) > 1 and
                         row["source_frames"][1] == {"class":"bw","method":"c","descriptor":"(Lsn;DDDFF)V","pc":30}]
        shadow_bind = [row for row in events if row["location"]["class"] == "ji" and row["location"]["method"] == "b" and
                       row["location"]["descriptor"] == "(I)V" and len(row["source_frames"]) > 1 and
                       row["source_frames"][1] == {"class":"bw","method":"c","descriptor":"(Lsn;DDDFF)V","pc":33}]
        need(len(shadow_lookup) == len(shadow_bind) == 1, "one actual source shadow lookup/bind")
        expected.append(("post",shadow_lookup[0],shadow_bind[0],shadow_lookup[0]["source_frames"][1]))
        actual = groups["texture_calls"][case]["calls"]
        need(len(actual) == len(expected), "complete direct texture attempt inventory")
        bridge = {reference(value):int(key) for key,value in record["entry_bridge"].items()}
        for ordinal, (a,(phase,lookup,bind,caller)) in enumerate(zip(actual,expected)):
            label = f"case{case}/texture{ordinal}"
            need(a["phase"] == phase and a["returned"] and lookup["receiver"] == bind["receiver"], "actual helper/engine/bind return order")
            engine = reference(lookup["receiver"])
            need(engine in bridge, "actual captured source Engine bridge")
            pair.value(a["engine"], {"class":engine[0],"identity":bridge[engine]}, label+"/engine")
            key = lookup["argument_references"][0]
            need(key["class"] == "java.lang.String" and key["string"] == lookup["arguments"][0], "actual source String identity/content")
            pair.value(a["key"], {"class":"java.lang.String","identity":"source-jdi-string:"+str(key["oid"])}, label+"/key_alias")
            pair.value(a["units"], list(key["string"].encode("utf-16-be"))[1::2], label+"/key_units")
            pair.value(a["caller"], caller, label+"/caller")
            pair.value(a["lookup_site"], lookup["source_frames"][1], label+"/lookup_site")
            pair.value(a["bind_site"], bind["source_frames"][1], label+"/bind_site")
            pair.name(a["texture_id"], bind["arguments"][0], "texture")
            total += 1
    need(total == 18, "actual18 direct living/post texture routes")
    return total


def negatives(groups, source, device_source=None):
    mutations = []
    for name in ("renderer_class", "model_alias", "part_replacement", "query_order", "missing_face", "texture_role",
                 "missing_saddle_lookup", "saddle_identity_split", "flap_return", "duplicate_cow_pose_work"):
        changed = copy.deepcopy(groups)
        graph = changed["state"][0]["graph"]
        if name == "renderer_class":
            graph["roots"]["renderer"]["class"] = "gd"
        elif name == "model_alias":
            graph["roots"]["fur"] = copy.deepcopy(graph["roots"]["primary"])
        elif name == "part_replacement":
            changed["state"][1]["parts"][0]["part"]["identity"] = "fresh-negative-PS"
        elif name == "query_order":
            queries = changed["queries"][0]["queries"]
            queries[0], queries[1] = queries[1], queries[0]
        elif name == "missing_face":
            changed["list"][0]["batches"].pop()
        elif name == "texture_role":
            changed["texture"][2]["cached_name"] = changed["texture"][0]["name"]
        elif name == "missing_saddle_lookup":
            changed["texture_calls"][1]["calls"].pop(0)
        elif name == "saddle_identity_split":
            changed["texture_calls"][1]["calls"][1]["key"]["identity"] = "fresh-wrong-saddle-object"
        elif name == "flap_return":
            changed["animation"][5]["age"] = {"F":"00000000"}
        else:
            work=next(row for row in changed["model_work"] if row["case"]==3)
            work["pose_stores"]+=11
            work["math_calls"]+=4
        try:
            compare(changed, source, device_source)
        except AssertionError:
            mutations.append({"name": name, "rejected": True})
        else:
            raise AssertionError("comparator admitted meaningful corruption: " + name)
    return mutations


def pixel_checks(out):
    images = {}
    for index in range(8):
        for phase in ("baseline", "draw"):
            path = out / f"{phase}{index}.rgba"
            need(path.stat().st_size == 262144, "complete actual RGBA capture")
            images[phase,index] = path.read_bytes()
    need(images["draw",0] == images["draw",7] and images["draw",1] == images["draw",2] and
         images["draw",3] == images["draw",4], "actual first/reuse/revisit raster equality")
    need(images["draw",5] != images["draw",6], "actual changed Chicken partial/flap visibly changes raster")
    result = []
    for index in range(8):
        before, after = images["baseline",index], images["draw",index]
        changed = sum(before[i:i+4] != after[i:i+4] for i in range(0,len(after),4))
        darkened = sum(all(after[i+c] <= before[i+c] for c in range(3)) and any(after[i+c] < before[i+c] for c in range(3)) for i in range(0,len(after),4))
        need(changed > 256 and darkened > (16 if index in (5,6) else 128), "real visible actor and shadow")
        result.append({"case":index,"changed":changed,"darkened":darkened,"sha256":hashlib.sha256(after).hexdigest()})
    return {"scope":"native actual readback/reuse/flap visibility; no original pixel equality", "rows":result}


def component_controls(executable, resources, assets, directories):
    results=[]
    for mode,directory in directories.items():
        environment=os.environ.copy()
        environment["ALSOFT_DRIVERS"]="null"
        for name in ("APPDATA","HOME","XDG_DATA_HOME","XDG_CONFIG_HOME","XDG_CACHE_HOME"):
            path=directory/name.lower();path.mkdir();environment[name]=str(path)
        command=[str(executable),str(resources),str(assets),str(directory/"world"),str(directory),mode]
        result=subprocess.run(command,cwd=directory,env=environment,capture_output=True,text=True,timeout=180)
        (directory/"native.stdout.txt").write_text(result.stdout,newline="\n")
        (directory/"native.stderr.txt").write_text(result.stderr,newline="\n")
        need(result.returncode==0 and result.stderr in ("","[ALSOFT] (WW) No capture backend available!\n"),
             "fresh entered-component native assertions: "+mode)
        footer=re.fullmatch(r"Animal draw: (\d+) checks; genuine RAW App/World/Animals/TH",result.stdout.splitlines()[-1])
        need(footer is not None,"actual component assertion footer")
        rows=[json.loads(line) for line in (directory/"native.ndjson").read_text().splitlines()]
        controls=[row for row in rows if row["event"]=="control"]
        need(len([row for row in rows if row["event"]=="texture"])==7 and not any(row["event"]=="draw" for row in rows),
             "fresh real constructed/component context without hidden baseline draws")
        if mode=="component-ko":
            need([row["model"] for row in controls]==["eh","hh","td"] and all(row=={
                "event":"control","name":"component_ko_empty","model":row["model"],"actor_null":True,
                "services_null":True,"source_pc":0,"returned":True,"model_and_parts_unchanged":True,
                "no_math_parts_gl_pool_rng":True} for row in controls),"three actual inherited KO no-read returns")
        elif mode=="component-hh-null":
            need(controls==[{"event":"control","name":"component_hh_null_horn","source_site":"hh.b(FFFFFF)V:24",
                "nn_pose_return":115,"selected_pose_returned":False,"nn_render_returned":False,
                "selected_render_returned":False,"stores":7,"math_calls":4,"part_calls":0,
                "ancestors":["hh.a:10","nn.a:10"],"caught_source_exception":"java/lang/NullPointerException",
                "native_frontier":"gv.a:747","null_survives_retirement":True,"prefix_survives_retirement":True}],
                "actual nested HH failure, completed NN prefix and explicit native printing frontier")
        elif mode in ("component-va-cast","component-ip-cast"):
            need(controls==[{"event":"control","name":"component_wrong_bridge","renderer":"va" if mode=="component-va-cast" else "ip",
                "actual_actor":"dl","source_pc":2,"exception":"java/lang/ClassCastException","before_push":True,
                "actual_retained_scope":True,"checked_retirement":True}],"actual wrong-kind checkcast before any push")
        else:
            need(mode=="component-ip-math" and controls==[{"event":"control","name":"component_chicken_math_limit",
                "declared_native_service_bytes":4096,"capture_succeeded":True,"source_site":"ip.a(Lww;F)F:36",
                "child_site":"in.<clinit>:2","flap_first":"3ee00000","flap_second":"3f480000","native_policy":True,
                "source_exception":False,"failed_phase_survives_retirement":True,"world_math_unchanged":True,
                "prefix_survives_retirement":True}],"actual captured bounded Math initializer limit after real flap reads")
        results.append({"mode":mode,"checks":int(footer[1]),"controls":controls,"command":command,
                        "scope":"fresh genuine native component; Code-qualified control, not an original runtime outcome pair"})
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    parser.add_argument("--device-observations", type=Path,
                        help="audited supplementary ordinary capture.json; GL only, requires --observations")
    args = parser.parse_args()
    need(args.device_observations is None or args.observations is not None,
         "device supplement requires the full ordinary/JDI original authority")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="animal-draw-", dir=args.work_dir.resolve()))
    assets = out / "assets"
    world = out / "world"
    authored_assets(assets)
    authored_world(world)
    component_directories={}
    for mode in ("component-ko","component-hh-null","component-va-cast","component-ip-cast","component-ip-math"):
        directory=out/mode;directory.mkdir();authored_world(directory/"world");component_directories[mode]=directory
    executable = args.executable.resolve(strict=True)
    resources = ROOT / "tests/client/startup/fixtures"
    inputs = [executable, Path(__file__), ROOT / "tests/client/render/test_animal_draw.c",
              Path(__file__).with_name("test_startup.py"), ROOT / "tests/tools/entity/test_mob_tick.py",
              ROOT / "tests/tools/entity/test_mob_construction.py"]
    inputs.extend(ROOT / "tests/client/startup" / name for name in
                  ("test_application_startup.c", "controller_boundary_observer.h",
                   "application_startup_statistics_controls.inc", "application_startup_effects_controls.inc",
                   "application_startup_rendering_controls.inc"))
    for directory in (ROOT / "src", ROOT / "include", REFERENCE, resources, assets):
        inputs.extend(p for p in directory.rglob("*") if p.is_file())
    inputs.extend(world.rglob("*.mcr"))
    for directory in component_directories.values():
        inputs.extend((directory/"world").rglob("*.mcr"))
    source = None
    audit = None
    device_source = None
    device_report = None
    if args.observations:
        original_inputs, audit = capture_audit(args.observations.resolve(strict=True))
        inputs.extend(original_inputs)
        source = json.loads(args.observations.read_text(encoding="utf-8"))
        if args.device_observations:
            device_paths, device_source, device_report = device_audit(
                args.device_observations.resolve(strict=True), source, audit)
            inputs.extend(device_paths)
    before = inventory(inputs)
    report = {"status": "FAIL", "input_before_sha256": before, "original_capture_audit": audit,
              "device_observations_audit": device_report,
              "comparison_limits": ["Native live GL invocation order is not exposed; compare final GL, committed lists/batches and ordered selected World/Block boundaries. Complete original GL order remains in the audited original artifact.",
                                    "Model work counters aggregate within each selected entered render and are paired to original model identity/pose/Math counts; global cross-entered invocation order is not recorded natively.",
                                    "Block query receivers pair actual Registry m[1] identity/aliases and declaring method; native dynamic Block class is not exposed by the query API.",
                                    "Raw constructor/default entropy is retained independently; only within-process active-draw stability is asserted.",
                                    "Original raw Entity fieldsets remain source-only; native pairs actual Entity class/identity, prepared geometry effects and unchanged streams, without equating independent constructor-derived fields.",
                                    "Native pixels independently qualify visible model/shadow and reuse; no original pixel equality."],
              "scope": "genuine prepared RAW App/static input, real constructors and stored GAME World/Sheep/Pig/Cow/Chicken/TH; finite first/reuse/shadow draw bodies; independent raw constructor entropy; no App startup/publication, other Mob, hurt/fire/debug true-branch or complete-frame claim"}
    try:
        environment = os.environ.copy()
        environment["ALSOFT_DRIVERS"] = "null"
        for name in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
            target = out / name.lower()
            target.mkdir()
            environment[name] = str(target)
        command = [str(executable), str(resources), str(assets), str(world), str(out)]
        result = subprocess.run(command, cwd=out, env=environment, capture_output=True, text=True, timeout=180)
        (out / "native.stdout.txt").write_text(result.stdout, newline="\n")
        (out / "native.stderr.txt").write_text(result.stderr, newline="\n")
        report.update(command=command, exit=result.returncode, stderr=result.stderr)
        need(result.returncode == 0, "native Mob draw assertions failed")
        footer = re.fullmatch(r"Animal draw: (\d+) checks; genuine RAW App/World/Animals/TH", result.stdout.splitlines()[-1])
        need(footer is not None, "actual native assertion footer")
        need(result.stderr in ("", "[ALSOFT] (WW) No capture backend available!\n"), "no undeclared native diagnostic")
        groups = parse_native(out / "native.ndjson")
        report["native_device"] = groups["device"][0]
        report["native_checks"] = int(footer[1])
        report["native_controls"] = groups["control"]
        report["pixels"] = pixel_checks(out)
        report["component_controls"]=component_controls(executable,resources,assets,component_directories)
        if source is not None:
            if device_source is not None:
                report["device_manifest_negative_controls"] = device_manifest_negatives(
                    args.device_observations.resolve(strict=True), audit)
                report["same_device"] = matching_device(groups["device"][0], device_source["device"])
                try:
                    compare(groups, source)
                except AssertionError as failure:
                    report["strict_default_Mesa_comparison_rejection"] = str(failure)
                    need("/gl/normal/" in str(failure), "same-device supplement cannot hide another default failure")
                else:
                    raise AssertionError("expected actual NVIDIA/Mesa raw normal distinction was not observed")
            report["original_comparison"] = compare(groups, source, device_source)
            report["negative_controls"] = negatives(groups, source, device_source)
        report["status"] = "PASS"
        print(footer[0])
        print(out / "run.json")
    finally:
        report["input_after_sha256"] = inventory(inputs)
        unchanged = before == report["input_after_sha256"]
        if audit is not None:
            _, final_audit = capture_audit(args.observations.resolve(strict=True))
            unchanged = unchanged and audit == final_audit
            if device_report is not None:
                _, _, final_device_audit = device_audit(args.device_observations.resolve(strict=True), source, audit)
                unchanged = unchanged and device_report == final_device_audit
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", newline="\n")
        need(unchanged, "native inputs/executable/assets or original source/helpers changed")


if __name__ == "__main__":
    main()
