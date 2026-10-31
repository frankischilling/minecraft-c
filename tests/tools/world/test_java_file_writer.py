"""Bounded real-file controls for the canonical typed FOS/Writer stack."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    helper = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("typed_writer_capture", helper)
    require(spec is not None and spec.loader is not None, "existing bounded capture helper")
    support = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = support
    spec.loader.exec_module(support)
    paths = ["include/mc/world/storage/java_file_writer.h", "src/world/storage/java_file_writer.c",
             "include/mc/world/storage/java_output.h", "src/world/storage/java_output.c",
             "src/world/storage/java_output_internal.h", "include/mc/core/java/writer.h",
             "src/core/java/writer.c", "src/platform/files/byte_output.c",
             "tests/world/storage/test_java_file_writer.c", "tests/tools/world/test_java_file_writer.py"]
    before = {name: support.fingerprint(ROOT / name) for name in paths}
    executable = args.executable.resolve(strict=True)
    binary = support.fingerprint(executable)
    helper_pin = support.fingerprint(helper)
    work = support.fresh_directory(args.work_dir, "typed-writer-")
    files = work / "files"
    files.mkdir()
    command = [str(executable), str(files)]
    result = support.run_bounded(command, work, "native", timeout=120, limit=1048576)
    capture = result.capture_report
    try:
        require(type(result.returncode) is int and result.returncode == 0, "actual C success")
        require(type(capture.get("exit")) is int and capture["exit"] == 0, "captured success")
        require(capture.get("reaped") is True and capture.get("pipe_capture_complete") is True,
                "actual process/streams complete")
        require(capture.get("timed_out") is False and capture.get("output_overflow") is False and
                capture.get("reader_errors") == {}, "bounded capture")
        require(capture.get("command") == command, "exact actual argv")
        require(type(capture.get("stdout_bytes")) is int and
                capture["stdout_bytes"] == len(result.stdout), "complete stdout")
        require(type(capture.get("stderr_bytes")) is int and
                capture["stderr_bytes"] == 0 and result.stderr == b"", "empty actual stderr")
        require(re.fullmatch(rb"Actual typed FileWriter controls: [1-9][0-9]* checks\r?\n",
                             result.stdout) is not None, "exact authored footer")
        require(before == {name: support.fingerprint(ROOT / name) for name in paths} and
                binary == support.fingerprint(executable) and helper_pin == support.fingerprint(helper),
                "source/binary/helper unchanged")
    except Exception as error:
        raise ValueError(f"{error}\nstdout tail:\n{result.stdout[-8192:].decode('utf-8', 'replace')}"
                         f"\nstderr tail:\n{result.stderr[-8192:].decode('utf-8', 'replace')}") from error
    report = {"status": "PASS_AUTHORED_TYPED_WRITER_CONTROLS", "source": before,
              "executable": binary, "helper": helper_pin,
              "capture": support.fingerprint(work / "native.capture.json"),
              "stdout": support.fingerprint(work / "native.stdout"),
              "stderr": support.fingerprint(work / "native.stderr"),
              "files_after": {p.relative_to(files).as_posix(): support.fingerprint(p)
                              for p in sorted(files.rglob("*")) if p.is_file()},
              "original_JVM_calls": 0, "scope": "Authored actual-file contracts; no original parity claim"}
    (work / "REPORT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"typed FileWriter: PASS; retained {work}")


if __name__ == "__main__":
    main()
