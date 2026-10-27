"""Pure-data SDK footer controls; no compiler, native child or JVM."""
import unittest
from protocol import footer


class ProtocolTests(unittest.TestCase):
    def test_four_actual_footer_families(self):
        for kind in ("queue", "icons", "transport", "window"):
            raw = ("SDK_" + kind.upper() + "_OK|checks=1\n").encode()
            self.assertEqual(footer(raw, kind)["checks"], 1)
            self.assertEqual(footer(raw.replace(b"\n", b"\r\n"), kind)["checks"], 1)

    def test_separator_and_record_negatives(self):
        good = b"SDK_QUEUE_OK|checks=1\n"
        negatives = [good.replace(b"\n", separator) for separator in (b"\r", b"\v", b"\f", b"\x1c", b"\x85")]
        negatives += [good + good, b"prefix\n" + good, good + b"\n", good[:-1], b"", good + b"extra"]
        for raw in negatives:
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                footer(raw, "queue")

    def test_typed_positive_counter(self):
        for value in (b"0", b"-1", b"01", b"+1", b"1.0", b"true", b"1000001"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                footer(b"SDK_QUEUE_OK|checks=" + value + b"\n", "queue")

    def test_wrong_family_and_nonascii(self):
        for raw in (b"SDK_WINDOW_OK|checks=1\n", b"SDK_QUEUE_OK|checks=1\x00\n", b"SDK_QUEUE_OK|checks=1\t\n"):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                footer(raw, "queue")


if __name__ == "__main__":
    unittest.main()
