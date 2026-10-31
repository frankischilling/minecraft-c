"""Exercise genuine local Player construction with authored resources."""

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
    output = args.work_dir.resolve() / ("run-" + uuid.uuid4().hex)
    resources = output / "resources"
    for name, data in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    world = output / "world"
    world.mkdir()
    command = [str(args.executable.resolve()), str(resources), str(world)]
    result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True,
                            timeout=60, check=False)
    (output / "native.stdout").write_bytes(result.stdout)
    (output / "native.stderr").write_bytes(result.stderr)
    lines = result.stdout.splitlines()
    count = re.fullmatch(rb"Concrete local Player construction checks passed: ([1-9][0-9]*)",
                         lines[-1]) if lines else None
    if result.returncode or result.stderr or len(lines) != 3 or \
            lines[:2] != [b"151 recipes", b"16 achievements"] or count is None:
        raise RuntimeError(f"Player construction failed; captures: {output}")
    pins = {name: hashlib.sha256(data).hexdigest() for name, data in RESOURCES.items()}
    if pins != {name: hashlib.sha256((resources / name).read_bytes()).hexdigest()
                for name in RESOURCES}:
        raise RuntimeError("Authored resources changed during Player construction")
    report = {"command": command, "exit_code": result.returncode,
              "checks": int(count.group(1)), "authored_resources": pins,
              "scope": "Actual constructor, Session, ownership and failed-prefix contracts"}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
