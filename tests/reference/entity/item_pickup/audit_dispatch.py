"""Resolve every current original Item property's actual inherited body."""
import argparse
import json
import zipfile
from pathlib import Path
from support import digest, module, require, REFERENCE


def audit(jar):
    require(digest(jar) == "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c", "original JAR differs")
    parser = module("pickup_graph_parser", REFERENCE / "entity/player/classfile.py")
    with zipfile.ZipFile(jar) as archive:
        classes = {Path(name).stem: parser.parse_class(archive.read(name))
                   for name in archive.namelist() if name.endswith(".class") and "/" not in name}

    def extends(name):
        seen = set()
        while name in classes and name not in seen:
            if name == "gm":
                return True
            seen.add(name)
            name = classes[name]["super"]
        return False

    descendants = sorted(name for name in classes if extends(name))
    require(len(descendants) == 40, "current original Item closure changed")
    resolved = {}
    for receiver in descendants:
        resolved[receiver] = {}
        for method, descriptor in (("d", "()I"), ("e", "()Z"), ("f", "()I")):
            owner = receiver
            while owner in classes:
                body = next((m for m in classes[owner]["methods"] if
                             m["name"] == method and m["descriptor"] == descriptor), None)
                if body is not None:
                    require(owner == "gm", "non-base actual Item property: " + receiver + method)
                    resolved[receiver][method + descriptor] = {"owner": owner, **body}
                    break
                owner = classes[owner]["super"]
            else:
                raise ValueError("missing actual Item property")
    return resolved


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.client_jar)
    print("Item properties: " + str(len(result) * 3) + " actual current-class resolutions inherit gm")
