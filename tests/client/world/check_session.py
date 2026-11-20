"""Run the real World owner on authored resources and a fresh test directory."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="session-", dir=args.output))
    resources = root / "resources"
    language = resources / "lang"
    language.mkdir(parents=True)
    (resources / "achievement").mkdir()
    (resources / "achievement" / "map.txt").write_text("", encoding="ascii")
    (language / "en_US.lang").write_text("tile.stone.name=Authored stone\n", encoding="ascii")
    (language / "stats_US.lang").write_text(
        "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
        "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
        encoding="ascii",
    )
    return subprocess.run([args.executable, str(resources), str(root / "world")], check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
