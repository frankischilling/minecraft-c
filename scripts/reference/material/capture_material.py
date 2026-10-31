#!/usr/bin/env python3
"""Capture the pinned originals' materials without running a native producer."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROCESS_HELPER = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROCESS_SHA256 = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
HELPER = ROOT / "tests/reference/material/MaterialObservation.java"
HELPER_SHA256 = "8ea23d95d423181f975e0f2858ce2d30afe082b46d63a894ac40f9687b39ee71"
JARS = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    options = parser.parse_args()
    if hashlib.sha256(PROCESS_HELPER.read_bytes()).hexdigest() != PROCESS_SHA256:
        raise ValueError("reviewed process helper required")
    specification = importlib.util.spec_from_file_location("material_capture_process", PROCESS_HELPER)
    process = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(process)
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    process.require(java.parent == javac.parent, "one JDK required")
    jdk = java.parent.parent
    inputs = {
        "java": java, "javac": javac, "helper": HELPER,
        "recorder": Path(__file__).resolve(), "process_helper": PROCESS_HELPER,
        "modules": jdk / "lib/modules", "jvm": jdk / "bin/server/jvm.dll",
        "java_native": jdk / "bin/java.dll", "release": jdk / "release",
        "source_archive": jdk / "lib/src.zip",
        "client": options.client_jar.resolve(strict=True),
        "server": options.server_jar.resolve(strict=True),
    }
    before = {key: process.fingerprint(path) for key, path in inputs.items()}
    process.require(before["helper"]["sha256"] == HELPER_SHA256, "reviewed observer required")
    process.require(all(before[key]["sha256"] == value for key, value in JARS.items()),
                    "pinned original JARs required")
    process.require(all(before[key]["sha256"] == value for key, value in process.PROFILE.items()),
                    "retained Windows JDK profile required")
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    classes = directory / "classes"
    classes.mkdir()
    receipts = []
    report = {"status": "failed", "scope": "Original material/palette receiver observations",
              "inputs_before": before, "children": receipts}
    try:
        process.run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, HELPER],
                    directory, "compile", receipts)
        outputs = {}
        for side in JARS:
            raw = process.run([java, "-Xint", "-Xmx256m", "-cp", classes,
                               "MaterialObservation", side, inputs[side]],
                              directory, side, receipts)
            process.require(raw.startswith(b"MATERIAL_V1\n")
                            and raw.endswith(b"END\t28\t14\t16\n"), "observation envelope")
            process.require(b"\r" not in raw and raw.count(b"\n") == 116,
                            "complete literal LF corpus")
            outputs[side] = raw
        process.require(outputs["client"] == outputs["server"], "original sides differ")
        report["compiled_helper"] = process.fingerprint(classes / "MaterialObservation.class")
        report["fixture"] = process.fingerprint(directory / "client.stdout")
        report["status"] = "passed"
    finally:
        # Preserve child receipts even if a later fingerprint/read rejects.
        try:
            report["inputs_after"] = {key: process.fingerprint(path) for key, path in inputs.items()}
            if report["inputs_after"] != before:
                report["status"] = "failed"
                raise ValueError("capture inputs changed")
        except BaseException:
            report["status"] = "failed"
            raise
        finally:
            with (directory / "capture.json").open("x", encoding="utf-8", newline="\n") as stream:
                json.dump(report, stream, indent=2, sort_keys=True)
                stream.write("\n")
    print("Captured 116 matching original rows; native code supplied no expected values.")


if __name__ == "__main__":
    main()
