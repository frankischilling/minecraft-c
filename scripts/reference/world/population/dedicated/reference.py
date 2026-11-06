#!/usr/bin/env python3
"""Compare fresh original and native dedicated construction, saves and reload.

The original-only audit supplies constructor clock inputs. The native process
receives those inputs and resources, never an expected World or Random state.
Build-to-source and platform qualification remain separate from this capture.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[5]
DIRECTORY = Path(__file__).resolve().parent


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("missing dedicated reference module: " + str(path))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client", "server", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    capture = load("_dedicated_reference_capture", DIRECTORY / "capture.py")
    audit = load("_dedicated_reference_audit", DIRECTORY / "audit.py")
    replay = load("_dedicated_reference_replay", DIRECTORY / "replay.py")
    comparison = load("_dedicated_reference_comparison", DIRECTORY / "comparison.py")
    natural = load("_dedicated_reference_resource_transport",
                   ROOT / "scripts/reference/world/population/compare_natural.py")
    process = capture.P
    work = None
    started = time.monotonic()
    report = {"schema": 2, "scope": "fresh original/native finite dedicated profile",
              "profile": "dedicated-natural-unload", "passed": False,
              "tested_commit": args.tested_commit or None, "processes": [],
              "qualification_complete": False, "whole_game_equivalence": False,
              "build_attribution_limit": "Selected executable, source and tool pins; build-to-source and full platform qualification are external checks."}
    exit_code = 1
    try:
        process.need(not args.tested_commit or re.fullmatch(r"[0-9a-f]{40}", args.tested_commit),
                     "complete tested commit")
        executable = process.ordinary(args.executable.absolute()).resolve(strict=True)
        jars = {side: process.ordinary(getattr(args, side).absolute()).resolve(strict=True)
                for side in ("client", "server")}
        for side, path in jars.items():
            process.need(process.fingerprint(path)["sha256"] == natural.HASHES[side],
                         "pinned original " + side)
        java = process.ordinary(args.java.absolute()).resolve(strict=True)
        javac = process.ordinary(args.javac.absolute()).resolve(strict=True)
        work = natural.CAPTURE.capture_root(args.work_dir,
            [Path(sys.executable).resolve(), java, javac])
        process.need(not executable.is_relative_to(work), "executable outside owned capture")
        paths = replay.input_paths(executable)
        paths.update(driver=Path(__file__).resolve(), client_jar=jars["client"], server_jar=jars["server"])
        for module in ("capture", "audit", "semantic", "process", "reference"):
            paths["dedicated/" + module] = DIRECTORY / (module + ".py")
        report["inputs_before"] = process.input_snapshot(paths)
        original_work, original_report = capture.run_capture(jars["server"], java, javac,
            work / "original", args.tested_commit)
        report["original_capture"] = {"path": str(original_work),
                                      "receipt": process.fingerprint(original_work / "report.json")}
        process.need(original_report.get("passed") is True, "original capture completed")
        report["source_context"] = original_report["source_before"]
        summary, tape = audit.audit_capture(original_work)
        audit_path, tape_path = work / "original-audit.json", work / "constructor-inputs.txt"
        audit.write_outputs(summary, tape, audit_path, tape_path, original_work)
        report["original_audit"] = {"path": audit_path.name, **process.fingerprint(audit_path)}
        report["constructor_inputs"] = {"path": tape_path.name, **process.fingerprint(tape_path)}
        resources = work / "resources"
        report["resources"] = natural.extract_resources(jars["client"], resources)
        resources_before = process.inventory(resources)
        process.need({name: {key: value[key] for key in ("bytes", "sha256")}
                      for name, value in resources_before.items() if value["kind"] == "file"}
                     == report["resources"], "exact supplied original resource files")
        output, world = work / "native.json", work / "native-world"
        native = replay.run_native(executable, resources, world, tape_path, output,
                                   work, report["processes"], environment=process.environment())
        report["native_final_disk"] = replay.validate_final_disk(native, world)
        original_raw = comparison.bounded_read(original_work / "original.json")
        trace = comparison.bounded_read(original_work / "observer.stdout", 16 * process.MIB)
        original = comparison.strict_json(original_raw)
        result = comparison.compare(original, native, trace, tape)
        result_path = work / "comparison.json"
        with result_path.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(result, stream, indent=2, allow_nan=False)
            stream.write("\n")
        report["comparison"] = {"path": result_path.name, **process.fingerprint(result_path)}
        report["native_output"] = process.fingerprint(output)
        report["original_semantic_audit_complete"] = True
        report["native_comparison"] = True
        report["inputs_after"] = process.input_snapshot(paths)
        process.need(report["inputs_before"] == report["inputs_after"], "selected comparison inputs unchanged")
        process.need(report["native_final_disk"] == replay.validate_final_disk(native, world),
                     "actual native final disk unchanged")
        process.need(comparison.bounded_read(tape_path, 65536) == tape,
                     "audited constructor tape unchanged through native execution")
        process.need(process.inventory(resources) == resources_before,
                     "exact extracted original resources unchanged through native execution")
        report["passed"] = True
        exit_code = 0
    except Exception as error:
        report["failure"] = f"{type(error).__name__}: {error}"
        print("FAIL: " + report["failure"], file=sys.stderr)
    finally:
        if work is not None:
            report["seconds"] = round(time.monotonic() - started, 3)
            try:
                report["files"] = process.inventory(work)
            except Exception as error:
                report["inventory_failure"] = str(error)
                report["passed"] = False
                exit_code = 1
            with (work / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
                json.dump(report, stream, indent=2, allow_nan=False)
                stream.write("\n")
            print("Capture: " + str(work), file=sys.stderr)
    if exit_code == 0:
        print("PASS: fresh dedicated construction, population, saves and unload/reload")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
