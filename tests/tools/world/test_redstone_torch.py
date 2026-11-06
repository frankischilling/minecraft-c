"""Four retained native cohorts with explicit bounded Redstone Torch scope."""
import argparse
import importlib.util
import os
from pathlib import Path
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


D = load('db203_current', HERE / 'redstone_torch_current.py')
A = load('db203_accounting', HERE / 'redstone_torch_accounting.py')
C = load('db203_capture', ROOT / 'tests/reference/client/controller/capture.py')
N = load('db203_products', ROOT / 'tests/tools/client/native_product_capture.py')
R = load('db203_resources', ROOT / 'tests/reference/client/player_statistics/support.py')
STATUS = 'PASS_BOUNDED_OBSERVED_STREAMS_WITH_UNOBSERVED_ENTITY_FRONTIER'
HISTORY = {
    'original_Source_capture': {'status': 'FAIL_UNCHANGED',
        'report_sha256': '9560ec76ba4c8ec5e9f859db8650c85b6a3c21019e51f8db14ff857f78ba6f82'},
    'original_native_capture': {'status': 'FAIL_UNCHANGED',
        'report_sha256': 'db3671e71e31c0b512b5f460eca3753487a6b1eaf3e60d07005b5074719a5130'},
    'later_accounting': STATUS,
    'fresh_test_is_not_a_receipt_rebinding': True,
}


def selected_inputs(executable):
    paths = {executable}
    for directory in (ROOT / 'src', ROOT / 'include'):
        paths.update(p for p in directory.rglob('*') if p.is_file() and p.suffix in ('.c', '.h', '.inc'))
    paths.update(p for p in (ROOT / 'tests/world/runtime/redstone_torch').rglob('*') if p.is_file())
    paths.update(p for p in D.REFERENCE.rglob('*') if p.is_file())
    paths.update(HERE / name for name in ('test_redstone_torch.py', 'redstone_torch_current.py',
                                         'redstone_torch_accounting.py', 'test_redstone_torch_controller.py'))
    paths.update((ROOT / name for name in (
        'tests/tools/client/native_product_capture.py',
        'tests/reference/client/controller/capture.py',
        'tests/reference/client/player_statistics/support.py',
        'tests/reference/world/torch_placement/entropy.py')))
    return {str(p.resolve()): D.digest(p) for p in sorted(paths)}


def compare_saved_bytes(before, after):
    D.exact({k: v for k, v in before.items() if not k.endswith('/session.lock')},
            {k: v for k, v in after.items() if not k.endswith('/session.lock')},
            'saved bytes except ordinary session locks')


def constructor_joins(constructors, rows, own):
    for role, actual in enumerate(constructors):
        D.exact(actual['identity'], rows[0]['before']['worlds'][role]['identity'], 'actual constructor World identity')
        D.exact(actual['time'], rows[0]['before']['worlds'][role]['time'], 'actual saved time at first call')
        if role:
            D.exact(constructors[role - 1]['uniquifier_after'], actual['uniquifier_before'], 'successive World constructor counters')
            D.exact(constructors[role - 1]['math_after'], actual['math_before'], 'successive World constructor Math')
    D.exact(constructors[-1]['uniquifier_after'], own[0]['seed_uniquifier_before'], 'constructor to first call counter')
    D.exact(constructors[-1]['math_after'], own[0]['math_before'], 'constructor to first call Math')


