"""Run authored World, Item and controller placement controls without assets."""
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
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--mode", choices=("world", "callbacks", "item", "controller"), required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.mode + "-", dir=args.work_dir)).resolve()
    world = output / "world"
    world.mkdir()
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
    if args.mode == "world":
        arguments = ["world-placement", str(world).encode("utf-8").hex()]
        marker = r"PASS: ([1-9][0-9]*) World placement source-order/ownership checks"
    elif args.mode == "callbacks":
        source = Path(__file__).resolve().parents[1] / "world/test_source_saving.py"
        spec = importlib.util.spec_from_file_location("placement_authored_save", source)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        metadata, region = fixture.seed("item-chest")
        saved = world / "world"
        (saved / "region").mkdir(parents=True)
        (saved / "level.dat").write_bytes(metadata)
        (saved / "region/r.0.0.mcr").write_bytes(region)
        arguments = ["world-placement-callbacks", str(world).encode("utf-8").hex()]
        marker = r"PASS: ([1-9][0-9]*) World placement live callback/retirement checks"
    elif args.mode == "item":
        arguments = [str(resources), str(world)]
        marker = r"PASS: ([1-9][0-9]*) Item use source-order/ownership checks"
    else:
        arguments = [str(resources), str(world)]
        marker = r"PASS: ([1-9][0-9]*) controller-use/RA-reset checks"
    before = digest(executable)
    result = subprocess.run([str(executable), *arguments], capture_output=True, timeout=150)
    (output / "stdout.txt").write_bytes(result.stdout)
    (output / "stderr.txt").write_bytes(result.stderr)
    report = {"mode": args.mode, "returncode": result.returncode,
              "executable_before": before, "executable_after": digest(executable)}
    match = re.search(marker, result.stdout.decode("utf-8", errors="strict"))
    report["status"] = ("PASS" if result.returncode == 0 and match and
                        before == report["executable_after"] else "FAIL")
    if match:
        report["checks"] = int(match[1])
    (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(result.stdout.decode("utf-8"), end="")
    if result.stderr:
        print(result.stderr.decode("utf-8"), end="")
    if report["status"] != "PASS":
        raise RuntimeError("Native placement component failed: " + str(output))
    print("Native placement " + args.mode + " PASS: " + str(output))


if __name__ == "__main__":
    main()
