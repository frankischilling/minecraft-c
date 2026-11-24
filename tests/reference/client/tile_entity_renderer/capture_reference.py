"""Capture unchanged Beta TileEntityRenderer construction and passive-Class lookup.

The four mandatory profiles use fresh JVMs; finite.json records their observed
stdout. No game class or constructor body is replaced.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


support = load("tile_entity_renderer_capture_owner", HERE.parent / "controller/capture.py")
classfile = load("tile_entity_renderer_classfile", HERE.parent / "notice/classfile.py")
ensure, digest, capture, require_success = support.ensure, support.digest, support.capture, support.require_success
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}
PROFILES = ("constructor", "forward", "reverse", "nulls")
TILES = ("sk", "js", "eg", "az", "yk", "cy", "tn", "uk", "ow")
TAGS = {"TILE_CASE", "TILE_LITERAL", "TILE_RENDERER", "TILE_CTOR", "TILE_DEFAULTS", "TILE_MAP",
        "TILE_ENTRY", "TILE_SIGN", "TILE_PART", "TILE_VERTEX", "TILE_QUAD", "TILE_GRAPH",
        "TILE_LOOKUP", "TILE_NULL_TILE", "TILE_NULL_CLASS"}


def rows(text, profile):
    lines = text.splitlines()
    observed = [line for line in lines if line.split(" ", 1)[0] in TAGS]
    ensure(observed and observed[0] == "TILE_CASE " + profile, "complete profile identity")
    footer = f"Original TileEntityRenderer: {profile}; rows={len(observed)}; network=0; native=0; writes=0"
    ensure(lines[-1] == footer, "normal helper completion and zero external-work counts")
    ensure([line.split()[1] for line in observed if line.startswith("TILE_LITERAL ")] == list(TILES),
           "complete passive Class literal inventory/order")
    for tag, count in (("TILE_RENDERER", 3), ("TILE_CTOR", 1), ("TILE_DEFAULTS", 1)):
        ensure(sum(line.startswith(tag + " ") for line in observed) == count, "constructor inventory: " + tag)
    geometry = {"TILE_SIGN": 1, "TILE_PART": 2, "TILE_VERTEX": 16, "TILE_QUAD": 12, "TILE_GRAPH": 1}
    for tag, count in geometry.items():
        ensure(sum(line.startswith(tag + " ") for line in observed) == (count if profile == "constructor" else 0),
               "complete constructor geometry inventory: " + tag)
    for line in observed:
        cells = line.split()
        if cells[0] == "TILE_PART":
            ensure(len(cells) == 15 and all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[-6:]),
                   "raw part-state words")
        elif cells[0] == "TILE_VERTEX":
            ensure(len(cells) == 8 and all(re.fullmatch(r"[0-9a-f]{16}", value) for value in cells[3:6]) and
                   all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[6:]), "raw position/UV words")
        elif cells[0] == "TILE_QUAD":
            ensure(len(cells) == 21 and all(re.fullmatch(r"[0-9a-f]{8}", cells[i])
                   for base in range(5, 21, 4) for i in (base + 2, base + 3)), "raw quad UV/alias words")
    lookups = [line.split() for line in observed if line.startswith("TILE_LOOKUP ")]
    if profile in ("forward", "reverse"):
        order = TILES if profile == "forward" else tuple(reversed(TILES))
        ensure([(row[1], row[2]) for row in lookups] == [(str(i), key) for i, key in enumerate(order)],
               "all nine original lookup and repeated-query operands")
    else:
        ensure(not lookups, "no lookup-profile substitution")
    ensure(sum(line.startswith("TILE_NULL_TILE ") for line in observed) == int(profile == "nulls") and
           sum(line.startswith("TILE_NULL_CLASS ") for line in observed) == int(profile == "nulls"),
           "separate null receiver and actual null-Class failure inventory")
    stages = ["initial"]
    if profile in ("forward", "reverse"):
        stages.extend(f"lookup{i:02d}" for i in range(9))
    if profile == "nulls":
        stages.append("after_nulls")
    maps = [line.split() for line in observed if line.startswith("TILE_MAP ")]
    ensure([row[1] for row in maps] == stages, "all reached Map snapshots")
    for record in maps:
        ensure(len(record) == 8 and all(value.isdigit() for value in record[2:]), "Map snapshot shape")
        stage, size = record[1], int(record[2])
        entries = [line for line in observed if line.startswith("TILE_ENTRY " + stage + " ")]
        iterations = [line for line in lines if line.startswith("TILE_ITERATION " + stage + " ")]
        buckets = [line for line in lines if line.startswith("TILE_BUCKET " + stage + " ")]
        receipts = [line for line in lines if line.startswith("TILE_MAP_RECEIPT " + stage + " ")]
        ensure(0 <= size <= 8 and len(entries) == len(iterations) == len(buckets) == size and
               entries == sorted(entries) and receipts == [f"TILE_MAP_RECEIPT {stage} {size} {size}"],
               "complete logical entries plus separate raw physical Map evidence")
    allowed = TAGS | {"TILE_ITERATION", "TILE_BUCKET", "TILE_MAP_RECEIPT", "TILE_EXCEPTION"}
    ensure(all(not line.startswith("TILE_") or line.split(" ", 1)[0] in allowed for line in lines),
           "no unparsed observer row kind")
    return observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(args.java and args.javac, "matching JDK17 tools required")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "matching JDK17 bin directory")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
           "untouched original client and pinned SDK type archives")
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(pins["client_jar_sha256"] == JAR, "original source-pin authority")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = classfile.inspect_class(archive.read(name + ".class"))
            selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"] if (method["name"], method["descriptor"]) in selected]
            ensure(actual == expected, "complete class and selected Code/Code-attribute pins: " + name)
    args.output.mkdir(parents=True, exist_ok=False)
    inputs = [java, javac, jar, *libraries, HERE / "TileEntityRendererObservation.java",
              HERE / "capture_reference.py", HERE / "source-pins.json", HERE / "README.md",
              HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "Unchanged Beta TileEntityRenderer singleton/constructor and nullable Class lookup only; "
                       "no TileEntity instances, active OW/Piston initialization, World bind, assets, graphics, native links or draws. "
                       "Logical Map rows sort class names; raw iteration/bucket/hash evidence remains separately in complete stdout.",
              "null_class_pc": {"class": "ll", "method": "a", "descriptor": "(Ljava/lang/Class;)Lje;",
                                "pc": 26, "authority": "pinned original bytecode, not a stack-trace BCI measurement"}}
    environment = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(key, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        versions = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            versions.append((args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text())
        runtime = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0])
        compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], "matching JDK17 runtime/compiler")
        report["jdk"] = versions
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                           str(HERE / "TileEntityRendererObservation.java")], args.output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helper) == {"TileEntityRendererObservation.class", "TileEntityRendererObservation$1.class"},
               "only authored observer helper classes, no rewritten game class")
        report["helper_before_sha256"] = helper
        observed = []
        for profile in PROFILES:
            work = args.output / profile
            work.mkdir()
            receipt = capture([str(java), "--add-opens", "java.base/java.util=ALL-UNNAMED", "-Xint", "-Xverify:all",
                               "-Xmx256m", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                               "-Xlog:class+init=info:file=class-init.log::filesize=2m,filecount=1",
                               "-cp", cp, "TileEntityRendererObservation", str(jar), profile],
                              args.output, profile, 60, environment, work.resolve())
            item = {"name": profile, "capture": receipt}
            report["profiles"].append(item)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text(encoding="utf-8")
            actual = rows(text, profile)
            log = work / "class-init.log"
            ensure(log.is_file() and log.stat().st_size < 2 * 1024 * 1024 and
                   len(list(work.glob("class-init.log*"))) == 1, "complete bounded initialization log")
            initialized = re.findall(r"Initializing '([^']+)'", log.read_text(encoding="utf-8"))
            ensure(all(name not in initialized for name in TILES) and
                   not any(name.startswith(("org/lwjgl/", "org.lwjgl.")) for name in initialized),
                   "passive TileEntity literals, no OW/Piston/SDK active initialization")
            ensure(all(name in initialized for name in ("ll", "je", "po", "ag", "hy", "rf", "ps", "ib", "tz", "bt")),
                   "complete actual constructor graph initialization")
            item.update(status="PASS", rows=len(actual), initialization_log_sha256=digest(log),
                        raw_map_diagnostics=[line for line in text.splitlines() if line.startswith(
                            ("TILE_ITERATION ", "TILE_BUCKET ", "TILE_MAP_RECEIPT ", "TILE_EXCEPTION "))])
            observed.extend(actual)
        fixture = args.output / "finite.json"
        fixture.write_text(json.dumps({"rows": observed}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=len(observed), fixture_sha256=digest(fixture))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
               report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "original archive, SDK, tools, sources and authored helper bytes unchanged")
    print("Original TileEntityRenderer: all four profiles; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
