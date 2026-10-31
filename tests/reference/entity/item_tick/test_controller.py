"""Authored text-protocol controls, never runtime Entity/World fixtures."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

spec = importlib.util.spec_from_file_location("item_driver", Path(__file__).with_name("run.py"))
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


def authored_protocol():
    result = []
    events = {n: [("S", "random.fizz")] for n in (12, 13, 19, 20)}
    for n in (15, 36):
        events[n] = [("S", "random.splash")] + [("P", "bubble")] * 6 + [("P", "splash")] * 6
    events[17] = [("S", "random.splash")]
    events[18] = [("S", "random.splash"), ("P", "bubble")]
    for scene in range(37):
        for i, (kind, name) in enumerate(events.get(scene, [])):
            result.append(" ".join(["EVENT", str(scene), str(i), kind, name] + ["0"] * (5 if kind == "S" else 6)))
        result.append(" ".join(["STATE", str(scene), "0" if scene in {17, 18, 19, 30} else "1"] + ["0"] * 53))
    return result


class ControllerControls(unittest.TestCase):
    def test_complete_authored_schema(self):
        rows = authored_protocol()
        self.assertEqual(driver.projection("\n".join(rows)), rows)

    def test_missing_duplicate_and_wrong_order(self):
        rows = authored_protocol()
        for changed in (rows[:-1], rows + rows[-1:], rows[1:] + rows[:1]):
            with self.subTest(changed=changed[:1]), self.assertRaises(ValueError):
                driver.projection("\n".join(changed))

    def test_completion_inventory(self):
        rows = authored_protocol()
        rows[0] = rows[0].replace("STATE 0 1", "STATE 0 0")
        with self.assertRaises(ValueError):
            driver.projection("\n".join(rows))

    def test_partial_and_wrong_event(self):
        for kind in ("partial", "name", "index"):
            rows = authored_protocol()
            index = next(i for i, line in enumerate(rows) if line.startswith("EVENT "))
            if kind == "partial":
                rows[index] = rows[index].rsplit(" ", 1)[0]
            elif kind == "name":
                rows[index] = rows[index].replace("random.fizz", "random.pop")
            else:
                rows[index] = rows[index].replace("EVENT 12 0", "EVENT 12 1")
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_partial_state(self):
        rows = authored_protocol()
        rows[0] = rows[0].rsplit(" ", 1)[0]
        with self.assertRaises(ValueError):
            driver.projection("\n".join(rows))

    def test_positive_final_native_marker(self):
        self.assertEqual(driver.native_count("diagnostic\nItem tick: 12 checks\n"), 12)
        for text in ("", "partial\n", "Item tick: 0 checks\n", "Item tick: 1 checks\npartial\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.native_count(text)

    def test_real_exit_and_stderr_override_marker(self):
        # Small outputs are explicitly bounded by the child program here; the
        # production driver's separately reviewed capture owns pipe fault tests.
        for body in ("print('Item tick: 1 checks');raise SystemExit(7)",
                     "import sys;print('Item tick: 1 checks');sys.stderr.write('diagnostic')"):
            result = subprocess.run([sys.executable, "-B", "-c", body], capture_output=True, timeout=5)
            with self.subTest(body=body), self.assertRaises(ValueError):
                driver.require_process(result)


if __name__ == "__main__":
    unittest.main()
