"""Publish immutable four-VM source observations, with explicit unpaired entropy."""
import argparse
import json
from pathlib import Path
import shutil

import packet as P


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args(); base = args.capture.resolve()
    receipt = P.load(base / 'capture.json')
    P.need(receipt['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and receipt['profiles_per_vm'] == 40 and
           receipt['input_before_sha256'] == receipt['input_after_sha256'] and receipt['helper_before_sha256'] == receipt['helper_after_sha256'], 'actual immutable complete capture')
    sources = {name: P.digest(P.HERE / name) for name in P.ORIGINAL_SOURCES}
    processing = {name: P.digest(P.HERE / name) for name in P.PROCESSING_SOURCES}
    P.need(all(receipt['public_source_sha256'][name] == pin for name, pin in sources.items()), 'captured producer identity')
    pins = P.load(P.HERE / 'source-pins.json'); raw, traces, receipts, runs, code_runs = [], [], [], [], []
    for kind in ('ordinary', 'debugger'):
        for index in range(2):
            path = base / receipt['ordinary'][index]['stdout_file'] if kind == 'ordinary' else base / ('debugger-' + str(index)) / 'target.stdout'
            values = P.parse_rows(path.read_text(encoding='utf-8')); raw.append(values)
            run = {'kind': kind, 'stdout': {'name': path.relative_to(base).as_posix(), 'bytes': path.stat().st_size, 'sha256': P.digest(path)}}
            if kind == 'debugger':
                journal = path.parent / 'journal.jsonl'; calls, observed = P.journal(journal, values, pins); traces.append(calls); receipts.append(observed)
                code_runs.append([P.loads(line) for line in journal.read_text(encoding='utf-8').splitlines() if P.loads(line)['event'] == 'code'])
                run['journal'] = {'name': journal.relative_to(base).as_posix(), 'bytes': journal.stat().st_size, 'sha256': P.digest(journal)}
            runs.append(run)
    rows = [P.project(row, trace) for row, trace in zip(raw[2], traces[0])]
    finite = {'schema': 'ARROW_CONSTRUCTION_FINITE_V1', 'scope': P.SCOPE, 'rows': rows, 'raw_runs': raw, 'traces': traces, 'receipts': receipts, 'code_runs': code_runs}
    # Validate all projections before making a public artifact.
    P.compare(rows, [P.project(row, trace) for row, trace in zip(raw[3], traces[1])])
    for values in raw[:2]:
        expected = __import__('copy').deepcopy(rows)
        for case in (14, 15):
            expected[case]['arrow_after'] = None
        P.compare(expected, [P.project(row) for row in values])
    (P.HERE / 'finite.json').write_text(json.dumps(finite, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
    shutil.copyfile(base / 'capture.json', P.HERE / 'capture-receipt.json')
    proof = {'schema': 'ARROW_CONSTRUCTION_PROVENANCE_V1', 'scope': P.SCOPE, 'archive_sha256': P.JAR,
             'original_sources': sources, 'processing_sources': processing, 'finite_sha256': P.digest(P.HERE / 'finite.json'),
             'capture_sha256': P.digest(P.HERE / 'capture-receipt.json'), 'runs': runs}
    (P.HERE / 'provenance.json').write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8', newline='\n')
    P.read(); print('Arrow original packet: 4 VMs; 160 complete calls; 40 finite profiles')


if __name__ == '__main__':
    main()