def preserved_products(folder, item):
    receipt = item['products']
    D.exact(D.read(folder / receipt['manifest']), receipt, 'preserved native product manifest')
    D.need(receipt['status'] == 'PASS' and receipt['command_executable'] == item['command'][0], 'actual command executable preserved')
    for product in receipt['products']:
        artifact = folder / product['artifact']
        D.need(artifact.is_file(), 'preserved native product exists')
        digest, size = D.digest(artifact), artifact.stat().st_size
        D.need(all(product[k]['sha256'] == digest and product[k]['bytes'] == size
                   for k in ('before', 'copied', 'preserved', 'after')) and D.digest(Path(product['source'])) == digest,
               'native product before/copy/after identity')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    D.need(executable.is_file(), 'explicit native executable')
    packet = D.packet()
    source_admission = packet.audit()
    original = packet.reference_cohorts()
    declaration = D.inputs()
    D.need(set(original) == set(D.COHORTS), 'all four Source cohorts')
    producer = load('db203_world_inputs', D.REFERENCE / 'world_inputs.py')
    work = args.work_dir.resolve()
    protected = (ROOT / 'src', ROOT / 'include', ROOT / 'tests', ROOT / 'cmake')
    D.need(all(work != p and p not in work.parents and work not in p.parents for p in protected),
           'evidence directory must be disjoint from selected source/fixture inputs')
    work.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='redstone-torch-', dir=work))
    resources = output / 'resources'
    resources.mkdir()
    R.resources_at(resources)
    before = selected_inputs(executable)
    resources_before = D.tree(resources)
    report = {'status': 'FAIL', 'history': HISTORY, 'source_packet_admission': source_admission,
              'source_reference_rows': sum(len(v['rows']) for v in original.values()),
              'full_entropy_qualification': False, 'inputs_before': before,
              'resources_before': resources_before, 'runs': []}
    decoded = {}
    try:
        # Preserve all four prescribed process opportunities before comparison.
        for cohort in D.COHORTS:
            folder = output / cohort
            folder.mkdir()
            item = {'cohort': cohort}
            report['runs'].append(item)
            try:
                mutable = folder / 'worlds'
                producer.create(mutable)
                metadata = D.read(D.REFERENCE / 'world-inputs.json')
                for info in metadata.values():
                    D.exact(D.tree(mutable / info['name']), info['files'], 'full authored World input bytes')
                    D.exact(D.nbt_time(mutable / info['name'] / 'level.dat'), info['authored_nbt_time'], 'independently decoded saved Time')
                command = [str(executable), str(resources), str(mutable), str(D.REFERENCE / (cohort + '-profiles.tsv')), cohort]
                item.update(command=command, worlds_before=D.tree(mutable))
                item['products'] = N.capture_native_products(command, folder)
                item['capture'] = C.capture(command, folder, 'native', 90, os.environ.copy(), folder)
            except N.NativeProductCaptureError as error:
                item.update(product_failure=error.receipt, failure=str(error))
            except Exception as error:
                item['failure'] = type(error).__name__ + ': ' + str(error)
            finally:
                if (folder / 'worlds').is_dir():
                    item['worlds_after'] = D.tree(folder / 'worlds')
                D.write(output / 'run.json', report)
        for item in report['runs']:
            try:
                D.need('failure' not in item, item.get('failure', 'native process missing'))
                cohort = item['cohort']
                folder = output / cohort
                payload = {k: (folder / ('native.' + k)).read_bytes() for k in ('stdout', 'stderr')}
                D.receipt(item['capture'], payload, 'native', 90)
                D.exact(D.read(folder / 'native.capture.json'), item['capture'], 'actual bounded process receipt')
                D.exact(item['capture']['command'], item['command'], 'actual native argv')
                D.need(payload['stderr'] == b'', 'empty native stderr, including sanitizer diagnostics')
                constructors, rows, own, finish = D.parse_current(payload['stdout'], cohort, declaration['cohorts'][cohort])
                constructor_joins(constructors, rows, own)
                item['constructors'], item['finish'] = constructors, finish
                item['comparison'] = A.audit_native(cohort, rows, own, original[cohort])
                D.need(item['comparison']['status'] == STATUS and item['comparison']['full_entropy_qualification'] is False,
                       'exact bounded observable-state qualification')
                compare_saved_bytes(item['worlds_before'], item['worlds_after'])
                preserved_products(folder, item)
                decoded[cohort] = (rows, own)
                item['status'] = STATUS
            except Exception as error:
                item.update(status='FAIL', failure=type(error).__name__ + ': ' + str(error))
        D.need(len(report['runs']) == 4 and [r['cohort'] for r in report['runs']] == list(D.COHORTS), 'all four retained native opportunities')
        if all(item.get('status') == STATUS for item in report['runs']):
            report['corruption_controls'] = A.corruption_controls(*decoded['lifecycle'], original['lifecycle'])
            report.update(status=STATUS, native_rows=sum(x['comparison']['rows'] for x in report['runs']))
    finally:
        try:
            report['inputs_after'] = selected_inputs(executable)
            report['resources_after'] = D.tree(resources)
            report['inputs_unchanged'] = D.same(before, report['inputs_after'])
            report['resources_unchanged'] = D.same(resources_before, report['resources_after'])
            if not report['inputs_unchanged'] or not report['resources_unchanged']:
                report['status'] = 'FAIL'
        except Exception as error:
            report.update(status='FAIL', continuity_error=type(error).__name__ + ': ' + str(error))
        D.write(output / 'run.json', report)
    D.need(report['status'] == STATUS, 'native qualification failed; all artifacts retained at ' + str(output / 'run.json'))
    print(STATUS + ': 203 native rows; full_entropy_qualification=false; ' + str(output / 'run.json'))


if __name__ == '__main__':
    main()
