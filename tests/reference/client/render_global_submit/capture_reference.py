"""Capture whole original N submission on a real device in seven fresh processes."""
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
from classfile import inspect_class
sys.path.insert(0, str(HERE.parent / "render_global_sky"))
from capture_prefix import JAR, LIBRARIES


def rows(text, name):
    found = [line for line in text.splitlines() if line.startswith(("SUBMIT ", "SUBMIT_TG "))]
    ensure(len(found) == 5, "complete submit/TG rows")
    main = found[0].split()
    ensure(len(main) == 17 and main[:2] == ["SUBMIT", name] and main[2] in ("OK", "NULL"), "submit row shape")
    ensure(main[4] == "0" and main[10] == "1,3", "all ten dirty polls and queue identities")
    ensure(re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){15}", main[12]), "complete modelview binary32 matrix")
    for i, line in enumerate(found[1:]):
        cells = line.split()
        ensure(len(cells) == 15 and cells[:3] == ["SUBMIT_TG", name, str(i)], "fixed four actual TG identities")
        ensure(all(re.fullmatch(r"[0-9a-f]{8}", v) for v in cells[8:11]) and
               re.fullmatch(r"[0-9a-f]{16}", cells[14]) and cells[13] == "65536", "TG floats/whole-buffer fold")
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "declared genuine LWJGL native profile is Windows")
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(p) or "").resolve(strict=True) for p in (args.java, args.javac)]
    ensure(java.parent.samefile(javac.parent), "matching JDK17 tools")
    jar = args.client_jar.resolve(strict=True)
    libraries = [args.library_root / name for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(p) == h for p, h in zip(libraries, LIBRARIES.values())), "original archive/SDK pins")
    pins = json.loads((HERE / "source-pins.json").read_text())
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = inspect_class(archive.read(name + ".class"))
            selected = {(m["name"], m["descriptor"]) for m in expected["methods"]}
            actual["methods"] = [m for m in actual["methods"] if (m["name"], m["descriptor"]) in selected]
            ensure(actual == expected, "unchanged class/Code " + name)
    native_archive = args.library_root / "org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar"
    ensure(digest(native_archive) == "68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43", "pinned native archive")
    natives = args.output / "natives"
    natives.mkdir()
    with zipfile.ZipFile(native_archive) as archive:
        (natives / "lwjgl64.dll").write_bytes(archive.read("lwjgl64.dll"))
    cases = json.loads((HERE / "inputs.json").read_text())["cases"]
    tsv = args.output / "inputs.tsv"
    tsv.write_text("\n".join("\t".join([c["name"], str(c["pass"]), ",".join(map(str, c["sorted"])),
                   str(int(c["w"])), str(int(c["cull_middle"])), str(int(c["empty_last"])), str(int(c["null_view"])),
                   str(int(c.get("actual_capability", False)))])
                   for c in cases) + "\n", encoding="utf-8", newline="\n")
    inputs = [java, javac, jar, *libraries, native_archive, natives / "lwjgl64.dll", tsv,
              HERE / "SubmitObservation.java", HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "capture_reference.py", SUPPORT / "capture.py", SUPPORT / "classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    classes = args.output / "classes"
    classes.mkdir()
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(name, None)
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "Seven whole unchanged N.a(ls,ID) calls on genuine RGBA8 GL: six ordinary and one16-entry occlusion profile using actual CX capability. Raw N/App/cameras, real TG/DK constructors with NULL World and declared pass-list geometry. Explicit Math before constructors. No N/App constructor, World, recenter/rebuild, issued-query result or full-frame claim."}
    try:
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], args.output, label, 10, environment)
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text() + (args.output / receipt["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), "JDK17 required")
        cp = os.pathsep.join(str(p.resolve()) for p in [classes, jar, *libraries])
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes), str(HERE / "SubmitObservation.java")],
                          args.output, "compile", 30, environment)
        require_success(receipt, empty_stderr=True)
        helper = {p.name: digest(p) for p in classes.glob("*.class")}
        ensure(set(helper) == {"SubmitObservation.class", "SubmitObservation$1.class"}, "authored helper inventory")
        report["helper_before_sha256"] = helper
        observed = []
        for i, case in enumerate(cases):
            receipt = capture([str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.security.manager=allow",
                               "-Dorg.lwjgl.librarypath=" + str(natives.resolve()), "-cp", cp, "SubmitObservation", str(jar),
                               str(natives.resolve()), str(tsv.resolve()), str(i)], args.output, "original" + str(i), 60,
                              environment, args.output.resolve())
            report["profiles"].append({"name": case["name"], "capture": receipt})
            require_success(receipt)
            text = (args.output / receipt["stdout_file"]).read_text()
            ensure("Original submit: " + case["name"] + "; actual GL; network=0" in text.splitlines(), "source normal helper completion")
            links = [line.removeprefix("NATIVE_LINK ") for line in text.splitlines() if line.startswith("NATIVE_LINK ")]
            ensure(len(links) == 1 and Path(links[0]).resolve() == (natives / "lwjgl64.dll").resolve(), "only declared genuine native link")
            report["profiles"][-1]["driver"] = [line for line in text.splitlines() if line.startswith("SUBMIT_DRIVER ")]
            report["profiles"][-1]["format"] = [line for line in text.splitlines() if line.startswith("SUBMIT_FORMAT ")]
            capability = [line for line in text.splitlines() if line.startswith("SUBMIT_CAPABILITY ")]
            ensure(capability == (["SUBMIT_CAPABILITY " + case["name"] + " 1"] if case.get("actual_capability") else []),
                   "actual supported-context capability receipt")
            report["profiles"][-1]["capability"] = capability
            observed += rows(text, case["name"])
        ensure(len(observed) == 35, "seven complete device profiles")
        (args.output / "finite.json").write_text(json.dumps({"rows": observed}, indent=2) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=35, fixture_sha256=digest(args.output / "finite.json"))
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["helper_after_sha256"] = {p.name: digest(p) for p in classes.glob("*.class")}
        same = before == report["input_after_sha256"] and (
            "helper_before_sha256" not in report or helper == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (args.output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "game/native/runtime/helper/input bytes unchanged")
    print("Original N submit: 35 rows; " + str(args.output / "capture.json"))


if __name__ == "__main__":
    main()
