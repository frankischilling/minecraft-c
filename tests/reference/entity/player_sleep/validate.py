"""Strict private packet reader; observations come only from the sealed VM runs."""
import hashlib
import json
from pathlib import Path
import re

HERE=Path(__file__).resolve().parent
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
NAMES=['OK','NOT_POSSIBLE_HERE','NOT_POSSIBLE_NOW','TOO_FAR_AWAY','OTHER_PROBLEM']
FIELDS=list('abcde')
def require(ok,message):
    if not ok:raise ValueError(message)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def unique(pairs):
    r={}
    for k,v in pairs:
        require(k not in r,'duplicate key '+k);r[k]=v
    return r
def invalid_constant(value):raise ValueError('non-JSON numeric constant '+value)
def loads(text):return json.loads(text,object_pairs_hook=unique,parse_constant=invalid_constant)
def load(path):return loads(Path(path).read_text(encoding='utf-8'))
def exact(a,b):
    if type(a) is not type(b):return False
    if type(a) is dict:return a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a)
    if type(a) is list:return len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b))
    return a==b
def integer(v,low=-(1<<31),high=(1<<31)-1):return type(v) is int and low<=v<=high
def keys(v,expected,message):require(type(v) is dict and set(v)==set(expected),message)
def profiles():
    p=load(HERE/'profiles.json'); lines=(HERE/'profiles.tsv').read_text(encoding='ascii').splitlines()
    require(type(p) is list and len(p)==len(lines)==48,'profile extent')
    seen=set()
    for i,(r,line) in enumerate(zip(p,lines)):
        keys(r,('name','values'),'profile schema');require(type(r['name']) is str and r['name'] not in seen,'unique profile name');seen.add(r['name'])
        v=r['values'];require(type(v) is list and len(v)==15 and all(type(x) is str for x in v) and v==line.split(' '),'exact profile TSV')
        require(v[0]==str(i),'profile case order')
        for k in list(range(9))+[12,13,14]:require(re.fullmatch('0|-?[1-9][0-9]*',v[k]) and integer(int(v[k])),'integer profile field')
        require(all(v[k] in ('0','1') for k in (1,2,3,4,6,7)),'boolean profile field')
        require(all(re.fullmatch('[0-9a-f]{16}',v[k]) for k in (9,10,11)),'raw double profile word')
    return p
def enum_value(v):
    keys(v,('name','ordinal'),'enum value schema');require(type(v['name']) is str and integer(v['ordinal'],0,4) and v['name']==NAMES[v['ordinal']],'enum name/ordinal')
def enum_state(v):
    keys(v,('initialized','statics','identity_matrix','values_same'),'CW state schema')
    require(type(v['initialized']) is bool and type(v['statics']) is list and len(v['statics'])==5,'CW state fields')
    matrix=v['identity_matrix'];require(type(matrix) is list and len(matrix)==5 and all(type(r) is list and len(r)==5 and all(type(x) is bool for x in r) for r in matrix),'CW identity matrix types')
    n=0
    for i,c in enumerate(v['statics']):
        if c is not None:enum_value(c);require(c['ordinal']==i,'CW source cell ordinal');n+=1
    require(v['statics']==[{'name':NAMES[i],'ordinal':i} for i in range(n)]+[None]*(5-n),'CW publication prefix')
    require(matrix==[[i==j or (i>=n and j>=n) for j in range(5)] for i in range(5)],'CW actual source identity relations')
    if v['values_same'] is not None:require(type(v['values_same']) is list and len(v['values_same'])==5 and all(x is True for x in v['values_same']) and n==5,'CW values current-cell identity')
    require(not v['initialized'] or (n==5 and v['values_same']==[True]*5),'CW completed initialized state')
def site(v):
    keys(v,('class','method','descriptor','pc'),'site schema');require(all(type(v[k]) is str for k in ('class','method','descriptor')) and integer(v['pc'],0,337),'site scalar types')
    require((v['class'],v['method'],v['descriptor']) in {('gs','b','(III)Lcw;'),('gs','N','()Z'),('ls','W','()Z'),('fd','f','()Z'),('cw','<init>','(Ljava/lang/String;I)V'),('cw','<clinit>','()V')},'selected source method')
