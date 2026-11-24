"""Audit the supplemental original GL_COMPILE cohort and explicit CPU-only projection."""
import importlib.util
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('inventory_modes_audit', HERE / 'audit.py')
A = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(A)


def read(name):
    return json.loads((HERE / name).read_text(), object_pairs_hook=A.P.unique)


class ModeTests(unittest.TestCase):
    def test_compile_source_and_own_prior_backing_audit(self):
        observations = read('compile-observations.json')
        result = A.audit({'pins': read('source-pins.json')}, observations['ordinary_stdout'],
                         observations['debugger_stdout'], observations['events'])
        self.assertEqual(result['rows'], read('compile-finite.json')['rows'])
        self.assertEqual(result['inactive_word_checks'], 2772)

    def test_exact_cpu_equivalence_and_explicit_gl_exclusion(self):
        immediate, compiled = read('finite.json'), read('compile-finite.json')
        self.assertEqual(immediate['mode'], 'immediate')
        self.assertEqual(compiled['mode'], 'compile')
        self.assertEqual(immediate['rows'], compiled['rows'])
        self.assertEqual(immediate['inputs'], compiled['inputs'])
        changed = []
        for left, right in zip(immediate['returns'], compiled['returns']):
            self.assertEqual({k: v for k, v in left.items() if k != 'gl_after'},
                             {k: v for k, v in right.items() if k != 'gl_after'})
            if left['gl_after'] != right['gl_after']:
                changed.append(left['profile'])
        self.assertEqual(changed, [0, 2, 4, 5, 7, 9, 11, 14, 15, 16])

    def test_passive_native_projection_has_no_invented_source_fields(self):
        original = read('finite.json')['rows']
        passive = [{'profile': r['profile'], 'id': r['id'], 'batch': r['batch'], 'words': r['words'],
                    'vertices': 6, 'mode': 4, 'arrays': 15 if r['nw_state']['l'] else 11}
                   for r in original]
        result = A.P.compare_native_geometry(passive, original)
        self.assertEqual(result['active_words'], 7596)
        passive[0]['nw_state'] = original[0]['nw_state']
        with self.assertRaises(ValueError):
            A.P.compare_native_geometry(passive, original)


if __name__ == '__main__':
    unittest.main()
