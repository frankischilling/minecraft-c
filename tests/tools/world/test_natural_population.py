"""Failure controls for the complete original-derived population projection."""

import copy
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "natural_population_comparison", ROOT / "scripts/reference/world/population/compare_natural.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("missing natural population comparison")
DRIVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DRIVER)


class CompleteProjection(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, rows = DRIVER.PUBLIC.load_fixture()
        cls.expected = next(row["expected"] for row in rows
                            if row["record"]["side"] == "client" and
                            row["record"]["scenario"] == "population")

    def native(self):
        value = copy.deepcopy(self.expected)
        value.update(scope="native-game-normal-population", side="client", failure=None)
        for row in value["operations"]:
            row.pop("callbacks", None)
        return value

    def test_complete_original_profile(self):
        DRIVER.cache_projection(self.native(), self.expected)

    def test_negative_chunk_key_sign_halves_are_independent(self):
        for coordinates, key in (((0, -1), 65535), ((-1, 0), -65536),
                                  ((-1, -1), -1), ((1, -1), 131071), ((0, 0), 0)):
            with self.subTest(coordinates=coordinates):
                self.assertEqual(DRIVER.packed_key(*coordinates), key)

    def test_unknown_or_mismatched_profile_is_rejected(self):
        for profile in ("unknown", "ring", [], True):
            value = self.native()
            value["profile"] = profile
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                DRIVER.cache_projection(value, self.expected)

    def test_matching_producers_cannot_override_the_requested_profile(self):
        for native, original, requested, producer in (
                ({}, {}, "ring", "native"),
                ({"profile": "ring"}, {}, "ring", "original"),
                ({"profile": "ring"}, {"profile": "ring"}, "origin", "native")):
            with self.subTest(requested=requested, producer=producer), \
                    self.assertRaisesRegex(ValueError, "requested " + producer + " population profile"):
                DRIVER.compare_original(native, original, self.expected, Path("unused.jar"), profile=requested)

    def test_complete_state_changes_are_rejected(self):
        def mutate_array(value, field):
            data = value["operations"][2]["snapshot"]["world"]["chunks"][1]["data"][field]
            data["values"] = ("0" if data["values"][0] != "0" else "1") + data["values"][1:]

        mutations = {
            "constructor": lambda v: v["initial"]["world"]["queue"].append([0, 0, 0, 0, 0, 0, 0]),
            "status": lambda v: v["operations"][0].update(status="failed"),
            "identity": lambda v: v["operations"][3].update(result=1),
            "schedule": lambda v: v["operations"][0].update(coordinates=[0, 1]),
            "truncated_schedule": lambda v: v["operations"].pop(),
            "integer_boolean": lambda v: v["initial"]["context"].update(dedicated=0),
            "hidden_failure": lambda v: v.update(failure={"status": "failed"}),
            "flags": lambda v: v["operations"][2]["snapshot"]["world"]["chunks"][1]["flags"].update(n=False),
            "queue_order": lambda v: v["operations"][2]["snapshot"]["world"]["queue"].reverse(),
            "queue_tail": lambda v: v["operations"][2]["snapshot"]["world"]["queue"].pop(),
            "owner_map": lambda v: v["operations"][2]["snapshot"]["world"]["map"].pop(),
            "minimum": lambda v: v["operations"][2]["snapshot"]["world"]["chunks"][1]["data"].update(minimum_height=0),
        }
        mutations.update({field: lambda value, name=field: mutate_array(value, name)
                          for field in ("blocks", "metadata", "skylight", "block_light", "heightmap")})
        for name, mutation in mutations.items():
            with self.subTest(name=name):
                value = self.native()
                mutation(value)
                with self.assertRaises(ValueError):
                    DRIVER.cache_projection(value, self.expected)

    def test_cross_boundary_alias_and_replacement_changes(self):
        original = [{"q": "old", "v": "same", "d": "same"},
                    {"q": "old", "v": "new", "d": "new"}]
        native = [{"q": 44, "v": 91, "d": 91}, {"q": 44, "v": 99, "d": 99}]
        DRIVER.equal(DRIVER.normalize_identities(native), DRIVER.normalize_identities(original), "aliases")
        for field in ("q", "d"):
            changed = copy.deepcopy(native)
            changed[1][field] = 101
            with self.subTest(field=field), self.assertRaises(ValueError):
                DRIVER.equal(DRIVER.normalize_identities(changed), DRIVER.normalize_identities(original), "aliases")

    def test_large_payload_diagnostic_is_bounded(self):
        difference = DRIVER.first_difference("a" * 100000 + "b", "a" * 100000 + "c")
        self.assertIn("100000", difference)
        self.assertLess(len(difference), 128)

    def test_random_and_array_transport_rejects_coercion_and_truncation(self):
        state = {"state": "000000000001", "have_next_next_gaussian": False,
                 "next_next_gaussian_bits": "0000000000000000", "next_longs": ["0000000000000000"] * 8}
        DRIVER.random_value(state)
        for mutation in (lambda v: v.update(have_next_next_gaussian=0),
                         lambda v: v["next_longs"].pop(),
                         lambda v: v.update(state="00000000000g")):
            value = copy.deepcopy(state)
            mutation(value)
            with self.assertRaises(ValueError):
                DRIVER.random_value(value)
        array = {"identity": 1, "length": 2, "values": "0000000000000000" * 2}
        DRIVER.array_value(array)
        array["values"] = array["values"][:-1]
        with self.assertRaises(ValueError):
            DRIVER.array_value(array)

    def test_independently_seeded_original_random_requires_complete_evidence(self):
        valid = {"seed48": "123456789abc", "have_next_next_gaussian": False,
                 "next_next_gaussian_bits": "0000000000000000",
                 "next_longs": ["0000000000000000"] * 8}
        DRIVER.original_random_value(valid)
        for value in (None, {}, {**valid, "have_next_next_gaussian": 0},
                      {**valid, "next_longs": []}, {**valid, "seed48": "00000000000g"}):
            with self.subTest(value=value), self.assertRaises(ValueError):
                DRIVER.original_random_value(value)

    def test_injected_javac_options_are_rejected_before_capture(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "capture-base"
            arguments = [str(ROOT / "scripts/reference/world/population/compare_natural.py")]
            for option in ("executable", "client", "server", "java", "javac"):
                arguments.extend(("--" + option, str(Path(temporary) / option)))
            arguments.extend(("--work-dir", str(work)))
            errors = io.StringIO()
            with mock.patch.object(DRIVER.sys, "argv", arguments), \
                    mock.patch.dict(DRIVER.os.environ, {"JDK_JAVAC_OPTIONS": "-proc:only"}), \
                    contextlib.redirect_stderr(errors):
                self.assertEqual(DRIVER.main(), 1)
            self.assertIn("uninjected original Java environment", errors.getvalue())
            self.assertFalse(work.exists())


class RingFixtureContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = ROOT / "tests/reference/world/population/ring_fixtures.json"
        cls.fixtures = DRIVER.load_module("natural_ring_fixture_controls", "scripts/reference/world/population/fixtures.py")
        cls.package = DRIVER.read_json(cls.path)

    def load_changed(self, value):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fixture.json"
            path.write_bytes(self.fixtures.serialize_package(value))
            return self.fixtures.load_package(path, DRIVER)

    def test_complete_original_ring_inventory(self):
        cases = self.fixtures.load_package(self.path, DRIVER)
        self.assertEqual(set(cases), {"client", "server"})
        for side, case in cases.items():
            with self.subTest(side=side):
                self.assertEqual(len(case["original"]["states"]), 11)
                operations = case["expected"]["operations"]
                self.assertEqual([len(row["snapshot"]["world"]["chunks"]) - 1 for row in operations],
                                 [2, 3, 4, 4, 5, 6, 7, 8, 9, 9])
                self.assertEqual([len(row["snapshot"]["world"]["queue"]) for row in operations],
                                 [3, 9, 1777, 1777, 1778, 2950, 2952, 5824, 6968, 6968])
                final = operations[-1]["snapshot"]["world"]
                populated = sorted(chunk["coordinates"] for chunk in final["chunks"] if chunk["flags"]["n"])
                self.assertEqual(populated, [[-1, -1], [-1, 0], [0, -1], [0, 0]])

    def test_metadata_digest_and_dirty_attribution_corruption_is_rejected(self):
        mutations = {
            "profile": lambda v: v.update(profile="origin"),
            "archive": lambda v: v["sides"]["client"].update(jar_sha256="0" * 64),
            "packed_digest": lambda v: v["sides"]["client"]["original"].update(sha256="0" * 64),
            "dirty_commit": lambda v: v["provenance"]["capture"].update(tested_commit="0" * 40),
            "nested_dirty_commit": lambda v: v["provenance"]["capture"].update(
                attribution={"tested_commit": "0" * 40}),
            "nested_dirty_override": lambda v: v["provenance"]["capture"].update(
                attribution=[{"source_dirty": False, "committed_source": "0" * 40}]),
        }
        for name, mutate in mutations.items():
            value = copy.deepcopy(self.package)
            mutate(value)
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.load_changed(value)

    def test_expected_negative_key_must_derive_from_the_original(self):
        value = copy.deepcopy(self.package)
        stored = DRIVER.PUBLIC.strict_json(DRIVER.PUBLIC.unpack(value["sides"]["client"]["expected"]))
        stored["operations"][8]["snapshot"]["world"]["map"][0][0] ^= 32768
        value["sides"]["client"]["expected"] = DRIVER.PUBLIC.packed(json.dumps(stored).encode("ascii"))
        with self.assertRaisesRegex(ValueError, "stored projection"):
            self.load_changed(value)


if __name__ == "__main__":
    unittest.main()
