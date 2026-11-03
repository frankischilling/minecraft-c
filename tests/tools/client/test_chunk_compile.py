"""Check DK completion, actual Sand/Cactus lists, reuse and retained failure."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    spec = importlib.util.spec_from_file_location("dk_authored_world", HERE / "test_picking.py")
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="chunk-compile-", dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = out / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(out / "world")
    result = subprocess.run([str(executable), str(out / "resources"), str(out / "world")],
                            env=os.environ.copy(), capture_output=True, text=True, timeout=100)
    (out / "native.stdout").write_text(result.stdout, encoding="utf-8", newline="\n")
    (out / "native.stderr").write_text(result.stderr, encoding="utf-8", newline="\n")
    expected = json.loads((HERE.parents[1] / "reference/client/chunk_compile/finite.json").read_text())["rows"]
    actual = [line for line in result.stdout.splitlines() if line.startswith("DK_EMPTY ")]
    cactus = [line for line in result.stdout.splitlines() if line.startswith("DK_CACTUS ")]
    cactus_expected = [
        "DK_CACTUS stage=compile block=81 last-scanned=0 cv-return=65 pass=0 vertices=66 inset-vertices=24 dirty=0 pass0-empty=0 pass1-empty=1",
        "DK_CACTUS stage=reuse attempts=0 same-definition=1 vertices=66",
        "DK_CACTUS stage=dirty block=81 last-scanned=0 cv-return=65 pass=0 vertices=66 inset-vertices=24 dirty=0 pass0-empty=0 pass1-empty=1",
    ]
    if (result.returncode or result.stderr or actual != expected or cactus != cactus_expected or not result.stdout.splitlines()
            or not result.stdout.splitlines()[-1].startswith("Source DK: ")):
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    print(result.stdout.splitlines()[-1])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
