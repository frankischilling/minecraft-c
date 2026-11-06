"""Validate the complete finite original Block inventory matrix and source families."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
FILES = ("BlockInventoryObservation.java", "capture_reference.py", "inputs.json",
         "source-pins.json", "observations.tsv", "guards.tsv")
NATIVE_TAGS = ("BLOCK", "COLOR", "SHAPE", "TEXTURE", "TEXTURE_LATE")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def lines(path):
    raw = path.read_bytes()
    require(raw.endswith(b"\n") and b"\r" not in raw, "raw original LF transport")
    return raw.decode("ascii").splitlines()


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))
    require(provenance["client_jar_sha256"] == JAR and provenance["repeats"] == 2 and
            provenance["original_inputs_unchanged"] is True, "original authority/repeats")
    for name in FILES:
        require(digest(directory / name) == provenance["files_sha256"][name], "changed packet input: " + name)
    inputs = json.loads((directory / "inputs.json").read_text(encoding="utf-8"))
    metadata = inputs["metadata"]
    require(metadata[:16] == list(range(16)) and len(metadata) == len(set(metadata)) == 23 and
            all(isinstance(value, int) and -(1 << 31) <= value < 1 << 31 for value in metadata) and
            -(1 << 31) in metadata and (1 << 31) - 1 in metadata, "complete signed metadata inputs")
    require(inputs["shape_scenes"] == ["constructor", "authored-non-cube"] and
            inputs["texture_render_types"] == [0] and inputs["texture_sides"] == list(range(6)) and
            inputs["late_texture_field"] == [-3, 197] and
            inputs["late_metadata"] == [0, 3, -(1 << 31), (1 << 31) - 1], "finite input axes")
    authored = [struct.pack(">d", struct.unpack(">f", struct.pack(">f", value))[0]).hex()
                for value in inputs["authored_bounds_binary32"]]
    require(len(authored) == 6, "six authored bounds")
    pins = json.loads((directory / "source-pins.json").read_text(encoding="utf-8"))
    block_classes = pins["block_classes"]
    classes = pins["classes"]
    require(pins["client_jar_sha256"] == JAR and len(block_classes) == len(set(block_classes)) == 73 and
            set(classes) == set(block_classes) | {"jh"}, "complete original Block/JH class inventory")
    for name, detail in classes.items():
        require(detail["declared_methods_complete"] is True and
                re.fullmatch(r"[0-9a-f]{64}", detail["class_sha256"]) is not None, "complete class identity: " + name)
        methods = detail["methods"]
        require(len({(m["name"], m["descriptor"]) for m in methods}) == len(methods), "unique method inventory")
        for method in methods:
            if "code_sha256" in method:
                require(re.fullmatch(r"[0-9a-f]{64}", method["code_sha256"]) is not None and
                        method["code_bytes"] > 0, "exact method Code identity")

    def declaring(name, method, descriptor):
        visited = set()
        while name in classes and name not in visited:
            visited.add(name)
            if any(m["name"] == method and m["descriptor"] == descriptor for m in classes[name]["methods"]):
                return name
            name = classes[name]["superclass"]
        raise ValueError("unresolved original method family")

    declared_color = [name for name in block_classes if declaring(name, "b", "(I)I") == name]
    declared_shape = [name for name in block_classes if declaring(name, "g", "()V") == name]
    declared_bounds = [name for name in block_classes if declaring(name, "a", "(FFFFFF)V") == name]
    require(declared_color == ["bk", "uu"] and declared_shape == ["bv", "jq", "oi", "oq", "um", "uu"] and
            declared_bounds == ["uu"], "closed declaring body families")
    rows = lines(directory / "observations.tsv")
    cursor = 0
    native = []
    declarations = []
    cube_ids = []

    def row(tag, prefix, count):
        nonlocal cursor
        require(cursor < len(rows), "truncated complete matrix")
        literal = rows[cursor]
        cursor += 1
        fields = literal.split("\t")
        require(len(fields) == count and fields[:len(prefix) + 1] == [tag, *map(str, prefix)], "row schema/order: " + tag)
        if tag in NATIVE_TAGS:
            native.append(literal)
        return fields

    for identity in range(1, 97):
        decl = row("DECL", [identity], 6)
        require(decl[2] in block_classes and
                decl[3] == declaring(decl[2], "b", "(I)I") and
                decl[4] == declaring(decl[2], "g", "()V") and
                decl[5] == declaring(decl[2], "a", "(FFFFFF)V"), "actual original declaring owner")
        declarations.append(decl)
        block = row("BLOCK", [identity], 3)
        render_type = int(block[2])
        for value in metadata:
            color = row("COLOR", [identity, value], 4)
            require(re.fullmatch(r"[0-9a-f]{8}", color[-1]) is not None, "color raw bits")
        for scene in range(2):
            shape = row("SHAPE", [identity, scene], 15)
            require(all(re.fullmatch(r"[0-9a-f]{16}", value) for value in shape[3:]), "shape raw bits")
            if scene == 1:
                require(shape[3:9] == authored, "actual authored shape input")
        if render_type == 0:
            cube_ids.append(identity)
            for value in metadata:
                for side in range(6):
                    texture = row("TEXTURE", [identity, value, side], 5)
                    require(re.fullmatch(r"[0-9a-f]{8}", texture[-1]) is not None, "texture raw bits")
            for texture_field in inputs["late_texture_field"]:
                for value in inputs["late_metadata"]:
                    for side in range(6):
                        texture = row("TEXTURE_LATE", [identity, texture_field, value, side], 6)
                        require(re.fullmatch(r"[0-9a-f]{8}", texture[-1]) is not None, "late texture raw bits")
    require(cursor == len(rows) and len(cube_ids) == 57, "complete finite matrix end")
    guard = lines(directory / "guards.tsv")
    require(guard == ["BLOCK_INVENTORY_GUARD\t96\t57\t0\t0"], "source network/native attempts")
    require(len(rows) == provenance["rows"] and len(native) == provenance["native_rows"], "bound row counts")
    return {"rows": rows, "native_rows": native, "declarations": declarations,
            "cube_ids": cube_ids, "inputs": inputs}
