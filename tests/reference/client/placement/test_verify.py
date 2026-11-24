"""Normal and refusal controls for the static original placement identity verifier."""
import copy
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('placement_static_verifier', BASE / 'verify.py')
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class PlacementIdentities(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = json.loads((BASE / 'source-pins.json').read_text(encoding='utf-8'))
        cls.jar = VERIFY.ROOT / 'b1.7.3.jar'
        cls.registry = VERIFY.ROOT / 'src/block/registry/registration.c'

    def check_metadata(self, metadata, registry=None):
        return VERIFY.verify(self.jar, metadata, registry or self.registry)

    def test_original_archive_and_complete_inventory(self):
        result = self.check_metadata(self.original)
        self.assertEqual(result['status'], 'PASS_STATIC_IDENTITIES')
        self.assertEqual(result['classes'], 124)
        self.assertEqual(result['block_kinds'], 69)
        self.assertFalse(result['game_class_execution'])

    def test_changed_method_identity_rejected(self):
        changed = copy.deepcopy(self.original)
        method = next(row for row in changed['classes']['ck']['methods']
                      if row['descriptor'] == '(Liz;Lgs;Lfd;IIII)Z')
        method['code_sha256'] = '0' * 64
        with self.assertRaisesRegex(AssertionError, 'complete declared-method identities'):
            self.check_metadata(changed)

    def test_omitted_real_override_rejected(self):
        changed = copy.deepcopy(self.original)
        changed['declaring_sets']['can_place']['declared'] = [
            row for row in changed['declaring_sets']['can_place']['declared'] if row['class'] != 'wb']
        with self.assertRaisesRegex(AssertionError, 'complete declaring set'):
            self.check_metadata(changed)

    def test_false_inherited_base_body_rejected(self):
        changed = copy.deepcopy(self.original)
        changed['resolved_block_methods']['can_place']['MC_BLOCK_SAPLING'] = 'uu'
        with self.assertRaisesRegex(AssertionError, 'actual inherited dispatch'):
            self.check_metadata(changed)

    def test_actual_registry_allocation_site_mutation_rejected(self):
        text = self.registry.read_text(encoding='utf-8')
        site = re.search(r'NEW\((\d+), MC_BLOCK_DIRT\)', text)
        self.assertIsNotNone(site)
        old = site.group(1)
        new = str(int(old) + 1)
        self.assertEqual(len(old), len(new))
        mutated = text[:site.start(1)] + new + text[site.end(1):]
        self.assertEqual(len(mutated), len(text))
        with tempfile.TemporaryDirectory(prefix='placement-static-identity-') as temporary:
            path = Path(temporary) / 'registration.c'
            path.write_text(mutated, encoding='utf-8')
            with self.assertRaisesRegex(AssertionError, 'original allocation opcode'):
                self.check_metadata(self.original, path)


if __name__ == '__main__':
    if not (VERIFY.ROOT / 'b1.7.3.jar').is_file():
        print(json.dumps({'status': 'UNAVAILABLE', 'reason': 'Original client archive is missing'}))
        sys.exit(77)
    unittest.main()
