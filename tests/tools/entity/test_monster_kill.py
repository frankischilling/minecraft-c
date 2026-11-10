"""Execute Player Monster kill callbacks with real App, writer and notice owners."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    support_path = Path(__file__).resolve().parents[2] / 'reference/client/player_statistics/support.py'
    spec = importlib.util.spec_from_file_location('monster_kill_support', support_path)
    if spec is None or spec.loader is None:
        raise RuntimeError('Player statistics capture support unavailable')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='monster-kill-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    result = support.run([args.executable.resolve(strict=True), resources, game], output, 'native')
    lines = result.stdout.splitlines()
    match = re.fullmatch(rb'Player Monster kill: ([1-9][0-9]*) checks', lines[-1]) if lines else None
    prefix = b'MONSTER_DEATH '
    returned = [line for line in lines if line.startswith(prefix)]
    rows = [json.loads(line[len(prefix):]) for line in returned]
    names = ['uz-fatal-player-local-38', 'fr-fatal-player-local-38']
    row_valid = len(rows) == 2 and [row.get('profile') for row in rows] == names
    report = {'returncode': result.returncode, 'stderr_bytes': len(result.stderr),
              'checks': int(match[1]) if match else 0,
              'rows': rows,
              'scope': 'Authored real-owner callback and reached failure controls'}
    banners = [line for line in lines[:-1] if not line.startswith(prefix)]
    report['status'] = ('PASS' if result.returncode == 0 and not result.stderr and match and
                        row_valid and banners == [b'16 achievements', b'151 recipes'] * 3 else 'FAIL')
    if report['status'] == 'PASS':
        (output / 'returns.stdout').write_bytes(b'\n'.join(returned) + b'\n' + lines[-1] + b'\n')
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(result.stdout.decode('utf-8'), end='')
    print(result.stderr.decode('utf-8'), end='')
    if report['status'] != 'PASS':
        raise RuntimeError('Player Monster kill control failed: ' + str(output))
    print('Player Monster kill PASS: ' + str(output))


if __name__ == '__main__':
    main()
