"""Run actual Skeleton NULL-attacker damage and World fire-tick controls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="skeleton-", dir=args.work_dir)).resolve()
    resources = output / "resources"
    world = output / "world"
    world.mkdir()
    for name, data in {
        "lang/en_US.lang": (b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                            b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n"),
        "lang/stats_US.lang": b"# Authored statistics resource\n",
        "achievement/map.txt": b"",
    }.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    before = digest(executable)
    result = subprocess.run([str(executable), str(resources), str(world)],
                            capture_output=True, timeout=150)
    (output / "stdout.txt").write_bytes(result.stdout)
    (output / "stderr.txt").write_bytes(result.stderr)
    match = re.search(r"PASS: ([1-9][0-9]*) Skeleton NULL damage source-order/ownership checks",
                      result.stdout.decode("utf-8", errors="strict"))
    report = {"returncode": result.returncode, "executable_before": before,
              "executable_after": digest(executable),
              "scope": "Authored actual-owner controls; no original runtime comparison"}
    report["status"] = ("PASS" if result.returncode == 0 and match and
                        before == report["executable_after"] else "FAIL")
    if match:
        report["checks"] = int(match[1])
    (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(result.stdout.decode("utf-8"), end="")
    if result.stderr:
        print(result.stderr.decode("utf-8"), end="")
    if report["status"] != "PASS":
        raise RuntimeError("Native Skeleton damage control failed: " + str(output))
    print("Native Skeleton NULL damage PASS: " + str(output))


if __name__ == "__main__":
    main()
