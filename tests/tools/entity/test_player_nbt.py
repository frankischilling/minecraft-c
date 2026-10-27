"""Bounded native Player NBT contracts on authored resources and fresh paths."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile


HERE = Path(__file__).resolve().parent


def need(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_capture(source):
    path = source / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("player_nbt_bounded_capture", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module, path


def require_process(result):
    need(result.returncode == 0, f"native exit {result.returncode}")
    need(result.stderr == b"", "native stderr is nonempty")
    report = result.capture_report
    need(report["exit"] == 0 and report["pipe_capture_complete"] is True,
         "native capture incomplete or failed")
    need(report.get("reader_errors") == {}, "native reader errors or missing reader receipt")
    need(report["timed_out"] is False and report["output_overflow"] is False and
         report["reaped"] is True, "native capture termination/bound")
    need(report["stdout_bytes"] == len(result.stdout) and report["stderr_bytes"] == 0,
         "native capture byte inventory differs")


def completion(text, kind):
    lines = text.splitlines()
    need(bool(lines), "empty native output")
    prefix = "Player NBT" if kind == "nbt" else "Player NBT coordinates"
    match = re.fullmatch(re.escape(prefix) + r": ([1-9][0-9]*) checks", lines[-1])
    need(match is not None, "missing positive final native marker")
    count = int(match[1])
    if kind == "coordinates":
        need(len(lines) >= 2 and lines[-2] == f"Player active tick: {count} checks",
             "actual active fixture completion/count missing")
    return count


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--kind", choices=("nbt", "coordinates"), default="nbt")
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[2])
    args = parser.parse_args(argv)
    source = args.source_root.resolve()
    support, support_path = load_capture(source)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="player-nbt-", dir=args.work_dir.resolve()))
    resources = root / "resources"
    for name, value in {
        "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
        "lang/stats_US.lang": b"# Authored statistics resource\n",
        "achievement/map.txt": b"",
    }.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
    mode = "player-nbt" if args.kind == "nbt" else "player-active"
    executable = args.executable.resolve()
    inputs = (executable, Path(__file__).resolve(), support_path)
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "kind": args.kind, "input_before_sha256": before}
    env = os.environ.copy()
    command = [str(executable), mode, str(root / "worlds").encode().hex(),
               str(resources).encode().hex()]
    try:
        result = support.run_bounded(command, cwd=root, prefix="native", timeout=180,
                                     limit=2 * 1024 * 1024, env=env)
        report["capture"] = result.capture_report
        require_process(result)
        count = completion(result.stdout.decode("ascii", "strict"), args.kind)
        report.update(status="PASS", checks=count)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        after = {str(path): digest(path) for path in inputs}
        report["input_after_sha256"] = after
        if before != after:
            report["status"] = "FAIL"
            report["input_changed"] = True
        report["products"] = {path.name: digest(path) for path in sorted(root.iterdir())
                              if path.is_file()}
        (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    need(report["status"] == "PASS", "native input integrity failure")
    print(f"Player NBT contracts: {count} checks; report {root / 'report.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
