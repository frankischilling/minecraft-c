"""Capture exhaustive authored current/array/compiled-list normal diagnostics."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
sys.path.insert(0, str(HERE.parent / "render_global_sky"))
from capture_prefix import JAR, LIBRARIES


def words(text, count):
    return re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){" + str(count - 1) + "}", text) is not None


def rows(text):
    found = [line for line in text.splitlines()
             if line.startswith(("NORMAL_LIST_STORAGE ", "NORMAL_LIST_DECODE ", "NORMAL_LIST_STATE "))]
    ensure(len(found) == 518, "256 storage/decode pairs plus six actual state receipts")
    for index, value in enumerate(range(-128,128)):
        storage, decoded = (line.split() for line in found[2 * index:2 * index + 2])
        ensure(len(storage) == 5 and storage[:2] == ["NORMAL_LIST_STORAGE", str(value)] and
               all(re.fullmatch(r"[0-9a-f]{64}", item) for item in storage[2:]) and
               len(set(storage[2:])) == 3, "distinct actual source/two poisoned client-storage hashes")
        ensure(len(decoded) == 7 and decoded[:2] == ["NORMAL_LIST_DECODE", str(value)] and
               words(decoded[2],3) and all(words(item,9) for item in decoded[3:]),
               "all current-query/current-TF/array-TF/list-TF/repeated-list-TF components")
        ensure(decoded[5] == decoded[6], "compiled attributes survive independent post-compile input changes")
    labels = ("before_compile", "after_compile", "before_call", "after_call", "before_repeat", "after_repeat")
    for label, line in zip(labels,found[512:]):
        values = line.split()
        ensure(len(values) == 11 and values[:2] == ["NORMAL_LIST_STATE", label] and
               words(values[2],3) and all(words(item,4) for item in values[3:8]) and
               values[8:] == ["1", "1032", "5634"],
               "actual current normal/RGBA/front-back tracked material and selectors")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt" and args.java is not None and args.javac is not None,
           "declared Windows/JDK17 diagnostic profile")
    java, javac = [Path(shutil.which(value) or value).resolve(strict=True) for value in (args.java,args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == expected
           for path,expected in zip(libraries,LIBRARIES.values())), "unchanged original/SDK archive pins")
    archive = (args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar").resolve(strict=True)
    ensure(digest(archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43",
           "pinned Windows native archive")
    args.output.mkdir(parents=True, exist_ok=False)
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(archive) as source:
        (natives / "lwjgl64.dll").write_bytes(source.read("lwjgl64.dll"))
    classes = args.output / "classes"
    classes.mkdir()
    helper_source = HERE / "NormalListConversionReceiptObservation.java"
    inputs = [java,javac,jar,*libraries,archive,natives / "lwjgl64.dll",helper_source,
              HERE / "capture_list_conversion_receipts.py",SUPPORT / "capture.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name,None)
    report = {"status": "FAIL", "input_before_sha256": before,
              "scope": "Separate authored legacy-driver diagnostic: all256 byte-normal values "
                       "through current glNormal3b query, current and array GLSL120 gl_Normal "
                       "transform feedback, and fixed-function GL_COMPILE DrawArrays lists replayed "
                       "through the same shader after two different input changes. Nine written "
                       "components, seven preserved sentinel words and one actual TF triangle per "
                       "draw. Six separate fixed-function current-normal/RGBA/ColorMaterial state "
                       "receipts across compilation, poisoned-storage replay and repeat. "
                       "No game class execution, replacement, raster pixels or runtime qualification."}
    try:
        for label,tool in (("java",java),("javac",javac)):
            receipt = capture([str(tool),"-version"],args.output,label,10,environment)
            require_success(receipt)
            text = ((args.output / receipt["stdout_file"]).read_text() +
                    (args.output / receipt["stderr_file"]).read_text())
            ensure(re.search(r'(?:version "|javac )17\.',text), "JDK17 required")
        cp = os.pathsep.join(str(path.resolve()) for path in (classes,jar,*libraries))
        receipt = capture([str(javac),"-encoding","UTF-8","-cp",cp,"-d",str(classes),str(helper_source)],
                          args.output,"compile",30,environment)
        require_success(receipt,empty_stderr=True)
        helper = {path.name: digest(path) for path in classes.glob("*.class")}
        ensure(set(helper) == {"NormalListConversionReceiptObservation.class",
               "NormalListConversionReceiptObservation$1.class"}, "authored helper inventory")
        report["helper_before_sha256"] = helper
        receipt = capture([str(java),"-Xint","-Xverify:all","-Xmx256m","-Djava.security.manager=allow",
                           "-Dorg.lwjgl.librarypath=" + str(natives.resolve()),"-cp",cp,
                           "NormalListConversionReceiptObservation",str(natives.resolve())],
                          args.output,"original",90,environment,args.output.resolve())
        report["capture"] = receipt
        require_success(receipt)
        text = (args.output / receipt["stdout_file"]).read_text()
        marker = "Original normal list conversion: 256 values; 6 state receipts; cleanup-complete; network=0"
        ensure(text.splitlines().count(marker) == 1, "checked complete diagnostic and cleanup")
        links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
        ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(),
               "only declared genuine native link")
        headers = {}
        for prefix in ("NORMAL_LIST_DRIVER ","NORMAL_LIST_FORMAT ","NORMAL_LIST_LAYOUT ",
                       "NORMAL_LIST_CAPTURE ","NORMAL_LIST_STATE_LAYOUT ","NORMAL_LIST_STATE_STORAGE "):
            matches = [line for line in text.splitlines() if line.startswith(prefix)]
            ensure(len(matches) == 1, "single actual " + prefix + "receipt")
            headers[prefix.strip()] = matches[0]
        layout = ("normal_type=5120 normal_stride=32 normal_buffer=0 vertex_type=5126 "
                  "vertex_size=3 vertex_stride=32 vertex_buffer=0")
        ensure(headers["NORMAL_LIST_LAYOUT"] == "NORMAL_LIST_LAYOUT " + layout,
               "actual immediate and compiled array layout")
        ensure(headers["NORMAL_LIST_STATE_LAYOUT"] == "NORMAL_LIST_STATE_LAYOUT color_type=5121 "
               "color_size=4 color_stride=32 color_buffer=0 " + layout, "actual state-control arrays")
        ensure(headers["NORMAL_LIST_CAPTURE"] == "NORMAL_LIST_CAPTURE mode=4864 compile_program=0 "
               "draw_mode=4 vertices=3 feedback_components=9 sentinel_components=7 "
               "lighting=1 normalize=0 rescale=0", "actual list/feedback profile")
        ensure(re.fullmatch(r"NORMAL_LIST_FORMAT 8 8 8 8 [1-9][0-9]* [0-9]+",
                            headers["NORMAL_LIST_FORMAT"]), "actual RGBA8/depth context")
        storage = headers["NORMAL_LIST_STATE_STORAGE"].split()
        ensure(len(storage) == 3 and storage[1] != storage[2] and
               all(re.fullmatch(r"[0-9a-f]{64}",value) for value in storage[1:]),
               "distinct actual state-control source/poisoned bytes")
        observed = rows(text)
        report["headers"] = headers
        decoded = [line.split() for line in observed if line.startswith("NORMAL_LIST_DECODE ")]
        report["equal_value_counts"] = {
            "current_query_vs_current_feedback": sum(row[2].split(",") * 3 == row[3].split(",") for row in decoded),
            "current_feedback_vs_array_feedback": sum(row[3] == row[4] for row in decoded),
            "array_feedback_vs_list_feedback": sum(row[4] == row[5] for row in decoded),
            "current_feedback_vs_list_feedback": sum(row[3] == row[5] for row in decoded),
        }
        fixture = args.output / "normal-list-conversion-receipts.json"
        fixture.write_text(json.dumps({"headers":headers,"rows":observed},indent=2) + "\n",
                           encoding="utf-8",newline="\n")
        report.update(status="PASS",rows=len(observed),decode_rows=256,storage_rows=256,state_rows=6,
                      fixture_sha256=digest(fixture))
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.name: digest(path) for path in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or
                report["helper_before_sha256"] == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report,indent=2) + "\n",
                                                encoding="utf-8",newline="\n")
        ensure(same, "runtime/native/SDK/helper/input bytes unchanged")
    print("Legacy normal list conversion: 256 values and six state receipts; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
