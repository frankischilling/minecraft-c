"""Strict finite Bed CPU-render rows; native ownership controls are separate."""

from pathlib import Path
import hashlib
import importlib.util
import json
import re
import zipfile

HERE = Path(__file__).resolve().parent
ARCHIVE = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
SOURCES = ("BedObservation.java", "capture.py", "packet.py", "finite.json", "source-pins.json",
           "../controller/capture.py", "../picking/observe.py", "../../../tools/client/test_picking.py",
           "../../entity/player/classfile.py")


def need(condition, message):
    if not condition:
        raise ValueError(message)


def rows(text):
    selected = [line for line in text.splitlines() if line.startswith("BED\t")]
    need(len(selected) == 192, "exact192 Bed render calls")
    for index, line in enumerate(selected):
        metadata, override_index, profile = index // 12, (index // 6) % 2, index % 6
        override = 17 if override_index else -1
        vertices = 12 if profile == 1 else 36 if profile == 4 else 30
        fields = line.split("\t")
        need(len(fields) == 11 and fields[:9] == ["BED", str(metadata), str(override),
             str(profile), "true", str(override), "false", "true" if profile == 3 else "false",
             str(vertices)], "ordered Bed call inputs/returned mutable fields")
        need(fields[9] == ",".join(["0000000000000000"] * 3 +
             ["3ff0000000000000", "3fe2000000000000", "3ff0000000000000"]),
             "actual VE.r bounds remain")
        need(re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){" + str(vertices * 8 - 1) + "}",
                          fields[10]) is not None, "complete ordered NW words")
    return selected


def read_packet(path):
    path = Path(path)
    need(path.stat().st_size <= 4 * 1024 * 1024, "finite packet byte ceiling")
    text = path.read_text(encoding="ascii")
    selected = rows(text)
    need(text == "\n".join(selected) + "\n", "packet contains only exact ordered rows")
    return selected


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


def verify_source_pins(jar):
    pins = read_json(HERE / "source-pins.json")
    need(pins["schema"] == "FINITE_CLASS_CODE_PINS_V1" and
         pins["archive"] == {"bytes": Path(jar).stat().st_size, "sha256": ARCHIVE} and
         digest(jar) == ARCHIVE, "unchanged Bed render archive")
    spec = importlib.util.spec_from_file_location("bed_render_code_pins", HERE.parents[1] / "entity/player/classfile.py")
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["classes"].items():
            actual = parser.parse_class(archive.read(name + ".class"))
            del actual["fields"]
            signatures = {(method["name"], method["descriptor"]) for method in expected["methods"]}
            actual["methods"] = [method for method in actual["methods"]
                                 if (method["name"], method["descriptor"]) in signatures]
            need(actual == expected, "actual original Bed class/Code pin: " + name)


def read_reference(directory=HERE):
    directory = Path(directory)
    provenance = read_json(directory / "provenance.json")
    need(provenance["schema"] == "BED_RENDER_CAPTURE_V1" and provenance["status"] == "PASS" and
         type(provenance["rows_per_vm"]) is int and provenance["rows_per_vm"] == 192 and
         type(provenance["ordinary_vms"]) is int and provenance["ordinary_vms"] == 2 and
         provenance["input_unchanged"] is True and provenance["helper_unchanged"] is True and
         provenance["archive_sha256"] == ARCHIVE, "Bed capture qualification")
    sources = provenance["source_sha256"]
    need(type(sources) is dict and set(sources) == set(SOURCES), "exact Bed capture source inventory")
    for name, pin in sources.items():
        need(type(pin) is str and re.fullmatch("[0-9a-f]{64}", pin) is not None and
             digest(directory / name) == pin, "captured Bed source changed: " + name)
    finite = read_json(directory / "finite.json")
    need(equal(finite, {"schema": "BED_RENDER_INPUT_V1", "metadata": list(range(16)),
         "overrides": [-1, 17], "profiles": ["air", "opaque-neighbors", "force-all-faces",
         "retained-AO-colors", "JJ-a-zero", "JJ-c-top-two"], "position": [8, 64, 8],
         "skylight_subtraction": 3, "initial_flip": True, "AO_colors": "(i+1)/16.0f; i=0..11"}),
         "exact finite Bed inputs")
    original = provenance["original"]
    need(type(original) is list and len(original) == 2, "two original Bed receipts")
    for receipt in original:
        need(type(receipt["exit"]) is int and receipt["exit"] == 0 and receipt["timed_out"] is False and
             receipt["pipe_capture_complete"] is True and receipt["reader_errors"] == {} and
             receipt["output_overflow"] is False and type(receipt["stdout_bytes"]) is int and
             0 < receipt["stdout_bytes"] <= 4 * 1024 * 1024 and
             type(receipt["stderr_bytes"]) is int and 0 <= receipt["stderr_bytes"] <= 1024 * 1024 and
             all(type(receipt[name]) is str and re.fullmatch("[0-9a-f]{64}", receipt[name]) is not None
                 for name in ("stdout_sha256", "stderr_sha256")),
             "bounded complete Bed original process")
    path = directory / "selected.txt"
    need(digest(path) == provenance["selected_sha256"], "Bed finite packet digest")
    need(provenance["selected_per_vm_sha256"] == [provenance["selected_sha256"]] * 2,
         "both original VMs selected this Bed packet")
    return read_packet(path)
