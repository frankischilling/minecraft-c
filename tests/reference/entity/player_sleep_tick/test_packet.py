"""Changed-copy controls for the actual portable reader; no native execution."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def module(path):
    spec = importlib.util.spec_from_file_location('sleep_parent_packet_controls', path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    packet = module(HERE / 'packet.py')
    original = packet.read_reference()
    proof = packet.load(HERE / 'provenance.json')
    rejected = 0
    with tempfile.TemporaryDirectory(prefix='sleep-parent-reader-') as temporary:
        target = Path(temporary) / 'portable'
        shutil.copytree(HERE, target)
        # This positive copied reader cannot reach an archive, VM or raw tree.
        if not packet.exact(module(target / 'packet.py').read_reference(), original):
            raise ValueError('portable original positive differs')

        def reject(name, transform):
            nonlocal rejected
            path = target / name
            initial = path.read_bytes()
            try:
                changed = transform(initial)
                path.write_bytes(changed)
                try:
                    module(target / 'packet.py').read_reference()
                except (ValueError, KeyError, TypeError, IndexError, OSError, SyntaxError):
                    rejected += 1
                else:
                    raise AssertionError('changed copy accepted: ' + name)
            finally:
                path.write_bytes(initial)

        def proof_change(change):
            def mutate(_):
                value = copy.deepcopy(proof)
                change(value)
                return (json.dumps(value, indent=2) + '\n').encode()
            return mutate

        for name in proof:
            reject('provenance.json', proof_change(lambda v, n=name: v.pop(n)))
        reject('provenance.json', proof_change(lambda v: v.update(extra=True)))
        reject('provenance.json', lambda b: b.replace(b'{', b'{"schema":"duplicate",', 1))
        for group in ('executed_producer_sha256', 'executed_helper_sha256',
                      'execution_binary_input_sha256', 'data_sha256'):
            for key in proof[group]:
                reject('provenance.json', proof_change(
                    lambda v, g=group, k=key: v[g].__setitem__(k, '0' * 64)))
            reject('provenance.json', proof_change(
                lambda v, g=group: v[g].pop(next(iter(v[g])))))
            reject('provenance.json', proof_change(
                lambda v, g=group: v[g].__setitem__('invented-member', '0' * 64)))
        reject('provenance.json', proof_change(lambda v:
            v['executed_producer_sha256'].__setitem__('producer/capture_reference.py',
                v['current_reproduction_sha256']['capture_reference.py'])))
        for index in range(39):
            reject('provenance.json', proof_change(lambda v, i=index:
                v['live_code_roles'][i].__setitem__('role', 'invented-executed-continuation')))
        for number in range(36):
            for phase in ('before', 'after'):
                def finite_change(_, i=number, p=phase):
                    value=copy.deepcopy(original)
                    value['rows'][i][p]['timer'] += 1
                    return (json.dumps(value, indent=2) + '\n').encode()
                reject('finite.json', finite_change)
        for name in proof['data_sha256']:
            reject(name, lambda b: b + b' ')
        for name in proof['current_reproduction_sha256']:
            if name != 'packet.py':
                reject(name, lambda b: b + b' ')
        reject('finite.json', lambda b: b.replace(b'{', b'{"schema":"duplicate",', 1))
        writer=module(HERE/'capture_reference.py')
        authored_root=Path(temporary)/'workspace'
        (authored_root/'.local').mkdir(parents=True)
        (authored_root/'tests').mkdir()
        for forbidden in (authored_root,authored_root/'.local',authored_root/'tests'/'capture',
                          Path(temporary)/'outside'):
            try:writer.private_output(forbidden,authored_root)
            except ValueError:rejected+=1
            else:raise AssertionError('public/nonfresh capture output admitted')
        if writer.private_output(authored_root/'.local'/'fresh',authored_root)!=\
                (authored_root/'.local'/'fresh').resolve():
            raise AssertionError('fresh private output positive rejected')
    print('Player sleep-parent portable reader:', rejected, 'changed-copy rejections')


if __name__ == '__main__':
    main()
