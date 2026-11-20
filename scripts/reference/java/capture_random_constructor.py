#!/usr/bin/env python3
"""Observe actual JDK default constructors through an owned loopback JDI target."""

from __future__ import annotations

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

from capture_nanotime_conversion import PROFILE, fingerprint, require, run

ROOT = Path(__file__).resolve().parents[3]
JAVA_SOURCES = {
    "target": ROOT / "tests/reference/java/RandomConstructorTarget.java",
    "observer": ROOT / "tests/reference/java/RandomConstructorObserver.java",
}
SOURCE_PINS = {
    "target": "35ed03df8cbcbdafc90d58118f229a2e69c8e3aec292c78f24045ec7ab0f9555",
    "observer": "b5006129d2316d8bb604d7e8ff64888b62fd595123eff8de2b8b8c9c374426e4",
    "process_helper": "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c",
}
TRANSPORT_PINS = {
    "jdwp": "4ff7345a0a66091f519a7546f1a865c4cbc9cc132fba31608604e88dec1cedf2",
    "dt_socket": "1a5c8a2a198c8645f892cb68dca87885ad759c3a3f1f828167dcdb3ac41d4874",
}
BANNER = re.compile(rb"^Listening for transport dt_socket at address: ([0-9]{1,5})\r?\n", re.MULTILINE)
HEX = re.compile(r"[0-9a-f]{16}\Z")


def verify(raw):
    require(raw.endswith(b"\n") and b"\r" not in raw, "complete LF trace required")
    require(all(value in (9, 10) or 32 <= value <= 126 for value in raw), "ASCII trace")
    rows = [line.split("\t") for line in raw[:-1].decode("ascii").split("\n")]
    require(rows[0] == ["meta", "protocol", "random-constructor-1"] and
            rows[-1] == ["result", "complete"], "complete observer protocol")
    require(len(rows) <= 8192 and all(row[0] in {"meta", "field", "method", "state", "u",
            "marker", "event", "life", "count", "result"} for row in rows), "trace row domain")

    def one(prefix):
        found = [row for row in rows if row[:len(prefix)] == prefix]
        require(len(found) == 1, "unique receipt: " + "/".join(prefix))
        return found[0]

    one(["meta", "holder_absent_at_vm_start", "1"])
    one(["meta", "can_get_method_return_values", "1"])
    one(["life", "vm_death"])
    one(["life", "vm_disconnect"])
    for name in ("default_constructors", "uniquifier_returns", "nano_time_returns", "delegate_returns"):
        require(one(["count", name]) == ["count", name, "3"], "constructor count")
    bootstrap = one(["u", "bootstrap", "after"])
    require(len(bootstrap) == 5 and HEX.fullmatch(bootstrap[4]), "bootstrap U shape")
    current = int(bootstrap[4], 16)
    require(current == 8682522807148012, "fresh Random profile admission failed")
    constructors = []
    for stage, role in (("default_a", "A"), ("math_first", "M"), ("default_b", "B")):
        committed = one(["event", stage, "uniquifier_return"])
        clock = one(["event", stage, "nano_time_return"])
        state = one(["state", stage, "default_return", role])
        require(len(committed) == len(clock) == 5 and len(state) == 9, "constructor receipt shape")
        require(all(HEX.fullmatch(value) for value in
                    (committed[3], committed[4], clock[3], clock[4], state[6], state[7])), "long bits")
        current = (current * 1181783497276652981) & ((1 << 64) - 1)
        require(int(committed[3], 16) == current, "actual shared uniquifier progression")
        require(int(state[6], 16) == ((current ^ int(clock[3], 16) ^ 0x5deece66d) & ((1 << 48) - 1)),
                "actual constructor state/clock binding")
        require(state[7:] == ["0000000000000000", "0"], "fresh constructor cache")
        constructors.append({"stage": stage, "role": role, "committed": committed[3],
                             "observed_nano_time": clock[3], "observed_state": state[6],
                             "observed_cache": state[7], "observed_flag": state[8]})
    return constructors


