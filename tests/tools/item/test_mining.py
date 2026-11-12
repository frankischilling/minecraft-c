"""Compare six actual mining results per Item/Block pair to the original capture."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / "tests/reference/item/mining"
sys.path.insert(0, str(PACKET))
from packet import read_packet


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="item-mining-", dir=args.work_dir)).resolve()
    resources, world = output / "resources", output / "world"
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
    executable = args.executable.resolve(strict=True)
    before = digest(executable)
    result = subprocess.run([str(executable), str(resources), str(world)], capture_output=True, timeout=120)
    (output / "stdout.txt").write_bytes(result.stdout)
    (output / "stderr.txt").write_bytes(result.stderr)
    lines = result.stdout.decode("utf-8", errors="strict").splitlines()
    actual = [line for line in lines if line.startswith(("MINING\t", "TOTAL\t"))]
    expected = read_packet(PACKET)
    if actual != expected:
        for index, (left, right) in enumerate(zip(actual, expected)):
            if left != right:
                raise AssertionError(f"mining row {index}: native={left!r}, original={right!r}")
        raise AssertionError(f"mining row count native={len(actual)}, original={len(expected)}")
    passed = result.returncode == 0 and before == digest(executable) and any(
        line.startswith("PASS: ") and line.endswith(" Item mining checks") for line in lines)
    (output / "run.json").write_text(json.dumps({
        "status": "PASS" if passed else "FAIL", "returncode": result.returncode,
        "executable_sha256": before, "rows": len(actual) - 1,
        "observations_sha256": digest(PACKET / "observations.tsv"),
    }, indent=2) + "\n", encoding="utf-8")
    if not passed:
        raise AssertionError("native Item mining controls failed: " + result.stderr.decode("utf-8"))
    print(f"PASS: {len(actual) - 1} exact original Item/Block mining rows; " + lines[-1])


if __name__ == "__main__":
    main()
