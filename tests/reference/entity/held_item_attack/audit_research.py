"""Independently audit the retained32 ordinary-original Dirt/Wool observations."""
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def require(value, text):
    if not value:
        raise AssertionError(text)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def audit(directory, jar):
    directory = Path(directory)
    report = json.loads((directory / 'capture.json').read_text())
    require(report['status'] == 'PASS' and report['row_count'] == 32 and
            report['inputs_before'] == report['inputs_after'] and
            report['helpers_before'] == report['helpers_after'], 'retained capture continuity')
    require(all(digest(path) == value for path, value in report['inputs_before'].items()),
            'retained source/tool/JAR inputs still match')
    require(all(digest(directory / 'classes' / name) == value
                for name, value in report['helpers_before'].items()), 'retained helper bytes')
    for event in ('compile', 'bytecode', 'original'):
        item = report[event]
        require(item['exit'] == 0 and not item['timed_out'] and item['pipe_capture_complete'] and
                not item['reader_errors'] and not item['output_overflow'], 'complete ordinary event')
        for stream in ('stdout', 'stderr'):
            path = directory / item[stream + '_file']
            require(digest(path) == item[stream + '_sha256'] and
                    path.stat().st_size == item[stream + '_bytes'], 'actual pipe hashes/lengths')
    pins = json.loads((directory / 'source-pins.json').read_text())
    require(digest(jar) == JAR and pins['client_jar_sha256'] == JAR, 'untouched original archive')
    spec = importlib.util.spec_from_file_location('held_research_classfile',
        ROOT / 'tests/reference/client/controller/classfile.py')
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins['classes'].items():
            actual = inspector.inspect_class(archive.read(name + '.class'))
            signatures = {(m['name'], m['descriptor']) for m in expected['methods']}
            if name not in ('ck', 'bi'):
                actual['methods'] = [m for m in actual['methods']
                                     if (m['name'], m['descriptor']) in signatures]
            require(actual == expected, 'independent class/Code pins: ' + name)
    stdout = (directory / 'original.stdout').read_text()
    require(stdout.splitlines()[-1] == 'Original held Item attack: 32 rows; network=0; native=0; genuine_owners=1; random_rebinding=0',
            'original ordinary completion guards')
    rows = [json.loads(line.removeprefix('HELD_ITEM ')) for line in stdout.splitlines()
            if line.startswith('HELD_ITEM ')]
    require(rows == json.loads((directory / 'selected-outcomes.json').read_text())['rows'] and
            rows == json.loads((HERE / 'research-rows.json').read_text())['rows'],
            'published selected rows are exact stdout observations')
    profiles = [(item + '-' + mode) for item in ('dirt', 'wool')
                for mode in ('ordinary', 'falling', 'resisted', 'fatal')]
    profiles += ['wool-meta-' + str(i) for i in range(16)]
    profiles += [item + '-count' + str(count) + '-' + mode for item in ('dirt', 'wool')
                 for count in (0, -3) for mode in ('ordinary', 'fatal')]
    require([row['profile'] for row in rows] == profiles, 'exact independent32 profile order')
    for row in rows:
        damage = 2 if row['mode'] == 'falling' else 1
        fatal, resisted = row['mode'] == 'fatal', row['mode'] == 'resisted'
        require(all(row[key + '-before'] == row[key + '-after']
                    for key in ('id', 'count', 'damage', 'animation')), 'base Item stack unchanged')
        require(row['selected-same'] == (row['count-before'] > 0) and
                row['selected-cleared'] == (row['count-before'] <= 0) and
                row['inventory-changed'] is False and row['used-before'] == row['used-after'] == 11,
                'actual original selection/used statistic semantics')
        require(row['health'] == (0 if fatal else 10 if resisted else 10 - damage) and
                row['previous-health'] == (1 if fatal else 10) and row['damage-dealt'] == damage and
                row['score'] == (10 if fatal else 3) and row['mob-kills'] == int(fatal) and
                row['drops'] == int(fatal) and row['dying'] is fatal,
                'independent selected attack/death formula')
    return {'status': 'PASS', 'rows': 32, 'jar_sha256': JAR,
            'capture_sha256': digest(directory / 'capture.json'),
            'selected_rows_sha256': digest(directory / 'selected-outcomes.json'),
            'source_pins_sha256': digest(directory / 'source-pins.json'),
            'helpers': report['helpers_before'], 'input_and_helper_continuity': True,
            'ordinary_exit': report['original']['exit'], 'network': 0, 'native_links': 0}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--jar', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.directory, args.jar), indent=2))
