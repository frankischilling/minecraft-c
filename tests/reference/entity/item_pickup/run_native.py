"""Asset/JVM-free CTest driver for genuine connected Item pickup owners."""
import argparse
import json
import re
import tempfile
from pathlib import Path
from support import digest, require, resources_at, run


def native(executable, out, reference=False):
    resources, game = out / "resources", out / "game"
    resources_at(resources)
    game.mkdir()
    before = digest(executable)
    result = run([executable.resolve(strict=True), resources.resolve(), game.resolve(),
                  "reference" if reference else "native"], out, "native")
    require(digest(executable) == before, "native executable changed")
    lines = result.stdout.splitlines()
    require(lines[:2] == [b"16 achievements", b"151 recipes"], "actual constructor banners")
    match = re.fullmatch(rb"Item pickup: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    require(match is not None, "native final positive check count missing")
    if not reference:
        require(len(lines) == 3, "unexpected native output")
    report = {"status": "PASS", "checks": int(match[1]), "capture": result.capture_report,
              "executable_sha256": before, "stdout_sha256": digest(out / "native.stdout"),
              "stderr_sha256": digest(out / "native.stderr")}
    (out / "native.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    return lines, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="item-pickup-", dir=args.work_dir.resolve()))
    _, report = native(args.executable, out)
    print("Item pickup: " + str(report["checks"]) + " checks; report=" + str(out / "native.json"))


if __name__ == "__main__":
    main()
