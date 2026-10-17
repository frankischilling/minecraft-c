#!/usr/bin/env python3
"""Export bounded light fixtures from successful complete original observations."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

from compare_light import JAR_HASHES, require, validate_record, regions


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def initializer(values):
    return "{" + ", ".join("INT32_MIN" if value == -(1 << 31) else str(value) for value in values) + "}"


def region(value):
    return "{" + str(value[0]) + ", " + initializer(value[1:]) + "}"


def generate(manifest):
    require(manifest.stat().st_size <= 4 * 1024 * 1024, "manifest size bound")
    report = json.loads(manifest.read_bytes())
    require(report["status"] == "pass" and not report["errors"] and
            report["mode"] == "original-only", "successful original-only capture required")
    for side, expected in JAR_HASHES.items():
        require(any(item["sha256"] == expected for item in report["inputs_before"].values()),
                side + " reference fingerprint missing")
    records = {}
    for side in ("client", "server"):
        path = manifest.parent / (side + "-record.json")
        require(path.stat().st_size <= 32 * 1024 * 1024 and
                digest(path) == report["sides"][side]["record"]["sha256"], "original record fingerprint")
        records[side] = validate_record(json.loads(path.read_bytes()), side)
    require(records["client"]["merges"] == records["server"]["merges"], "original merge disagreement")
    result = ["/* Generated original observations; no expected behavior is computed here.",
              " * Capture manifest SHA-256: " + digest(manifest),
              " * Client record SHA-256: " + report["sides"]["client"]["record"]["sha256"],
              " * Server record SHA-256: " + report["sides"]["server"]["record"]["sha256"],
              " */", "#ifndef MC_LIGHT_REFERENCE_FIXTURES_H", "#define MC_LIGHT_REFERENCE_FIXTURES_H", "",
              "typedef struct light_merge_fixture {", "    mc_light_bounds initial, requested, expected;",
              "    bool accepted;", "} light_merge_fixture;", "",
              "static const light_merge_fixture light_merge_fixtures[] = {"]
    for case in records["client"]["merges"]:
        before = case["initial"][1:]
        for request, step in zip(case["requests"], case["steps"]):
            result.append("    {" + initializer(before) + ", " + initializer(request) + ", " +
                          initializer(step["region"][1:]) + ", " + str(step["accepted"]).lower() + "},")
            before = step["region"][1:]
    result += ["};", "", "typedef struct light_queue_fixture {", "    mc_light_region request;",
               "    bool coalesce, admitted;", "    size_t count, changed_index;",
               "    mc_light_region changed;", "} light_queue_fixture;", ""]
    for side, value in records.items():
        previous = regions(value["queue"]["before_world"])
        require(0 < len(previous) <= 128, "public baseline fixture budget")
        result.append("static const mc_light_region light_" + side + "_baseline[] = {")
        result.extend("    " + region(entry) + "," for entry in previous)
        result += ["};", "", "static const light_queue_fixture light_" + side + "_steps[] = {"]
        for request, step in zip(value["queue"]["requests"], value["queue"]["steps"]):
            require(len(previous) <= len(step) <= len(previous) + 1, "bounded observed queue growth")
            changes = [i for i, entry in enumerate(step) if i >= len(previous) or previous[i] != entry]
            require(len(changes) <= 1, "at most one original region changes")
            index = str(changes[0]) if changes else "SIZE_MAX"
            changed = step[changes[0]] if changes else [0] * 7
            result.append("    {" + region(request["region"]) + ", " + str(request["coalesce"]).lower() +
                          ", " + str(request["admitted"]).lower() + ", " + str(len(step)) + ", " + index +
                          ", " + region(changed) + "},")
            previous = step
        result += ["};", ""]
    result += ["#endif", ""]
    return "\n".join(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        text = generate(args.manifest.resolve(strict=True))
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            output.write(text)
    except Exception as error:
        print("FAILURE: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
