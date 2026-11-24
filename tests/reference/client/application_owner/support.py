"""Bounded native application-owner evidence; exact LF/CRLF transport only."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
EXPECTED_CHECKS = 225
ORIGINAL_ROWS = [b"CLASS arrays 10485760 512 512 1 1 1 1", b"CLASS fields 0 0 1 1",
                 b"THREAD zero 1 1 1 0", b"THREAD live 1 1 1 1", b"THREAD source-stop 1 0"]


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, "concrete capture module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


capture_owner = load("application_owner_capture", HERE.parent / "notice/capture.py")


def fingerprint(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def source_binding():
    proof = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
    bound = {}
    # All paths in this family's provenance are repository-relative.
    for group in ["native_sources", "reference_files", "controller_files", "shared_helpers"]:
        actual = {name: fingerprint(ROOT / name) for name in proof[group]}
        require(actual == proof[group], "complete source binding: " + group)
        bound[group] = actual
    bound["provenance"] = fingerprint(HERE / "provenance.json")
    return bound


def capture(command, output, stem, environment=None):
    return capture_owner.capture([str(value) for value in command], output, stem, 90, environment)


def rows(stdout, mode):
    require(type(stdout) is bytes, "actual captured bytes")
    normalized = stdout.replace(b"\r\n", b"\n")
    require(b"\r" not in normalized and normalized.endswith(b"\n"), "LF/CRLF complete lines")
    values = normalized[:-1].split(b"\n")
    if mode == "contracts":
        expected = [f"Application class/Timer hack contracts: {EXPECTED_CHECKS} checks".encode("ascii")]
    else:
        require(mode == "original", "explicit native profile")
        expected = ORIGINAL_ROWS
    require(values == expected, "complete native application-owner records")
    return values


def validate(report, stdout, stderr, mode):
    require(type(report["exit"]) is int and report["exit"] == 0, "real zero process exit")
    require(report["pipe_capture_complete"] is True and report["timed_out"] is False
            and report["output_overflow"] is False, "completed bounded process capture")
    require(type(report["reader_errors"]) is dict and report["reader_errors"] == {},
            "no capture reader failure")
    require(report["stdout_limit"] == capture_owner.STDOUT_LIMIT and
            report["stderr_limit"] == capture_owner.STDERR_LIMIT, "actual bounded stream limits")
    for name, data in [("stdout", stdout), ("stderr", stderr)]:
        require(type(report[name + "_bytes"]) is int and report[name + "_bytes"] == len(data),
                "complete captured stream length")
        require(report[name + "_sha256"] == hashlib.sha256(data).hexdigest(),
                "complete captured stream digest")
        require(len(data) <= report[name + "_limit"], "bounded actual captured stream")
    require(stderr == b"", "empty native stderr")
    return rows(stdout, mode)
