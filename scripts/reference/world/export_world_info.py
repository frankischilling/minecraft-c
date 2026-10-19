#!/usr/bin/env python3
"""Export stable WorldInfo observations exclusively from a retained original capture."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RECORDER = ROOT / "scripts/reference/world/capture_world_info.py"
RECORDER_SHA256 = "9ddd373e45c8ad968cf7ac0164563b29d44552b1d1036338f5793565883d9812"


def fingerprint(path):
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    options = parser.parse_args()
    capture = options.capture_dir.resolve(strict=True)
    report = json.loads((capture / "capture.json").read_text(encoding="ascii"))
    require(report["status"] == "passed" and report["inputs_before"] == report["inputs_after"],
            "successful unchanged original capture required")
    require(fingerprint(RECORDER)["sha256"] == RECORDER_SHA256 ==
            report["inputs_before"]["recorder"]["sha256"], "pinned original recorder")
    spec = importlib.util.spec_from_file_location("world_info_original_export", RECORDER)
    recorder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recorder)
    require(report["inputs_before"]["helper"]["sha256"] == recorder.HELPER_SHA256,
            "reviewed original observer")
    require(all(report["inputs_before"][side]["sha256"] == digest
                for side, digest in recorder.JARS.items()), "original archive identities")
    require(report["compiled_classes"] == report["compiled_classes_after"] and
            all(fingerprint(capture / "classes" / name) == row
                for name, row in report["compiled_classes"].items()), "compiled class binding")
    require(len(report["children"]) == 3 and all(child["exit_code"] == 0 and child["reaped"] is True
                and child["stderr"]["bytes"] == 0 for child in report["children"]),
            "successful reaped original children")
    payloads = {}
    for ordinal, role in enumerate(("compile", "client", "server")):
        for stream in ("stdout", "stderr"):
            require(fingerprint(capture / (role + "." + stream)) == report["children"][ordinal][stream],
                    "complete retained child stream")
        if role == "compile":
            continue
        raw = (capture / (role + ".stdout")).read_bytes()
        counts, stable = recorder.parse(raw, role)
        require(counts == report["observations"][role]["counts"] and
                fingerprint(capture / (role + ".stdout")) == report["observations"][role]["stdout"] and
                fingerprint(capture / (role + ".stable.tsv")) == report["observations"][role]["stable"] and
                stable == (capture / (role + ".stable.tsv")).read_bytes(), "original-only projection")
        payloads["world_info_" + role + ".tsv"] = stable
    provenance = {
        "schema": "WORLD_INFO_ORIGINAL_GRAPH_V1",
        "scope": "Original WorldInfo fields, shared reference graph and no-list serializer",
        "capture_receipt": fingerprint(capture / "capture.json"),
        "inputs": report["inputs_before"],
        "compiled_classes": report["compiled_classes"],
        "children": [{"role": role, "exit_code": child["exit_code"], "reaped": child["reaped"],
                      "stdout": child["stdout"], "stderr": child["stderr"]}
                     for role, child in zip(("compile", "client", "server"), report["children"])],
        "observations": report["observations"],
        "transformation": "Remove only complete CLOCK_OUTPUT rows; retain all other bytes and identity IDs",
        "native_failure_mapping": "INVALID_FIELD at LevelName maps to the recorded IllegalArgumentException category",
        "limitations": report["limitations"] + [
            "Prepared original API scenarios, not natural World startup or generic mutable NBT",
            "No C output, sample clock replacement or inferred values supply expected fixture rows",
            "No original Java exception is thrown by the native status interface"],
    }
    payloads["world_info_provenance.json"] = (
        json.dumps(provenance, indent=2, sort_keys=True) + "\n").encode("ascii")
    destination = options.output_dir.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    require(all(not (destination / name).exists() for name in payloads), "fresh export targets required")
    for name, raw in payloads.items():
        with (destination / name).open("xb") as stream:
            stream.write(raw)
    print("Exported 427 client and 323 server original-derived stable rows; only clock rows excluded.")


if __name__ == "__main__":
    main()
