"""Run concrete crafting contracts on both sides using authored resources."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import uuid


RESOURCES = {
    "lang/en_US.lang": (
        b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
        b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n"
    ),
    "lang/stats_US.lang": b"# Authored statistics resource\n",
    "achievement/map.txt": b"",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve()
    output = args.work_dir.resolve() / ("run-" + uuid.uuid4().hex)
    resources = output / "resources"
    for name, data in RESOURCES.items():
        target = resources / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    pins = {name: hashlib.sha256(data).hexdigest() for name, data in RESOURCES.items()}
    report = {"authored_resources": pins, "sides": {}}
    for side in ("client", "server"):
        command = [str(executable), side, str(resources)]
        result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True,
                                timeout=45, check=False)
        (output / (side + ".stdout")).write_bytes(result.stdout)
        (output / (side + ".stderr")).write_bytes(result.stderr)
        lines = result.stdout.splitlines()
        count = re.fullmatch(rb"Concrete crafting checks passed: ([1-9][0-9]*)", lines[-1]) \
            if lines else None
        if result.returncode or result.stderr or len(lines) != 3 or \
                lines[:2] != [b"151 recipes", b"16 achievements"] or count is None:
            raise RuntimeError(f"{side} crafting contracts failed; captures: {output}")
        report["sides"][side] = {"command": command, "exit_code": result.returncode,
                                  "outer_checks": int(count.group(1))}
    if pins != {name: hashlib.sha256((resources / name).read_bytes()).hexdigest()
                for name in RESOURCES}:
        raise RuntimeError("Authored resources changed during native contracts")
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
