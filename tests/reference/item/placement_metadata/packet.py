"""Audit complete, finite unchanged-original Item placement metadata scalars."""

import hashlib
import json
from pathlib import Path
import re

PINS = {'.gitattributes': '33662d5a22f1563ce28ffebe9e7ef96e3efb84b73a9baf8153ab74a0db4a1af2', 'capture-provenance.json': '5bcd57117d2f62cff280908942f743c4d4fa34e4496dab3490d070d99900ccda', 'capture_reference.py': 'e67f5e834f19fc3b7e19b04715859ad2da7e1481e832efd8339973b04c78487e', 'declared-inputs.json': 'adf9d8700ecbdec83de9b06ff455fa56ea9015e62bb6902f94e8cbe30f9f962d', 'operands.txt': 'ff76c1e74128f5df5a8ffeb04e4a4d9389bae599d8ad8a2e8af984ddeb8c8b0e', 'ordinary-0.stdout': 'fdb39f167a4e438fd9ab55331bfedcf8b593126118963753b2cfb704ebdbf114', 'ordinary-1.stdout': 'fdb39f167a4e438fd9ab55331bfedcf8b593126118963753b2cfb704ebdbf114', 'original.stdout': 'bdbf564d6bf72183c2aaddc69b16b6c45d0f526383cc201cba274826440644f9', 'PlacementMetadataObservation.java': '755da891f2b2b71f167de4e7d4eb0049d442381c01db6f4898b444770d14609b', 'README.md': '4796ce485581e2eaa7ddc47b13f31ca922bd826f010feb669cb04a7ec3594077', 'source-pins.json': '56a1597a6cb89a586f4ddd6341862efd902e102f428c4d6823f53dc445738746'}
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
OPERANDS = [-2147483648, -2147483647, -65536, -17, -16, -1, 0, 1, 2, 3,
            4, 7, 8, 15, 16, 17, 255, 65535, 2147483646, 2147483647]
OWNERS = {"gm", "bi", "en", "go", "mr", "qr", "uy"}
GUARD = "GUARD\tnetwork=0\tnative=0\texecs=0\twrites=0\tslots-unchanged=1"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def integer(text):
    require(re.fullmatch(r"0|-?[1-9][0-9]*", text) is not None, "canonical integer")
    value = int(text)
    require(-2147483648 <= value <= 2147483647, "signed int range")
    return value


def parse_lines(lines):
    """Validate the complete row contract; observed result bytes stay literal."""
    require(len(lines) == 4243, "complete row inventory")
    classes = []
    for line in lines[:202]:
        row = line.split("\t")
        require(len(row) == 4 and row[0] == "CLASS", "CLASS schema")
        item_id = integer(row[1])
        require(item_id > 0 and re.fullmatch(r"[a-z]{2,3}", row[2]) is not None, "registered receiver")
        require(row[3] in OWNERS, "declaring owner")
        require(row[3] == (row[2] if row[2] in OWNERS - {"gm"} else "gm"), "actual declaration family")
        classes.append((item_id, row[2], row[3]))
    ids = [row[0] for row in classes]
    require(ids == sorted(set(ids)), "ascending unique Item IDs")
    require(len({row[1] for row in classes}) == 38, "38 receiver classes")
    require({row[2] for row in classes} == OWNERS, "seven reached declarations")
    position = 202
    for item_id in ids:
        for operand in OPERANDS:
            row = lines[position].split("\t")
            require(len(row) == 4 and row[0] == "METADATA", "METADATA schema")
            require(integer(row[1]) == item_id and integer(row[2]) == operand, "ordered complete operands")
            integer(row[3])
            position += 1
    require(lines[-1] == "COMPLETE\t202\t38\t4040", "completion")
    return classes


