"""Run real dynamic texture uploads against fresh authored resources."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="effect-upload-", dir=args.work_dir))
    directory = work / "game"
    directory.mkdir()
    result = subprocess.run([str(args.executable.resolve()), str(work / "resources"),
                             str(directory)], capture_output=True, timeout=120, check=False)
    (work / "stdout.txt").write_bytes(result.stdout)
    (work / "stderr.txt").write_bytes(result.stderr)
    if result.returncode != 0 or b"Actual dynamic texture uploads: " not in result.stdout:
        raise RuntimeError(f"dynamic texture upload failed ({result.returncode}): "
                           f"{result.stderr.decode(errors='replace')}; files: {work}")
    print(f"Actual dynamic texture upload passed; files: {work}")


if __name__ == "__main__":
    main()
