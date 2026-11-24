"""Portable packet admission and corruption controls; no runtime is launched."""
import sys
sys.dont_write_bytecode = True
import copy
import json
from pathlib import Path
import shutil
import tempfile

import packet as P

def reject(function, label):
    try:
        function()
    except (ValueError, KeyError, TypeError, IndexError, UnicodeError):
        return label
    raise RuntimeError('corruption was accepted: ' + label)

def main():
    original = P.read_packet()
    data, target = original['expected'], original['ordinary']
    rejected = []
    def semantic(label, change):
        altered = copy.deepcopy(data)
        change(altered)
        rejected.append(reject(lambda: P.validate_parts(altered, target), label))
    semantic('missing case', lambda d: d['cases.json']['cases'].pop())
    semantic('wrong orientation', lambda d: d['cases.json']['cases'][0].__setitem__('metadata', 3))
    semantic('split surrogate replaced', lambda d: d['cases.json']['cases'][28]['loaded_units'].__setitem__(1, '0061' * 14 + 'fffd'))
    semantic('private path', lambda d: d['provenance.json'].__setitem__('private', 'C:/Users/private/example'))
    semantic('encoded private path', lambda d: d['provenance.json'].__setitem__('units', ['0043', '003a', '005c', '0055']))
    semantic('native payload credit invented', lambda d: d['provenance.json']['scope'].__setitem__('native_active_payload_trace', True))
    semantic('private JNI exclusion removed', lambda d: d['provenance.json']['scope']['excluded'].remove('private JNI entry'))
    semantic('extra mode', lambda d: d['provenance.json']['modes'].append('unrun-mode'))
    semantic('pipe loss', lambda d: d['provenance.json']['streams']['ordinary-default']['transport'].__setitem__('pipe_capture_complete', False))
    semantic('Boolean exit', lambda d: d['provenance.json']['streams']['ordinary-default']['transport'].__setitem__('exit', False))
    semantic('missing native receipt', lambda d: d['provenance.json']['streams']['jdi-default']['target']['buffer_evidence'].pop())
    semantic('wrong typed PO Code', lambda d: next(m for m in d['source-pins.json']['live_methods'] if m['class'] == 'po' and m['method'] == 'a').__setitem__('sha256', '0' * 64))
    semantic('duplicate live method', lambda d: d['source-pins.json']['live_methods'].__setitem__(0, d['source-pins.json']['live_methods'][1]))
    semantic('missing class', lambda d: d['source-pins.json']['classes'].pop('yk'))
    semantic('missing trace', lambda d: d['traces.json']['traces'].pop())
    semantic('bad trace reference', lambda d: d['draws.json']['draws'][0]['entry']['source_frames'].__setitem__('trace', 999))
    semantic('missing active batch', lambda d: d['batches.json']['batches'].pop())
    semantic('short active prefix', lambda d: d['batches.json']['batches'][0]['words'].pop())
    semantic('stale suffix as active', lambda d: d['batches.json']['batches'][0]['words'].append('00000000'))
    semantic('bad active word', lambda d: d['batches.json']['batches'][0]['words'].__setitem__(0, 'bad'))
    semantic('wrong quad-converted count', lambda d: d['batches.json']['batches'][0]['state'].__setitem__('nw.h', 4))
    semantic('invented GL triangles', lambda d: d['batches.json']['batches'][0]['draw_arrays']['arguments'].__setitem__(2, 4))
    semantic('missing font list', lambda d: d['font.json']['definitions'].pop())
    semantic('reused font list id', lambda d: d['font.json']['definitions'][1].__setitem__('id', 1))
    semantic('unassigned actual batch', lambda d: d['font.json']['definitions'][0]['batches'].clear())
    semantic('invented allocation', lambda d: d['font.json']['list_allocations'][0].__setitem__('base', 999))
    semantic('unexpected definition field', lambda d: d['font.json']['definitions'][0].__setitem__('hidden', 1))
    semantic('extra draw field', lambda d: d['draws.json']['draws'][0].__setitem__('hidden', 1))
    semantic('wall pole visible', lambda d: d['draws.json']['draws'][0]['model']['parts']['b']['state'].__setitem__('ps.h', True))
    semantic('missing actual Font row', lambda d: d['draws.json']['draws'][0]['rows'].pop())
    semantic('centering changed', lambda d: d['draws.json']['draws'][24]['rows'][3]['draw']['arguments'].__setitem__(1, -10))
    semantic('measure changed', lambda d: d['draws.json']['draws'][24]['rows'][3].__setitem__('width', 20))
    semantic('argument surrogate loss', lambda d: d['draws.json']['draws'][28]['rows'][1]['measure']['arguments'][0]['utf16_units'].__setitem__(-1, 'fffd'))
    semantic('raw backing loss', lambda d: d['draws.json']['draws'][28]['rows'][1]['string_storage'].__setitem__('bytes_hex', ''))
    semantic('unobserved endian fallback', lambda d: d['draws.json']['draws'][0]['rows'][0]['string_storage'].__setitem__('hi_byte_shift', 8))
    semantic('payload truncated', lambda d: d['draws.json']['draws'][24]['rows'][0]['payload'].__setitem__('active_bytes_hex', ''))
    semantic('payload word altered', lambda d: d['draws.json']['draws'][24]['rows'][0]['payload']['words'].__setitem__(0, 'ffffffff'))
    semantic('nonproducer display-list id', lambda d: d['draws.json']['draws'][24]['rows'][0]['payload'].__setitem__('active_bytes_hex', 'ffffffff'))
    semantic('native handle left open', lambda d: d['draws.json']['draws'][0]['rows'][0]['payload'].__setitem__('native_handle_closed', False))
    semantic('extra payload field', lambda d: d['draws.json']['draws'][0]['rows'][0]['payload'].__setitem__('address', 'private'))
    semantic('terminal call returned', lambda d: d['draws.json']['draws'][-1].__setitem__('returned', True))
    semantic('terminal Source PC repaired', lambda d: d['draws.json']['draws'][-1]['failures'][0]['site'].__setitem__('pc', 387))
    semantic('terminal row fabricated', lambda d: d['draws.json']['draws'][-1]['rows'].append(copy.deepcopy(d['draws.json']['draws'][0]['rows'][0])))
    rejected.append(reject(lambda: P.P.parse(b'{"a":1,"a":2}'), 'duplicate JSON'))
    rejected.append(reject(lambda: P.P.parse(b'{"a":NaN}'), 'nonfinite JSON'))
    raw = (P.HERE / 'ordinary.stdout').read_bytes()
    rejected.append(reject(lambda: P.P.ordinary(raw[:-2]), 'truncated raw stdout'))
    rejected.append(reject(lambda: P.P.ordinary(raw.replace(b'\r\n', b'\n')), 'normalized original stdout'))
    with tempfile.TemporaryDirectory(prefix='sign-packet-controls-') as temporary:
        for index, name in enumerate(sorted(P.ROOTS)):
            root = Path(temporary) / str(index)
            shutil.copytree(P.HERE, root)
            (root / name).write_bytes((root / name).read_bytes() + b' ')
            P.P.write(root / 'publication.json', {'schema': 1, 'files': {p.name: P.P.pin(p) for p in sorted(root.iterdir()) if p.name != 'publication.json'}})
            rejected.append(reject(lambda r=root: P.read_packet(r), 'repaired manifest cannot change ' + name))
        root = Path(temporary) / 'extra'; shutil.copytree(P.HERE, root)
        (root / 'original.class').write_bytes(b'excluded binary')
        rejected.append(reject(lambda: P.read_packet(root), 'extra binary file'))
    for path in P.HERE.iterdir():
        if path.name not in ('ordinary.stdout',):
            P.P.need(b'\r' not in path.read_bytes(), 'all generated public text is LF')
    result = dict(original['summary'], corruption_controls=len(rejected), rejected=rejected)
    print(json.dumps(result, sort_keys=True))

if __name__ == '__main__':
    main()
