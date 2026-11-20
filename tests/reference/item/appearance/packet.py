"""Audit the complete finite unchanged-original Item appearance packet."""

import hashlib
import json
from pathlib import Path
import re

PINS = {'.gitattributes': '33662d5a22f1563ce28ffebe9e7ef96e3efb84b73a9baf8153ab74a0db4a1af2', 'AppearanceObservation.java': '6aba8461f95f0d99638d6c5846759e83e8d7aeb2b77957a5b9cd269962a2556c', 'bm-operands.txt': '2c46568d53a17f3a31e8943f9c43bf3a9d55126398ce1cb458bc96c56719bc4d', 'capture-provenance.json': 'd82fd822135ea31cbbdcff83e1e39ed6df2b38d983d6272add32fdaf286c8b8e', 'capture_reference.py': 'd3e9e73a6cb689adde278a455f921da1f50c0499112ab89caa01602c5b12688a', 'declared-inputs.json': '17f7845705faf632b4d8028f7fa742db4d15f6c46141a3ea9d5aa264f3648845', 'operands.txt': 'ff76c1e74128f5df5a8ffeb04e4a4d9389bae599d8ad8a2e8af984ddeb8c8b0e', 'ordinary-0.stdout': '56dff94c44182927502b65b0a864ef37fb301c9780c78f8af74f3bdae0d76d73', 'ordinary-1.stdout': '56dff94c44182927502b65b0a864ef37fb301c9780c78f8af74f3bdae0d76d73', 'original.stdout': 'abb599069c8bc2a1ff1496904e07eb743ef38c73d7fa7b0859b44f3da1481697', 'README.md': '5424497b53cdb4e1728574aed3e005256d8792701649c5268cf900f27906de24', 'source-pins.json': '999b6e868b5947cf7c9627edc93e42790adaf23ddc16f14adf7ced523ea8d222'}
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
OPERANDS = [-2147483648, -2147483647, -65536, -17, -16, -1, 0, 1, 2, 3,
            4, 7, 8, 15, 16, 17, 255, 65535, 2147483646, 2147483647]
BM_OPERANDS = [-2147483648, -81, -80, -1, 0, 2147483567, 2147483568, 2147483647]
CONTEXT_CLASSES = {"uu", "iz", "dq", "ee", "vg", "ys", "he", "bk", "wb", "jh"}
GUARD = ("GUARD\tnetwork=0\tnative=0\texecs=0\twrites=0\tslots-unchanged=1"
         "\tbh-unchanged=1\tbm-restored=1")


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
    """Check inventory/order/types; scalar result bytes remain observed literals."""
    require(len(lines) == 4307, "complete row inventory")
    classes = []
    owner_sets = [set() for _ in range(4)]
    for line in lines[:202]:
        row = line.split("\t")
        require(len(row) == 8 and row[0] == "CLASS", "CLASS schema")
        item_id = integer(row[1])
        require(item_id > 0 and all(re.fullmatch(r"[a-z]{2,3}", name) for name in row[2:7]),
                "actual class/declaration names")
        integer(row[7])
        for owners, name in zip(owner_sets, row[3:7]):
            owners.add(name)
        classes.append((item_id, *row[2:7], integer(row[7])))
    ids = [row[0] for row in classes]
    require(ids == sorted(set(ids)), "ascending unique Item IDs")
    require(len({row[1] for row in classes}) == 38, "38 actual receiver classes")
    require(owner_sets == [{"gm", "km", "bi", "go", "en", "mr", "uy"}, {"gm", "uy"},
                           {"gm", "dq", "qd", "la", "cs"}, {"gm", "cs"}], "actual declaration inventory")
    position = 202
    for item_id in ids:
        for damage in OPERANDS:
            row = lines[position].split("\t")
            require(len(row) == 8 and row[0] == "APPEARANCE", "APPEARANCE schema")
            values = [integer(text) for text in row[1:]]
            require(values[:2] == [item_id, damage], "ordered complete signed damage operands")
            require(values[2] == values[3], "observed direct/IZ icon equality")
            require(values[5] in (0, 1) and values[6] in (0, 1), "source Boolean observations")
            position += 1
    for item_id in (6, 18):
        for bm in BM_OPERANDS:
            for damage in range(4):
                row = lines[position].split("\t")
                require(len(row) == 6 and row[0] == "BM", "BM schema")
                values = [integer(text) for text in row[1:]]
                require(values[:3] == [item_id, bm, damage], "ordered declared instance bm cases")
                require(values[3] == values[4], "observed mutable-bm direct/IZ equality")
                position += 1
    require(lines[-1] == "COMPLETE\t202\t38\t4040\t64", "completion")
    return classes


