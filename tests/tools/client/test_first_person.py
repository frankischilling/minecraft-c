"""Run source-selected camera contracts on genuine LocalPlayer/World owners."""
import argparse
import importlib.util
import json
from pathlib import Path
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
    root = Path(tempfile.mkdtemp(prefix="first-person-", dir=args.work_dir.resolve()))
    support.resources_at(root / "resources")
    executable = args.executable.resolve(strict=True)
    result = support.run([executable, root / "resources", root / "world"], root, "native")
    observed = support.native_rows(result)
    bad = support.run([executable], root, "argc")
    support.require(bad.returncode == 2 and bad.stdout == b"" and
        bad.stderr.splitlines() == [b"First-person camera input: resources world-directory"], "exact argc rejection")
    report = {"status": "PASS", "checks": observed["checks"], "cases": len(observed["cases"]),
        "capture": result.capture_report, "argc": bad.capture_report, "work_directory": str(root)}
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
