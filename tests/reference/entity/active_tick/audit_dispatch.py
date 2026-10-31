"""Resolve genuine canonical Player virtuals from the unchanged client classfiles."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent
CLASSES = ("dc", "gs", "ls", "sn", "aa", "it")
METHODS = (("w_", "()V"), ("U", "()V"), ("o", "()V"), ("f_", "()V"),
           ("i", "(Lsn;)V"), ("c", "(DDDFF)V"), ("E", "()V"),
           ("r", "()V"), ("N", "()Z"), ("a", "(Lbr;)V"))


def observe(jar, parser):
    pins = json.loads((HERE / "source-pins.json").read_text())
    if hashlib.sha256(Path(jar).read_bytes()).hexdigest() != pins["client_jar_sha256"]:
        raise ValueError("original client JAR differs")
    with zipfile.ZipFile(jar) as archive:
        classes = {name: parser.parse_class(archive.read(name + ".class")) for name in CLASSES}
    result = {}
    for name, descriptor in METHODS:
        owner = "dc"
        while owner in classes:
            found = next((m for m in classes[owner]["methods"]
                          if m["name"] == name and m["descriptor"] == descriptor), None)
            if found is not None:
                result[name + descriptor] = {"owner": owner, **found}
                break
            owner = classes[owner]["super"]
        else:
            raise ValueError("missing actual virtual: dc." + name + descriptor)
    method = next(m for m in classes["aa"]["methods"]
                  if m["name"] == "b" and m["descriptor"] == "(Lgs;)Z")
    if method["code_length"] != 2 or method["code_sha256"] != hashlib.sha256(b"\x04\xac").hexdigest():
        raise ValueError("literal body differs: aa.b(gs)")
    return {"superclasses": {name: cls["super"] for name, cls in classes.items()},
            "dispatch": result}


def load_parser(source_root):
    path = source_root / "tests/reference/entity/player/classfile.py"
    spec = importlib.util.spec_from_file_location("active_tick_metadata", path)
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
    print("Player active tick: 10 actual virtual resolutions match")
