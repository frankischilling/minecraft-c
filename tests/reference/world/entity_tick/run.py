"""Bounded native CTest driver and unchanged-client World Entity-update comparison."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}
ORIGINS = ("hl", "sn", "fd", "lm", "li", "iz", "pm", "ow", "js", "cy", "uk", "ju", "yl", "dc", "gs", "ls", "gr")
PREFIXES = {17, 18, 19, 30}


def need(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def projection(text):
    records = [line for line in text.splitlines() if line.startswith(("STATE ", "EVENT ", "RIDE ", "TILE ", "TP "))]
    scenes = [s for s in range(24) if s != 7]
    events = {15:[0],16:[0],17:[0,0],18:[0],19:[0,1]}
    index = 0
    for scene in scenes:
        for event, identity in enumerate(events.get(scene, [])):
            need(index < len(records), "missing event")
            need(records[index].split() == ["EVENT",str(scene),str(event),"R",str(identity)], "event inventory/order")
            index += 1
        need(index < len(records), "missing state")
        fields = records[index].split()
        need(fields[:3] == ["STATE",str(scene),"0" if scene == 18 else "1"], "state inventory/order")
        need(len(fields) == 33, "partial state record")
        index += 1
    for scene in range(7):
        need(index < len(records), "missing riding record")
        fields=records[index].split()
        need(fields[:2]==["RIDE",str(scene)] and len(fields)==18, "riding inventory/width")
        index+=1
    for scene in range(7):
        if scene in (1,2,3,4,5):
            for event,name in enumerate(("smoke","flame")):
                need(index<len(records),"missing particle")
                fields=records[index].split()
                need(fields[:4]==["TP",str(scene),str(event),name] and len(fields)==7,"particle inventory/width")
                index+=1
        need(index<len(records),"missing TileEntity state")
        fields=records[index].split()
        need(fields[:3]==["TILE",str(scene),"0" if scene==6 else "1"] and len(fields)==11,"TileEntity inventory/width")
        index+=1
    need(index == len(records) == 54, "extra/trailing records")
    return records


def native_count(text):
    lines = text.splitlines()
    need(bool(lines), "empty native output")
    match = re.fullmatch(r"World Entity tick: ([1-9][0-9]*) checks", lines[-1])
    need(match is not None, "missing positive native completion marker")
    return int(match[1])


def require_process(result, *, empty_stderr=True):
    need(result.returncode == 0, f"exit {result.returncode}")
    need(not empty_stderr or result.stderr == b"", "nonempty stderr")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path)
    parser.add_argument("--library-dir", type=Path)
    parser.add_argument("--java", type=Path)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[3])
    args = parser.parse_args(argv)
    source = args.source_root.resolve()
    support_path = source / "tests/reference/client/statistics/support.py"
    classfile_path = source / "tests/reference/client/notice/classfile.py"
    support = module(support_path, "world_tick_capture")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="world-tick-", dir=args.work_dir.resolve()))
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(name, None)
    inputs = [args.native.resolve(), Path(__file__).resolve(), support_path]
    report = {"status": "FAIL", "scope": "actual World Entity-update methods; supported Player/Item/TE paths and explicit reached dependencies, not complete World time/block update"}
    before = None
    classes = None
    helper_before = None

    def run(command, prefix, *, empty_stderr=True, timeout=180):
        result = support.run_bounded(command, cwd=output, prefix=prefix, timeout=timeout, env=env)
        require_process(result, empty_stderr=empty_stderr)
        return result

    try:
        if args.client_jar is not None:
            need(args.library_dir is not None, "--library-dir required with --client-jar")
            jar = args.client_jar.resolve()
            need(digest(jar) == JAR, "client JAR differs")
            libraries = [args.library_dir.resolve() / name for name in LIBRARIES]
            for path in libraries:
                need(digest(path) == LIBRARIES[path.name], f"library differs: {path.name}")
            java = (args.java or Path(shutil.which("java") or "")).resolve(strict=True)
            javac = java.with_name("javac.exe" if os.name == "nt" else "javac")
            need(javac.is_file() and java.parent.samefile(javac.parent), "matching same-bin JDK required")
            metadata = module(classfile_path, "world_tick_classfile")
            pins_path = HERE / "source-pins.json"
            pins = json.loads(pins_path.read_text())
            with zipfile.ZipFile(jar) as archive:
                for name, expected in pins["classes"].items():
                    need(metadata.inspect_class(archive.read(name + ".class")) == expected,
                         "whole class/Code metadata differs: " + name)
            dispatch_path=HERE / "dispatch-pins.json"
            dispatch=json.loads(dispatch_path.read_text())
            for receiver,chain in dispatch["chains"].items():
                for signature,expected in dispatch["resolved"][receiver].items():
                    resolved=None
                    for owner in chain:
                        methods=[m for m in pins["classes"][owner]["methods"] if m["name"]+m["descriptor"]==signature]
                        if methods:
                            resolved={"owner":owner,"method":methods[0]}
                            break
                    need(resolved==expected,"actual method resolution differs: "+receiver+"."+signature)
            inputs += [java, javac, jar, *libraries, classfile_path, pins_path, dispatch_path,
                       HERE / "WorldTickObservation.java"]
        before = {str(path): digest(path) for path in inputs}
        report["input_before_sha256"] = before
        if args.client_jar is not None:
            versions = []
            for name, tool in (("java", java), ("javac", javac)):
                result = run([str(tool), "-version"], name + "-version", empty_stderr=False, timeout=10)
                versions.append((result.stdout + result.stderr).decode("utf-8", "strict"))
            vm = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0])
            compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
            need(vm and compiler and vm[1] == compiler[1], "matching JDK17 java/javac required")
            classes = output / "classes"
            classes.mkdir()
            cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
            run([str(javac), "-encoding", "UTF-8", "-classpath", cp, "-d", str(classes),
                 str(HERE / "WorldTickObservation.java")], "compile", timeout=30)
            helper_before = {p.name: digest(p) for p in classes.glob("*.class")}
            need(set(helper_before) == {"WorldTickObservation.class", "WorldTickObservation$Observer.class"},
                 "unexpected compiled helper inventory")
            report["helper_before_sha256"] = helper_before
            report["helper_class_code"] = {p.name: metadata.inspect_class(p.read_bytes()) for p in classes.glob("*.class")}
            original = run([str(java), "--add-opens", "java.base/java.util=ALL-UNNAMED", "-Xint", "-Xverify:all",
                            "-classpath", cp, "WorldTickObservation", str(output / "original-worlds"), str(jar)], "original")
            left = original.stdout.decode("ascii").splitlines()
            need(left[-1] == "Original World Entity tick: 23 states, 7 riding states and 7 TileEntity states", "original completion marker missing")
            origins = [line.split(" ", 2) for line in left if line.startswith("ORIGIN ")]
            need([line[1] for line in origins] == list(ORIGINS), "original origin inventory changed")
            need(all(Path(line[2]).resolve().samefile(jar) for line in origins), "foreign actual game class")
            expected = projection(original.stdout.decode("ascii"))
            report["original"] = original.capture_report
            report["jdk"] = versions
        resources = output / "resources"
        (resources / "lang").mkdir(parents=True)
        (resources / "achievement").mkdir()
        (resources / "lang/en_US.lang").write_text("achievement.openInventory=Inventory\nachievement.openInventory.desc=Use %1$s\nkey.inventory=Inventory\n")
        for language in ("stats_US.lang","en_GB.lang"):
            (resources / "lang" / language).write_text("")
        (resources / "achievement/map.txt").write_text("")
        native = run([str(args.native.resolve()), "world-entity-tick-reference" if args.client_jar else "world-entity-tick",
                      str(output / "native-worlds").encode().hex(), str(resources).encode().hex()], "native")
        count = native_count(native.stdout.decode("ascii"))
        report["native"] = native.capture_report
        if args.client_jar is not None:
            actual = projection(native.stdout.decode("ascii"))
            need(actual == expected, {"first_difference": next(((i, a, b) for i, (a, b) in enumerate(zip(expected, actual)) if a != b), None)})
            report.update(world_states=23, world_completed_calls=22, world_interrupted_prefixes=1, removal_event_records=7, riding_states=7, tile_states=7, tile_interrupted_prefixes=1, particle_records=10, states=37, completed_calls=35, interrupted_prefixes=2, event_records=17, records=54)
        report.update(status="PASS", native_checks=count, reference=args.client_jar is not None)
    finally:
        after = {str(path): digest(path) for path in inputs}
        report["input_after_sha256"] = after
        if before is not None and before != after:
            report["status"] = "FAIL"
            report["input_changed"] = True
        if classes is not None:
            helper_after = {p.name: digest(p) for p in classes.glob("*.class")}
            report["helper_after_sha256"] = helper_after
            if helper_before != helper_after:
                report["status"] = "FAIL"
                report["helper_changed"] = True
        report["products"] = {p.name: digest(p) for p in sorted(output.glob("*")) if p.is_file()}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    need(report["status"] == "PASS", "input/helper/product integrity failure")
    print(f"World Entity tick qualification: {report['native_checks']} checks; report {output / 'report.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
