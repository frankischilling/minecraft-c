"""Compare genuine local xi/cj construction, files, workers and the static codec."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile
from urllib.parse import urlparse
from urllib.request import url2pathname

from classfile import parse_class, validate_jar
from support import extract_resources, fingerprint, fresh_directory, require, run_bounded

HERE = Path(__file__).resolve().parent
JAVA_OPTIONS = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS")


def tool(value):
    path = shutil.which(value)
    require(path is not None, "missing executable: " + value)
    return Path(path).resolve(strict=True)


def bounded_json(path):
    require(path.is_file() and path.stat().st_size <= 2097152, "metadata extent")
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--native", required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--javac", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    require(sys.flags.optimize == 0, "Python assertions must remain enabled")
    require(not any(os.environ.get(name) for name in JAVA_OPTIONS), "ambient Java options must be unset")
    environment = os.environ.copy()
    for name in (*JAVA_OPTIONS, "CLASSPATH"):
        environment.pop(name, None)
    provenance = bounded_json(HERE / "provenance.json")
    require(provenance["schema"] == "CLIENT_STATS_COMPARISON_V1", "provenance schema")
    for name, pin in provenance["reference_files"].items():
        require(fingerprint(HERE / name) == pin, "reference pin: " + name)
    source = args.source_root.resolve(strict=True)
    for name, pin in provenance["native_sources"].items():
        require(fingerprint(source / name) == pin, "native source pin: " + name)
    java, javac, native = (tool(value) for value in (args.java, args.javac, args.native))
    require(java.parent == javac.parent, "java/javac must belong to one explicit JDK17")
    root = fresh_directory(args.work_dir, "stats-comparison-")
    report = {"run_directory": str(root), "native": {"path": str(native), **fingerprint(native)},
              "source_root": str(source), "tools": {}}
    for label, executable in (("java", java), ("javac", javac)):
        result = run_bounded([executable, "-version"], root, label + "-version", env=environment)
        version = (result.stdout + result.stderr).decode("utf-8")
        require(result.returncode == 0 and re.search(r'\b(?:version\s+")?17\.', version), "JDK17 tool version")
        report["tools"][label] = {"path": str(executable), **fingerprint(executable), "version": version}
    pins = bounded_json(HERE / "source-pins.json")
    jar = args.client_jar.resolve(strict=True)
    report["jar"] = validate_jar(jar, "client", pins)
    compiled = root / "helpers"
    compiled.mkdir()
    result = run_bounded([javac, "-source", "17", "-target", "17", "-g:none", "-encoding", "UTF-8",
                          "-classpath", compiled, "-d", compiled, HERE / "StatsFileObservation.java"],
                         root, "compile", env=environment)
    require(result.returncode == 0 and not result.stdout and not result.stderr, "authored helper compilation")
    require({p.name for p in compiled.iterdir()} == {"StatsFileObservation.class"}, "helper-only directory")
    require(parse_class((compiled / "StatsFileObservation.class").read_bytes()) == provenance["helper_class"], "helper class/Code pin")
    original = root / "original"
    original.mkdir()
    result = run_bounded([java, "-Xmx256m", "-Xverify:all", "-Dfile.encoding=UTF-8", "-Duser.language=en",
                          "-Duser.country=US", "-Xlog:class+init=info:file=class-init.log",
                          "-Xlog:class+load=info:file=class-load.log", "-classpath", compiled,
                          "StatsFileObservation", jar, original, original / "binding.tsv"],
                         original, "original", env=environment)
    require(result.returncode == 0 and not result.stderr, "original exit0/empty process stderr")
    original_lines = result.stdout.splitlines()
    require(original_lines[:2] == [b"16 achievements", b"151 recipes"], "actual original console banners")
    require(len(original_lines) == 24, "original projection extent")
    expected_binding = {value["name"]: value for value in pins["jars"]["client"]["classes"]}
    binding = (original / "binding.tsv").read_text(encoding="utf-8").splitlines()
    required_binding = ("xi", "cj", "vk", "vj", "hf", "hx", "gr", "jl", "ep", "vr", "ny")
    require(len(binding) == len(required_binding), "runtime binding extent")
    for line, name in zip(binding, required_binding):
        parts = line.split("\t")
        require(len(parts) == 3 and parts[:2] == [name, expected_binding[name]["class_sha256"]], "runtime class bytes: " + name)
        require(Path(url2pathname(urlparse(parts[2]).path)).resolve() == jar, "runtime JAR origin: " + name)
    initialized = set(re.findall(r"Initializing '([^']+)'", (original / "class-init.log").read_text(encoding="utf-8")))
    require(set(required_binding) <= initialized, "actual selected stats class initialization")
    exclusions = ("net/minecraft/client/Minecraft", "kq", "dc", "kp", "vu", "kh")
    require(not (set(exclusions) & initialized), "out-of-scope client/GUI/tick/network initialization")
    with zipfile.ZipFile(jar) as archive:
        game_names = {name[:-6].replace("/", ".") for name in archive.namelist() if name.endswith(".class")}
    loaded = {}
    for name, location in re.findall(r"\[class,load\] ([^ ]+) source: ([^\r\n]+)",
                                     (original / "class-load.log").read_text(encoding="utf-8")):
        if name in game_names:
            require(Path(url2pathname(urlparse(location).path)).resolve() == jar, "actual loaded game class origin: " + name)
            loaded[name] = location
    require(set(required_binding) <= set(loaded), "selected class-load evidence")
    caught = (original / "source-caught.stderr").read_bytes().splitlines()
    require(sum(line.startswith(b"xe:") for line in caught) == 3 and
            all(line.startswith((b"xe:", b"\tat ")) for line in caught), "three actual caught Argo syntax diagnostics")
    require(sum(b"at xi.a(" in line for line in caught) == 3, "source catch frames retained")
    provider = (original / "provider.tsv").read_bytes().splitlines()
    require(provider == [b"rename_existing\t" + (b"0" if os.name == "nt" else b"1"),
                         b"directory_reader\tjava.io.FileNotFoundException", b"delete_empty_directory\t1"],
            "selected actual JDK filesystem-provider profile")
    if os.name != "nt":
        expected = str(original / "ending\\" / "stats" / "stats_player123_unsent.dat").encode("utf-8")
        require((original / "posix-parent.tsv").read_bytes().splitlines() ==
                [b"expected\t" + expected, b"actual\t" + expected], "actual xi literal-backslash parent path")
    native_root = root / "native-game"
    native_root.mkdir()
    resources = root / "private-resources"
    extract_resources(jar, resources, pins["resources"])
    result = run_bounded([native, resources, native_root], root, "native", env=environment)
    require(result.returncode == 0 and not result.stderr, "native exit0/empty stderr; no sanitizer diagnostics")
    native_lines = result.stdout.splitlines()
    require(len(native_lines) == 26 and native_lines[:2] == [b"151 recipes", b"16 achievements"], "actual native console extent/banners")
    require(re.fullmatch(rb"Stats catalog: [1-9][0-9]* entries, [1-9][0-9]* JSON units", native_lines[9]) is not None, "normal catalog roundtrip")
    checks = re.fullmatch(rb"Client stats file: ([1-9][0-9]*) checks", native_lines[-1])
    require(checks is not None, "native checks trailer")
    # Retain stdout byte captures intact; compare only the finite authored rows.
    native_rows = native_lines[2:9] + native_lines[10:-1]
    require(native_rows == original_lines[2:], "native/original state, codec and checksum projections")
    require((native_root / "single.json").read_bytes() == (original / "single.json").read_bytes(), "single-entry serializer CRLF/GUID/checksum bytes")
    report.update(status="passed", checks=int(checks.group(1)), projection_rows=len(native_rows),
                  reader_errors=result.capture_report["reader_errors"],
                  pipe_capture_complete=result.capture_report["pipe_capture_complete"],
                  reaped=result.capture_report["reaped"],
                  runtime_bindings=len(binding), loaded_original_classes=len(loaded), source_caught_syntax_diagnostics=3,
                  process_stderr_bytes=0, original_exit=0, native_exit=0,
                  filesystem_provider_rows=len(provider),
                  posix_literal_backslash_parent_checked=os.name != "nt",
                  source_class_initialization_exclusions=list(exclusions))
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
