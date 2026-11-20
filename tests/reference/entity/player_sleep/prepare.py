"""Private authored inputs and unchanged original source/Class-Code pins."""
import importlib.util
import json
from pathlib import Path
import struct
import sys
import zipfile
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
spec = importlib.util.spec_from_file_location('sleep_classfile', ROOT / 'tests/reference/entity/player/classfile.py')
cf = importlib.util.module_from_spec(spec); spec.loader.exec_module(cf)
JAR = ROOT / 'b1.7.3.jar'
def word(x):
    return struct.pack('>d', x).hex() if not isinstance(x, str) else x
rows = []
def add(name, **kw):
    p = dict(world_null=0, remote=0, sleeping=0, dead=0, health=20, provider_null=0,
             hell=0, sky_darkness=5, x=0., y=0., z=0., target_x=0, target_y=0, target_z=0)
    p.update(kw)
    values = [len(rows), *[p[k] for k in ('world_null', 'remote', 'sleeping', 'dead', 'health',
              'provider_null', 'hell', 'sky_darkness')], *[word(p[k]) for k in ('x','y','z')],
              *[p[k] for k in ('target_x','target_y','target_z')]]
    rows.append(dict(name=name, values=[str(v) for v in values]))
add('null-world-before-sleeping', world_null=1, sleeping=1, dead=1, health=0)
add('sleeping-masks-dead-and-null-provider', sleeping=1, dead=1, health=0, provider_null=1)
add('dead-flag-positive-health', dead=1)
add('dead-flag-negative-health', dead=1, health=-1)
add('zero-health', health=0, provider_null=1)
add('negative-health', health=-2147483648, hell=1)
add('null-provider-alive', provider_null=1)
add('hell-masks-day-and-nan', hell=1, sky_darkness=3, x='7ff8000000001234')
add('day-masks-all-nan', sky_darkness=3, x='7ff8000000001234', y='fff8000000005678', z='7ff8000000009abc')
add('minimum-darkness-day-distant', sky_darkness=-2147483648, x=float('inf'))
add('night-x-positive-outside', sky_darkness=4, x='4008000000000001')
add('night-x-negative-outside', x='c008000000000001')
add('night-x-positive-infinity', x=float('inf'), y='7ff8000000001234')
add('night-x-negative-infinity', x=-float('inf'))
add('night-x-nan', x='7ff8000000001234')
add('night-x-boundary-y-positive-outside', x=3., y='4000000000000001')
add('night-x-negative-boundary-y-negative-outside', x=-3., y='c000000000000001')
add('night-y-positive-infinity', y=float('inf'), z='7ff8000000001234')
add('night-y-nan', y='fff8000000005678')
add('night-y-boundary-z-positive-outside', y=2., z='4008000000000001')
add('night-y-negative-boundary-z-negative-outside', y=-2., z='c008000000000001')
add('night-z-positive-infinity', z=float('inf'))
add('night-z-nan', z='7ff8000000009abc')
add('night-extreme-int-distance', x=2147483647., target_x=-2147483648)
add('maximum-darkness-night', sky_darkness=2147483647, x=4.)
add('frontier-all-positive-boundaries', x=3., y=2., z=3.)
add('frontier-all-negative-boundaries', x=-3., y=-2., z=-3.)
add('frontier-zero', x=0., y=0., z=0.)
add('frontier-remote-masks-all-gates', remote=1, sleeping=1, dead=1, health=0,
    provider_null=1, x='7ff8000000001234', y=float('inf'), z=-float('inf'))
add('frontier-remote-null-provider', remote=1, provider_null=1)
add('sleeping-positive-health-day', sleeping=1, sky_darkness=3)
add('hell-awake-normal', hell=1)
add('darkness-four-nan', sky_darkness=4, x='fff8000000000001')
add('day-normal', sky_darkness=3)
add('frontier-remote-zero-health', remote=1, health=0)
add('null-world-awake', world_null=1)
add('sleeping-null-provider-only', sleeping=1, provider_null=1)
add('zero-health-day-before-provider', health=0, sky_darkness=3)
add('night-x-boundary-y-nan-z-infinite', x=3., y='7ff8000000001234', z=float('inf'))
add('frontier-inside-xy-z-boundary', x='4007ffffffffffff', y='3fffffffffffffff', z=3.)
add('night-inside-x-y-boundary-z-outside', x='4007ffffffffffff', y=2., z='4008000000000001')
add('day-extreme-positions-and-targets', sky_darkness=3, x=-float('inf'), y=float('inf'),
    z='7ff8000000001234', target_x=2147483647, target_y=-2147483648, target_z=2147483647)
add('frontier-negative-zero', x='8000000000000000', y='8000000000000000', z='8000000000000000')
add('night-extreme-y-boundary-z-outside', y=-2147483646., target_y=-2147483648, z=4.)
add('frontier-extreme-z-positive-boundary', z=2147483650., target_z=2147483647)
add('dead-short-circuits-health-read', dead=1, health=2147483647, provider_null=1)
add('positive-min-health-alive-day', health=1, sky_darkness=3)
add('negative-day-with-null-provider-fault', sky_darkness=-1, provider_null=1)
(HERE / 'profiles.json').write_text(json.dumps(rows, indent=2) + '\n', encoding='utf-8')
(HERE / 'profiles.tsv').write_text('\n'.join(' '.join(p['values']) for p in rows)+'\n', encoding='ascii')
selected = {'gs': [('b','(III)Lcw;'), ('N','()Z')], 'ls':[('W','()Z')], 'fd':[('f','()Z')],
            'cw':[('<init>','(Ljava/lang/String;I)V'),('<clinit>','()V')],
            'dc': [], 'sn': [('b','(FF)V')], 'xa': [], 'br': [], 'gr': [], 'wy': []}
pins = {'schema':'PLAYER_SLEEP_CLASS_CODE_V1', 'archive_sha256':cf.sha(JAR.read_bytes()), 'classes':{}}
with zipfile.ZipFile(JAR) as z:
    for n, methods in selected.items():
        c = cf.parse_class(z.read(n+'.class')); c.pop('fields')
        c['methods'] = [m for m in c['methods'] if (m['name'],m['descriptor']) in methods]
        if len(c['methods']) != len(methods): raise ValueError('pin method inventory '+n)
        pins['classes'][n] = c
(HERE/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8')
(HERE/'class-names.txt').write_text('\n'.join(pins['classes'])+'\n',encoding='ascii')
print('Prepared',len(rows),'profiles and',len(pins['classes']),'original class pins')
