#!/usr/bin/env python3
"""Run one fresh original/control lifecycle capture and its native comparison."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys

sys.dont_write_bytecode = True
NATIVE_PATH = Path(__file__).with_name("compare_lifecycle.py").resolve()
NATIVE_RAW = NATIVE_PATH.read_bytes()
if len(NATIVE_RAW) > 2 * 1024 * 1024:
    raise ValueError("bounded executing native support source")
NATIVE_SPEC = importlib.util.spec_from_file_location("_live_lifecycle_native", NATIVE_PATH)
native = importlib.util.module_from_spec(NATIVE_SPEC)
sys.modules[NATIVE_SPEC.name] = native
exec(compile(NATIVE_RAW, str(NATIVE_PATH), "exec"), native.__dict__)

ROOT = Path(__file__).resolve().parents[3]


def native_nonempty_refusal(capture, executable, work):
    """Verify the documented native restriction, not original activation parity."""
    work.mkdir()
    source = native.Original(native.strict_json(native.read(capture / "observed.json")))
    baseline, target = capture / "preparation/world", work / "native-world"
    before = native.inventory(baseline)
    shutil.copytree(baseline, target)
    native.require(native.inventory(target) == before == native.inventory(baseline), "exact nonempty baseline copy")
    records = []
    result = {"scope": "native unsupported nonempty activation policy; original activation succeeds",
              "processes": records, "passed": False}
    session = native.Session(executable, work, records)
    successful, settled = False, None
    try:
        record = source.record
        response = session.request("OPEN", str(target).encode("utf-8").hex(), record["initial"]["handler_token"],
                                   source.blob(record["block_map"]).hex(), 0,
                                   "dedicated" if record["context"] == "dedicated" else "base")
        native.require(native.exact(response["status"], 0), "native owner opens copied nonempty store")
        settled = native.raw_files(target)
        for name, raw in native.raw_files(baseline).items():
            native.require(name == "session.lock" or settled.get(name) == raw, "opening preserves every non-session file")
        native.require(set(settled) == set(before), "opening preserves complete nonempty file inventory")
        response = session.request("ACQUIRE", 16, 0)
        state = response["state"]
        native.require(response["category"] == "acquire" and native.exact(response["status"], 4) and response["returned_id"] is None and
                       native.exact(response["error"], {"status": 5, "operation": "opaque entity or tile activation", "system_error": 0}) and
                       response["callbacks"] == [] and native.exact(state["owned_count"], 0) and native.exact(state["reservation_count"], 0) and
                       state["chunks"] == state["map"] == state["list"] == state["pending"] == [],
                       "native refuses opaque activation before publication/registration/retained ownership")
        native.require(native.raw_files(target) == settled, "failed native activation preserves every copied file byte")
        result["files_after_failed_activation"] = native.inventory(target)
        successful = True
    finally:
        primary = sys.exc_info()[0] is not None
        errors = []
        try:
            session.finish(successful)
        except Exception as error:
            errors.append(error)
        try:
            native.require(settled is not None and native.raw_files(target) == settled, "refusal EOF never changes files")
            native.require(native.inventory(baseline) == before, "original nonempty preparation remains untouched")
        except Exception as error:
            errors.append(error)
        result["cleanup_errors"] = [str(error) for error in errors]
        result["passed"] = successful and not errors
        (work / "report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
        if errors and not primary:
            raise errors[0]
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "java", "javac", "executable", "callback-executable", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--context", choices=("client", "base", "dedicated"), required=True)
    parser.add_argument("--scenario", choices=("cohort", "unload101", "session-world", "detach-owned"), required=True)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args(argv)
    report = {"schema": 1, "scope": "fresh-original-controlled-lifecycle-and-native-replay", "status": "running",
              "context": args.context, "scenario": args.scenario, "tested_commit": args.tested_commit or None,
              "source_label": "Caller label; source archives and executable bytes are measured independently. No implicit Git/binary binding.",
              "errors": []}
    work, inputs = None, {}
    try:
        native.require(native.file_fingerprint(NATIVE_PATH) == native.fingerprint(NATIVE_RAW), "raw executing native support binding")
        native.require(not args.tested_commit or re.fullmatch("[0-9a-f]{40}", args.tested_commit), "complete lowercase source label")
        for name in ("client", "server", "java", "javac", "executable", "callback_executable"):
            if not getattr(args, name).is_file():
                print("UNAVAILABLE: lifecycle prerequisite " + name)
                return 77
        paths = set(native.source_paths(True)) | {Path(__file__).resolve(),
                ROOT / "scripts/reference/cache/export_lifecycle.py", ROOT / "scripts/reference/cache/compare_lifecycle_callbacks.py",
                ROOT / "tests/world/cache/lifecycle_callbacks_probe.c",
                ROOT / "tests/reference/cache/ChunkLifecycleObservation.java", ROOT / "tests/reference/cache/ChunkLifecycleController.java"}
        paths.update(path for path in (ROOT / "tests/world/cache").rglob("*") if path.suffix in (".c", ".h"))
        paths.update(path.resolve() for path in (args.executable, args.callback_executable, args.client, args.server,
                                               args.java, args.javac, Path(sys.executable)))
        inputs = {str(path): native.file_fingerprint(path) for path in sorted(paths)}
        work = native.fresh_work(args.work_dir, tuple(Path(path) for path in inputs))
        report["inputs_before"] = inputs
        native.require(inputs[str(NATIVE_PATH)] == native.fingerprint(NATIVE_RAW), "archived native source matches executed raw bytes")
        for path in paths:
            if path.is_relative_to(ROOT) and path.suffix in (".py", ".h", ".c", ".java", ".txt"):
                target = work / "source" / path.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(native.read(path, 2 * 1024 * 1024))
                native.require(native.file_fingerprint(target) == inputs[str(path)], "exact pipeline source archive")
        producer_path = ROOT / "scripts/reference/cache/export_lifecycle.py"
        producer = native.load("_live_lifecycle_producer", producer_path, expected=inputs[str(producer_path)])
        callbacks_path = ROOT / "scripts/reference/cache/compare_lifecycle_callbacks.py"
        callbacks = native.load("_live_lifecycle_callbacks", callbacks_path, expected=inputs[str(callbacks_path)])
        original_root = work / "original"
        arguments = ["--client", str(args.client.resolve()), "--server", str(args.server.resolve()),
                     "--java", str(args.java.resolve()), "--javac", str(args.javac.resolve()),
                     "--context", args.context, "--scenario", args.scenario, "--work-dir", str(original_root)]
        original_result = producer.main(arguments)
        captures = list(original_root.glob("capture-*"))
        native.require(len(captures) == 1, "one isolated original capture")
        capture = captures[0]
        report["original_manifest"] = {"path": str(capture / "manifest.json"), **native.file_fingerprint(capture / "manifest.json")}
        native.require(original_result == 0, "fresh original controlled capture must pass")
        executable = args.callback_executable if args.scenario == "detach-owned" else args.executable
        comparison = callbacks if args.scenario == "detach-owned" else native
        native_result = comparison.main(["--capture", str(capture), "--executable", str(executable.resolve()),
                                         "--work-dir", str(work / "native")])
        reports = list((work / "native").glob("capture-*/report.json"))
        native.require(len(reports) == 1, "one native comparison report")
        report["native_report"] = {"path": str(reports[0]), **native.file_fingerprint(reports[0])}
        native.require(native_result == 0, "strict native comparison must pass")
        if args.scenario == "detach-owned":
            report["native_nonempty_refusal"] = native_nonempty_refusal(capture, args.executable.resolve(), work / "native-refusal")
        report["status"] = "pass"
    except Exception as error:
        report["status"] = "fail"
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        if work is not None:
            report["inputs_after"] = {}
            for path, expected in inputs.items():
                try:
                    actual = native.file_fingerprint(Path(path))
                    report["inputs_after"][path] = actual
                    native.require(actual == expected, "pipeline input preserved: " + path)
                except Exception as error:
                    report["status"] = "fail"
                    report["errors"].append(str(error))
            path = work / "report.json"
            path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print(json.dumps({"status": report["status"], "report": str(path), "errors": report["errors"]}))
        elif report["status"] == "fail":
            print(json.dumps({"status": "fail", "errors": report["errors"]}), file=sys.stderr)
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
