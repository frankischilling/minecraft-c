"""Run authored typed-File Stats components with fresh, owned fixture roots."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--kind", choices=("files", "overlap", "app"), required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    helper = Path(__file__).resolve().with_name("test_stats_file.py")
    spec = importlib.util.spec_from_file_location("mc_stats_source_existing_capture", helper)
    if spec is None or spec.loader is None:
        raise RuntimeError("Stats existing authored/capture helper unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="stats-source-" + args.kind + "-", dir=args.work_dir.resolve()))
    resources = root / "resources"
    module.resources_at(resources)
    game = root / "game"
    game.mkdir()
    command = [str(executable), str(resources), str(game)]
    if args.kind == "app":
        command.append("--typed-files")
    result = module.run(command, root, "native", timeout=180)
    footers = {"files": b"Stats source files", "overlap": b"Stats File overlap", "app": b"Stats source App"}
    lines = result.stdout.splitlines()
    count = re.fullmatch(re.escape(footers[args.kind]) + rb": ([1-9][0-9]*) checks", lines[-1]) if lines else None
    expected = ([b"16 achievements", b"151 recipes"] * 3 if args.kind == "app"
                else [b"151 recipes", b"16 achievements"])
    if (result.returncode != 0 or result.stderr or count is None or lines[:-1] != expected
            or result.capture_report.get("reader_errors") != {}
            or result.capture_report.get("pipe_capture_complete") is not True
            or result.capture_report.get("reaped") is not True):
        raise RuntimeError("Stats source failed; retained captures: " + str(root))
    report = {"kind": args.kind, "status": "passed", "checks": int(count.group(1)),
              "exit": result.returncode, "stderr_bytes": len(result.stderr),
              "run_directory": str(root), "capture": result.capture_report}
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
