"""Pure portable positive/corruption controls; no original/native execution."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import packet as P
import validate as V
import capture_reference as C
import export_reference as E

def controls():
    finite=P.read_reference();pins=dict(V.load(HERE/'source-pins.json')['classes']);pins.update(V.load(HERE/'helper-pins.json')['classes']);rejected=0
    def refusal(action,label):
        nonlocal rejected
        try:action()
        except (ValueError,KeyError,TypeError,IndexError):rejected+=1;return
        raise ValueError('corruption accepted: '+label)
    for row in finite['rows']:
        for mutation in ('boolcase','booltick','callargument','unknown','droppedreturn','rawid'):
            j=copy.deepcopy(row['journal'])
            if mutation=='boolcase':j['case']=True
            elif mutation=='booltick':j['events'][0]['tick']=False
            elif mutation=='callargument':j['calls'][0]['arguments'][0]=1 if type(j['calls'][0]['arguments'][0]) is bool else True
            elif mutation=='unknown':j['events'][0]['extra']=0
            elif mutation=='droppedreturn':j['events']=j['events'][:-1]
            else:j['calls'][0]['receiver']['raw_object_id']=1
            refusal(lambda j=j:V.audit_journal(j,pins),mutation)
        for name in ('timer','world_J','v_same','dimensions','private_bN'):
            s=copy.deepcopy(row['after'])
            if name=='timer':s[name]=True
            elif name in ('world_J','v_same'):s[name]=1
            elif name=='dimensions':s[name][0]='NaN'
            else:s[name][0]=False
            refusal(lambda s=s:V.selected(s),'selected typed value '+name)
    for bad in ('{"a":1,"a":1}','{"a":NaN}','{"a":Infinity}'):
        refusal(lambda bad=bad:V.loads(bad),'noncanonical JSON')
    for bad in ({'raw_object_id':1,'identity':'x'}, {'class':'br','identity':'x','raw_object_id':1}, {'class':'cw','identity':'x','static':''}, {'class':'br','identity':'x','hidden':0}):
        refusal(lambda bad=bad:V.typed(bad),'closed canonical reference')
    for mutation in ('row_bool','wrong_fault','code_bool','enum_identity','random_bool','extra'):
        f=copy.deepcopy(finite)
        if mutation=='row_bool':f['rows'][0]['case']=False
        elif mutation=='wrong_fault':next(e for e in f['rows'][0]['journal']['events'] if e['kind']=='EXCEPTION')['site']['pc']=85
        elif mutation=='code_bool':f['live_code'][0]['bytes']=True
        elif mutation=='enum_identity':f['rows'][1]['result']['current_a']=1
        elif mutation=='random_bool':f['rows'][1]['random_before'][0][1]=0
        else:f['rows'][1]['unknown']=0
        refusal(lambda f=f:V.validate_canonical(f),'whole finite '+mutation)
    # Historical maps are anchored independently of editable current-tool pins.
    proof=V.load(HERE/'provenance.json')
    for name,anchor in P.HISTORICAL.items():
        m=copy.deepcopy(proof[name]);first=next(iter(m));m[first]='0'*64
        refusal(lambda m=m,anchor=anchor:V.require(P.map_digest(m)==anchor,'historical map changed'),'historical map '+name)
        m=copy.deepcopy(proof[name]);m.pop(next(iter(m)))
        refusal(lambda m=m,anchor=anchor:V.require(P.map_digest(m)==anchor,'historical inventory changed'),'historical inventory '+name)
    # Exercise the actual reader against altered proofs, including table
    # rewrites that cannot rewrite the independently anchored historical maps.
    with tempfile.TemporaryDirectory(prefix='mc-sleep-wake-reader-') as temporary:
        directory=Path(temporary)
        for name in (*P.DATA,*P.SOURCES,'provenance.json'):(directory/name).write_bytes((HERE/name).read_bytes())
        def altered(value):
            (directory/'provenance.json').write_text(__import__('json').dumps(value),encoding='utf-8')
            return P.read_reference(directory)
        V.require(V.exact(P.read_reference(directory),finite),'portable copy reads independently')
        for name in P.HISTORICAL:
            p=copy.deepcopy(proof);first=next(iter(p[name]));p[name][first]='0'*64
            refusal(lambda p=p:altered(p),'actual reader historical map '+name)
            p=copy.deepcopy(proof);p[name].pop(next(iter(p[name])))
            refusal(lambda p=p:altered(p),'actual reader historical inventory '+name)
        for name in ('fresh_target_vms','selected_calls_per_vm','ordered_events_per_vm'):
            p=copy.deepcopy(proof);p[name]=True;refusal(lambda p=p:altered(p),'actual reader bool count')
        p=copy.deepcopy(proof);p['source_guard_counters']['network']=False;refusal(lambda p=p:altered(p),'actual reader bool counter')
        p=copy.deepcopy(proof);p['current_reproduction_sha256']['../escape.py']='0'*64;refusal(lambda p=p:altered(p),'actual reader source inventory escape')
        f=copy.deepcopy(finite);f['rows'][0]['case']=False
        member=directory/'finite.json';member.write_text(__import__('json').dumps(f),encoding='utf-8')
        p=copy.deepcopy(proof);p['canonical_sha256']['finite.json']=V.digest(member)
        refusal(lambda p=p:altered(p),'actual reader canonical hash-table rewrite')
    # Both producers reject these destinations before mkdir; none is created.
    for path in (HERE/'forbidden-attempt',HERE,C.ROOT/'tests/reference/forbidden-attempt',C.ROOT/'.local',C.ROOT):
        existed=path.exists();refusal(lambda path=path:C.private_output(path),'public/existing output');V.require(path.exists()==existed,'refusal had no filesystem effect')
    V.require(E.private_output is C.private_output,'exporter shares checked output admission')
    for script,arguments in (
        ('capture_reference.py',['--jar','missing.jar','--jdk','missing-jdk','--libraries-root','missing-libraries']),
        ('export_reference.py',['--evidence','missing-evidence'])):
        forbidden=HERE/('forbidden-'+script[:-3])
        V.require(not forbidden.exists(),'negative CLI destination absent')
        r=subprocess.run([sys.executable,'-B',str(HERE/script),'--output',str(forbidden),*arguments],capture_output=True,timeout=10,check=False)
        V.require(r.returncode!=0 and b'all raw/canonical attempt output' in r.stderr and not forbidden.exists(),'public output refused before evidence/tool reads or mkdir')
        rejected+=1
    V.require(not V.exact(True,1) and V.exact(V.profiles(),P.profiles()),'strict type comparison and public profile contract')
    V.require(sum(len(r['journal']['events']) for r in finite['rows'])==3979,'all original ordered events retained')
    return finite,rejected

if __name__=='__main__':
    finite,n=controls()
    print('Player sleep/wake packet:',len(finite['rows']),'rows;3979events;26Code; rejected',n,'corruptions; optimized='+str(not __debug__))
