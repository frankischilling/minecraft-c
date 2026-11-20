"""Source-bound finite Player coordinate observations; no JVM required to read."""

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import zipfile

HERE = Path(__file__).resolve().parent
ARCHIVE = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
SOURCES = ("PlayerCoordinatesObservation.java", "capture_reference.py", "packet.py", "finite.json",
           "source-pins.json", "../player/classfile.py", "../../client/controller/capture.py")


def need(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        need(key not in value, "duplicate JSON key")
        value[key] = item
    return value


def read_json(path):
    path = Path(path)
    need(path.stat().st_size <= 4 * 1024 * 1024, "finite JSON byte ceiling")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)


def equal(left, right):
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(equal(left[key], right[key]) for key in left)
    if isinstance(left, list):
        return len(left) == len(right) and all(equal(a, b) for a, b in zip(left, right))
    return left == right


def rows(text):
    selected = [line for line in text.splitlines() if line.startswith("COORDINATE_")]
    need(len(selected) == 5, "exact coordinate method/placement row inventory")
    need(selected[0] == "COORDINATE_METHODS same=1 distinct=1 replaced-local=1 null=1",
         "actual identity/copy/null method completion")
    for index in range(3):
        need(re.fullmatch(r"COORDINATE_POSITION " + str(index) + r"(?: [0-9a-f]{16}){15}",
                          selected[index + 1]) is not None, "complete ordered pose/history/bounds row")
    need(selected[4] == "COORDINATE_POSITION_NULL source=NPE complete-pose=1",
         "actual null-bounds child prefix")
    return selected


def verify_source_pins(jar):
    pins = read_json(HERE / "source-pins.json")
    need(pins["schema"] == "FINITE_CLASS_CODE_PINS_V1" and
         pins["archive"] == {"bytes": Path(jar).stat().st_size, "sha256": ARCHIVE} and
         digest(jar) == ARCHIVE, "unchanged Player coordinate archive")
    spec = importlib.util.spec_from_file_location("coordinate_code_pins", HERE.parent / "player/classfile.py")
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = parser.parse_class(archive.read(name + ".class"))
            del actual["fields"]
            signatures = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in signatures]
            need(actual == expected, "actual original coordinate class/Code pin: " + name)


def read_reference(directory=HERE):
    directory = Path(directory)
    provenance = read_json(directory / "provenance.json")
    need(provenance["schema"] == "PLAYER_COORDINATES_CAPTURE_V1" and provenance["status"] == "PASS" and
         type(provenance["rows_per_vm"]) is int and provenance["rows_per_vm"] == 5 and
         type(provenance["ordinary_vms"]) is int and provenance["ordinary_vms"] == 2 and
         provenance["input_unchanged"] is True and provenance["helper_unchanged"] is True and
         provenance["archive_sha256"] == ARCHIVE, "Player coordinate capture qualification")
    sources = provenance["source_sha256"]
    need(type(sources) is dict and set(sources) == set(SOURCES), "exact coordinate capture sources")
    for name, pin in sources.items():
        need(type(pin) is str and re.fullmatch("[0-9a-f]{64}", pin) is not None and
             digest(directory / name) == pin, "captured coordinate source changed: " + name)
    finite = read_json(directory / "finite.json")
    need(equal(finite, {"schema": "PLAYER_COORDINATES_INPUT_V1",
         "vectors": [[4, 64, -7], [16777217, 16777217, -16777217], [2147483647, -2147483648, 2147483647]],
         "shape": ["0.6f", "1.8f", "1.62f", "0.125f"], "angles": [37, -17],
         "saved_spawn_input": [7, 8, 9], "null_bounds_call": [1, 2, 3, 4, 5]}),
         "exact declared coordinate inputs")
    original = provenance["original"]
    need(type(original) is list and len(original) == 2, "two original coordinate receipts")
    for receipt in original:
        need(type(receipt["exit"]) is int and receipt["exit"] == 0 and receipt["timed_out"] is False and
             receipt["pipe_capture_complete"] is True and receipt["reader_errors"] == {} and
             receipt["output_overflow"] is False and type(receipt["stdout_bytes"]) is int and
             0 < receipt["stdout_bytes"] <= 4 * 1024 * 1024 and
             type(receipt["stderr_bytes"]) is int and 0 <= receipt["stderr_bytes"] <= 1024 * 1024 and
             all(type(receipt[name]) is str and re.fullmatch("[0-9a-f]{64}", receipt[name]) is not None
                 for name in ("stdout_sha256", "stderr_sha256")),
             "bounded complete coordinate original process")
    path = directory / "selected.txt"
    need(path.stat().st_size <= 4 * 1024 * 1024 and digest(path) == provenance["selected_sha256"],
         "coordinate finite packet digest")
    need(provenance["selected_per_vm_sha256"] == [provenance["selected_sha256"]] * 2,
         "both original VMs selected this coordinate packet")
    text = path.read_text(encoding="ascii")
    selected = rows(text)
    need(text == "\n".join(selected) + "\n", "only selected coordinate rows")
    return selected
