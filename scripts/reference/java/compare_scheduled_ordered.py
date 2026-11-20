#!/usr/bin/env python3
"""Replay immutable JDK ordered-record observations against the native owner.

Fixture capture uses only the project-owned Java helper and the specified JDK.
The native executable never supplies expected results. This is a collection
profile comparison, not an execution of the original game's classes.
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
HELPER = ROOT / "tests/reference/java/ScheduledOrderedObservation.java"
FIXTURE = ROOT / "tests/reference/java/scheduled_ordered_fixtures.json"
HELPER_SHA256 = "e9deade2bfefb6e6f0ca8755943553734bef1740c27a20fe7eb717875142a4de"
TRACE_SHA256 = "91796c6ecddec91376486ada468ff5cdebfc8fb5e51ed153963c8a555537bd7f"
PROVENANCE_SHA256 = "c2c518b5442b160e73880f079004d728b277ebd6c5a89b6fbc1c4a3543fe21a4"
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
    result = ["new", "record 0 0 0", "record 1 1 1", "record 2 2 2",
              "add 0", "add 1", "add 2", f"shift {LONG_MAX}",
              "first", "remove 0", "add 0", "remove 0", "first",
              "new", "record 0 7 11", "record 1 7 11", "add 0", "add 1",
              "remove 1", "first"]
    state = 0x91D07E3ABCE42F65

    def draw():
        nonlocal state
        state = (state * 6364136223846793005 + 1442695040888963407) & MASK
        return state

    def permutation(count):
        values = list(range(count))
        for index in range(count - 1, 0, -1):
            other = draw() % (index + 1)
            values[index], values[other] = values[other], values[index]
        return values

    # Vary insertion and deletion shape before introducing mutable keys.
    for history in range(12):
        result.append("new")
        for identity in range(31):
            result.append(f"record {identity} {identity - 15} {identity}")
        insertion = (list(range(31)) if history == 0 else
                     list(reversed(range(31))) if history == 1 else permutation(31))
        result.extend(f"add {identity}" for identity in insertion)
        removal = permutation(31)
        for identity in removal[:15]:
            result.extend((f"remove {identity}", "first"))
        result.append(f"shift {LONG_MAX}")
        for identity in removal[15:]:
            result.extend((f"remove {identity}", f"add {identity}", "first"))

    # Arbitrary signed times, comparator ties and repeat insertion of SAME owners.
    boundaries = [0, 1, -1, LONG_MIN, LONG_MAX, 0x123456789ABCDEF]
    for history in range(20):
        result.append("new")
        for identity in range(64):
            time = boundaries[identity % len(boundaries)] if identity < 18 else signed(draw())
            sequence = (identity % 7) - 3 if history % 2 else signed(draw())
            result.append(f"record {identity} {time} {sequence}")
        result.extend(f"add {identity}" for identity in permutation(64))
        for _ in range(400):
            action, identity = draw() % 8, (draw() >> 32) % 64
            if action < 3:
                result.append(f"add {identity}")
            elif action < 5:
                result.append(f"remove {identity}")
            elif action == 5:
                result.append(f"time {identity} {signed(draw())}")
            elif action == 6:
                result.append(f"shift {boundaries[(draw() >> 32) % len(boundaries)]}")
            else:
                result.append("first")
        result.extend(f"remove {identity}" for identity in permutation(64))
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
    with tempfile.TemporaryDirectory(prefix="mc-ordered-jdk-") as temporary:
        run([javac, "-encoding", "US-ASCII", "-d", temporary, HELPER], receipts=receipts)
        first = run([java, "-cp", temporary, "ScheduledOrderedObservation"], request, receipts)
        second = run([java, "-cp", temporary, "ScheduledOrderedObservation"], request, receipts)
    require(first == second, "independent JDK replays disagree")
    after = {name: {"path": str(path), **fingerprint(path)} for name, path in inputs.items()}
    require(before == after, "JDK or helper inputs changed during observation")
    return lines(first), {"version": version, "inputs_before": before, "inputs_after": after,
                          "children": receipts,
                          "successful_observations": 2, "original_game_classes_used": False,
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
        package = {"schema": 1, "scope": "jdk-scheduled-ordered-public-api",
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
    require(package["schema"] == 1 and package["scope"] == "jdk-scheduled-ordered-public-api"
            and package["helper_sha256"] == HELPER_SHA256, "fixture scope")
    provenance = package["provenance"]
    require(hashlib.sha256(encoded(provenance)).hexdigest() ==
            package["provenance_sha256"] == PROVENANCE_SHA256, "immutable capture provenance")
    require(provenance["successful_observations"] == 2 and
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
        raise ValueError(f"ordered trace mismatch: {report['mismatch']}")
    report.update(status="pass", executable={"path": str(executable), **before})
    if options.report:
        write_json(options.report, report)
    print(json.dumps({"status": "pass", "operations": len(actual), "trace_sha256": TRACE_SHA256}))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.TimeoutExpired, KeyError, TypeError) as error:
        print(f"scheduled ordered comparison failed: {error}", file=sys.stderr)
        sys.exit(1)
