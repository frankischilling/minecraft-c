#!/usr/bin/env python3
"""Export verified original observations unchanged, without original assets."""

import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-report", type=Path, required=True)
    args = parser.parse_args()
    recorder = load("biome_original_recorder", ROOT / "scripts/reference/world/capture_biome_registry.py")
    support = load("biome_export_process", recorder.PROCESS_HELPER)
    path = args.capture_report.resolve(strict=True)
    report = json.loads(path.read_text(encoding="ascii"))
    require(report["status"] == "passed" and report["three_modes_equal_per_side"] is True,
            "successful actual six-mode capture required")
    children = report["children"]
    require(len(children) == 9 and all(row["exit"] == 0 and row["reaped"] is True
            and row["timed_out"] is False and row["output_limited"] is False for row in children),
            "nine actual bounded/reaped successful children")
    require(report["inputs_before"] == report["inputs_after"], "unchanged captured inputs")
    for key, source in report["input_paths"].items():
        require(support.file_fingerprint(Path(source)) == report["inputs_before"][key],
                "original measured input changed: " + key)
    request_path = Path(report["input_paths"]["requests"])
    require(request_path.resolve() == (ROOT / "tests/reference/world/biome_registry_requests.tsv").resolve(),
            "canonical neutral declaration file")
    operations = recorder.declarations(request_path.read_bytes())
    outputs, modes, counts = {}, {}, {}
    for side in ("client", "server"):
        for mode in ("interpreted", "tier1", "default"):
            key = side + "-" + mode
            entry = report["observations"][key]
            source = Path(entry["protocol_path"]).resolve(strict=True)
            require(source.parent == path.parent, "actual capture-owned protocol")
            raw = source.read_bytes()
            pin = support.fingerprint(raw)
            require(pin == entry["protocol"] == entry["retained_protocol"], "retained actual protocol bytes")
            current_counts = recorder.protocol(raw, side, operations)
            require(current_counts == entry["counts"], "complete observed protocol")
            if side in outputs:
                require(raw == outputs[side], "three actual JVM modes agree")
            else:
                outputs[side], counts[side] = raw, current_counts
            modes[key] = pin
    fixture = ROOT / "tests/reference/world"
    destinations = {side: fixture / ("biome_registry_" + side + ".tsv") for side in outputs}
    provenance_path = fixture / "biome_registry_provenance.json"
    require(not provenance_path.exists() and all(not target.exists() for target in destinations.values()),
            "fresh fixture destinations")
    provenance = {
        "schema": "BIOME_REGISTRY_ORIGINAL_OBSERVATIONS_V1",
        "transformation": "Exact observed TAB/LF protocols; no game results computed or rows filtered",
        "capture_report": support.file_fingerprint(path),
        "capture_schema": report["schema"],
        "inputs": report["inputs_before"],
        "compiled_helper_classes": report["compiled_classes"],
        "mode_protocols": modes,
        "counts": counts,
        "declarations": report["declarations"],
        "actual_children": [{key: row[key] for key in
                             ("name", "exit", "reaped", "timed_out", "output_limited", "stdout", "stderr")}
                            for row in children],
        "limitations": report["limitations"] + [
            "All identities are graph-local first-seen logical references, not JVM addresses",
            "194 live reachable references exclude cleared records and other transitive bootstrap objects",
            "Native comparison maps NULL/index status kinds to observed exception names only",
            "These finite observations do not establish original full World startup, generation or population"],
    }
    for side, target in destinations.items():
        with target.open("xb") as stream:
            stream.write(outputs[side])
    with provenance_path.open("x", encoding="ascii", newline="\n") as stream:
        stream.write(json.dumps(provenance, indent=2, ensure_ascii=True) + "\n")
    print(json.dumps({target.name: support.file_fingerprint(target)
                      for target in (*destinations.values(), provenance_path, request_path)}, indent=2))


if __name__ == "__main__":
    main()
