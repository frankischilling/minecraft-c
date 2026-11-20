"""Constructed decoder/protocol controls; never launches a native process."""
import gzip
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


driver = load('stage_progress_driver', HERE / 'test_stage_progress.py')
source = load('stage_progress_source', HERE / 'test_source_saving.py')
owned = load('stage_progress_owned', HERE / 'test_owned_cache.py')


class StageProgressDriverTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sequence = 0

    def replace_once(self, data, before, after):
        self.assertEqual(data.count(before), 1)
        return data.replace(before, after, 1)

    def record(self, image, slot, transform):
        result = bytearray(image)
        allocation, = struct.unpack_from('>I', result, slot * 4)
        sector, count = allocation >> 8, allocation & 255
        start = sector * 4096
        size, = struct.unpack_from('>i', result, start)
        wire = zlib.decompress(result[start + 5:start + 4 + size])
        compressed = zlib.compress(transform(wire))
        self.assertLessEqual(len(compressed) + 5, count * 4096)
        payload = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
        result[start:start + count * 4096] = payload + bytes(count * 4096 - len(payload))
        return bytes(result)

    def written(self, wire):
        # Literal Source output shape: fresh root/Level, canonical BYTE1 empty
        # lists. This is parser test data, never an alternate runtime writer.
        for key, value in (('RootExtra', 901), ('LevelExtra', 902)):
            wire = self.replace_once(wire, source.named(4, key, struct.pack('>q', value)), b'')
        for key in ('Entities', 'TileEntities'):
            wire = self.replace_once(wire, source.named(9, key, b'\12' + bytes(4)),
                                     source.named(9, key, b'\1' + bytes(4)))
        return wire

    def case(self, mode='stage-success'):
        self.sequence += 1
        case = self.root / str(self.sequence)
        case.mkdir()
        owned.source_inputs(source, case, False)
        snapshots = case / 'snapshots'
        snapshots.mkdir()
        metadata = (case / 'world/level.dat').read_bytes()
        region = (case / 'world/region/r.0.0.mcr').read_bytes()
        (snapshots / 'before-level.dat').write_bytes(metadata)
        (snapshots / 'before-region.mcr').write_bytes(region)
        if mode not in ('stage-level-fatal', 'stage-export'):
            raw = self.replace_once(gzip.decompress(metadata),
                                    source.named(4, 'LastPlayed', struct.pack('>q', 1304000000000)),
                                    source.named(4, 'LastPlayed', struct.pack('>q', 2000)))
            metadata = gzip.compress(raw, mtime=0)
        if mode in ('stage-success', 'stage-null', 'stage-reentry'):
            for slot in (0, 66):
                region = self.record(region, slot, self.written)
        (snapshots / 'after-level.dat').write_bytes(metadata)
        (snapshots / 'after-region.mcr').write_bytes(region)
        # Explicit expected native receipts for parser tests only.
        values = {
            'stage-success': ('2', '0', '0', '1', '0', '0', '1'),
            'stage-level-fatal': ('1', '0', '3', '1', '0', '1', '0'),
            'stage-chunks-fatal': ('2', '0', '3', '1', '0', '1', '1'),
            'stage-null': ('0', '0', '0', '1', '0', '0', '0'),
            'stage-reentry': ('2', '1', '0', '1', '0', '0', '1'),
            'stage-export': ('1', '0', '3', '0', '1', '1', '0'),
        }
        calls, nested, status, retained, exports, dirty, metadata_effect = values[mode]
        stdout = (f'STAGE_PROGRESS_CASE mode={mode} calls={calls} nested={nested} blocked=0 '
                  f'status={status} owned={retained} raw_exports={exports} dirty={dirty} '
                  f'metadata={metadata_effect} binding=1 balanced=1\n'
                  f'STAGE_PROGRESS_FINISH mode={mode} checks=63 cleanup=1 genuine-source-world=1\n')
        return case, stdout.encode('ascii')

    def audit(self, case, stdout, mode='stage-success', stderr=b'', returncode=0):
        return driver.audit_case(source, owned, mode, case, 1000, 3000,
                                 returncode, stdout, stderr)

    def change_record(self, case, slot, transform):
        path = case / 'snapshots/after-region.mcr'
        path.write_bytes(self.record(path.read_bytes(), slot, transform))

    def test_six_declared_shapes(self):
        for mode in driver.MODES:
            with self.subTest(mode=mode):
                case, stdout = self.case(mode)
                self.assertEqual(self.audit(case, stdout, mode)['status'], 'PASS')

    def test_written_empty_lists_require_byte_one(self):
        for slot in (0, 66):
            for key in ('Entities', 'TileEntities'):
                for kind in (0, 2, 10):
                    with self.subTest(slot=slot, key=key, kind=kind):
                        case, stdout = self.case()
                        self.change_record(case, slot, lambda wire: self.replace_once(
                            wire, source.named(9, key, b'\1' + bytes(4)),
                            source.named(9, key, bytes([kind]) + bytes(4))))
                        with self.assertRaises(AssertionError):
                            self.audit(case, stdout)

    def test_written_list_payload_is_not_ignored(self):
        case, stdout = self.case()
        self.change_record(case, 0, lambda wire: self.replace_once(
            wire, source.named(9, 'Entities', b'\1' + bytes(4)),
            source.named(9, 'Entities', b'\1' + struct.pack('>i', 1) + b'\7')))
        with self.assertRaises(AssertionError):
            self.audit(case, stdout)

    def test_written_scalar_and_schema_stay_exact(self):
        for mutation in ('time', 'root-extra', 'level-extra'):
            with self.subTest(mutation=mutation):
                case, stdout = self.case()

                def change(wire):
                    if mutation == 'time':
                        return self.replace_once(wire, source.named(4, 'LastUpdate', struct.pack('>q', 6000)),
                                                 source.named(4, 'LastUpdate', struct.pack('>q', 6001)))
                    if mutation == 'root-extra':
                        return wire[:3] + source.named(3, 'unexpected', bytes(4)) + wire[3:]
                    return self.replace_once(wire, source.named(3, 'xPos', bytes(4)),
                                             source.named(3, 'xPos', bytes(4)) +
                                             source.named(3, 'unexpected', bytes(4)))

                self.change_record(case, 0, change)
                with self.assertRaises(AssertionError):
                    self.audit(case, stdout)

    def test_unwritten_list_type_is_not_normalized(self):
        case, stdout = self.case()
        self.change_record(case, 231, lambda wire: self.replace_once(
            wire, source.named(9, 'Entities', b'\12' + bytes(4)),
            source.named(9, 'Entities', b'\1' + bytes(4))))
        with self.assertRaises(AssertionError):
            self.audit(case, stdout)

    def test_unwritten_record_padding_timestamp_and_location_are_exact(self):
        for mutation in ('padding', 'timestamp', 'location'):
            with self.subTest(mutation=mutation):
                case, stdout = self.case()
                path = case / 'snapshots/after-region.mcr'
                data = bytearray(path.read_bytes())
                allocation, = struct.unpack_from('>I', data, 231 * 4)
                sector, count = allocation >> 8, allocation & 255
                if mutation == 'padding':
                    data[(sector + count) * 4096 - 1] ^= 1
                elif mutation == 'timestamp':
                    data[4096 + 231 * 4] ^= 1
                else:
                    new_sector = len(data) // 4096
                    data += data[sector * 4096:(sector + count) * 4096]
                    struct.pack_into('>I', data, 231 * 4, (new_sector << 8) | count)
                path.write_bytes(data)
                with self.assertRaises(AssertionError):
                    self.audit(case, stdout)

    def test_failure_prefixes_keep_whole_files(self):
        for mode, name in (('stage-level-fatal', 'after-level.dat'),
                           ('stage-chunks-fatal', 'after-region.mcr')):
            with self.subTest(mode=mode):
                case, stdout = self.case(mode)
                path = case / 'snapshots' / name
                data = bytearray(path.read_bytes())
                data[4 if name.endswith('.dat') else -1] ^= 1
                path.write_bytes(data)
                with self.assertRaises(AssertionError):
                    self.audit(case, stdout, mode)

    def test_receipt_effects_and_schema_are_closed(self):
        case, stdout = self.case()
        for before, after in ((b'calls=2', b'calls=1'), (b'status=0', b'status=3'),
                              (b'nested=0', b'nested=1'), (b'raw_exports=0', b'raw_exports=1'),
                              (b'cleanup=1', b'cleanup=0'),
                              (b'calls=2', b'calls=2 calls=2')):
            with self.subTest(before=before, after=after):
                with self.assertRaises(AssertionError):
                    self.audit(case, stdout.replace(before, after))

    def test_native_failure_and_sanitizers_are_retained(self):
        case, stdout = self.case()
        with self.assertRaises(AssertionError):
            self.audit(case, stdout, returncode=1)
        with self.assertRaises(AssertionError):
            self.audit(case, stdout, stderr=b'ERROR: AddressSanitizer: test fixture')


if __name__ == '__main__':
    unittest.main()
