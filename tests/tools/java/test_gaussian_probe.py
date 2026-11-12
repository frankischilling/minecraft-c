#!/usr/bin/env python3
"""Native Gaussian/math probe protocol controls using pinned JDK observations."""

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

# Raw observations from random_gaussian_fixtures.json, SHA-256
# fe943a8177eb02663c29a66bae1e1eab22f1c12452d2ceef06fa4e3a8646a604.
# These are fixed inputs/expected bytes, not a native-result capture or RNG model.
ABSENT = b"absent"
FRESH_0 = b"00000005deece66d:0000000000000000:0"
FRESH_1 = b"00000005deece66c:0000000000000000:0"
FRESH_MAX = b"0000fffa21131992:0000000000000000:0"
GAUSSIAN_0 = b"00009b3970be8d41:bfecd9772eb2e0c8:1"
GAUSSIAN_0_RESULT = b"3fe9ae59d1d6f861"
INPUT_ERROR = b"Gaussian probe: invalid input or input limit\n"


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


def event(command, result=b"-", owner0=ABSENT, owner1=ABSENT):
    # Only serialize already specified observations; never calculate a state.
    return b" ".join((command, result, owner0, owner1)) + b"\n"


PREFIX = b"new 0 0\nnew 1 1\ngaussian 0\n"
PREFIX_OUTPUT = (event(b"new", owner0=FRESH_0) +
                 event(b"new", owner0=FRESH_0, owner1=FRESH_1) +
                 event(b"gaussian", GAUSSIAN_0_RESULT, GAUSSIAN_0, FRESH_1))


def rejected(name, command, *, prefix=PREFIX, output=PREFIX_OUTPUT,
             operation=4, reader_error=False):
    diagnostic = (INPUT_ERROR if reader_error else
                  f"Gaussian probe: invalid command at operation {operation}\n".encode("ascii"))
    # A valid following command must never execute after the invalid command.
    return Case(name, prefix + command + b"\nsnapshot 0\n", output, diagnostic, 1)


