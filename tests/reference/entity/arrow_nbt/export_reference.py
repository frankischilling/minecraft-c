"""Publish exact four-VM Arrow NBT observations after complete source validation."""
import argparse
import json
from pathlib import Path
import shutil

import packet as P


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args(); base = args.capture.resolve()
    receipt = P.load(base / 'capture.json'); P.validate_capture(receipt)
    sources = {name: P.digest(P.HERE / name) for name in P.ORIGINAL_SOURCES}
    processing = {name: P.digest(P.HERE / name) for name in P.PROCESSING_SOURCES}
    P.need(all(receipt['public_source_sha256'][name] == pin for name, pin in sources.items()), 'captured producer identities')
    pins = P.load(P.HERE / 'source-pins.json'); raw, traces, receipts, runs, code_runs = [], [], [], [], []
    for kind in ('ordinary', 'debugger'):
        for i in range(2):
            path = base / receipt['ordinary'][i]['stdout_file'] if kind == 'ordinary' else base / ('debugger-' + str(i)) / 'target.stdout'
            values = P.parse_rows(path.read_text(encoding='utf-8')); raw.append(values)
            run = {'kind': kind, 'stdout': {'name': path.relative_to(base).as_posix(), 'bytes': path.stat().st_size, 'sha256': P.digest(path)}}
            if kind == 'debugger':
                journal = path.parent / 'journal.jsonl'; calls, observed, codes = P.journal(journal, values, pins)
                traces.append(calls); receipts.append(observed); code_runs.append(codes)
                run['journal'] = {'name': journal.relative_to(base).as_posix(), 'bytes': journal.stat().st_size, 'sha256': P.digest(journal)}
            runs.append(run)
    rows = [P.project(row) for row in raw[2][:44]]
    for values in raw:
        P.compare(rows, [P.project(row) for row in values[:44]])
    finite = {'schema': 'ARROW_NBT_FINITE_V1', 'scope': P.SCOPE, 'rows': rows, 'raw_runs': raw,
              'traces': traces, 'receipts': receipts, 'code_runs': code_runs}
    (P.HERE / 'finite.json').write_text(json.dumps(finite, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
    shutil.copyfile(base / 'capture.json', P.HERE / 'capture-receipt.json')
    provenance = {'schema': 'ARROW_NBT_PROVENANCE_V1', 'scope': P.SCOPE, 'archive_sha256': P.JAR,
                  'original_sources': sources, 'processing_sources': processing, 'finite_sha256': P.digest(P.HERE / 'finite.json'),
                  'capture_sha256': P.digest(P.HERE / 'capture-receipt.json'), 'runs': runs}
    (P.HERE / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8', newline='\n')
    P.read(); print('Arrow NBT original packet: four VMs; 192 complete calls; 44 paired and four source-only profiles')


if __name__ == '__main__':
    main()
