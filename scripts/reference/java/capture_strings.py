#!/usr/bin/env python3
"""Capture synthetic Formatter or English uppercase observations on pinned Windows JDK17."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil

import capture_nanotime_conversion as runner

ROOT = Path(__file__).resolve().parents[3]
HELPERS = {
    "format": ("FormatObservation", "1eefa038dd0de0b445e27d97963027fc5c07c884af66fb9447ff96b19f5d8724"),
    "case": ("StringCaseObservation", "d65ecf319d6416d878d474e9d90b385a2ab15c58bb5e9d74b835343372c57c27"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=HELPERS)
    parser.add_argument("--java", required=True, type=Path)
    parser.add_argument("--javac", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    java, javac = args.java.resolve(strict=True), args.javac.resolve(strict=True)
    runner.require(java.parent == javac.parent, "one JDK installation")
    jdk = java.parent.parent
    name, helper_hash = HELPERS[args.kind]
    helper = ROOT / ("tests/reference/java/" + name + ".java")
    inputs = {"java": java, "javac": javac, "modules": jdk / "lib/modules",
              "jvm": jdk / "bin/server/jvm.dll", "java_native": jdk / "bin/java.dll",
              "source_archive": jdk / "lib/src.zip", "release": jdk / "release",
              "helper": helper, "recorder": Path(__file__).resolve(),
              "runner": Path(runner.__file__).resolve()}
    for path in sorted((jdk / "bin").rglob("*.dll")):
        inputs["runtime/" + path.relative_to(jdk).as_posix()] = path
    before = {key: {"path": str(path), **runner.fingerprint(path)} for key, path in inputs.items()}
    runner.require(all(before[key]["sha256"] == value for key, value in runner.PROFILE.items()),
                   "pinned Windows JDK17 profile")
    runner.require(before["helper"]["sha256"] == helper_hash, "reviewed helper")
    output = args.output_dir.resolve()
    output.mkdir(parents=False, exist_ok=False)
    classes = output / "classes"
    classes.mkdir()
    for key in ("helper", "recorder", "runner"):
        shutil.copy2(inputs[key], output / inputs[key].name)
    children = []
    report = {"schema": "JAVA_STRING_CAPTURE_V1", "kind": args.kind,
              "status": "failed", "inputs_before": before, "children": children}
    try:
        runner.require(not runner.run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, helper],
                                      output, "compile", children), "compiler stdout")
        traces = []
        for mode, flags in (("interpreted", ["-Xint"]),
                            ("tier1", ["-XX:TieredStopAtLevel=1"]), ("default", [])):
            traces.append(runner.run([java, *flags, "-Xmx128m", "-cp", classes, name],
                                     output, mode, children))
        runner.require(all(raw == traces[0] for raw in traces), "mode outputs differ")
        fixture = ROOT / ("tests/reference/java/string-" + args.kind + "/observations.tsv")
        runner.require(traces[0] == fixture.read_bytes(), "fresh observation differs from retained fixture")
        report.update(status="captured", three_modes_equal=True,
                      observation={"bytes": len(traces[0]), "sha256": hashlib.sha256(traces[0]).hexdigest()})
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        report["inputs_after"] = {key: {"path": str(path), **runner.fingerprint(path)}
                                  for key, path in inputs.items()}
        report["inputs_unchanged"] = before == report["inputs_after"]
        if not report["inputs_unchanged"]:
            report["status"] = "failed"
        report["artifacts"] = {path.relative_to(output).as_posix(): runner.fingerprint(path)
                               for path in sorted(output.rglob("*")) if path.is_file()}
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    runner.require(report["status"] == "captured", "capture did not qualify")
    print(json.dumps({key: report[key] for key in ("kind", "status", "three_modes_equal", "inputs_unchanged", "observation")}))


if __name__ == "__main__":
    main()
