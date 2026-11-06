#!/usr/bin/env python3
"""Export scalar formula observations; inferred draws never become constructor entropy."""

import argparse
import hashlib
import json
from pathlib import Path

from capture_entity import parse


def fingerprint(path):
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--provenance", type=Path, required=True)
    options = parser.parse_args()
    require(not options.fixture.exists() and not options.provenance.exists(), "fresh outputs required")
    capture = options.capture_dir.resolve(strict=True)
    receipt = json.loads((capture / "capture.json").read_text(encoding="ascii"))
    require(receipt["status"] == "passed" and receipt["inputs_before"] == receipt["inputs_after"],
            "successful unchanged-input original capture required")
    require(len(receipt["children"]) == 3 and all(child["exit_code"] == 0 and child["reaped"]
            and child["stderr"]["bytes"] == 0 for child in receipt["children"]), "complete child receipts")
    for name, pin in receipt["compiled_classes"].items():
        require(fingerprint(capture / "classes" / name) == pin, "compiled helper changed")
    lines = ["ITEM_FORMULA_OBSERVATION_V1"]
    observations = {}
    for side in ("client", "server"):
        original = capture / (side + ".stdout")
        require(fingerprint(original) == receipt["observations"][side]["stdout"], "original output changed")
        raw = original.read_bytes()
        observations[side] = {"output": fingerprint(original), "counts": parse(raw, side)}
        rows = [line.split("\t") for line in raw.decode("ascii").splitlines()]
        for case in range(5):
            case_rows = [row for row in rows if len(row) > 1 and row[1] == str(case)]
            draws = [row[3] for row in case_rows if row[0] == "INFERRED_DRAW"]
            fields = {(row[2], row[3]): row[7] for row in case_rows if row[0] == "FIELD"}
            values = [fields[key] for key in (("I", "d"), ("B", "aS"), ("B", "aP"),
                                               ("B", "aQ"), ("B", "aR"))]
            mode = "WORLD" if case == 0 else "FULL"
            lines.append("\t".join([side, str(case), mode] + draws + ["-"] * (4 - len(draws)) + values))
    lines.append("END\t10")
    fixture = ("\n".join(lines) + "\n").encode("ascii")
    provenance = {
        "schema": 1, "scope": "Ten original constructor scalar formula cases; 34 inferred draws and 50 observed fields",
        "inference": "Inputs reconstructed from actual post-Math states using source-proved call counts; no method-exit trace",
        "limits": "Formula replay only; no production constructor seed injection, clock/U observation, full-field or world parity",
        "capture_receipt": fingerprint(capture / "capture.json"),
        "capture_inputs": receipt["inputs_before"], "compiled_classes": receipt["compiled_classes"],
        "observations": observations, "fixture": {"bytes": len(fixture), "sha256": hashlib.sha256(fixture).hexdigest()},
        "exporter": fingerprint(Path(__file__).resolve()),
    }
    with options.fixture.open("xb") as stream:
        stream.write(fixture)
    with options.provenance.open("x", encoding="ascii", newline="\n") as stream:
        json.dump(provenance, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print("Exported 10 original scalar cases; Math inputs remain explicitly inferred.")


if __name__ == "__main__":
    main()
