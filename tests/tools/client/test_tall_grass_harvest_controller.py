"""Replay actual singleplayer TallGrass controller calls against original rows."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    packet = module('tall_grass_controller_packet',
                    root / 'tests/reference/block/tall_grass_harvest/packet.py')
    corpus = packet.read_packet()
    profiles = [p for p in corpus['profiles'] if p['mode'] == 1]
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='tall-grass-controller-', dir=args.work_dir.resolve()))
    support = module('tall_grass_controller_resources',
                     root / 'tests/reference/client/player_statistics/support.py')
    resources = output / 'resources'
    support.resources_at(resources)
    executable = args.executable.resolve(strict=True)
    checks, rows = 0, 0
    for p in profiles:
        fields = [str(p[k]) for k in packet.PROFILE_COLUMNS]
        game = output / ('game-' + fields[0])
        game.mkdir()
        result = subprocess.run([str(executable), str(resources), str(game), *fields],
                                capture_output=True, timeout=120)
        (output / (fields[0] + '.stdout')).write_bytes(result.stdout)
        (output / (fields[0] + '.stderr')).write_bytes(result.stderr)
        text = result.stdout.decode('utf-8', 'strict').splitlines()
        counts = [re.fullmatch(r'PASS: ([0-9]+) concrete Block harvest checks', line) for line in text]
        counts = [m for m in counts if m is not None]
        if result.returncode or len(counts) != 1:
            raise AssertionError(f'TallGrass controller {p} failed: {result.stderr.decode("utf-8")}')
        actual = [line for line in text if line.split('\t', 1)[0] in ('RESULT', 'ITEM', 'EVENT')]
        expected = corpus['by_profile'][p['index']]
        if actual != expected:
            raise AssertionError(f'TallGrass original/native mismatch for {p}:\n'
                                 f'native={actual}\noriginal={expected}')
        rows += len(actual)
        checks += int(counts[0][1])
    if len(profiles) != 68 or rows != 192:
        raise AssertionError('TallGrass native replay scope changed')
    print(f'PASS: 68 exact original TallGrass controller profiles, 192 rows, {checks} checks; '
          'six original NULL-Player direct calls remain supplemental')


if __name__ == '__main__':
    main()
