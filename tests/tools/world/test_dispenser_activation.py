"""Run authored loaded-World Dispenser activation and ownership controls."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def admit(stdout, stderr, code):
    if type(stdout) is not bytes or type(stderr) is not bytes or type(code) is not int:
        raise ValueError("Incomplete native transport")
    if code != 0 or stderr:
        raise ValueError("Native failure or nonempty stderr")
    separator = b"\r\n" if b"\r" in stdout else b"\n"
    if not stdout.endswith(separator):
        raise ValueError("Incomplete native output")
    lines = stdout.split(separator)[:-1]
    # This loaded-World fixture reaches achievement initialization before the
    # crafting registry. Keep both Source rows in their observed order.
    if len(lines) != 3 or lines[:2] != [b"16 achievements", b"151 recipes"]:
        raise ValueError("Unexpected Registry output")
    match = re.fullmatch(rb"Dispenser activation CPU controls passed: ([1-9][0-9]*)", lines[2])
    if match is None or int(match[1]) > 0xffffffff:
        raise ValueError("Missing activation footer")
    return int(match[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    path = ROOT / "tests/reference/client/controller/capture.py"
    spec = importlib.util.spec_from_file_location("dispenser_activation_capture", path)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="activation-", dir=args.work_dir.resolve()))
    world = output / "world"
    world.mkdir()
    executable = args.executable.resolve(strict=True)
    inputs = [Path(__file__).resolve(), path, executable,
              ROOT / "tests/world/runtime/dispenser/test_activation.c",
              ROOT / "tests/world/runtime/collision/test_boxes.c",
              ROOT / "include/mc/world/runtime/placement.h",
              ROOT / "src/world/runtime/placement.c",
              ROOT / "src/world/runtime/placement/dispenser.inc"]

    def pins():
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}

    report = {"status": "FAIL", "scope": __doc__, "inputs_before": pins()}
    try:
        receipt = capture.capture([str(executable), "world-dispenser-activation",
                                   str(world).encode("utf-8").hex()], output, "native", 150)
        report["capture"] = receipt
        capture.require_success(receipt, empty_stderr=True)
        stdout = (output / receipt["stdout_file"]).read_bytes()
        stderr = (output / receipt["stderr_file"]).read_bytes()
        for name, raw in (("stdout", stdout), ("stderr", stderr)):
            if (len(raw) != receipt[name + "_bytes"] or
                    capture.digest(output / receipt[name + "_file"]) != receipt[name + "_sha256"]):
                raise ValueError("Captured stream changed: " + name)
        report["checks"] = admit(stdout, stderr, receipt["exit"])
        report["inputs_after"] = pins()
        if report["inputs_before"] != report["inputs_after"]:
            raise ValueError("Activation inputs changed")
        report["status"] = "PASS"
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "checks": report["checks"],
                      "report": str(output / "report.json")}))


if __name__ == "__main__":
    main()
