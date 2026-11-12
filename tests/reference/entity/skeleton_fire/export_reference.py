"""Derive the public source packet from the same four sealed FR captures."""
import argparse
import importlib.util
import json
from pathlib import Path

import leaf_ledger as L
import observation as O
import packet as P


def text(path):
    return Path(path).read_bytes().decode('utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args(); base = args.capture.resolve()
    P.need(P.digest(base / 'capture.json') == P.CAPTURE and P.digest(base / 'postflight.json') == P.POSTFLIGHT,
           'only the exact reviewed first four-VM source capture is accepted')
    capture = P.load(base / 'capture.json'); P.validate_capture(capture)
    postflight = P.load(base / 'postflight.json'); P.validate_postflight(postflight)
    inputs = {base / 'capture.json', base / 'postflight.json', P.HERE / 'native-boundaries.json',
              *(P.HERE / p for p in P.PROCESSING), *(P.HERE / p for p in P.CAPTURED.values()),
              *(P.ROOT / p for p in P.SUPPORT.values())}
    for i in range(2):
        inputs.update(base / capture['ordinary'][i][k + '_file'] for k in ('stdout', 'stderr'))
        folder = base / ('debugger-' + str(i))
        inputs.update(folder / name for name in ('target.stdout', 'target.stderr', 'target.capture.json', 'journal.jsonl'))
    inputs.update(base / 'classes' / name for name in capture['helper_before_sha256'])
    before = {str(path): P.digest(path) for path in sorted(inputs)}
    spec = importlib.util.spec_from_file_location('skeleton_fire_captured_classfile', P.ROOT / P.SUPPORT['classfile.py'])
    classfile = importlib.util.module_from_spec(spec); spec.loader.exec_module(classfile)
    helper_classes = {}
    for name, pin in capture['helper_before_sha256'].items():
        path = base / 'classes' / name
        P.need(P.digest(path) == pin, 'same actual compiled capture helper ' + name)
        value = classfile.parse_class(path.read_bytes()); P.metadata_class(value)
        P.need(value['class_sha256'] == pin, 'compiled helper metadata identity')
        helper_classes[value['name']] = value
    helpers = {'schema': 'SKELETON_FIRE_CAPTURED_HELPER_METADATA_V1', 'classes': helper_classes}
    P.helper_metadata(helpers, capture)
    streams = []
    for i in range(2):
        streams.append({name: text(base / capture['ordinary'][i][name + '_file']) for name in ('stdout', 'stderr')})
    for i in range(2):
        folder = base / ('debugger-' + str(i))
        target = P.load(folder / 'target.capture.json')
        P.need(P.exact(target, postflight['runs'][i + 2]['target_receipt']), 'same complete/reaped source target receipt')
        streams.append({name: text(folder / ('target.' + name)) for name in ('stdout', 'stderr')})
    journals = [text(base / ('debugger-' + str(i)) / 'journal.jsonl') for i in range(2)]
    pins = P.source_pins()
    raw, traces, receipts, codes, authored, common, children = P.source_domains(streams, journals, capture, postflight, pins, helpers)
    counts = {'VMs': 4, 'calls': sum(len(rows) for rows in raw), 'normal_returns': sum(row['returned'] for rows in raw for row in rows),
              'source_exceptions': sum(not row['returned'] for rows in raw for row in rows), 'ordinary_common_scalars': O.scalars(common),
              'additional_passive_child_scalars': O.scalars(children), 'source_events_each': [sum(len(t['events']) for t in calls) for calls in traces],
              'original_Code_each': [len(v) for v in codes], 'authored_helper_Code_each': [len(v) for v in authored]}
    finite = {'schema': 'SKELETON_FIRE_SOURCE_FINITE_V1', 'scope': P.SCOPE, 'source_streams': streams, 'source_journals': journals,
              'ordinary_common': common, 'passive_children': children, 'source_receipts': receipts, 'source_code_runs': codes,
              'helper_code_runs': authored, 'raw_leaf_ledgers': [L.summary(rows) for rows in raw], 'counts': counts}
    after = {str(path): P.digest(path) for path in sorted(inputs)}
    P.need(before == after, 'all retained capture/supplier/current-processing inputs stay unchanged through source revalidation')
    processing = {'schema': 'SKELETON_FIRE_PROCESSING_ONLY_REVALIDATION_V1', 'source_capture_sha256': P.CAPTURE,
                  'source_postflight_sha256': P.POSTFLIGHT, 'derivation': P.processing_derivation(),
                  'input_before_sha256': before, 'input_after_sha256': after, 'JVM_started': False, 'native_started': False,
                  'source_supplier_edits': False, 'source_retries': 0, 'native_boundary_source_review': P.load(P.HERE / 'native-boundaries.json'),
                  'processing_history': {'first_proposed_projection': 'Rejected actual unposed auxiliary-Wolf yaw disagreement',
                      'retained_failure_report_sha256': '9ff37aade8209cfce60afe973f5824de5f550a5bf392ac26ebb6d54951297f88',
                      'correction': 'Only unposed auxiliary-Wolf current yaw is excluded across processes; raw values and full within-run equality remain; posed rows6/8 retain every angle',
                      'source_dependency': 'Complete LS constructor Math.random196 through SN.aS store208; fixture posing applies only when Wolf is the actual target'}}
    # Outputs contain authored helpers, declarations, hashes and observations;
    # no original class bytes, Code bytes, assets or compiled helpers are copied.
    for name, value in (('helper-pins.json', helpers), ('finite.json', finite), ('processing-receipt.json', processing)):
        (P.HERE / name).write_text(json.dumps(value, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
    provenance = {'schema': 'SKELETON_FIRE_SOURCE_PROVENANCE_V1', 'scope': P.SCOPE, 'archive_sha256': P.JAR,
                  'capture_sha256': P.CAPTURE, 'source_postflight_sha256': P.POSTFLIGHT,
                  'captured_sources': {name: {'path': path, 'sha256': P.digest(P.HERE / path)} for name, path in P.CAPTURED.items()},
                  'current_processing': {name: P.digest(P.HERE / name) for name in P.PROCESSING},
                  'finite_sha256': P.digest(P.HERE / 'finite.json'), 'helper_metadata_sha256': P.digest(P.HERE / 'helper-pins.json'),
                  'processing_receipt_sha256': P.digest(P.HERE / 'processing-receipt.json'), 'runs': postflight['runs']}
    (P.HERE / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8', newline='\n')
    P.read()
    print('Original Skeleton firing source packet: ' + str(counts['ordinary_common_scalars']) + ' ordinary-common scalars; ' +
          str(counts['additional_passive_child_scalars']) + ' additional passive-child scalars; same96 original calls; no native execution')


if __name__ == '__main__':
    main()
