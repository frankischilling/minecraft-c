"""Run explicit local original/native NBT observations and their strict comparison."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

HERE = Path(__file__).resolve().parent


def need(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("executable", "client-jar", "libraries", "natives", "java", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[3])
    args = parser.parse_args()
    need(os.name == "nt", "the qualified unchanged-original profile requires Windows")
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        need(not os.environ.get(name), "ambient Java options are not admitted: " + name)
    source = args.source_root.resolve(strict=True)
    executable = args.executable.resolve(strict=True)
    java = args.java.resolve(strict=True)
    jar = args.client_jar.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    need(shutil.disk_usage(args.work_dir).free >= 6 * 1024**3, "6GiB local reference capacity required")
    work = Path(tempfile.mkdtemp(prefix="player-nbt-reference-", dir=args.work_dir.resolve()))
    capture_path = source / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("nbt_reference_capture", capture_path)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    inputs = [executable, java, java.with_name("javac.exe"), jar,
        capture_path, source / "tests/reference/client/notice/classfile.py",
        source / "tests/tools/entity/test_player_nbt.py", Path(__file__).resolve(), HERE / "run_original.py",
        HERE / "compare.py", HERE / "source-pins.json", HERE / "PlayerNbtObservation.java", HERE / "PlayerNbtPrefix.java",
        args.natives.resolve() / "lwjgl64.dll", *(args.libraries.resolve() / name for name in
            ("lwjgl-2.9.0.jar", "lwjgl_util-2.9.0.jar", "jinput-2.0.5.jar", "jutils-1.0.0.jar"))]
    before = {str(path): fingerprint(path) for path in inputs}
    report = {"status": "FAIL", "scope": "finite original/native NBT operations and separate wake93 prefix",
              "input_before": before, "work_directory": str(work)}
    env = dict(os.environ)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    env.pop("CLASSPATH", None)

    def run(label, command, timeout):
        need(shutil.disk_usage(work).free >= 6 * 1024**3, "6GiB capacity before " + label)
        result = capture.run_bounded(command, work, label, timeout=timeout, limit=4 * 1024 * 1024, env=env)
        c = result.capture_report
        report[label] = c
        need(type(c.get("exit")) is int and c["exit"] == 0 and c.get("reaped") is True and
            c.get("timed_out") is False and c.get("output_overflow") is False and
            c.get("pipe_capture_complete") is True and c.get("reader_errors") == {} and result.stderr == b"",
            "complete clean reference phase required: " + label)
        need(c == json.loads((work / (label + ".capture.json")).read_text()), "raw phase receipt equality")
        return result

    try:
        run("native", [sys.executable, "-B", source / "tests/tools/entity/test_player_nbt.py",
            "--executable", executable, "--source-root", source, "--work-dir", work / "native-runs"], 240)
        native_reports = list((work / "native-runs").glob("player-nbt-*/report.json"))
        need(len(native_reports) == 1, "one actual native report")
        run("original", [sys.executable, "-B", HERE / "run_original.py", "--client-jar", jar,
            "--libraries", args.libraries.resolve(), "--natives", args.natives.resolve(), "--java", java,
            "--source-root", source, "--work-dir", work / "original-runs"], 480)
        original_reports = list((work / "original-runs").glob("original-player-nbt-*/report.json"))
        need(len(original_reports) == 1, "one actual unchanged-original report")
        comparison = work / "comparison.json"
        run("comparison", [sys.executable, "-B", HERE / "compare.py", "--native-run", native_reports[0],
            "--executable", executable, "--original-run", original_reports[0], "--source-root", source,
            "--output", comparison], 90)
        result = json.loads(comparison.read_text())
        need(result["status"] == "PASS" and result["cases"] == 33 and result["exact_normal_tag_nodes"] == 1302 and
            result["wake93_prefix_tag_nodes"] == 33 and result["wake_full_call_parity"] is False,
            "complete declared finite comparison")
        report.update(status="PASS", cases=33, exact_normal_tag_nodes=1302, wake93_prefix_records=37,
            wake_full_call_parity=False, native_report={"path": str(native_reports[0]), "pin": fingerprint(native_reports[0])},
            original_report={"path": str(original_reports[0]), "pin": fingerprint(original_reports[0])},
            comparison=fingerprint(comparison))
    finally:
        report["input_after"] = {str(path): fingerprint(path) for path in inputs}
        if report["input_after"] != before:
            report["status"] = "FAIL"
        report["products"] = {path.name: fingerprint(path) for path in work.iterdir() if path.is_file()}
        (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    need(report["status"] == "PASS", "unchanged reference execution inputs required")
    print(json.dumps({"status": "PASS", "cases": 33, "exact_normal_tag_nodes": 1302,
                      "wake_full_call_parity": False, "work_directory": str(work)}))


if __name__ == "__main__":
    main()
