"""Java-free audit of the finite original pressure plate inventory scalars."""

import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
CLASS_HASHES = {
    "uu": "caeaf354357d66fc168c7f03bf62bb6c6a1fe459e9be6b537d4d5d1efabd0845",
    "bv": "a74a0ec39d7dd2457dd0aac07474a81f9dc6bd216b91bc0b2545a71e1bc0903e",
}
CODE = {
    ("bv", "<init>", "(IILrt;Lln;)V"): (43, "4c96fe6d5c9768e8f81f150d6ee3e15a72a286d44ab8d10cc2f5362c5ca0d221"),
    ("bv", "g", "()V"): (38, "58029da5c1211df6c27e907cead287bd0bf8ef90a4ead5b15a1482d6c008f870"),
    ("uu", "b", "(I)I"): (3, "2f9ee7b099791bbd12a602f3d8b9fed56298f342a523435e75334295b0ff95e9"),
    ("uu", "b", "()I"): (2, "42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e"),
    ("uu", "a", "(II)I"): (6, "f20c3e09b28f823cc21d11124611ce6c00ee8c6ff2401080810b345ed3650fdf"),
    ("uu", "a", "(I)I"): (5, "5e5d14f709d860f1cdea2e6c150d802146d44c7760ea4cc48128430ca05d07ba"),
    ("uu", "a", "(FFFFFF)V"): (40, "4022b4fb5d99c5ff178933d6d3791eb65f2b27f004404fffbeff2803cc444d28"),
}
INPUTS = [-2147483648, -1, 0, 1, 15, 2147483647]
CONSTRUCTOR = ["3fb0000000000000", "0000000000000000", "3fb0000000000000",
               "3fee000000000000", "3fa0000000000000", "3fee000000000000"]
INVENTORY = ["0000000000000000", "3fd8000000000000", "0000000000000000",
             "3ff0000000000000", "3fe4000000000000", "3ff0000000000000"]
OWNERS = {"color": "uu", "render_type": "uu", "shape": "bv", "texture_metadata": "uu",
          "texture_side": "uu", "bounds": "uu"}
ROW_KEYS = {"field", "id", "runtime_class", "superclass", "owners", "class_sha256",
            "registered_identity", "render_type", "texture_field", "constructor_bounds",
            "after_scalar_bounds", "color_samples", "texture_samples", "inventory_bounds",
            "repeated_inventory_bounds"}


def require(condition, message):
    if not condition:
        raise AssertionError("pressure plate inventory: " + message)


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def loads(text):
    return json.loads(text, object_pairs_hook=unique)


def load(name):
    return loads((HERE / name).read_text(encoding="utf-8"))


def rows(stdout):
    lines = stdout.splitlines()
    require(len(lines) == 4 and lines[:2] == ["16 achievements", "151 recipes"],
            "complete ordinary original stdout and class-init banners")
    prefix = "PRESSURE_PLATE_INVENTORY "
    require(all(line.startswith(prefix) for line in lines[2:]), "exact scalar record markers")
    return [loads(line[len(prefix):]) for line in lines[2:]]


