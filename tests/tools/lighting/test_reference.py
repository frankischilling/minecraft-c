import importlib.util
from contextlib import redirect_stderr
import io
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "light_comparison", ROOT / "scripts/reference/lighting/compare_light.py")
LIGHT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LIGHT)


class LightComparisonTests(unittest.TestCase):
    def test_old_jdk_is_unavailable(self):
        class CommonUnavailable(Exception):
            pass

        class HelperUnavailable(Exception):
            pass

        def old_jdk(_):
            raise HelperUnavailable("Java 11 or newer is required")

        common = SimpleNamespace(Unavailable=CommonUnavailable,
                                 runtime_tools=lambda _: {"jdk_release": Path("unused")})
        helper = SimpleNamespace(Unavailable=HelperUnavailable, release_version=old_jdk)
        environment = {name: "" for name in LIGHT.JAVA_ENVIRONMENT}
        with patch.object(LIGHT, "helpers", return_value=(common, helper, [])), \
                patch.dict(LIGHT.os.environ, environment), redirect_stderr(io.StringIO()):
            self.assertEqual(LIGHT.main(["--client", "unused", "--server", "unused",
                                         "--capture-only", "--work-dir", "unused"]), 77)

    def test_output_compares_every_bound_and_count(self):
        expected = [["M", 1, 0, 1, 2, 3, 4, 5], ["E", 1, 0, 8, 64, 8, 8, 65, 8]]
        good = b"M 1 0 1 2 3 4 5\nE 1 0 8 64 8 8 65 8\n"
        self.assertEqual(LIGHT.compare_output(good, expected), 2)
        for changed in (good.replace(b"64", b"63"), good.replace(b"E 1", b"E 2"),
                        good + b"Q 0\n", good.split(b"\n", 1)[0]):
            with self.subTest(changed=changed), self.assertRaises(LIGHT.Failure):
                LIGHT.compare_output(changed, expected)

    def test_region_integer_domain(self):
        for value in ([0] * 5, [True] * 6, [0] * 5 + [1 << 31], [2] + [0] * 6):
            with self.subTest(value=value), self.assertRaises(LIGHT.Failure):
                LIGHT.bounds(value, len(value) if len(value) == 7 else 6)
        self.assertEqual(LIGHT.bounds([-(1 << 31), 0, 0, (1 << 31) - 1, 0, 0]),
                         [-(1 << 31), 0, 0, (1 << 31) - 1, 0, 0])

    def test_rejected_request_must_preserve_queue(self):
        value = {"merges": [], "queue": {"before_world": {"queue": {"entries": []}},
                 "requests": [{"region": [0, 8, 64, 8, 8, 64, 8],
                               "admitted": False, "coalesce": True}],
                 "steps": [[[0, 8, 64, 8, 8, 64, 8]]]}}
        with self.assertRaises(LIGHT.Failure):
            LIGHT.replay_input(value)
        value["queue"]["steps"] = [[]]
        self.assertEqual(LIGHT.replay_input(value), ("Q 0\n", [["Q", 0]]))

    def test_baseline_is_replayed_in_order(self):
        entries = [{"ordinal": index, "kind": {"identity": "b", "name": "Block",
                    "ordinal": 1, "default": 0}, "bounds": [x, 64, 2, x, 64, 2]}
                   for index, x in enumerate((-9, 8))]
        self.assertEqual(LIGHT.regions({"queue": {"entries": entries}}),
                         [[1, -9, 64, 2, -9, 64, 2], [1, 8, 64, 2, 8, 64, 2]])


if __name__ == "__main__":
    unittest.main()
