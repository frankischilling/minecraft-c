"""Strict schema for the finite unchanged-original mining observations."""
import hashlib
import json
from pathlib import Path
import re


def read_packet(directory):
    directory = Path(directory)
    proof = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))
    for name, key in (("classes.tsv", "classes_sha256"), ("source-pins.json", "source_pins_sha256")):
        if hashlib.sha256((directory / name).read_bytes()).hexdigest() != proof[key]:
            raise ValueError("mining source identity hash mismatch")
    classes = [line.split("\t") for line in (directory / "classes.tsv").read_text().splitlines()]
    if len(classes) != 202 or any(len(row) != 5 or row[0] != "CLASS" for row in classes):
        raise ValueError("mining Item Class schema")
    identities = {int(row[1]) for row in classes}
    if (len(identities) != 202 or len({row[2] for row in classes}) != 38 or
            {row[3] for row in classes} != {"gm", "dq", "qd", "bl"} or
            {row[4] for row in classes} != {"gm", "wc", "au", "qd", "bl"}):
        raise ValueError("mining complete Item Class dispatch")
    observations = directory / "observations.tsv"
    if hashlib.sha256(observations.read_bytes()).hexdigest() != proof["observations_sha256"]:
        raise ValueError("mining observations hash mismatch")
    lines = observations.read_text(encoding="utf-8").splitlines()
    if len(lines) != 19595 or lines[-1] != "TOTAL\t202\t19594":
        raise ValueError("mining observation count")
    pairs = set()
    per_item = {}
    for line in lines[:-1]:
        row = line.split("\t")
        if len(row) != 9 or row[0] != "MINING":
            raise ValueError("mining observation schema")
        item, block = int(row[1]), int(row[2])
        if item < 1 or item > 2257 or block not in {-1, *range(1, 97)}:
            raise ValueError("mining observation identity")
        if (item, block) in pairs:
            raise ValueError("duplicate mining observation")
        pairs.add((item, block))
        per_item.setdefault(item, set()).add(block)
        for index in (3, 5, 7):
            if row[index] != "NULL" and re.fullmatch("[0-9a-f]{8}", row[index]) is None:
                raise ValueError("mining speed encoding")
        for index in (4, 6, 8):
            if row[index] not in {"0", "1", "NULL"}:
                raise ValueError("mining harvest encoding")
        if row[3] != row[5] or row[3] != row[7] or row[4] != row[6]:
            raise ValueError("mining Item/Stack/Inventory correspondence")
        if (block == -1) != (row[8] == "NULL"):
            raise ValueError("mining Inventory Block dereference")
    if set(per_item) != identities or any(len(blocks) != 97 for blocks in per_item.values()):
        raise ValueError("mining complete cross product")
    return lines


def read_destroy_packet(directory):
    directory = Path(directory)
    proof = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))["destroy"]
    for name, key in (("destroy-observations.tsv", "observations_sha256"),
                      ("controller-destroy-observations.tsv", "controller_sha256"),
                      ("destroy-source-pins.json", "source_pins_sha256")):
        if hashlib.sha256((directory / name).read_bytes()).hexdigest() != proof[key]:
            raise ValueError("mining destruction input hash mismatch")
    lines = (directory / "destroy-observations.tsv").read_text().splitlines()
    controller = (directory / "controller-destroy-observations.tsv").read_text().splitlines()
    direct = [line for line in lines if line.startswith("DESTROY\t")]
    wrapper = [line for line in lines if line.startswith("STACK_DESTROY\t")]
    if len(direct) != 1947 or len(wrapper) != 384 or len(lines) != 2331 or len(controller) != 6:
        raise ValueError("mining destruction corpus count")
    for line in lines + controller:
        row = line.split("\t")
        expected = {"DESTROY": 9, "STACK_DESTROY": 10, "CONTROLLER_DESTROY": 12}.get(row[0])
        if expected is None or len(row) != expected:
            raise ValueError("mining destruction row schema")
        if any(re.fullmatch("-?(0|[1-9][0-9]*)", value) is None for value in row[1:]):
            raise ValueError("mining destruction integer encoding")
        numbers = [int(value) for value in row[1:]]
        if any(value < -(1 << 31) or value >= (1 << 31) for value in numbers):
            raise ValueError("mining destruction integer range")
        if row[0] == "DESTROY" and (row[5] not in {"0", "1"} or row[8] != "5"):
            raise ValueError("mining destruction Item result/animation")
        if row[0] == "STACK_DESTROY" and (row[7] != "5" or row[8] not in {"0", "1"} or
                                          row[9] not in {"-1", "0", "1"}):
            raise ValueError("mining destruction Stack statistic/animation")
    if len({line.split("\t")[1] for line in direct[:1212]}) != 202:
        raise ValueError("mining destruction complete Item dispatch")
    if [line.split("\t")[1] for line in controller] != [str(i) for i in range(6)]:
        raise ValueError("mining destruction controller profiles")
    return lines, controller
