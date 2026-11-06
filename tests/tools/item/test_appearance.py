"""Compare complete native Item appearance observations with the original packet."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
REFERENCE = Path(__file__).resolve().parents[2] / "reference/item/appearance"
PREFIXES = ("CLASS\t", "APPEARANCE\t", "BM\t", "COMPLETE\t")
BOOTSTRAP = ["16 achievements", "151 recipes"]
BOOTSTRAP_SEPARATOR = b"\r\n" if os.name == "nt" else b"\n"
BOOTSTRAP_BYTES = (BOOTSTRAP_SEPARATOR.join(line.encode("ascii") for line in BOOTSTRAP) +
                   BOOTSTRAP_SEPARATOR)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare(actual, expected):
    if actual != expected:
        for index, (left, right) in enumerate(zip(actual, expected)):
            if left != right:
                raise ValueError(f"Item appearance row {index}: {left!r} != {right!r}")
        raise ValueError(f"Item appearance row count {len(actual)} != {len(expected)}")


def select_rows(output):
    if not output.startswith(BOOTSTRAP_BYTES):
        raise ValueError("Native appearance Registry bootstrap output differs")
    data = output[len(BOOTSTRAP_BYTES):]
    rows = data.decode("ascii").splitlines()
    if data != ("\n".join(rows) + "\n").encode("ascii"):
        raise ValueError("Native appearance data rows must have exact LF line endings")
    if any(not row.startswith(PREFIXES) for row in rows):
        raise ValueError("Unexpected native Item appearance output")
    return rows


def controls(expected):
    compare(expected, expected)
    data = ("\n".join(expected) + "\n").encode("ascii")
    compare(select_rows(BOOTSTRAP_BYTES + data), expected)
    changes = [expected[1:], expected + [expected[-1]],
               [expected[1], expected[0], *expected[2:]]]
    for prefix in ("APPEARANCE\t", "BM\t"):
        index = next(i for i, row in enumerate(expected) if row.startswith(prefix))
        changed = expected.copy()
        changed[index], changed[index + 1] = changed[index + 1], changed[index]
        changes.append(changed)
    for prefix, columns in (("CLASS\t", range(1, 8)),
                            ("APPEARANCE\t", range(1, 8)),
                            ("BM\t", range(1, 6)),
                            ("COMPLETE\t", range(1, 5))):
        index = next(i for i, row in enumerate(expected) if row.startswith(prefix))
        for column in columns:
            changed = expected.copy()
            fields = changed[index].split("\t")
            fields[column] = (str(int(fields[column]) + 1)
                              if fields[column].lstrip("-").isdigit() else "zz")
            changed[index] = "\t".join(fields)
            changes.append(changed)
    for changed in changes:
        try:
            compare(changed, expected)
        except ValueError:
            continue
        raise ValueError("Item appearance comparator accepted changed transcript")
    other_separator = b"\n" if BOOTSTRAP_SEPARATOR == b"\r\n" else b"\r\n"
    other_bootstrap = (other_separator.join(line.encode("ascii") for line in BOOTSTRAP) +
                       other_separator)
    mixed_bootstrap = (BOOTSTRAP[0].encode("ascii") + BOOTSTRAP_SEPARATOR +
                       BOOTSTRAP[1].encode("ascii") + other_separator)
    malformed = [data,
                 BOOTSTRAP_BYTES + ("\r\n".join(expected) + "\r\n").encode("ascii"),
                 BOOTSTRAP_BYTES + data + b"unexpected\n",
                 other_bootstrap + data,
                 mixed_bootstrap + data,
                 BOOTSTRAP_BYTES + data.replace(b"\n", b"\r\n", 1),
                 BOOTSTRAP_BYTES + data[:-1]]
    for output in malformed:
        try:
            select_rows(output)
        except ValueError:
            continue
        raise ValueError("Item appearance selector accepted malformed output")
    return len(changes) + len(malformed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="appearance-", dir=args.work_dir)).resolve()
    before = digest(executable)
    report = {"status": "FAIL", "executable_before": before,
              "command": [str(executable)]}
    try:
        spec = importlib.util.spec_from_file_location("appearance_packet", REFERENCE / "packet.py")
        packet = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(packet)
        report["reference"] = packet.audit()
        expected = (REFERENCE / "original.stdout").read_text(encoding="ascii").splitlines()
        report["comparator_controls"] = controls(expected)
        result = subprocess.run([str(executable)], capture_output=True, timeout=45)
        (output / "stdout.txt").write_bytes(result.stdout)
        (output / "stderr.txt").write_bytes(result.stderr)
        report["returncode"] = result.returncode
        report["stdout_sha256"] = hashlib.sha256(result.stdout).hexdigest()
        report["stderr_sha256"] = hashlib.sha256(result.stderr).hexdigest()
        if result.returncode != 0 or result.stderr:
            raise ValueError("Native Item appearance process failed: " +
                             result.stderr.decode("utf-8", errors="backslashreplace"))
        compare(select_rows(result.stdout), expected)
        if packet.audit() != report["reference"]:
            raise ValueError("Original Item appearance packet changed during comparison")
        report["executable_after"] = digest(executable)
        if report["executable_after"] != before:
            raise ValueError("Native Item appearance executable changed during comparison")
        report.update(status="PASS", items=202, classes=38, appearance_rows=4040,
                      bm_rows=64, compared_lines=len(expected))
    except BaseException as error:
        report["error"] = repr(error)
        if isinstance(error, subprocess.TimeoutExpired):
            (output / "stdout.txt").write_bytes(error.stdout or b"")
            (output / "stderr.txt").write_bytes(error.stderr or b"")
        raise
    finally:
        if executable.is_file():
            report["executable_after"] = digest(executable)
        report["executable_unchanged"] = report.get("executable_after") == before
        continuity_failed = report["status"] == "PASS" and not report["executable_unchanged"]
        if continuity_failed:
            report.update(status="FAIL", error="Native appearance executable continuity failed")
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if continuity_failed:
            raise ValueError(report["error"])
    print("PASS: 4307 original Item appearance rows, "
          f"{report['comparator_controls']} corruption controls; {output}")


if __name__ == "__main__":
    main()
