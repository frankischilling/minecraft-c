"""UNRUN authored real-file fm.c/fm.b controls; no original/JDK/game assets."""
import argparse
import gzip
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def named(kind, name, payload):
    units = name.encode('ascii')
    return bytes([kind]) + struct.pack('>H', len(units)) + units + payload


def metadata(seed, wrong_version=False):
    # Explicit authored interchange input, not captured game answers or a
    # runtime replacement for the canonical NBT parser/Info constructors.
    data = named(4, 'RandomSeed', struct.pack('>q', seed))
    for key, value in [('SpawnX', -3), ('SpawnY', 64), ('SpawnZ', 7)]:
        data += named(3, key, struct.pack('>i', value))
    data += named(8, 'LevelName', struct.pack('>H', 8) + b'Authored')
    data += named(8 if wrong_version else 3, 'version',
                  b'\x00\x01x' if wrong_version else struct.pack('>i', 19132))
    return b'\x0a\x00\x00' + named(10, 'Data', data + b'\x00') + b'\x00'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('source_reads_capture',
        ROOT / 'tests/reference/client/statistics/support.py')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='source-reads-', dir=args.work_dir.resolve()))
    root = work / 'owned'
    root.mkdir()
    compressed = lambda data: gzip.compress(data, mtime=0)
    inputs = {
        'primary/level.dat': compressed(metadata(42)),
        'primary/level.dat_old': compressed(metadata(84)),
        'old-only/level.dat_old': compressed(metadata(84)),
        'missing-data/level.dat': compressed(b'\x0a\x00\x00\x00'),
        'bad-primary/level.dat': b'bad',
        'bad-primary/level.dat_old': compressed(metadata(84)),
        'bad-old/level.dat_old': b'bad',
        'wrong-root/level.dat': compressed(b'\x01\x00\x00\x00'),
        'wrong-data/level.dat': compressed(b'\x0a\x00\x00' + named(1, 'Data', b'\x00') + b'\x00'),
        'wrong-version/level.dat': compressed(metadata(42, wrong_version=True)),
    }
    for rel, data in inputs.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open('xb') as stream:
            stream.write(data)
    before = {rel: support.fingerprint(root / rel) for rel in inputs}
    exe = args.executable.resolve(strict=True)
    executable = support.fingerprint(exe)
    result = support.run_bounded([str(exe), str(root)], work, 'native', timeout=180,
                                 limit=1048576)
    support.require(result.returncode == 0 and result.stderr == b'',
                    'complete authored native exit0/empty stderr')
    match = re.fullmatch(rb'Source metadata/session authored contracts: ([1-9][0-9]*) checks\r?\n',
                         result.stdout)
    support.require(match is not None, 'one complete positive authored footer')
    support.require({rel: support.fingerprint(root / rel) for rel in inputs} == before,
                    'actual metadata input files unchanged')
    support.require(support.fingerprint(exe) == executable, 'native executable unchanged')
    report = {'status': 'PASS_AUTHORED_NATIVE_ONLY', 'checks': int(match[1]),
              'executable': executable, 'capture': result.capture_report,
              'input_files_before': before, 'input_files_after': before,
              'retained_files': {p.relative_to(root).as_posix(): support.fingerprint(p)
                                 for p in sorted(root.rglob('*')) if p.is_file()},
              'raw': {name: support.fingerprint(work / name) for name in
                       ['native.stdout', 'native.stderr', 'native.capture.json']},
              'scope': 'Finite authored metadata/session contracts; us and diagnostic frontiers remain'}
    with (work / 'REPORT.json').open('x', encoding='utf8', newline='\n') as stream:
        stream.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'retained': str(work)}))


if __name__ == '__main__':
    main()
