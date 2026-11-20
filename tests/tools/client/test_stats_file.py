"""Run the real local stats constructor/files/workers using authored resources."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile


def resources_at(root):
    for name, data in {
        "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
        "lang/stats_US.lang": b"# Authored statistics resource\n",
        "achievement/map.txt": b"",
    }.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def run(command, directory, prefix, timeout=120, limit=262144):
    # One reviewed capture implementation; load the sibling reference helper
    # by its exact package path, without adding ambient import search paths.
    path = Path(__file__).resolve().parents[2] / "reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("mc_stats_capture_support", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Stats capture helper unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.run_bounded(command, directory, prefix, timeout=timeout, limit=limit)
    if result.capture_report.get("reader_errors") != {}:
        raise RuntimeError("Stats capture reader errors")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = str(args.executable.resolve(strict=True))
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="stats-file-", dir=args.work_dir.resolve()))
    resources = root / "resources"
    resources_at(resources)
    world = root / "game"
    world.mkdir()
    result = run([executable, str(resources), str(world)], root, "native")
    lines = result.stdout.splitlines()
    catalog = re.fullmatch(rb"Stats catalog: ([1-9][0-9]*) entries, ([1-9][0-9]*) JSON units", lines[9]) if len(lines) == 26 else None
    count = re.fullmatch(rb"Client stats file: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    labels = [b"fresh", b"zero", b"wrapped", b"saved", b"async", b"pending", b"serviced"]
    rows = lines[2:9]
    valid_rows = len(rows) == 7 and all(row.split(b"\t")[1:2] == [label] and
        re.fullmatch(rb"STATS\t[a-z]+\t[0-9]+\t[0-9]+\t[01]\t[01]\t-?[0-9]+\t[01]\t[01]\t[01]\t[0-9]+", row)
        for row, label in zip(rows, labels))
    codec_results = [b"OK", b"ao", b"ao", b"java.util.NoSuchElementException", b"java.lang.IllegalStateException",
                     b"java.lang.NumberFormatException", b"java.lang.IllegalStateException", b"java.lang.NumberFormatException", b"OK", b"OK"]
    valid_codec = lines[10:20] == [b"CODEC\t" + str(i).encode("ascii") + b"\t" + value for i, value in enumerate(codec_results)]
    valid_hashes = len(lines[20:-1]) == 5 and all(re.fullmatch(rb"HASH\t" + str(i).encode("ascii") + rb"\t[0-9a-f]{1,32}", row) for i, row in enumerate(lines[20:-1]))
    if result.returncode != 0 or result.stderr or lines[:2] != [b"151 recipes", b"16 achievements"] or catalog is None or count is None or not valid_rows or not valid_codec or not valid_hashes:
        raise RuntimeError("Stats contract failed; captures: " + str(root))
    for label, command, diagnostic in (
        ("argc", [executable], b"Stats contract input: resources work-directory\n"),
        ("directory", [executable, str(resources), str(root / "absent")], b"Stats contract input: work-directory\n"),
    ):
        bad = run(command, root, label)
        if bad.returncode != 2 or bad.stdout or bad.stderr.replace(b"\r\n", b"\n") != diagnostic:
            raise RuntimeError("Stats malformed admission failed: " + label)
    report = {"status": "passed", "checks": int(count.group(1)),
              "reader_errors": result.capture_report["reader_errors"],
              "pipe_capture_complete": result.capture_report["pipe_capture_complete"],
              "reaped": result.capture_report["reaped"],
              "catalog_entries": int(catalog.group(1)), "catalog_json_units": int(catalog.group(2)),
              "exit": 0, "stderr_bytes": 0, "input_rejections": 2,
              "run_directory": str(root)}
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
