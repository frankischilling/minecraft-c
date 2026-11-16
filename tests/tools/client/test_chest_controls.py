"""Portable receipt/NBT/image corruption controls; no native or original gameplay evidence."""
import copy
import gzip
import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

HERE = Path(__file__).resolve().parent
S = importlib.util.spec_from_file_location('chest_controls_subject', HERE / 'test_chest_loop.py')
G = importlib.util.module_from_spec(S)
S.loader.exec_module(G)
D = G.module('chest_controls_nbt', G.ROOT / 'tests/tools/world/test_source_saving.py')
MONITOR = {'selected': {'work': [-1920, 0, 1920, 1040]}}

# Exact stdout from placement-j24hz5oz, retained as decoder input only. This
# does not execute a native fixture or qualify the current source revision.
PLACEMENT_TRANSPORT = """16 achievements
151 recipes
WORLD_SELECTION_MONITOR stage=open window=frame x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040
WORLD_SELECTION_MONITOR stage=open window=canvas x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040
WORLD_SAVE_TITLE_HOST stage=open frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1
CHEST_PLACE_TAP target=0,63,2 placed=1 attempts=1 clicks=1 block-use=1 admitted=1 setters=1 sounds=1 count-stores=1
CHEST_PLACE_TAP target=0,64,2 placed=0 attempts=1 clicks=1 block-use=1 admitted=0 setters=0 sounds=0 count-stores=0
CHEST_STRAFE attempt=1 target=1,63,2 key=A x=0.92908222422795428
CHEST_STRAFE attempt=2 target=1,63,2 key=A x=2.0074382539138549
CHEST_STRAFE attempt=3 target=1,63,2 key=D x=1.8054552663941426
CHEST_PLACE_TAP target=1,63,2 placed=1 attempts=1 clicks=1 block-use=1 admitted=1 setters=1 sounds=1 count-stores=1
CHEST_STRAFE attempt=1 target=2,63,2 key=A x=1.7960697192563173
CHEST_STRAFE attempt=2 target=2,63,2 key=A x=1.7951784889629629
CHEST_STRAFE attempt=3 target=2,63,2 key=A x=1.9918313466477171
CHEST_STRAFE attempt=4 target=2,63,2 key=A x=2.0078910037460895
CHEST_STRAFE attempt=5 target=2,63,2 key=A x=2.2162462943658507
CHEST_PLACE_TAP target=2,63,2 placed=0 attempts=1 clicks=1 block-use=1 admitted=0 setters=0 sounds=0 count-stores=0
CHEST_STRAFE attempt=1 target=1,64,2 key=D x=2.0296817290675091
CHEST_STRAFE attempt=2 target=1,64,2 key=D x=2.0140519632075042
CHEST_STRAFE attempt=3 target=1,64,2 key=D x=1.8057761951018101
CHEST_PLACE_TAP target=1,64,2 placed=0 attempts=1 clicks=1 block-use=1 admitted=0 setters=0 sounds=0 count-stores=0
WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=28 resumed=1
WORLD_SAVE_TITLE_ESCAPE keyboard-before=28 keyboard-after=30 fresh-pause=1 distinct-recovered=1
WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=98 time=75 size-before=3162112 size-after=3563520 entered=1 bytes=1242290 copied-before-frame=1
WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=98 time=75 size-before=3162112 size-after=3563520 first=1794804251320 last=1794804251447 entry-frame=0 entry-bytes=1242290
WORLD_RELOAD_SNAPSHOT phase=chest-saved files=13 bytes=1643697 actual-file-copy=1
CHEST_PLACE_TAP target=1,64,2 placed=0 attempts=1 clicks=1 block-use=1 admitted=0 setters=0 sounds=0 count-stores=0
WORLD_SELECTION_MONITOR stage=after-chest-reload window=frame x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040
WORLD_SELECTION_MONITOR stage=after-chest-reload window=canvas x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040
WORLD_SAVE_TITLE_HOST stage=after-chest-reload frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1
CHEST_PLACEMENT placed=2 rejected=1 transferred=5 held=1 sdk-strafe=1 source-field-repair=0
CHEST_SDK profile=placement ordinary-save-reload=1 epochs=2 source-field-repair=0
CHEST_FINISH profile=placement cleanup=1 windows-absent=1 checks=1464
"""


def transport(profile='double'):
    if profile == 'placement':
        return PLACEMENT_TRANSPORT
    lines = ['16 achievements', '151 recipes']
    stages = ('open', 'open-chest-terminal' if profile == 'shutdown' else 'after-chest-reload')
    for stage in stages:
        for window in ('frame', 'canvas'):
            lines.append(f'WORLD_SELECTION_MONITOR stage={stage} window={window} '
                         'x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040')
        lines.append(f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 '
                     'display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1')
    if profile == 'shutdown':
        lines.extend((G.OPEN_PREPARE, 'CHEST_OPEN_SHUTDOWN actual-open-hp=1 source-close=0'))
    else:
        lines.append(f'CHEST_SDK profile={profile} ordinary-save-reload=1 epochs=2 source-field-repair=0')
    lines.append(f'CHEST_FINISH profile={profile} cleanup=1 windows-absent=1 checks=7')
    return '\n'.join(lines) + '\n'


