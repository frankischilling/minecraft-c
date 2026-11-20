"""Run mining leaves and actual World auxiliary dispatch with authored inputs."""
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
    parser.add_argument("--mode", choices=("controller", "auxiliary"), required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.mode + "-", dir=args.work_dir)).resolve()
    world = output / "world"
    world.mkdir()
    before = digest(executable)
    if args.mode == "controller":
        resources = output / "resources"
        for name, data in {
            "lang/en_US.lang": (b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n"),
            "lang/stats_US.lang": b"# Authored statistics resource\n",
            "achievement/map.txt": b"",
        }.items():
            path = resources / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        arguments = [str(resources), str(world)]
        marker = r"PASS: ([1-9][0-9]*) mining prefix/leaf/ownership checks"
    else:
        arguments = ["world-auxiliary", str(world).encode("utf-8").hex()]
        marker = r"PASS: ([1-9][0-9]*) World auxiliary live-list/fire-order checks"
    result = subprocess.run([str(executable), *arguments], capture_output=True, timeout=120)
    (output / "stdout.txt").write_bytes(result.stdout)
    (output / "stderr.txt").write_bytes(result.stderr)
    report = {"mode": args.mode, "returncode": result.returncode,
              "executable_before": before, "executable_after": digest(executable)}
    match = re.search(marker, result.stdout.decode("utf-8", errors="strict"))
    report["status"] = "PASS" if result.returncode == 0 and match and before == report["executable_after"] else "FAIL"
    if match:
        report["checks"] = int(match[1])
    (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(result.stdout.decode("utf-8"), end="")
    if result.stderr:
        print(result.stderr.decode("utf-8"), end="")
    if report["status"] != "PASS":
        raise RuntimeError("Native mining component failed: " + str(output))
    print("Native mining " + args.mode + " PASS: " + str(output))


if __name__ == "__main__":
    main()
