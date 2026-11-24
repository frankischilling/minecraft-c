"""Finite scalar corruption controls; no original assets, JVM or native game."""

import copy
import importlib.util
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("pressure_plate_inventory_packet", HERE / "packet.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


class PressurePlateInventoryPacket(unittest.TestCase):
    def setUp(self):
        self.finite = P.load("finite.json")
        self.observed = P.load("observations.json")
        self.pins = P.load("source-pins.json")

    def rejected(self, change, refresh=True):
        change(self.finite, self.observed, self.pins)
        if refresh:
            lines = ["16 achievements", "151 recipes"] + [
                "PRESSURE_PLATE_INVENTORY " + P.json.dumps(row) for row in self.finite["rows"]]
            self.observed["ordinary_a_stdout"] = self.observed["ordinary_b_stdout"] = "\n".join(lines) + "\n"
        with self.assertRaises((AssertionError, KeyError, StopIteration)):
            P.audit(self.finite, self.observed, self.pins)

    def test_public_packet(self):
        self.assertEqual(P.audit_public()["shape_calls_per_vm"], 4)

    def test_missing_receiver(self):
        self.rejected(lambda f, o, p: f["rows"].pop())

    def test_wrong_registered_id(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(id=72))

    def test_boolean_id_is_not_integer(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(id=True))

    def test_wrong_runtime_class(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(runtime_class="uu"))

    def test_invented_inherited_shape(self):
        self.rejected(lambda f, o, p: f["rows"][0]["owners"].update(shape="uu"))

    def test_scalar_reads_changed_bounds(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(after_scalar_bounds=P.INVENTORY))

    def test_constructor_bounds_substituted_for_inventory(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(inventory_bounds=P.CONSTRUCTOR))

    def test_world_unpressed_shape_substituted(self):
        self.rejected(lambda f, o, p: f["rows"][0]["inventory_bounds"].__setitem__(1, "0000000000000000"))

    def test_repeat_restores_constructor(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(repeated_inventory_bounds=P.CONSTRUCTOR))

    def test_negative_zero_bound_not_silently_equal(self):
        self.rejected(lambda f, o, p: f["rows"][0]["inventory_bounds"].__setitem__(0, "8000000000000000"))

    def test_nonwhite_metadata_color(self):
        self.rejected(lambda f, o, p: f["rows"][0]["color_samples"][0].update(color=0))

    def test_color_metadata_operand_order(self):
        self.rejected(lambda f, o, p: f["rows"][0]["color_samples"].reverse())

    def test_wrong_texture_metadata_value(self):
        self.rejected(lambda f, o, p: f["rows"][1]["texture_samples"][0].update(value=1))

    def test_missing_texture_operand_pair(self):
        self.rejected(lambda f, o, p: f["rows"][0]["texture_samples"].pop())

    def test_wrong_type_selects_another_geometry(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(render_type=1))

    def test_unpinned_live_class_resource(self):
        self.rejected(lambda f, o, p: f["rows"][0]["class_sha256"].update(bv="0" * 64))

    def test_changed_shape_code(self):
        def change(f, o, p):
            next(m for m in p["classes"]["bv"]["methods"]
                 if (m["name"], m["descriptor"]) == ("g", "()V"))["code_sha256"] = "0" * 64
        self.rejected(change)

    def test_fake_override_is_not_inheritance(self):
        self.rejected(lambda f, o, p: p["classes"]["bv"]["methods"].append(
            {"name": "b", "descriptor": "(I)I", "access": 1}))

    def test_two_vm_disagreement(self):
        self.rejected(lambda f, o, p: o.update(ordinary_b_stdout=""), refresh=False)

    def test_stderr_failure_preserved(self):
        self.rejected(lambda f, o, p: o.update(ordinary_a_stderr="source failure\n"), refresh=False)

    def test_finite_cannot_replace_observation(self):
        self.rejected(lambda f, o, p: f["rows"][0].update(id=71), refresh=False)

    def test_duplicate_json_key(self):
        with self.assertRaises(AssertionError):
            P.loads('{"shape":"bv","shape":"uu"}')


if __name__ == "__main__":
    unittest.main()
