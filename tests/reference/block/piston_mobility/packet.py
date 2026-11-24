"""Strict offline audit of the finite original Block.h evidence packet."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('mobility_capture',HERE/'capture.py');C=importlib.util.module_from_spec(spec);spec.loader.exec_module(C)
JAR=C.JAR
MODES=('interpreter','default')
STDOUT_PIN={'bytes':28063,'sha256':'e54a39da6827cfb9b4bb323b640ef17fb27e1822521ec4a23ca0075b043be85e'}
SOURCE_PIN={'bytes':197746,'sha256':'fdc3461f68823bb89c3a5f04449688d1de702e43d36f80ef353e3d9a5cbd647c'}
FINITE_PIN={'bytes':36117,'sha256':'61432471a12c71e80b2cbba1ec390c215f55815b5a8ea473b4a42567fc42bff3'}
OVERRIDES={26:('ve',1),27:('pc',0),28:('pc',0),64:('le',1),66:('pc',0),70:('bv',1),71:('le',1),72:('bv',1),79:('nk',0)}
CODE={'uu':(8,'3a25d0cb6f286b9c7b98b0f17bcd23451078ebf160073e82644cfb26583a04d2'),
      **{n:(2,'95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2') for n in ('bv','le','ve')},
      **{n:(2,'42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e') for n in ('pc','nk')}}
EXCLUDED=['World or piston gameplay','server original execution','native execution or parity','GL or pixels','JDI/live-Code or call-order trace','original NULL Material receiver']
FILES={'PistonMobilityObservation.java','cases.json','capture.py','packet.py','publish.py','test_packet.py','observations.json','finite.json','source-pins.json','provenance.json','native-observations.h','README.md'}
def need(v,m):
    if not v:raise ValueError(m)
def parse(raw):return json.loads(raw,object_pairs_hook=C.unique,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite JSON')))
def encoded(v):return (json.dumps(v,indent=2)+'\n').encode('utf8')
def digest(raw):return hashlib.sha256(raw).hexdigest()
def pin(raw):return {'bytes':len(raw),'sha256':digest(raw)}
def keys(v,k,m):need(type(v) is dict and set(v)==set(k),'closed '+m)
def native_header(rows):
    lines=['#ifndef MC_REFERENCE_PISTON_MOBILITY_H','#define MC_REFERENCE_PISTON_MOBILITY_H','','#include <stdint.h>','','/* Actual original Block.h results; no World or gameplay trace. */','typedef struct {','    int32_t block_id;','    int32_t mobility;','} mc_piston_mobility_reference_row;','','static const mc_piston_mobility_reference_row mc_piston_mobility_reference[] = {']
    # Canonical clang-format 18.1.3 output: ten aligned entries per line.
    for start in range(0,len(rows),10):
        cells=['{'+str(r['id'])+', '+str(r['mobility'])+'},' for r in rows[start:start+10]]
        lines.append('    '+''.join(cell.ljust(9) for cell in cells).rstrip())
    lines+=['};','','#define MC_PISTON_MOBILITY_REFERENCE_COUNT 96u','','#endif','']
    return '\n'.join(lines).encode('ascii')
def declaration(d):
    need(d=={'schema':'ORIGINAL_PISTON_MOBILITY_INPUTS_V1','scope':'Direct virtual UU.h()I on every nonnull ordinary Block.m singleton, ascending actual array index; repeat each call, then read actual Material.j()I.',
        'startup':'Class.forName uu with ordinary untouched original class initialization','inputs':'Actual occupied Block.m cells; no fabricated receiver or Material',
        'forbidden':['source field writes','Map or array mutation','World construction','piston activation','tick','GL','native execution','JDI or live-Code claim'],'modes':list(MODES)},'exact predeclared finite original leaf scope')
def finite(f,pins):
    keys(f,('origins','rows','complete'),'original finite projection')
    keys(f['origins'],('jar_sha256','classes','origin_role'),'loaded resource identity')
    need(f['origins']['jar_sha256']==JAR and f['origins']['origin_role']=='verified-original-jar','actual original archive origin')
    classes=pins['classes'];need(f['origins']['classes']=={n:p['class_sha256'] for n,p in classes.items()},'loaded class resource bytes equal independently inspected archive entries')
    rows=f['rows'];need(type(rows) is list and len(rows)==96,'all 96 registered singleton observations')
    for i,r in enumerate(rows,1):
        keys(r,('id','runtime_class','declaring_class','registered_identity','id_before','id_after','material_same','material_class','material_mobility','mobility','repeated_mobility'),'actual Block leaf row')
        need(all(type(r[n]) is int for n in ('id','id_before','id_after','material_mobility','mobility','repeated_mobility')),'exact integral leaf values')
        need(r['id']==r['id_before']==r['id_after']==i and r['registered_identity'] is True and r['material_same'] is True,'actual registered identity retained without field repair')
        need(r['runtime_class'] in classes and r['material_class'] in classes,'actual original runtime/material classes')
        owner,value=OVERRIDES.get(i,('uu',r['material_mobility']))
        need(r['declaring_class']==owner and r['mobility']==r['repeated_mobility']==value,'actual virtual overriding/inherited leaf result')
        need(r['material_mobility'] in (0,1,2) and r['mobility'] in (0,1,2),'bounded original mobility values')
    need(f['complete']=={'rows':96,'h_calls':192,'material_j_calls':96,'writes':0,'network':0,'natives':0,'execs':0} and all(type(x) is int for x in f['complete'].values()),'exact complete guarded operation counts')
    return rows
def source(pins):
    keys(pins,('jar_sha256','classes'),'archive source pins');need(pins['jar_sha256']==JAR,'unchanged client JAR')
    classes=pins['classes'];need(type(classes) is dict and len(classes)==78,'complete actual loaded game class closure')
    need(sum(len(p['methods']) for p in classes.values())==808 and sum(m.get('code_sha256') is not None for p in classes.values() for m in p['methods'])==807,'exact archive method inventory summary')
    for n,p in classes.items():
        keys(p,('class_bytes','class_sha256','major','minor','methods'),'archive class inventory')
        need(type(p['class_bytes']) is int and p['class_bytes']>0 and len(p['class_sha256'])==64,'archive class identity');methods=p['methods'];identities=[(m['name'],m['descriptor']) for m in methods];need(len(identities)==len(set(identities)),'unique archive method inventory')
    for n,(length,sha) in CODE.items():
        selected=[m for m in classes[n]['methods'] if m['name']=='h' and m['descriptor']=='()I']
        need(len(selected)==1 and selected[0]['code_bytes']==length and selected[0]['code_sha256']==sha,'exact archive virtual mobility Code identity')
    need(not any(m['name']=='h' and m['descriptor']=='()I' for m in classes['ph']['methods']),'detector rail inherits PC.h rather than fabricated override')
def command(mode):return ['<java17>',*(['-Xint'] if mode=='interpreter' else []),'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow','-cp','<compiled-observer>','PistonMobilityObservation','<verified-client-jar>']
def audit(files):
    need(set(files)==FILES|{'publication.json'},'exact public packet files; no original binary/assets/private files')
    manifest=parse(files['publication.json']);keys(manifest,('schema','scope','files'),'publication manifest')
    need(manifest['schema']=='PISTON_MOBILITY_PUBLICATION_V1' and manifest['scope']=='Post-execution packaging; archive pins are not live-Code evidence.','publication scope')
    need(manifest['files']=={n:pin(raw) for n,raw in files.items() if n!='publication.json'},'complete physical public bytes/hash manifest')
    declaration(parse(files['cases.json']));pins=parse(files['source-pins.json']);source(pins);f=parse(files['finite.json']);rows=finite(f,pins)
    need(pin(files['source-pins.json'])==SOURCE_PIN and pin(files['finite.json'])==FINITE_PIN,'unchanged captured archive and finite projection bytes')
    observations=parse(files['observations.json']);keys(observations,MODES,'both original streams')
    p=parse(files['provenance.json']);keys(p,('schema','status','scope','excluded','jar_sha256','modes','inputs_before','inputs_after','products_before','products_after','removed_environment','host','compile','runs','capture_report','archive_inventory','captured_helpers','public_support_pins'),'provenance')
    need(p['schema']=='PISTON_MOBILITY_PROVENANCE_V1' and p['status']=='CAPTURED_ORIGINAL' and p['scope']=='Two fresh ordinary original VMs; direct Block.h leaves only; no field writes or World.' and p['excluded']==EXCLUDED,'exact measured scope/exclusions')
    need(p['jar_sha256']==JAR and p['modes']==list(MODES) and p['removed_environment']==['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'],'executed modes/environment')
    need(p['inputs_before']==p['inputs_after'] and p['products_before']==p['products_after'] and set(p['products_before'])=={'PistonMobilityObservation.class','PistonMobilityObservation$1.class'},'selected runtime/helper/product continuity')
    need(p['inputs_before']['original-jar']['sha256']==JAR and p['archive_inventory']=={'classes':78,'declared_methods':808,'Code_methods':807,'mobility_Code_pins':6},'actual archive scope/counts')
    for n in ('PistonMobilityObservation.java','cases.json','capture.py'):need(pin(files[n])==p['captured_helpers'][n]==p['inputs_before']['helper/'+n],'exact executed authored helper/input bytes')
    for n in ('capture.py','classfile.py'):
        public='tests/reference/client/controller/'+n;path=HERE.parents[3]/public
        need(p['public_support_pins'][public]==p['inputs_before']['support/'+n]==pin(path.read_bytes()),'unchanged public capture/Code reader support')
    compiler=p['compile'];keys(compiler,('command','exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stdout_sha256','stderr_bytes','stderr_sha256'),'authored helper compiler receipt')
    need(compiler['command']==['<javac17>','-proc:none','-encoding','UTF-8','-d','<compiled-observer>','PistonMobilityObservation.java'] and type(compiler['exit']) is int and compiler['exit']==0 and compiler['timed_out'] is False and compiler['pipe_capture_complete'] is True and compiler['reader_errors']=={} and compiler['output_overflow'] is False and compiler['timeout_seconds']==60 and compiler['stdout_bytes']==compiler['stderr_bytes']==0 and compiler['stdout_sha256']==compiler['stderr_sha256']==digest(b''),'exact helper compiler invocation, complete status and clean streams')
    for mode in MODES:
        raw=observations[mode].encode('utf8');actual=C.stdout(raw);need(actual==f,'reparse each complete original stream without row substitution')
        r=p['runs'][mode];keys(r,('command','exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stdout_sha256','stderr_bytes','stderr_sha256','stderr_scope'),'original process receipt')
        need(r['command']==command(mode) and type(r['exit']) is int and r['exit']==0 and r['timeout_seconds']==60 and r['timed_out'] is False and r['pipe_capture_complete'] is True and r['reader_errors']=={} and r['output_overflow'] is False,'exact successful bounded ordinary execution')
        need(pin(raw)=={'bytes':r['stdout_bytes'],'sha256':r['stdout_sha256']}==STDOUT_PIN and r['stderr_bytes']==415 and r['stderr_sha256']=='01935b62c4e9bc081d9a90a583fad77fbdfc4dd0746415dcfd91077f361ef120' and r['stderr_scope']=='Exact four JDK17 SecurityManager warnings; raw retained privately and physically audited.','complete original stream hashes and precise retained diagnostic scope')
    need(observations['interpreter']==observations['default'],'two modes agree on full finite stdout')
    need(files['native-observations.h']==native_header(rows),'native table derives exactly from actual original rows')
    return {'original_vms':2,'registered_blocks':96,'h_calls_per_vm':192,'material_j_calls_per_vm':96,'archive_classes':78,'archive_Code_methods':807,'original_World_or_tick_calls':0,'native_execution_claimed':False}
def read_packet():
    files={p.name:p.read_bytes() for p in HERE.iterdir() if p.is_file()};audit(files);return parse(files['finite.json'])
if __name__=='__main__':print(audit({p.name:p.read_bytes() for p in HERE.iterdir() if p.is_file()}))
