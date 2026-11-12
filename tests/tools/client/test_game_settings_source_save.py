"""Run the typed Settings save/reload control in a fresh retained directory."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def pin(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    if not work.is_dir() or work.is_symlink():
        raise ValueError("Expected the configured owned work directory")
    root = Path(tempfile.mkdtemp(prefix="typed-settings-save-", dir=work))
    before = pin(binary)
    receipt = {"scope": "native typed Settings save/reload; no original observation",
               "binary_before": before, "root": str(root), "passed": False}
    try:
        with (root / "native.stdout").open("xb") as stdout, \
                (root / "native.stderr").open("xb") as stderr:
            child = subprocess.run([str(binary), str(root)], stdin=subprocess.DEVNULL,
                                   stdout=stdout, stderr=stderr, timeout=60, check=False)
        receipt["returncode"] = child.returncode
        if any((root / name).stat().st_size > 65536
               for name in ("native.stdout", "native.stderr")):
            raise ValueError("Typed Settings diagnostic output exceeded its bound")
        raw = (root / "native.stdout").read_bytes()
        errors = (root / "native.stderr").read_bytes()
        receipt["stdout"] = pin(root / "native.stdout")
        receipt["stderr"] = pin(root / "native.stderr")
        if child.returncode != 0 or errors:
            raise ValueError("Typed Settings native control failed")
        match = re.fullmatch(rb"Typed Settings save controls: ([1-9][0-9]*) checks\r?\n", raw)
        if match is None:
            raise ValueError("Expected the complete typed Settings control footer")
        receipt["native_checks"] = int(match.group(1))
        receipt["options"] = pin(root / "options.txt")
        receipt["passed"] = True
    except BaseException as error:
        receipt["failure"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            receipt["binary_after"] = pin(binary)
        except OSError as error:
            receipt["passed"] = False
            receipt["binary_check_error"] = str(error)
        else:
            if receipt["binary_after"] != before:
                receipt["passed"] = False
                receipt["binary_changed"] = True
        (root / "capture.json").write_text(json.dumps(receipt, indent=2) + "\n",
                                          encoding="utf-8", newline="\n")
    if not receipt["passed"]:
        raise ValueError("Typed Settings binary changed during the control")
    print(f"Typed Settings save: {receipt['native_checks']} native checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
