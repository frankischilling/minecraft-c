"""Run complete native/original maintenance comparisons in fresh owned directories."""

import argparse
import json
from pathlib import Path
import sys
import tempfile

from capture import capture, ensure, require_success
from run_native import validate_report

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="maintenance-comparison-", dir=args.work_dir.resolve()))
    commands = [
        ("native-driver", [sys.executable, str(HERE / "run_native.py"),
            "--executable", str(args.native.resolve()), "--output", str(root / "native")]),
        ("original-driver", [sys.executable, str(HERE / "run_original.py"),
            "--client-jar", str(args.client_jar.resolve()),
            "--libraries", str(args.libraries.resolve()),
            "--java", str(args.java.resolve()), "--javac", str(args.javac.resolve()),
            "--output", str(root / "original")]),
        ("comparison", [sys.executable, str(HERE / "compare.py"),
            "--original", str(root / "original/observations.json"),
            "--native-run", str(root / "native/run.json"),
            "--output", str(root / "comparison.json")]),
    ]
    for name, command in commands:
        result = capture(command, root, name, 210)
        require_success(result, empty_stderr=True)
    validate_report(root / "native/run.json")
    comparison = json.loads((root / "comparison.json").read_text())
    ensure(comparison["status"] == "PASS" and comparison["cases"] == 6 and
           comparison["matched_state_groups"] == 30 and comparison["chunk_state_rows"] == 104,
           "complete six-case maintenance comparison")
    print(json.dumps({"run_directory": str(root), "comparison": comparison}))


if __name__ == "__main__":
    main()
