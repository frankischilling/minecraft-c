"""Strict finite component rows and independent read-only source trace checks."""
import json
from collections import Counter


def require(value,message):
    if not value:
        raise ValueError(message)


def rows(text,prefix):
    return [json.loads(line[len(prefix)+1:]) for line in text.splitlines() if line.startswith(prefix+' ')]


def components(text):
    found=rows(text,'REDSTONE_PARTICLE_COMPONENT')
    require([r['profile'] for r in found]==list(range(6,36)),'30 ordered deterministic component rows')
    require(Counter(r['operation'] for r in found)=={'tick':12,'draw':10,'draw_null_nw':2,'brightness':6},'declared component operation coverage')
    require(Counter(r['exception'] for r in found)=={'none':24,'NullPointerException':5,'ArithmeticException':1},'actual component outcomes')
    for row in found:
        if row['nw'] is not None:
            count=0 if row['exception']!='none' else 48
            require(row['nw']['words']==count and row['nw']['vertices']==count//8 and row['nw']['source_vertices']==count//12,'real NW quad-to-triangle append sizes')
            require(len(row['packed'])==100,'active words and actual retained tail projection')
        else:
            require(row['packed']==[],'non-draw component has no fabricated NW evidence')
    require('REDSTONE_PARTICLE_GUARD source network=0 native=0 write=0 exec=0' in text.splitlines(),'actual source guards')
    marker='REDSTONE_PARTICLE_COMPLETE constructors=37 measured=36 components=30 own-unchanged-words=25165440 network-controls=2 native-controls=1 write-controls=1 exec-controls=1'
    require(text.splitlines()[-1]==marker,'complete full own-prior backing audit and five denial controls')
    return found


def compare(actual,expected):
    require(json.dumps(actual,sort_keys=True)==json.dumps(expected,sort_keys=True),'exact typed deterministic component state, brightness and all packed word rows')


