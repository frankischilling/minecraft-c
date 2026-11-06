"""Raw Inventory save receipts, independent NBT and pickup timing controls."""
import gzip
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('attack_inventory_wrapper', HERE / 'test_attack_loop.py')
attack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(attack)
decoder = attack.module('attack_inventory_decoder', attack.ROOT / 'tests/tools/world/test_source_saving.py')


def tag(kind, name, payload):
    name = name.encode('ascii')
    return bytes([kind]) + struct.pack('>H', len(name)) + name + payload


def inventory(slots, override=None, empty_child=1):
    """Encode the fixture independently of the receipt-to-NBT comparator."""
    body = bytes([10 if slots else empty_child]) + struct.pack('>i', len(slots))
    for slot, item, count, damage in slots:
        for kind, name, value in ((1, 'Slot', slot), (2, 'id', item),
                                  (1, 'Count', count), (2, 'Damage', damage)):
            if override is not None and name == override[0]:
                kind = override[1]
            body += tag(kind, name, struct.pack('>' + {1: 'b', 2: 'h', 3: 'i'}[kind], value))
        body += b'\0'
    return tag(9, 'Inventory', body)


def metadata(slots, **options):
    player = tag(10, 'Player', inventory(slots, **options) + b'\0')
    return gzip.compress(tag(10, '', tag(10, 'Data', player + b'\0') + b'\0'), mtime=0)


