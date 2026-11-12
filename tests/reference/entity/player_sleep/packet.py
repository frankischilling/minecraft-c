"""Portable strict GS/CW prefix evidence; later source completions stay uncredited."""
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from validate import (FIELDS,JAR,NAMES,code_records,digest,enum_state,enum_value,event,exact,
                      integer,keys,load,loads,profiles,project,require,site,validate_data)

SOURCES=('PlayerSleepObservation.java','PlayerSleepJdi.java','prepare.py','capture_reference.py',
         'validate.py','packet.py','test_packet.py','profiles.json','profiles.tsv','source-pins.json',
         'class-names.txt','README.md')
FINITE='0d83579aeac2438a3594e3a56d320395968dfad3d5dbaefb926597a7012f995a'
EXECUTED_PRODUCERS='0d42f1120fcfafc31f55f513bdc2a3dc5b6c202287e754ccf29f337ff023157d'
EXECUTED_HELPERS='66b538e09c9cc796150e05a155a58acd983acc1e9e398d3936c09661caec564d'
BINARY_INPUTS='f0b6369ad6e29ff530f8bcc6f2915bf6e2c527c6efeec2a171a38b4226927bb9'
BINARY_INVENTORY='3fefb031d6185338706d3ebbde5202eb716e0042209f56ce5c95c5741ca10fd7'


def map_digest(value):
    import hashlib
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode('ascii')).hexdigest()


def binary_inputs(value):
    require(type(value) is dict and len(value)==98 and
            all(type(n) is str and n.isascii() and n.startswith(('jdk/','sdk/')) and
                ':' not in n and chr(92) not in n and all(p not in ('','.','..') for p in n.split('/')) and
                type(s) is str and re.fullmatch('[0-9a-f]{64}',s) for n,s in value.items()),
            'portable complete historical binary-input map')
    require(sum(n.startswith('jdk/') for n in value)==94 and sum(n.startswith('sdk/') for n in value)==4 and
            {'jdk/bin/java.exe','jdk/bin/javac.exe','jdk/lib/modules','jdk/bin/server/jvm.dll'}<=set(value),
            '94JDK and4SDK historical binary roles')
    require(map_digest(sorted(value))==BINARY_INVENTORY and map_digest(value)==BINARY_INPUTS,
            'immutable complete historical binary inventory and values')


def bounded(path,maximum=4*1024*1024):
    path=Path(path)
    require(path.stat().st_size<=maximum,'bounded packet artifact')
    return path


DATA=('finite.json','profiles.json','profiles.tsv','source-pins.json','class-names.txt')


def validate_canonical(value):
    keys(value,('schema','rows','live_code','original_completion_only'),'canonical finite schema')
    require(value['schema']=='PLAYER_SLEEP_FINITE_V1' and type(value['rows']) is list and len(value['rows'])==48,
            'all48 selected prefixes')
    ps=profiles()
    counts={'RETURN':0,'SOURCE_NULL':0,'FRONTIER119':0}
    events=[]
    for index,(row,profile) in enumerate(zip(value['rows'],ps)):
        keys(row,('case','name','input','outcome','site','result','preserved','cw_before','cw_after','journal'),
             'closed selected prefix schema')
        require(integer(row['case'],0,47) and row['case']==index and row['name']==profile['name'] and
                exact(row['input'],profile['values']) and row['preserved'] is True,'ordered prepared prefix identity')
        require(row['outcome'] in counts,'selected outcome')
        counts[row['outcome']]+=1
        site(row['site']);enum_state(row['cw_before']);enum_state(row['cw_after'])
        require(row['cw_before']['initialized'] is False,'CW initially cold')
        require(type(row['journal']) is list and 1<=len(row['journal'])<=128,'bounded selected source journal')
        for e in row['journal']:event(e)
        require(row['journal'][0]=={'kind':'ENTER','site':{'class':'gs','method':'b','descriptor':'(III)Lcw;','pc':0},
                'arguments':list(map(int,profile['values'][12:]))},'actual entered method arguments')
        last=row['journal'][-1]
        require(exact(last['site'],row['site']),'actual terminal prefix site')
        if row['outcome']=='RETURN':
            require(last['kind']=='RETURN' and row['site']['pc'] in (27,44,58,113) and
                    row['cw_after']['initialized'] is True,'actual ordinary return')
            result=row['result'];keys(result,('static_field','name_utf16','ordinal'),'returned CW reference projection')
            require(integer(result['ordinal'],0,4) and result['static_field']==FIELDS[result['ordinal']] and
                    exact(result['name_utf16'],list(map(ord,NAMES[result['ordinal']]))),'actual returned name/ordinal/static')
        else:
            require(row['result'] is None and row['cw_after']['initialized'] is False,'no fabricated CW result')
            require(last['kind']==('FRONTIER119' if row['outcome']=='FRONTIER119' else 'EXCEPTION'),
                    'actual frontier/null terminal')
        events.extend(row['journal'])
    require(counts=={'RETURN':31,'SOURCE_NULL':4,'FRONTIER119':13} and len(events)==1500 and
            sum(e['kind']=='FIELD_READ' for e in events)==620 and sum(e['kind']=='STATIC_WRITE' for e in events)==186,
            'exact original selected outcomes/events/reads/CW publications')
    code_records(value['live_code'])
    completion=value['original_completion_only']
    require(type(completion) is list and len(completion)==13,'later source completions segregated')
    expected=[r['case'] for r in value['rows'] if r['outcome']=='FRONTIER119']
    for row,index in zip(completion,expected):
        keys(row,('case','scope','result','exception','journal','cw_after'),'uncredited completion schema')
        require(integer(row['case'],0,47) and row['case']==index and row['scope']=='ORIGINAL_COMPLETION_ONLY' and
                exact(row['result'],{'name':'OK','ordinal':0}) and row['exception']=='NONE','uncredited later completion')
        require(type(row['journal']) is list and len(row['journal'])<=128,'bounded uncredited later journal')
        for e in row['journal']:event(e)
        enum_state(row['cw_after'])
    return value


