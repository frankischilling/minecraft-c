"""Authored decoder corruption controls; these synthetic words are not render evidence."""

from pathlib import Path
import json
import shutil
import tempfile
import unittest

from packet import HERE, SOURCES, digest, read_packet, read_reference, rows


def copy_packet(root):
    directory = Path(root) / "tests/reference/client/bed_world"
    directory.mkdir(parents=True)
    for name in (*SOURCES, "selected.txt", "provenance.json"):
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(HERE / name, target)
    return directory


def fixture():
    result = []
    for metadata in range(16):
        for override in (-1, 17):
            for profile in range(6):
                vertices = 12 if profile == 1 else 36 if profile == 4 else 30
                bounds = ",".join(["0000000000000000"] * 3 +
                                  ["3ff0000000000000", "3fe2000000000000", "3ff0000000000000"])
                result.append("\t".join(["BED", str(metadata), str(override), str(profile), "true",
                                        str(override), "false", "true" if profile == 3 else "false",
                                        str(vertices), bounds, ",".join(["00000000"] * (vertices * 8))]))
    return result


class BedPacketControls(unittest.TestCase):
    def test_real_source_bound_packet(self):
        self.assertEqual(len(read_reference()), 192)

    def test_complete_authored_decoder_input(self):
        expected = fixture()
        self.assertEqual(rows("\n".join(expected)), expected)

    def test_order_inventory_and_reached_fields(self):
        expected = fixture()
        corruptions = [expected[:-1], expected + expected[:1], expected[1:] + expected[:1]]
        for field, value in ((1, "1"), (2, "0"), (3, "3"), (4, "false"), (5, "17"),
                             (6, "true"), (7, "true"), (8, "20"), (9, "0"), (10, "00000000")):
            changed = expected.copy()
            parts = changed[0].split("\t")
            parts[field] = value
            changed[0] = "\t".join(parts)
            corruptions.append(changed)
        for changed in corruptions:
            with self.subTest(first=changed[0][:80]), self.assertRaises(ValueError):
                rows("\n".join(changed))

    def test_strict_packet_and_word_inventory(self):
        text = "\n".join(fixture()) + "\n"
        with tempfile.TemporaryDirectory(prefix="bed-decoder-") as directory:
            path = Path(directory) / "packet.txt"
            path.write_text(text, encoding="ascii")
            self.assertEqual(len(read_packet(path)), 192)
            for changed in (text + "extra\n", text[:-1], text.replace("BED\t0", "BED\t00", 1),
                            text.replace("00000000,", "xxxxxxxx,", 1)):
                path.write_text(changed, encoding="ascii")
                with self.assertRaises(ValueError):
                    read_packet(path)

    def test_captured_sources_and_receipts(self):
        changes = [lambda p: p.update(status="FAIL"), lambda p: p.update(rows_per_vm=True),
                   lambda p: p.update(ordinary_vms=1), lambda p: p.update(input_unchanged=False),
                   lambda p: p.update(helper_unchanged=False), lambda p: p.update(archive_sha256="0"*64),
                   lambda p: p["original"][0].update(exit=True),
                   lambda p: p["original"][0].update(timed_out=True),
                   lambda p: p["original"][0].update(pipe_capture_complete=False),
                   lambda p: p["original"][0].update(reader_errors={"stdout": ["failure"]}),
                   lambda p: p["original"][0].update(output_overflow=True),
                   lambda p: p["original"][0].update(stdout_bytes=True),
                   lambda p: p.update(selected_per_vm_sha256=["0"*64]*2),
                   lambda p: p["source_sha256"].update(extra="0"*64)]
        for change in changes:
            with self.subTest(change=change), tempfile.TemporaryDirectory(prefix="bed-qualify-") as root:
                directory = copy_packet(root)
                path = directory / "provenance.json"
                provenance = json.loads(path.read_text())
                change(provenance)
                path.write_text(json.dumps(provenance), encoding="utf-8")
                with self.assertRaises(ValueError):
                    read_reference(directory)

    def test_source_or_word_or_input_corruption(self):
        for mode in range(4):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="bed-bindings-") as root:
                directory = copy_packet(root)
                if mode == 0:
                    (directory / "BedObservation.java").write_text("changed", encoding="ascii")
                elif mode == 1:
                    path = directory / "selected.txt"
                    text = path.read_text(encoding="ascii")
                    path.write_text(text.replace("00000000,", "00000001,", 1), encoding="ascii")
                elif mode == 2:
                    path = directory / "finite.json"
                    finite = json.loads(path.read_text())
                    finite["initial_flip"] = 1
                    path.write_text(json.dumps(finite))
                    provenance = json.loads((directory / "provenance.json").read_text())
                    provenance["source_sha256"]["finite.json"] = digest(path)
                    (directory / "provenance.json").write_text(json.dumps(provenance))
                else:
                    (directory / "provenance.json").write_text('{"schema":0,"schema":1}')
                with self.assertRaises(ValueError):
                    read_reference(directory)


if __name__ == "__main__":
    unittest.main()
