"""Publish finite selected observations from a retained successful four-VM capture."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

import packet as P
from observation import deterministic, finder, need, project


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def artifact(path, base):
    return {'name': path.relative_to(base).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest(path)}


def raw_rows(path):
    return [json.loads(line[len('NIGHTMARE '):]) for line in path.read_text(encoding='utf-8').splitlines()
            if line.startswith('NIGHTMARE ')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    base = args.capture.resolve()
    receipt = json.loads((base / 'capture.json').read_text())
    need(receipt['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and
         receipt['input_before_sha256'] == receipt['input_after_sha256'] and
         receipt['helper_before_sha256'] == receipt['helper_after_sha256'], 'completed immutable original capture')
    original_sources = {name: digest(P.HERE / name) for name in P.ORIGINAL_SOURCES}
    processing_sources = {name: digest(P.HERE / name) for name in P.PROCESSING_SOURCES}
    need(all(receipt['public_source_sha256'][name] == pin for name, pin in original_sources.items()),
         'unchanged actual original producers/inputs; current postprocessing identified separately')
    journals = []
    all_rows = []
    runs = []
    evidence = []
    searches = []
    exceptions = []
    for index in range(2):
        folder = base / ('debugger-' + str(index))
        records = [json.loads(line) for line in (folder / 'journal.jsonl').read_text(encoding='utf-8').splitlines()]
        calls = [row for row in records if row['event'] == 'call']
        need([row['case'] for row in calls] == list(range(28)), 'all whole JDI calls')
        need(all(row['counters'].get('root_calls') == 1 and row['counters'].get('finder_constructors') == 1
                 for row in calls), 'actual root and one reusable finder')
        pins = json.loads((P.HERE / 'source-pins.json').read_text())['classes']
        for code in (row for row in records if row['event'] == 'code'):
            method = next(method for method in pins[code['class']]['methods']
                          if method['name'] == code['method'] and method['descriptor'] == code['descriptor'])
            need(code['bytes'] == method['code_length'] and code['sha256'] == method['code_sha256'],
                 'actual executing unchanged method Code')
        journals.append(calls)
        raw = raw_rows(folder / 'target.stdout')
        need(len(raw) == 28, 'whole debugger root output')
        rows = [project(before, journal) for before, journal in zip(raw, calls)]
        all_rows.append(rows)
        evidence.append([{'case': row['case'], 'constructors': [
            {'class': constructor['class'], 'id': constructor['entity']['fields']['sn.aD'],
             'random': constructor['entity']['random'], 'math_before': constructor['math_before'],
             'math_after': constructor['math_after'], 'entropy': constructor['entropy']}
            for constructor in row['constructors']]} for row in calls])
        searches.append([{'case': row['case'], 'searches': [finder(value, search + 1)
                          for search, value in enumerate(row['searches'])],
                          'paths': [value['path_points'] for value in row['searches']]} for row in calls])
        stderr = (folder / 'target.stderr').read_text(encoding='utf-8')
        exceptions.append([{'case': row['case'], 'frames': raw[row['case']]['exception_frames'],
                            'events': [event['value'] for event in row['events'] if event['value']['kind'] == 'EXCEPTION'],
                            'stderr': stderr.split('NIGHTMARE_STDERR_BEGIN ' + str(row['case']) + '\n', 1)[1].split(
                                'NIGHTMARE_STDERR_END ' + str(row['case']) + '\n', 1)[0]}
                           for row in calls])
    baseline = [deterministic(row) for row in all_rows[0]]
    need(P.exact(baseline, [deterministic(row) for row in all_rows[1]]), 'independent debugger selected readbacks agree')
    for kind in ('ordinary', 'debugger'):
        for index in range(2):
            path = base / ('ordinary-' + str(index) + '.stdout.txt') if kind == 'ordinary' else base / ('debugger-' + str(index)) / 'target.stdout'
            if kind == 'ordinary' and not path.exists():
                path = base / receipt['ordinary'][index]['stdout_file']
            raw = raw_rows(path)
            rows = [project(before, journal) for before, journal in zip(raw, journals[index])]
            need(len(rows) == 28 and P.exact(baseline, [deterministic(row) for row in rows]),
                 'ordinary and debugger complete selected readbacks agree')
            runs.append({'kind': kind, 'calls': len(rows), 'returned': sum(row['returned'] for row in rows),
                         'shared_scalars': P.scalars(baseline), 'stdout': artifact(path, base),
                         **({'journal': artifact(path.parent / 'journal.jsonl', base)} if kind == 'debugger' else {})})
    finite = {'schema': 'NIGHTMARE_FINITE_V1', 'scope': P.SCOPE, 'rows': all_rows[0],
              'constructor_evidence': evidence, 'search_evidence': searches, 'exception_evidence': exceptions}
    (P.HERE / 'expected.json').write_text(json.dumps(finite, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
    provenance = {'schema': 'NIGHTMARE_PROVENANCE_V1', 'scope': P.SCOPE, 'archive_sha256': P.JAR,
                  'original_sources': original_sources, 'processing_sources': processing_sources,
                  'expected_sha256': digest(P.HERE / 'expected.json'),
                  'capture': artifact(base / 'capture.json', base), 'runs': runs, 'summary': P.summary(baseline)}
    (P.HERE / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8', newline='\n')
    shutil.copyfile(base / 'capture.json', P.HERE / 'capture-receipt.json')
    P.read()
    print(json.dumps(provenance['summary'], sort_keys=True))


if __name__ == '__main__':
    main()
