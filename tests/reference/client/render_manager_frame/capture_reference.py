"""Genuine cold/prepared TH constructor and awake/null-Living frame capture only."""
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
spec = importlib.util.spec_from_file_location("manager_frame_pinned_capture_support", HERE.parent / "tile_entity_renderer/capture_reference.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
ensure, digest, capture, require_success = base.ensure, base.digest, base.capture, base.require_success
PROFILES = ("cold_th", "prepared_awake", "prepared_null")
TAGS = {"RM_CASE", "RM_PREP", "RM_PREREQ_THROW", "RM_COLD_THROW", "RM_TH_NORMAL", "RM_ROSTER", "RM_REF", "RM_CTOR",
        "RM_ORIGINS", "RM_PIG", "RM_INPUT", "RM_N", "RM_FRAME", "RM_SIDE", "RM_NULL_BEFORE", "RM_NULL_THROW", "RM_NULL_AFTER", "RM_NULL_STABLE", "RM_NULL_SIDE"}


def encode(cases):
    return [" ".join([str(case["id"]), case["name"], case["partial"], *case["float_pose"], *case["double_pose"]]) for case in cases]


def records(text, profile, cases, pins):
    lines = text.splitlines()
    result = [line for line in lines if line.split(" ", 1)[0] in TAGS]
    ensure(result and result[0] == "RM_CASE " + profile and lines[-1] ==
           f"Original RenderManager frame: {profile}; rows={len(result)}; network=0; native=0; writes=0", "whole profile completion/zero external work")
    ensure(not any(line.startswith("RM_PREREQ_THROW ") for line in result), "real prepared Statistics-first prerequisite returned")
    prepared = profile != "cold_th"
    ensure(sum(line.startswith("RM_PREP ") for line in result) == int(prepared), "declared full Statistics entry inventory")
    throws = [line.split() for line in result if line.startswith("RM_COLD_THROW ")]
    normal = [line for line in result if line.startswith("RM_TH_NORMAL ")]
    ensure((not throws and normal == ["RM_TH_NORMAL 1"]) or (profile == "cold_th" and throws and not normal),
           "actual cold success/failure is distinct from completed prepared TH publication")
    for index, failure in enumerate(throws):
        ensure(len(failure) == 6 and failure[1] == str(index) and re.fullmatch(r"-?[0-9]+", failure[5]), "complete unnormalized actual cold cause/frame chain")
    roster = [line.split() for line in result if line.startswith("RM_ROSTER ")]
    refs = [line.split() for line in result if line.startswith("RM_REF ")]
    if normal:
        expected = sorted((entry["key"], entry["renderer"]) for entry in pins["th_constructor_roster"])
        ensure([(row[1], row[2]) for row in roster] == expected and len(roster) == 28 and
               all(len(row) == 8 and row[3] == "1" and row[4] == "fh" and row[5] == "cv" and
                   row[6].isdigit() and row[7].isdigit() for row in roster), "all28 actual BW renderers/base models/CVs/backpointers")
        ensure(refs and all(len(row) == 5 and row[4].isdigit() for row in refs) and
               set(row[1] for row in refs) == set(row[1] for row in roster), "all actual declared renderer reference/model aliases")
        ensure(sum(line.startswith("RM_CTOR ") for line in result) == 1 and
               sum(line.startswith("RM_ORIGINS ") for line in result) == 1, "actual constructor and raw source origins")
        orders = [line for line in lines if line.startswith("RM_MAP_ORDER ")]
        ensure(len(orders) == 28 and [line.split()[1] for line in orders] == [str(i) for i in range(28)], "complete unsorted physical Map iteration diagnostics")
    else:
        ensure(not roster and not refs and not any(line.startswith(("RM_CTOR ", "RM_ORIGINS ", "RM_PIG ", "RM_FRAME ")) for line in result),
               "failed cold initializer receives no complete roster/frame credit")
    ids = list(range(24)) if profile == "prepared_awake" else [0] if profile == "prepared_null" else []
    for tag in ("RM_INPUT", "RM_N", "RM_FRAME", "RM_SIDE"):
        ensure([int(line.split()[1]) for line in result if line.startswith(tag + " ")] == ids, "complete mandatory awake cases: " + tag)
    for line in result:
        row = line.split()
        if row[0] == "RM_PREP":
            ensure(len(row) == 15 and row[1:4] == ["jl", "a", "normal"] and
                   all(re.fullmatch(r"-?[0-9]+", word) for word in row[4:13]) and row[13] in ("0", "1") and row[14] in ("0", "1"), "actual whole Statistics prerequisite effects")
        elif row[0] == "RM_CTOR":
            ensure(len(row) == 9 and row[1:7] == ["th", "java.util.HashMap", "28", "28", "28", "null"] and
                   all(word.isdigit() and int(word) > 0 for word in row[7:]), "actual full TH graph and nullable unconstructed RA")
        elif row[0] == "RM_ORIGINS":
            ensure(len(row) == 4 and all(re.fullmatch(r"[0-9a-f]{16}", word) for word in row[1:]), "unmodified raw original source origin fields")
        elif row[0] == "RM_PIG":
            ensure(len(row) == 5 and row[1] == "wh" and all(re.fullmatch(r"-?[0-9]+", word) for word in row[2:]), "actual original Pig constructor side-effect counts")
        elif row[0] == "RM_INPUT":
            ensure(len(row) == 14 and row[2] == cases[int(row[1])]["name"] and
                   all(re.fullmatch(r"[0-9a-f]{8}", word) for word in row[3:8]) and
                   all(re.fullmatch(r"[0-9a-f]{16}", word) for word in row[8:]), "actual stored pose/partial raw words")
        elif row[0] == "RM_N":
            ensure(row[2:] == ["ls", "0"], "actual unchanged inherited Living.N false")
        elif row[0] == "RM_FRAME":
            ensure(len(row) == 12 and row[2:7] == ["1"] * 5 and
                   all(re.fullmatch(r"[0-9a-f]{8}", word) for word in row[7:9]) and
                   all(re.fullmatch(r"[0-9a-f]{16}", word) for word in row[9:]), "actual five reference stores and raw awake outputs")
        elif row[0] == "RM_SIDE":
            ensure(len(row) == 11 and row[2:] == ["1"] * 9, "actual Random/Math/renderer-Random/ID/pose/Watcher/roster-model-RA/origin stability")
        elif row[0] == "RM_NULL_THROW":
            ensure(len(row) == 5 and row[1:4] == ["java.lang.NullPointerException", "th", "a"] and
                   re.fullmatch(r"-?[0-9]+", row[4]), "actual unnormalized null-Living exception and top Java frame")
    for tag in ("RM_NULL_BEFORE", "RM_NULL_THROW", "RM_NULL_AFTER", "RM_NULL_STABLE", "RM_NULL_SIDE"):
        ensure(sum(line.startswith(tag + " ") for line in result) == int(profile == "prepared_null"), "whole null-prefix inventory: " + tag)
    if profile == "prepared_null":
        before = next(line.split() for line in result if line.startswith("RM_NULL_BEFORE "))
        after = next(line.split() for line in result if line.startswith("RM_NULL_AFTER "))
        ensure(len(before) == 6 and all(re.fullmatch(r"[0-9a-f]{8}", word) for word in before[1:3]) and
               all(re.fullmatch(r"[0-9a-f]{16}", word) for word in before[3:]) and
               after == ["RM_NULL_AFTER", *(["1"] * 5), *before[1:]], "five actual null stores then unchanged scalar failure prefix")
        ensure("RM_NULL_STABLE 1 1" in result and "RM_NULL_SIDE " + " ".join(["1"] * 9) in result, "null call leaves graph, Random, IDs, Watcher and primitive origins unchanged")
    ensure(sum(line.startswith("RM_PIG ") for line in result) == int(prepared), "genuine Pig constructor operand inventory")
    ensure(all(not line.startswith("RM_") or line.split(" ", 1)[0] in TAGS | {"RM_MAP_ORDER", "RM_EXCEPTION_DETAIL"} for line in lines), "no unparsed observer record kind")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", required=True, type=Path)
    parser.add_argument("--library-root", required=True, type=Path)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    ensure(args.java and args.javac, "matching JDK17 tools")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), "same JDK17 bin directory")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in base.LIBRARIES]
    ensure(digest(jar) == base.JAR and all(digest(path) == pin for path, pin in zip(libraries, base.LIBRARIES.values())), "pinned complete original archive/SDKs")
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    ensure(pins["client_jar_sha256"] == base.JAR, "unchanged source authority")
    all_code = {}
    with zipfile.ZipFile(jar) as archive:
        for entry in sorted(archive.namelist()):
            if entry.endswith(".class"):
                all_code[entry[:-6]] = base.classfile.inspect_class(archive.read(entry))
    for name, expected in pins["classes"].items():
        actual = dict(all_code[name])
        selected = {(method["name"], method["descriptor"]) for method in expected["methods"]}
        actual["methods"] = [method for method in actual["methods"] if (method["name"], method["descriptor"]) in selected]
        ensure(actual == expected, "whole class/selected Code/Code-attribute pins: " + name)
    inputs_spec = json.loads((HERE / "frame-inputs.json").read_text(encoding="utf-8"))
    cases = inputs_spec["cases"]
    ensure(len(cases) == 24 and [case["id"] for case in cases] == list(range(24)) and inputs_spec["profiles"] == list(PROFILES), "all three fresh profiles and24 mandatory operands")
    for case in cases:
        ensure(re.fullmatch(r"[a-z0-9_]+", case["name"]) and len(case["float_pose"]) == 4 and len(case["double_pose"]) == 6 and
               all(re.fullmatch(r"[0-9a-f]{8}", word) for word in [case["partial"], *case["float_pose"]]) and
               all(re.fullmatch(r"[0-9a-f]{16}", word) for word in case["double_pose"]), "raw input-only source fields")
    args.output.mkdir(parents=True, exist_ok=False)
    metadata = args.output / "all-class-code.json"
    metadata.write_text(json.dumps(all_code, indent=2) + "\n", encoding="utf-8", newline="\n")
    tsv = args.output / "frame-inputs.tsv"
    tsv.write_text("\n".join(encode(cases)) + "\n", encoding="ascii", newline="\n")
    inputs = [java, javac, jar, *libraries, metadata, tsv, *[HERE / name for name in
              ("RenderManagerFrameObservation.java", "capture_reference.py", "frame-inputs.json", "source-pins.json", "README.md")],
              HERE.parent / "tile_entity_renderer/capture_reference.py", HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "profiles": [], "input_before_sha256": before,
              "all_original_class_code_inventory": {"file": metadata.name, "sha256": digest(metadata), "classes": len(all_code)},
              "scope": "Separate genuine cold TH outcome and explicit original Statistics-first prerequisite followed by full TH28 "
                       "and actual Pig(NULL), awake24 arithmetic and null-Living29 prefix. No World/Engine/Font/Settings instance, "
                       "sleep/bed path, GL/device/draw or whole-startup/whole-frame completion credit. Original class-loader "
                       "resources reached by jl.a remain inside the pinned original archive; no supplied replacement resources.",
              "declared_precondition": inputs_spec["prepared_entry"],
              "source_frame": {"class": "th", "method": "a", "descriptor": "(Lfd;Lji;Lsj;Lls;Lkv;F)V", "null_living_pc": 29,
                               "stack_line_policy": "actual Java lines retained; PC29 is independent unchanged Code authority"}}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    classes = args.output / "classes"
    classes.mkdir()
    try:
        versions = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            versions.append((args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text())
        runtime, compiler = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0]), re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], "matching JDK17 versions")
        report["jdk"] = versions
        cp = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), str(HERE / "RenderManagerFrameObservation.java")], args.output, "compile", 30, environment)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helper) == {"RenderManagerFrameObservation.class", "RenderManagerFrameObservation$1.class", "RenderManagerFrameObservation$Slot.class", "RenderManagerFrameObservation$ArraySlot.class"}, "only authored observer classes")
        report["helper_before_sha256"] = helper
        observed = []
        for profile in PROFILES:
            work = args.output / profile
            work.mkdir()
            command = [str(java), "--add-opens", "java.base/java.util=ALL-UNNAMED", "--add-opens", "java.base/java.lang=ALL-UNNAMED",
                       "-Xint", "-Xverify:all", "-Xmx512m", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                       "-Xlog:class+init=info:file=class-init.log::filesize=2m,filecount=1", "-cp", cp,
                       "RenderManagerFrameObservation", str(jar), profile, str(tsv.resolve())]
            receipt = capture(command, args.output, profile, 60, environment, work.resolve())
            item = {"name": profile, "capture": receipt}
            report["profiles"].append(item)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text(encoding="utf-8")
            actual = records(text, profile, cases, pins)
            log = work / "class-init.log"
            ensure(log.is_file() and log.stat().st_size < 2 * 1024 * 1024 and len(list(work.glob("class-init.log*"))) == 1, "complete bounded initialization log")
            initialized = re.findall(r"Initializing '([^']+)'", log.read_text(encoding="utf-8"))
            initialized_game = list(dict.fromkeys(name for name in initialized if name in all_code))
            ensure("th" in initialized_game and not any(name.startswith(("org/lwjgl/", "org.lwjgl.")) for name in initialized), "real TH attempt and no SDK active/native work")
            if profile != "cold_th":
                ensure(all(name in initialized_game for name in ("jl", "gm", "uu", "th", "bw", "cv", "fh", "ps", "bt", "wh", "bg", "ii", "ls", "sn", "eq", "ud", "ma")) and
                       all(row.split()[2] in initialized_game for row in actual if row.startswith("RM_ROSTER ")), "full genuine prerequisite/renderer/model/Pig initialization")
            item.update(status="PASS", rows=len(actual), initialized_original_classes=initialized_game, initialization_log_sha256=digest(log),
                        diagnostics=[line for line in text.splitlines() if line.startswith(("RM_MAP_ORDER ", "RM_EXCEPTION_DETAIL ")) or not line.startswith(("RM_", "Original RenderManager frame:"))])
            observed.extend(actual)
        fixture = args.output / "finite.json"
        fixture.write_text(json.dumps({"rows": observed}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=len(observed), fixture_sha256=digest(fixture))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same: report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "all original archive/Code metadata/tool/helper/input bytes unchanged")
    print("Original RenderManager frame: " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
