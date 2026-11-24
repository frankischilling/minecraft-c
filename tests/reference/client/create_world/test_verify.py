"""Unchanged and corrupted-input controls for static Create World source pins."""
import argparse
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('create_world_static', HERE / 'verify_source.py')
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
JAR = VERIFY.ROOT / 'b1.7.3.jar'


class CreateWorldIdentities(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = VERIFY.load_pins(HERE / 'source-pins.json')

    def check_metadata(self, metadata):
        return VERIFY.verify(JAR, metadata)

    def test_original_archive_inventory_and_inheritance(self):
        result = self.check_metadata(self.original)
        self.assertEqual(result['status'], 'PASS_STATIC_IDENTITIES')
        self.assertEqual(result['classes'], 14)
        self.assertEqual(result['selected_methods'], 80)
        self.assertEqual(result['selected_references'], 185)
        self.assertIs(result['game_class_execution'], False)
        declarations = self.original['resolved_declarations']
        self.assertEqual(declarations['fj.c()Z'], 'da')
        self.assertEqual(declarations['fj.a' + VERIFY.TEXT], 'ub')
        self.assertEqual(declarations['ro.b' + VERIFY.TEXT], 'ub')
        self.assertEqual(declarations['hi.b(Ljava/lang/String;)Lei;'], 'tq')

    def test_changed_action_code_identity_rejected(self):
        changed = copy.deepcopy(self.original)
        method = next(row for row in changed['classes']['fj']['selected_methods']
                      if (row['name'], row['descriptor']) == ('a', '(Lke;)V'))
        method['code_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, r'fj.selected_methods.*code_sha256'):
            self.check_metadata(changed)

    def test_omitted_real_key_override_rejected(self):
        changed = copy.deepcopy(self.original)
        changed['classes']['fj']['methods'] = [
            row for row in changed['classes']['fj']['methods']
            if (row['name'], row['descriptor']) != ('a', '(CI)V')]
        with self.assertRaisesRegex(ValueError, r'length.*fj.methods'):
            self.check_metadata(changed)

    def test_false_inherited_declarations_rejected(self):
        for key, false_owner in [('hi.b(Ljava/lang/String;)Lei;', 'hi'),
                                 ('ro.b' + VERIFY.TEXT, 'ro'), ('fj.c()Z', 'fj')]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.original)
                changed['resolved_declarations'][key] = false_owner
                with self.assertRaisesRegex(ValueError, 'resolved_declarations'):
                    self.check_metadata(changed)

    def test_clipboard_reference_owner_rejected(self):
        changed = copy.deepcopy(self.original)
        changed['selected_references']['ro.a(CI)V:34']['reference']['class'] = 'fj'
        with self.assertRaisesRegex(ValueError, r'selected_references.ro.a\(CI\)V:34.reference.class'):
            self.check_metadata(changed)

    def test_save_format_allocation_reference_rejected(self):
        changed = copy.deepcopy(self.original)
        changed['selected_references'][VERIFY.APP + '.a()V:167']['reference']['name'] = 'tq'
        with self.assertRaisesRegex(ValueError, 'selected_references.*167.reference.name'):
            self.check_metadata(changed)

    def test_type_coercion_rejected(self):
        changed = copy.deepcopy(self.original)
        changed['schema'] = True
        with self.assertRaisesRegex(ValueError, 'type differs.*schema'):
            self.check_metadata(changed)

    def test_equal_size_changed_archive_rejected(self):
        raw = bytearray(JAR.read_bytes())
        raw[-1] ^= 1
        with tempfile.TemporaryDirectory(prefix='create-world-static-') as temporary:
            path = Path(temporary) / 'changed.jar'
            path.write_bytes(raw)
            with self.assertRaisesRegex(ValueError, 'Exact original client archive'):
                VERIFY.verify(path, self.original)

    def test_duplicate_metadata_key_rejected(self):
        with tempfile.TemporaryDirectory(prefix='create-world-static-') as temporary:
            path = Path(temporary) / 'duplicate.json'
            path.write_text('{"schema":1,"schema":1}', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Duplicate metadata key: schema'):
                VERIFY.load_pins(path)

    def test_repeated_mutations_leave_unchanged_baseline(self):
        baseline = json.dumps(self.original, sort_keys=True)
        for false_owner in ('ro', 'fj', 'ub', 'da'):
            changed = copy.deepcopy(self.original)
            changed['resolved_declarations']['hi.b(Ljava/lang/String;)Lei;'] = false_owner
            with self.assertRaisesRegex(ValueError, 'resolved_declarations'):
                self.check_metadata(changed)
        self.assertEqual(json.dumps(self.original, sort_keys=True), baseline)
        self.assertEqual(self.check_metadata(self.original)['status'], 'PASS_STATIC_IDENTITIES')

    def test_missing_archive_is_unavailable(self):
        with tempfile.TemporaryDirectory(prefix='create-world-static-') as temporary:
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = VERIFY.main(['--jar', str(Path(temporary) / 'missing.jar')])
            self.assertEqual(status, 77)
            self.assertIn('UNAVAILABLE', output.getvalue())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=JAR)
    args, remaining = parser.parse_known_args()
    JAR = args.jar
    if not JAR.is_file():
        print('UNAVAILABLE: original client archive is missing')
        sys.exit(77)
    unittest.main(argv=[sys.argv[0], *remaining])
