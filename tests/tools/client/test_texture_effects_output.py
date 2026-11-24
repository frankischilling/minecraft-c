"""Pure admission checks for complete texture-effect contract output."""
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).with_name("test_texture_effects.py")
spec = importlib.util.spec_from_file_location("effect_output_driver", path)
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class OutputAdmission(unittest.TestCase):
    footers = {
        "cpu": b"Actual texture-effect CPU controls: 8852",
        "registration": b"Actual g registration controls: 1",
        "allocation": b"Actual g allocation controls: 1",
        "world": b"Actual effect World/Player controls: 1",
        "spawn": b"Actual spawn extraction controls: 1",
    }

    def output(self, profile, newline=b"\n"):
        lines = [b"151 recipes", b"16 achievements"]
        if profile == "registration":
            lines += [b"16 achievements", b"151 recipes"]
        return newline.join(lines + [self.footers[profile], b""])

    def test_complete_profiles_and_line_endings(self):
        for profile in self.footers:
            for newline in (b"\n", b"\r\n"):
                with self.subTest(profile=profile, newline=newline):
                    self.assertTrue(driver._admit_output(profile, self.output(profile, newline)))

    def test_requires_both_banners_in_source_order(self):
        footer = self.footers["cpu"] + b"\n"
        for prefix in (b"", b"151 recipes\n", b"16 achievements\n",
                       b"16 achievements\n151 recipes\n"):
            self.assertFalse(driver._admit_output("cpu", prefix + footer))
        registration = self.output("registration")
        self.assertFalse(driver._admit_output("registration", registration.replace(
            b"16 achievements\n151 recipes\n", b"")))
        self.assertFalse(driver._admit_output("registration", registration.replace(
            b"16 achievements\n151 recipes\n", b"151 recipes\n16 achievements\n")))

    def test_rejects_extra_duplicate_or_incomplete_output(self):
        raw = self.output("cpu")
        for invalid in (b"extra\n" + raw, raw + b"extra\n", b"151 recipes\n" + raw,
                        raw.replace(b"16 achievements\n", b"16 achievements\n16 achievements\n"),
                        raw[:-1], raw + b"\0"):
            self.assertFalse(driver._admit_output("cpu", invalid))

    def test_requires_positive_profile_specific_footer(self):
        for profile in self.footers:
            if profile != "cpu":
                self.assertFalse(driver._admit_output(profile, self.output("cpu")))
        for count in (b"0", b"-1", b"01", b"8852 trailing", b""):
            self.assertFalse(driver._admit_output("cpu", self.output("cpu").replace(b"8852", count)))

    def test_rejects_unknown_profiles_and_nonbyte_streams(self):
        self.assertFalse(driver._admit_output("unknown", self.output("cpu")))
        self.assertFalse(driver._admit_output("cpu", self.output("cpu").decode("ascii")))


if __name__ == "__main__":
    unittest.main()