def audit(directory=None):
    base = Path(directory) if directory is not None else Path(__file__).resolve().parent
    require(bool(PINS), "frozen artifact inventory")
    for name, expected in PINS.items():
        require(digest((base / name).read_bytes()) == expected, "artifact pin: " + name)
    raw = [(base / f"ordinary-{index}.stdout").read_bytes() for index in range(2)]
    require(raw[0] == raw[1], "two fresh ordinary VM outputs identical")
    raw_lines = raw[0].decode("ascii").splitlines()
    require(raw[0] == ("\r\n".join(raw_lines) + "\r\n").encode("ascii"), "exact captured CRLF shape")
    require(raw_lines[:2] == ["16 achievements", "151 recipes"] and raw_lines[-1] == GUARD,
            "actual bootstrap and no-effect/restoration guard")
    selected = [line for line in raw_lines if line.startswith(("CLASS\t", "APPEARANCE\t", "BM\t", "COMPLETE\t"))]
    require(len(raw_lines) == len(selected) + 3, "no unaccounted raw rows")
    projection = (base / "original.stdout").read_bytes()
    require(projection == ("\n".join(selected) + "\n").encode("ascii"), "declared LF projection")
    classes = parse_lines(projection.decode("ascii").splitlines())
    declared = json.loads((base / "declared-inputs.json").read_text(encoding="utf-8"))
    require(declared["jar_sha256"] == JAR and declared["operands"] == OPERANDS
            and declared["bm_operands"] == BM_OPERANDS and declared["bm_items"] == [6, 18]
            and declared["bm_damage"] == [0, 1, 2, 3], "declared complete inputs")
    for name, values in (("operands.txt", OPERANDS), ("bm-operands.txt", BM_OPERANDS)):
        require((base / name).read_bytes() == ("\n".join(map(str, values)) + "\n").encode("ascii"),
                "canonical operands: " + name)
    source = json.loads((base / "source-pins.json").read_text(encoding="utf-8"))
    require(source["jar_sha256"] == JAR and source["receiver_classes"] == sorted({row[1] for row in classes}),
            "all genuine receiver class identities")
    require(set(source["classes"]) == set(source["receiver_classes"]) | CONTEXT_CLASSES, "source child inventory")
    for name, info in source["classes"].items():
        require(info["major"] == 49 and re.fullmatch("[0-9a-f]{64}", info["class_sha256"]), "original class pin")
        for method in info["methods"]:
            require(method["code_bytes"] > 0 and re.fullmatch("[0-9a-f]{64}", method["code_sha256"]),
                    "selected original Code identity")
    jh = {(row["name"], row["descriptor"]) for row in source["classes"]["jh"]["methods"]}
    require({("<clinit>", "()V"), ("a", "()I"), ("b", "()I"), ("c", "()I")} <= jh,
            "actual JH initializer and constant children")
    receipt = json.loads((base / "capture-provenance.json").read_text(encoding="utf-8"))
    require(receipt["status"] == "PASS" and receipt["ordinary_fresh_vms"] == 2
            and receipt["original_game_class_replacements"] == 0, "unchanged original VM receipt")
    require(receipt["inputs_before"] == receipt["inputs_after"] and receipt["inputs_unchanged"], "input continuity")
    require(receipt["helpers_before"] == receipt["helpers_after"] and receipt["helpers_unchanged"], "helper continuity")
    require(receipt["raw_outputs_identical"] and receipt["original_stdout_sha256"] == digest(projection), "projection receipt")
    require(receipt["compile"]["returncode"] == 0 and receipt["compile"]["stderr_raw_private_sha256"] == digest(b""),
            "warning-free authored helper compilation")
    require(len(receipt["runs"]) == 2 and all(row["returncode"] == 0 and row["stdout_sha256"] == digest(raw[index])
                                             for index, row in enumerate(receipt["runs"])), "actual successful repeats")
    require(receipt["ordinary_vm_flags"] == ["-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
                                             "-Djava.security.manager=allow"], "ordinary VM flags")
    for name in ("AppearanceObservation.java", "operands.txt", "bm-operands.txt", "declared-inputs.json",
                 "capture_reference.py", "source-pins.json"):
        require(receipt["inputs_before"]["authored/" + name] == digest((base / name).read_bytes()),
                "frozen authored input: " + name)
    return {"items": 202, "receiver_classes": 38, "appearance_rows": 4040, "bm_rows": 64,
            "original_stdout_sha256": digest(projection)}


def reference_lines(directory=None):
    audit(directory)
    base = Path(directory) if directory is not None else Path(__file__).resolve().parent
    return (base / "original.stdout").read_text(encoding="ascii").splitlines()


if __name__ == "__main__":
    print("Item appearance original packet PASS:", audit())
