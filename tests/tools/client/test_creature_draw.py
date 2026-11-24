"""Genuine prepared Spider/Slime/Squid/Creeper drawing and optional audited original observations.

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
REFERENCE = ROOT / "tests/reference/client/mob_draw_creatures"


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
    png = module("creature_draw_png", Path(__file__).with_name("test_startup.py")).authored_png
    def java_hash(name):
        value=0
        for ch in name:value=(value*31+ord(ch))&0xffffffff
        return value
    def pixels(name):
        if name=="misc/shadow.png":
            def shadow(x,y):
                d=math.sqrt(((x+.5)/64-1)**2+((y+.5)/64-1)**2)
                return 0,0,0,int(max(0,1-d)*180)
            return shadow
        if "eyes" in name:return lambda x,y:(0xf0,0xb0,0x20,0xff if ((x//4+y//4)&1)==0 else 0x40)
        blue=(java_hash(name)>>3)&255
        return lambda x,y:(0x60 if x<32 else 0xd0,0xb0 if y<16 else 0x40,blue,255)
    for name in ("pack.png","mob/spider.png","mob/spider_eyes.png","mob/slime.png","mob/squid.png","mob/creeper.png","misc/shadow.png"):
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True)
        width=height=128 if name=="misc/shadow.png" else 64
        if height==64:height=32
        target.write_bytes(png(width,height,pixels(name)))


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
         and "CreatureDrawTarget" in data["command"], "actual declared Windows JDK17/helper invocation")
    sdk_suffixes = ("org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar",
                    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar",
                    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar",
                    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar")
    sdk = [local_path(next(name for name in primary["input_after_sha256"]
                          if name.replace("\\", "/").endswith(suffix))).resolve()
           for suffix in sdk_suffixes]
    archive = sdk[0].parents[2] / "lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    mandatory = [ROOT / ".local/run_creature_draw_windows_original_20261031.py", ROOT / "b1.7.3.jar",
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
             for case in range(13) for extension in ("png", "rgba")}
    names |= {"normal/" + name for name in ("class-init.log", "classes.txt", "process.stderr",
                                           "process.stdout", "target.ndjson")}
    names |= {"normal/resources/" + name for name in ("pack.png", "misc/shadow.png", "mob/spider.png", "mob/spider_eyes.png", "mob/slime.png", "mob/squid.png", "mob/creeper.png")}
    artifacts = {name.replace("\\", "/"): expected for name, expected in data["artifacts"].items()}
    need(len(data["artifacts"]) == len(artifacts) == 64 and set(artifacts) == names,
         "exact64-artifact inventory must include every consumed ordinary target file")
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

    Its model/geometry/texture observations must agree with the primary independently.
    It never replaces the primary read-only debugger, list or query evidence.
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
         "supplement executed all four byte-identical frozen primary helpers")
    paths = [receipt, *device_manifest(data, receipt, primary, original_audit)]
    for name, expected in data["helper_after_sha256"].items():
        path = receipt.parent / "classes" / name
        need(path.is_file() and digest(path) == expected, "current supplementary compiled helper")
        paths.append(path)
    rows = [json.loads(line) for line in (receipt.parent / "normal/target.ndjson").read_text().splitlines()]
    draws = [row for row in rows if row["event"] == "draw"]
    devices = [row for row in rows if row["event"] == "device"]
    textures = [row for row in rows if row["event"] == "texture_lookup"]
    need([row["index"] for row in draws] == list(range(13)) and len(devices) == 1
         and devices == data["devices"] and textures == source["texture_records"],
         "actual complete supplementary calls, device and identical uploads/cache state")
    pair = Pair()
    for index, row in enumerate(draws):
        need(row["outcome"] == 1 and row["failure"] is None and row["gl_error"] == 0
             and row["before"]["entropy"] == row["after"]["entropy"],
             "actual full ordinary return and unchanged own-process entropy")
        for key in ("input", "kind", "declared_getters", "outcome", "failure", "nw_position", "nw_words", "gl_error"):
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
    rows=[json.loads(line) for line in path.read_text(encoding="ascii").splitlines()]
    groups={name:[row for row in rows if row["event"]==name] for name in
            ("context","device","texture","state","draw","list","queries","control","texture_calls","hooks","model_work")}
    need(sum(map(len,groups.values()))==len(rows),"complete declared native transport")
    need(len(groups["device"])==len(groups["context"])==1,"actual native device/raw App")
    need(groups["context"][0]=={"event":"context","RAW_app":True,"namespace_attached":True,"canonical_nonnull":True,"static_holds":1,"default_profile_rebound":False},"actual raw/static ownership")
    need([r["index"] for r in groups["texture"]]==list(range(6)),"all6 genuine uploads/hits")
    for name in ("draw","queries","texture_calls","hooks"):
        need([r["index" if name=="draw" else "case"] for r in groups[name]]==list(range(13)),"all13 calls: "+name)
    need([(r["case"],r["phase"]) for r in groups["state"]]==[(i,p) for i in range(13) for p in ("before","after")],"all26 snapshots")
    need([(r["case"],r["phase"]) for r in groups["model_work"]]==[(i,p) for i in range(13) for p in (("primary","pass0") if i in (0,1,2,3,6,7,8,9,10) else ("primary",))],"all22 actual selected renders")
    need(len(groups["list"])==42,"all42 compiled PS lists")
    need([r["name"] for r in groups["control"]]==["retained_graph"]*13+["allocation_zero","null_actor","foreign_actor","shadow_limit"],"all retained/failure controls")
    for i,row in enumerate(groups["control"][:13]):
        need(row=={"event":"control","name":"retained_graph","case":i,"nested_busy":True,"class_retire_busy":True,"app_retire_busy":True,"render_borrows_restored":True},"actual ownership refusal/retirement")
    need(groups["control"][13]=={"event":"control","name":"allocation_zero","admission_only":True},"zero allocation is native policy")
    need(groups["control"][14]=={"event":"control","name":"null_actor","exception":"java/lang/NullPointerException","pc":1},"source null actor")
    need(groups["control"][15]=={"event":"control","name":"foreign_actor","native_admission":True},"foreign owner admission")
    need(groups["control"][16]=={"event":"control","name":"shadow_limit","primary_returned":True,"native_policy":True,"unfinished_NW_preserved":True,"manager_status":"LIMIT","living_status":"LIMIT","quads":0,"vertices":0,"site":{"class":"bw","method":"a","descriptor":"(Luu;DDDIIIFFDDD)V","pc":241},"operation":"Shadow reached native quad ceiling"},"exact real shadow ceiling")
    pools=[528,528,648,648,864,864,864,864,864,864,864,1008,1008]
    for i,row in enumerate(groups["draw"]):
        words=48 if i in (2,3,6,7,8,9) else 192 if i in (11,12) else 432
        need(row["outcome"]==1 and row["gl_error"]==0 and row["nw_position"]==len(row["nw_words"])==words,"actual complete final NW batch")
        before,after=groups["state"][i*2:i*2+2]
        need(before["entropy"]==after["entropy"] and after["pool"]==pools[i],"unchanged streams/real pool")
        need(sum(p["is_list"] for p in after["parts"])== (22 if i in (0,1,10) else 5 if i in (2,3,6,7,8,9) else 9 if i in (4,5) else 6),"actual reached parts; IL unused parts stay uncompiled")
    for row in groups["list"]:
        need(len(row["batches"])==6 and all(len(b["words"])==48 and b["vertices"]==6 and b["mode"]==4 for b in row["batches"]),"all252 model face buffers")
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
    return result


def compare(groups, source, device_source=None):
    need([r["index"] for r in source["draw_records"]]==list(range(13)) and
         [r["case"] for r in source["case_records"]]==list(range(13)) and
         [r["case"] for r in source["hook_records"]]==list(range(13)),"complete ordered original13-case record groups")
    need([r["index"] for r in source["texture_records"]]==list(range(6)) and
         len(source["list_records"])==42 and len(source["model_work_records"])==22,"complete original uploads/lists/model records")
    need(len(source["no_constructor_records"]["allocations"])==6 and
         len(source["no_constructor_records"]["displaced_unrendered"])==1,"retained separately labelled original NO constructor evidence")
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
        compare_hooks(pair, groups["hooks"][index], source, index)
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
            "query_attempts": query_count, "compiled_lists": 42, "model_faces": 252,
            "complete_shadow_words": sum(r["nw_position"] for r in groups["draw"]), "matrix_maximum_absolute_error": pair.matrix_maximum,
            "matrix_allowance": "four float ULP plus numeric equality of opposite signed zeros, prepared view/projection only",
            "matrix_opposite_signed_zeros": pair.matrix_signed_zeros,
            "GL_authority": "complete Linux ordinary/JDI primary" if device_source is None else "separate audited same-device ordinary source; no supplementary JDI credit",
            "identity_pairs": len(pair.forward), "list_name_pairs": len(pair.lists),
            "texture_name_pairs": len(pair.textures) - 1, "direct_texture_calls": texture_count,
            "actual_animation_returns": 13, "selected_model_work_records": model_count}


def source_call_pairs(source, case):
    result=[];stack=[]
    for row in source["source_events"]:
        if row.get("case")!=case:continue
        if row["event"]=="entry":stack.append(row)
        elif row["event"]=="return":
            need(stack,"actual original entry/return stack")
            entry=stack.pop()
            need(all(entry["location"][k]==row["location"][k] for k in ("class","method","descriptor")),"actual source method return")
            result.append((entry,row))
    need(not stack,"complete selected original call stack")
    return result


def compare_hooks(pair, actual, source, case):
    record=source["hook_records"][case]
    need(record["case"]==case,"complete ordered hook evidence")
    draw=source["draw_records"][case];kind=draw["kind"]
    calls=source_call_pairs(source,case)
    ages=[returned for entry,returned in calls if entry["location"]["class"]==("es" if kind=="xt" else "gv") and entry["location"]["method"]==("b" if kind=="xt" else "c") and entry["location"]["descriptor"]==("(Lxt;F)F" if kind=="xt" else "(Lls;F)F")]
    need(len(ages)==1,"actual in-draw age return")
    pair.value(actual["age"],ages[0]["result"],f"case{case}/age")
    if kind=="cn":
        colors=record["eyes_colors"]
        need(len(colors)==1,"one actual eyes color call")
        pair.value(actual["eyes_alpha"],colors[0]["arguments"][3],f"case{case}/eyes_alpha")
        brightness=[returned for entry,returned in calls if entry["location"]["class"]=="sn" and entry["location"]["method"]=="a" and entry["location"]["descriptor"]=="(F)F" and any(frame["class"]=="yx" for frame in entry["source_frames"])]
        need(len(brightness)==1,"actual in-pass brightness return")
        pair.value(actual["eyes_brightness"],brightness[0]["result"],f"case{case}/eyes_brightness")
        need([r["result"] for r in record["pass_returns"]]==[True,False,False,False],"eyes alpha zero still renders")
    elif kind=="uw":
        need(record["size_returns"] and all(r["result"]==int(draw["input"][4]) for r in record["size_returns"]),"actual Slime size calls")
        pair.value(actual["slime_size"],record["size_returns"][0]["result"],f"case{case}/size")
        pair.value(actual["secondary_entry"],draw["before"]["graph"]["roots"]["fur"],f"case{case}/secondary_entry")
        pair.value(actual["secondary_stored"],draw["after"]["graph"]["roots"]["own_model"],f"case{case}/secondary_stored")
        scales=[row for row in source["source_events"] if row.get("case")==case and row["event"]=="gl-entry" and row["method"]=="glScalef" and any(frame["class"]=="mj" and frame["descriptor"]=="(Luw;F)V" for frame in row["source_frames"])]
        need(len(scales)==1,"actual Slime pre-scale")
        pair.value(actual["prescale"],scales[0]["arguments"],f"case{case}/slime_scale")
        # Independent float-by-float replay of declared caller operands; never runtime input.
        f32=lambda x:struct.unpack(">f",struct.pack(">f",x))[0]
        bits=lambda x:{"F":struct.pack(">f",x).hex()}
        partial=float(draw["input"][2]);size=int(draw["input"][4])
        squash=f32(f32(-.25+f32(f32(.5-(-.25))*partial))/f32(f32(size*.5)+1))
        reciprocal=f32(1/f32(squash+1))
        pair.value(actual["slime_squash"],bits(squash),f"case{case}/declared_squash")
        pair.value(actual["slime_reciprocal"],bits(reciprocal),f"case{case}/declared_reciprocal")
    elif kind=="xt":
        rotations=record["rotation_gl"]
        need([r["method"] for r in rotations]==["glTranslatef","glRotatef","glRotatef","glRotatef","glTranslatef"],"actual Squid selected transforms")
        pair.value(actual["squid_pitch"],rotations[2]["arguments"][0],f"case{case}/squid_pitch")
        pair.value(actual["squid_yaw"],rotations[3]["arguments"][0],f"case{case}/squid_yaw")
    else:
        pair.value(actual["powered"],[r["result"] for r in record["powered_returns"]],f"case{case}/powered")
        need(len(record["sine_entries"])==1 and record["sine_entries"][0]["arguments"]==[{"F":"00000000"}],"actual zero-input sine call")
        sine=[returned for entry,returned in calls if entry==record["sine_entries"][0]]
        need(len(sine)==1,"actual Creeper sine return")
        pair.value(actual["creeper_sine"],sine[0]["result"],f"case{case}/creeper_sine")
        scales=[row for row in source["source_events"] if row.get("case")==case and row["event"]=="gl-entry" and row["method"]=="glScalef" and any(frame["class"]=="m" and frame["descriptor"]=="(Lgb;F)V" for frame in row["source_frames"])]
        need(len(scales)==1,"actual Creeper pre-scale")
        pair.value(actual["prescale"],scales[0]["arguments"],f"case{case}/creeper_scale")


def compare_model_work(pair, groups, source):
    need(len(groups["model_work"])==len(source["model_work_records"])==22,"all22 selected actual model invocations")
    for actual,expected in zip(groups["model_work"],source["model_work_records"]):
        case=actual["case"];need(expected["case"]==case,"actual model case order")
        draw=source["draw_records"][case]
        model=draw["after"]["graph"]["roots"]["primary" if actual["phase"]=="primary" else "fur"]
        label=f"case{case}/{actual['phase']}/model_work"
        pair.value(actual["model"],model,label+"/model")
        root=source["case_records"][case]["after_bridge"][str(model["identity"])]
        need(expected["render_entry"]["receiver"]==root and len(expected["pose_entries"])==1 and expected["pose_entries"][0]["receiver"]==root,"actual entered model/pose identity")
        cls=model["class"]
        stores={"sc":34,"no":0,"wn":8,"il":6}[cls]
        pair.value(actual["pose_stores"],stores,label+"/Code_stores")
        for key,source_key in (("cosine_calls","cosine_entries"),("sine_calls","sine_entries"),("absolute_calls","absolute_entries")):
            pair.value(actual[key],len(expected[source_key]),label+"/"+key)
        pair.value(actual["math_calls"],sum(len(expected[k]) for k in ("cosine_entries","sine_entries","absolute_entries")),label+"/math_calls")
        for key in ("part_calls","part_returns"):pair.value(actual[key],len(expected["part_entries"]),label+"/"+key)
        pair.value(actual["render_site"],expected["render_return"]["location"],label+"/render_site")
        poses=[r for e,r in source_call_pairs(source,case) if e==expected["pose_entries"][0]]
        need(len(poses)==1,"actual selected pose return")
        pair.value(actual["pose_site"],poses[0]["location"],label+"/pose_site")
        if cls=="wn":
            fields=next(o["fields"] for o in draw["after"]["graph"]["objects"] if o["identity"]==model["identity"])
            pair.value(actual["array"],fields["wn.b"],label+"/actual_array")
            for key,value in (("array_index",8),("array_length",8),("array_role",3),("array_length_reads",18),("array_element_reads",16)):
                pair.value(actual[key],value,label+"/Code_"+key)
        else:
            pair.value(actual["array"],None,label+"/no_array")
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
    need(total == 16, "actual16 direct living/post texture routes")
    return total


def negatives(groups, source, device_source=None):
    result=[]
    names=("renderer_class","model_alias","part_replacement","query_order","missing_face","texture_role","missing_eyes_lookup","eyes_identity_split","slime_secondary_alias","array_temporal_replacement","zero_alpha_skip","creeper_sine_value")
    for name in names:
        changed=copy.deepcopy(groups);graph=changed["state"][0]["graph"]
        if name=="renderer_class":graph["roots"]["renderer"]["class"]="xy"
        elif name=="model_alias":graph["roots"]["fur"]=copy.deepcopy(graph["roots"]["primary"])
        elif name=="part_replacement":changed["state"][1]["parts"][0]["part"]["identity"]="fresh-wrong-part"
        elif name=="query_order":
            rows=changed["queries"][0]["queries"];rows[0],rows[1]=rows[1],rows[0]
        elif name=="missing_face":changed["list"][0]["batches"].pop()
        elif name=="texture_role":changed["texture"][5]["cached_name"]=changed["texture"][0]["name"]
        elif name=="missing_eyes_lookup":changed["texture_calls"][0]["calls"].pop(0)
        elif name=="eyes_identity_split":changed["texture_calls"][1]["calls"][0]["key"]["identity"]="fresh-wrong-eyes"
        elif name=="slime_secondary_alias":
            g=changed["state"][5]["graph"];g["roots"]["fur"]=copy.deepcopy(g["roots"]["primary"])
        elif name=="array_temporal_replacement":
            g=changed["state"][9]["graph"]
            node=next(n for n in g["objects"] if n["class"]=="[Lps;")
            old=node["identity"]
            def replace(value):
                if isinstance(value,dict):
                    if reference(value)==("[Lps;",old):value["identity"]="fresh-same-content-array"
                    for child in value.values():replace(child)
                elif isinstance(value,list):
                    for child in value:replace(child)
            replace(g)
        elif name=="zero_alpha_skip":
            work=next(r for r in changed["model_work"] if r["case"]==0 and r["phase"]=="pass0");work["part_calls"]=work["part_returns"]=0
        else:changed["hooks"][11]["creeper_sine"]={"F":"3f800000"}
        try:compare(changed,source,device_source)
        except AssertionError:result.append({"name":name,"rejected":True})
        else:raise AssertionError("comparator admitted meaningful corruption: "+name)
    return result


def pixel_checks(out):
    rows=[]
    for case in range(13):
        files=[out/f"{phase}{case}.rgba" for phase in ("baseline","draw")]
        need(all(p.stat().st_size==262144 for p in files),"complete RGBA readbacks")
        before,after=(p.read_bytes() for p in files)
        changed=sum(before[i:i+4]!=after[i:i+4] for i in range(0,len(after),4))
        darkened=sum(all(after[i+c]<=before[i+c] for c in range(3)) and any(after[i+c]<before[i+c] for c in range(3)) for i in range(0,len(after),4))
        need(changed>64 and darkened>1,"actual visible creature and shadow")
        rows.append({"case":case,"changed":changed,"darkened":darkened,"sha256":hashlib.sha256(after).hexdigest()})
    return {"scope":"native authored readback visibility only; no original pixel equality","rows":rows}


def component_controls(executable, resources, assets, directories):
    expected={
        "array-null":(9,0,0,0,True,False,"ps[]"),
        "element-null":(21,3,4,4,True,False,"ps[]"),
        "head-null":(19,8,9,8,True,False,"ps[]"),
        "length-mismatch":(9,0,1,0,False,True,"ps[]"),
        "opaque-payload":(9,0,0,0,False,True,"ps"),
    }
    results=[]
    for mode,directory in directories.items():
        environment=os.environ.copy();environment["ALSOFT_DRIVERS"]="null"
        for name in ("APPDATA","HOME","XDG_DATA_HOME","XDG_CONFIG_HOME","XDG_CACHE_HOME"):
            path=directory/name.lower();path.mkdir();environment[name]=str(path)
        command=[str(executable),str(resources),str(assets),str(directory/"world"),str(directory),mode]
        result=subprocess.run(command,cwd=directory,env=environment,capture_output=True,text=True,timeout=180)
        (directory/"native.stdout.txt").write_text(result.stdout,newline="\n")
        (directory/"native.stderr.txt").write_text(result.stderr,newline="\n")
        need(result.returncode==0 and result.stderr in ("","[ALSOFT] (WW) No capture backend available!\n"),"fresh WN control: "+mode)
        footer=re.fullmatch(r"Creature draw: (\d+) checks; genuine RAW App/World/Creatures/TH",result.stdout.splitlines()[-1])
        need(footer is not None,"actual component assertion footer")
        rows=[json.loads(line) for line in (directory/"native.ndjson").read_text().splitlines()]
        groups={name:[r for r in rows if r["event"]==name] for name in ("context","device","texture","control")}
        need(sum(map(len,groups.values()))==len(rows) and len(groups["context"])==len(groups["device"])==len(groups["control"])==1 and len(groups["texture"])==6,"fresh actual owner fixture; no hidden baseline draws/records")
        need(groups["context"][0]=={"event":"context","RAW_app":True,"namespace_attached":True,"canonical_nonnull":True,"static_holds":1,"default_profile_rebound":False},"genuine fresh raw/static context")
        pc,stores,length,element,source_exception,native_policy,payload=expected[mode]
        need(groups["control"]==[{"event":"control","name":"wn_array","mode":mode,"allocated_capacity":8,"pc":pc,"pose_stores":stores,"length_reads":length,"element_reads":element,"source_exception":source_exception,"native_policy":native_policy,"actual_payload_class":payload,"prefix_preserved":True,"source_fields_restored":False,"checked_retirement":True}],"exact model/array source prefix and separate native admission")
        results.append({"mode":mode,"checks":int(footer[1]),"control":groups["control"][0],"command":command,
            "scope":"fresh genuine WN model; Code-qualified native control, not an unchanged-original dynamic Throwable pair"})
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
    out = Path(tempfile.mkdtemp(prefix="creature-draw-", dir=args.work_dir.resolve()))
    assets = out / "assets"
    world = out / "world"
    authored_assets(assets)
    authored_world(world)
    component_directories={}
    for mode in ("array-null","element-null","head-null","length-mismatch","opaque-payload"):
        directory=out/mode;directory.mkdir();authored_world(directory/"world");component_directories[mode]=directory
    executable = args.executable.resolve(strict=True)
    resources = ROOT / "tests/client/startup/fixtures"
    inputs = [executable, Path(__file__), ROOT / "tests/client/render/test_creature_draw.c",
              Path(__file__).with_name("test_startup.py"), ROOT / "tests/tools/entity/test_mob_tick.py",
              ROOT / "tests/tools/entity/test_mob_construction.py"]
    inputs.extend(ROOT / "tests/client/startup" / name for name in
                  ("test_application_startup.c", "controller_boundary_observer.h",
                   "application_startup_statistics_controls.inc", "application_startup_effects_controls.inc",
                   "application_startup_rendering_controls.inc"))
    for directory in (ROOT / "src", ROOT / "include", resources, assets):
        inputs.extend(p for p in directory.rglob("*") if p.is_file())
    inputs.extend(world.rglob("*.mcr"))
    for directory in component_directories.values():inputs.extend((directory/"world").rglob("*.mcr"))
    source = None
    audit = None
    device_source = None
    device_report = None
    if args.observations:
        inputs.extend(p for p in REFERENCE.rglob("*") if p.is_file())
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
                                    "Native pixels independently qualify visible model/shadow; no original pixel equality.",
                                    "Native per-part invocation order on reuse is not exposed; committed list geometry and final NW/GL are paired, with complete original order retained separately.",
                                    "The actual replaced NO constructor cube is Source-only JDI allocation evidence; no native per-allocation constructor trace is claimed.",
                                    "Fresh WN NULL controls are Code-qualified; immutable descriptor/capacity or opaque-payload corruption is native STATE, not a Java bounds outcome. No unforced exceptional cleanup refusal is injected."],
              "scope": "genuine prepared RAW App/static input, real constructors and stored GAME World/Spider/Slime/Squid/Creeper/TH; finite first/reuse/mixed NORMALIZE/shadow draw bodies; independent raw constructor entropy; no App startup/publication, other Mob, hurt/fire/debug true-branch or complete-frame claim"}
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
        footer = re.fullmatch(r"Creature draw: (\d+) checks; genuine RAW App/World/Creatures/TH", result.stdout.splitlines()[-1])
        need(footer is not None, "actual native assertion footer")
        need(result.stderr in ("", "[ALSOFT] (WW) No capture backend available!\n"), "no undeclared native diagnostic")
        groups = parse_native(out / "native.ndjson")
        report["native_device"] = groups["device"][0]
        report["native_checks"] = int(footer[1])
        report["native_controls"] = groups["control"]
        report["pixels"] = pixel_checks(out)
        report["component_controls"]=component_controls(executable,resources,assets,component_directories)
        report["native_checks_total"]=report["native_checks"]+sum(r["checks"] for r in report["component_controls"])
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
