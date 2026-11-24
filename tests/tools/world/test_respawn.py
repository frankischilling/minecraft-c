"""Exercise actual client World respawn leaves with authored native inputs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    helper = root / 'tests/reference/client/statistics/support.py'
    spec = importlib.util.spec_from_file_location('world_respawn_capture', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='world-respawn-', dir=args.work_dir.resolve()))
    resources = output / 'resources'
    for name, text in {
            'lang/en_US.lang': 'achievement.openInventory=Inventory\nachievement.openInventory.desc=Use %1$s\nkey.inventory=Inventory\n',
            'lang/stats_US.lang': '', 'lang/en_GB.lang': '', 'achievement/map.txt': ''}.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    inputs = [executable, Path(__file__).resolve(), helper,
              root / 'src/world/runtime/respawn.c', root / 'include/mc/world/runtime/respawn.h',
              root / 'tests/world/runtime/test_respawn.c']
    before = {str(path): digest(path) for path in inputs}
    result = support.run_bounded([str(executable), 'world-respawn',
                                  str(output / 'worlds').encode('utf-8').hex(),
                                  str(resources).encode('utf-8').hex()], cwd=output,
                                 prefix='native', timeout=180)
    if result.returncode != 0 or result.stderr != b'':
        raise RuntimeError('World respawn native failure; retained capture: ' + str(output))
    lines = result.stdout.decode('utf-8', errors='strict').splitlines()
    match = re.fullmatch(r'World respawn: ([1-9][0-9]*) checks', lines[-1] if lines else '')
    if match is None or any(line not in ('16 achievements', '151 recipes') for line in lines[:-1]):
        raise RuntimeError('Incomplete World respawn output: ' + str(output))
    after = {str(path): digest(path) for path in inputs}
    if before != after:
        raise RuntimeError('World respawn inputs changed: ' + str(output))
    report = {'status': 'PASS', 'checks': int(match[1]), 'capture': result.capture_report,
              'input_before_sha256': before, 'input_after_sha256': after,
              'scope': 'Authored native client FD.u/d/v/e leaves and XA/WD can-respawn; no original runtime or loaded respawn comparison'}
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('World respawn: ' + str(report['checks']) + ' checks; report=' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
