#!/usr/bin/env python3
"""Record Java scalar clock-conversion results as native test vectors.

This observes Java arithmetic, not System.nanoTime or an original game.
No native executable supplies expected values.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "tests/reference/java/NanotimeConversionObservation.java"
HELPER_SHA256 = "decd72436969714b9fafd79fb6d214fc873e76390521c22a0a5cda422ffd2a05"
MODULES = "2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8"
JVM = "5539382b75556a9873fafa2d8b7d0b36b169ac436a3608467da4f74c2600cbce"
PROFILE = {
    "modules": MODULES, "jvm": JVM,
    "java": "974fd93b1279f87db5c86e15469dc5f76e708c3a341847523170c532118c8841",
    "javac": "cb8056d3878c2bd252e0f6316e009cdf8a140ef7cb38878c62b8aa2c75fbd5f3",
    "java_native": "18b105254c16e1d83614ff4e32cd62ce402fbe6339bdd1b24ccc7d999fed7f5f",
    "release": "fdf03ea4c142f886ff60e4f2fb44b1a9f8c47aa9784c8d34fc7f8cbada48cf44",
    "source_archive": "5d0afb30b29d1eff95502604e9415d361f0f503553f32416320c58c1f0619e1e",
}
HEX = re.compile(r"[0-9a-f]{16}\Z")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(block)
            digest.update(block)
    return {"bytes": size, "sha256": digest.hexdigest()}


def run(command, directory, name, receipts):
    environment = os.environ.copy()
    for variable in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS",
                     "_JAVA_OPTIONS", "CLASSPATH"):
        environment.pop(variable, None)
    command = [str(part) for part in command]
    stdout_path, stderr_path = directory / (name + ".stdout"), directory / (name + ".stderr")
    child = None
    failure = None
    started = time.monotonic()
    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
        try:
            child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stdout,
                                     stderr=stderr, env=environment)
            receipt = {"command": command, "pid": child.pid, "exit_code": None, "reaped": False}
            receipts.append(receipt)
            while child.poll() is None:
                if time.monotonic() - started >= 60:
                    failure = "child deadline exceeded"
                if os.fstat(stdout.fileno()).st_size > 4 * 1024 * 1024 or \
                        os.fstat(stderr.fileno()).st_size > 1024 * 1024:
                    failure = "child output threshold exceeded"
                if failure:
                    break
                try:
                    child.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    pass
        finally:
            if child is not None:
                try:
                    if child.poll() is None:
                        child.kill()
                except OSError as error:
                    failure = f"owned child termination failed: {error}"
                finally:
                    # A failed termination request must not skip the wait.
                    try:
                        child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        failure = "owned child did not reap within cleanup interval"
                    except OSError as error:
                        failure = f"owned child wait failed: {error}"
                receipt.update(exit_code=child.returncode, reaped=child.returncode is not None,
                               seconds=time.monotonic() - started)
                if failure:
                    receipt["failure"] = failure
                try:
                    receipt.update(stdout=fingerprint(stdout_path), stderr=fingerprint(stderr_path))
                except OSError as error:
                    receipt["journal_error"] = str(error)
                    raise
    require(failure is None, failure)
    require(child is not None and child.returncode == 0, "child failed")
    require(stderr_path.stat().st_size == 0, "unexpected child stderr")
    require(stdout_path.stat().st_size <= 4 * 1024 * 1024, "final output size")
    return stdout_path.read_bytes()


def parse(raw):
    require(raw.endswith(b"\n") and b"\r" not in raw, "literal LF output required")
    lines = raw[:-1].decode("ascii").split("\n")
    require(lines[0] == "NANO_CONVERSION_V1", "conversion header")
    require(3 <= len(lines) <= 20002, "conversion row bound")
    rows = {"C": [], "S": []}
    for line in lines[1:-1]:
        fields = line.split("\t")
        require(len(fields) == 5, "conversion field count")
        kind, first, second, status, expected = fields
        require(kind in rows, "conversion row kind")
        require(all(HEX.fullmatch(value) for value in (first, second, expected)), "long bits")
        require(status in ("OK", "RANGE"), "conversion status")
        require(status == "OK" or expected == "0000000000000000", "range placeholder")
        rows[kind].append((first, second, status, expected))
    require(rows["C"] and rows["S"], "both conversions required")
    require(lines[-1] == f"END\t{len(rows['C'])}\t{len(rows['S'])}", "conversion footer")
    require(len(rows["C"]) == 2608 and len(rows["S"]) == 2258, "complete helper corpus")
    return rows


def literal(bits):
    value = int(bits, 16)
    if value >= 1 << 63:
        value -= 1 << 64
    if value == -(1 << 63):
        return "INT64_MIN"
    return f"-INT64_C({-value})" if value < 0 else f"INT64_C({value})"


def header(rows):
    lines = ["#ifndef MC_REFERENCE_NANOTIME_CONVERSION_FIXTURES_H",
             "#define MC_REFERENCE_NANOTIME_CONVERSION_FIXTURES_H", "",
             "/* Captured Java scalar arithmetic; not System.nanoTime observations. */",
             "typedef struct mc_nanotime_conversion_vector {", "    int64_t first;",
             "    int64_t second;", "    mc_platform_nanotime_status status;",
             "    int64_t expected;", "} mc_nanotime_conversion_vector;", ""]
    for kind, name in (("C", "counter_vectors"), ("S", "timespec_vectors")):
        lines.append(f"static const mc_nanotime_conversion_vector {name}[] = {{")
        for first, second, status, expected in rows[kind]:
            lines.append("    {" + ", ".join((literal(first), literal(second),
                         "MC_PLATFORM_NANOTIME_" + status, literal(expected))) + "},")
        lines.extend(("};", ""))
    return ("\n".join(lines + ["#endif", ""])).encode("ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    options = parser.parse_args()
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one JDK required")
    jdk = java.parent.parent
    inputs = {"java": java, "javac": javac, "helper": HELPER,
              "recorder": Path(__file__).resolve(), "modules": jdk / "lib/modules",
              "jvm": jdk / "bin/server/jvm.dll", "java_native": jdk / "bin/java.dll",
              "release": jdk / "release", "source_archive": jdk / "lib/src.zip"}
    before = {key: fingerprint(path) for key, path in inputs.items()}
    require(before["helper"]["sha256"] == HELPER_SHA256, "reviewed helper required")
    require(all(before[name]["sha256"] == value for name, value in PROFILE.items()),
            "retained Windows JDK profile required")
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    classes = directory / "classes"
    classes.mkdir()
    receipts = []
    report = {"scope": "Java scalar formulas and explicit native range policy",
              "inputs_before": before, "children": receipts, "status": "failed"}
    try:
        compiled = run([javac, "-J-Xmx256m", "-encoding", "UTF-8", "-d", classes, HELPER],
                       directory, "compile", receipts)
        require(not compiled, "unexpected compiler stdout")
        outputs = []
        for name, flags in (("interpreted", ["-Xint"]),
                            ("tier1", ["-XX:TieredStopAtLevel=1"]), ("default", [])):
            outputs.append(run([java, *flags, "-Xmx128m", "-cp", classes,
                                "NanotimeConversionObservation"], directory, name, receipts))
        rows = parse(outputs[0])
        require(all(output == outputs[0] for output in outputs), "mode outputs differ")
        for output in outputs[1:]:
            parse(output)
        after = {key: fingerprint(path) for key, path in inputs.items()}
        require(before == after, "capture inputs changed")
        fixture = directory / "nanotime_conversion_fixtures.h"
        with fixture.open("xb") as stream:
            stream.write(header(rows))
        report.update(status="captured", inputs_after=after,
                      counts={kind: len(values) for kind, values in rows.items()},
                      fixture=fingerprint(fixture),
                      classes={path.name: fingerprint(path) for path in sorted(classes.glob("*.class"))},
                      trace={"bytes": len(outputs[0]), "sha256": hashlib.sha256(outputs[0]).hexdigest()},
                      limitation="Mode flags do not establish compilation or intrinsic selection")
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        with (directory / "capture.json").open("x", encoding="ascii", newline="\n") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    print(json.dumps({"status": report["status"], "counts": report["counts"],
                      "trace": report["trace"]}))


if __name__ == "__main__":
    main()
