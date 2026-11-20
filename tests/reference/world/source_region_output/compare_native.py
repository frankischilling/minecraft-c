"""Compare prepared source region output against independently captured original owners."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
import zlib

sys.dont_write_bytecode = True
BOUND = 1048576


def require(condition, message):
    if not condition:
        raise ValueError(message)


def bounded(path, limit=BOUND):
    require(path.is_file() and path.stat().st_size <= limit, 'bounded existing comparison input')
    return path.read_bytes()


def fingerprint(path, limit=BOUND):
    data = bounded(path, limit)
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def payload(count):
    state, data = 0x12345678, bytearray()
    for _ in range(count):
        state ^= (state << 13) & 0xffffffff
        state ^= state >> 17
        state ^= (state << 5) & 0xffffffff
        data.append(state & 255)
    return bytes(data)


def nbt(data):
    require(len(data) >= 18 and data[:13] == b'\x0a\x00\x00\x07\x00\x07Payload',
            'one unnamed Compound and exactly one named ByteArray')
    count = struct.unpack('>i', data[13:17])[0]
    require(0 <= count <= BOUND and len(data) == count + 18 and data[-1] == 0,
            'bounded exact ByteArray and single root End')
    require(data[17:-1] == payload(count), 'independently regenerated actual payload bytes')
    return {'root_type': 10, 'root_name': [], 'field': 'Payload', 'type': 7,
            'length': count, 'sha256': hashlib.sha256(data[17:-1]).hexdigest(),
            'raw_bytes': len(data)}


def decode(path):
    image = bounded(path)
    require(len(image) >= 8192 and len(image) % 4096 == 0, 'complete source region sectors')
    offsets = list(struct.unpack('>1024i', image[:4096]))
    timestamps = list(struct.unpack('>1024i', image[4096:8192]))
    occupied, entries = {0, 1}, {}
    for slot, offset in enumerate(offsets):
        if offset == 0:
            require(timestamps[slot] == 0, 'absent record timestamp remains zero')
            continue
        require(offset > 0, 'ordinary positive source offset')
        start, count = offset >> 8, offset & 255
        require(start >= 2 and count > 0 and start + count <= len(image) // 4096,
                'actual bounded allocated sector span')
        span = set(range(start, start + count))
        require(not occupied.intersection(span), 'nonoverlapping live record sectors')
        occupied.update(span)
        at = start * 4096
        length = struct.unpack('>i', image[at:at + 4])[0]
        require(1 < length <= count * 4096 - 4 and image[at + 4] == 2,
                'actual source selector2 and bounded compressed length')
        encoded = image[at + 5:at + 4 + length]
        inflater = zlib.decompressobj(15)
        raw = inflater.decompress(encoded, BOUND + 1)
        require(len(raw) <= BOUND and inflater.eof and not inflater.unused_data and
                not inflater.unconsumed_tail, 'complete bounded zlib member/Adler checksum')
        entries[str(slot)] = {'sector': start, 'required': count, 'timestamp': timestamps[slot],
                              'compressed_bytes': len(encoded),
                              'compressed_sha256': hashlib.sha256(encoded).hexdigest(),
                              'nbt': nbt(raw)}
    return {'offsets': offsets, 'timestamps': timestamps, 'entries': entries,
            'extent': len(image), 'free': [int(i not in occupied) for i in range(len(image) // 4096)]}


def flag(value):
    require(type(value) in (int, bool) and value in (0, 1), 'concrete native Boolean value')
    return bool(value)


def interval(stamp, first, last):
    require(type(stamp) is int and first <= last and last - first <= 300000,
            'bounded own real timestamp interval')
    require(any(((second + 0x80000000) & 0xffffffff) - 0x80000000 == stamp
                for second in range(first // 1000, last // 1000 + 1)),
            'actual Java divide1000/narrow timestamp in own process interval')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-receipt', type=Path, required=True)
    parser.add_argument('--native-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference_path = args.reference_receipt.resolve(strict=True)
    native_path = args.native_report.resolve(strict=True)
    reference = json.loads(bounded(reference_path, 16777216))
    native = json.loads(bounded(native_path, 16777216))
    require(reference['status'] == 'OBSERVED_AND_AUDITED_ORIGINAL_SOURCE_REGION_OUTPUT_COHORT' and
            reference['inputs_unchanged'] and reference['ordinary_exit'] == reference['debugger_exit'] == 0,
            'complete frozen ordinary and independent read-only-JDI original receipt')
    require(native['status'] == 'PASS_AUTHORED_NATIVE_SOURCE_REGION_OUTPUT' and
            native['inputs_unchanged'] and native['input_before_sha256'] == native['input_after_sha256'],
            'complete unchanged authored native region-output cohort')
    states = native['profiles']['flow']['states']
    original = {}
    for mode in ('ordinary', 'debugger'):
        rows_path = reference_path.parent / mode / 'target.ndjson'
        rows = [json.loads(row) for row in bounded(rows_path, 16777216).splitlines()]
        require(rows[-1]['event'] == 'complete' and rows[-1]['outputs'] == 9,
                'complete nine-chain original output inventory')
        original[mode] = {(row['phase'], row['stage']): row for row in rows[:-1]}
    native_snapshots = native_path.parent / 'flow' / 'owned' / 'snapshots'

    def native_image(label):
        path = native_snapshots / (label + '.mcr')
        require(fingerprint(path) == native['profiles']['flow']['snapshots'][label]['file'],
                label + ': actual retained native snapshot unchanged')
        return decode(path)

    def original_image(mode, row):
        name = row['owner']['file']
        path = reference_path.parent / mode / 'snapshots' / f"{row['phase']}-{row['stage']}-{name}"
        require(fingerprint(path) == row['files'][name],
                'actual retained original region snapshot unchanged')
        return decode(path)

    def compare_state(label, phase, stage):
        row = states[label]
        image = native_image(label)
        require(row['offsets'] == image['offsets'] and row['timestamps'] == image['timestamps'] and
                row['free'] == image['free'] and row['extent'] == image['extent'],
                label + ': independently decoded native disk agrees with actual passive RAM')
        for mode in ('ordinary', 'debugger'):
            observed = original[mode][phase, stage]
            owner = observed['owner']
            source = original_image(mode, observed)
            require(owner['offsets'] == source['offsets'] and owner['timestamps'] == source['timestamps'] and
                    [int(value) for value in owner['free']] == source['free'],
                    label + ': independently decoded original disk agrees with actual passive RAM')
            require(row['offsets'] == owner['offsets'] and row['free'] == [int(value) for value in owner['free']]
                    and row['extent'] == source['extent'] and row['growth'] == owner['growth'],
                    label + ': exact allocation/free mask/extent/growth against independent original')
            require(row['x'] == owner['localX'] and row['z'] == owner['localZ'] and
                    row['written'] == owner['dosWritten'] and flag(row['closed']) == owner['outerClosed'],
                    label + ': actual local coordinates, primitive count and outer close state')
            require(set(image['entries']) == set(source['entries']), label + ': exact live record slots')
            for slot in image['entries']:
                left, right = image['entries'][slot], source['entries'][slot]
                require(left['sector'] == right['sector'] and left['required'] == right['required'] and
                        left['nbt'] == right['nbt'], label + ': exact typed record and allocated sectors')
        return image

    cases = {}
    normal = [('fresh', 0, 0, 0, 32), ('rewrite', 1, 0, 0, 32), ('grow', 2, 0, 0, 20000),
              ('shrink', 3, 0, 0, 32), ('reuse', 4, 1, 0, 12000), ('negative', 5, -1, -33, 32)]
    histories = {'native': {}, 'ordinary': {}, 'debugger': {}}
    main_native = states['fresh-open']
    for name, phase, x, z, length in normal:
        for suffix, stage in [('open', 'begin'), ('written', 'written'), ('closed', 'closed'),
                              ('growth', 'growth-reset')]:
            compare_state(name + '-' + suffix, phase, stage)
        opened, closed, growth = [states[name + '-' + suffix] for suffix in ('open', 'closed', 'growth')]
        require(opened['capacity'] == 8096 and opened['compressed_length'] == opened['written'] == 0,
                name + ': genuine source Hz8096 construction')
        require(flag(closed['finished']) and flag(closed['deflater_ended']) and
                flag(closed['commit_entered']) and flag(closed['commit_returned']),
                name + ': source close finishes default zlib and returns from commit')
        native_closed = native_image(name + '-closed')
        slot = str((x & 31) + 32 * (z & 31))
        interval(native_closed['timestamps'][int(slot)], closed['first'], closed['last'])
        require(native_closed['entries'][slot]['nbt']['length'] == length,
                name + ': actual independently decoded expected ByteArray length')
        source_rows = {}
        for mode in ('ordinary', 'debugger'):
            row = original[mode][phase, 'closed']
            interval(row['owner']['timestamps'][int(slot)], row['details']['beforeMillis'], row['details']['afterMillis'])
            source_growth = original[mode][phase, 'growth']['details']['returned']
            require(growth['growth1'] == source_growth and growth['growth2'] ==
                    original[mode][phase, 'growth-reset']['details']['returned'] == 0,
                    name + ': exact actual late growth then zero reset')
            owner = row['owner']
            source_rows[mode] = {'timestamp': owner['timestamps'][int(slot)],
                                 'interval': [row['details']['beforeMillis'], row['details']['afterMillis']],
                                 'compressed': owner['hzBytes']}
        # All prior timestamps are preserved within their own process. Only
        # the presently committed slot may acquire a new real sampled value.
        for mode in ('native', 'ordinary', 'debugger'):
            if mode == 'native':
                timestamps = closed['timestamps']
                file = original['ordinary'][phase, 'closed']['owner']['file']
            else:
                owner = original[mode][phase, 'closed']['owner']
                timestamps, file = owner['timestamps'], owner['file']
            previous = histories[mode].get(file)
            if previous is not None:
                require(all(value == previous[i] for i, value in enumerate(timestamps) if i != int(slot)),
                        name + ': unrelated own-process timestamps unchanged')
            histories[mode][file] = timestamps
        require(opened['qid'] == closed['qid'] == growth['qid'] and
                opened['raf_id'] == closed['raf_id'] == growth['raf_id'],
                name + ': same native QJ/RAF throughout output')
        if name != 'negative':
            require(closed['qid'] == main_native['qid'] and closed['raf_id'] == main_native['raf_id'],
                    name + ': actual native cache QJ/RAF reuse')
        else:
            require(closed['qid'] != main_native['qid'] and closed['raf_id'] != main_native['raf_id'] and
                    'region/r.-1.-2.mcr' in native['profiles']['flow']['files'],
                    'negative coordinates select actual distinct region file/QJ/RAF')
        cases[name] = {'phase': phase, 'x': x, 'z': z, 'payload': length,
                       'sector': closed['sector'], 'required': closed['required'],
                       'appended': closed['appended'], 'growth': growth['growth1'],
                       'native_timestamp': closed['timestamp'], 'native_interval': [closed['first'], closed['last']],
                       'source_processes': source_rows,
                       'native_compressed_bytes': closed['compressed_length']}
    common = [('abandon-written', 6, 'abandoned-unclosed'), ('flush-before', 7, 'written'),
              ('flush-old', 7, 'cache-flushed'), ('flush-replacement', 8, 'replacement-unclosed'),
              ('flush-old-failed', 9, 'old-closed-source-returned'),
              ('flush-new-uncommitted', 9, 'replacement-still-unclosed')]
    for label, phase, stage in common:
        compare_state(label, phase, stage)
    old, new, failed = [states[label] for label in ('flush-old', 'flush-replacement', 'flush-old-failed')]
    require(old['qid'] == failed['qid'] == main_native['qid'] and
            old['raf_id'] == failed['raf_id'] == main_native['raf_id'] and
            new['qid'] != old['qid'] and new['raf_id'] != old['raf_id'],
            'native strong old QJ/RAF survives cache flush while replacement is distinct')
    require(not flag(old['raf_open']) and flag(old['raf_source_close']) and old['entries'] == 0 and
            flag(new['raf_open']) and new['entries'] == 1 and flag(failed['diagnostic']) and
            failed['cause_pc'] == 33 and failed['prior_pc'] == 74 and
            flag(failed['commit_entered']) and not flag(failed['commit_returned']),
            'native old closed RAF reaches explicit qj441 prefix without caught return credit')
    for mode in ('ordinary', 'debugger'):
        a, b, c = [original[mode][phase, stage]['owner'] for phase, stage in
                   ((7, 'cache-flushed'), (8, 'replacement-unclosed'), (9, 'old-closed-source-returned'))]
        require(a['qj'] == c['qj'] and a['raf'] == c['raf'] and b['qj'] != a['qj'] and b['raf'] != a['raf'] and
                a['rafPointer']['status'] == 'exception' and a['cache'] == {} and len(b['cache']) == 1,
                'same actual original strong-old/replacement identity and closed-RAF/cache effects')
    report = {'status': 'PASS_PREPARED_SOURCE_REGION_OUTPUT_ORIGINAL_COMPARISON',
              'recipe': fingerprint(Path(__file__), 1048576),
              'reference_receipt': fingerprint(reference_path, 16777216),
              'native_report': fingerprint(native_path, 16777216), 'normal_cases': cases,
              'common_abandon_and_flush_snapshots': [label for label, _, _ in common],
              'clock_policy': 'Each real int timestamp is checked in its own process millisecond interval with Java divide1000/narrow. Unrelated timestamps remain unchanged within that process; no timestamp value is forced equal across processes.',
              'compression_policy': 'Every actual live native/original selector2 payload is independently zlib/Adler decoded and every typed ByteArray byte regenerated. Actual offset/free mask/allocation/growth are exact. Compressed lengths/hashes are retained separately; no cross-zlib byte identity or whole-file equality is asserted.',
              'identity_policy': 'Actual same-node and distinct replacement QJ/RAF relationships are compared within each process. Raw pointer/VM identity numbers and Java/native reference counts are not equated. Native readback, owners and cleanup-fault profiles remain extra native controls.',
              'failure_scope': 'Original old-Hz close reaches the same source-closed RAF, catches/prints IOException and returns. Native stops at qj441 with prior rewrite74/lower seek33. Only the common pre-write RAM/file/closed-RAF/outer-close prefix is compared; no native caught return or later replacement commit/growth is credited.',
              'order_scope': 'Native passive RAM/RAF counters and actual disk snapshots qualify whole successful branches. Original source order has separate read-only JDI evidence; no new native per-operation or injected-failure trace is claimed.',
              'not_qualified': ['source Chunk/World/Entity/TileEntity caller or gameplay save/reload',
                                'Info.SizeOnDisk caller addition', '255/256 sectors or exact4096 boundary',
                                'actual native OS failure equivalence or full Throwable identity']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'normal_cases': len(cases), 'output': str(args.output)}))


if __name__ == '__main__':
    main()
