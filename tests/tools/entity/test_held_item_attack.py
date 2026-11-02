"""Run genuine Player held-Item attack owners with authored resources."""
import argparse
import importlib.util
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def case_file(declared, path):
    lines = []
    for group, values in enumerate((declared['cases'], declared['cow_cases'])):
        for case in values:
            if (not re.fullmatch('[a-z0-9-]+', case['profile']) or
                    not re.fullmatch('[a-z-]+', case['mode']) or
                    case['target-kind'] not in ('Cow', 'Sheep', 'Pig')):
                raise ValueError('Declared bounded native case labels')
            numbers = [case[key] for key in ('id', 'count', 'damage', 'target-health',
                        'target-hurt', 'target-last-damage', 'target-reward')]
            if any(type(value) is not int or not -(1 << 31) <= value < (1 << 31) for value in numbers):
                raise ValueError('Declared signed32 native case operands')
            if any(type(case[key]) is not bool for key in ('falling', 'pig-saddled')):
                raise ValueError('Declared native Boolean operands')
            fields = [case['profile'], case['mode'], *numbers[:3], case['target-kind'],
                      *numbers[3:], int(case['falling']), int(case['pig-saddled']), group]
            lines.append('\t'.join(map(str, fields)))
    if len(lines) != 425:
        raise ValueError('Complete422 ordinary and3 Cow death input inventory')
    path.write_bytes(('\n'.join(lines) + '\n').encode('ascii'))


def snapshot(paths):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='held-attack-', dir=args.work_dir.resolve()))
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('held_attack_resources', support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    root = Path(__file__).resolve().parents[3]
    reference = root / 'tests/reference/entity/held_item_attack'
    comparison = module(reference / 'compare.py', 'native_held_comparison')
    cow = module(reference / 'cow.py', 'native_held_cow_audit')
    declared = json.loads((reference / 'cases.json').read_text(), object_pairs_hook=comparison.unique)
    inputs = output / 'cases.tsv'
    case_file(declared, inputs)
    selected_inputs = [executable, Path(__file__).resolve(), support_path, inputs,
                      root / 'tests/entity/player/test_held_attack.c',
                      root / 'tests/entity/player/test_attack.c',
                      root / 'tests/client/statistics/test_player_dispatch.c',
                      root / 'src/entity/player/attack.c', root / 'src/item/combat/combat.c',
                      root / 'src/inventory/player/constructor.c', root / 'src/item/stack/stack.c',
                      root / 'include/mc/entity/player/attack.h', root / 'include/mc/item/combat/combat.h']
    selected_inputs += [path for path in reference.rglob('*') if path.is_file()]
    before = snapshot(selected_inputs)
    result = subprocess.run([str(executable), str(resources), str(game), str(inputs)], capture_output=True,
                            timeout=180)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    print(result.stderr.decode('utf-8'), end='', file=sys.stderr)
    report = {'status': 'FAIL', 'exit_code': result.returncode, 'inputs_before': before,
              'inputs_after': snapshot(selected_inputs)}
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    if result.returncode or b'Player held Item attack checks' not in result.stdout:
        raise RuntimeError(f'Native held Item attack exited {result.returncode}: {output}')
    if report['inputs_after'] != before:
        raise RuntimeError('Declared held Item execution inputs changed during run')
    text = result.stdout.decode('utf-8')
    expected = json.loads((reference / 'finite.json').read_text(), object_pairs_hook=comparison.unique)
    comparison.compare(comparison.rows(text), expected['rows'])
    observed_cows = cow.rows(text)
    original_cows = json.loads((reference / 'cow-death.json').read_text(), object_pairs_hook=comparison.unique)
    for observation in original_cows['observations']:
        cow.compare(observed_cows, observation['rows'])
    report.update(status='PASS', original_rows=422, audited_cow_rows=3,
                  cow_entropy_audits=cow.audit(observed_cows), inputs_unchanged=True)
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    print(next(line for line in text.splitlines() if line.startswith('PASS:')))
    print('Held Item attacks:422 exact original rows;3 independently audited Cow death rows')


if __name__ == '__main__':
    main()
