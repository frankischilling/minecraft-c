"""Optimized-safe corruption controls for the private original-only packet."""
import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
sys.dont_write_bytecode=True
from validate import enum_state,event,exact,load,loads,require,validate_data
from packet import read_reference
HERE=Path(__file__).resolve().parent
expected=read_reference(); actual=load(HERE/'finite.json');validate_data(actual,expected)
checks=0
def rejected(action,label):
    global checks
    try:action()
    except (ValueError,KeyError,TypeError,IndexError):checks+=1;return
    raise ValueError('corruption accepted: '+label)
def mutate(path,value,label):
    v=copy.deepcopy(actual);at=v
    for key in path[:-1]:at=at[key]
    at[path[-1]]=value;rejected(lambda:validate_data(v,expected),label)
for text in ('{"a":1,"a":1}','{"a":{"z":0,"z":1}}','{"x":NaN}','{"x":Infinity}','{"x":-Infinity}'):
    rejected(lambda text=text:loads(text),'duplicate/non-JSON constant')
mutate(['schema'],'PLAYER_SLEEP_FINITE_V2','schema')
mutate(['rows'],actual['rows'][:-1],'missing row')
mutate(['rows'],actual['rows']+[actual['rows'][-1]],'extra row')
mutate(['rows'],list(reversed(actual['rows'])),'reordered rows')
for i,r in enumerate(actual['rows']):
    mutate(['rows',i,'case'],True,'bool case '+str(i))
    mutate(['rows',i,'name'],r['name']+'x','profile name '+str(i))
    mutate(['rows',i,'input',9],'0000000000000001','raw input '+str(i))
    mutate(['rows',i,'outcome'],'RETURN' if r['outcome']!='RETURN' else 'FRONTIER119','outcome '+str(i))
    mutate(['rows',i,'site','pc'],True,'bool source pc '+str(i))
    mutate(['rows',i,'preserved'],1,'bool preservation '+str(i))
    mutate(['rows',i,'cw_before','initialized'],True,'eager enum '+str(i))
    mutate(['rows',i,'cw_after','initialized'],not r['cw_after']['initialized'],'enum phase '+str(i))
    mutate(['rows',i,'journal'],r['journal'][1:],'missing actual entry '+str(i))
    bad=copy.deepcopy(r);bad['unadvertised']=0;mutate(['rows',i],bad,'unknown row key '+str(i))
    if r['result'] is not None:
        mutate(['rows',i,'result','ordinal'],True,'bool ordinal '+str(i))
        mutate(['rows',i,'result','static_field'],'a','wrong static result '+str(i))
        mutate(['rows',i,'result','name_utf16',0],True,'bool name unit '+str(i))
    else:mutate(['rows',i,'result'],{'static_field':'a','name_utf16':[79,75],'ordinal':0},'invented completed value '+str(i))
for i,c in enumerate(actual['live_code']):
    mutate(['live_code',i,'sha256'],'0'*64,'Code hash '+str(i))
    mutate(['live_code',i,'bytes'],True,'Code length bool '+str(i))
    mutate(['live_code',i,'method'],'not-original','Code name '+str(i))
mutate(['original_completion_only'],[],'discard original-only completions')
for i,r in enumerate(actual['original_completion_only']):
    mutate(['original_completion_only',i,'scope'],'NATIVE_PARITY','broaden night scope')
    mutate(['original_completion_only',i,'result','ordinal'],2,'night result')
# Exercise semantic readers independently of the canonical-packet equality gate.
cold=actual['rows'][0]['cw_before']; warm=next(r['cw_after'] for r in actual['rows'] if r['outcome']=='RETURN')
for obj,path,value in ((cold,['initialized'],1),(cold,['statics',0],{'name':'OK','ordinal':False}),
                       (warm,['values_same',0],False),(warm,['identity_matrix',0,1],True),
                       (warm,['statics',0,'ordinal'],False),(warm,['statics',0,'name'],'wrong')):
    v=copy.deepcopy(obj);at=v
    for k in path[:-1]:at=at[k]
    at[path[-1]]=value;rejected(lambda v=v:enum_state(v),'semantic enum shape/value')
all_events=[e for r in actual['rows'] for e in r['journal']]
for kind in ('ENTER','RETURN','FIELD_READ','STATIC_WRITE','FRONTIER119','EXCEPTION'):
    e=copy.deepcopy(next(e for e in all_events if e['kind']==kind));e['site']['pc']=True
    rejected(lambda e=e:event(e),'semantic bool PC '+kind)
    e=copy.deepcopy(next(e for e in all_events if e['kind']==kind));e['extra']=None
    rejected(lambda e=e:event(e),'semantic unknown event key '+kind)
require(not exact(True,1) and not exact([False],[0]) and not exact({'a':True},{'a':1}),'deep strict types')
with tempfile.TemporaryDirectory(prefix='sleep-history-') as temporary:
    directory=Path(temporary)/'packet'
    shutil.copytree(HERE,directory,ignore=shutil.ignore_patterns('__pycache__'))
    path=directory/'provenance.json'; original=load(path)
    for kind in ('current-unexecuted-writer','other-historical-producer','historical-helper','jdk-binary','sdk-binary','binary-inventory'):
        value=copy.deepcopy(original)
        if kind=='current-unexecuted-writer':
            require(value['executed_producer_sha256']['capture_reference.py']!=value['current_reproduction_sha256']['capture_reference.py'],
                    'replacement must be a different unexecuted writer')
            value['executed_producer_sha256']['capture_reference.py']=value['current_reproduction_sha256']['capture_reference.py']
        elif kind=='other-historical-producer':value['executed_producer_sha256']['prepare.py']='0'*64
        elif kind=='historical-helper':value['helper_sha256']['PlayerSleepJdi.class']='0'*64
        elif kind=='jdk-binary':value['binary_input_sha256']['jdk/bin/java.exe']='0'*64
        elif kind=='sdk-binary':value['binary_input_sha256']['sdk/org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar']='0'*64
        else:value['binary_input_sha256']['jdk/bin/not-captured.dll']=value['binary_input_sha256'].pop('jdk/bin/zip.dll')
        path.write_text(json.dumps(value),encoding='utf-8')
        rejected(lambda:read_reference(directory),'immutable historical identity '+kind)
    path.write_text(json.dumps(original),encoding='utf-8')
    require(exact(read_reference(directory),actual),'historical controls preserve positive packet')
print('Player sleep packet controls:',checks,'rejections; optimized='+str(not __debug__))
