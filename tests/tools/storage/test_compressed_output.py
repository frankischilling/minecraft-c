"""Bounded real-file codec controls with independent authored byte expectations."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
HEADER = bytes.fromhex('1f8b08000000000000ff')


def need(value, message):
    if not value:
        raise ValueError(message)


def mutf8(units):
    out = bytearray()
    for c in units:
        if 1 <= c <= 0x7f:
            out.append(c)
        elif c <= 0x7ff:
            out.extend((0xc0 | c >> 6, 0x80 | c & 0x3f))
        else:
            out.extend((0xe0 | c >> 12, 0x80 | c >> 6 & 0x3f, 0x80 | c & 0x3f))
    return struct.pack('>H', len(out)) + out


def utf(text):
    return mutf8(tuple(map(ord, text)))


def named(kind, name, payload):
    return bytes((kind,)) + utf(name) + payload


def expected_types():
    values = [
        named(1, 'a', struct.pack('>b', -7)),
        named(2, 'b', struct.pack('>h', -12345)),
        named(3, 'c', struct.pack('>i', -123456789)),
        named(4, 'd', struct.pack('>q', -(1 << 63) + 7)),
        named(5, 'e', bytes.fromhex('7fc00000')),
        named(6, 'f', bytes.fromhex('7ff8000000000000')),
        named(7, 'g', struct.pack('>i', 32) + bytes(range(32))),
        named(8, 'h', mutf8((0, 0x41, 0x7ff, 0x800, 0xd800, 0xdc00, 0xd83d, 0xde03, 0xffff))),
        named(9, 'i', b'\x03' + struct.pack('>iiii', 3, -3, 0, (1 << 31) - 1)),
        named(9, 'j', b'\x01\0\0\0\0'),
    ]
    deep = b'\0'
    for _ in range(15):
        deep = named(10, 'x', deep) + b'\0'
    values.append(named(10, 'k', deep))
    long_text = ''.join(chr(ord('A') + i % 26) for i in range(40000))
    values.append(named(8, 'l', utf(long_text)))
    return named(10, '', b''.join(values) + b'\0')


def expected_large():
    rng = 0x6d2b79f5
    data = bytearray()
    for _ in range(65536):
        rng ^= (rng << 13) & 0xffffffff
        rng ^= rng >> 17
        rng ^= (rng << 5) & 0xffffffff
        data.append(rng & 255)
    return named(10, '', named(7, 'x', struct.pack('>i', len(data)) + data) + b'\0')


def source_root(explicit):
    if explicit is not None:
        return explicit.resolve(strict=True)
    for ancestor in Path(__file__).resolve().parents:
        if (ancestor / 'CMakeLists.txt').is_file() and (ancestor / 'tests/reference').is_dir():
            return ancestor
    raise ValueError('source root not found')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--executable', type=Path, required=True)
    p.add_argument('--work-dir', type=Path, required=True)
    p.add_argument('--source-root', type=Path)
    args = p.parse_args()
    root = source_root(args.source_root)
    helper = root / 'tests/reference/client/statistics/support.py'
    spec = importlib.util.spec_from_file_location('compressed_output_support', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='compressed-', dir=args.work_dir.resolve()))
    owned = work / 'owned'
    owned.mkdir()
    command = [str(args.executable.resolve(strict=True)), str(owned)]
    result = support.run_bounded(command, work, 'native', timeout=180, limit=1048576)
    need(result.returncode == 0 and result.stderr == b'', 'complete actual exit0 and empty stderr')
    match = re.fullmatch(rb'COMPRESSED_OUTPUT_OK checks=([1-9][0-9]{0,9}) writes=([1-9][0-9]{0,9}) closes=([1-9][0-9]{0,9})\r?\n', result.stdout)
    need(match is not None, 'one exact authored native footer')
    wanted = {'types.gz', 'large.gz', 'depth-limit.gz', 'array-limit.gz', 'write-fault.gz',
              'finish-fault.gz', 'close-fault.gz', 'native-close-fault.gz', 'byte-limit.gz', 'cycle.gz'}
    need({p.name for p in owned.iterdir()} == wanted and all(p.is_file() for p in owned.iterdir()),
         'exact real output files and no alternate transport')
    expected = {'types.gz': expected_types(), 'large.gz': expected_large(),
                'close-fault.gz': expected_types()}
    rows = {}
    for name in sorted(wanted):
        path = owned / name
        need(path.stat().st_size <= 4194304, 'bounded file size')
        data = path.read_bytes()
        need(data.startswith(HEADER), 'exact reached JDK17 gzip header: ' + name)
        row = {'file': support.fingerprint(path)}
        if name in expected:
            decoder = zlib.decompressobj(31)
            decoded = decoder.decompress(data, 262145)
            need(len(decoded) <= 262144 and decoder.eof and not decoder.unconsumed_tail and
                 not decoder.unused_data and decoded == expected[name],
                 'bounded independent typed NBT bytes, CRC and trailer: ' + name)
            raw_path = work / (name + '.nbt')
            with raw_path.open('xb') as f:
                f.write(decoded)
            row.update({'raw': support.fingerprint(raw_path), 'decoded_bytes': len(decoded),
                        'independent_authored_payload_exact': True})
        elif name in {'depth-limit.gz', 'array-limit.gz', 'native-close-fault.gz', 'byte-limit.gz', 'cycle.gz'}:
            need(data == HEADER, 'native retirement leaves reached header without source finish: ' + name)
        else:
            need(len(data) > 10, 'actual committed fault prefix: ' + name)
            row['incomplete_fault_prefix_only'] = True
        rows[name] = row
    report = {'status': 'PASS_AUTHORED_NATIVE_ONLY', 'command': command,
              'authored_checks': int(match[1]), 'actual_writes': int(match[2]),
              'actual_closes': int(match[3]), 'capture': result.capture_report,
              'executable': support.fingerprint(args.executable),
              'helper': support.fingerprint(Path(__file__)),
              'bounded_capture_helper': support.fingerprint(helper),
              'stdout': support.fingerprint(work / 'native.stdout'),
              'stderr': support.fingerprint(work / 'native.stderr'), 'rows': rows,
              'original_JVM_or_full_save_success_claim': False, 'retained_work': str(work)}
    with (work / 'REPORT.json').open('x', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'retained': str(work), 'checks': int(match[1])}))


if __name__ == '__main__':
    main()
