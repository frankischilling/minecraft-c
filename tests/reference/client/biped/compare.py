"""Fresh unchanged-original normal-zero Biped CPU comparison on Windows or Linux."""
import argparse
import json
import os
from pathlib import Path
import re
import tempfile
from urllib.parse import unquote, urlparse
import zipfile
from support import (HERE, capture, classfile, fingerprint, protocol, read_native,
                     require, source_binding, streams)

LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5"}


def origin_path(text):
    parsed = urlparse(text)
    require(parsed.scheme == "file" and parsed.netloc in ["", "localhost"], "actual file origin")
    path = unquote(parsed.path)
    if re.match(r"^/[A-Za-z]:/", path):
        path = path[1:]
    return Path(path).resolve(strict=True)


def bindings(log, owners, helper_directory):
    observed = {}
    for name, origin in re.findall(r"\[class,load\] ([^ ]+) source: ([^\r\n]+)", log):
        if name == "BipedObservation":
            require(origin_path(origin) == helper_directory, "actual helper class origin")
            observed[name] = origin
        elif name in owners:
            require(origin_path(origin) in owners[name], "actual game/SDK class JAR origin: " + name)
            observed[name] = origin
    require("BipedObservation" in observed and "fh" in observed, "actual helper/game load inventory")
    return observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["client-jar", "libraries", "java", "javac", "native-probe", "work-dir"]:
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    require(args.java.resolve(strict=True).parent == args.javac.resolve(strict=True).parent,
            "matching explicitly supplied JDK17 tools")
    require(not args.work_dir.resolve().is_relative_to(HERE.parents[2]), "capture outside source tests")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="biped-original-", dir=args.work_dir.resolve()))
    proof = source_binding()
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    jar = args.client_jar.resolve(strict=True)
    native = args.native_probe.resolve(strict=True)
    libraries = [args.libraries.resolve(strict=True) / name for name in LIBRARIES]
    require(fingerprint(jar)["sha256"] == pins["client_jar_sha256"], "whole supplied original JAR")
    for path in libraries:
        require(fingerprint(path)["sha256"] == LIBRARIES[path.name], "pinned original SDK archive: " + path.name)
    owners = {}
    for archive in [jar, *libraries]:
        with zipfile.ZipFile(archive) as source:
            for name in source.namelist():
                if name.endswith(".class"):
                    owners.setdefault(name[:-6].replace("/", "."), set()).add(archive)
            if archive == jar:
                for name, wanted in pins["classes"].items():
                    require(classfile.inspect_class(source.read(name + ".class")) == wanted,
                            "complete original class/Code: " + name)
    inputs = [jar, native, args.java.resolve(strict=True), args.javac.resolve(strict=True), *libraries]
    before = {str(path): fingerprint(path) for path in inputs}
    env = os.environ.copy()
    for key in ["JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH"]:
        env.pop(key, None)
    report = {"status": "RUNNING", "scope": "normal zero-profile unchanged-original CPU constructors only",
              "host": os.name, "inputs_before": before, "source_binding": proof,
              "original_class_Code_pins": pins, "captures": {}, "profiles": {}}

    def run(command, stem, timeout=90, empty_stderr=True):
        process = capture(command, out, stem, timeout, env)
        report["captures"][stem] = process
        return process, streams(out, process, empty_stderr)

    try:
        for name, tool in [("java", args.java), ("javac", args.javac)]:
            process, data = run([tool.resolve(), "-version"], name + "-version", empty_stderr=False)
            require(re.search(rb'(?:version "|javac )17\.', data[0] + data[1]), "actual explicit JDK17")
        classes = out / "classes"
        classes.mkdir()
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        run([args.javac.resolve(), "--release", "17", "-encoding", "UTF-8", "-classpath", cp,
             "-d", classes, HERE / "BipedObservation.java"], "compile")
        helper = {path.relative_to(classes).as_posix(): fingerprint(path)
                  for path in classes.rglob("*") if path.is_file()}
        require(set(helper) == {"BipedObservation.class"}, "helper-only class directory; no replacement game classes")
        report["helpers_before"] = helper
        seen = set()
        for mode in ["cold", "pair", "pool", "notice"]:
            command = [args.java.resolve(), "-Xint", "-Xverify:all", "-Xmx512m",
                       "-Xlog:class+load=info,class+init=info:stderr",
                       "--add-opens=java.base/java.util=ALL-UNNAMED", "-Djava.awt.headless=true",
                       "-classpath", cp, "BipedObservation", mode]
            actual, data = run(command, "original-" + mode, empty_stderr=False)
            original_rows = protocol.parse(data[0], mode)
            log = data[1].decode("utf-8")
            origins = bindings(log, owners, classes.resolve())
            seen.update(origins)
            for name in ["ge", "nw", "org/lwjgl/opengl/GL11", "org/lwjgl/opengl/Display",
                         "org/lwjgl/opengl/GLContext", "org/lwjgl/input/Mouse",
                         "org/lwjgl/input/Keyboard", "org/lwjgl/Sys", "org/lwjgl/openal/AL"]:
                require("Initializing '" + name + "'" not in log, "no rendering/device active use: " + name)
            require("Initializing 'fh'" in log and "Initializing 'bt'" in log, "genuine constructor activation")
            process = capture([native, mode], out, "native-" + mode, 90, env)
            report["captures"]["native-" + mode] = process
            native_rows = read_native(out, process, mode)
            require(original_rows == native_rows, "fresh actual/native complete rows: " + mode)
            report["profiles"][mode] = {"records": len(original_rows), "exact_equal": True,
                                        "actual_class_origins": origins}
        require(set(pins["classes"]).issubset(seen), "all pinned constructor families observed from actual archives")
        require(sum(profile["records"] for profile in report["profiles"].values()) == 838,
                "all838 fresh original constructor records")
        helpers_after = {path.relative_to(classes).as_posix(): fingerprint(path)
                         for path in classes.rglob("*") if path.is_file()}
        after = {str(path): fingerprint(path) for path in inputs}
        require(after == before and helpers_after == helper and source_binding() == proof,
                "all input/native/helper/source readback")
        report.update(status="PASS", inputs_after=after, helpers_after=helpers_after)
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "work_directory": str(out)}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
