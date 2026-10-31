"""Fresh typed native20 versus unchanged original ge/nw CPU observations."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile
from urllib.parse import unquote, urlsplit
import zipfile
from capture import native, owner, source_inventory, write_json
from protocol import completed, cpu_rows, fingerprint, records, require

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def _files(directory):
    return {p.relative_to(directory).as_posix(): fingerprint(p)
            for p in sorted(directory.rglob("*")) if p.is_file()}


def activation(log, jar, names, helper_directory):
    loaded = {}
    initialized = set()
    helper_loaded = False
    for raw in records(log):
        line = raw.decode("ascii")
        require(re.match(r"^\[[0-9.]+s\]\[(?:info|debug) *\]\[class,(?:load|init)\]", line),
                "only requested class diagnostics on original stderr")
        load = re.search(r"\[class,load\]\s+(\S+) source: (.+)$", line)
        if load and load[1] == "GeNwCpuReference":
            url = urlsplit(load[2])
            require(url.scheme == "file" and not url.netloc and not url.query and not url.fragment,
                    "actual authored helper origin")
            path = unquote(url.path)
            if re.match(r"^/[A-Za-z]:/", path): path = path[1:]
            require(not helper_loaded and os.path.samefile(path, helper_directory), "actual compiled helper loader")
            helper_loaded = True
        if load and load[1].replace(".", "/") in names:
            name = load[1].replace(".", "/")
            url = urlsplit(load[2])
            require(url.scheme == "file" and not url.netloc and not url.query and not url.fragment,
                    "actual game class file origin")
            path = unquote(url.path)
            if re.match(r"^/[A-Za-z]:/", path): path = path[1:]
            require(os.path.samefile(path, jar) and name not in loaded, "actual original loader namespace")
            loaded[name] = load[2]
        init = re.search(r"Initializing '([^']+)'", line)
        if init: initialized.add(init[1])
    require(helper_loaded and {"ge", "nw"}.issubset(loaded) and {"ge", "nw"}.issubset(initialized),
            "actual original ge/nw initialization and origins")
    excluded = {"org/lwjgl/opengl/Display", "org/lwjgl/opengl/GLContext", "org/lwjgl/Sys",
                "org/lwjgl/input/Keyboard", "org/lwjgl/input/Mouse", "org/lwjgl/openal/AL"}
    require(not (excluded & initialized), "no native/device/input activation in CPU original")
    return {"game_classes": loaded, "initialized": sorted(initialized), "excluded": sorted(excluded)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("executable", "client-jar", "lwjgl", "java", "javac", "work-dir"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    source_before = source_inventory(ROOT)
    require(not any(os.environ.get(k) for k in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH")),
            "explicit JVM configuration only")
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH"):
        env.pop(key, None)
    java, javac = args.java.resolve(strict=True), args.javac.resolve(strict=True)
    require(java.parent == javac.parent, "same explicit JDK installation")
    home = java.parent.parent
    jvm = home / ("bin/server/jvm.dll" if os.name == "nt" else "lib/server/libjvm.so")
    inputs = [args.client_jar.resolve(strict=True), args.lwjgl.resolve(strict=True), java, javac,
              home / "release", home / "lib/modules", jvm,
              HERE / "GeNwCpuReference.java", HERE / "source-pins.json", HERE / "provenance.json",
              args.executable.resolve(strict=True)]
    before = {str(path): fingerprint(path) for path in inputs}
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    jar, lwjgl = inputs[:2]
    require(fingerprint(jar) == pins["client_jar"] and fingerprint(lwjgl) == pins["lwjgl"],
            "unchanged original JAR/library bytes")
    class_parser = ROOT / "tests/reference/client/notice/classfile.py"
    spec = importlib.util.spec_from_file_location("resource_original_classfile", class_parser)
    require(spec is not None and spec.loader is not None, "bound class/Code parser")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with zipfile.ZipFile(jar) as archive:
        jar_names = {name[:-6] for name in archive.namelist() if name.endswith(".class")}
        for name, pin in pins["classes"].items():
            require(module.inspect_class(archive.read(name + ".class")) == pin, "complete original Code: " + name)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="resource-original-", dir=args.work_dir.resolve()))
    native_dir = work / "native"
    native_dir.mkdir()
    native_result, native_report = native(args.executable, [], native_dir, ROOT, "cpu")
    closed_native = _files(native_dir)
    run = owner(ROOT)
    for executable, label in ((java, "java-version"), (javac, "javac-version")):
        version_command = [str(executable), "-version"]
        version = run(version_command, work, label, timeout=15, limit=65536, env=env)
        completed(version.capture_report, version.stdout, version.stderr, executable, empty_stderr=False)
        require(version.capture_report["command"] == version_command, "exact JDK probe command")
        require(re.search(rb"(?:openjdk|java|javac) (?:version )?\"?17\.", version.stdout + version.stderr),
                "matching explicit JDK17 probes")
    classes = work / "classes"
    classes.mkdir()
    compile_command = [str(javac), "--release", "17", "-encoding", "UTF-8", "-d", str(classes),
                       str(HERE / "GeNwCpuReference.java")]
    compile_result = run(compile_command, work, "compile", timeout=90, env=env)
    completed(compile_result.capture_report, compile_result.stdout, compile_result.stderr, javac)
    require(compile_result.capture_report["command"] == compile_command, "exact authored compile command")
    require(compile_result.stdout == b"", "silent helper compile")
    helper_before = _files(classes)
    require(set(helper_before) == {"GeNwCpuReference.class"}, "exact authored helper inventory")
    helper_code_before = module.inspect_class((classes / "GeNwCpuReference.class").read_bytes())
    command = [str(java), "-Xint", "-Xms32m", "-Xmx256m",
               "-Xlog:class+load=info,class+init=debug:stderr", "-cp", str(classes),
               "GeNwCpuReference", str(jar), str(lwjgl)]
    original = run(command, work, "original", timeout=90, env=env)
    completed(original.capture_report, original.stdout, original.stderr, java, empty_stderr=False)
    require(original.capture_report["command"] == command, "exact original invocation")
    original_rows = cpu_rows(original.stdout)
    require(original_rows == cpu_rows(native_result.stdout), "twenty fresh exact original/native records")
    observed = activation(original.stderr, jar, jar_names, classes)
    require(_files(native_dir) == closed_native, "closed native process receipts and streams unchanged")
    require(_files(classes) == helper_before, "compiled helper unchanged after observation")
    require(module.inspect_class((classes / "GeNwCpuReference.class").read_bytes()) == helper_code_before,
            "complete helper class/Code unchanged")
    require({str(path): fingerprint(path) for path in inputs} == before and source_inventory(ROOT) == source_before,
            "original/helper/JDK/native sources unchanged")
    products = {p.relative_to(work).as_posix(): fingerprint(p) for p in sorted(work.rglob("*")) if p.is_file()}
    report = {"status": "PASS", "native_compared": True, "rows": 20, "inputs_before": before,
              "inputs_after": before, "source_before": source_before, "source_after": source_before,
              "helper_before": helper_before, "helper_after": helper_before,
              "helper_Code_before": helper_code_before, "helper_Code_after": helper_code_before,
              "native": native_report, "original_capture": original.capture_report,
              "activation": observed, "products": products,
              "scope": "CPU ge/nw and private reset only; no GL draw or whole startup"}
    write_json(work / "report.json", report)
    print(work)


if __name__ == "__main__":
    main()
