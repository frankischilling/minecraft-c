"""Genuine prepared Sheep drawing and optional audited original observations.

Authored resources and Chunk bytes are execution inputs. Original observations
are read only after execution, and never become model, query or image inputs.
"""
import argparse
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
REFERENCE = ROOT / "tests/reference/client/mob_draw"


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

    for name, width, height, pixel in (("pack.png", 64, 32, skin),
                                      ("mob/sheep.png", 64, 32, skin),
                                      ("mob/sheep_fur.png", 64, 32, fur),
                                      ("misc/shadow.png", 128, 128, shadow)):
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
         and "MobDrawTarget" in data["command"], "actual declared Windows JDK17/helper invocation")
    sdk_suffixes = ("org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar",
                    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar",
                    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar",
                    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar")
    sdk = [local_path(next(name for name in primary["input_after_sha256"]
                          if name.replace("\\", "/").endswith(suffix))).resolve()
           for suffix in sdk_suffixes]
    archive = sdk[0].parents[2] / "lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    mandatory = [ROOT / ".local/run_mob_draw_windows_original_20261031.py", ROOT / "b1.7.3.jar",
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
             for case in range(3) for extension in ("png", "rgba")}
    names |= {"normal/" + name for name in ("class-init.log", "classes.txt", "process.stderr",
                                           "process.stdout", "target.ndjson")}
    names |= {"normal/resources/" + name for name in ("pack.png", "misc/shadow.png",
                                                     "mob/sheep.png", "mob/sheep_fur.png")}
    artifacts = {name.replace("\\", "/"): expected for name, expected in data["artifacts"].items()}
    need(len(data["artifacts"]) == len(artifacts) == 21 and set(artifacts) == names,
         "exact21-artifact inventory must include every consumed ordinary target file")
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
    need(data["original_linux_b_capture_sha256"] == original_audit["capture_sha256"] and
         data["original_linux_b_observations_sha256"] == original_audit["observations_sha256"],
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
    need([row["index"] for row in draws] == [0, 1, 2] and len(devices) == 1
         and devices == data["devices"] and textures == source["texture_records"],
         "actual complete supplementary calls, device and identical uploads/cache state")
    pair = Pair()
    for index, row in enumerate(draws):
        need(row["outcome"] == 1 and row["failure"] is None and row["gl_error"] == 0
             and row["before"]["entropy"] == row["after"]["entropy"],
             "actual full ordinary return and unchanged own-process entropy")
        for key in ("input", "shadow_only", "outcome", "failure", "nw_position", "nw_words", "gl_error"):
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
             and len(bobjects) == len(expected["objects"]) == len(aobjects) == 1160,
             "all 1160 selected actual graph objects, unique identities")
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
        need(len(visited) == 1160, "no omitted or unreachable selected object")

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
    groups = {name: [row for row in rows if row["event"] == name]
              for name in ("context", "device", "texture", "state", "draw", "list", "queries", "control")}
    need(sum(map(len, groups.values())) == len(rows), "no undeclared native transport")
    need(len(groups["device"]) == 1, "one actual native Display device observation")
    need(len(groups["context"]) == 1 and groups["context"][0] == {
        "event": "context", "RAW_app": True, "namespace_attached": True, "canonical_nonnull": True,
        "static_holds": 1, "default_profile_rebound": False}, "actual prepared RAW/static owner proof")
    need([row["index"] for row in groups["texture"]] == [0, 1, 2] and
         [row["index"] for row in groups["draw"]] == [0, 1, 2] and
         [row["case"] for row in groups["queries"]] == [0, 1, 2], "complete finite native calls")
    need([(row["case"], row["phase"]) for row in groups["state"]] ==
         [(i, p) for i in range(3) for p in ("before", "after")], "all six observed native states")
    need(len(groups["list"]) == 12 and len(groups["control"]) == 7, "twelve real lists and seven controls")
    need([row["name"] for row in groups["control"]] == ["retained_graph"] * 3 +
         ["allocation_zero", "null_actor", "foreign_actor", "shadow_limit"], "all retained/failure controls")
    for index, control in enumerate(groups["control"][:3]):
        need(control == {"event": "control", "name": "retained_graph", "case": index,
                         "nested_busy": True, "class_retire_busy": True,
                         "app_retire_busy": index < 2, "render_borrows_restored": True},
             "actual retained manager/App borrows and exact restoration")
    need(groups["control"][3] == {"event": "control", "name": "allocation_zero", "admission_only": True}
         and groups["control"][4] == {"event": "control", "name": "null_actor",
                                      "exception": "java/lang/NullPointerException", "pc": 1}
         and groups["control"][5] == {"event": "control", "name": "foreign_actor", "native_admission": True}
         and groups["control"][6] == {"event": "control", "name": "shadow_limit", "primary_returned": True,
                                      "native_policy": True, "unfinished_NW_preserved": True,
                                      "manager_status": "LIMIT", "living_status": "LIMIT", "quads": 0,
                                      "vertices": 0, "site": {"class": "bw", "method": "a",
                                      "descriptor": "(Luu;DDDIIIFFDDD)V", "pc": 241},
                                      "operation": "Shadow reached native quad ceiling"},
         "complete native admission/source-null/retained-prefix controls")
    for index, draw in enumerate(groups["draw"]):
        need(draw["outcome"] == 1 and draw["gl_error"] == 0 and draw["nw_position"] == 432 and
             len(draw["nw_words"]) == 432, "complete real final shadow batch and return")
        before, after = groups["state"][index * 2:index * 2 + 2]
        need(before["entropy"] == after["entropy"], "actual within-process draw entropy unchanged")
        need(before["pool"] == (0 if index == 0 else 288) and after["pool"] == 288,
             "first compilation uses 288 real vectors; later calls reuse actual lists")
        need(all(part["is_list"] and part["fields"]["ps.n"] for part in after["parts"]),
             "actual compiled parts")
        if index:
            need(before["parts"] == groups["state"][(index - 1) * 2 + 1]["parts"], "persistent part/list identities")
    for row in groups["list"]:
        need(len(row["batches"]) == 6 and all(len(b["words"]) == 48 and b["vertices"] == 6
             and b["mode"] == 4 for b in row["batches"]), "all 72 actual converted model faces")
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
    need(len(result) == (51 if case < 2 else 45), "finite actual direct boundary count")
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
    bridge = source["case_records"][0]["after_bridge"]
    reverse_bridge = {reference(value): int(key) for key, value in bridge.items()}
    for ordinal, (actual, original) in enumerate(zip(groups["list"], source["list_records"])):
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
    return {"typed_graph_state_checks": pair.checks, "expanded_objects": 6 * 1160,
            "query_attempts": query_count, "compiled_lists": 12, "model_faces": 72,
            "complete_shadow_words": 3 * 432, "matrix_maximum_absolute_error": pair.matrix_maximum,
            "matrix_allowance": "four float ULP plus numeric equality of opposite signed zeros, prepared view/projection only",
            "matrix_opposite_signed_zeros": pair.matrix_signed_zeros,
            "GL_authority": "Linux ordinary/JDI b" if device_source is None else "separate audited same-device ordinary source; no supplementary JDI credit",
            "identity_pairs": len(pair.forward), "list_name_pairs": len(pair.lists),
            "texture_name_pairs": len(pair.textures) - 1}


