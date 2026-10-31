"""Run the typed CPU observer with strict current-source and process admission."""
import argparse
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tests/reference/client/render_resources"))
from capture import native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="source-cpu-", dir=args.work_dir.resolve()))
    native(args.executable, [], work, ROOT, "cpu")
    print(work)


if __name__ == "__main__":
    main()