def audit(finite, observed, pins):
    require(finite["schema"] == observed["schema"] == pins["schema"] == 1, "schema")
    require(finite["client_jar_sha256"] == pins["client_jar_sha256"] == JAR, "original JAR")
    require(observed["ordinary_a_stdout"] == observed["ordinary_b_stdout"],
            "two fresh original VM observations agree")
    require(observed["ordinary_a_stderr"] == observed["ordinary_b_stderr"] == "",
            "no caught original source failure or stderr")
    actual = rows(observed["ordinary_a_stdout"])
    require(finite["rows"] == actual, "finite rows are direct actual stdout observations")
    require(type(actual) is list and len(actual) == 2, "both actual pressure plate receivers")
    for row, field, identity, texture in zip(actual, ["aL", "aN"], [70, 72], [1, 4]):
        require(set(row) == ROW_KEYS, "complete bounded row schema")
        require(row["field"] == field and type(row["id"]) is int and row["id"] == identity and
                row["runtime_class"] == "bv" and row["superclass"] == "uu" and
                row["registered_identity"] is True, "genuine registered receiver identity")
        require(row["owners"] == OWNERS and row["class_sha256"] == CLASS_HASHES,
                "actual inherited and overriding method owners and class resources")
        require(type(row["render_type"]) is int and row["render_type"] == 0 and
                type(row["texture_field"]) is int and row["texture_field"] == texture,
                "actual type0 and live inherited texture field")
        require(row["constructor_bounds"] == row["after_scalar_bounds"] == CONSTRUCTOR,
                "constructor bounds survive scalar color/texture reads")
        require(row["inventory_bounds"] == row["repeated_inventory_bounds"] == INVENTORY,
                "real BV.g changes shared six bounds and repeat preserves result")
        colors = row["color_samples"]
        require(type(colors) is list and len(colors) == 6, "all signed metadata color samples")
        for sample, metadata in zip(colors, INPUTS):
            require(set(sample) == {"metadata", "color"} and
                    type(sample["metadata"]) is type(sample["color"]) is int and
                    sample == {"metadata": metadata, "color": 0xffffff},
                    "inherited constant color for exact signed metadata")
        textures = row["texture_samples"]
        require(type(textures) is list and len(textures) == 36, "complete side/metadata matrix")
        expected = [{"side": side, "metadata": metadata, "value": texture}
                    for metadata in INPUTS for side in INPUTS]
        require(textures == expected and all(type(value) is int for sample in textures
                                            for value in sample.values()),
                "actual live texture for every exact signed operand pair")
    require(set(pins["classes"]) == {"uu", "bv"}, "two unchanged original class inventories")
    for owner, pin in pins["classes"].items():
        require(pin["class_sha256"] == CLASS_HASHES[owner], "exact original class entry")
        methods = pin["methods"]
        keys = [(method["name"], method["descriptor"]) for method in methods]
        require(len(set(keys)) == len(keys), "unique complete method declarations")
        if owner == "bv":
            for inherited in [("b", "(I)I"), ("b", "()I"), ("a", "(I)I"), ("a", "(II)I")]:
                require(inherited not in keys, "actual BV inheritance without invented override")
    for (owner, name, descriptor), (length, digest) in CODE.items():
        method = next(method for method in pins["classes"][owner]["methods"]
                      if (method["name"], method["descriptor"]) == (name, descriptor))
        require(method["code_bytes"] == length and method["code_sha256"] == digest,
                "exact source constructor/leaf/ordered-bound-store Code")
    return {"original_receivers": 2, "fresh_original_vms": 2, "color_calls_per_vm": 12,
            "texture_calls_per_vm": 72, "shape_calls_per_vm": 4, "Code_pins": 7,
            "original_or_native_GL_draws": 0}


def audit_public():
    result = audit(load("finite.json"), load("observations.json"), load("source-pins.json"))
    provenance = load("provenance.json")
    require(provenance["status"] == "CAPTURED_ORIGINAL" and
            provenance["client_jar_sha256"] == JAR and
            provenance["selected_inputs_unchanged"] is True and
            provenance["compiled_observer_unchanged"] is True and
            provenance["display_created"] is False and
            provenance["source_fields_written_by_observer"] is False, "capture scope")
    for category in ("captured_helpers", "publication_helpers", "outputs"):
        for name, expected in provenance[category].items():
            require(hashlib.sha256((HERE / name).read_bytes()).hexdigest() == expected,
                    "unchanged " + category + " " + name)
    for stem in ("ordinary_a", "ordinary_b"):
        for channel in ("stdout", "stderr"):
            text = load("observations.json")[stem + "_" + channel]
            require(hashlib.sha256(text.encode("utf-8")).hexdigest() ==
                    provenance[stem + "_" + channel + "_sha256"], "preserved original output bytes")
    return result


if __name__ == "__main__":
    print(audit_public())
