"""Bounded authored binary-file contracts; no original or JVM execution."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def pin(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    exe = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="binary-output-", dir=args.work_dir.resolve()))
    owned = work / "owned"
    # The real C File.mkdir/construct path creates owned; no output bytes or
    # source object is supplied by this controller.
    support_path = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("binary_output_capture", support_path)
    require(spec is not None and spec.loader is not None, "actual bounded capture helper")
    support = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = support
    spec.loader.exec_module(support)
    source_names = (
        "src/platform/files/byte_output.c", "src/platform/files/byte_output.h",
        "src/world/storage/java_output.c", "src/world/storage/java_output_internal.h",
        "src/world/storage/java_data_output.c", "include/mc/world/storage/java_output.h",
        "src/core/java/writer.c", "tests/world/storage/test_java_output.c",
        "tests/world/storage/java_output_native.c", "tests/tools/java/test_java_output.py")
    before = {name: pin(ROOT / name) for name in source_names}
    executable_before, helper_before = pin(exe), pin(support_path)
    command = [str(exe), str(owned)]
    result = support.run_bounded(command, work, "native", timeout=180, limit=8 * 1024 * 1024)
    capture = result.capture_report
    require(type(result.returncode) is int and result.returncode == 0, "actual successful exit")
    require(type(capture.get("exit")) is int and capture["exit"] == 0, "captured exit")
    for key in ("pipe_capture_complete", "reaped"):
        require(capture.get(key) is True, key)
    for key in ("timed_out", "output_overflow"):
        require(capture.get(key) is False, key)
    require(capture.get("reader_errors") == {}, "all readers complete")
    require(capture.get("command") == command, "exact executed argv")
    require(type(capture.get("stdout_bytes")) is int and capture["stdout_bytes"] == len(result.stdout),
            "complete stdout")
    require(type(capture.get("stderr_bytes")) is int and capture["stderr_bytes"] == 0 and
            result.stderr == b"", "complete empty stderr")
    footer = re.fullmatch(rb"JAVA_BINARY_OUTPUT_OK checks=([1-9][0-9]*) writes=([1-9][0-9]*) closes=([1-9][0-9]*)\r?\n",
                          result.stdout)
    require(footer is not None, "complete positive contract footer")
    require({name: pin(ROOT / name) for name in source_names} == before, "source unchanged")
    require(pin(exe) == executable_before and pin(support_path) == helper_before, "binary/helper unchanged")
    report = {"status": "AUTHORED_NATIVE_CONTRACT_PASS", "source_before": before,
              "source_after": before, "command": command, "capture": capture,
              "executable_before": executable_before, "executable_after": pin(exe),
              "capture_helper": helper_before, "counts": [int(x) for x in footer.groups()],
              "raw": {name: pin(work / name) for name in
                      ("native.stdout", "native.stderr", "native.capture.json")},
              "owned_files": {p.relative_to(owned).as_posix(): pin(p)
                              for p in sorted(owned.rglob("*")) if p.is_file()},
              "scope": "Finite binary output/source-close/native-retirement contracts; no Store/Logger/original parity"}
    (work / "REPORT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(work)


if __name__ == "__main__":
    main()