def item(slot, item_id, count):
    return {'Slot': [1, slot], 'id': [2, item_id], 'Count': [1, count], 'Damage': [2, 0]}


def payload(kind, value):
    if kind in (1, 2, 3, 4, 5, 6):
        return struct.pack({1: '>b', 2: '>h', 3: '>i', 4: '>q', 5: '>I', 6: '>Q'}[kind], value)
    if kind == 7:
        return struct.pack('>i', len(value)) + bytes(value)
    if kind == 8:
        return D.text(value)
    if kind == 9:
        child, values = value
        return bytes([child]) + struct.pack('>i', len(values)) + b''.join(payload(child, item) for item in values)
    if kind == 10:
        return b''.join(D.named(tag[0], name, payload(*tag)) for name, tag in value.items()) + b'\0'
    raise AssertionError('undeclared synthetic NBT type')


def graph(profile):
    dense, initial = profile == 'dense', profile in ('blocked', 'shutdown')
    placement = profile == 'placement'
    inventory = ([item(slot, 264, 2) for slot in range(36)] if dense else
                 [item(0, 54, 1)] if placement else [] if initial else [item(8, 266, 3)])
    metadata = {'Data': [10, {'Player': [10, {'Dimension': [3, 0], 'Inventory': [9, [10, inventory]]}]}]}
    if placement:
        player = metadata['Data'][1]['Player'][1]
        player['Pos'] = [9, [6, [struct.unpack('>Q', struct.pack('>d', value))[0]
                                for value in (1.5, 65.62, -.5)]]]
        player['Rotation'] = [9, [5, [struct.unpack('>I', struct.pack('>f', value))[0]
                                     for value in (0., 30.)]]]
    blocks, nibbles = bytearray(32768), bytearray(16384)
    if placement:
        blocks[(2 << 11) | (2 << 7) | 63] = 3
    tiles = []
    for x in range(1 if profile == 'single' else 2):
        at = (x << 11) | (2 << 7) | 64
        blocks[at] = 54
        if profile == 'blocked' and x == 1:
            blocks[at + 1] = 1
        rows = G.authored_slots(dense, x != 0)
        if x == 0 and not dense and not initial:
            rows = [(0, 264, 5, 0)]
        elif placement:
            rows = []
        tiles.append({'id': [8, 'Chest'], 'x': [3, x], 'y': [3, 64], 'z': [3, 2],
                      'Items': [9, [10, [item(slot, item_id, count) for slot, item_id, count, _ in rows]]]})
    chunk = {'Level': [10, {'xPos': [3, 0], 'zPos': [3, 0], 'Blocks': [7, blocks],
                           'Data': [7, nibbles], 'Entities': [9, [10, []]],
                           'TileEntities': [9, [10, tiles]]}]}
    return metadata, chunk


def write_saved(directory, metadata, chunk):
    directory.mkdir(parents=True, exist_ok=True)
    wire = b'\x0a\0\0' + payload(10, metadata)
    (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
    wire = b'\x0a\0\0' + payload(10, chunk)
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    sectors = (len(record) + 4095) // 4096
    region = bytearray(8192)
    struct.pack_into('>I', region, 0, 2 * 256 + sectors)
    region += record + bytes(sectors * 4096 - len(record))
    (directory / 'region').mkdir(exist_ok=True)
    (directory / 'region/r.0.0.mcr').write_bytes(region)


def png_chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))


def png(width=854, height=480, first_filter=0):
    stride = width * 4 + 1
    pixels = bytearray(stride * height)
    pixels[0] = first_filter
    return (b'\x89PNG\r\n\x1a\n' +
            png_chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
            png_chunk(b'IDAT', zlib.compress(pixels)) + png_chunk(b'IEND', b''))