def cases():
    result = [Case("empty-eof", b"", b"")]
    commands = (b"snapshot 0", b"new 0 0", b"new 1 1", b"gaussian 0", b"snapshot 1")
    output = (event(b"snapshot") + PREFIX_OUTPUT +
              event(b"snapshot", owner0=GAUSSIAN_0, owner1=FRESH_1))
    for name, terminator in (("lf", b"\n"), ("cr", b"\r"), ("crlf", b"\r\n")):
        request = terminator.join(commands)
        result.append(Case(name + "-terminated", request + terminator, output))
        result.append(Case(name + "-unterminated-final", request, output))
    result.append(Case("mixed-terminators",
                       b"snapshot 0\r\nnew 0 0\rnew 1 1\ngaussian 0\r\nsnapshot 1",
                       output))
    result.append(Case("absent-snapshots-and-ownerless-math",
                       b"snapshot 0\nsnapshot 1\nlog 3ff0000000000000\n"
                       b"sqrt 3FF0000000000000\nlog 3FF0000000000000\n"
                       b"sqrt 3ff0000000000000\n",
                       event(b"snapshot") + event(b"snapshot") +
                       event(b"log", b"0000000000000000") +
                       event(b"sqrt", b"3ff0000000000000") +
                       event(b"log", b"0000000000000000") +
                       event(b"sqrt", b"3ff0000000000000")))

    # Complete consecutive fixture history at indices 6002..6016. The skipped
    # standalone math calls preceding it do not change either Random owner.
    history = (
        (b"new 0 0", event(b"new", owner0=FRESH_0, owner1=FRESH_1)),
        (b"gaussian 0", event(b"gaussian", GAUSSIAN_0_RESULT, GAUSSIAN_0, FRESH_1)),
        (b"snapshot 0", event(b"snapshot", owner0=GAUSSIAN_0, owner1=FRESH_1)),
        (b"int 0", event(b"int", b"a32dc9f6",
                          b"0000a32dc9f68ab8:bfecd9772eb2e0c8:1", FRESH_1)),
        (b"double 0", event(b"double", b"3fd3c77c08ce970a",
                             b"00008ce970b7fc32:bfecd9772eb2e0c8:1", FRESH_1)),
        (b"gaussian 0", event(b"gaussian", b"bfecd9772eb2e0c8",
                               b"00008ce970b7fc32:bfecd9772eb2e0c8:0", FRESH_1)),
        (b"gaussian 0", event(b"gaussian", b"bfd732068a7ed544",
                               b"0000554dcd2b1a56:3fd10cfbc79228fe:1", FRESH_1)),
        (b"seed 0 0", event(b"seed", owner0=b"00000005deece66d:3fd10cfbc79228fe:0",
                             owner1=FRESH_1)),
        (b"gaussian 0", event(b"gaussian", GAUSSIAN_0_RESULT, GAUSSIAN_0, FRESH_1)),
        (b"seed 0 -3335678366873096957",
         event(b"seed", owner0=b"00004a370f7e0b6e:bfecd9772eb2e0c8:0", owner1=FRESH_1)),
        (b"gaussian 0", event(b"gaussian", b"3fd6fa53f7833317",
                               b"0000c4945a496152:bfd191b64c94d6cd:1", FRESH_1)),
        (b"new 1 -3335678366873096957",
         event(b"new", owner0=b"0000c4945a496152:bfd191b64c94d6cd:1",
               owner1=b"00004a370f7e0b6e:0000000000000000:0")),
        (b"gaussian 1", event(b"gaussian", b"3fd6fa53f7833317",
                               b"0000c4945a496152:bfd191b64c94d6cd:1",
                               b"0000c4945a496152:bfd191b64c94d6cd:1")),
        (b"long 0", event(b"long", b"c0ed3245f0d2a0d4",
                           b"0000f0d2a0d4055c:bfd191b64c94d6cd:1",
                           b"0000c4945a496152:bfd191b64c94d6cd:1")),
        (b"gaussian 1", event(b"gaussian", b"bfd191b64c94d6cd",
                               b"0000f0d2a0d4055c:bfd191b64c94d6cd:1",
                               b"0000c4945a496152:bfd191b64c94d6cd:0")),
    )
    result.append(Case("fixture-interleaving-cache-and-reseed-history",
                       b"new 0 0\nnew 1 1\n" + b"\n".join(row[0] for row in history) + b"\n",
                       event(b"new", owner0=FRESH_0) +
                       event(b"new", owner0=FRESH_0, owner1=FRESH_1) +
                       b"".join(row[1] for row in history)))

    result.append(Case("fresh-replacement-clears-pending-cache",
                       PREFIX + b"new 0 0\nsnapshot 1\n",
                       PREFIX_OUTPUT + event(b"new", owner0=FRESH_0, owner1=FRESH_1) +
                       event(b"snapshot", owner0=FRESH_0, owner1=FRESH_1)))
    result.append(Case("signed-seed-endpoints-and-owner-signs",
                       b"new -0 -9223372036854775808\n"
                       b"new +001 +9223372036854775807\nseed +000 +0\nsnapshot -0\n",
                       event(b"new", owner0=FRESH_0) +
                       event(b"new", owner0=FRESH_0, owner1=FRESH_MAX) +
                       event(b"seed", owner0=FRESH_0, owner1=FRESH_MAX) +
                       event(b"snapshot", owner0=FRESH_0, owner1=FRESH_MAX)))

    # Local-owner fields/results below are captured adjacent new/draw pairs;
    # the other slot is independently absent in each fresh test process.
    result.append(Case("fixture-float-result-and-full-state",
                       b"new 1 3334766954686900500\nfloat 1\nsnapshot 0\n",
                       event(b"new", owner1=b"000078e5a03e0f79:0000000000000000:0") +
                       event(b"float", b"3f1fbb2d",
                             owner1=b"00009fbb2ddf4c90:0000000000000000:0") +
                       event(b"snapshot", owner1=b"00009fbb2ddf4c90:0000000000000000:0")))
    result.append(Case("positive-int32-max-bound-with-leading-plus",
                       b"new 1 -511192783695997150\nbounded +01 +2147483647\n",
                       event(b"new", owner1=b"0000e0dafae2714f:0000000000000000:0") +
                       event(b"bounded", b"2c191d85",
                             owner1=b"000058323b0b38ae:0000000000000000:0")))
    result.append(Case("positive-bound-one-still-advances-state",
                       b"new 0 2713990684924858633\nbounded -0 +0001\n",
                       event(b"new", owner0=b"00000823d446b764:0000000000000000:0") +
                       event(b"bounded", b"00000000",
                             owner0=b"00004126b30fed9f:0000000000000000:0")))

    invalid_bounds = (b"bounded 0 -2147483648", b"bounded 1 -1", b"bounded +00 -0",
                      b"bounded 0 +0", b"bounded 1 +0000")
    result.append(Case("nonpositive-bounds-succeed-with-no-owner-effects",
                       PREFIX + b"\n".join(invalid_bounds) + b"\nsnapshot 1\n",
                       PREFIX_OUTPUT +
                       event(b"bounded", b"bound-error", GAUSSIAN_0, FRESH_1) * len(invalid_bounds) +
                       event(b"snapshot", owner0=GAUSSIAN_0, owner1=FRESH_1)))

    math = ((b"log 3ff0000000000000", b"log", b"0000000000000000"),
            (b"sqrt 3FF0000000000000", b"sqrt", b"3ff0000000000000"),
            (b"log 0000000000000001", b"log", b"c0874385446d71c3"),
            (b"sqrt 0000000000000001", b"sqrt", b"1e60000000000000"),
            (b"log 0000000080000001", b"log", b"c086979ebd354e82"),
            (b"sqrt 0000000080000001", b"sqrt", b"1f56a09e6695dc6b"))
    result.append(Case("math-positive-subnormal-and-identity-preserve-cached-owners",
                       PREFIX + b"\n".join(row[0] for row in math) + b"\n",
                       PREFIX_OUTPUT + b"".join(event(row[1], row[2], GAUSSIAN_0, FRESH_1)
                                                for row in math)))

    longest = b"new 0 " + b"0" * 154
    require(len(longest) == 160, "constructed valid 160-byte command")
    result.append(Case("exactly-160-printable-bytes",
                       PREFIX + longest + b"\nsnapshot 0\n",
                       PREFIX_OUTPUT + event(b"new", owner0=FRESH_0, owner1=FRESH_1) +
                       event(b"snapshot", owner0=FRESH_0, owner1=FRESH_1)))

    absent_prefix = b"snapshot 0\nsnapshot 1\n"
    absent_output = event(b"snapshot") * 2
    for owner in (b"0", b"1"):
        for command in (b"seed " + owner + b" 0", b"gaussian " + owner, b"int " + owner,
                        b"long " + owner, b"float " + owner, b"double " + owner,
                        b"bounded " + owner + b" 0"):
            result.append(rejected("absent-owner-" + command.decode("ascii"), command,
                                   prefix=absent_prefix, output=absent_output, operation=3))

    for command in (b"new", b"new 0", b"new 0 0 extra", b"seed", b"seed 0",
                    b"seed 0 0 extra", b"bounded", b"bounded 0", b"bounded 0 1 extra"):
        result.append(rejected("arity-" + command.decode("ascii"), command))
    for name in (b"gaussian", b"int", b"long", b"float", b"double", b"snapshot"):
        for command in (name, name + b" 0 extra"):
            result.append(rejected("arity-" + command.decode("ascii"), command))
    for name in (b"log", b"sqrt"):
        for command in (name, name + b" 3ff0000000000000 extra"):
            result.append(rejected("arity-" + command.decode("ascii"), command))

    for token in (b"-1", b"2", b"2147483648", b"-2147483649",
                  b"9223372036854775808"):
        for command in (b"snapshot " + token, b"new " + token + b" 0"):
            result.append(rejected("owner-id-" + command.decode("ascii"), command))
    for token in (b"9223372036854775808", b"-9223372036854775809"):
        for command in (b"new 0 " + token, b"seed 0 " + token):
            result.append(rejected("seed-overflow-" + command.decode("ascii"), command))
    for token in (b"2147483648", b"-2147483649", b"9223372036854775808"):
        result.append(rejected("bound-overflow-" + token.decode("ascii"), b"bounded 0 " + token))
    for token in (b"+", b"-", b"0x10", b"1e0", b"1junk", b"++1", b"--1"):
        result.append(rejected("bad-decimal-" + token.decode("ascii"), b"seed 0 " + token))

    malformed_math = (b"0000000000000000", b"8000000000000000", b"bff0000000000000",
                      b"7ff0000000000000", b"fff0000000000000", b"7ff8000000000000",
                      b"7ff0000000000001", b"fff8000000000001", b"ffffffffffffffff",
                      b"3ff000000000000", b"03ff0000000000000", b"+ff0000000000000",
                      b"0x3ff0000000000000", b"3fg0000000000000", b"3ff000000000000z", b"")
    for name in (b"log", b"sqrt"):
        for token in malformed_math:
            command = name + b" " + token
            result.append(rejected("bad-math-" + command.decode("ascii"), command))
    for name, command in (("unknown", b"unknown 0"), ("empty", b""),
                          ("leading-space", b" snapshot 0"), ("trailing-space", b"snapshot 0 "),
                          ("repeated-space", b"new  0 0"), ("empty-seed", b"seed 0 ")):
        result.append(rejected(name, command))

    for value in [number for number in range(0x20) if number not in (0x0a, 0x0d)] + [0x7f]:
        result.append(rejected(f"control-byte-{value:02x}", b"snapshot 0" + bytes([value]),
                               reader_error=True))
    for name, raw in (("non-ascii-80", b"\x80"), ("non-ascii-ff", b"\xff"),
                      ("utf8-non-ascii", b"\xc2\xa0")):
        result.append(rejected(name, b"snapshot 0" + raw, reader_error=True))
    overlong = b"new 0 " + b"0" * 155
    require(len(overlong) == 161, "constructed invalid 161-byte command")
    result.append(rejected("161-printable-bytes", overlong, reader_error=True))
    result.append(Case("cr-cr-empty-logical-line", b"snapshot 0\r\rsnapshot 1\n",
                       event(b"snapshot"), b"Gaussian probe: invalid command at operation 2\n", 1))
    for argument in ("--help", ""):
        result.append(Case("extra-argument-" + repr(argument), PREFIX, b"",
                           b"Gaussian probe: no arguments accepted\n", 1, (argument,)))
    return result


def run(executable, case):
    require(len(case.request) <= MAX_INPUT, case.name + ": bounded request")
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
                    require(remaining > 0, case.name + ": child exceeded ten seconds")
                    require(os.fstat(outgoing.fileno()).st_size <= MAX_STDOUT and
                            os.fstat(errors.fileno()).st_size <= MAX_STDERR,
                            case.name + ": output threshold exceeded")
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
                    case.name + ": output bounds")
            return child.returncode, stdout, stderr


def exact_output(actual, expected):
    return actual == expected or actual == expected.replace(b"\n", b"\r\n")


def check(executable, case):
    exit_code, stdout, stderr = run(executable, case)
    require(exit_code == case.exit_code,
            f"{case.name}: expected exit {case.exit_code}, got {exit_code}; stderr={stderr!r}")
    require(exact_output(stdout, case.stdout),
            f"{case.name}: expected stdout {case.stdout!r}, got {stdout!r}")
    require(exact_output(stderr, case.stderr),
            f"{case.name}: expected stderr {case.stderr!r}, got {stderr!r}")


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
        print(f"FAIL: Gaussian probe protocol: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {len(planned)} native Gaussian/math protocol cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
