"""Reject malformed authored-native camera traces; no JAR or original capture."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / "reference/client/camera/support.py"
    spec = importlib.util.spec_from_file_location("camera_support", path)
    if spec is None or spec.loader is None:
        raise ValueError("camera support")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="camera-controller-", dir=args.work_dir.resolve()))
    support.resources_at(root / "resources")
    actual = support.run([args.executable.resolve(strict=True), root / "resources", root / "world"], root, "native")
    support.native_rows(actual)
    data = actual.stdout
    controls = [
        (1, data, b""), (139, data, b"AddressSanitizer: crash\n"),
        (0, data, b"runtime error: undefined behavior\n"), (0, data, b"leak report\n"),
        (0, b"", b""), (0, data + b"extra\n", b""),
        (0, data.replace(b"151 recipes", b"152 recipes", 1), b""),
        (0, data.replace(b"CAMERA 0 0", b"CAMERA 0 1", 1), b""),
        (0, data.replace(b"CAMERA 1 0", b"CAMERA 0 0", 1), b""),
        (0, data.replace(b" 292 ", b" 293 ", 1), b""),
        (0, data.replace(b"OP 0 2 1 a 160", b"OP 0 2 1 a 161", 1), b""),
        (0, data.replace(b"PROJECTION 0", b"PROJECTION 1", 1), b""),
        (0, data.replace(b"ORIGIN 0", b"ORIGIN 1", 1), b""),
        (0, data.replace(b"CAMERA 0 0 - 0 6", b"CAMERA 0 0 - 0 5", 1), b""),
        (0, data.replace(b"MODELVIEW 0", b"MODELVIEW\x000", 1), b""),
        (0, data.replace(b"MODELVIEW 0", b"MODELVIEW\x800", 1), b""),
        (0, data[:-1] + b"\rX", b""), (0, data[:len(data)//2], b""),
        (0, data.replace(b" checks", b" check", 1), b""),
    ]
    for index, (code, stdout, stderr) in enumerate(controls):
        support.require(code != actual.returncode or stdout != data or stderr != b"", "mutation control changed")
        try:
            support.native_rows(subprocess.CompletedProcess([], code, stdout, stderr))
        except ValueError:
            continue
        raise ValueError("accepted malformed native trace " + str(index))
    report = {"status": "PASS", "checks": len(controls)+1, "native_checks": support.native_rows(actual)["checks"],
        "capture": actual.capture_report, "work_directory": str(root)}
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
