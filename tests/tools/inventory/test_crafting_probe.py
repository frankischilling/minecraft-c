#!/usr/bin/env python3
"""Check accepted plan boundaries and malformed native crafting-probe input."""

import argparse
import importlib.util
import json
from pathlib import Path
import re

REFERENCE = Path(__file__).resolve().parents[2] / "reference/inventory/crafting"
SPEC = importlib.util.spec_from_file_location("crafting_reference_support", REFERENCE / "support.py")
support = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(support)

ENGLISH = (b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    run = support.fresh_directory(args.work_dir, "protocol-")
    resources = run / "authored-resources"
    for name, data in {"lang/en_US.lang": ENGLISH, "lang/stats_US.lang": b"synthetic=one\n",
                       "achievement/map.txt": b"1000,synthetic\n"}.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    suffix = b"\t2\t0\t-\t-\t-\t-"
    valid = {
        "lf": b"valid" + suffix + b"\n",
        "crlf": b"valid" + suffix + b"\r\n",
        "eof": b"valid" + suffix,
        "maximum-row": b"x" * (1023 - len(suffix)) + suffix + b"\n",
        "signed-counts": b"signed\t2\t0\t5,0,0,1\t-\t5,-3,0,2\t-\n",
        "graph-only": b"",
    }
    bad = {
        "embedded-nul": b"valid" + suffix + b"\x00unread-data\n",
        "blank": b"\n",
        "bare-cr": b"valid" + suffix + b"\r",
        "tab-only": b"\t\t\n",
        "control": b"valid" + suffix + b"\x01\n",
        "non-ascii": b"v\xc3\xa9" + suffix + b"\n",
        "overlong": b"x" * (1024 - len(suffix)) + suffix + b"\n",
        "width": b"bad\t4\t0\t-\t-\t-\t-\n",
        "negative-recipe": b"bad\t2\t-1\t-\t-\t-\t-\n",
        "unknown-recipe": b"bad\t2\t151\t-\t-\t-\t-\n",
        "integer-overflow": b"bad\t2147483648\t0\t-\t-\t-\t-\n",
        "missing-cell": b"bad\t2\t0\t-\t-\t-\n",
        "extra-cell": b"bad\t2\t0\t-\t-\t-\t-\t-\n",
        "group-zero": b"bad\t2\t0\t5,1,0,0\t-\t-\t-\n",
        "group-high": b"bad\t2\t0\t5,1,0,10\t-\t-\t-\n",
        "tuple-extent": b"bad\t2\t0\t5,1,0,1,9\t-\t-\t-\n",
        "stack-overflow": b"bad\t2\t0\t5,2147483648,0,1\t-\t-\t-\n",
        "conflicting-alias": b"bad\t2\t0\t5,1,0,1\t5,2,0,1\t-\t-\n",
    }
    results = {}
    for side in ("client", "server"):
        side_dir = run / side
        side_dir.mkdir()
        graph_prefix = None
        for name, data in {**valid, **bad}.items():
            plan = side_dir / (name + ".tsv")
            plan.write_bytes(data)
            result = support.run_bounded([executable, side, resources, plan], side_dir, name, 30)
            lines = result.stdout.splitlines()
            if name in valid:
                support.require(result.returncode == 0 and not result.stderr and lines[-1:] == [b"END"],
                                side + "/" + name + " valid protocol")
                support.require(lines[:3] == [b"151 recipes", b"16 achievements",
                                             b"CRAFTING_TRACE_V1\t" + side.encode()], "probe banners")
                if name == "graph-only":
                    graph_prefix = lines[:-1]
            else:
                diagnostics = result.stderr.splitlines()
                support.require(result.returncode == 1 and diagnostics and
                                all(re.fullmatch(rb"trace:[1-9][0-9]*: [ -~]+", line)
                                    for line in diagnostics),
                                side + "/" + name + " unexpected rejection/sanitizer diagnostics")
                support.require(graph_prefix is not None and lines == graph_prefix,
                                side + "/" + name + " incomplete setup or emitted malformed case")
            results[side + "/" + name] = {"exit_code": result.returncode,
                                            "accepted": name in valid,
                                            "plan": support.fingerprint(plan)}
    report = {"status": "passed", "run_directory": str(run), "cases": len(results),
              "results": results, "scope": "Native harness protocol; no replacement gameplay"}
    (run / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
