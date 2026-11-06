"""Public checks for authored inputs and strict retained-result admission."""

import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from authority import method_codes
from compare import call_plan, parse_trace, pcm_inventory
from fixtures import RECIPES, encode, materialize, profiles


INIT = "INIT-true\t0\ttrue\tfalse\tfalse\t-1\tNONE\tPCM_SIGNED\t1\t16\t2\t1190789120\t0\t12"
READ = "read\t1\ttrue\ttrue\ttrue\t6\tNONE\tPCM_SIGNED\t1\t16\t2\t1190789120\t0\t12"
CLEANUP = "CLEANUP\t2\ttrue\ttrue\tfalse\t-1\tNONE\tNONE\t0\t0\t0\t0\t0\t-1"


def trace(*rows):
    return ("\n".join(rows) + "\n").encode()


def tiny_class(opcode=0xb1):
    def utf8(text):
        raw = text.encode("ascii")
        return b"\1" + struct.pack(">H", len(raw)) + raw
    pool = utf8("Code") + utf8("m") + utf8("()V") + utf8("Test")
    pool += b"\7\0\4" + utf8("java/lang/Object") + b"\7\0\6"
    body = struct.pack(">HHI", 0, 0, 1) + bytes([opcode]) + struct.pack(">HH", 0, 0)
    return (struct.pack(">IHHH", 0xcafebabe, 0, 49, 8) + pool
            + struct.pack(">HHHHHH", 0x21, 5, 7, 0, 0, 1)
            + struct.pack(">HHHHHI", 9, 2, 3, 1, 1, len(body)) + body + b"\0\0")


class FixtureChecks(unittest.TestCase):
    def test_complete_inventory(self):
        self.assertEqual(len(RECIPES), 24)
        self.assertEqual(len(profiles()), 60)
        self.assertEqual(len({recipe.name for recipe in RECIPES}), 24)

    def test_riff_size_and_authored_pcm(self):
        raw = encode(RECIPES[1])
        self.assertEqual(raw[:12], b"RIFF" + struct.pack("<I", 60) + b"WAVE")
        self.assertEqual(raw[-24:], bytes((i * 17 + 3) & 255 for i in range(24)))
        self.assertEqual(struct.unpack_from("<I", raw, 40)[0], 24)

    def test_extensible_metadata(self):
        raw = encode(RECIPES[12])
        self.assertEqual(struct.unpack_from("<I", raw, 16)[0], 40)
        self.assertEqual(struct.unpack_from("<HHI", raw, 36), (22, 20, 3))
        self.assertEqual(raw[44:60], bytes.fromhex("0100000000001000800000aa00389b71"))

    def test_stereo_provider_alignment_input(self):
        raw = encode(RECIPES[6])
        self.assertEqual(struct.unpack_from("<HH", raw, 32), (1, 16))

    def test_fixture_materialization(self):
        with tempfile.TemporaryDirectory(prefix="wav-input-check-") as name:
            root = Path(name)
            pins = materialize(root)
            self.assertEqual(len(list(root.iterdir())), 24)
            for recipe in RECIPES:
                raw = (root / (recipe.name + ".wav")).read_bytes()
                self.assertEqual(raw, encode(recipe))
                self.assertEqual(pins[recipe.name]["sha256"], hashlib.sha256(raw).hexdigest())

    def test_special_profiles(self):
        selected = profiles()[36:42]
        self.assertEqual([row["streaming_size"] for row in selected], [0, -1, 1, 2, 24, 26])
        self.assertEqual([row["max_file_size"] for row in profiles()[42:45]], [1000, 1000, 0])


class TraceChecks(unittest.TestCase):
    def test_complete_trace(self):
        rows, diagnostics = parse_trace(trace(INIT, READ, CLEANUP), False)
        self.assertEqual(len(rows), 3)
        self.assertFalse(diagnostics)

    def test_diagnostics_retained_separately(self):
        rows, diagnostics = parse_trace(trace(INIT, "retained original diagnostic", READ, CLEANUP), True)
        self.assertEqual(len(rows), 3)
        self.assertEqual(diagnostics, ["retained original diagnostic"])
        with self.assertRaises(ValueError):
            parse_trace(trace(INIT, "unexpected native output", READ, CLEANUP), False)

    def test_missing_or_reordered_result(self):
        for raw in (trace(INIT, CLEANUP), trace(INIT, CLEANUP, READ), trace(INIT, READ)):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                parse_trace(raw, False)

    def test_presence_length_join(self):
        with self.assertRaises(ValueError):
            parse_trace(trace(INIT, READ.replace("true\t6\tNONE", "false\t6\tNONE"), CLEANUP), False)

    def test_format_and_exception_domain(self):
        for read in (READ.replace("PCM_SIGNED", "FLAC"), READ.replace("\tNONE\tPCM", "\tNATIVE\tPCM"),
                     READ.replace("1190789120", "4294967296")):
            with self.subTest(read=read), self.assertRaises(ValueError):
                parse_trace(trace(INIT, read, CLEANUP), False)

    def test_complete_buffer_files(self):
        rows, _ = parse_trace(trace(INIT, READ, CLEANUP), False)
        with tempfile.TemporaryDirectory(prefix="wav-buffer-check-") as name:
            root = Path(name)
            with self.assertRaises(ValueError):
                pcm_inventory(root, rows, False)
            (root / "pcm-1.bin").write_bytes(b"abcdef")
            data, _ = pcm_inventory(root, rows, False)
            self.assertEqual(data, {"pcm-1.bin": b"abcdef"})
            (root / "pcm-1.bin").write_bytes(b"abcde")
            with self.assertRaises(ValueError):
                pcm_inventory(root, rows, False)
            (root / "pcm-1.bin").write_bytes(b"abcdef")
            (root / "extra.bin").write_bytes(b"")
            with self.assertRaises(ValueError):
                pcm_inventory(root, rows, False)

    def test_fresh_read_all_plan_is_complete(self):
        rows, _ = parse_trace(trace(INIT, READ, CLEANUP), False)
        with self.assertRaises(ValueError):
            call_plan(rows, {"mode": "all"})

    def test_read_loop_must_reach_eos_or_limit(self):
        raw = trace(INIT, READ.replace("\ttrue\ttrue\ttrue\t6", "\ttrue\tfalse\ttrue\t6"),
                    READ.replace("read\t1", "read\t2"),
                    READ.replace("read\t1", "readAll\t3"), CLEANUP.replace("\t2\t", "\t4\t", 1))
        rows, _ = parse_trace(raw, False)
        with self.assertRaises(ValueError):
            call_plan(rows, {"mode": "read"})


class AuthorityChecks(unittest.TestCase):
    def test_synthetic_method_pin(self):
        result = method_codes(tiny_class())
        self.assertEqual((result["major"], result["minor"]), (49, 0))
        self.assertEqual(result["methods"]["m()V"]["code_bytes"], 1)
        self.assertEqual(result["methods"]["m()V"]["code_sha256"], hashlib.sha256(b"\xb1").hexdigest())

    def test_instruction_change_changes_pin(self):
        self.assertNotEqual(method_codes(tiny_class())["methods"],
                            method_codes(tiny_class(0))["methods"])

    def test_truncated_class_rejected(self):
        raw = tiny_class()
        for size in range(len(raw)):
            with self.subTest(size=size), self.assertRaises((ValueError, IndexError)):
                method_codes(raw[:size])


if __name__ == "__main__":
    unittest.main()
