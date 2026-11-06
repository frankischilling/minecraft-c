"""Finite coordinate receipt and corruption controls; no original execution."""

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import HERE, SOURCES, digest, read_reference, rows


def copy_packet(root):
    directory = Path(root) / "tests/reference/entity/player_coordinates"
    directory.mkdir(parents=True)
    for name in (*SOURCES, "selected.txt", "provenance.json"):
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(HERE / name, target)
    return directory


class CoordinatePacketControls(unittest.TestCase):
    def test_positive(self):
        self.assertEqual(len(read_reference()), 5)

    def test_packet_rows_and_digests(self):
        for mode in range(7):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="coordinate-packet-") as root:
                directory = copy_packet(root)
                selected = directory / "selected.txt"
                text = selected.read_text(encoding="ascii")
                if mode == 0:
                    selected.write_text(text.replace("same=1", "same=0"), encoding="ascii")
                elif mode == 1:
                    selected.write_text(text.replace("4012000000000000", "4012000000000001", 1), encoding="ascii")
                elif mode == 2:
                    selected.write_text("\n".join(text.splitlines()[1:]) + "\n", encoding="ascii")
                elif mode == 3:
                    selected.write_text(text + text.splitlines()[0] + "\n", encoding="ascii")
                elif mode == 4:
                    (directory / "PlayerCoordinatesObservation.java").write_text("changed", encoding="ascii")
                elif mode == 5:
                    selected.write_text(text + "extra\n", encoding="ascii")
                    provenance = json.loads((directory / "provenance.json").read_text())
                    provenance["selected_sha256"] = digest(selected)
                    provenance["selected_per_vm_sha256"] = [digest(selected)] * 2
                    (directory / "provenance.json").write_text(json.dumps(provenance))
                else:
                    lines = text.splitlines()
                    lines[1], lines[2] = lines[2], lines[1]
                    self.assertRaises(ValueError, rows, "\n".join(lines))
                    selected.write_text("\n".join(lines) + "\n", encoding="ascii")
                with self.assertRaises(ValueError):
                    read_reference(directory)

    def test_capture_qualification(self):
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
            with self.subTest(change=change), tempfile.TemporaryDirectory(prefix="coordinate-qualify-") as root:
                directory = copy_packet(root)
                path = directory / "provenance.json"
                provenance = json.loads(path.read_text())
                change(provenance)
                path.write_text(json.dumps(provenance), encoding="utf-8")
                with self.assertRaises(ValueError):
                    read_reference(directory)

    def test_declared_inputs_and_duplicate_json(self):
        with tempfile.TemporaryDirectory(prefix="coordinate-inputs-") as root:
            directory = copy_packet(root)
            path = directory / "finite.json"
            finite = json.loads(path.read_text())
            finite["null_bounds_call"][0] = True
            path.write_text(json.dumps(finite))
            provenance = json.loads((directory / "provenance.json").read_text())
            provenance["source_sha256"]["finite.json"] = digest(path)
            (directory / "provenance.json").write_text(json.dumps(provenance))
            with self.assertRaises(ValueError):
                read_reference(directory)
            (directory / "provenance.json").write_text('{"schema":0,"schema":1}')
            with self.assertRaises(ValueError):
                read_reference(directory)


if __name__ == "__main__":
    unittest.main()