def event(v):
    require(type(v) is dict and type(v.get('kind')) is str,'event object/type')
    k=v['kind']; shapes={'ENTER':('kind','site','arguments'),'RETURN':('kind','site','value'),
        'FIELD_READ':('kind','site','owner','field','value'),'STATIC_WRITE':('kind','site','field','before','value','statics_before'),
        'FRONTIER119':('kind','site','preserved','cw_initialized'),'EXCEPTION':('kind','site','class')}
    require(k in shapes,'selected event kind');keys(v,shapes[k],'event exact schema');site(v['site'])
    if k=='ENTER':require(type(v['arguments']) is list and len(v['arguments'])<=3,'entry argument extent')
    if k=='FIELD_READ':require(type(v['owner']) is str and type(v['field']) is str,'read field names')
    if k=='STATIC_WRITE':enum_state(v['statics_before']);require(v['before'] is None and v['field'] in 'abcdef','source static publication')
    if k=='FRONTIER119':require(v['site']=={'class':'gs','method':'b','descriptor':'(III)Lcw;','pc':119} and v['preserved'] is True and v['cw_initialized'] is False,'pre-resize frontier')
    if k=='EXCEPTION':require(v['class']=='java/lang/NullPointerException' and v['site']['class']=='gs' and v['site']['method']=='b' and v['site']['pc'] in (4,35),'actual reached null site')
def code_records(records):
    codes=[v for v in records if v.get('event')=='code'];pins=load(HERE/'source-pins.json')['classes'];seen=set()
    for c in codes:
        keys(c,('event','class','method','descriptor','bytes','sha256'),'live Code schema');key=(c['class'],c['method'],c['descriptor']);require(key not in seen,'unique live Code');seen.add(key)
        found=[m for m in pins[c['class']]['methods'] if (m['name'],m['descriptor'])==key[1:]];require(len(found)==1 and integer(c['bytes'],1) and c['bytes']==found[0]['code_length'] and c['sha256']==found[0]['code_sha256'],'actual live Code pin')
    require(seen=={('gs','b','(III)Lcw;'),('gs','N','()Z'),('ls','W','()Z'),('fd','f','()Z'),('cw','<init>','(Ljava/lang/String;I)V'),('cw','<clinit>','()V')},'complete live Code inventory')
    return codes
def original_enum(v,result):
    keys(v,('statics','identity_matrix','values_same','values_distinct'),'raw enum schema');require(type(v['statics']) is list and len(v['statics'])==5,'raw enum statics extent')
    require(exact(v['identity_matrix'],[[i==j for j in range(5)] for i in range(5)]) and exact(v['values_same'],[True]*5) and v['values_distinct'] is True,'actual enum relation types')
    selected=[]
    for i,c in enumerate(v['statics']):
        keys(c,('field','name_utf16','ordinal','result_same','name_interned'),'raw static schema')
        require(c['field']==FIELDS[i] and integer(c['ordinal'],0,4) and c['ordinal']==i and exact(c['name_utf16'],list(map(ord,NAMES[i]))) and type(c['result_same']) is bool and c['name_interned'] is True,'raw enum cell observation')
        if c['result_same']:selected.append(i)
    require(selected==[result['ordinal']],'source result/current-static identity')