class ChestControls(unittest.TestCase):
    def test_exact_native_receipts_for_all_profiles(self):
        for profile in sorted(G.PROFILES):
            with self.subTest(profile=profile):
                result = G.native_receipts(transport(profile), profile, MONITOR)
                self.assertEqual(result['checks'], 1464 if profile == 'placement' else 7)
                self.assertEqual(result['window_ids'], [3, 4])
                self.assertEqual(len(result['monitor_witnesses']), 4)

    def test_cleanup_footer_and_continuation_corruptions(self):
        source = transport()
        variants = [source.replace('checks=7', 'checks=' + value) for value in
                    ('', '0', '-1', '1.0', '7garbage', '007', '4294967296', '99999999999')]
        variants += [source.replace('cleanup=1', 'cleanup=0'),
                     source.replace('windows-absent=1', 'windows-absent=0'),
                     source.replace('epochs=2', 'epochs=1'),
                     source.replace('source-field-repair=0', 'source-field-repair=1'),
                     source.replace('CHEST_SDK profile=double', 'CHEST_SDK profile=single'),
                     source + source.splitlines()[-1] + '\n',
                     source + 'late native output\n',
                     source.replace('CHEST_SDK', 'CHEST_SDK malformed=1'),
                     source.replace('CHEST_SDK profile=double ordinary-save-reload=1 epochs=2 source-field-repair=0\n', '')]
        for index, value in enumerate(variants):
            with self.subTest(index=index), self.assertRaises(RuntimeError):
                G.native_receipts(value, 'double', MONITOR)
        with self.assertRaises(RuntimeError):
            G.native_receipts(source, 'double', MONITOR, 'unexpected warning\n')

    def test_monitor_and_owner_corruptions(self):
        source = transport()
        variants = [source.replace('x=-1387', 'x=0', 1), source.replace('width=854', 'width=853', 1),
                    source.replace('target=-1920,0,1920,1040', 'target=0,0,1920,1040', 1),
                    source.replace('frame-id=3', 'frame-id=0', 1),
                    source.replace('canvas-id=4', 'canvas-id=3', 1),
                    source.replace('bound-app=1', 'bound-app=0', 1),
                    source.replace('borrowed-window=1', 'borrowed-window=0', 1)]
        lines = source.splitlines()
        variants.extend(('\n'.join(lines[:2] + lines[3:]) + '\n',
                         '\n'.join(lines[:2] + [lines[3], lines[2]] + lines[4:]) + '\n',
                         source.replace('stage=after-chest-reload frame-id=3',
                                        'stage=after-chest-reload frame-id=5')))
        for index, value in enumerate(variants):
            with self.subTest(index=index), self.assertRaises(RuntimeError):
                G.native_receipts(value, 'double', MONITOR)

    def test_shutdown_is_a_distinct_native_protocol(self):
        source = transport('shutdown')
        for value in (source.replace(G.OPEN_PREPARE + '\n', ''),
                      source.replace('source-close=0', 'source-close=1'),
                      source.replace('stacks-unchanged=1', 'stacks-unchanged=0'),
                      source.replace('native-prefix-only=1', 'native-prefix-only=0'),
                      source.replace('CHEST_FINISH', 'CHEST_SDK profile=shutdown\nCHEST_FINISH')):
            with self.assertRaises(RuntimeError):
                G.native_receipts(value, 'shutdown', MONITOR)
        with self.assertRaises(RuntimeError):
            G.native_receipts(source, 'single', MONITOR)

    def test_placement_receipt_requires_actual_complete_sequence(self):
        source = transport('placement')
        sdk = 'CHEST_SDK profile=placement ordinary-save-reload=1 epochs=2 source-field-repair=0'
        final_host = ('WORLD_SAVE_TITLE_HOST stage=after-chest-reload frame-id=3 canvas-id=4 canvas=1 '
                      'display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1')
        variants = [source.replace(G.PLACEMENT + '\n', ''),
                    source.replace(G.PLACEMENT, G.PLACEMENT + '\n' + G.PLACEMENT),
                    source.replace(G.PLACEMENT + '\n', '').replace(sdk, sdk + '\n' + G.PLACEMENT),
                    source.replace(G.PLACEMENT + '\n', '').replace(final_host, G.PLACEMENT + '\n' + final_host)]
        for old, new in (('placed=2', 'placed=1'), ('rejected=1', 'rejected=0'),
                         ('transferred=5', 'transferred=4'), ('held=1', 'held=0'),
                         ('sdk-strafe=1', 'sdk-strafe=0'), ('source-field-repair=0', 'source-field-repair=1')):
            variants.append(source.replace(G.PLACEMENT, G.PLACEMENT.replace(old, new)))
        for index, value in enumerate(variants):
            with self.subTest(index=index), self.assertRaises(RuntimeError):
                G.native_receipts(value, 'placement', MONITOR)
        for profile in sorted(G.PROFILES - {'placement'}):
            value = transport(profile).replace('CHEST_FINISH', G.PLACEMENT + '\nCHEST_FINISH')
            with self.subTest(profile=profile), self.assertRaises(RuntimeError):
                G.native_receipts(value, profile, MONITOR)

    def test_retained_placement_stdout_is_a_pinned_decoder_input(self):
        self.assertEqual(hashlib.sha256(PLACEMENT_TRANSPORT.encode('utf8')).hexdigest(),
                         '475c4bcfc9bdef400f90a3f9b9f5be4dd2f364f1c7cbd4bbc2a30f178f5f7ef9')
        result = G.native_receipts(PLACEMENT_TRANSPORT, 'placement', MONITOR,
                                  process_interval=(1794804243189, 1794804251448))
        self.assertEqual(result['checks'], 1464)
        for interval in ((1794804251321, 1794804251448), (1794804243189, 1794804251446),
                         (0, 1794804251448), ('1794804243189', 1794804251448), (1,)):
            with self.subTest(interval=interval), self.assertRaises(RuntimeError):
                G.native_receipts(PLACEMENT_TRANSPORT, 'placement', MONITOR, process_interval=interval)

    def test_each_placement_intermediate_record_is_required_once_in_order(self):
        lines = PLACEMENT_TRANSPORT.splitlines()
        prefixes = ('CHEST_PLACE_TAP', 'CHEST_STRAFE', 'WORLD_SAVE_TITLE_RECOVERY',
                    'WORLD_SAVE_TITLE_ESCAPE', 'WORLD_SAVE_TITLE_ENTRY', 'WORLD_SAVE_TITLE ',
                    'WORLD_RELOAD_SNAPSHOT')
        for index, line in enumerate(lines):
            if not line.startswith(prefixes):
                continue
            without = lines[:index] + lines[index + 1:]
            variants = (without, lines[:index] + [line] + lines[index:],
                        lines[:index] + [line + ' trailing=1'] + lines[index + 1:],
                        [line] + without, without[:-1] + [line] + without[-1:])
            for variant, value in enumerate(variants):
                with self.subTest(index=index, variant=variant), self.assertRaises(RuntimeError):
                    G.native_receipts('\n'.join(value) + '\n', 'placement', MONITOR)
        for prefix in prefixes:
            with self.subTest(family=prefix), self.assertRaises(RuntimeError):
                G.native_receipts('\n'.join(line for line in lines if not line.startswith(prefix)) + '\n',
                                  'placement', MONITOR)

    def test_placement_tap_targets_attempts_and_exact_mutation_deltas(self):
        lines = PLACEMENT_TRANSPORT.splitlines()
        for index, line in enumerate(lines):
            if not line.startswith('CHEST_PLACE_TAP'):
                continue
            changes = [('target=', 'target=9'), ('placed=', 'placed=9')]
            for field in ('attempts', 'clicks', 'block-use', 'admitted', 'setters', 'sounds', 'count-stores'):
                word = next(word for word in line.split() if word.startswith(field + '='))
                changes.append((word, field + '=2' if field != 'attempts' else field + '=13'))
                changes.append((word, field + '=-1'))
            changes.extend((('attempts=1', 'attempts=0'), ('attempts=1', 'attempts=01')))
            for old, new in changes:
                with self.subTest(index=index, field=old), self.assertRaises(RuntimeError):
                    value = lines[:index] + [line.replace(old, new)] + lines[index + 1:]
                    G.native_receipts('\n'.join(value) + '\n', 'placement', MONITOR)
        self.assertEqual(G.native_receipts(PLACEMENT_TRANSPORT.replace('attempts=1', 'attempts=12'),
                                          'placement', MONITOR)['checks'], 1464)

    def test_placement_strafe_groups_are_contiguous_bounded_and_finite(self):
        line = 'CHEST_STRAFE attempt=2 target=1,63,2 key=A x=2.0074382539138549'
        variants = [line.replace('attempt=2', 'attempt=' + value) for value in ('0', '1', '3', '21', '02')]
        variants += [line.replace('target=1,63,2', 'target=' + value)
                     for value in ('2,63,2', '1,64,2', '1,63,3')]
        variants += [line.replace('key=A', 'key=' + value) for value in ('B', 'a', '')]
        variants += [line.replace('x=2.0074382539138549', 'x=' + value)
                     for value in ('nan', 'inf', '-inf', '1e999', '1.0garbage', '')]
        for value in variants:
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                G.native_receipts(PLACEMENT_TRANSPORT.replace(line, value), 'placement', MONITOR)
        # The count cap is independent of contiguous numbering and a valid final X.
        group = '\n'.join(f'CHEST_STRAFE attempt={attempt} target=1,63,2 key=A x=1.5'
                          for attempt in range(1, 22))
        first = PLACEMENT_TRANSPORT.index('CHEST_STRAFE attempt=1 target=1,63,2')
        end = PLACEMENT_TRANSPORT.index('CHEST_PLACE_TAP target=1,63,2')
        with self.assertRaises(RuntimeError):
            G.native_receipts(PLACEMENT_TRANSPORT[:first] + group + '\n' + PLACEMENT_TRANSPORT[end:],
                              'placement', MONITOR)
        for target, number in (('1,63,2', '1.8054552663941426'), ('2,63,2', '2.2162462943658507'),
                               ('1,64,2', '1.8057761951018101')):
            with self.subTest(final_target=target), self.assertRaises(RuntimeError):
                G.native_receipts(PLACEMENT_TRANSPORT.replace('x=' + number, 'x=9'),
                                  'placement', MONITOR)

    def test_placement_save_fields_widths_and_timestamp_continuity(self):
        source = PLACEMENT_TRANSPORT
        changes = (('mode=save-title frame=', 'mode=save-title-session frame='),
                   ('frame=0 eligible=', 'frame=64 eligible='), ('entered=1', 'entered=0'),
                   ('copied-before-frame=1', 'copied-before-frame=0'), ('leave-pc=366', 'leave-pc=64'),
                   ('eligible=98', 'eligible=0'), ('time=75', 'time=-1'),
                   ('time=75', 'time=9223372036854775808'),
                   ('size-before=3162112', 'size-before=3563521'),
                   ('size-after=3563520', 'size-after=9223372036854775808'),
                   ('entry-frame=0', 'entry-frame=1'), ('entry-bytes=1242290', 'entry-bytes=1242291'),
                   ('first=1794804251320', 'first=0'),
                   ('first=1794804251320', 'first=1794804251448'),
                   ('last=1794804251447', 'last=9223372036854775808'),
                   ('phase=chest-saved', 'phase=wrong'), ('files=13', 'files=0'),
                   ('files=13', 'files=1025'), ('bytes=1643697', 'bytes=16777217'),
                   ('actual-file-copy=1', 'actual-file-copy=0'),
                   ('keyboard-after=30', 'keyboard-after=29'),
                   ('keyboard-before=28', 'keyboard-before=27'), ('focus-pauses=0', 'focus-pauses=5'))
        for old, new in changes:
            with self.subTest(field=old, changed=new), self.assertRaises(RuntimeError):
                G.native_receipts(source.replace(old, new), 'placement', MONITOR)
        # A well-formed field which disagrees with only its paired save record fails too.
        for field in ('eligible=98', 'time=75', 'size-before=3162112', 'size-after=3563520'):
            key, value = field.split('=')
            with self.subTest(join=key), self.assertRaises(RuntimeError):
                G.native_receipts(source.replace(field, key + '=' + str(int(value) + 1), 1),
                                  'placement', MONITOR)

    def test_placement_allows_only_bounded_real_autopause_protocol(self):
        original = 'WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=28 resumed=1\n'
        retry = (original + 'WORLD_SAVE_TITLE_AUTOPAUSE attempt=1 focus-pauses=1 old-oz-keys=2 '
                 'real-id4=1 resumed=1 frames=2\n' +
                 'WORLD_SAVE_TITLE_RECOVERY focus-pauses=1 keyboard-before-escape=30 resumed=1\n')
        # This is a synthetic optional-path decoder input, not another native observation.
        value = PLACEMENT_TRANSPORT.replace(original, retry).replace('keyboard-before=28 keyboard-after=30',
                                                                     'keyboard-before=30 keyboard-after=32')
        self.assertEqual(G.native_receipts(value, 'placement', MONITOR)['checks'], 1464)
        for old, new in (('attempt=1 focus-pauses=1', 'attempt=2 focus-pauses=1'),
                         ('focus-pauses=1 old-oz', 'focus-pauses=2 old-oz'), ('frames=2', 'frames=64'),
                         ('old-oz-keys=2', 'old-oz-keys=1'), ('keyboard-before-escape=30', 'keyboard-before-escape=29')):
            with self.subTest(field=old), self.assertRaises(RuntimeError):
                G.native_receipts(value.replace(old, new), 'placement', MONITOR)

    def test_placement_save_receipts_match_retained_file_inventories(self):
        # Small synthetic files exercise transport accounting, not saved-world semantics.
        source = PLACEMENT_TRANSPORT.replace('bytes=1242290', 'bytes=10').replace(
            'files=13 bytes=1643697', 'files=2 bytes=11')
        with tempfile.TemporaryDirectory(prefix='chest-placement-accounting-') as tmp:
            evidence = Path(tmp)
            entry = evidence / 'entries/frame-0'
            snapshot = evidence / 'snapshots/chest-saved'
            entry.mkdir(parents=True)
            snapshot.mkdir(parents=True)
            (entry / 'a').write_bytes(b'0123456789')
            (snapshot / 'a').write_bytes(b'01234')
            (snapshot / 'b').write_bytes(b'012345')
            self.assertEqual(G.native_receipts(source, 'placement', MONITOR, evidence=evidence)['checks'], 1464)
            for path in (entry / 'a', snapshot / 'a', snapshot / 'b'):
                old = path.read_bytes()
                path.write_bytes(old + b'x')
                with self.subTest(path=path), self.assertRaises(RuntimeError):
                    G.native_receipts(source, 'placement', MONITOR, evidence=evidence)
                path.write_bytes(old)
            (evidence / 'entries/frame-1').mkdir()
            with self.assertRaises(RuntimeError):
                G.native_receipts(source, 'placement', MONITOR, evidence=evidence)

    def test_independent_saved_gate_all_profiles(self):
        with tempfile.TemporaryDirectory(prefix='chest-saved-reader-') as tmp:
            directory = Path(tmp)
            for profile in sorted(G.PROFILES):
                with self.subTest(profile=profile):
                    write_saved(directory, *graph(profile))
                    result = G.saved_gate(D, directory, profile)
                    self.assertEqual(len(result['chests']), 1 if profile == 'single' else 2)
                    self.assertEqual(len(result['player']), 36 if profile == 'dense' else
                                     0 if profile in ('blocked', 'shutdown') else 1)

    def test_empty_entity_lists_admit_all_valid_child_types(self):
        with tempfile.TemporaryDirectory(prefix='chest-empty-entities-') as tmp:
            directory = Path(tmp)
            for child in range(11):
                with self.subTest(empty_child=child):
                    metadata, chunk = graph('single')
                    chunk['Level'][1]['Entities'] = [9, [child, []]]
                    write_saved(directory, metadata, chunk)
                    self.assertEqual(len(G.saved_gate(D, directory, 'single')['chests']), 1)
            values = {1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: b'', 8: '', 9: [0, []], 10: {}}
            for child, value in values.items():
                with self.subTest(nonempty_child=child):
                    metadata, chunk = graph('single')
                    chunk['Level'][1]['Entities'] = [9, [child, [value]]]
                    write_saved(directory, metadata, chunk)
                    with self.assertRaises(RuntimeError):
                        G.saved_gate(D, directory, 'single')

    def test_player_chest_and_chunk_corruptions(self):
        def player(metadata):
            return metadata['Data'][1]['Player'][1]

        def level(chunk):
            return chunk['Level'][1]

        def tile(chunk):
            return level(chunk)['TileEntities'][1][1][0]

        mutations = (
            lambda m, c: player(m)['Inventory'][1][1][0]['Count'].__setitem__(1, 4),
            lambda m, c: player(m)['Inventory'][1][1][0]['Slot'].__setitem__(1, 9),
            lambda m, c: player(m)['Inventory'][1][1].append(copy.deepcopy(player(m)['Inventory'][1][1][0])),
            lambda m, c: player(m)['Inventory'][1][1][0]['Count'].__setitem__(0, 2),
            lambda m, c: player(m)['Dimension'].__setitem__(1, -1),
            lambda m, c: player(m)['Dimension'].__setitem__(0, 2),
            lambda m, c: tile(c)['x'].__setitem__(0, 2),
            lambda m, c: tile(c)['x'].__setitem__(1, 2),
            lambda m, c: tile(c)['id'].__setitem__(1, 'Furnace'),
            lambda m, c: tile(c)['Items'][1][1][0]['id'].__setitem__(1, 265),
            lambda m, c: tile(c)['Items'][1][1][0]['Count'].__setitem__(1, 6),
            lambda m, c: tile(c)['Items'][1][1][0]['Damage'].__setitem__(1, 1),
            lambda m, c: tile(c)['Items'][1][1].append(copy.deepcopy(tile(c)['Items'][1][1][0])),
            lambda m, c: tile(c).update(unexpected=[3, 1]),
            lambda m, c: level(c)['TileEntities'][1][1].pop(),
            lambda m, c: level(c)['TileEntities'][1][1].append(copy.deepcopy(tile(c))),
            lambda m, c: level(c)['Entities'][1][1].append({'id': [8, 'Item']}),
            lambda m, c: level(c)['Blocks'][1].__setitem__((2 << 7) | 64, 1),
            lambda m, c: level(c)['Data'][1].__setitem__(((2 << 7) | 64) // 2, 1),
            lambda m, c: level(c)['xPos'].__setitem__(1, 1),
            lambda m, c: level(c)['zPos'].__setitem__(0, 2),
        )
        with tempfile.TemporaryDirectory(prefix='chest-saved-corrupt-') as tmp:
            directory = Path(tmp)
            for index, mutate in enumerate(mutations):
                with self.subTest(index=index):
                    metadata, chunk = graph('double')
                    mutate(metadata, chunk)
                    write_saved(directory, metadata, chunk)
                    with self.assertRaises((RuntimeError, AssertionError)):
                        G.saved_gate(D, directory, 'double')

    def test_placement_saved_pose_consumption_and_third_cell_corruptions(self):
        def player(metadata):
            return metadata['Data'][1]['Player'][1]

        def level(chunk):
            return chunk['Level'][1]

        def tiles(chunk):
            return level(chunk)['TileEntities'][1][1]

        third = (2 << 11) | (2 << 7) | 64
        mutations = (
            lambda m, c: player(m)['Inventory'][1][1][0]['Count'].__setitem__(1, 2),
            lambda m, c: player(m)['Inventory'][1][1][0]['Count'].__setitem__(1, 0),
            lambda m, c: player(m)['Inventory'][1][1].append(item(9, 264, 5)),
            lambda m, c: player(m).__setitem__('Pos', [9, [5, [0, 0, 0]]]),
            lambda m, c: player(m)['Pos'][1][1].pop(),
            lambda m, c: player(m)['Pos'][1][1].__setitem__(0, struct.unpack('>Q', struct.pack('>d', .5))[0]),
            lambda m, c: player(m)['Rotation'][1].__setitem__(0, 3),
            lambda m, c: player(m)['Rotation'][1][1].__setitem__(1, struct.unpack('>I', struct.pack('>f', 31.))[0]),
            lambda m, c: level(c)['Blocks'][1].__setitem__(third, 54),
            lambda m, c: level(c)['Blocks'][1].__setitem__(third - 1, 0),
            lambda m, c: level(c)['Data'][1].__setitem__(third // 2, 1),
            lambda m, c: tiles(c).append(dict(copy.deepcopy(tiles(c)[1]), x=[3, 2])),
            lambda m, c: tiles(c)[0]['Items'][1][1][0]['Count'].__setitem__(1, 4),
            lambda m, c: tiles(c)[1]['Items'][1][1].append(item(0, 264, 5)),
        )
        with tempfile.TemporaryDirectory(prefix='chest-placement-corrupt-') as tmp:
            directory = Path(tmp)
            for index, mutate in enumerate(mutations):
                with self.subTest(index=index):
                    metadata, chunk = graph('placement')
                    mutate(metadata, chunk)
                    write_saved(directory, metadata, chunk)
                    with self.assertRaises((RuntimeError, AssertionError)):
                        G.saved_gate(D, directory, 'placement')

    def test_region_allocation_and_compressed_members(self):
        with tempfile.TemporaryDirectory(prefix='chest-region-corrupt-') as tmp:
            directory = Path(tmp)
            write_saved(directory, *graph('double'))
            path = directory / 'region/r.0.0.mcr'
            original = path.read_bytes()
            mutations = []
            for value in (0, 1, 257, 512, 769, (2 << 8) | 2):
                bad = bytearray(original)
                struct.pack_into('>I', bad, 0, value)
                mutations.append(bytes(bad))
            bad = bytearray(original)
            bad[4:8] = bad[:4]
            mutations.extend((bytes(bad), original[:-1], original + b'\0'))
            for size in (0, 1, 4093, 10000000):
                bad = bytearray(original)
                struct.pack_into('>I', bad, 8192, size)
                mutations.append(bytes(bad))
            bad = bytearray(original)
            bad[8196] = 1
            mutations.append(bytes(bad))
            for index, value in enumerate(mutations):
                with self.subTest(index=index):
                    path.write_bytes(value)
                    with self.assertRaises((RuntimeError, AssertionError, zlib.error)):
                        G.saved_gate(D, directory, 'double')
            path.write_bytes(original)
            metadata = directory / 'level.dat'
            raw = metadata.read_bytes()
            for value in (raw[:-1], raw + b'trailing', b'\0' * (D.BOUND + 1)):
                metadata.write_bytes(value)
                with self.assertRaises((RuntimeError, AssertionError, zlib.error)):
                    G.saved_gate(D, directory, 'double')

    def test_empty_end_list_and_stack_widths(self):
        self.assertEqual(G.slot_rows([9, [0, []]]), [])
        for value in ([9, [0, [item(0, 264, 1)]]], [9, [11, []]], [9, [10, [item(-1, 264, 1)] * 2]],
                      [9, [10, [dict(item(0, 264, 1), Count=[True, 1])]]]):
            with self.assertRaises(RuntimeError):
                G.slot_rows(value)

    def test_empty_stack_lists_admit_all_valid_child_types(self):
        with tempfile.TemporaryDirectory(prefix='chest-empty-inventory-') as tmp:
            directory = Path(tmp)
            for child in range(11):
                with self.subTest(empty_child=child):
                    self.assertEqual(G.slot_rows([9, [child, []]]), [])
                    metadata, chunk = graph('blocked')
                    metadata['Data'][1]['Player'][1]['Inventory'] = [9, [child, []]]
                    write_saved(directory, metadata, chunk)
                    self.assertEqual(G.saved_gate(D, directory, 'blocked')['player'], [])
            values = {1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: b'', 8: '', 9: [0, []]}
            for child, value in values.items():
                with self.subTest(nonempty_child=child):
                    with self.assertRaises(RuntimeError):
                        G.slot_rows([9, [child, [value]]])
                    metadata, chunk = graph('blocked')
                    metadata['Data'][1]['Player'][1]['Inventory'] = [9, [child, [value]]]
                    write_saved(directory, metadata, chunk)
                    with self.assertRaises(RuntimeError):
                        G.saved_gate(D, directory, 'blocked')
            self.assertEqual(G.slot_rows([9, [10, [item(0, 264, 1)]]]), [(0, 264, 1, 0)])

    def test_blocked_and_shutdown_keep_initial_chest_contents(self):
        with tempfile.TemporaryDirectory(prefix='chest-initial-controls-') as tmp:
            root = Path(tmp)
            for profile in ('blocked', 'shutdown'):
                metadata, chunk = graph(profile)
                # A transfer result is wrong for both profiles even if it is valid elsewhere.
                chunk['Level'][1]['TileEntities'][1][1][0]['Items'][1][1].pop()
                write_saved(root, metadata, chunk)
                with self.assertRaises(RuntimeError):
                    G.saved_gate(D, root, profile)
            metadata, chunk = graph('blocked')
            chunk['Level'][1]['Blocks'][1][(1 << 11) | (2 << 7) | 65] = 0
            write_saved(root, metadata, chunk)
            with self.assertRaises(RuntimeError):
                G.saved_gate(D, root, 'blocked')

    def test_shutdown_snapshot_bytes_and_initial_nbt(self):
        with tempfile.TemporaryDirectory(prefix='chest-shutdown-reader-') as tmp:
            evidence = Path(tmp)
            stopped = evidence / 'snapshots/chest-open-stopped'
            prepared = evidence / 'snapshots/chest-open-prepared'
            write_saved(stopped / 'alpha', *graph('shutdown'))
            shutil.copytree(stopped, prepared)
            self.assertTrue(G.shutdown_gate(D, evidence)['source_bytes_unchanged'])
            metadata = prepared / 'alpha/level.dat'
            metadata.write_bytes(metadata.read_bytes() + b'\0')
            with self.assertRaises(RuntimeError):
                G.shutdown_gate(D, evidence)
            # Matching damaged snapshots still fail independent semantic readback.
            wrong, chunk = graph('shutdown')
            wrong['Data'][1]['Player'][1]['Inventory'][1][1].append(item(0, 264, 1))
            write_saved(stopped / 'alpha', wrong, chunk)
            write_saved(prepared / 'alpha', wrong, chunk)
            with self.assertRaises(RuntimeError):
                G.shutdown_gate(D, evidence)

    def test_fresh_complete_png_inventory(self):
        with tempfile.TemporaryDirectory(prefix='chest-image-controls-') as tmp:
            evidence = Path(tmp)
            for name in ('chest-open.png', 'chest-title.png', 'chest-reloaded.png'):
                path = evidence / name
                path.write_bytes(png())
                os.utime(path, ns=(20000000, 20000000))
            self.assertEqual(len(G.image_gate(evidence, 'double', 10)), 3)
            path = evidence / 'chest-open.png'
            bad_crc = bytearray(png())
            bad_crc[-1] ^= 1
            for value in (b'not PNG', png()[:-1], png() + b'trailing', png(853), png(first_filter=5), bytes(bad_crc)):
                path.write_bytes(value)
                with self.assertRaises(RuntimeError):
                    G.image_gate(evidence, 'double', 10)
            path.write_bytes(png())
            os.utime(path, ns=(1000000, 1000000))
            with self.assertRaises(RuntimeError):
                G.image_gate(evidence, 'double', 10)
            os.utime(path, ns=(20000000, 20000000))
            self.assertEqual(len(G.image_gate(evidence, 'double', 10)), 3)
            (evidence / 'chest-stale.png').write_bytes(png())
            with self.assertRaises(RuntimeError):
                G.image_gate(evidence, 'double', 10)

    def test_profile_specific_image_inventory(self):
        with tempfile.TemporaryDirectory(prefix='chest-profile-images-') as tmp:
            root = Path(tmp)
            for profile, names in (('blocked', ('chest-title.png',)),
                                   ('shutdown', ('chest-open.png',)),
                                   ('dense', ('chest-open.png', 'chest-title.png',
                                              'chest-reloaded.png', 'chest-dense-cursor.png')),
                                   ('placement', ('chest-open.png', 'chest-title.png', 'chest-reloaded.png',
                                                  'chest-placed-single.png', 'chest-placement-double.png',
                                                  'chest-third-rejected.png'))):
                evidence = root / profile
                evidence.mkdir()
                for name in names:
                    path = evidence / name
                    path.write_bytes(png())
                    os.utime(path, ns=(20000000, 20000000))
                self.assertEqual(set(G.image_gate(evidence, profile, 10)), set(names))

    def test_input_inventory_additions_deletions_and_changed_dll(self):
        with tempfile.TemporaryDirectory(prefix='chest-source-controls-') as tmp:
            root = Path(tmp)
            for name in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client', 'tests/tools/world'):
                (root / name).mkdir(parents=True, exist_ok=True)
            for name in ('CMakeLists.txt', 'tests/tools/world/test_source_saving.py', 'client.exe', 'runtime.dll'):
                (root / name).write_bytes(b'controlled input')
            executable = root / 'client.exe'
            with patch.object(G, 'ROOT', root):
                before = G.fingerprints(G.source_inputs(executable))
                (root / 'src/new.c').write_bytes(b'new current source')
                self.assertNotEqual(before, G.fingerprints(G.source_inputs(executable)))
                (root / 'src/new.c').unlink()
                self.assertEqual(before, G.fingerprints(G.source_inputs(executable)))
                (root / 'runtime.dll').write_bytes(b'changed native product')
                self.assertNotEqual(before, G.fingerprints(G.source_inputs(executable)))
                (root / 'runtime.dll').unlink()
                self.assertNotEqual(before, G.fingerprints(G.source_inputs(executable)))


if __name__ == '__main__':
    unittest.main()
