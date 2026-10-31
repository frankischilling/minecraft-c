"""Run the actual-App pickup fixture with authored resources and bounded capture."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import tempfile


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    reference = Path(__file__).resolve().parents[2] / "reference/client"
    capture = module("pickup_app_capture", reference / "statistics/support.py")
    resources_helper = module("pickup_app_resources", reference / "player_statistics/support.py")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="player-pickup-", dir=args.work_dir.resolve()))
    resources = out / "resources"
    resources_helper.resources_at(resources)
    game = out / "game"
    game.mkdir()
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    result = capture.run_bounded([str(executable), str(resources), str(game)],
                                 cwd=out, prefix="native", timeout=180, limit=2 * 1024 * 1024)
    lines = result.stdout.splitlines()
    marker = re.fullmatch(rb"Player pickup particles: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    if (result.returncode != 0 or result.stderr != b"" or len(lines) != 3 or
            lines[:2] != [b"16 achievements", b"151 recipes"] or marker is None):
        raise RuntimeError("actual-App particle fixture failed; retained captures: " + str(out))
    if hashlib.sha256(executable.read_bytes()).hexdigest() != before:
        raise RuntimeError("actual-App particle executable changed")
    report = {"status": "PASS", "checks": int(marker[1]), "capture": result.capture_report,
              "executable_sha256": before}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Player pickup particles: " + str(report["checks"]) + " checks; report " + str(out / "report.json"))


if __name__ == "__main__":
    main()
