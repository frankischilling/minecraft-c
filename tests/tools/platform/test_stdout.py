#!/usr/bin/env python3
"""Real stdout-handle controls; optional bytes projected from an actual JDK observer."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


SECONDS = 10
MAX_STDOUT = 256
MAX_STDERR = 2048
PAYLOADS = {"normal": b"hello", "empty": b"",
            "ascii64": b"0123456789abcdef" * 4, "nul": b"A\0B"}


class Failure(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def decode_hex(value, maximum):
    require(isinstance(value, str) and len(value) <= maximum * 2 and
            len(value) % 2 == 0 and re.fullmatch(r"[0-9a-f]*", value) is not None,
            "fixture requires bounded canonical byte hex")
    return bytes.fromhex(value)


def fixture_payloads(path):
    with path.open("rb") as stream:
        raw = stream.read(16385)
    require(len(raw) <= 16384, "JDK fixture byte bound")
    document = json.loads(raw)
    require(isinstance(document, dict) and document.get("schema") == "PRINTLN_BYTES_V1",
            "JDK fixture schema")
    separator = decode_hex(document.get("line_separator_hex"), 2)
    require(separator in (b"\n", b"\r\n"), "observed JDK line separator profile")
    rows = document.get("rows")
    require(isinstance(rows, list) and len(rows) == 6, "six actual JDK observations required")
    observed = {}
    for row in rows:
        require(isinstance(row, dict), "JDK observation row")
        name = row.get("name")
        require(isinstance(name, str) and name in {*PAYLOADS, "failing", "closed"} and
                name not in observed, "JDK observation names")
        source = decode_hex(row.get("input_hex"), 64)
        output = decode_hex(row.get("output_hex"), 128)
        error = row.get("check_error")
        require(type(error) is bool, "observed checkError boolean")
        if name in PAYLOADS:
            require(source == PAYLOADS[name] and not error and output.endswith(separator),
                    name + ": observed primitive declaration/result profile")
            # Preserve every observed payload byte. Adapt only the separately
            # declared platform suffix; do not normalize embedded payloads.
            observed[name] = output[:-len(separator)]
        else:
            require(source == b"hello" and error, name + ": actual IOException suppression")
            # These synthetic Java streams and native OS failures have distinct
            # fault locations; only their suppressed-error category is shared.
            observed[name] = output
    require(set(observed) == {*PAYLOADS, "failing", "closed"}, "complete observed profile")
    return {name: observed[name] for name in PAYLOADS}, hashlib.sha256(raw).hexdigest()


@dataclass(frozen=True)
class Case:
    name: str
    arguments: tuple[str, ...]
    stdout: bytes
    stderr: bytes
    broken: bool = False
    exit_code: int = 0


def cases(payloads):
    require(os.name == "nt" or sys.platform.startswith("linux"), "Windows/Linux profile required")
    separator = b"\r\n" if os.name == "nt" else b"\n"
    planned = [Case(name, (name,), value + separator, b"STATUS\tOK\t0\n")
               for name, value in payloads.items()]
    for name in ("over64", "non-ascii", "null-stream", "null-data"):
        planned.append(Case(name, (name,), b"", b"STATUS\tINVALID_ARGUMENT\t1\n"))
    for name in ("null-output", "null-capture"):
        planned.append(Case(name, (name,), b"", b"STATUS\tINVALID_ARGUMENT\t-\n"))
    planned.append(Case("closed", ("closed",), b"", b"STATUS\tOK\t1\n"))
    broken_modes = ["broken-default"]
    if os.name != "nt":
        broken_modes += ["broken-prepending", "broken-ignored"]
    planned += [Case(name, (name,), b"", b"STATUS\tOK\t1\n", True)
                for name in broken_modes]
    diagnostic = b"stdout probe: expected one known mode\n"
    planned += [Case("missing-mode", (), b"", diagnostic, exit_code=1),
                Case("unknown-mode", ("unknown",), b"", diagnostic, exit_code=1),
                Case("extra-mode", ("normal", "extra"), b"", diagnostic, exit_code=1)]
    return planned


def bounded_read(path, maximum):
    with path.open("rb") as stream:
        value = stream.read(maximum + 1)
    require(len(value) <= maximum, path.name + ": byte limit exceeded")
    return value


def run(executable, case, directory):
    directory.mkdir()
    input_path = directory / "stdin.bin"
    stdout_path = directory / "stdout.bin"
    stderr_path = directory / "stderr.bin"
    input_path.write_bytes(b"G" if case.broken else b"")
    child = None
    receipt = {"arguments": list(case.arguments), "pipe_reader_closed_before_gate": case.broken,
               "stdout_capture": "closed-reader-no-bytes" if case.broken else "regular-file"}
    try:
        with input_path.open("rb") as incoming, stdout_path.open("wb") as outgoing, \
                stderr_path.open("wb") as errors:
            child = subprocess.Popen([str(executable), *case.arguments],
                                     stdin=subprocess.PIPE if case.broken else incoming,
                                     stdout=subprocess.PIPE if case.broken else outgoing,
                                     stderr=errors)
            receipt["pid"] = child.pid
            deadline = time.monotonic() + SECONDS
            try:
                if case.broken:
                    require(child.stdout is not None and child.stdin is not None, "owned pipes")
                    child.stdout.close()  # All read endpoints closed before the child may write.
                    child.stdin.write(b"G")
                    child.stdin.close()   # EOF is part of the two-read gate.
                while child.poll() is None:
                    remaining = deadline - time.monotonic()
                    require(remaining > 0, case.name + ": ten-second child deadline")
                    require(os.fstat(outgoing.fileno()).st_size <= MAX_STDOUT and
                            os.fstat(errors.fileno()).st_size <= MAX_STDERR,
                            case.name + ": observed output threshold")
                    try:
                        child.wait(timeout=min(remaining, 0.05))
                    except subprocess.TimeoutExpired:
                        pass
            finally:
                if child.poll() is None:
                    child.kill()
                child.wait()
                receipt["returncode"] = child.returncode
                if child.stdin is not None:
                    child.stdin.close()
                if child.stdout is not None:
                    child.stdout.close()
        stdout = bounded_read(stdout_path, MAX_STDOUT)
        stderr = bounded_read(stderr_path, MAX_STDERR)
        receipt["stdout_sha256"] = hashlib.sha256(stdout).hexdigest()
        receipt["stderr_sha256"] = hashlib.sha256(stderr).hexdigest()
        return child.returncode, stdout, stderr
    finally:
        # Preserve exact captured bytes and the reaped child's actual outcome,
        # including failures. No descendant/process-tree guarantee is made.
        if child is not None:
            if child.poll() is None:
                child.kill()
                child.wait()
            receipt["returncode"] = child.returncode
        (directory / "receipt.json").write_text(json.dumps(receipt, sort_keys=True) + "\n",
                                                 encoding="ascii", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--jdk-fixture", type=Path)
    parser.add_argument("--work-dir", type=Path)
    options = parser.parse_args()
    try:
        executable = options.executable.resolve(strict=True)
        require(executable.is_file(), "probe executable must be a file")
        payloads = PAYLOADS
        fixture_hash = None
        if options.jdk_fixture is not None:
            payloads, fixture_hash = fixture_payloads(options.jdk_fixture)
        planned = cases(payloads)
        if options.work_dir is not None:
            options.work_dir.mkdir(parents=True, exist_ok=True)
            destination = Path(tempfile.mkdtemp(prefix="stdout-", dir=options.work_dir))
            temporary = None
        else:
            temporary = tempfile.TemporaryDirectory(prefix="stdout-")
            destination = Path(temporary.name)
        try:
            for case in planned:
                status, stdout, stderr = run(executable, case, destination / ("case-" + case.name))
                require(status == case.exit_code,
                        f"{case.name}: exact exit {case.exit_code} required, got {status}")
                require(stdout == case.stdout, case.name + ": exact stdout mismatch")
                require(stderr == case.stderr,
                        f"{case.name}: exact binary stderr mismatch: {stderr!r}")
            (destination / "summary.json").write_text(
                json.dumps({"cases": len(planned), "jdk_fixture_sha256": fixture_hash},
                           sort_keys=True) + "\n", encoding="ascii", newline="\n")
        finally:
            if temporary is not None:
                temporary.cleanup()
    except (Failure, OSError, ValueError) as error:
        print(f"FAIL: native stdout: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {len(planned)} native stdout cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