def project(root,vm):
    ps=profiles();root=Path(root);folder=root/f'vm-{vm}'
    rawtext=(folder/'target.stdout').read_text(encoding='utf-8');lines=rawtext.splitlines()
    require(lines[-1]=='Original Player sleep: 48 profiles; network=0; native=0; rejected_writes=0','raw external guards/completion')
    raw=[loads(s[len('PLAYER_SLEEP '):]) for s in lines if s.startswith('PLAYER_SLEEP ')];require(len(raw)==48,'all raw rows')
    records=[loads(s) for s in (folder/'journal.jsonl').read_text().splitlines()];require(all(type(v) is dict and v.get('event') in ('code','journal') for v in records),'JDI record kind')
    code=code_records(records);js=[v for v in records if v['event']=='journal'];require(len(js)==48,'all JDI rows');rows=[];completed=[]
    for i,(r,j,p) in enumerate(zip(raw,js,ps)):
        keys(r,('schema','case','input','exception','result','before','after','unchanged','enum'),'raw row schema')
        require(r['schema']=='PLAYER_SLEEP_RAW_V1' and integer(r['case'],0,47) and r['case']==i and exact(r['input'],p['values']),'raw exact input/order')
        require(type(r['unchanged']) is bool and r['unchanged']==exact(r['before'],r['after']),'raw actual preservation predicate')
        require(r['exception'] in ('NONE','java/lang/NullPointerException'),'raw exception')
        if r['result'] is not None:enum_value(r['result']);require(r['exception']=='NONE','returned reference and fault exclusion');original_enum(r['enum'],r['result'])
        else:require(r['enum'] is None and r['exception']!='NONE','only null-source rows return no enum')
        keys(j,('event','case','cw_before','cw_prefix_after','preserved','frontier119','prefix','completion','completion_preserved','cw_completion_after'),'JDI row exact schema')
        require(integer(j['case'],0,47) and j['case']==i and j['preserved'] is True and type(j['frontier119']) is bool and type(j['completion_preserved']) is bool,'JDI scalar types/order/preservation')
        for k in ('cw_before','cw_prefix_after','cw_completion_after'):enum_state(j[k])
        require(j['cw_before']['initialized'] is False and j['cw_before']['statics']==[None]*5,'fresh actual CW cold namespace')
        for k in ('prefix','completion'):
            require(type(j[k]) is list and len(j[k])<=128,'bounded passive events')
            for q in j[k]:event(q)
        require(j['prefix'][0]=={'kind':'ENTER','site':{'class':'gs','method':'b','descriptor':'(III)Lcw;','pc':0},'arguments':list(map(int,p['values'][12:]))},'actual sleep entry/arguments')
        terminal=j['prefix'][-1]
        if j['frontier119']:
            require(terminal['kind']=='FRONTIER119' and j['cw_prefix_after']['initialized'] is False and j['cw_prefix_after']['statics']==[None]*5,'frontier not a completed result')
            outcome='FRONTIER119';value=None;at=terminal['site'];require(r['result']=={'name':'OK','ordinal':0} and not r['unchanged'] and not j['completion_preserved'],'separate original full completion')
            completed.append({'case':i,'scope':'ORIGINAL_COMPLETION_ONLY','result':r['result'],'exception':r['exception'],'journal':j['completion'],'cw_after':j['cw_completion_after']})
        elif r['exception']!='NONE':
            require(terminal['kind']=='EXCEPTION' and r['unchanged'] and j['completion']==[] and j['cw_prefix_after']['initialized'] is False,'actual null cold preserved prefix')
            outcome='SOURCE_NULL';value=None;at=terminal['site']
        else:
            require(terminal['kind']=='RETURN' and terminal['site']['class']=='gs' and terminal['site']['method']=='b' and terminal['site']['pc'] in (27,44,58,113) and exact(terminal['value'],r['result']) and r['unchanged'] and j['completion']==[] and j['cw_prefix_after']['initialized'] is True,'actual ordinary preserved return')
            outcome='RETURN';at=terminal['site'];v=r['result'];value={'static_field':FIELDS[v['ordinal']],'name_utf16':list(map(ord,v['name'])),'ordinal':v['ordinal']}
        rows.append({'case':i,'name':p['name'],'input':p['values'],'outcome':outcome,'site':at,'result':value,'preserved':True,'cw_before':j['cw_before'],'cw_after':j['cw_prefix_after'],'journal':j['prefix']})
    return {'schema':'PLAYER_SLEEP_FINITE_V1','rows':rows,'live_code':code,'original_completion_only':completed}
def validate_data(v,expected):
    require(exact(v,expected),'finite packet differs in type/value/order/extent from both original projections')
    return v
def capture(root):
    root=Path(root);c=load(root/'capture.json');require(c['status']=='PASS' and c['profiles_per_vm']==48 and c['input_before_sha256']==c['input_after_sha256'] and c['helper_before_sha256']==c['helper_after_sha256'],'capture terminal/unchanged inputs')
    for name,sha in c['input_before_sha256'].items():require(type(sha) is str and re.fullmatch('[0-9a-f]{64}',sha) and digest(name)==sha,'current exact capture input '+name)
    for name,sha in c['helper_before_sha256'].items():require(digest(root/'classes'/name)==sha,'capture unchanged helper '+name)
    for r in [c['compile'],*c['original']]:
        require(r['exit']==0 and type(r['exit']) is int and r['timed_out'] is False and r['pipe_capture_complete'] is True and r['reader_errors']=={} and r['output_overflow'] is False,'bounded subprocess successful receipt')
        for stream in ('stdout','stderr'):
            p=root/r[stream+'_file'];require(integer(r[stream+'_bytes'],0,8*1024*1024) and p.stat().st_size==r[stream+'_bytes'] and digest(p)==r[stream+'_sha256'],'bounded pipe complete bytes/hash')
    pins=load(HERE/'source-pins.json');require(pins['archive_sha256']==JAR,'sealed original archive')
    for vm in range(2):
        expected=[str(i)+'\t'+n+'\t'+str(v['class_bytes'])+'\t'+v['class_sha256'] for i in range(48) for n,v in pins['classes'].items()]
        require((root/f'work-{vm}'/'bindings.tsv').read_text().splitlines()==expected,'per-profile original class origin/byte bindings')
    a,b=project(root,0),project(root,1);require(exact(a,b),'independent fresh original VM equivalence');return a
def main():
    import argparse
    p=argparse.ArgumentParser();p.add_argument('capture',type=Path);p.add_argument('--write-draft',action='store_true');a=p.parse_args();expected=capture(a.capture)
    if a.write_draft:(HERE/'finite.json').write_text(json.dumps(expected,indent=2)+'\n',encoding='utf-8')
    validate_data(load(HERE/'finite.json'),expected)
    print('Player sleep original packet:',len(expected['rows']),'rows;',len(expected['original_completion_only']),'original-only completions;',len(expected['live_code']),'live Code pins')
if __name__=='__main__':main()