def audit(directory=None):
    base = Path(directory) if directory is not None else Path(__file__).resolve().parent
    require(bool(PINS), "frozen artifact inventory")
    for name, expected in PINS.items():
        require(digest((base / name).read_bytes()) == expected, "artifact pin: " + name)
    raw = [(base / f"ordinary-{index}.stdout").read_bytes() for index in range(2)]
    require(raw[0] == raw[1], "two fresh VM outputs identical")
    raw_lines = raw[0].decode("ascii").splitlines()
    require(raw[0] == ("\r\n".join(raw_lines) + "\r\n").encode("ascii"), "exact captured CRLF shape")
    require(raw_lines[:2] == ["16 achievements", "151 recipes"] and raw_lines[-1] == GUARD,
            "original bootstrap and no-effect guard")
    selected = [line for line in raw_lines if line.startswith(("CLASS\t", "METADATA\t", "COMPLETE\t"))]
    require(len(raw_lines) == len(selected) + 3, "no unaccounted raw rows")
    projection = (base / "original.stdout").read_bytes()
    require(projection == ("\n".join(selected) + "\n").encode("ascii"), "declared LF projection")
    classes = parse_lines(projection.decode("ascii").splitlines())
    declared = json.loads((base / "declared-inputs.json").read_text(encoding="utf-8"))
    require(declared["jar_sha256"] == JAR and declared["operands"] == OPERANDS, "declared original operands")
    require((base / "operands.txt").read_bytes() == ("\n".join(map(str, OPERANDS)) + "\n").encode("ascii"),
            "canonical operand file")
    source = json.loads((base / "source-pins.json").read_text(encoding="utf-8"))
    require(source["jar_sha256"] == JAR, "original source authority")
    require(source["receiver_classes"] == sorted({row[1] for row in classes}), "all receiver class pins")
    require(set(source["classes"]) == set(source["receiver_classes"]) | {"uu", "iz"}, "source context inventory")
    leaves = {name: method for name, info in source["classes"].items() for method in info["methods"]
              if name in source["receiver_classes"] and method["name"] == "b" and method["descriptor"] == "(I)I"}
    require(set(leaves) == OWNERS, "seven original leaf Code pins")
    require(all(info["major"] == 49 and re.fullmatch("[0-9a-f]{64}", info["class_sha256"])
                for info in source["classes"].values()), "original class identities")
    receipt = json.loads((base / "capture-provenance.json").read_text(encoding="utf-8"))
    require(receipt["status"] == "PASS" and receipt["ordinary_fresh_vms"] == 2
            and receipt["original_game_class_replacements"] == 0, "original VM receipt")
    require(receipt["inputs_before"] == receipt["inputs_after"] and receipt["inputs_unchanged"], "input continuity")
    require(receipt["helpers_before"] == receipt["helpers_after"] and receipt["helpers_unchanged"], "helper continuity")
    require(receipt["raw_outputs_identical"] and receipt["original_stdout_sha256"] == digest(projection), "projection receipt")
    require(receipt["compile"]["returncode"] == 0 and receipt["compile"]["stderr_raw_private_sha256"] == digest(b""),
            "warning-free authored compilation")
    require(len(receipt["runs"]) == 2 and all(row["returncode"] == 0 and row["stdout_sha256"] == digest(raw[index])
                                             for index, row in enumerate(receipt["runs"])), "actual successful repeats")
    require(receipt["ordinary_vm_flags"] == ["-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
                                             "-Djava.security.manager=allow"], "ordinary VM flags")
    for name in ("PlacementMetadataObservation.java", "operands.txt", "declared-inputs.json", "capture_reference.py", "source-pins.json"):
        require(receipt["inputs_before"]["authored/" + name] == digest((base / name).read_bytes()), "frozen authored input: " + name)
    return {"items": len(classes), "receiver_classes": 38, "metadata_rows": 4040,
            "original_stdout_sha256": digest(projection)}


def reference_lines(directory=None):
    audit(directory)
    base = Path(directory) if directory is not None else Path(__file__).resolve().parent
    return (base / "original.stdout").read_text(encoding="ascii").splitlines()


if __name__ == "__main__":
    print("Placement metadata original packet PASS:", audit())
