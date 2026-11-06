"""Corruption controls for the bounded autosave observer/disk comparator."""
import copy
import gzip
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('attack_autosave', HERE / 'attack_autosave.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
nbt = audit.load('autosave_test_nbt', audit.ROOT / 'tests/tools/world/test_source_saving.py')


def tag(kind, name, value):
    if kind == 10:
        value = nbt.compound(value)
    elif kind == 9:
        child, items = value
        value = bytes([child]) + struct.pack('>i', len(items)) + b''.join(items)
    elif kind == 8:
        value = nbt.text(value)
    elif kind == 7:
        value = struct.pack('>i', len(value)) + value
    else:
        value = struct.pack('>' + {1: 'b', 2: 'h', 3: 'i', 4: 'q'}[kind], value)
    return nbt.named(kind, name, value)


def region(tree, stamp, previous=None, sector=2):
    compressed = zlib.compress(tree)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    sectors = (len(record) + 4095) // 4096
    image = bytearray(previous) if previous is not None else bytearray(8192)
    image.extend(bytes(max(0, (sector + sectors) * 4096 - len(image))))
    struct.pack_into('>I', image, 0, (sector << 8) | sectors)
    struct.pack_into('>I', image, 4096, stamp)
    image[sector * 4096:sector * 4096 + len(record)] = record
    return bytes(image)


class AutosaveControls(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.event = self.root / 'autosaves/save-0'
        self.event.mkdir(parents=True)
        old_metadata, old_region = nbt.seed('item-chest')
        self.before = {'alpha/level.dat': old_metadata,
            'alpha/region/r.0.0.mcr': bytes(old_region), 'older/preserve.dat': b'untouched'}
        self.entry = dict(time=39, next_time=40, period=40, first_millis=1700000000000,
            seed=23, size=12288, spawn=[8, 64, 8], rain_time=600, thunder_time=700,
            raining=False, thundering=False, name_units=list(map(ord, 'Attack world')),
            cache=[dict(identity='1234', x=0, z=0, blank=False, never_save=False,
                        dirty=True, has_entities=False, last_save=0)],
            selected=[dict(cache_index=0, x=0, z=0, populated=True, entities=0)],
            entity_list=dict(phase=0, bytes=4096, maps=['0'] * 4, counts=[0] * 4),
            entity_list_calls=0, observer_unchanged=True, unnamed_player_excluded=True)
        self.exit = dict(time=39, size=12288, last_millis=1700000000999, source_returned=True,
                         entity_list=copy.deepcopy(self.entry['entity_list']))
        self.raw = bytes(32768 + 3 * 16384 + 256)
        player = [tag(3, 'Score', 14), tag(2, 'Health', 20), tag(9, 'Inventory', (1, []))]
        observer = [tag(3, 'Score', 14), tag(2, 'Health', 20), tag(9, 'Inventory', (10, []))]
        (self.event / 'player.nbt').write_bytes(tag(10, '', observer))
        (self.event / 'chunk-0.bin').write_bytes(self.raw)
        level = [tag(3, 'xPos', 0), tag(3, 'zPos', 0), tag(4, 'LastUpdate', 39)]
        for key, size in [('Blocks', 32768), ('Data', 16384), ('SkyLight', 16384),
                          ('BlockLight', 16384), ('HeightMap', 256)]:
            level.append(tag(7, key, bytes(size)))
        level.extend([tag(1, 'TerrainPopulated', 1), tag(9, 'Entities', (1, [])),
                      tag(9, 'TileEntities', (1, []))])
        self.chunk_tree = tag(10, '', [tag(10, 'Level', level)])
        fields = [tag(4, 'RandomSeed', 23), tag(4, 'Time', 39), tag(4, 'SizeOnDisk', 12288),
                  tag(4, 'LastPlayed', 1700000000500), tag(8, 'LevelName', 'Attack world'),
                  tag(3, 'version', 19132), tag(3, 'rainTime', 600), tag(1, 'raining', 0),
                  tag(3, 'thunderTime', 700), tag(1, 'thundering', 0),
                  tag(3, 'SpawnX', 8), tag(3, 'SpawnY', 64), tag(3, 'SpawnZ', 8),
                  tag(10, 'Player', player)]
        self.after = dict(self.before, **{'alpha/session.lock': bytes(8),
            'alpha/level.dat_old': old_metadata,
            'alpha/level.dat': gzip.compress(tag(10, '', [tag(10, 'Data', fields)]), mtime=0),
            'alpha/region/r.0.0.mcr': region(self.chunk_tree, 1700000000, old_region)})
        self.save(self.root / 'before', self.before)
        self.save(self.event / 'before', dict(self.before, **{'alpha/session.lock': bytes(8)}))
        for folder in (self.event / 'after', self.root / 'after', self.root / 'after-verified'):
            self.save(folder, self.after)
        self.manifests()
        self.stdout = ('ATTACK_AUTOSAVE_ENTRY event=0 time=39 next=40 period=40 selected=1 '
            'source=fd.l:177 focused=1\nATTACK_AUTOSAVE_RETURN event=0 time=39 selected=1 '
            'source-returned=1 time-store-not-entered=1\nATTACK_AUTOSAVE_POST_DEATH event=0 '
            'world-remove=1 player-score=14\nATTACK_AUTOSAVE_SUMMARY entries=1 '
            'returns=1 passive-observer=1 normal-timer=1\n')

    def tearDown(self):
        self.temporary.cleanup()

    def save(self, folder, images):
        for name, data in images.items():
            path = folder / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def manifests(self):
        for name, value in [('entry', self.entry), ('exit', self.exit)]:
            (self.event / (name + '.json')).write_text(json.dumps(value))

    def fails(self):
        with self.assertRaises(AssertionError):
            audit.audit(self.root, self.stdout)

    def test_complete_packet(self):
        self.assertEqual(audit.audit(self.root, self.stdout)[0]['time_saved_before_store'], 39)

    def test_wrong_modulo_and_postincrement_time(self):
        for key, value in [('time', 40), ('next_time', 41), ('period', 20)]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.entry)
                changed[key] = value
                with self.assertRaises(AssertionError):
                    audit.selection(changed)

    def test_eligibility_order_and_limit(self):
        entry = copy.deepcopy(self.entry)
        entry['cache'] *= 30
        entry['cache'] = [dict(row, identity=hex(i + 1)[2:], x=i) for i, row in enumerate(entry['cache'])]
        entry['cache'][0]['dirty'] = False
        entry['cache'][1].update(dirty=False, has_entities=True, last_save=-561)
        entry['selected'] = [dict(cache_index=i, x=i, z=0, populated=True, entities=0)
                             for i in range(1, 25)]
        self.assertEqual(len(audit.selection(entry)), 24)
        entry['cache'][1]['last_save'] += 1
        with self.assertRaises(AssertionError):
            audit.selection(entry)

    def test_every_raw_array_byte(self):
        for index in (0, 32768, 49152, 65536, 81920, 82175):
            raw = bytearray(self.raw)
            raw[index] = 1
            (self.event / 'chunk-0.bin').write_bytes(raw)
            self.fails()

    def test_player_fields_and_floating_types(self):
        (self.event / 'player.nbt').write_bytes(tag(10, '', [tag(3, 'Score', 15)]))
        self.fails()
        self.assertNotEqual(audit.typed([6, 0]), audit.typed([6, 1 << 63]))
        self.assertNotEqual(audit.typed([3, 14]), audit.typed([2, 14]))

    def test_backup_and_other_world_continuity(self):
        (self.event / 'after/alpha/level.dat_old').write_bytes(b'wrong rotation')
        self.fails()

    def test_unobserved_write(self):
        (self.root / 'after-verified/older/preserve.dat').write_bytes(b'changed outside save')
        self.fails()

    def test_unselected_region_header_and_free_sector(self):
        images = dict(self.before)
        changed = bytearray(images['alpha/region/r.0.0.mcr'])
        changed[4100] = 1
        images['alpha/region/r.0.0.mcr'] = bytes(changed)
        with self.assertRaises(AssertionError):
            audit.region_changes(self.before, images, {(0, 0)}, nbt)
        images = dict(self.before)
        images['alpha/region/r.0.0.mcr'] += b'\1' + bytes(4095)
        with self.assertRaises(AssertionError):
            audit.region_changes(self.before, images, {(0, 0)}, nbt)

    def test_actual_saved_time_and_source_empty_list_type(self):
        original = gzip.decompress(self.after['alpha/level.dat'])
        for old, new in [(tag(4, 'Time', 39), tag(4, 'Time', 40)),
                         (tag(9, 'Inventory', (1, [])), tag(9, 'Inventory', (10, [])))]:
            with self.subTest(old=old):
                changed = original.replace(old, new, 1)
                self.assertNotEqual(changed, original)
                (self.event / 'after/alpha/level.dat').write_bytes(gzip.compress(changed, mtime=0))
                self.fails()

    def test_actual_selected_region_clock(self):
        changed = bytearray(self.after['alpha/region/r.0.0.mcr'])
        struct.pack_into('>I', changed, 4096, 1700000001)
        (self.event / 'after/alpha/region/r.0.0.mcr').write_bytes(changed)
        self.fails()

    def test_selected_reused_tail_padding_is_unchanged(self):
        changed = bytearray(self.after['alpha/region/r.0.0.mcr'])
        changed[12287] ^= 0x5a
        (self.event / 'after/alpha/region/r.0.0.mcr').write_bytes(changed)
        self.fails()

    def test_relocated_abandoned_payload_and_appended_padding(self):
        # Force a larger compressed record. Preserve the abandoned sector2
        # exactly, as the source allocator/writer does when appending.
        noise, state = bytearray(), 0x12345678
        for _ in range(16000):
            state ^= (state << 13) & 0xffffffff
            state ^= state >> 17
            state ^= (state << 5) & 0xffffffff
            noise.append(state & 255)
        tree = tag(10, '', [tag(10, 'Level', [tag(3, 'xPos', 0), tag(3, 'zPos', 0),
                                           tag(7, 'Blocks', bytes(noise))])])
        name = 'alpha/region/r.0.0.mcr'
        moved = region(tree, 1700000000, self.before[name], sector=3)
        images = dict(self.before, **{name: moved})
        audit.region_changes(self.before, images, {(0, 0)}, nbt)
        for index in (8198, len(moved) - 1):
            with self.subTest(index=index):
                changed = bytearray(moved)
                changed[index] ^= 0x5a
                with self.assertRaises(AssertionError):
                    audit.region_changes(self.before, dict(images, **{name: bytes(changed)}),
                                         {(0, 0)}, nbt)

    def test_entire_ordered_entity_list(self):
        self.entry['selected'][0]['entities'] = 1
        self.manifests()
        (self.event / 'chunk-0-entity-0.nbt').write_bytes(tag(10, '', [tag(8, 'id', 'Item')]))
        self.fails()

    def test_cold_entity_list_observer_admission(self):
        audit.entity_list(self.entry, self.exit)
        for key, value in [('observer_unchanged', False), ('unnamed_player_excluded', False)]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.entry)
                changed[key] = value
                with self.assertRaises(AssertionError):
                    audit.entity_list(changed, self.exit)
        for key, value in [('phase', 2), ('bytes', 4097), ('maps', ['1', '0', '0', '0']),
                           ('counts', [1, 0, 0, 0])]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.exit)
                changed['entity_list'][key] = value
                with self.assertRaises(AssertionError):
                    audit.entity_list(self.entry, changed)

    def test_only_real_slice_calls_can_initialize_entity_list(self):
        changed = copy.deepcopy(self.exit)
        changed['entity_list'] = dict(phase=2, bytes=6496, maps=['1', '2', '3', '4'],
                                      counts=[24] * 4)
        with self.assertRaises(AssertionError):
            audit.entity_list(self.entry, changed)
        called = copy.deepcopy(self.entry)
        called['entity_list_calls'] = 1
        audit.entity_list(called, changed)
        changed['entity_list']['bytes'] = 4096
        with self.assertRaises(AssertionError):
            audit.entity_list(called, changed)

    def test_failed_or_unfocused_receipt(self):
        for old, new in [('focused=1', 'focused=0'), ('source-returned=1', 'source-returned=0'),
                         ('entries=1', 'entries=2')]:
            with self.subTest(old=old):
                saved = self.stdout
                self.stdout = saved.replace(old, new)
                self.fails()
                self.stdout = saved


if __name__ == '__main__':
    unittest.main()
