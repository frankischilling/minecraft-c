"""Run a fresh native contract before comparing unchanged original methods."""
import argparse
import json
from pathlib import Path
import sys
import tempfile

from capture import capture, digest, ensure, require_success, validate_native_result


HERE = Path(__file__).resolve().parent


def main(arguments=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--mode", choices=["spawn", "position"], required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--natives", type=Path, required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--javac", required=True)
    parser.add_argument("--tested-commit", default="")
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args(arguments)
    options.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=options.mode + "-", dir=options.work_dir)).resolve()
    executable = options.executable.resolve(strict=True)
    mode = "spawn-reference" if options.mode == "spawn" else "position-probe"
    original_driver = (HERE / "capture_reference.py" if options.mode == "spawn" else
                       HERE.parent / "position/capture_position.py")
    inputs = [Path(__file__).resolve(), HERE / "run_native.py", HERE / "capture.py",
              original_driver, executable]
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "mode": options.mode, "tested_commit": options.tested_commit,
              "input_before_sha256": before}
    try:
        native_driver = capture(
            [sys.executable, "-B", str(HERE / "run_native.py"), "--executable", str(executable),
             "--mode", mode, "--output", str(output / "native")], output, "native-driver", 200)
        require_success(native_driver, empty_stderr=True)
        completion = validate_native_result(output / "native/native.stdout",
                                            output / "native/run.json", mode)
        report["native_driver"] = native_driver
        report["native_completion"] = completion
        original = capture(
            [sys.executable, "-B", str(original_driver), "--client-jar", str(options.client_jar.resolve()),
             "--libraries", str(options.libraries.resolve()), "--natives", str(options.natives.resolve()),
             "--java", options.java, "--javac", options.javac,
             "--native-stdout", str(output / "native/native.stdout"),
             "--native-report", str(output / "native/run.json"),
             "--output", str(output / "original")], output, "original-driver", 180)
        require_success(original, empty_stderr=True)
        observed = json.loads((output / "original/capture.json").read_text())
        ensure(observed["status"] == "PASS" and observed["native_compared"] is True,
               "original comparison did not complete")
        ensure(observed["native_completion"] == completion, "original used another native completion")
        ensure(observed["input_before_sha256"] == observed["input_after_sha256"] and
               observed["helper_before_sha256"] == observed["helper_after_sha256"],
               "original comparison inputs changed")
        expected = {"states": 42, "complete_calls": 34, "interrupted_prefixes": 8} if options.mode == "spawn" else {"records": 36}
        ensure(all(observed.get(key) == value for key, value in expected.items()),
               "original comparison inventory differs")
        report.update(status="PASS", original_driver=original, records=expected,
                      original_capture_sha256=digest(output / "original/capture.json"),
                      scope=observed["scope"])
    finally:
        after = {str(path): digest(path) for path in inputs}
        report["input_after_sha256"] = after
        if before != after:
            report["status"] = "FAIL"
        (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n",
                                                encoding="utf-8", newline="\n")
        ensure(before == after, "comparison inputs changed")
    print("Original " + options.mode + " comparison: PASS; report=" + str(output / "comparison.json"))


if __name__ == "__main__":
    main()