def observe(java, classes, directory, receipts):
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "_JAVA_OPTIONS", "CLASSPATH"):
        environment.pop(name, None)
    children = []
    deadline = time.monotonic() + 120
    failure = None
    with ExitStack() as files:
        def launch(name, command):
            outgoing, errors = directory / (name + ".stdout"), directory / (name + ".stderr")
            stdout = files.enter_context(outgoing.open("xb"))
            stderr = files.enter_context(errors.open("xb"))
            command = [str(part) for part in command]
            child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stdout,
                                     stderr=stderr, env=environment)
            receipt = {"name": name, "command": command, "pid": child.pid,
                       "exit_code": None, "reaped": False}
            receipts.append(receipt)
            children.append((child, receipt, outgoing, errors))
            return child

        def bounds():
            require(time.monotonic() < deadline, "outer observation deadline")
            for _, _, outgoing, errors in children:
                require(outgoing.stat().st_size <= 1024 * 1024 and
                        errors.stat().st_size <= 1024 * 1024, "child output threshold")

        try:
            target = launch("target", [java, "-Xint", "-Xmx128m",
                "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                "-cp", classes, "RandomConstructorTarget"])
            port = None
            banner_deadline = time.monotonic() + 10
            while port is None:
                bounds()
                require(target.poll() is None, "target exited before attach")
                require(time.monotonic() < banner_deadline, "JDWP listener deadline")
                matches = []
                for _, _, outgoing, errors in children:
                    matches.extend(BANNER.findall(outgoing.read_bytes()))
                    matches.extend(BANNER.findall(errors.read_bytes()))
                require(len(matches) <= 1, "duplicate JDWP listener banner")
                if matches:
                    port = int(matches[0])
                    require(1 <= port <= 65535, "JDWP port")
                else:
                    time.sleep(0.05)
            observer = launch("observer", [java, "--add-modules", "jdk.jdi", "-Xmx256m",
                "-cp", classes, "RandomConstructorObserver", str(port)])
            while True:
                target_status, observer_status = target.poll(), observer.poll()
                bounds()
                require(observer_status in (None, 0), "observer failed; inspect private stderr")
                require(target_status in (None, 0), "target failed; inspect private stderr")
                if target_status is not None and observer_status is not None:
                    break
                time.sleep(0.05)
            require(target.returncode == observer.returncode == 0, "natural child exits required")
        except BaseException as error:
            failure = error
        finally:
            # Terminate both owned children before waiting, including when attach/JDI stalls.
            for child, receipt, _, _ in reversed(children):
                try:
                    if child.poll() is None:
                        child.kill()
                except OSError as error:
                    receipt["termination_error"] = str(error)
                    if failure is None:
                        failure = error
            for child, receipt, _, _ in reversed(children):
                try:
                    child.wait(timeout=5)
                except (OSError, subprocess.TimeoutExpired) as error:
                    receipt["wait_error"] = str(error)
                    if failure is None:
                        failure = error
                receipt.update(exit_code=child.returncode, reaped=child.returncode is not None)
            # Finish every wait/identity receipt before optional filesystem hashing.
            for _, receipt, outgoing, errors in children:
                try:
                    receipt.update(stdout=fingerprint(outgoing), stderr=fingerprint(errors))
                except OSError as error:
                    receipt["journal_error"] = str(error)
                    if failure is None:
                        failure = error
        if failure is not None:
            raise failure
    target_out = (directory / "target.stdout").read_bytes()
    target_err = (directory / "target.stderr").read_bytes()
    require(len(BANNER.findall(target_out) + BANNER.findall(target_err)) == 1 and
            not BANNER.sub(b"", target_out) and not BANNER.sub(b"", target_err), "target banner-only output")
    require((directory / "observer.stderr").stat().st_size == 0, "unexpected observer stderr")
    raw = (directory / "observer.stdout").read_bytes()
    require(len(raw) <= 1024 * 1024, "final trace bound")
    return raw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    options = parser.parse_args()
    java, javac = options.java.resolve(strict=True), options.javac.resolve(strict=True)
    require(java.parent == javac.parent, "one JDK required")
    jdk = java.parent.parent
    inputs = {**JAVA_SOURCES, "recorder": Path(__file__).resolve(),
              "process_helper": Path(__file__).with_name("capture_nanotime_conversion.py"),
              "java": java, "javac": javac, "modules": jdk / "lib/modules",
              "jvm": jdk / "bin/server/jvm.dll", "java_native": jdk / "bin/java.dll",
              "release": jdk / "release", "source_archive": jdk / "lib/src.zip",
              "jdwp": jdk / "bin/jdwp.dll", "dt_socket": jdk / "bin/dt_socket.dll"}
    before = {key: fingerprint(path) for key, path in inputs.items()}
    for name, expected in {**PROFILE, **SOURCE_PINS, **TRANSPORT_PINS}.items():
        require(before[name]["sha256"] == expected, "reviewed input required: " + name)
    directory = options.output_dir.resolve()
    directory.mkdir(parents=False, exist_ok=False)
    classes = directory / "classes"
    classes.mkdir()
    receipts = []
    report = {"status": "failed", "scope": "Instrumented Windows JDK17 -Xint constructor observations",
              "inputs_before": before, "children": receipts}
    try:
        compiled = run([javac, "-J-Xmx256m", "--add-modules", "jdk.jdi", "-g", "-encoding", "UTF-8",
                        "-d", classes, *JAVA_SOURCES.values()], directory, "compile", receipts)
        require(not compiled, "unexpected compiler stdout")
        raw = observe(java, classes, directory, receipts)
        constructors = verify(raw)
        after = {key: fingerprint(path) for key, path in inputs.items()}
        require(before == after, "observation inputs changed")
        report.update(status="observed", inputs_after=after, constructors=constructors,
                      trace={"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()},
                      classes={path.name: fingerprint(path) for path in sorted(classes.glob("*.class"))},
                      limitation="Debugger pauses alter samples; full nanoTime returns are not underlying QPC observations")
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        with (directory / "capture.json").open("x", encoding="ascii", newline="\n") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    print(json.dumps({"status": report["status"], "constructors": len(constructors), "trace": report["trace"]}))


if __name__ == "__main__":
    main()
