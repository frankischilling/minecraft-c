"""Resolve actual supported Entity virtuals directly from unchanged classfiles."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent
CLASSES = ("dc", "gs", "ls", "sn", "hl", "ju", "yl")
METHODS = (("f", "()Leq;"), ("a", "(Lsn;)Leq;"), ("i_", "()Z"),
           ("h", "(Lsn;)V"), ("b", "(Lgs;)V"), ("d", "(DDD)V"))


def observe(jar, parser):
    pin = json.loads((HERE / "source-pins.json").read_text())["client_jar_sha256"]
    if hashlib.sha256(Path(jar).read_bytes()).hexdigest() != pin:
        raise ValueError("original client JAR differs")
    with zipfile.ZipFile(jar) as archive:
        classes = {name: parser.parse_class(archive.read(name + ".class")) for name in CLASSES}
    result = {}
    for receiver in ("dc", "hl", "ju", "yl"):
        result[receiver] = {}
        for name, descriptor in METHODS:
            owner = receiver
            while owner in classes:
                found = next((m for m in classes[owner]["methods"]
                              if m["name"] == name and m["descriptor"] == descriptor), None)
                if found is not None:
                    result[receiver][name + descriptor] = {"owner": owner, **found}
                    break
                owner = classes[owner]["super"]
            else:
                raise ValueError("missing actual virtual: " + receiver + "." + name + descriptor)
    # These exact Code bodies justify only their actual empty/null returns.
    # The different yl.a(sn) body is pinned and remains explicitly unsupported
    # when yl is the moving collision-query receiver.
    for owner, name, descriptor, code in (
        ("sn", "f", "()Leq;", b"\x01\xb0"),
        ("sn", "a", "(Lsn;)Leq;", b"\x01\xb0"),
        ("yl", "f", "()Leq;", b"\x01\xb0"),
        ("sn", "b", "(Lgs;)V", b"\xb1"),
        ("sn", "i_", "()Z", b"\x03\xac"),
        ("yl", "i_", "()Z", b"\x04\xac")):
        method = next(m for m in classes[owner]["methods"]
                      if m["name"] == name and m["descriptor"] == descriptor)
        if method["code_length"] != len(code) or method["code_sha256"] != hashlib.sha256(code).hexdigest():
            raise ValueError("literal body differs: " + owner + "." + name + descriptor)
    return {"superclasses": {name: cls["super"] for name, cls in classes.items()}, "dispatch": result}


def load_parser(source_root):
    path = source_root / "tests/reference/entity/player/classfile.py"
    spec = importlib.util.spec_from_file_location("action_dispatch_metadata", path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    return parser


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[3])
    args = parser.parse_args()
    actual = observe(args.client_jar, load_parser(args.source_root))
    if actual != json.loads((HERE / "dispatch-pins.json").read_text()):
        raise ValueError("actual virtual resolution or Code differs")
    print("Player action: 24 actual virtual resolutions match")
