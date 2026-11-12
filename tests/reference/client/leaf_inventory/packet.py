"""Read the hash-bound finite Leaf inventory CPU observations."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))
    if provenance["client_jar_sha256"] != JAR or provenance["repeats"] != 2:
        raise ValueError("Leaf inventory original authority")
    for name in ("observations.txt", "source-pins.json", "LeafInventoryObservation.java", "capture_reference.py"):
        if digest(directory / name) != provenance["files_sha256"][name]:
            raise ValueError("Leaf inventory changed input: " + name)
    raw = (directory / "observations.txt").read_bytes()
    if b"\r" in raw or not raw.endswith(b"\n"):
        raise ValueError("Leaf inventory raw LF row transport")
    rows = raw.decode("ascii").splitlines()
    if len(rows) != 17:
        raise ValueError("Leaf inventory row inventory")
    for metadata, line in enumerate(rows[:16]):
        fields = line.split(" ")
        if len(fields) != 9 or fields[:2] != ["LEAF_INVENTORY_COLOR", str(metadata)] or not re.fullmatch(r"[0-9a-f]{8}", fields[2]):
            raise ValueError("Leaf metadata row schema")
        if not all(re.fullmatch(r"[0-9a-f]{16}", value) for value in fields[3:]):
            raise ValueError("Leaf metadata bounds schema")
    shape = rows[-1].split(" ")
    if len(shape) != 7 or shape[0] != "LEAF_INVENTORY_SHAPE" or not all(re.fullmatch(r"[0-9a-f]{16}", value) for value in shape[1:]):
        raise ValueError("Leaf authored shape schema")
    pins = json.loads((directory / "source-pins.json").read_text(encoding="utf-8"))
    if pins["client_jar_sha256"] != JAR or set(pins["classes"]) != {"bk", "nr", "uu", "jh"}:
        raise ValueError("Leaf source identity inventory")
    for name in ("bk", "nr"):
        detail = pins["classes"][name]
        if not detail["declared_methods_complete"] or any(m["name"] == "g" and m["descriptor"] == "()V" for m in detail["methods"]):
            raise ValueError("Leaf inherited no-op declaration authority")
    for name, method, descriptor in (("bk", "b", "(I)I"), ("uu", "g", "()V"),
                                      ("jh", "a", "()I"), ("jh", "b", "()I"),
                                      ("jh", "c", "()I"), ("jh", "<clinit>", "()V")):
        selected = [m for m in pins["classes"][name]["methods"] if m["name"] == method and m["descriptor"] == descriptor]
        if len(selected) != 1 or not re.fullmatch(r"[0-9a-f]{64}", selected[0]["code_sha256"]):
            raise ValueError("Leaf required Code identity")
    return rows
