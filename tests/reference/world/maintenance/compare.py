"""Compare fresh authored native and unchanged-original maintenance output."""
import argparse
import json
from pathlib import Path
from run_native import validate_report
from capture import ensure


def compare(original, native_run):
    report, native = validate_report(native_run)
    expected = json.loads(original.read_text())
    observed = {}
    groups = 0
    for line in native.read_text().splitlines():
        cells = line.split("\t")
        if cells[0] == "case":
            observed[cells[1]] = {k: int(v) for k, v in (c.split("=", 1) for c in cells[2:])}
        elif cells[0] in ["states", "remaining"]:
            observed[cells[1]][cells[0]] = json.loads(cells[2])
    ensure(len(expected) == len(observed) == 6, "six complete cases required")
    for row in expected:
        actual = observed[row["case"]]
        for key in ["cache", "pending", "states", "remaining"]:
            ensure(row[key] == actual[key], (row["case"], key, row[key], actual[key]))
            groups += 1
        status = {"return": 0, "java.lang.NullPointerException": 4, "us": 3}[row["outcome"]]
        ensure(actual["status"] == status, row["case"])
        groups += 1
    return {"status": "PASS", "cases": 6, "matched_state_groups": groups,
            "native_exit": report["exit"], "native_stderr_bytes": report["stderr_bytes"],
            "native_positive_final_checks": report["checks"],
            "chunk_state_rows": sum(len(row["states"]) for row in expected),
            "controlled_inputs": "Only pending101/missing/lock/item use actual owned Set.add; natural empty/cached profiles remain separate. No normal provider enqueue operation is inferred."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--native-run", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = compare(args.original, args.native_run)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
