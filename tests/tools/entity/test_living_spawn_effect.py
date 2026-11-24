"""Run authored Living spawn-effect inputs against the actual WorldAccess path."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def digest(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="living-spawn-effect-", dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("spawn_effect_resources", support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources = output / "resources"
    support.resources_at(resources)
    inputs = {str(path.relative_to(resources)): digest(path)
              for path in sorted(resources.rglob("*")) if path.is_file()}
    product = digest(executable)
    command = [str(executable), str(resources), str(output / "world")]
    child = subprocess.run(command, capture_output=True, timeout=220)
    (output / "stdout.txt").write_bytes(child.stdout)
    (output / "stderr.txt").write_bytes(child.stderr)
    expected = rb"151 recipes\r?\n16 achievements\r?\nPASS: ([1-9][0-9]*) Living spawn effect/source-order/ownership checks\r?\n"
    match = re.fullmatch(expected, child.stdout)
    after = {str(path.relative_to(resources)): digest(path)
             for path in sorted(resources.rglob("*")) if path.is_file()}
    unchanged = inputs == after and product == digest(executable)
    passed = child.returncode == 0 and not child.stderr and match is not None and unchanged
    receipt = {"command": command, "returncode": child.returncode,
               "stdout": digest(output / "stdout.txt"), "stderr": digest(output / "stderr.txt"),
               "executable": product, "resources": inputs, "inputs_unchanged": unchanged,
               "checks": int(match.group(1)) if match else None, "passed": passed,
               "scope": "Authored native inputs; no original VM or server runtime comparison."}
    (output / "run.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(child.stdout.decode("utf-8"), end="")
    print(child.stderr.decode("utf-8"), end="", file=sys.stderr)
    if not passed:
        raise RuntimeError("Living spawn effect controls failed: " + str(output))
    print("Living spawn effect controls passed: " + str(output))


if __name__ == "__main__":
    main()
