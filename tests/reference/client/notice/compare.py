"""Fresh genuine achievement-notice constructors and notification comparison."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import zipfile
sys.dont_write_bytecode = True
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

HERE = Path(__file__).resolve().parent
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5"}
NATIVE = {"lwjgl64.dll": "86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7"}


def records(path, mode):
    lines = path.read_text(encoding="utf-8").splitlines()
    rows = [x for x in lines if re.match(r"^(MODEL|BIPED|PART|VERTEX|QUAD|BLOCKS|STATE|OWNERSHIP|LIFETIME) ", x)]
    ensure(lines and lines[-1] == "LIFETIME clean", "missing completed lifetime marker")
    ensure(len(rows) == (140 if mode == "geometry" else 151), "record cardinality mismatch")
    expected = {"MODEL":1,"BIPED":1,"PART":9,"VERTEX":72,"QUAD":54,"BLOCKS":2,
                "STATE":0 if mode=="geometry" else 10,"LIFETIME":1,"OWNERSHIP":0 if mode=="geometry" else 1}
    ensure({tag:sum(row.startswith(tag+" ") for row in rows) for tag in expected}==expected,
           "record family inventory differs")
    return rows


def diagnostics(path):
    return [line for line in path.read_text(encoding="utf-8").splitlines()
            if line and not re.match(r"^(ORIGIN|MODEL|BIPED|PART|VERTEX|QUAD|BLOCKS|STATE|OWNERSHIP|LIFETIME) ", line)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("client-jar", "libraries", "natives", "java", "javac", "native-probe", "resources", "work-dir"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--native-build-manifest", type=Path)
    a = p.parse_args()
    if os.name != "nt":
        print("Original notice profile currently qualifies Windows x64 only", file=sys.stderr)
        return 77
    files = [a.client_jar, a.java, a.javac, a.native_probe]
    files += [a.libraries / n for n in LIBRARIES] + [a.natives / n for n in NATIVE]
    files += [a.resources / n for n in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt")]
    if any(not x.is_file() for x in files):
        print("Missing explicitly supplied private input", file=sys.stderr)
        return 77
    ensure(a.java.resolve().parent == a.javac.resolve().parent, "matching explicit JDK bin required")
    ensure(HERE not in a.work_dir.resolve().parents and a.work_dir.resolve() != HERE, "private output must be outside source package")
    a.work_dir.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="notice-", dir=a.work_dir)).resolve()
    inputs = {str(x.resolve()): digest(x) for x in files}
    sources = {x.name: digest(x) for x in HERE.iterdir() if x.is_file()}
    result = {"status": "RUNNING", "inputs": inputs, "sources": sources, "profiles": {}}
    try:
        pins = json.loads((HERE / "source-pins.json").read_text())
        ensure(digest(a.client_jar) == pins["client_jar_sha256"], "client JAR pin")
        for name, wanted in LIBRARIES.items():
            ensure(digest(a.libraries / name) == wanted, "library pin: " + name)
        for name, wanted in NATIVE.items():
            ensure(digest(a.natives / name) == wanted, "native-library pin: " + name)
        with zipfile.ZipFile(a.client_jar) as jar:
            for name, wanted in pins["classes"].items():
                ensure(inspect_class(jar.read(name + ".class")) == wanted, "original class/Code pin: " + name)
            resource_pins = {}
            for name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"):
                data = jar.read(name)
                ensure((a.resources / name).read_bytes() == data, "native/original resource differs: " + name)
                resource_pins[name] = digest(a.resources / name)
        result["native_resource_inputs"] = resource_pins
        env = os.environ.copy()
        for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
            env.pop(key, None)
        result["tools"] = {}
        for name, executable in (("java", a.java), ("javac", a.javac)):
            report = capture([str(executable.resolve()), "-version"], run, name + "-version", 10, env)
            require_success(report)
            version = (run / report["stdout_file"]).read_text() + (run / report["stderr_file"]).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), "explicit JDK17 required")
            result["tools"][name] = {"capture": report, "version": version}
        classes = run / "classes"
        classes.mkdir()
        classpath = os.pathsep.join(str(x.resolve()) for x in [classes, a.client_jar] + [a.libraries / n for n in LIBRARIES])
        compile_report = capture([str(a.javac.resolve()), "--release", "17", "-encoding", "UTF-8", "-classpath", classpath,
                                  "-d", str(classes), str(HERE / "NoticeObservation.java")], run, "compile", 30, env)
        require_success(compile_report, empty_stderr=True)
        helpers = {str(x.relative_to(classes)): digest(x) for x in classes.rglob("*.class")}
        ensure(set(helpers) == {"NoticeObservation.class"}, "unexpected compiled helper class")
        result["helpers_before"] = helpers
        for mode in ("geometry", "callbacks"):
            java = [str(a.java.resolve()), "-Xint", "-Xverify:all", "-Xmx512m",
                    "--add-opens=java.base/java.util=ALL-UNNAMED",
                    "-Dorg.lwjgl.librarypath=" + str(a.natives.resolve()), "-classpath", classpath,
                    "NoticeObservation", mode]
            native = [str(a.native_probe.resolve()), mode, str(a.resources.resolve())]
            original_report = capture(java, run, "original-" + mode, 60, env)
            native_report = capture(native, run, "native-" + mode, 60, env)
            require_success(original_report, empty_stderr=True)
            require_success(native_report, empty_stderr=True)
            original_rows = records(run / original_report["stdout_file"], mode)
            native_rows = records(run / native_report["stdout_file"], mode)
            ensure(diagnostics(run / original_report["stdout_file"]) ==
                   diagnostics(run / native_report["stdout_file"]), "source diagnostics differ")
            ensure(original_rows == native_rows, next((f"record {i}: {x!r} != {y!r}" for i, (x, y) in
                   enumerate(zip(original_rows, native_rows)) if x != y), "different record inventories"))
            result["profiles"][mode] = {"records": len(original_rows), "exact_equal": True,
                                        "original": original_report, "native": native_report}
        ensure(inputs == {str(x.resolve()): digest(x) for x in files}, "binary inputs changed")
        ensure(resource_pins == {name:digest(a.resources/name) for name in resource_pins}, "native resource inputs changed")
        ensure(sources == {x.name: digest(x) for x in HERE.iterdir() if x.is_file()}, "reference sources/pins changed")
        ensure(helpers == {str(x.relative_to(classes)): digest(x) for x in classes.rglob("*.class")}, "helper classes changed")
        if a.native_build_manifest:
            result["native_build_manifest"] = {"sha256": digest(a.native_build_manifest),
                "record": json.loads(a.native_build_manifest.read_text())}
        result["status"] = "PASS"
    except Exception as error:
        result.update(status="FAIL", error=f"{type(error).__name__}: {error}")
    (run / "result.json").write_bytes((json.dumps(result, indent=2) + "\n").encode())
    print(json.dumps({"status": result["status"], "result": str(run / "result.json"),
                      "records": {k: v["records"] for k, v in result["profiles"].items()},
                      **({"error": result["error"]} if "error" in result else {})}))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
