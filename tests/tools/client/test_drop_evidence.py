"""Reject save loss, invented Items, duplicate Items and changed unloaded state."""
from copy import deepcopy
import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


mining = module('drop_evidence_mining', HERE / 'test_mining_loop.py')
drop = module('drop_evidence', HERE / 'test_drop_loop.py')


class SavedItems(unittest.TestCase):
    def setUp(self):
        self.live = {'Item': [10, {'id': [2, 3], 'Count': [1, 1], 'Damage': [2, 0]}],
                     'Age': [2, 18], 'Motion': [9, [6, [0, 0, 1]]],
                     'Pos': [9, [6, [4602678819172646912, 4634204016564240384, 0]]]}
        self.loaded = {'chunk': [0, 0], 'entity': dict(self.live, id=[8, 'Item'])}
        self.unloaded = {'chunk': [8, -10], 'entity': {
            'id': [8, 'Item'], 'Item': [10, {'id': [2, 295], 'Count': [1, 1], 'Damage': [2, 0]}],
            'Age': [2, 0], 'Motion': [9, [6, [0, 0, 0]]]}}
        self.saved = deepcopy([self.loaded, self.unloaded])

    def compare(self):
        return drop.compare_saved_items(mining, self.saved, [self.live], {(0, 0)}, [self.unloaded])

    def test_loaded_and_unloaded_bodies(self):
        self.assertEqual(self.compare(), ([self.loaded], [self.unloaded]))

    def test_lost_live_item(self):
        self.saved.pop(0)
        with self.assertRaisesRegex(RuntimeError, 'Every loaded saved Item'):
            self.compare()

    def test_duplicate_live_item(self):
        self.saved.append(deepcopy(self.loaded))
        with self.assertRaisesRegex(RuntimeError, 'Every loaded saved Item'):
            self.compare()

    def test_changed_live_float_bit(self):
        self.saved[0]['entity']['Motion'][1][1][2] ^= 1
        with self.assertRaisesRegex(RuntimeError, 'Every loaded saved Item'):
            self.compare()

    def test_lost_unloaded_item(self):
        self.saved.pop()
        with self.assertRaisesRegex(RuntimeError, 'Every unloaded saved Item'):
            self.compare()

    def test_changed_unloaded_float_bit(self):
        self.saved[1]['entity']['Motion'][1][1][0] ^= 1
        with self.assertRaisesRegex(RuntimeError, 'Every unloaded saved Item'):
            self.compare()

    def test_relocated_unloaded_item(self):
        self.saved[1]['chunk'][0] += 1
        with self.assertRaisesRegex(RuntimeError, 'Every unloaded saved Item'):
            self.compare()

    def test_invented_unloaded_item(self):
        self.saved.append(deepcopy(self.unloaded))
        with self.assertRaisesRegex(RuntimeError, 'Every unloaded saved Item'):
            self.compare()


if __name__ == '__main__':
    unittest.main()
