"""Reject malformed Biped records/process evidence using fresh authored native output."""
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
    parser.add_argument("--native-probe", required=True, type=Path)
    parser.add_argument("--fp-probe", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / "reference/client/biped/support.py"
    spec = importlib.util.spec_from_file_location("biped_controls", source)
    if spec is None or spec.loader is None:
        raise ValueError("concrete Biped support")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="biped-driver-", dir=args.work_dir.resolve()))
    bound = support.source_binding()
    probes = [args.native_probe.resolve(strict=True), args.fp_probe.resolve(strict=True)]
    inputs = {str(p): support.fingerprint(p) for p in probes}
    checks = 0
    captures = {}

    def rejected(record, stdout, stderr, mode):
        nonlocal checks
        try:
            support.native_result(record, stdout, stderr, mode)
        except (ValueError, AssertionError, UnicodeError, KeyError, IndexError):
            checks += 1
        else:
            raise ValueError("accepted malformed process/record control")

    def rebound(record, stdout, stderr=b""):
        changed = copy.deepcopy(record)
        for name, data in [("stdout", stdout), ("stderr", stderr)]:
            changed[name + "_bytes"] = len(data)
            changed[name + "_sha256"] = hashlib.sha256(data).hexdigest()
        return changed

    for mode in ["cold", "pair", "pool", "notice", "contracts", "item", "biped"]:
        record = support.capture([probes[1 if mode in ["item", "biped"] else 0], mode], out,
                                 mode, environment=os.environ.copy())
        stdout = (out / record["stdout_file"]).read_bytes()
        stderr = (out / record["stderr_file"]).read_bytes()
        support.native_result(record, stdout, stderr, mode)
        checks += 1
        captures[mode] = record
        for key, value in [("exit", 1), ("exit", 139), ("pipe_capture_complete", False),
                           ("timed_out", True), ("output_overflow", True),
                           ("reader_errors", {"stdout": [{"phase": "write", "type": "OSError"}]}),
                           ("stdout_bytes", len(stdout) + 1), ("stdout_sha256", "0" * 64)]:
            changed = copy.deepcopy(record)
            changed[key] = value
            rejected(changed, stdout, stderr, mode)
        for bad in [b"", stdout + b"EXTRA\n", stdout[:len(stdout)//2], stdout.replace(b" ", b"\x00", 1)]:
            rejected(rebound(record, bad), bad, b"", mode)
        for bad_error in [b"AddressSanitizer: failure\n", b"runtime error: undefined behavior\n", b"leak report\n"]:
            rejected(rebound(record, stdout, bad_error), stdout, bad_error, mode)
        if mode in ["cold", "pair", "pool", "notice"]:
            for bad in [stdout.replace(b"ALIASES 72 216 1", b"ALIASES 72 216 0"),
                        stdout.replace(b"RANDOM unchanged 1", b"RANDOM unchanged 0"),
                        stdout.replace(b"VERTEX 0 0", b"VERTEX 0 1", 1),
                        stdout.replace(b"END clean", b"END failed")]:
                support.require(bad != stdout, "semantic mutation changed output")
                rejected(rebound(record, bad), bad, b"", mode)
    support.require(checks == 128, "all bounded driver controls")
    support.require(inputs == {str(p): support.fingerprint(p) for p in probes}
                    and bound == support.source_binding(), "source/executable control readback")
    report = {"status": "PASS", "controls": checks, "source_binding": bound,
              "native_inputs": inputs, "captures": captures}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": "PASS", "controls": checks, "work_directory": str(out)}))


if __name__ == "__main__":
    main()
