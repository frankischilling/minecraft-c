"""Authored protocol controls; these rows are not simulation fixtures."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

spec = importlib.util.spec_from_file_location("weather_driver", Path(__file__).with_name("run.py"))
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


def protocol():
    rows = [" ".join(["INIT", str(i)] + ["0"] * 6) for i in range(4)]
    rows += [" ".join(["WEATHER", str(i), "0" if 39 <= i <= 44 else "1"] + ["0"] * 14)
             for i in range(64)]
    return rows


class WeatherControllerControls(unittest.TestCase):
    def test_complete_authored_inventory(self):
        rows = protocol()
        self.assertEqual(driver.projection("\n".join(rows)), rows)

    def test_missing_duplicate_order(self):
        rows = protocol()
        for changed in (rows[:-1], rows + rows[-1:], rows[1:] + rows[:1]):
            with self.subTest(first=changed[:1]), self.assertRaises(ValueError):
                driver.projection("\n".join(changed))

    def test_constructor_inventory_and_width(self):
        for kind in ("id", "width"):
            rows = protocol()
            rows[0] = rows[0].replace("INIT 0", "INIT 1") if kind == "id" else rows[0].rsplit(" ", 1)[0]
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_weather_width_and_order(self):
        for kind in ("id", "width"):
            rows = protocol()
            rows[4] = rows[4].replace("WEATHER 0", "WEATHER 1") if kind == "id" else rows[4].rsplit(" ", 1)[0]
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_complete_and_interrupted_inventory(self):
        for scene in (0, 36, 38, 39, 44, 45, 63):
            rows = protocol()
            fields = rows[scene + 4].split()
            fields[2] = "1" if fields[2] == "0" else "0"
            rows[scene + 4] = " ".join(fields)
            with self.subTest(scene=scene), self.assertRaises(ValueError):
                driver.projection("\n".join(rows))

    def test_positive_last_marker(self):
        self.assertEqual(driver.native_count("prefix diagnostic\nWorld weather: 12 checks\n"), 12)
        for text in ("", "World weather: 0 checks\n", "World weather: 2 checks\npartial\n", "World weather: -1 checks"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                driver.native_count(text)

    def test_real_exit_failure_overrides_marker(self):
        result = subprocess.run([sys.executable, "-B", "-c", "print('World weather: 1 checks');raise SystemExit(7)"],
                                capture_output=True, timeout=5)
        with self.assertRaises(ValueError):
            driver.require_process(result)

    def test_real_stderr_overrides_marker(self):
        result = subprocess.run([sys.executable, "-B", "-c", "import sys;print('World weather: 1 checks');sys.stderr.write('diagnostic')"],
                                capture_output=True, timeout=5)
        with self.assertRaises(ValueError):
            driver.require_process(result)


if __name__ == "__main__":
    unittest.main()
