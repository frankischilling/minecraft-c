"""Verify supplied binary metadata; never execute or rewrite original classes."""
import hashlib
import json
import zipfile
from pathlib import Path

from classfile import inspect_class

HERE = Path(__file__).resolve().parent


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_client(path):
    pins = json.loads((HERE / "source-pins.json").read_text())
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    require(actual == pins["client_jar_sha256"], "client JAR SHA256 does not match the supplied-version pin")
    methods = 0
    with zipfile.ZipFile(path) as jar:
        for name, wanted in pins["classes"].items():
            found = inspect_class(jar.read(name.replace(".", "/") + ".class"))
            require(found == wanted, f"original class/Code metadata changed: {name}")
            methods += len(found["methods"])
    return {"jar_sha256": actual, "verified_classes": len(pins["classes"]),
            "verified_method_metadata_records": methods}


def verify_paired_server(path):
    """Confirm the binary origins of the separately recorded 30-pair audit.

    This verifies class/Code pins, not its symbol-normalization implementation,
    server execution or native downstream server diagnostic frames.
    """
    audit = json.loads((HERE / "paired-method-audit.json").read_text())
    expected = audit["jars"]["server"]
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    require(actual == expected["sha256"], "server JAR SHA256 differs from paired metadata audit")
    require(all(p["normalized_instructions_equal"] for p in audit["pairs"]),
            "paired audit contains a differing selected method")
    verified = 0
    with zipfile.ZipFile(path) as jar:
        cache = {}
        for pair in audit["pairs"]:
            owner = pair["server"].split(".", 1)[0]
            if owner not in cache:
                cache[owner] = inspect_class(jar.read(owner + ".class"))
                require(cache[owner]["class_sha256"] == expected["classes"][owner]["class_sha256"],
                        f"paired server class changed: {owner}")
            wanted = pair["server_pin"]
            found = next(m for m in cache[owner]["methods"] if
                         m["name"] == wanted["name"] and m["descriptor"] == wanted["descriptor"])
            for key in ("access", "code_sha256", "code_attribute_sha256", "max_stack", "max_locals"):
                require(found[key] == wanted[key], f"paired server method metadata changed: {pair['server']}")
            require(found["code_bytes"] == wanted["code_length"], "paired server Code length changed")
            verified += 1
    return {"server_jar_sha256": actual, "selected_binary_method_pins_verified": verified,
            "server_runtime_executed": False, "normalization_reexecuted": False}
