#!/usr/bin/env python3
"""Constructed native ordered-probe protocol cases; no Java or asset dependency."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


CHILD_SECONDS = 10
MAX_INPUT = 4096
MAX_STDOUT = 16 * 1024
MAX_STDERR = 512

PREFIX = b"new\nrecord 0 7 11\nadd 0\n"
PREFIX_OUTPUT = b"new - 0 empty\nrecord - 0 empty\nadd 1 1 0\n"
COMMAND_ERROR = b"ordered probe: invalid command or native failure at operation 4\n"
INPUT_ERROR = b"ordered probe: invalid input, read failure or input limit\n"


class ProtocolFailure(Exception):
    pass


@dataclass(frozen=True)
class Case:
    name: str
    request: bytes
    stdout: bytes
    stderr: bytes = b""
    exit_code: int = 0
    arguments: tuple[str, ...] = ()


def require(condition, message):
    if not condition:
        raise ProtocolFailure(message)


def rejected(name, command, diagnostic=COMMAND_ERROR):
    # The following valid command must never acquire a snapshot after rejection.
    return Case(name, PREFIX + command + b"\nfirst\n", PREFIX_OUTPUT,
                diagnostic, 1)


def cases():
    result = [Case("empty-eof", b"", b"")]
    commands = (b"new", b"first", b"record 127 0 0", b"add 127", b"remove 127")
    output = (b"new - 0 empty\nfirst - 0 empty\nrecord - 0 empty\n"
              b"add 1 1 127\nremove 1 0 empty\n")
    for name, terminator in (("lf", b"\n"), ("cr", b"\r"), ("crlf", b"\r\n")):
        request = terminator.join(commands)
        result.append(Case(name + "-terminated", request + terminator, output))
        result.append(Case(name + "-unterminated-final", request, output))
    result.append(Case("mixed-terminators",
                       b"new\r\nfirst\rrecord 127 0 0\nadd 127\r\nremove 127",
                       output))

    result.extend((
        Case("new-resets-installed-and-caller-records",
             b"new\nrecord 0 5 2\nadd 0\nnew\nrecord 0 -3 7\nadd 0\n"
             b"first\nremove 0\nremove 0\n",
             b"new - 0 empty\nrecord - 0 empty\nadd 1 1 0\nnew - 0 empty\n"
             b"record - 0 empty\nadd 1 1 0\nfirst - 1 0\nremove 1 0 empty\n"
             b"remove 0 0 empty\n"),
        Case("comparator-equal-distinct-record-query",
             b"new\nrecord 0 7 11\nrecord 1 7 11\nadd 0\nadd 1\nremove 1\n"
             b"first\nadd 1\nremove 0\n",
             b"new - 0 empty\nrecord - 0 empty\nrecord - 0 empty\nadd 1 1 0\n"
             b"add 0 1 0\nremove 1 0 empty\nfirst - 0 empty\nadd 1 1 1\n"
             b"remove 1 0 empty\n"),
        Case("current-first-is-not-resorted-after-time-set",
             b"new\nrecord 1 20 4\nrecord 0 10 3\nadd 1\nadd 0\ntime 0 30\n"
             b"first\nremove 0\ntime 0 5\nremove 0\nfirst\n",
             b"new - 0 empty\nrecord - 0 empty\nrecord - 0 empty\nadd 1 1 1\n"
             b"add 1 2 0\ntime - 2 0\nfirst - 2 0\nremove 0 2 0\n"
             b"time - 2 0\nremove 1 1 1\nfirst - 1 1\n"),
        Case("signed-endpoints-leading-signs-and-absent-record-wrap",
             b"new\nrecord -0 -9223372036854775808 +9223372036854775807\n"
             b"shift -1\nrecord +127 +0 -9223372036854775808\nadd +000\n"
             b"add 000127\ntime -00 -9223372036854775808\nremove +127\n"
             b"time +127 -9223372036854775808\nremove 000127\nshift +000\nremove -0\n",
             b"new - 0 empty\nrecord - 0 empty\nshift - 0 empty\nrecord - 0 empty\n"
             b"add 1 1 0\nadd 1 2 127\ntime - 2 127\nremove 0 2 127\n"
             b"time - 2 127\nremove 1 1 0\nshift - 1 0\nremove 1 0 empty\n"),
    ))

    # 9-byte prefix + 149 zero digits + 2-byte suffix = exactly 160 bytes.
    longest_valid = b"record 1 " + b"0" * 149 + b" 0"
    require(len(longest_valid) == 160, "constructed 160-byte valid boundary")
    result.append(Case("exactly-160-printable-bytes",
                       PREFIX + longest_valid + b"\nadd 1\nfirst\n",
                       PREFIX_OUTPUT + b"record - 1 0\nadd 1 2 1\nfirst - 2 1\n"))

    arity = (
        ("new-extra", b"new 0"),
        ("record-missing-all", b"record"),
        ("record-missing-sequence", b"record 1 8"),
        ("record-extra", b"record 1 8 12 extra"),
        ("add-missing", b"add"),
        ("add-extra", b"add 0 extra"),
        ("remove-missing", b"remove"),
        ("remove-extra", b"remove 0 extra"),
        ("time-missing", b"time 0"),
        ("time-extra", b"time 0 7 extra"),
        ("shift-missing", b"shift"),
        ("shift-extra", b"shift 0 extra"),
        ("first-extra", b"first 0"),
    )
    for name, command in arity:
        result.append(rejected(name, command))
    for command in (b"add 1", b"remove 1", b"time 1 8"):
        result.append(rejected("unknown-record-" + command.decode("ascii"), command))
    result.append(rejected("reused-record-id", b"record 0 8 12"))

    for token in (b"-1", b"128"):
        for command in (b"record " + token + b" 0 0", b"add " + token,
                        b"remove " + token, b"time " + token + b" 0"):
            result.append(rejected("id-bound-" + command.decode("ascii"), command))
    for name, command in (
        ("id-above-int32", b"record 2147483648 0 0"),
        ("id-below-int32", b"add -2147483649"),
        ("id-positive-int64-overflow", b"add 9223372036854775808"),
        ("id-negative-int64-overflow", b"record -9223372036854775809 0 0"),
    ):
        result.append(rejected(name, command))

    for token in (b"9223372036854775808", b"-9223372036854775809"):
        for command in (b"record 1 " + token + b" 0", b"record 1 0 " + token,
                        b"time 0 " + token, b"shift " + token):
            result.append(rejected("int64-overflow-" + command.decode("ascii"), command))
    for token in (b"+", b"-", b"0x10", b"1e0", b"1junk", b"++1", b"--1"):
        result.append(rejected("invalid-numeric-token-" + token.decode("ascii"),
                               b"shift " + token))

    for name, command in (
        ("unknown-command", b"unknown"),
        ("empty-line", b""),
        ("leading-space", b" first"),
        ("trailing-space", b"first "),
        ("repeated-field-space", b"record 1  8 12"),
        ("empty-numeric-field", b"time 0 "),
    ):
        result.append(rejected(name, command))

    # Every non-terminating ASCII control, including NUL, TAB and Windows Ctrl-Z.
    controls = [value for value in range(0x20) if value not in (0x0a, 0x0d)] + [0x7f]
    for value in controls:
        result.append(rejected(f"control-byte-{value:02x}", b"first" + bytes([value]),
                               INPUT_ERROR))
    for name, raw in (("non-ascii-80", b"\x80"), ("non-ascii-ff", b"\xff"),
                      ("utf8-non-ascii", b"\xc2\xa0")):
        result.append(rejected(name, b"first" + raw, INPUT_ERROR))

    overlong = b"record 1 " + b"0" * 150 + b" 0"
    require(len(overlong) == 161, "constructed 161-byte invalid boundary")
    result.append(rejected("161-printable-bytes", overlong, INPUT_ERROR))
    result.append(Case("cr-cr-is-an-empty-logical-line", b"new\r\rfirst\n",
                       b"new - 0 empty\n",
                       b"ordered probe: invalid command or native failure at operation 2\n", 1))
    for argument in ("--help", ""):
        result.append(Case("extra-argument-" + repr(argument), PREFIX, b"",
                           b"ordered probe: no arguments accepted\n", 1, (argument,)))
    return result


def run(executable, case):
    require(len(case.request) <= MAX_INPUT, case.name + ": bounded constructed request")
    with tempfile.TemporaryFile() as incoming, tempfile.TemporaryFile() as outgoing, \
            tempfile.TemporaryFile() as errors:
        incoming.write(case.request)
        incoming.seek(0)
        with subprocess.Popen([str(executable), *case.arguments], stdin=incoming,
                              stdout=outgoing, stderr=errors) as child:
            deadline = time.monotonic() + CHILD_SECONDS
            try:
                while child.poll() is None:
                    remaining = deadline - time.monotonic()
                    require(remaining > 0, case.name + ": native child exceeded ten seconds")
                    require(os.fstat(outgoing.fileno()).st_size <= MAX_STDOUT and
                            os.fstat(errors.fileno()).st_size <= MAX_STDERR,
                            case.name + ": native output threshold exceeded")
                    try:
                        child.wait(timeout=min(0.05, remaining))
                    except subprocess.TimeoutExpired:
                        pass
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
            outgoing.seek(0)
            errors.seek(0)
            stdout = outgoing.read(MAX_STDOUT + 1)
            stderr = errors.read(MAX_STDERR + 1)
            require(len(stdout) <= MAX_STDOUT and len(stderr) <= MAX_STDERR,
                    case.name + ": native output bounds")
            return child.returncode, stdout, stderr


def exact_output(actual, expected):
    # printf text output is uniformly LF or Windows CRLF; no other normalization.
    return actual == expected or actual == expected.replace(b"\n", b"\r\n")


def check(executable, case):
    exit_code, stdout, stderr = run(executable, case)
    require(exit_code == case.exit_code,
            f"{case.name}: expected exit {case.exit_code}, got {exit_code}; "
            f"stderr={stderr[:MAX_STDERR]!r}")
    require(exact_output(stdout, case.stdout),
            f"{case.name}: exact stdout mismatch: expected {case.stdout!r}, got {stdout!r}")
    require(exact_output(stderr, case.stderr),
            f"{case.name}: exact stderr mismatch: expected {case.stderr!r}, got {stderr!r}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    options = parser.parse_args()
    try:
        executable = options.executable.resolve(strict=True)
        require(executable.is_file(), "native probe executable must be a file")
        planned = cases()
        for case in planned:
            check(executable, case)
    except (ProtocolFailure, OSError) as error:
        print(f"FAIL: scheduled ordered protocol: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {len(planned)} native scheduled ordered protocol cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