class InventoryControls(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.folder = Path(self.temporary.name)
        self.cases = 0
        self.profile = attack.PROFILES['iron-sword-break']
        self.wool = {'inventory_count': 1}
        self.fixture([[[0, 267, 1, 250]], [], [[0, 35, 1, 0]]])

    def tearDown(self):
        self.temporary.cleanup()

    def fixture(self, rows, post_death=1):
        self.root = self.folder / f'case-{self.cases}'
        self.cases += 1
        self.rows = rows
        self.events = len(rows)
        lines = []
        for event, slots in enumerate(rows):
            raw = ','.join(':'.join(map(str, slot)) for slot in slots) or '-'
            lines.append(f'ATTACK_INVENTORY_SAVE event={event} slots={raw}')
            folder = self.root / 'autosaves' / f'save-{event}'
            (folder / 'after/alpha').mkdir(parents=True)
            self.player(event, slots)
            self.saved(event, slots)
        lines.append(f'ATTACK_AUTOSAVE_POST_DEATH event={post_death} world-remove=1 player-score=14')
        self.stdout = '\n'.join(lines) + '\n'
        (self.root / 'after-verified/alpha').mkdir(parents=True)
        self.final(rows[-1])

    def player(self, event, slots, **options):
        path = self.root / 'autosaves' / f'save-{event}/player.nbt'
        path.write_bytes(tag(10, '', inventory(slots, **options) + b'\0'))

    def saved(self, event, slots, **options):
        path = self.root / 'autosaves' / f'save-{event}/after/alpha/level.dat'
        path.write_bytes(metadata(slots, **options))

    def final(self, slots, **options):
        (self.root / 'after-verified/alpha/level.dat').write_bytes(metadata(slots, **options))

    def audit(self):
        return attack.inventory_save_audit(self.root, self.stdout, decoder,
                                            self.profile, self.wool, self.events)

    def fails(self):
        with self.assertRaises(RuntimeError):
            self.audit()

    def test_each_save_matches_raw_input(self):
        result = self.audit()
        self.assertEqual([row['slots'] for row in result], self.rows)
        self.assertEqual(result[1]['inventory'], [9, [1, []]])
        self.assertEqual(result[2]['inventory'], [9, [10, [{
            'Slot': [1, 0], 'id': [2, 35], 'Count': [1, 1], 'Damage': [2, 0]}]]])

    def test_later_pickup_can_follow_last_save(self):
        self.fixture([[[0, 267, 1, 250]], []])
        self.assertEqual(self.wool['inventory_count'], 1)
        self.assertEqual(self.audit()[-1]['inventory'], [9, [1, []]])

    def test_later_pickup_is_not_persisted_early(self):
        self.fixture([[[0, 267, 1, 250]], []])
        self.final([[0, 35, 1, 0]])
        self.fails()

    def test_shared_wrong_player_and_metadata(self):
        self.player(1, [[0, 35, 1, 0]])
        self.saved(1, [[0, 35, 1, 0]])
        self.fails()

    def test_observer_player_inventory_mismatch(self):
        self.player(2, [])
        self.fails()

    def test_each_saved_metadata_inventory_mismatch(self):
        self.saved(1, [[0, 35, 1, 0]])
        self.fails()

    def test_inventory_field_types(self):
        for field, kind in (('Slot', 2), ('id', 3), ('Count', 2), ('Damage', 3)):
            with self.subTest(field=field):
                self.player(2, self.rows[2], override=(field, kind))
                self.saved(2, self.rows[2], override=(field, kind))
                self.fails()
        self.player(2, self.rows[2])
        self.saved(2, self.rows[2])
        self.player(1, [], empty_child=10)
        self.fails()

    def test_inventory_list_order_and_extra_slots(self):
        self.profile = attack.PROFILES['held-dirt']
        rows = [[0, 3, 7, 0], [1, 35, 1, 0]]
        self.fixture([[[0, 3, 7, 0]], rows])
        for corrupt in (rows[::-1], rows + [[100, 35, 1, 0]], rows + [rows[1]]):
            with self.subTest(slots=corrupt):
                self.player(1, corrupt)
                self.saved(1, corrupt)
                self.final(corrupt)
                self.fails()

    def test_receipt_events_exact(self):
        line = 'ATTACK_INVENTORY_SAVE event=0 slots=-'
        for text, count in ((line, 0), (line, 33), (line.replace('event=0', 'event=1'), 1),
                            (line + '\n' + line, 2),
                            (line + '\nATTACK_INVENTORY_SAVE malformed', 1)):
            with self.subTest(text=text, count=count), self.assertRaises(RuntimeError):
                attack.inventory_save_rows(text, count)
        self.stdout = self.stdout.replace('event=1 slots=-', 'event=01 slots=-')
        self.fails()

    def test_receipt_slots_order_unique(self):
        for raw in ('0:3:7:0,0:35:1:0', '1:35:1:0,0:3:7:0', '100:35:1:0,35:3:7:0'):
            with self.subTest(raw=raw), self.assertRaises(RuntimeError):
                attack.inventory_save_rows('ATTACK_INVENTORY_SAVE event=0 slots=' + raw, 1)

    def test_receipt_domains_and_counts(self):
        slots = list(range(36)) + list(range(100, 104))
        raw = ','.join(f'{slot}:35:1:0' for slot in slots)
        parsed = attack.inventory_save_rows('ATTACK_INVENTORY_SAVE event=0 slots=' + raw, 1)
        self.assertEqual([row[0] for row in parsed[0]], slots)
        for bad in ('36:35:1:0', '99:35:1:0', '104:35:1:0', '-1:35:1:0',
                    raw + ',104:35:1:0', '-,0:3:7:0'):
            with self.subTest(raw=bad), self.assertRaises(RuntimeError):
                attack.inventory_save_rows('ATTACK_INVENTORY_SAVE event=0 slots=' + bad, 1)

    def test_canonical_int32_and_line_bounds(self):
        for raw in ('00:3:7:0', '0:03:7:0', '0:3:-0:0', '0:3:+1:0',
                    '0:3:2147483648:0', '0:-2147483649:1:0', '0:3:1:' + '1' * 1800):
            with self.subTest(raw=raw), self.assertRaises(RuntimeError):
                attack.inventory_save_rows('ATTACK_INVENTORY_SAVE event=0 slots=' + raw, 1)

    def test_only_declared_profile_or_observed_wool(self):
        for name, profile in attack.PROFILES.items():
            base = [] if profile['id'] == 0 or profile['breaks'] else [[0, 3, 7, 0]]
            slot = 1 if profile['id'] == 3 else 0
            attack.inventory_save_policy(base, profile, None, True)
            attack.inventory_save_policy(base + [[slot, 35, 1, 0]], profile, self.wool, True)
            for corrupt in (base + [[slot, 35, 2, 0]], base + [[slot, 35, 1, 1]],
                            base + [[100, 35, 1, 0]], base + [[slot, 36, 1, 0]]):
                with self.subTest(profile=name, slots=corrupt), self.assertRaises(RuntimeError):
                    attack.inventory_save_policy(corrupt, profile, self.wool, True)
            with self.subTest(profile=name, observed=False), self.assertRaises(RuntimeError):
                attack.inventory_save_policy(base + [[slot, 35, 1, 0]], profile, None, True)

    def test_after_death_save_cannot_retain_sword(self):
        self.fixture([[[0, 267, 1, 250]], [[0, 267, 1, 250]]])
        self.fails()

    def test_raw_narrowing_matches_source_fields(self):
        raw = 'ATTACK_INVENTORY_SAVE event=0 slots=103:2147483647:-2147483648:2147483647'
        slots = attack.inventory_save_rows(raw, 1)[0]
        self.assertEqual(slots, [[103, 2147483647, -2147483648, 2147483647]])
        self.assertEqual(attack.inventory_save_value(slots), [9, [10, [{
            'Slot': [1, 103], 'id': [2, -1], 'Count': [1, 0], 'Damage': [2, -1]}]]])

    def test_all_profile_hand_and_wool_receipts(self):
        for name, profile in attack.PROFILES.items():
            for picked in (0, 1):
                with self.subTest(profile=name, picked=picked):
                    slot = (1 if profile['id'] == 3 else 0) if picked else -1
                    wool = (f'ATTACK_WOOL published=1 id=35 count=1 damage=0 pop-sounds={picked} '
                            f'inventory-slot={slot} inventory-count={picked} source-count={1-picked} '
                            f'removed={picked}')
                    lines = [wool]
                    for stage in ('before', 'after'):
                        pickup = picked if stage == 'after' else 0
                        initial_empty = profile['id'] == 0 or (stage == 'after' and profile['breaks'])
                        item = (35 if pickup else 0) if initial_empty else profile['id']
                        count = 0 if item == 0 else 1 if item == 35 else profile['count']
                        damage = profile['damage'] if item == 267 else 0
                        flat, block = int(item == 267), int(item in (3, 35))
                        lines.append(f'ATTACK_HAND stage={stage} profile={name} frames=3 '
                            f'empty={int(item == 0)} item={item} count={count} damage={damage} '
                            f'block={block} flat={flat} icon={66 if flat else 0} geometry={flat} '
                            f'faces={66 if flat else 6 if block else 0} '
                            f'vertices={264 if flat else 24 if block else 0} source=1377 wool-slots={pickup}')
                    if profile['id']:
                        broken = int(profile['breaks'])
                        lines.append(f'ATTACK_ITEM profile={name} id={profile["id"]} used={broken} '
                            f'broken={broken} broken-entry={broken} selected-clear={broken} '
                            f'final-count={0 if broken else profile["count"]} final-damage=0 '
                            f'clear-receipt={broken}')
                    stdout = '\n'.join(lines)
                    observed = attack.wool_receipt(stdout, profile)
                    attack.held_receipts(stdout, name, profile, observed)
                    with self.assertRaises(RuntimeError):
                        attack.wool_receipt(stdout.replace('source-count=' + str(1-picked),
                                                          'source-count=' + str(picked)), profile)


if __name__ == '__main__':
    unittest.main()
