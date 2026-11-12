"""Bounded genuine LocalPlayer NBT observations; no native result is inferred."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import zipfile


HERE = Path(__file__).resolve().parent
CLIENT = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
NATIVE = "86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7"
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}
CASES = (
    "write_fresh", "write_alias_narrow", "roundtrip", "base_only",
    "health_wrong", "hurt_wrong", "death_wrong", "attack_wrong",
    "inventory_wrong", "entry_wrong", "entry_null", "slot_wrong",
    "id_wrong", "count_wrong", "damage_wrong", "id_negative", "id_null",
    "outside_slot", "duplicate_slot", "armor_slots", "slot_255", "slot_missing",
    "partial_list", "dimension_wrong", "sleeping_wrong", "timer_wrong",
    "spawn_partial", "spawn_x_wrong", "spawn_y_wrong", "spawn_z_wrong",
    "spawn_good", "score_wrong", "score_absent",
)
HELPERS = {"PlayerNbtObservation.class", "PlayerNbtPrefix.class", "PlayerNbtPrefix$Drain.class"}


def need(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def require_process(result, *, empty_stderr=True):
    need(result.returncode == 0, f"exit {result.returncode}")
    need(not empty_stderr or result.stderr == b"", "nonempty process stderr")
    report = result.capture_report
    need(report.get("reader_errors") == {} and report["pipe_capture_complete"] is True and
         report["reaped"] is True and report["timed_out"] is False and
         report["output_overflow"] is False, "bounded capture receipt failed")


def validate_trees(lines):
    trees = {}
    for line in lines:
        if not line.startswith("TAG "):
            continue
        fields = line.split()
        need(len(fields) == 6, "partial NBT observation row")
        _, case, stage, path, kind, payload = fields
        key = (case, stage)
        nodes = trees.setdefault(key, {})
        need(path not in nodes, "duplicate observed tree path")
        nodes[path] = (int(kind), payload)
    need(bool(trees), "no observed NBT trees")
    for key, nodes in trees.items():
        need("$" in nodes and nodes["$"][0] == 10, f"missing Compound root {key}")
        for path, (kind, payload) in nodes.items():
            need(path == "$" or path.startswith("$/"), "orphan observed tree path")
            if path != "$":
                parent = path.rsplit("/", 1)[0]
                need(parent in nodes and nodes[parent][0] in (9, 10), "missing/noncontainer parent")
            need(1 <= kind <= 10 and kind != 7, "unsupported observed scalar kind")
            if kind in (1, 2, 3):
                number = int(payload)
                width = {1: 8, 2: 16, 3: 32}[kind]
                need(-(1 << (width - 1)) <= number < (1 << (width - 1)), "scalar width")
            elif kind in (4, 5, 6):
                need(re.fullmatch("[0-9a-f]{" + str(8 if kind == 5 else 16) + "}", payload), "raw scalar bits")
            elif kind == 8:
                need(payload == "-" or re.fullmatch(r"(?:[0-9a-f]{4})+", payload), "UTF16 string inventory")
            children = {p[len(path) + 1:]: v for p, v in nodes.items()
                        if p.startswith(path + "/") and "/" not in p[len(path) + 1:]}
            if kind == 10:
                need(len(children) == int(payload), f"partial Compound inventory {key}/{path}")
            elif kind == 9:
                element, count = map(int, payload.split(","))
                need(set(children) == {str(i) for i in range(count)}, "partial/reordered List inventory")
                need(all(v[0] == element for v in children.values()), "writer List element kind")
            else:
                need(not children, "scalar observation has children")
    return trees


PLAYER_FIELDS = {"Pos": 9, "Motion": 9, "Rotation": 9, "FallDistance": 5,
                 "Fire": 2, "Air": 2, "OnGround": 1, "Health": 2, "HurtTime": 2,
                 "DeathTime": 2, "AttackTime": 2, "Inventory": 9, "Dimension": 3,
                 "Sleeping": 1, "SleepTimer": 2, "Score": 3}
INFO_FIELDS = {"RandomSeed": 4, "SpawnX": 3, "SpawnY": 3, "SpawnZ": 3, "Time": 4,
               "SizeOnDisk": 4, "LastPlayed": 4, "LevelName": 8, "version": 3,
               "rainTime": 3, "raining": 1, "thunderTime": 3, "thundering": 1}


def schema(nodes, *, info=False):
    expected = INFO_FIELDS if info else PLAYER_FIELDS
    top = {p[2:]: v[0] for p, v in nodes.items() if p.startswith("$/") and "/" not in p[2:]}
    optional = {"Player": 10} if info else {"SpawnX": 3, "SpawnY": 3, "SpawnZ": 3}
    need(top == expected or top == {**expected, **optional}, "complete source field inventory differs")
    if info:
        if "Player" in top:
            selected = {"$" + p[len("$/Player"):]: v for p, v in nodes.items()
                        if p == "$/Player" or p.startswith("$/Player/")}
            schema(selected)
        return
    for name, element, size in (("Pos", 6, 3), ("Motion", 6, 3), ("Rotation", 5, 2)):
        need(nodes["$/" + name] == (9, f"{element},{size}"), "source vector shape differs")
    element, count = map(int, nodes["$/Inventory"][1].split(","))
    need(0 <= count <= 40 and element == (10 if count else 0), "real Inventory writer shape")
    for i in range(count):
        prefix = f"$/Inventory/{i}/"
        fields = {p[len(prefix):]: v[0] for p, v in nodes.items()
                  if p.startswith(prefix) and "/" not in p[len(prefix):]}
        need(fields == {"Slot": 1, "id": 2, "Count": 1, "Damage": 2}, "exact Stack writer fields")
        slot = int(nodes[prefix + "Slot"][1])
        need(0 <= slot < 36 or 100 <= slot < 104, "source emitted Slot range")


def validate_prefix(text, target, pins, jar):
    lines = text.splitlines()
    need(bool(lines) and lines[-1] == "Original Player NBT prefix: 1 checkpoint" and
         lines.count("SITE sleep gs a (Lnu;)V 93") == 1, "actual wake checkpoint incomplete")
    states = [line.split() for line in lines if line.startswith("STATE ")]
    need(len(states) == 1 and len(states[0]) == 35 and
         states[0][:3] == ["STATE", "sleep", "PENDING_WAKE"] and states[0][8] == "1" and
         states[0][11] != "-", "missing actual sleeping/coordinate prefix state")
    need([line for line in lines if line.startswith("ATTACH_PREFIX ")] ==
         ["ATTACH_PREFIX sleep 1 0 0"], "Info.Player/publication prefix inventory")
    randoms = [line for line in lines if line.startswith("RANDOM ")]
    need(len(randoms) == 1 and randoms[0].startswith("RANDOM sleep "), "prefix Random inventory")
    need(len(randoms[0].split()) == 4 and all(re.fullmatch(r"[0-9a-f]{12},[01],[0-9a-f]{16}", v)
         for v in randoms[0].split()[2:]), "prefix Random state width")
    trees = validate_trees(lines)
    need(set(trees) == {("sleep", "prefix")}, "prefix tree inventory differs")
    schema(trees[("sleep", "prefix")])
    after = target.splitlines()
    need(after and after[-1] == "Original Player NBT sleep: 1 completed call" and
         len([line for line in after if line.startswith("SLEEP_AFTER OK ")]) == 1,
         "actual post-checkpoint wake/read/clear/publication incomplete")
    origins = [line.split(" ", 2) for line in after if line.startswith("ORIGIN ")]
    need([v[1].replace(".", "/") for v in origins] == list(pins["classes"]) and
         all(Path(v[2]).resolve().samefile(jar) for v in origins), "prefix runtime origin inventory")
    return len(trees[("sleep", "prefix")])


def validate_normal(text, pins, jar):
    lines = text.splitlines()
    need(bool(lines), "empty original output")
    marker = re.fullmatch(r"Original Player NBT: 33 cases, ([0-9]+) complete reads, ([0-9]+) interrupted reads",
                          lines[-1])
    need(marker is not None, "missing actual original completion marker")
    endings = [line.split() for line in lines if line.startswith("END ")]
    need([v[1] for v in endings] == list(CASES) and all(len(v) == 3 for v in endings),
         "original case inventory/order differs")
    states = [line.split() for line in lines if line.startswith("STATE ")]
    need([v[1] for v in states] == list(CASES) + ["attach_base_only"] and
         all(len(v) == 35 for v in states), "original state inventory/width differs")
    need([v[2] for v in states[:33]] == [v[2] for v in endings], "state/end outcomes differ")
    reads = endings[2:]
    complete = sum(v[2] == "OK" for v in reads)
    interrupted = sum(v[2] in ("CAST", "NULL", "INDEX") for v in reads)
    need(complete > 0 and complete + interrupted == len(reads) and
         (complete, interrupted) == (int(marker[1]), int(marker[2])), "read completion inventory")
    throws = [line.split() for line in lines if line.startswith("THROW ")]
    need(len(throws) == interrupted and all(len(v) == 4 for v in throws), "Throwable inventory")
    origins = [line.split(" ", 2) for line in lines if line.startswith("ORIGIN ")]
    need([v[1].replace(".", "/") for v in origins] == list(pins["classes"]), "origin inventory/order")
    need(all(Path(v[2]).resolve().samefile(jar) for v in origins), "foreign runtime game origin")
    need([line for line in lines if line.startswith("NAMED ")] ==
         ["NAMED write_fresh 0 1", "NAMED write_alias_narrow 0 1"], "unnamed Boolean-save inventory")
    randoms = [line.split() for line in lines if line.startswith("RANDOM ")]
    need([v[1] for v in randoms] == list(CASES) and all(len(v) == 4 and
         all(re.fullmatch(r"[0-9a-f]{12},[01],[0-9a-f]{16}", state) for state in v[2:])
         for v in randoms), "owner Random inventory/width differs")
    need("ATTACH base_only 0 1 1" in lines and "SELECTION 3 1" in lines,
         "actual load/selection completion missing")
    saves = [line.split() for line in lines if line.startswith("SAVE ")]
    need(len(saves) == 1 and saves[0][:3] == ["SAVE", "fresh", "1"] and saves[0][-1] == "1",
         "genuine fresh construction/save/clock receipt missing")
    trees = validate_trees(lines)
    for name in CASES:
        need((name, "after") in trees, "missing actual post-call writer tree")
        schema(trees[(name, "after")])
    for key, nodes in trees.items():
        schema(nodes, info=key[1] == "data")
    need(("roundtrip", "input") in trees and
         all((name, "direct") in trees for name in CASES[:2]), "direct writer/roundtrip input tree missing")
    need([line for line in lines if line.startswith("CLOCK ")] ==
         ["CLOCK select_empty 1", "CLOCK select_null 1", "CLOCK select_first_only 1"],
         "actual Info writer clock bracket inventory")
    need({("full_save", "player"), ("attach_base_only", "after"),
          ("select_empty", "data"), ("select_null", "data"), ("select_first_only", "data")} <= set(trees),
         "composition tree inventory missing")
    return {"cases": len(CASES), "complete_reads": complete, "interrupted_reads": interrupted,
            "trees": len(trees), "tag_rows": sum(line.startswith("TAG ") for line in lines)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--natives", type=Path, required=True)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[3])
    parser.add_argument("--minimum-free-bytes", type=int, default=6 * 1024**3)
    args = parser.parse_args(argv)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="original-player-nbt-", dir=args.work_dir.resolve()))
    source = args.source_root.resolve()
    capture_path = source / "tests/reference/client/statistics/support.py"
    metadata_path = source / "tests/reference/client/notice/classfile.py"
    capture = module(capture_path, "player_nbt_original_capture")
    metadata = module(metadata_path, "player_nbt_original_metadata")
    jar = args.client_jar.resolve(strict=True)
    java = args.java.resolve(strict=True)
    javac = java.with_name("javac.exe" if os.name == "nt" else "javac")
    pins_path = HERE / "source-pins.json"
    pins = json.loads(pins_path.read_text())
    libraries = [args.libraries.resolve() / name for name in LIBRARIES]
    native = args.natives.resolve() / "lwjgl64.dll"
    inputs = [jar, java, javac, native, *libraries, capture_path, metadata_path, pins_path,
              Path(__file__).resolve(), HERE / "PlayerNbtObservation.java", HERE / "PlayerNbtPrefix.java"]
    report = {"status": "FAIL", "scope": "Unchanged-original only; no native parity/qualification claim"}
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(name, None)
    before = None
    helper_before = None
    classes = output / "classes"

    def run(command, prefix, *, timeout=180, empty_stderr=True):
        free = shutil.disk_usage(output).free
        report.setdefault("phase_capacity", []).append({"phase": prefix, "free_bytes": free,
                                                       "required_bytes": args.minimum_free_bytes})
        need(free >= args.minimum_free_bytes, "CAPACITY_HOLD before " + prefix)
        result = capture.run_bounded(command, cwd=output, prefix=prefix, timeout=timeout,
                                     limit=4 * 1024 * 1024, env=env)
        report[prefix] = result.capture_report
        require_process(result, empty_stderr=empty_stderr)
        return result

    try:
        need(java.parent.samefile(javac.parent) and javac.is_file(), "same-bin matching JDK required")
        need(digest(jar) == CLIENT == pins["jar"]["sha256"], "original JAR pin differs")
        need(digest(native) == NATIVE, "approved actual LWJGL CPU library differs")
        for path in libraries:
            need(digest(path) == LIBRARIES[path.name], "SDK pin differs " + path.name)
        with zipfile.ZipFile(jar) as archive:
            for name, expected in pins["classes"].items():
                need(metadata.inspect_class(archive.read(name + ".class")) == expected,
                     "whole class/Code facts differ " + name)
        before = {str(path): digest(path) for path in inputs}
        report["input_before_sha256"] = before
        versions = []
        for name, tool in (("java", java), ("javac", javac)):
            result = run([str(tool), "-version"], name + "-version", timeout=10, empty_stderr=False)
            versions.append((result.stdout + result.stderr).decode())
        vm = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0])
        compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        need(vm and compiler and vm[1] == compiler[1], "matching actual JDK17 runtime/compiler required")
        report["jdk"] = versions
        classes.mkdir()
        cp = os.pathsep.join(str(path) for path in [classes, jar, *libraries])
        run([str(javac), "--add-modules", "jdk.jdi", "-encoding", "UTF-8", "-classpath", cp,
             "-d", str(classes), str(HERE / "PlayerNbtObservation.java"),
             str(HERE / "PlayerNbtPrefix.java")], "compile", timeout=30)
        helper_before = {path.name: digest(path) for path in classes.glob("*.class")}
        need(set(helper_before) == HELPERS, "unexpected authored helper inventory")
        report["helper_before_sha256"] = helper_before
        report["helper_class_code"] = {path.name: metadata.inspect_class(path.read_bytes())
                                       for path in classes.glob("*.class")}
        result = run([str(java), "-Xint", "-Xverify:all", "-Xmx512m", "--add-opens",
                      "java.base/java.util=ALL-UNNAMED", "-Dorg.lwjgl.librarypath=" + str(args.natives.resolve()),
                      "-classpath", cp, "PlayerNbtObservation", str(output / "normal-worlds"),
                      str(jar), "normal"], "original", timeout=180)
        report["normal_observations"] = validate_normal(result.stdout.decode("utf-8"), pins, jar)
        gs_read = next(v for v in pins["classes"]["gs"]["methods"]
                       if v["name"] == "a" and v["descriptor"] == "(Lnu;)V")
        prefix = run([str(java), "--add-modules", "jdk.jdi", "-classpath", cp,
                      "PlayerNbtPrefix", cp, str(output / "sleep-worlds"), str(jar),
                      str(args.natives.resolve()), str(output / "sleep-target.stdout"),
                      str(output / "sleep-target.stderr"), gs_read["code_sha256"]],
                     "prefix", timeout=150)
        need((output / "sleep-target.stderr").read_bytes() == b"", "target stderr nonempty")
        target = (output / "sleep-target.stdout").read_text()
        prefix_tags = validate_prefix(prefix.stdout.decode(), target, pins, jar)
        report["sleeping_prefix"] = {"checkpoints": 1, "tag_rows": prefix_tags, "caller": "gs.a(Lnu;)V", "bytecode": 93,
                                     "original_after_checkpoint": "completed wake/read/clear/publication",
                                     "native_scope": "pending wake boundary, never full-call parity"}
        report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        after = {str(path): digest(path) for path in inputs if path.is_file()}
        report["input_after_sha256"] = after
        if before is not None and before != after:
            report["status"] = "FAIL"
            report["input_changed"] = True
        if classes.is_dir():
            helper_after = {path.name: digest(path) for path in classes.glob("*.class")}
            report["helper_after_sha256"] = helper_after
            if helper_before is not None and helper_before != helper_after:
                report["status"] = "FAIL"
                report["helper_changed"] = True
        report["products"] = {path.name: digest(path) for path in output.iterdir() if path.is_file()}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    need(report["status"] == "PASS", "observer integrity failure")
    print("Original Player NBT: " + str(report["normal_observations"]) + "; report " + str(output / "report.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
