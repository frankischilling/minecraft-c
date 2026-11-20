"""Reject incomplete process evidence and changed owner fields using fresh native output."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / "reference/client/application_owner/support.py"
    spec = importlib.util.spec_from_file_location("application_owner_controls", source)
    if spec is None or spec.loader is None:
        raise ValueError("concrete source-bound application owner support")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="application-owner-driver-", dir=args.work_dir.resolve()))
    executable = args.executable.resolve(strict=True)
    bound = support.source_binding()
    native = support.fingerprint(executable)
    checks = 0
    captures = {}

    def rebound(report, stdout, stderr=b""):
        result = copy.deepcopy(report)
        for name, data in [("stdout", stdout), ("stderr", stderr)]:
            result[name + "_bytes"] = len(data)
            result[name + "_sha256"] = hashlib.sha256(data).hexdigest()
        return result

    def rejected(report, stdout, stderr, mode):
        nonlocal checks
        try:
            support.validate(report, stdout, stderr, mode)
        except (ValueError, AssertionError, KeyError, UnicodeError):
            checks += 1
        else:
            raise ValueError("accepted malformed application-owner process/record evidence")

    for mode in ["contracts", "original"]:
        command = [executable] + (["original"] if mode == "original" else [])
        report = support.capture(command, out, mode, environment=os.environ.copy())
        stdout = (out / report["stdout_file"]).read_bytes()
        stderr = (out / report["stderr_file"]).read_bytes()
        support.validate(report, stdout, stderr, mode)
        checks += 1
        captures[mode] = report
        normal = stdout.replace(b"\r\n", b"\n")
        for good in [normal, normal.replace(b"\n", b"\r\n")]:
            support.validate(rebound(report, good), good, b"", mode)
            checks += 1
        for key, value in [("exit", 1), ("exit", 139), ("exit", False),
                           ("pipe_capture_complete", False), ("pipe_capture_complete", 1),
                           ("timed_out", True), ("timed_out", 0), ("output_overflow", True),
                           ("reader_errors", []), ("stdout_bytes", len(stdout) + 1),
                           ("stdout_sha256", "0" * 64), ("stderr_limit", 0)]:
            changed = copy.deepcopy(report)
            changed[key] = value
            rejected(changed, stdout, stderr, mode)
        for bad in [b"", stdout + b"EXTRA\n", stdout[:len(stdout) // 2], normal[:-1],
                    b"\x00" + stdout, stdout[1:], stdout + stdout, stdout.replace(b" ", b"\x00", 1)]:
            rejected(rebound(report, bad), bad, b"", mode)
        for delimiter in [b"\x0b", b"\x0c", b"\x1c", b"\x1d", b"\x1e"]:
            bad = normal.replace(b"\n", delimiter, 1)
            rejected(rebound(report, bad), bad, b"", mode)
        for bad_error in [b"AddressSanitizer: failure\n", b"runtime error: undefined behavior\n", b"leak report\n"]:
            rejected(rebound(report, stdout, bad_error), stdout, bad_error, mode)
        replacements = ([(b"225", b"224"), (b"225", b"226"), (b"225", b"0"), (b": ", b" ")]
                        if mode == "contracts" else
                        [(b"10485760", b"10485759"), (b"512 512", b"512 511"),
                         (b"1 1 1 1\nCLASS", b"1 1 1 0\nCLASS"),
                         (b"THREAD source-stop 1 0", b"THREAD source-stop 1 1")])
        for before, after in replacements:
            bad = normal.replace(before, after, 1)
            support.require(bad != normal, "semantic control changes an actual record")
            rejected(rebound(report, bad), bad, b"", mode)
    support.require(checks == 70, "complete bounded admission controls")
    support.require(support.source_binding() == bound and support.fingerprint(executable) == native,
                    "unchanged source/native executable through admission controls")
    result = {"status": "PASS", "controls": checks, "source_binding": bound,
              "executable": str(executable), "native_input": native, "captures": captures}
    (out / "report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": "PASS", "controls": checks, "work_directory": str(out)}))


if __name__ == "__main__":
    main()