def negatives(groups, source, device_source=None):
    mutations = []
    for name in ("renderer_class", "model_alias", "part_replacement", "query_order", "missing_face", "texture_role"):
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
        else:
            changed["texture"][2]["cached_name"] = changed["texture"][0]["name"]
        try:
            compare(changed, source, device_source)
        except AssertionError:
            mutations.append({"name": name, "rejected": True})
        else:
            raise AssertionError("comparator admitted meaningful corruption: " + name)
    return mutations


def pixel_checks(out):
    images = {}
    for index in range(3):
        for phase in ("baseline", "draw"):
            path = out / f"{phase}{index}.rgba"
            need(path.stat().st_size == 256 * 256 * 4, "complete actual RGBA readback")
            images[phase, index] = path.read_bytes()
    need(images["draw", 0] == images["draw", 1], "actual compile/reuse raster equality")
    need(images["baseline", 0] == images["baseline", 1] == images["baseline", 2], "same authored background")
    counts = []
    for index in range(3):
        before, after = images["baseline", index], images["draw", index]
        changed = sum(before[i:i + 4] != after[i:i + 4] for i in range(0, len(after), 4))
        darkened = sum(all(after[i + c] <= before[i + c] for c in range(3)) and
                       any(after[i + c] < before[i + c] for c in range(3)) for i in range(0, len(after), 4))
        need(changed > (1000 if index < 2 else 128), "real visible Sheep/shadow pixels")
        need(darkened > 128, "real shadow darkens the authored ground")
        counts.append({"case": index, "changed": changed, "darkened": darkened,
                       "sha256": hashlib.sha256(after).hexdigest()})
    return {"scope": "native real readback; compile/reuse exact, visible model and ground shadow; no original pixel equality", "rows": counts}


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
    out = Path(tempfile.mkdtemp(prefix="mob-draw-", dir=args.work_dir.resolve()))
    assets = out / "assets"
    world = out / "world"
    authored_assets(assets)
    authored_world(world)
    executable = args.executable.resolve(strict=True)
    resources = ROOT / "tests/client/startup/fixtures"
    inputs = [executable, Path(__file__), ROOT / "tests/client/render/test_mob_draw.c",
              Path(__file__).with_name("test_startup.py"), ROOT / "tests/tools/entity/test_mob_tick.py",
              ROOT / "tests/tools/entity/test_mob_construction.py"]
    for directory in (ROOT / "src", ROOT / "include", REFERENCE, resources, assets):
        inputs.extend(p for p in directory.rglob("*") if p.is_file())
    inputs.extend(world.rglob("*.mcr"))
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
                                    "Block query receivers pair actual Registry m[1] identity/aliases and declaring method; native dynamic Block class is not exposed by the query API.",
                                    "Raw constructor/default entropy is retained independently; only within-process active-draw stability is asserted.",
                                    "Original raw Entity fieldsets remain source-only; native pairs actual Entity class/identity, prepared geometry effects and unchanged streams, without equating independent constructor-derived fields.",
                                    "Native pixels independently qualify visible model/shadow and reuse; no original pixel equality."],
              "scope": "genuine prepared RAW App/static input, real constructors and stored GAME World/Sheep/Cow/TH; finite first/reuse/shadow draw bodies; independent raw constructor entropy; no App startup/publication, other Mob, hurt/fire/debug true-branch or complete-frame claim"}
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
        footer = re.fullmatch(r"Mob draw: (\d+) checks; genuine RAW App/World/Sheep/TH", result.stdout.splitlines()[-1])
        need(footer is not None, "actual native assertion footer")
        need(result.stderr in ("", "[ALSOFT] (WW) No capture backend available!\n"), "no undeclared native diagnostic")
        groups = parse_native(out / "native.ndjson")
        report["native_device"] = groups["device"][0]
        report["native_checks"] = int(footer[1])
        report["native_controls"] = groups["control"]
        report["pixels"] = pixel_checks(out)
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
