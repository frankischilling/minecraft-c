"""Bounded native CTest driver and unchanged-client weather-body comparison."""
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
ORIGINS = ("fd","ei","xa","rh","wd","ay","mx","fm","wt")


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
    records=[line for line in text.splitlines() if line.startswith(("INIT ","WEATHER "))]
    index=0
    for scene in range(4):
        need(index<len(records),"missing constructor record")
        fields=records[index].split()
        need(fields[:2]==["INIT",str(scene)] and len(fields)==8,"constructor inventory/width")
        index+=1
    for scene in range(64):
        need(index<len(records),"missing weather record")
        fields=records[index].split()
        need(fields[:3]==["WEATHER",str(scene),"0" if 39<=scene<=44 else "1"] and len(fields)==17,"weather inventory/width")
        index+=1
    need(index==len(records)==68,"extra/trailing records")
    return records


def native_count(text):
    lines = text.splitlines()
    need(bool(lines), "empty native output")
    match = re.fullmatch(r"World weather: ([1-9][0-9]*) checks", lines[-1])
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
    support = module(support_path, "weather_capture")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="weather-", dir=args.work_dir.resolve()))
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(name, None)
    inputs = [args.native.resolve(), Path(__file__).resolve(), support_path]
    report = {"status": "FAIL", "scope": "actual client fd.m weather only, with unchanged original owners; no aggregate World tick or server execution claim"}
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
            metadata = module(classfile_path, "weather_classfile")
            pins_path = HERE / "source-pins.json"
            pins = json.loads(pins_path.read_text())
            with zipfile.ZipFile(jar) as archive:
                for name, expected in pins["classes"].items():
                    need(metadata.inspect_class(archive.read(name + ".class")) == expected,
                         "whole class/Code metadata differs: " + name)
            inputs += [java, javac, jar, *libraries, classfile_path, pins_path,
                       HERE / "WeatherObservation.java"]
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
                 str(HERE / "WeatherObservation.java")], "compile", timeout=30)
            helper_before = {p.name: digest(p) for p in classes.glob("*.class")}
            need(set(helper_before) == {"WeatherObservation.class"},
                 "unexpected compiled helper inventory")
            report["helper_before_sha256"] = helper_before
            report["helper_class_code"] = {p.name: metadata.inspect_class(p.read_bytes()) for p in classes.glob("*.class")}
            original = run([str(java), "--add-opens", "java.base/java.util=ALL-UNNAMED", "-Xint", "-Xverify:all",
                            "-classpath", cp, "WeatherObservation", str(output / "original-worlds"), str(jar)], "original")
            left = original.stdout.decode("ascii").splitlines()
            need(left[-1] == "Original World weather: 64 states and 4 constructor states", "original completion marker missing")
            origins = [line.split(" ", 2) for line in left if line.startswith("ORIGIN ")]
            need([line[1] for line in origins] == list(ORIGINS), "original origin inventory changed")
            need(all(Path(line[2]).resolve().samefile(jar) for line in origins), "foreign actual game class")
            expected = projection(original.stdout.decode("ascii"))
            report["original"] = original.capture_report
            report["jdk"] = versions
        native = run([str(args.native.resolve()), "world-weather-reference" if args.client_jar else "world-weather",
                      str(output / "native-worlds").encode().hex()], "native")
        count = native_count(native.stdout.decode("ascii"))
        report["native"] = native.capture_report
        if args.client_jar is not None:
            actual = projection(native.stdout.decode("ascii"))
            need(actual == expected, {"first_difference": next(((i, a, b) for i, (a, b) in enumerate(zip(expected, actual)) if a != b), None)})
            report.update(weather_states=64,constructor_states=4,completed_weather_calls=58,interrupted_prefixes=6,records=68)

        report.update(status="PASS", native_checks=count, reference=args.client_jar is not None)
    except BaseException as exc:
        report["error"] = {"type": type(exc).__name__, "message": str(exc)}
        raise
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
    print(f"World weather qualification: {report['native_checks']} checks; report {output / 'report.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
