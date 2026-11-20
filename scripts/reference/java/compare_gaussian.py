#!/usr/bin/env python3
"""Replay immutable JDK Random/Gaussian and StrictMath state observations.

Fixture capture uses only the project-owned Java helper and the specified JDK.
The native executable never supplies expected results. This compares seeded
Random/math state on a specific JDK, not the original game's classes or clocks.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / "tests/reference/java/RandomGaussianObservation.java"
FIXTURE = ROOT / "tests/reference/java/random_gaussian_fixtures.json"
HELPER_SHA256 = "ad55c6bf3e6d60c0b729f1f3b1147a938e6a4b18817696f0a0651be8d374c58b"
TRACE_SHA256 = "19459419db728d6505bcb7350936c0cc872f7ec81a66a0c73bb900ce9228e961"
PROVENANCE_SHA256 = "f570b7f7c775a6e7b830267f3caaa5ce7656777f539ac39c67a55a5317b8a158"
PROFILE_MODULES_SHA256 = "2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8"
PROFILE_JVM_SHA256 = "5539382b75556a9873fafa2d8b7d0b36b169ac436a3608467da4f74c2600cbce"
MASK = (1 << 64) - 1
LONG_MIN, LONG_MAX = -(1 << 63), (1 << 63) - 1


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    digest = hashlib.sha256()
    count = 0
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            count += len(block)
            digest.update(block)
    return {"bytes": count, "sha256": digest.hexdigest()}


def encoded(value):
    return (json.dumps(value, ensure_ascii=True, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("ascii")


def write_json(path, value):
    with Path(path).open("x", encoding="ascii", newline="\n") as stream:
        json.dump(value, stream, ensure_ascii=True, indent=2)
        stream.write("\n")


def signed(value):
    value &= MASK
    return value if value <= LONG_MAX else value - (1 << 64)


def commands():
    result = ["snapshot 0", "snapshot 1", "new 0 0", "new 1 1"]
    inputs = {1, 2, 3, 0x0000000080000001, 0x000fffffffffffff,
              0x0010000000000000, 0x0010000000000001,
              0x3ff0000000000000, 0x7fefffffffffffff}
    # Powers, neighboring representations and the log normalization/polynomial
    # boundaries are selected from the source algorithm, never native outputs.
    mantissas = [0, 1, 0x000fffffffffffff]
    for high in (0, 1, 0x61479, 0x6147a, 0x6147b, 0x6a09b, 0x6a09c,
                 0x6a09d, 0x6b850, 0x6b851, 0x6b852, 0xffffd, 0xffffe, 0xfffff):
        for low in (0, 1, 0xffffffff):
            mantissas.append((high << 32) | low)
    for exponent in (1, 2, 3, 20, 511, 512, 1022, 1023, 1024, 1535, 2045, 2046):
        for mantissa in mantissas:
            bits = (exponent << 52) | mantissa
            inputs.update(value for value in (bits - 1, bits, bits + 1)
                          if 0 < value < 0x7ff0000000000000)
    state = 0x79A37E64D1BC025F

    def draw():
        nonlocal state
        state = (state * 6364136223846793005 + 1442695040888963407) & MASK
        return state

    for _ in range(2048):
        bits = draw() & LONG_MAX
        if 0 < bits < 0x7ff0000000000000:
            inputs.add(bits)
    for bits in sorted(inputs):
        result.extend((f"log {bits:016x}", f"sqrt {bits:016x}"))

    seeds = list(range(256)) + [-1, LONG_MIN, LONG_MAX, -123456789, 0x123456789ABCDEF]
    for seed in seeds:
        other = signed(seed ^ 0xD1B54A32D192ED03)
        result.extend((f"new 0 {seed}", "gaussian 0", "snapshot 0", "int 0",
                       "double 0", "gaussian 0", "gaussian 0", f"seed 0 {seed}",
                       "gaussian 0", f"seed 0 {other}", "gaussian 0",
                       f"new 1 {other}", "gaussian 1", "long 0", "gaussian 1"))

    bounds = [-2147483648, -1, 0, 1, 2, 16, 1073741825, 2147483647]
    for _ in range(4096):
        owner, action = (draw() >> 63), (draw() >> 32) % 12
        if action < 4:
            result.append(f"gaussian {owner}")
        elif action < 8:
            result.append(f"{('int', 'long', 'float', 'double')[action - 4]} {owner}")
        elif action == 8:
            result.append(f"bounded {owner} {bounds[(draw() >> 32) % len(bounds)]}")
        elif action == 9:
            result.append(f"seed {owner} {signed(draw())}")
        elif action == 10:
            result.append(f"new {owner} {signed(draw())}")
        else:
            result.append(f"snapshot {owner}")
    require(len(result) <= 100000 and max(map(len, result)) <= 160, "helper input bounds")
    return result


def run(command, request=None, receipts=None):
    command = [str(part) for part in command]
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "_JAVA_OPTIONS", "CLASSPATH"):
        environment.pop(name, None)
    started = time.monotonic()
    # Regular files avoid unbounded communicate() buffers, blocked stdin writes
    # and waiting for inherited output pipes after the immediate child exits.
    with tempfile.TemporaryFile() as incoming, tempfile.TemporaryFile() as outgoing, \
            tempfile.TemporaryFile() as errors:
        if request is not None:
            incoming.write(request)
        incoming.seek(0)
        with subprocess.Popen(command, stdin=incoming, stdout=outgoing, stderr=errors,
                              env=environment) as child:
            failure = None
            while child.poll() is None:
                if time.monotonic() - started >= 60:
                    failure = "helper timeout"
                elif (os.fstat(outgoing.fileno()).st_size > 16 * 1024 * 1024 or
                      os.fstat(errors.fileno()).st_size > 1024 * 1024):
                    failure = "helper output threshold exceeded"
                if failure:
                    child.kill()
                    child.wait()
                    break
                try:
                    child.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    pass
            outgoing.seek(0)
            errors.seek(0)
            stdout = outgoing.read(16 * 1024 * 1024 + 1)
            stderr = errors.read(1024 * 1024 + 1)
        if receipts is not None:
            receipts.append({"command": command, "pid": child.pid, "exit_code": child.returncode,
                             "reaped": child.poll() is not None,
                             "observed_seconds": time.monotonic() - started,
                             "stdout": {"bytes": len(stdout), "sha256": hashlib.sha256(stdout).hexdigest()},
                             "stderr": {"bytes": len(stderr), "sha256": hashlib.sha256(stderr).hexdigest()}})
        require(len(stdout) <= 16 * 1024 * 1024 and len(stderr) <= 1024 * 1024,
                "helper output bounds")
        require(failure is None, failure)
        require(child.returncode == 0, f"helper exit {child.returncode}: {stderr[:2000]!r}")
        require(not stderr, f"unexpected helper stderr: {stderr[:2000]!r}")
        return stdout


def lines(raw):
    text = raw.decode("ascii")
    require(text.endswith("\n") and "\r" not in text.replace("\r\n", ""),
            "complete ASCII helper lines")
    return text.replace("\r\n", "\n").splitlines()


def observe(java, javac, request):
    java, javac = Path(java).resolve(strict=True), Path(javac).resolve(strict=True)
    require(java.parent == javac.parent, "java and javac must belong to the same JDK")
    jdk = java.parent.parent
    inputs = {"java": java, "javac": javac, "modules": jdk / "lib/modules", "helper": HELPER}
    jvm_candidates = [jdk / "bin/server/jvm.dll", jdk / "lib/server/libjvm.so"]
    jvm = next((path for path in jvm_candidates if path.is_file()), None)
    require(jvm is not None, "JDK VM library is required")
    inputs["jvm"] = jvm
    inputs["release"] = jdk / "release"
    if jvm.suffix == ".dll":
        inputs["java_native_library"] = jdk / "bin/java.dll"
    before = {name: {"path": str(path), **fingerprint(path)} for name, path in inputs.items()}
    require(before["modules"]["sha256"] == PROFILE_MODULES_SHA256 and
            before["jvm"]["sha256"] == PROFILE_JVM_SHA256,
            "exact retained Windows Temurin profile inputs required for live observation")
    receipts = []
    version = run([java, "--version"], receipts=receipts).decode("ascii")
    require(version.splitlines()[0].startswith("openjdk 17.0.17 "), "captured 17.0.17 profile required")
    with tempfile.TemporaryDirectory(prefix="mc-gaussian-jdk-") as temporary:
        run([javac, "-J-Xmx128m", "-encoding", "US-ASCII", "-d", temporary, HELPER], receipts=receipts)
        first = None
        modes = [("interpreted", ["-Xint"]), ("c1", ["-XX:TieredStopAtLevel=1"]),
                 ("default", [])]
        for name, flags in modes:
            for repeat in range(2):
                observed = run([java, "-Xmx128m", *flags, "--add-opens",
                                "java.base/java.util=ALL-UNNAMED", "-cp", temporary,
                                "RandomGaussianObservation"], request, receipts)
                if first is None:
                    first = observed
                require(first == observed, f"JDK observations disagree in {name} replay {repeat}")
    after = {name: {"path": str(path), **fingerprint(path)} for name, path in inputs.items()}
    require(before == after, "JDK or helper inputs changed during observation")
    return lines(first), {"version": version, "inputs_before": before, "inputs_after": after,
                          "children": receipts,
                          "successful_observations": 6, "modes": ["interpreted", "c1", "default"],
                          "original_game_classes_used": False,
                          "native_executable_used": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--java", type=Path)
    parser.add_argument("--javac", type=Path)
    parser.add_argument("--fixture", type=Path, default=FIXTURE)
    parser.add_argument("--capture-fixture", type=Path)
    parser.add_argument("--report", type=Path)
    options = parser.parse_args()
    require(fingerprint(HELPER)["sha256"] == HELPER_SHA256, "immutable Java helper fingerprint")
    require(bool(options.java) == bool(options.javac), "java and javac must be supplied together")
    if options.capture_fixture:
        require(options.java and not options.executable, "capture requires only JDK inputs")
        actions = commands()
        request = ("\n".join(actions) + "\n").encode("ascii")
        expected, provenance = observe(options.java, options.javac, request)
        require(len(expected) == len(actions), "one JDK result per command")
        trace = {"commands": actions, "expected": expected}
        public_provenance = {key: value for key, value in provenance.items()
                             if key not in ("inputs_before", "inputs_after", "children")}
        for phase in ("inputs_before", "inputs_after"):
            public_provenance[phase] = {name: {key: value for key, value in pin.items() if key != "path"}
                                        for name, pin in provenance[phase].items()}
        package = {"schema": 1, "scope": "jdk-random-gaussian-strictmath",
                   "helper_sha256": HELPER_SHA256,
                   "trace_sha256": hashlib.sha256(encoded(trace)).hexdigest(),
                   "provenance_sha256": hashlib.sha256(encoded(public_provenance)).hexdigest(),
                   "trace": trace, "provenance": public_provenance}
        if options.report:
            write_json(options.report, {"status": "captured", "provenance": provenance,
                                        "trace_sha256": package["trace_sha256"],
                                        "operations": len(actions)})
        write_json(options.capture_fixture, package)
        print(json.dumps({"status": "captured", "operations": len(actions),
                          "trace_sha256": package["trace_sha256"],
                          "provenance_sha256": package["provenance_sha256"]}))
        return

    require(options.executable is not None, "a native executable is required for replay")
    package = json.loads(options.fixture.read_text(encoding="ascii"))
    require(package["schema"] == 1 and package["scope"] == "jdk-random-gaussian-strictmath"
            and package["helper_sha256"] == HELPER_SHA256, "fixture scope")
    provenance = package["provenance"]
    require(hashlib.sha256(encoded(provenance)).hexdigest() ==
            package["provenance_sha256"] == PROVENANCE_SHA256, "immutable capture provenance")
    require(provenance["successful_observations"] == 6 and
            provenance["modes"] == ["interpreted", "c1", "default"] and
            provenance["original_game_classes_used"] is False and
            provenance["native_executable_used"] is False and
            provenance["inputs_before"] == provenance["inputs_after"], "JDK-only capture provenance")
    require(provenance["inputs_before"]["modules"]["sha256"] == PROFILE_MODULES_SHA256 and
            provenance["inputs_before"]["jvm"]["sha256"] == PROFILE_JVM_SHA256 and
            provenance["inputs_before"]["helper"]["sha256"] == HELPER_SHA256,
            "captured profile and helper provenance")
    trace = package["trace"]
    require(hashlib.sha256(encoded(trace)).hexdigest() == package["trace_sha256"] == TRACE_SHA256,
            "immutable JDK-only trace fingerprint")
    require(trace["commands"] == commands() and len(trace["expected"]) == len(trace["commands"]),
            "complete fixed operation corpus")
    request = ("\n".join(trace["commands"]) + "\n").encode("ascii")
    report = {"status": "failed", "scope": package["scope"], "trace_sha256": TRACE_SHA256,
              "operations": len(trace["commands"]), "original_game_classes_used": False}
    if options.java:
        fresh, observation = observe(options.java, options.javac, request)
        require(fresh == trace["expected"], "live JDK differs from immutable fixture")
        report["live_jdk"] = observation
    executable = options.executable.resolve(strict=True)
    before = fingerprint(executable)
    native_receipts = []
    actual = lines(run([executable], request, native_receipts))
    report["native_children"] = native_receipts
    require(fingerprint(executable) == before, "native executable changed during replay")
    if actual != trace["expected"]:
        index = next((index for index, pair in enumerate(zip(actual, trace["expected"]))
                      if pair[0] != pair[1]), min(len(actual), len(trace["expected"])))
        report["mismatch"] = {"index": index,
                              "command": trace["commands"][index] if index < len(trace["commands"]) else None,
                              "actual": actual[index] if index < len(actual) else None,
                              "expected": trace["expected"][index] if index < len(trace["expected"]) else None}
        if options.report:
            write_json(options.report, report)
        raise ValueError(f"Gaussian/math trace mismatch: {report['mismatch']}")
    report.update(status="pass", executable={"path": str(executable), **before})
    if options.report:
        write_json(options.report, report)
    print(json.dumps({"status": "pass", "operations": len(actual), "trace_sha256": TRACE_SHA256}))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.TimeoutExpired, KeyError, TypeError) as error:
        print(f"Gaussian/math comparison failed: {error}", file=sys.stderr)
        sys.exit(1)