def read_reference(directory=HERE):
    """Canonical prefixes/identities only; raw attempts are private producer inputs."""
    directory=Path(directory)
    p=load(bounded(directory/'provenance.json'))
    counts={'profiles_per_vm':48,'fresh_headless_vms':2,'fresh_original_namespaces_per_vm':48,
            'selected_early_returns':31,'selected_source_null_faults':4,'selected_frontier119_snapshots':13,
            'selected_events_per_vm':1500,'selected_field_reads_per_vm':620,'selected_cw_publications_per_vm':186,
            'original_completion_only_rows':13,'live_code_methods':6,'original_classes':10,
            'jdi_target_invocations':0,'jdi_source_writes':0,'jdi_forced_returns':0,'jdi_redefinitions':0}
    expected={'schema','status','archive_sha256','source_inputs_unchanged','generated_helpers_unchanged',
              'original_projections_identical','source_guard_counters','canonical_sha256','executed_producer_sha256',
              'current_reproduction_sha256','canonical_exporter_sha256','private_evidence_sha256','helper_sha256','binary_input_sha256',
              'vm_projections','scope',*counts}
    keys(p,expected,'closed canonical provenance schema')
    require(p['schema']=='PLAYER_SLEEP_CANONICAL_V1' and p['status']=='PASS_PREFIX_ONLY' and p['archive_sha256']==JAR and
            p['source_inputs_unchanged'] is True and p['generated_helpers_unchanged'] is True and
            p['original_projections_identical'] is True,'historical checked original qualification')
    for name,value in counts.items():require(type(p[name]) is int and p[name]==value,'typed original count '+name)
    binary_inputs(p['binary_input_sha256'])
    require(exact(p['source_guard_counters'],{'network':0,'native':0,'rejected_writes':0}),'actual source guard attestations')
    require(set(p['canonical_sha256'])==set(DATA) and set(p['current_reproduction_sha256'])==set(SOURCES),
            'exact data/current reproduction inventories')
    for group in ('canonical_sha256','current_reproduction_sha256'):
        for name,sha in p[group].items():
            require(type(sha) is str and re.fullmatch('[0-9a-f]{64}',sha) and digest(directory/name)==sha,
                    'canonical data/current reproduction source changed '+name)
    require(set(p['executed_producer_sha256'])==set(SOURCES) and
            all(type(v) is str and re.fullmatch('[0-9a-f]{64}',v) for v in p['executed_producer_sha256'].values()),
            'historical executed producers kept separate')
    require(map_digest(p['executed_producer_sha256'])==EXECUTED_PRODUCERS,
            'immutable complete executed-producer identity')
    for name in ('PlayerSleepObservation.java','PlayerSleepJdi.java'):
        require(digest(directory/name)==p['executed_producer_sha256'][name],'actual observer/JDI source continuity')
    require(p['canonical_exporter_sha256']=={'validate.py':p['current_reproduction_sha256']['validate.py'],
                                          'packet.py':p['current_reproduction_sha256']['packet.py']},
            'separate canonical projection/reader identities')
    require(type(p['private_evidence_sha256']) is dict and len(p['private_evidence_sha256'])==18 and
            all(type(n) is str and '/' not in n and chr(92) not in n and type(s) is str and re.fullmatch('[0-9a-f]{64}',s)
                for n,s in p['private_evidence_sha256'].items()),'private immutable evidence digests only')
    require(type(p['helper_sha256']) is dict and set(p['helper_sha256'])=={'PlayerSleepObservation.class',
            'PlayerSleepObservation$Guard.class','PlayerSleepJdi.class'} and
            all(type(s) is str and re.fullmatch('[0-9a-f]{64}',s) for s in p['helper_sha256'].values()),
            'historical compiled-helper continuity')
    require(map_digest(p['helper_sha256'])==EXECUTED_HELPERS,'immutable complete executed-helper identity')
    require(digest(directory/'finite.json')==FINITE,'exact original canonical prefix/completion projection')
    value=validate_canonical(load(bounded(directory/'finite.json')))
    projection={'finite_sha256':FINITE,'profiles':48,'original_namespaces':48,'class_bindings_checked':480,
                'all_bindings_match':True,'target_exit':0,'timed_out':False,'capture_complete':True,
                'overflow':False,'reader_errors':0,'live_code_methods':6}
    require(type(p['vm_projections']) is list and len(p['vm_projections'])==2 and
            all(exact(v,projection) for v in p['vm_projections']),'two privately checked matching finite projections')
    return value


if __name__=='__main__':
    print('Canonical Player sleep prefix packet:',len(read_reference()['rows']),'rows;13later completions uncredited')
