"""Semantic guards use original-only data, never a manufactured native golden."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import sys
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location("dedicated_original_semantic_controls", ROOT / "scripts/reference/world/population/dedicated/fixtures.py")
F = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = F
SPEC.loader.exec_module(F)
C = F.comparison()


def object_at(state, ref):
    return next(row for row in state["graph"]["objects"] if row["object"] == ref)


def field_at(obj, declaring, name):
    return next(row for row in obj["fields"] if (row["declaring_class"], row["field"]) == (declaring, name))


def world_at(state):
    return object_at(state, state["graph"]["world"])


class DedicatedOriginalSemantics(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = F.load_package()

    def reject_original(self, edit, message=None):
        value = copy.deepcopy(self.case["original_value"])
        edit(value)
        context = self.assertRaisesRegex(ValueError, message) if message else self.assertRaises(ValueError)
        with context:
            F.validate_original(value, self.case["trace"], self.case["constructor_inputs"])

    def test_complete_original_invariants_without_java_or_native(self):
        result = F.validate_original(self.case["original_value"], self.case["trace"], self.case["constructor_inputs"])
        self.assertEqual(result["boundaries"], 18)
        self.assertEqual(result["interned_states"], 15)
        self.assertGreater(result["scheduled_occurrences"], 8000)
        self.assertFalse(result["source_classfile_audit_repeated"])

    def test_used_world_random_must_match_passive_jdi(self):
        def edit(value):
            first, last = value["states"][0], value["states"][-1]
            source = object_at(first, field_at(world_at(first), "dj", "r")["value"])
            target = object_at(last, field_at(world_at(last), "dj", "r")["value"])
            target["state"] = copy.deepcopy(source["state"])
            object_at(last, target["seed_owner"])["raw_long"] = target["state"]["seed48"].rjust(16, "0")
        self.reject_original(edit, "World Random")

    def test_derived_E_cannot_be_normalized(self):
        def edit(value):
            state = value["states"][0]
            field_at(world_at(state), "dp", "E")["value"] = True
            next(row for row in state["dedicated"]["fields"] if row["field"] == "E")["value"] = True
        self.reject_original(edit, "saving-disable")

    def test_missing_base_field_and_nullable_server_role_fail(self):
        def missing(value):
            fields = world_at(value["states"][0])["fields"]
            fields.remove(next(row for row in fields if (row["declaring_class"], row["field"]) == ("dj", "Q")))
        self.reject_original(missing, "manifest")
        def nullable(value):
            field_at(world_at(value["states"][0]), "dp", "F")["value"] = None
        self.reject_original(nullable)

    def test_paths_require_actual_absolute_source_binding(self):
        def edit(value):
            state = value["states"][0]
            store = object_at(state, field_at(world_at(state), "dj", "w")["value"])
            file = object_at(state, field_at(store, "dq", "b")["value"])
            identity = file["path"]
            for other in value["states"]:
                object_at(other, identity)["utf16"] = "world".encode("utf-16-be").hex()
        self.reject_original(edit, "Windows root")

    def test_queue_order_is_not_an_unordered_set(self):
        def edit(value):
            state = value["states"][-1]
            ordered = object_at(state, field_at(world_at(state), "dj", "E")["value"])["entries"]
            ordered[0], ordered[1] = ordered[1], ordered[0]
        self.reject_original(edit, "time/sequence")

    def test_validly_recompressed_metadata_cannot_change_seed(self):
        def edit(value):
            state = value["states"][value["operations"][6]["after"]["state_reference"]]
            ref = next(row["data"] for row in state["disk"]["entries"] if row["path"] == C.LEVEL)
            old = ref["sha256"]
            raw = bytes.fromhex(value["blobs"][old]["hex"])
            plain = bytearray(C.decompress(raw, True, 65536))
            key = b"\x04\x00\x0aRandomSeed"
            self.assertEqual(plain.count(key), 1)
            plain[plain.index(key) + len(key) + 7] ^= 1
            encoder = zlib.compressobj(wbits=31)
            raw = encoder.compress(bytes(plain)) + encoder.flush()
            digest = hashlib.sha256(raw).hexdigest()
            del value["blobs"][old]
            value["blobs"][digest] = {"bytes": len(raw), "hex": raw.hex()}
            pending = [value]
            while pending:
                row = pending.pop()
                if type(row) is dict:
                    if set(row) == {"bytes", "sha256"} and row["sha256"] == old:
                        row.update(bytes=len(raw), sha256=digest)
                    pending.extend(row.values())
                elif type(row) is list:
                    pending.extend(row)
            # Maintain complete disk byte totals so the typed seed, rather
            # than a broken transport length, supplies the rejection.
            pending = [value]
            while pending:
                row = pending.pop()
                if type(row) is dict:
                    if set(row) == {"complete", "entries", "files", "directories", "bytes"}:
                        row["bytes"] = sum(entry["data"]["bytes"] for entry in row["entries"] if entry["kind"] == "file")
                    pending.extend(row.values())
                elif type(row) is list:
                    pending.extend(row)
        self.reject_original(edit, "metadata seed")

    def test_constructor_inputs_remain_inputs_not_chosen_random_state(self):
        lines = self.case["constructor_inputs"].decode("ascii").splitlines()
        u, sample = lines[2].split("\t")
        lines[2] = u + "\t" + str(int(sample) ^ 1)
        changed = ("\n".join(lines) + "\n").encode("ascii")
        self.assertEqual(len(C.parse_tape(changed)), 9)
        with self.assertRaises(ValueError):
            F.validate_original(self.case["original_value"], self.case["trace"], changed)

    def test_clock_intervals_and_boundary_chain_are_not_discarded(self):
        self.reject_original(lambda value: value["operations"][6].update(epoch_ms=[2, 1]), "epoch")
        self.reject_original(lambda value: value["operations"][1].update(before={"state_reference": 14}), "continuous")


if __name__ == "__main__":
    unittest.main()
