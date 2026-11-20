"""Changed-copy controls for current input admission, without native execution."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
from unittest.mock import patch


def check_current_inputs(case, subject, profile):
    binding = subject.module(profile + '_binding_controls', subject.HERE / 'scene_inputs.py')
    scene = subject.SCENE.relative_to(subject.ROOT).as_posix()
    declaration = json.loads((subject.ROOT / binding.DECLARATION).read_text())
    names = [binding.DECLARATION, scene,
             *declaration['profiles'][profile]['source_input_sha256']]
    with tempfile.TemporaryDirectory(prefix=profile + '-current-inputs-') as temporary:
        root = Path(temporary)
        originals = {name: (subject.ROOT / name).read_bytes() for name in names}
        for name, raw in originals.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)

        def read():
            return binding.read_current_plan(root, profile, scene, subject.PLAN_SHA)

        def reject():
            with case.assertRaises(ValueError):
                read()

        plan = read()
        case.assertEqual(plan, json.loads(originals[scene]))
        case.assertEqual(plan['source_input_sha256'],
                         declaration['profiles'][profile]['predecessor']['source_input_sha256'])
        case.assertEqual(binding.receipt(root, profile, scene, subject.PLAN_SHA)
                         ['historical_native_results'], 'NOT_REQUALIFIED')
        for name in names:
            with case.subTest(changed_input=name):
                path = root / name
                path.write_bytes(originals[name] + b'\n')
                reject()
                path.write_bytes(originals[name])
            with case.subTest(missing_input=name):
                path.unlink()
                reject()
                path.write_bytes(originals[name])

        current = declaration['profiles'][profile]['source_input_sha256']
        first = next(iter(current))

        def entry(value):
            return value['profiles'][profile]

        mutations = (
            lambda value: value.update(format=True),
            lambda value: value.update(scope='HISTORICAL_NATIVE_QUALIFICATION'),
            lambda value: value.update(extra=1),
            lambda value: value['profiles'].pop('lever' if profile == 'redstone_torch' else 'redstone_torch'),
            lambda value: entry(value).update(extra=1),
            lambda value: entry(value)['predecessor'].update(scene='tests/client/gameplay/other_scene.json'),
            lambda value: entry(value)['predecessor'].update(sha256='0' * 64),
            lambda value: entry(value)['predecessor']['source_input_sha256'].__setitem__(first, '0' * 64),
            lambda value: entry(value)['source_input_sha256'].pop(first),
            lambda value: entry(value)['source_input_sha256'].__setitem__('unexpected.c', '0' * 64),
            lambda value: entry(value)['source_input_sha256'].__setitem__(first, '0' * 64),
            lambda value: entry(value)['source_input_sha256'].__setitem__(first, True),
        )
        for index, change in enumerate(mutations):
            with case.subTest(changed_declaration=index):
                value = copy.deepcopy(declaration)
                change(value)
                raw = (json.dumps(value) + '\n').encode()
                (root / binding.DECLARATION).write_bytes(raw)
                reject()
                # Exercise the structural/lineage guard independently of the outer byte pin.
                with patch.object(binding, 'DECLARATION_SHA', hashlib.sha256(raw).hexdigest()):
                    reject()
        for raw in (originals[binding.DECLARATION].replace(b'"format": 1', b'"format": 1, "format": 1'),
                    originals[binding.DECLARATION].replace(b'"format": 1', b'"format": NaN')):
            (root / binding.DECLARATION).write_bytes(raw)
            with patch.object(binding, 'DECLARATION_SHA', hashlib.sha256(raw).hexdigest()):
                reject()
        (root / binding.DECLARATION).write_bytes(originals[binding.DECLARATION])
        with case.assertRaises(ValueError):
            binding.read_current_plan(root, 'unknown', scene, subject.PLAN_SHA)
        with case.assertRaises(ValueError):
            binding.read_current_plan(root, 'lever' if profile == 'redstone_torch' else 'redstone_torch',
                                      scene, subject.PLAN_SHA)
