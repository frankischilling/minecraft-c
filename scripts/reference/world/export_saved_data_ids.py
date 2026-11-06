#!/usr/bin/env python3
"""Export the actual original memory observations, without deriving extra maps."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RECORDER = ROOT / "scripts/reference/world/capture_saved_data_ids.py"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def project(parsed):
    keys = parsed["keys"]
    request, expected = [], []
    previous = -1
    next_count = maps = values = 0
    for row in parsed["memory"]:
        kind = row[0]
        require(row[2] == "1", "one actual original memory owner")
        if kind == "NEW":
            require(row[1:] == ["null-store.new", "1", "0"] and not request, "memory constructor")
            request.append("NEW 0")
            expected.append("NEW\t0")
        elif kind == "NEXT":
            index = int(row[3])
            repeat = index - previous
            require(1 <= repeat <= 65538, "actual original draw index progression")
            key = keys[int(row[4])]
            argument = "NULL" if key is None else key if key else "EMPTY"
            request.append(f"NEXT 0 {repeat} {argument}")
            expected.append(f"RETURN\t0\t{repeat}\t{row[5]}")
            previous = index
            next_count += 1
        elif kind == "MAP":
            request.append("MAP 0")
            expected.append("MAP\t0\t" + row[3])
            maps += 1
        elif kind == "VALUE":
            key = keys[int(row[4])]
            argument = "NULL" if key is None else key if key else "EMPTY"
            expected.append(f"VALUE\t0\t{argument}\t{row[5]}")
            values += 1
        else:
            raise ValueError("unexpected memory observation")
    require((next_count, maps, values, previous + 1) == (15, 7, 15, 65543), "complete memory history")
    request.append("END")
    expected.append(f"END\t1\t{next_count}")
    return (("\n".join(request) + "\n").encode("ascii"),
            ("\n".join(expected) + "\n").encode("ascii"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "tests/reference/world")
    args = parser.parse_args()
    directory = args.capture.resolve(strict=True)
    report_path = directory / "capture.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    require(report["status"] == "passed" and report["inputs_before"] == report["inputs_after"],
            "successful unchanged-input original capture")
    require(fingerprint(RECORDER) == report["inputs_before"]["recorder"], "captured recorder unchanged")
    spec = importlib.util.spec_from_file_location("saved_ids_export_recorder", RECORDER)
    recorder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recorder)
    require(fingerprint(recorder.HELPER) == report["inputs_before"]["helper"] and
            report["inputs_before"]["helper"]["sha256"] == recorder.HELPER_SHA256,
            "actual observer source pin")
    require(report["classes_before"] == report["classes_after"] ==
            {path.name: fingerprint(path) for path in sorted((directory / "classes").glob("*.class"))},
            "actual compiled observer files")
    require(len(report["children"]) == 3 and all(row["exit_code"] == 0 and row["reaped"] and
            row["stderr"]["bytes"] == 0 for row in report["children"]), "natural completed children")
    payloads, observations = {}, {}
    for side, jar in recorder.JARS.items():
        require(report["inputs_before"][side]["sha256"] == jar, "pinned original archive")
        raw_path = directory / (side + ".stdout")
        world = directory / (side + "-worlds")
        parsed = recorder.parse(raw_path.read_bytes(), side, world)
        stored = report["observations"][side]
        require(all(stored[name] == value for name, value in parsed.items() if name != "keys") and
                stored["keys"] == {str(key): value for key, value in parsed["keys"].items()},
                "actual original transcript/report join")
        files = {path.relative_to(world).as_posix(): fingerprint(path)
                 for path in sorted(world.rglob("*")) if path.is_file()}
        require(files == stored["files"] and len(files) == 110, "complete retained original files")
        request, expected = project(parsed)
        payloads[f"saved_data_ids_{side}.commands"] = request
        payloads[f"saved_data_ids_{side}.tsv"] = expected
        observations[side] = {"raw_stdout": fingerprint(raw_path), "raw_rows": parsed["rows"],
                              "retained_files": len(files), "actual_next_calls": 65543,
                              "selected_returns": 15, "actual_maps": 7, "actual_values": 15,
                              "exported_rows": len(expected.splitlines())}
    provenance = {"schema": "SAVED_DATA_IDS_MEMORY_V1", "capture_receipt": fingerprint(report_path),
                  "inputs": report["inputs_before"], "compiled_classes": report["classes_before"],
                  "observations": observations,
                  "transformation": ["Select all original null-store NEW/NEXT/MAP/VALUE rows",
                                     "Map the sole original owner handle 1 to native command slot 0",
                                     "Use actual zero-based call-index differences for repeat counts; execute every omitted call",
                                     "Project UTF-16 key contents and nullability through observed key handles; omit String argument identity",
                                     "Preserve every observed signed return and all seven actual map snapshots; create no extra expected maps",
                                     "Frame commands and literal-LF native records without changing scalar or UTF-16 values"],
                  "limitations": ["Memory state/returns only; native copies key spans, without a Java String identity graph",
                                  "Original disk/diagnostic/session artifacts remain separately retained, without a native file-equivalence claim",
                                  "No named SavedData subtype, actual World startup or collection-dependent collision persistence acceptance"],
                  "fixtures": {name: {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
                               for name, raw in payloads.items()}}
    payloads["saved_data_ids_provenance.json"] = (json.dumps(provenance, indent=2, sort_keys=True) + "\n").encode("ascii")
    output = args.output_dir.resolve(strict=True)
    require(all(not (output / name).exists() for name in payloads), "fresh fixture filenames required")
    for name, raw in payloads.items():
        with (output / name).open("xb") as stream:
            stream.write(raw)
    print("Exported 15 original returns and seven actual memory maps per side; 39 literal rows each.")
    print(json.dumps({name: fingerprint(output / name) for name in payloads}, indent=2))


if __name__ == "__main__":
    main()
