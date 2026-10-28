"""Run authored real-file contracts on a fresh owned path; no original game."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
SOURCE = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    helper = SOURCE / 'tests/reference/client/statistics/support.py'
    spec = importlib.util.spec_from_file_location('source_handler_bounded_transport', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='handler-', dir=args.work_dir.resolve()))
    root = work / 'owned'
    root.mkdir()
    command = [str(args.executable.resolve()), str(root)]
    result = support.run_bounded(command, work, 'native', timeout=180, limit=1048576)
    support.require(result.returncode == 0 and result.stderr == b'', 'actual complete exit0/empty stderr')
    rows = result.stdout.replace(b'\r\n', b'\n').splitlines()
    footer = None if len(rows) != 1 else re.fullmatch(
        rb'Actual source mx/fm authored contracts: ([1-9][0-9]{0,9}) checks', rows[0])
    support.require(footer is not None, 'single actual authored footer')
    token_paths = ['with-players/session.lock', 'without-players/session.lock', 'from-hi/session.lock', 'obstacle']
    files = {path.relative_to(root).as_posix(): support.fingerprint(path)
             for path in sorted(root.rglob('*')) if path.is_file()}
    support.require(set(files) == set(token_paths) | {'limited/session.lock'} and
                    all(files[path]['bytes'] == 8 for path in token_paths) and files['limited/session.lock']['bytes'] == 0,
                    'exact real eight-byte files, no alternate metadata/world')
    directories = [path.relative_to(root).as_posix() for path in sorted(root.rglob('*')) if path.is_dir()]
    support.require(set(directories) == {'with-players', 'with-players/players', 'with-players/data',
                    'without-players', 'without-players/data', 'from-hi', 'from-hi/data', 'limited', 'limited/data'},
                    'actual players flag and ignored mkdir failure effects')
    report = {'status': 'PASS_AUTHORED_NATIVE_ONLY', 'command': command, 'authored_checks': int(footer[1]),
              'capture': result.capture_report, 'files': files, 'directories': directories,
              'raw_stdout': support.fingerprint(work / 'native.stdout'),
              'raw_stderr': support.fingerprint(work / 'native.stderr'),
              'retained_work': str(work), 'original_or_World_success_claim': False}
    with (work / 'REPORT.json').open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'retained': str(work)}))


if __name__ == '__main__':
    main()
