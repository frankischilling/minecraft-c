#!/usr/bin/env python3
"""Run plant callbacks on genuine GAME Worlds with disposable saved inputs."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    args.work_dir = Path(tempfile.mkdtemp(prefix="capture-", dir=args.work_dir)).resolve()
    with tempfile.TemporaryDirectory(prefix="plants-", dir=args.work_dir) as temporary:
        root = Path(temporary) / "world-Ã©-ä¸–ç•Œ"
        result = subprocess.run(
            [str(args.executable.resolve()), "world-plants", str(root).encode("utf-8").hex()],
            capture_output=True, timeout=80)
        (args.work_dir / "stdout.txt").write_bytes(result.stdout)
        (args.work_dir / "stderr.txt").write_bytes(result.stderr)
        text = result.stdout.decode("ascii").replace("\r\n", "\n")
        match = re.fullmatch(r"(?:16 achievements\n151 recipes\n){2}"
                            r"PASS: ([1-9][0-9]*) World plant checks \(world-plants\)\n", text)
        report = {"exit_code": result.returncode, "checks": int(match[1]) if match else None,
                  "original_comparison": False,
                  "passed": result.returncode == 0 and not result.stderr and match is not None}
        (args.work_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(text, end="")
        if result.stderr:
            print(result.stderr.decode("utf-8", errors="replace"))
        if not report["passed"]:
            print(f"FAIL: native plant process exit {result.returncode}; report {args.work_dir / 'report.json'}")
        return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
