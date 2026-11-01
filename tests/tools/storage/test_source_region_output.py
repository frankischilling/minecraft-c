"""Decode actual source region-output files independently of the native owners."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
BOUND = 1048576


def require(value, message):
    if not value:
        raise AssertionError(message)


def bounded(path):
    require(path.is_file() and path.stat().st_size <= BOUND, 'bounded actual file')
    return path.read_bytes()


def fingerprint(path):
    data = bounded(path)
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def payload(length):
    state, result = 0x12345678, bytearray()
    for _ in range(length):
        state ^= (state << 13) & 0xffffffff
        state ^= state >> 17
        state ^= (state << 5) & 0xffffffff
        result.append(state & 255)
    return bytes(result)


def nbt(raw):
    """Read the authored exact typed tree; no production codec is imported."""
    require(len(raw) >= 18 and raw[:3] == b'\x0a\0\0', 'single unnamed Compound root')
    at = 3
    require(raw[at] == 7, 'actual ByteArray type')
    at += 1
    size = struct.unpack_from('>H', raw, at)[0]
    at += 2
    require(size == 7 and raw[at:at + size] == b'Payload', 'actual authored field name')
    at += size
    length = struct.unpack_from('>i', raw, at)[0]
    at += 4
    require(0 <= length <= BOUND and at + length + 1 == len(raw) and raw[-1] == 0,
            'bounded ByteArray plus single Compound End/no trailing data')
    data = raw[at:at + length]
    require(data == payload(length), 'independently regenerated exact ByteArray contents')
    return {'type': 10, 'name': [], 'fields': {'Payload': {'type': 7, 'length': length,
            'sha256': hashlib.sha256(data).hexdigest()}}, 'raw_bytes': len(raw)}


def region(path):
    image = bounded(path)
    require(8192 <= len(image) <= BOUND and len(image) % 4096 == 0, 'bounded complete region sectors')
    offsets = list(struct.unpack_from('>1024i', image, 0))
    timestamps = list(struct.unpack_from('>1024i', image, 4096))
    occupied, entries = {0, 1}, {}
    for slot, offset in enumerate(offsets):
        if offset == 0:
            require(timestamps[slot] == 0, 'absent slots retain zero timestamps')
            continue
        require(offset > 0, 'admitted ordinary positive offset')
        start, count = offset >> 8, offset & 255
        require(count > 0 and start >= 2 and start + count <= len(image) // 4096, 'actual header sector extent')
        extent = set(range(start, start + count))
        require(not occupied.intersection(extent), 'nonoverlapping live sector runs')
        occupied.update(extent)
        at = start * 4096
        length = struct.unpack_from('>i', image, at)[0]
        require(1 < length <= count * 4096 - 4 and image[at + 4] == 2, 'actual bounded selector2 record')
        compressed = image[at + 5:at + 4 + length]
        decoder = zlib.decompressobj(15)
        raw = decoder.decompress(compressed, BOUND + 1)
        require(len(raw) <= BOUND and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
                'one complete bounded zlib member including Adler checksum')
        entries[str(slot)] = {'sector': start, 'required': count, 'timestamp': timestamps[slot],
                              'compressed_bytes': len(compressed),
                              'compressed_sha256': hashlib.sha256(compressed).hexdigest(), 'nbt': nbt(raw)}
    return {'extent': len(image), 'offsets': offsets, 'timestamps': timestamps,
            'entries': entries, 'occupied': sorted(occupied)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve()
    helper = source / 'tests/reference/client/statistics/support.py'
    specification = importlib.util.spec_from_file_location('region_output_transport', helper)
    support = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(support)
    scenario_path = Path(__file__).with_name('source_region_output_scenarios.json')
    scenarios = json.loads(bounded(scenario_path))
    frozen_paths = [Path(__file__).resolve(), scenario_path, helper, args.executable.resolve()]
    # Executables have their separate bounded build/product receipt; this freeze
    # hashes the actual native product without pretending it is a region image.
    def freeze():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in frozen_paths}
    before = freeze()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='source-region-output-', dir=args.work_dir.resolve()))
    results = {}
    for mode in ('flow', 'owners', 'cleanup-failure', 'constructor-budget'):
        directory = work / mode
        owned = directory / 'owned'
        (owned / 'snapshots').mkdir(parents=True)
        capture = support.run_bounded([str(args.executable.resolve()), str(owned), mode],
                                      directory, 'native', timeout=45, limit=BOUND)
        require(capture.returncode == 0 and capture.stderr == b'', mode + ': complete exit0/empty stderr')
        rows = [json.loads(line) for line in capture.stdout.splitlines()]
        require(rows and rows[-1]['event'] == 'complete' and rows[-1]['mode'] == mode and
                rows[-1]['checks'] > 0, mode + ': complete native receipt')
        states = {row['step']: row for row in rows[:-1] if row['event'] == 'state'}
        require(len(states) == sum(row['event'] == 'state' for row in rows[:-1]), 'unique state labels')
        snapshots = {path.stem: {'file': fingerprint(path), 'decoded': region(path)}
                     for path in sorted((owned / 'snapshots').glob('*.mcr'))}
        for label, row in states.items():
            decoded = snapshots[label]['decoded']
            require(row['offsets'] == decoded['offsets'] and row['timestamps'] == decoded['timestamps'] and
                    row['extent'] == decoded['extent'] and row['sectors'] * 4096 == decoded['extent'],
                    label + ': actual emitted headers match passive RAM state')
            require(len(row['free']) == row['sectors'] and row['free'][:2] == [0, 0] and
                    {i for i, free in enumerate(row['free']) if not free} == set(decoded['occupied']),
                    label + ': exact RAM free-sector mask agrees with independent live region extents')
            require(row['owner_bytes'] <= row['peak_bytes'] and row['compressed_length'] <= row['capacity'],
                    label + ': honest bounded output storage')
        if mode == 'constructor-budget':
            require(set(states) == {'constructor-deflater-limit', 'constructor-buffer-limit'},
                    'both distinct constructor failure prefixes retained')
            a, b = states['constructor-deflater-limit'], states['constructor-buffer-limit']
            budget_rows = [row for row in rows if row['event'] == 'constructor_budget']
            require(len(budget_rows) == 1, 'one dynamically derived native constructor budget')
            budgets = budget_rows[0]
            base = a['owner_bytes'] - a['capacity']
            require(a['capacity'] == 8096 and budgets['base_owner_bytes'] == base > 0 and
                    a['owner_bytes'] < budgets['deflater_budget'] == 65536 and
                    a['peak_bytes'] <= 65536 and budgets['buffer_budget'] == base + 8096 - 1,
                    'partial zlib allocations retired; native base size is observed rather than hardcoded')
            require(a['qid'] == b['qid'] and a['raf_id'] == b['raf_id'] and a['qholds'] == b['qholds'] == 2 and
                    [a['x'], a['z']] == [budgets['requested_x'], budgets['requested_z']] == [7, 11] and
                    b['capacity'] == 0 and b['owner_bytes'] == b['peak_bytes'] == base and
                    b['x'] == b['z'] == 0,
                    'Hz coordinate stores follow its returning byte[8096] superclass constructor')
            for row in (a, b):
                require(not row['closed'] and not row['finished'] and not row['deflater_ended'] and
                        not row['commit_entered'] and not row['commit_returned'] and
                        row['source_closes'] == row['written'] == row['raw_bytes'] == row['compressed_length'] == 0 and
                        row['raf_open'] and not row['raf_source_close'] and row['growth'] == 8192 and
                        snapshots[row['step']]['decoded']['entries'] == {},
                        'constructor policy failure adds no source close, stream emission or region commit')
            require(all(a[key] == b[key] for key in ('raf_reads', 'raf_writes', 'raf_seeks', 'raf_bytes')) and
                    budgets['extra_raf_writes'] == 0 and
                    snapshots['constructor-deflater-limit']['file'] == snapshots['constructor-buffer-limit']['file'] ==
                    snapshots['constructor-retired']['file'] and
                    snapshots['constructor-retired']['decoded']['extent'] == 8192 and
                    rows[-1]['native_closes'] == 1 and rows[-1]['consumed_faults'] == 0,
                    'same cached QJ and safe cleanup preserve constructor-only region bytes and close once')
        elif mode == 'flow':
            main_qid = states['fresh-open']['qid']
            for case in scenarios['flow']:
                name = case['step']
                opened, written, closed, growth = [states[name + '-' + suffix]
                                                    for suffix in ('open', 'written', 'closed', 'growth')]
                require(snapshots[name + '-open']['file'] == snapshots[name + '-written']['file'],
                        name + ': no commit while NBT writes into Hz')
                require(closed['closed'] and closed['finished'] and closed['deflater_ended'] and
                        closed['commit_entered'] and closed['commit_returned'] and not closed['oversized'],
                        name + ': actual complete outer close and QJ commit')
                require([closed[key] for key in ('sector', 'required', 'appended', 'growth')] ==
                        [case[key] for key in ('sector', 'required', 'appended', 'growth')],
                        name + ': independently authored allocation/growth branch')
                slot = str((case['x'] & 31) + 32 * (case['z'] & 31))
                record = snapshots[name + '-closed']['decoded']['entries'][slot]
                require(record['nbt']['fields']['Payload']['length'] == case['length'] and
                        record['compressed_bytes'] == closed['compressed_length'] and
                        record['sector'] == case['sector'] and record['required'] == case['required'],
                        name + ': independent complete payload readback/sector placement')
                require(closed['first'] // 1000 <= record['timestamp'] <= closed['last'] // 1000,
                        name + ': independently sampled actual timestamp interval')
                require(closed['written'] == closed['raw_bytes'] == case['length'] + 18 and
                        written['written'] == case['length'] + 18 and written['source_closes'] == 0 and
                        growth['growth1'] == case['growth'] and growth['growth2'] == growth['growth'] == 0,
                        name + ': real primitive count and late signed growth reset')
                require(snapshots[name + '-closed']['file'] == snapshots[name + '-growth']['file'] and
                        growth['source_closes'] == 2 and growth['raf_writes'] == closed['raf_writes'] and
                        growth['raf_seeks'] == closed['raf_seeks'] and growth['raf_bytes'] == closed['raf_bytes'],
                        name + ': repeated outer close and growth getters never replay IO')
                require(opened['qid'] == written['qid'] == closed['qid'] == growth['qid'],
                        name + ': same actual QJ through output/write/close/growth')
                if name != 'negative':
                    require(closed['qid'] == main_qid, name + ': actual cached main QJ reuse')
            require((owned / 'region/r.-1.-2.mcr').is_file(), 'negative Chunk region coordinates')
            base = snapshots['reuse-growth']['file']
            for label in ('abandon-written', 'flush-before', 'flush-old', 'flush-replacement',
                          'flush-old-failed', 'flush-new-uncommitted'):
                require(snapshots[label]['file'] == base, label + ': no abandoned or failed replacement commit')
            old, new, failed = states['flush-old'], states['flush-replacement'], states['flush-old-failed']
            require(not old['raf_open'] and old['raf_source_close'] and old['entries'] == 0 and
                    old['qholds'] == 1 and old['qid'] == main_qid, 'source flush closes old RAF and removes cache hold')
            require(new['qid'] != old['qid'] and new['raf_id'] != old['raf_id'] and new['raf_open'] and
                    new['entries'] == 1 and new['growth'] == 0, 'replacement retains distinct QJ/RAF on the same actual file')
            require(failed['qid'] == old['qid'] and failed['raf_id'] == old['raf_id'] and
                    failed['closed'] and failed['finished'] and failed['deflater_ended'] and
                    failed['commit_entered'] and not failed['commit_returned'] and failed['diagnostic'] and
                    failed['cause_pc'] == 33 and failed['prior_pc'] == 74,
                    'old Hz close reaches old closed RAF and explicit qj441 diagnostic frontier')
            require('4' not in region(owned / 'region/r.0.0.mcr')['entries'] and
                    '2' not in region(owned / 'region/r.0.0.mcr')['entries'],
                    'final native cleanup never finishes abandoned/replacement outputs')
        else:
            require(snapshots['owners-uncommitted']['file'] == snapshots['owners-retired']['file'] and
                    snapshots['owners-retired']['decoded']['entries'] == {} and
                    snapshots['owners-retired']['decoded']['extent'] == 8192,
                    mode + ': native final retirement preserves constructor-only bytes')
            require(rows[-1]['native_closes'] == 1 and rows[-1]['consumed_faults'] == (mode == 'cleanup-failure'),
                    mode + ': one actual native RAF close with no replay')
            if mode == 'cleanup-failure':
                cleanup = [row for row in rows if row['event'] == 'cleanup']
                require(len(cleanup) == 1 and cleanup[0]['retiring'] and cleanup[0]['inactive'] and
                        cleanup[0]['native_policy'] and not cleanup[0]['source_exception'] and
                        cleanup[0]['source_closes'] == cleanup[0]['committed'] == 0 and
                        snapshots['cleanup-quarantine']['file'] == snapshots['owners-retired']['file'],
                        'consumed lower close retains inactive native quarantine; retry adds no source work')
        inventory = {path.relative_to(owned).as_posix(): fingerprint(path)
                     for path in sorted(owned.rglob('*')) if path.is_file()}
        results[mode] = {'root': str(owned), 'capture': capture.capture_report, 'summary': rows[-1],
                         'states': states, 'files': inventory, 'snapshots': snapshots}
    after = freeze()
    require(before == after, 'fixture/scenarios/helper/product unchanged through execution')
    report = {'status': 'PASS_AUTHORED_NATIVE_SOURCE_REGION_OUTPUT', 'retained_work': str(work),
              'input_before_sha256': before, 'input_after_sha256': after, 'inputs_unchanged': True,
              'profiles': results, 'original_parity_claim': False,
              'scope': 'Concrete lower region output owners, actual cached QJ/RAF, source close/growth and native cleanup. Single authored ByteArray root; no World/Chunk/Entity serializer or gameplay save claim.',
              'order_limit': 'Passive before/after RAM/RAF counters and actual disk snapshots qualify whole successful branches. Per-operation RAF order and partial failure trace matrix remain separate.',
              'source_frontier': 'Original closed-RAF IOException catch prints and returns; native flow ends at qj441 diagnostic dependency. Later replacement commit is not credited.',
              'native_controls': 'Repeated outer close, native readback calls, ownership refusal and real-IO-first consumed-close failure/retry are additional native controls; no original OS fault equivalence.'}
    (work / 'REPORT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'retained': str(work), 'profiles': len(results),
                      'checks': sum(item['summary']['checks'] for item in results.values())}))


if __name__ == '__main__':
    main()
