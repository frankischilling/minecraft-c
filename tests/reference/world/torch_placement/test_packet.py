"""Finite literal/entropy corruption controls; no JVM/native process launch."""

import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("torch_packet_controls", HERE / "packet.py")
packet = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packet)


def own_rows(index=0):
    return [json.loads(line.split("\t", 1)[1]) for line in
            (HERE / f"ordinary-{index}.entropy.jsonl").read_text().splitlines()]


def row_corruptions(row):
    changes = []
    for key, value in (("index", 1), ("name", "wrong"), ("result", "TRUE"), ("exception", "other"),
                       ("center_before", [1, 0]), ("center_after", [1, 0]), ("world_rng_unchanged", 1),
                       ("events", [["unknown"]]), ("drops", [[0, "hl", 51, 1, 0, 10, 0, 5]])):
        changed = copy.deepcopy(row)
        changed[key] = value
        changes.append(changed)
    for column in range(12):
        changed = copy.deepcopy(row)
        changed["values"][column] += 1
        changes.append(changed)
    for value in ([[0, 0, 0, 0, 0]], [[0, 0, 0, 1, 2]], [[4, 0, 0, 0, 1]],
                  [[0, 5, 0, 0, 1]], [[0, 0, 32768, 0, 1]], [[0, 0, 0, 0, 256]],
                  [[0, 0, 1, 0, 1], [0, 0, 0, 0, 1]]):
        changed = copy.deepcopy(row)
        changed["changes"] = value
        changes.append(changed)
    for value in ([[2, 0, 0, 0, 0, 0, 0]], [[0, 1, 0, 0, 0, 0, 0]], [[0, 0]]):
        changed = copy.deepcopy(row)
        changed["queues_after"] = value
        changes.append(changed)
    changed = copy.deepcopy(row)
    changed["extra"] = 1
    changes.append(changed)
    changed = copy.deepcopy(row)
    del changed["events"]
    changes.append(changed)
    return changes


def entropy_corruptions(own):
    changes = []
    for key, value in (("index", -1), ("name", "wrong"), ("items", []), ("math_after", None)):
        changed = copy.deepcopy(own)
        changed[key] = value
        changes.append(changed)
    for key, value in (("seed", "0000000000000"), ("seed", "zzzzzzzzzzzz"),
                       ("gaussian", "0"), ("have", 0)):
        changed = copy.deepcopy(own)
        changed["world_after"][key] = value
        changes.append(changed)
    for key in ("seed", "gaussian"):
        changed = copy.deepcopy(own)
        original = changed["world_after"][key]
        changed["world_after"][key] = original[:-1] + ("1" if original[-1] != "1" else "2")
        changes.append(changed)
    for key, value in (("ordinal", 1), ("position", ["0000000000000000"] * 3),
                       ("motion", ["0000000000000000"] * 3), ("yaw", "00000000"), ("bob", "00000000")):
        changed = copy.deepcopy(own)
        changed["items"][0][key] = value
        changes.append(changed)
    changed = copy.deepcopy(own)
    changed["items"][0]["entity_random"]["have"] = True
    changes.append(changed)
    changed = copy.deepcopy(own)
    changed["math_before"] = changed["items"][0]["math_post"]
    changes.append(changed)
    changed = copy.deepcopy(own)
    changed["items"][0]["math_post"]["seed"] = "000000000000"
    changes.append(changed)
    changed = copy.deepcopy(own)
    changed["extra"] = 0
    changes.append(changed)
    return changes


class PacketControls(unittest.TestCase):
    def test_complete_actual_packet_and_independent_streams(self):
        result = packet.audit()
        rows = packet.reference_rows()
        self.assertEqual((len(rows), len(packet.reference_lines())), (193, 194))
        self.assertEqual([value["drops"] for value in result["own_entropy_audits"]], [34, 34])
        self.assertNotEqual(own_rows(0), own_rows(1))
        self.assertEqual(packet.audit_cohort(rows, own_rows(0)), packet.audit_cohort(rows, own_rows(1)))

    def test_each_artifact_pin_and_missing_packet(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(OSError):
                packet.audit(temporary)
            copied = Path(temporary) / "packet"
            shutil.copytree(HERE, copied)
            for name in packet.PINS:
                with self.subTest(artifact=name):
                    path = copied / name
                    original = path.read_bytes()
                    path.write_bytes(original + b"x")
                    with self.assertRaises(ValueError):
                        packet.audit(copied)
                    path.write_bytes(original)

    def test_literal_row_schema_and_order(self):
        rows = packet.reference_rows()
        profiles = packet.parse_profiles((HERE / "profiles.tsv").read_bytes())
        for index, changed in enumerate(row_corruptions(rows[0])):
            with self.subTest(row=index):
                with self.assertRaises(ValueError):
                    packet.validate_row(changed, profiles[0], 0)
        lines = packet.reference_lines()
        mutations = [lines[:-1], lines + [lines[-1]], [lines[1], lines[0], *lines[2:]],
                     [*lines[:-1], "COMPLETE\t192"]]
        for mutation in mutations:
            with self.assertRaises(ValueError):
                packet.parse_lines(mutation, profiles)

    def test_actual_own_entropy_corruptions(self):
        rows, own = packet.reference_rows(), own_rows()
        index = next(i for i, row in enumerate(rows) if row["drops"])
        for number, changed in enumerate(entropy_corruptions(own[index])):
            with self.subTest(entropy=number):
                with self.assertRaises(ValueError):
                    packet.audit_entropy(rows[index], changed)
        changed = copy.deepcopy(own)
        changed[index + 1]["world_before"] = changed[index]["world_before"]
        with self.assertRaises(ValueError):
            packet.audit_cohort(rows, changed)
        changed = copy.deepcopy(own)
        changed[index + 1]["math_before"] = None
        with self.assertRaises(ValueError):
            packet.audit_cohort(rows, changed)

    def test_full_literal_values_remain_pinned(self):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "packet"
            shutil.copytree(HERE, copied)
            path = copied / "original.stdout"
            data = path.read_bytes()
            self.assertIn(b'"result":"false"', data)
            path.write_bytes(data.replace(b'"result":"false"', b'"result":"true"', 1))
            with self.assertRaises(ValueError):
                packet.audit(copied)


if __name__ == "__main__":
    unittest.main()
