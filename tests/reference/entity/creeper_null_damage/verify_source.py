"""Verify private original classfiles offline; never load or execute a class."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
READER = HERE.parents[1] / "client/resource_lookup_common/offline_classfile.py"
spec = importlib.util.spec_from_file_location("creeper_damage_classfile", READER)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
SELECTED = {
    "gb": [("<init>", "(Lfd;)V"), ("j_", "()Ljava/lang/String;"), ("i", "()Ljava/lang/String;"), ("b", "(Lsn;)V")],
    "gz": [("a", "(Lsn;I)Z"), ("w_", "()V")],
    "ii": [],
    "ls": [("a", "(Lsn;I)Z"), ("b", "(I)V"), ("b", "(Lsn;)V"), ("k", "()F"), ("U", "()V"), ("w_", "()V")],
    "sn": [("U", "()V")],
}



def make_pins(jar):
    data = jar.read_bytes()
    if hashlib.sha256(data).hexdigest() != JAR:
        raise ValueError("unchanged supplied original archive SHA256")
    with zipfile.ZipFile(jar) as archive:
        classes = {name: reader.read_class(archive.read(name + ".class")) for name in SELECTED}

    def method(name, key):
        return next(m for m in classes[name]["methods"]
                    if (m["name"], m["descriptor"]) == key)

    def declaring(name, key):
        while name in classes:
            if any((m["name"], m["descriptor"]) == key for m in classes[name]["methods"]):
                return name
            name = classes[name]["super"]
        raise ValueError("unresolved selected method")

    result = {"schema": 1, "scope": "STATIC ONLY: offline original class/Code inventory and resolved references; no executed original damage or runtime observations.",
              "runtime_observation": False, "executed_original_damage_calls": 0,
              "jar_bytes": len(data), "jar_sha256": hashlib.sha256(data).hexdigest(), "classes": {}}
    for name, keys in SELECTED.items():
        cls = classes[name]
        inventory = [{k: m[k] for k in ("name", "descriptor", "access")} for m in cls["methods"]]
        selected = []
        for key in keys:
            m = method(name, key)
            selected.append({k: m[k] for k in ("name", "descriptor", "access", "code_bytes", "code_sha256",
                                               "code_attribute_sha256", "max_stack", "max_locals")})
        result["classes"][name] = {"class_bytes": cls["class_bytes"], "class_sha256": cls["class_sha256"],
                                   "superclass": cls["super"], "method_count": len(inventory),
                                   "method_inventory_sha256": hashlib.sha256(json.dumps(inventory, sort_keys=True,
                                                                                        separators=(",", ":")).encode()).hexdigest(),
                                   "method_inventory": inventory, "selected_methods": selected}
    damage = ("a", "(Lsn;I)Z")
    result["hierarchy"] = ["gb", "gz", "ii", "ls", "sn"]
    result["resolved_declarations"] = {
        "gb.a(Lsn;I)Z": declaring("gb", damage),
        "ii.a(Lsn;I)Z": declaring("ii", damage),
        "gb.j_()Ljava/lang/String;": declaring("gb", ("j_", "()Ljava/lang/String;")),
        "gb.i()Ljava/lang/String;": declaring("gb", ("i", "()Ljava/lang/String;")),
        "gb.k()F": declaring("gb", ("k", "()F")),
        "gb.b(Lsn;)V": declaring("gb", ("b", "(Lsn;)V")),
        "gz.b(Lsn;)V": declaring("gz", ("b", "(Lsn;)V")),
    }
    references = {}
    for name, key, pc in [("gz", damage, 3), ("sn", ("U", "()V"), 501),
                          ("ls", ("U", "()V"), 9), ("ls", ("U", "()V"), 66),
                          *[("ls", damage, pc) for pc in (284, 288, 311, 316, 332, 336, 359)],
                          ("gb", ("j_", "()Ljava/lang/String;"), 0),
                          ("gb", ("i", "()Ljava/lang/String;"), 0),
                          ("gb", ("b", "(Lsn;)V"), 2),
                          ("gb", ("b", "(Lsn;)V"), 6)]:
        instruction = next(i for i in method(name, key)["instructions"] if i["pc"] == pc)
        references[f"{name}.{key[0]}{key[1]}:{pc}"] = {"operation": instruction["name"], "reference": instruction["reference"]}
    result["selected_references"] = references
    windows = {}
    for name, key, pcs in [("gb", ("j_", "()Ljava/lang/String;"), (0,2)), ("gb", ("i", "()Ljava/lang/String;"), (0,2)),
                           ("gb", ("b", "(Lsn;)V"), (0,1,2)), ("ls", ("U", "()V"), (63,64,65,66,69)),
                           ("ls", ("k", "()F"), (0,1))]:
        windows[name+"."+key[0]+key[1]] = [{k:i[k] for k in ("pc","opcode","operand_hex")} for i in method(name,key)["instructions"] if i["pc"] in pcs]
    result["selected_instruction_windows"] = windows
    if hashlib.sha256(jar.read_bytes()).hexdigest() != result["jar_sha256"]:
        raise ValueError("archive changed during offline inspection")
    return result


def unique(pairs):
    value = dict(pairs)
    if len(value) != len(pairs):
        raise ValueError("duplicate JSON field")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--pins", type=Path, default=HERE / "source-pins.json")
    args = parser.parse_args()
    if not args.jar.is_file():
        print("UNAVAILABLE: original client archive is missing")
        return 77
    try:
        expected = json.loads(args.pins.read_text(encoding="utf8"), object_pairs_hook=unique)
        actual = make_pins(args.jar)
        if actual != expected:
            raise ValueError("Creeper damage original source identities/references differ")
    except (AssertionError, OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print("FAIL: " + str(error))
        return 1
    print("PASS: Creeper NULL damage original inventories, inheritance, Code and sound literals")
    return 0


if __name__ == "__main__":
    sys.exit(main())
