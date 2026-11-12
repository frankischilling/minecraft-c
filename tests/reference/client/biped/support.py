"""Bounded Biped evidence over the unchanged public Notice capture owner."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]


def require(value, message):
    if not value:
        raise ValueError(message)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, "concrete helper module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


capture_owner = load("biped_public_capture", HERE.parent / "notice/capture.py")
classfile = load("biped_public_classfile", HERE.parent / "notice/classfile.py")
protocol = load("biped_public_protocol", HERE / "protocol.py")


def fingerprint(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def source_binding():
    proof = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
    bound = {}
    for group in ["native_sources", "reference_files", "controller_files", "shared_helpers"]:
        actual = {name: fingerprint(REPO / name) for name in proof[group]}
        require(actual == proof[group], "complete source binding: " + group)
        bound[group] = actual
    bound["provenance"] = fingerprint(HERE / "provenance.json")
    return bound


def capture(command, output, stem, timeout=90, environment=None):
    return capture_owner.capture([str(x) for x in command], output, stem, timeout, environment)


def successful(report, stdout, stderr, empty_stderr=True):
    capture_owner.require_success(report, empty_stderr=empty_stderr)
    for name, data in [("stdout", stdout), ("stderr", stderr)]:
        require(report[name + "_bytes"] == len(data), "complete captured length")
        require(report[name + "_sha256"] == hashlib.sha256(data).hexdigest(), "captured digest")
        require(len(data) <= report[name + "_limit"], "bounded captured stream")
    return report


def streams(output, report, empty_stderr=True):
    stdout = (output / report["stdout_file"]).read_bytes()
    stderr = (output / report["stderr_file"]).read_bytes()
    successful(report, stdout, stderr, empty_stderr)
    return stdout, stderr


def native_result(report, stdout, stderr, mode):
    successful(report, stdout, stderr)
    if mode in ["cold", "pair", "pool", "notice"]:
        return protocol.parse(stdout, mode)
    lines = stdout.decode("ascii").splitlines()
    if mode == "contracts":
        require(lines == ["Biped contracts: 4381 checks;435 allocation prefixes"],
                "complete native contracts and allocation sweep")
    elif mode == "item":
        cells = lines[0].split() if len(lines) == 1 else []
        require(cells[:-1] == ["LEGACY_FP", "6", "ib", "7", "11", "8", "2", "0", "0", "0"]
                and cells[-1].isdigit() and int(cells[-1]) > 0,
                "actual delayed Item/Vec3 source failure prefix; positive native byte charge")
    else:
        require(mode == "biped" and lines == [
            "BIPED_FP delayed ib7 bt8; i29 j/k published; eight/six NULL slots; four nodes retained"],
                "complete standalone Biped delayed FP failure prefix")
    return lines


def read_native(output, report, mode):
    stdout = (output / report["stdout_file"]).read_bytes()
    stderr = (output / report["stderr_file"]).read_bytes()
    return native_result(report, stdout, stderr, mode)
