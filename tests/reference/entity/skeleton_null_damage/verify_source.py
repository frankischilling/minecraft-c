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
spec = importlib.util.spec_from_file_location("skeleton_damage_classfile", READER)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)

SELECTED = {
    "fr": [("j_", "()Ljava/lang/String;"), ("i", "()Ljava/lang/String;")],
    "gz": [("a", "(Lsn;I)Z"), ("w_", "()V")],
    "ii": [],
    "ls": [("a", "(Lsn;I)Z"), ("b", "(I)V"), ("b", "(Lsn;)V"),
           ("k", "()F"), ("U", "()V"), ("w_", "()V")],
    "sn": [("U", "()V")],
    "fd": [("a", "(Lsn;Z)V"), ("a", "(Lsn;Ljava/lang/String;FF)V")],
    "uz": [("j_", "()Ljava/lang/String;"), ("i", "()Ljava/lang/String;")],
}


def make_pins(jar):
    data = jar.read_bytes()
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

    result = {"scope": "Offline original class/Code inventory and resolved references only; no runtime observations.",
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
                                   "selected_methods": selected}
    damage = ("a", "(Lsn;I)Z")
    result["resolved_declarations"] = {
        "fr.a(Lsn;I)Z": declaring("fr", damage),
        "ii.a(Lsn;I)Z": declaring("ii", damage),
        "fr.j_()Ljava/lang/String;": declaring("fr", ("j_", "()Ljava/lang/String;")),
        "fr.i()Ljava/lang/String;": declaring("fr", ("i", "()Ljava/lang/String;")),
        "fr.k()F": declaring("fr", ("k", "()F")),
    }
    references = {}
    for name, key, pc in [("gz", damage, 3), ("sn", ("U", "()V"), 501),
                          ("ls", ("U", "()V"), 9),
                          ("fr", ("j_", "()Ljava/lang/String;"), 0),
                          ("fr", ("i", "()Ljava/lang/String;"), 0)]:
        instruction = next(i for i in method(name, key)["instructions"] if i["pc"] == pc)
        references[f"{name}.{key[0]}{key[1]}:{pc}"] = {
            "operation": instruction["name"], "reference": instruction["reference"]}
    result["selected_references"] = references
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--pins", type=Path, default=HERE / "source-pins.json")
    args = parser.parse_args()
    if not args.jar.is_file():
        print("UNAVAILABLE: original client archive is missing")
        return 77
    try:
        expected = json.loads(args.pins.read_text(encoding="utf8"))
        actual = make_pins(args.jar)
        if actual != expected:
            raise ValueError("Skeleton damage original source identities/references differ")
    except (AssertionError, OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print("FAIL: " + str(error))
        return 1
    print("PASS: Skeleton NULL damage original inventories, inheritance, Code and sound literals")
    return 0


if __name__ == "__main__":
    sys.exit(main())
