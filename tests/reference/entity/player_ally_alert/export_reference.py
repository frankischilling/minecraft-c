"""Publish selected observations from the immutable complete original cohort."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

import packet as P
from observation import entropy, project


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def raw(path):
    return [json.loads(line[len('PLAYER_ALLY_ALERT '):]) for line in path.read_text(encoding='utf-8').splitlines()
            if line.startswith('PLAYER_ALLY_ALERT ')]


def artifact(path, base):
    return {'name': path.relative_to(base).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    base = args.capture.resolve()
    receipt = json.loads((base / 'capture.json').read_text(encoding='utf-8'))
    P.need(receipt['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and
           receipt['input_before_sha256'] == receipt['input_after_sha256'] and
           receipt['helper_before_sha256'] == receipt['helper_after_sha256'], 'complete immutable four-VM capture')
    original = {name: digest(P.HERE / name) for name in P.ORIGINAL_SOURCES}
    processing = {name: digest(P.HERE / name) for name in P.PROCESSING_SOURCES}
    P.need(all(receipt['public_source_sha256'][name] == pin for name, pin in original.items()), 'actual captured original producer identity')
    observations, runs, traces, streams = [], [], [], []
    for kind in ('ordinary', 'debugger'):
        for index in range(2):
            path = base / receipt['ordinary'][index]['stdout_file'] if kind == 'ordinary' else base / ('debugger-' + str(index)) / 'target.stdout'
            values = raw(path)
            P.need([row['case'] for row in values] == list(range(40)), 'whole original call readback')
            rows = [project(row) for row in values]
            if observations:
                P.need(P.exact(observations[0], rows), 'independent original selected observations agree')
            observations.append(rows)
            runs.append({'kind': kind, 'stdout': artifact(path, base), 'calls': 40, 'summary': P.summary(rows)})
            if kind == 'debugger':
                journal = path.parent / 'journal.jsonl'
                records = [json.loads(line) for line in journal.read_text(encoding='utf-8').splitlines()]
                calls = [row for row in records if row['event'] == 'call']
                P.need([row['case'] for row in calls] == list(range(40)), 'complete passive call journals')
                pins = json.loads((P.HERE / 'source-pins.json').read_text(encoding='utf-8'))['classes']
                for code in (row for row in records if row['event'] == 'code'):
                    method = next(method for method in pins[code['class']]['methods'] if
                                  method['name'] == code['method'] and method['descriptor'] == code['descriptor'])
                    P.need(code['bytes'] == method['code_length'] and code['sha256'] == method['code_sha256'], 'actual executing original Code')
                traces.append(calls)
                streams.append([entropy(row) for row in values])
                runs[-1]['journal'] = artifact(journal, base)
    finite = {'schema': 'PLAYER_ALLY_ALERT_FINITE_V1', 'scope': P.SCOPE, 'rows': observations[0], 'traces': traces, 'entropy': streams}
    (P.HERE / 'finite.json').write_text(json.dumps(finite, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
    shutil.copyfile(base / 'capture.json', P.HERE / 'capture-receipt.json')
    provenance = {'schema': 'PLAYER_ALLY_ALERT_PROVENANCE_V1', 'scope': P.SCOPE, 'archive_sha256': P.JAR,
                  'original_sources': original, 'processing_sources': processing,
                  'finite_sha256': digest(P.HERE / 'finite.json'), 'capture_sha256': digest(P.HERE / 'capture-receipt.json'),
                  'runs': runs, 'summary': P.summary(observations[0])}
    (P.HERE / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8', newline='\n')
    P.read()
    print(json.dumps(provenance['summary'], sort_keys=True))


if __name__ == '__main__':
    main()
