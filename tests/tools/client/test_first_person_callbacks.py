"""Run real saved-Item camera/App/World callback contracts without a JAR."""
import argparse
import importlib.util
import json
from pathlib import Path
import tempfile
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / "reference/client/camera/support.py"
    spec = importlib.util.spec_from_file_location("camera_support", source)
    if spec is None or spec.loader is None:
        raise ValueError("camera support")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="camera-callbacks-", dir=args.work_dir.resolve()))
    support.resources_at(work / "resources")
    executable = args.executable.resolve(strict=True)
    result = support.run([executable, work / "resources", work / "world"], work, "native")
    observed = support.callback_rows(result)
    data = result.stdout
    controls = [(1, data, b""), (139, data, b"AddressSanitizer: crash\n"),
        (0, data, b"runtime error: undefined behavior\n"), (0, data + b"extra\n", b""),
        (0, data.replace(b"CALLBACK 0 0 0 9", b"CALLBACK 0 0 0 8", 1), b""),
        (0, data.replace(b"CALLBACK 3 3 203", b"CALLBACK 3 3 159", 1), b""),
        (0, data.replace(b"CALLBACK 7 4 15", b"CALLBACK 7 4 62", 1), b""),
        (0, data.replace(b"CBMODELVIEW 2", b"CBMODELVIEW 3", 1), b""),
        (0, data.replace(b"151 recipes", b"152 recipes", 1), b""),
        (0, data.replace(b"Camera callbacks:", b"Camera callback:", 1), b""),
        (0, data[:len(data)//2], b""), (0, b"", b"")]
    for index, (code, stdout, stderr) in enumerate(controls):
        support.require(code != result.returncode or stdout != data or stderr != b"", "changed callback control")
        try:
            support.callback_rows(subprocess.CompletedProcess([], code, stdout, stderr))
        except (ValueError, IndexError):
            continue
        raise ValueError("accepted malformed callback trace " + str(index))
    bad = support.run([executable], work, "argc")
    support.require(bad.returncode == 2 and bad.stdout == b"" and
        bad.stderr.splitlines() == [b"Camera callback input: resources world-directory"], "exact argc rejection")
    report = {"status": "PASS", "checks": observed["checks"], "controller_checks": len(controls)+2, "cases": len(observed["cases"]),
        "capture": result.capture_report, "argc": bad.capture_report, "work_directory": str(work)}
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
