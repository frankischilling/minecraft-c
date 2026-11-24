"""Offline corruption controls; no game/original/native process execution."""
import copy
import json
from pathlib import Path
import sys
import tempfile
sys.dont_write_bytecode=True
import packet as P

def updated(blobs,name,value):
    result=dict(blobs);result[name]=(json.dumps(value,indent=2)+'\n').encode('utf8');changed=[name]
    provenance=P.parse(result['provenance.json'])
    if name in ('finite.json','receipts.json','source-pins.json'):
        field={'finite.json':'finite_sha256','receipts.json':'receipts_sha256','source-pins.json':'class_pins_sha256'}[name]
        provenance[field]=P.digest(P.encoded(value['classes'] if name=='source-pins.json' else value))
        result['provenance.json']=(json.dumps(provenance,indent=2)+'\n').encode('utf8');changed.append('provenance.json')
    manifest=P.parse(result['publication.json'])
    for key in changed:manifest['files'][key]={'bytes':len(result[key]),'sha256':P.digest(result[key])}
    result['publication.json']=(json.dumps(manifest,indent=2)+'\n').encode('utf8')
    return result
def rejected(action,name):
    try:action()
    except (ValueError,AssertionError,KeyError,TypeError,IndexError,StopIteration):return name
    raise ValueError('corruption accepted: '+name)

def main():
    blobs=P.files(Path(__file__).resolve().parent);finite=P.audit(blobs);controls=[]
    def data(name,edit):
        value=copy.deepcopy(finite);edit(value)
        controls.append(rejected(lambda:P.audit(updated(blobs,'finite.json',value)),name))
    def meta(file,name,edit):
        value=P.parse(blobs[file]);edit(value)
        controls.append(rejected(lambda:P.audit(updated(blobs,file,value)),name))
    mode=lambda v:v['observations']['default']
    row=lambda v,i:mode(v)['rows'][i]
    positive=next(i for i,r in enumerate(mode(finite)['rows']) if len(r['events'])>2)
    data('unknown allocates fake Entity',lambda v:row(v,0)['after'].__setitem__('counter',999))
    data('cast failure flattened to NULL',lambda v:row(v,1).__setitem__('returned',True))
    data('cast failure moved into factory',lambda v:row(v,1)['failure']['frames'][0].__setitem__('class','jc'))
    data('population cap constructs extra candidates',lambda v:row(v,2)['after'].__setitem__('counter',999))
    data('cap delay omitted',lambda v:row(v,2)['after'].__setitem__('delay',0))
    data('rejected predicates reset delay',lambda v:row(v,3)['after'].__setitem__('delay',200))
    data('publication callback omitted',lambda v:row(v,positive)['events'].pop(2))
    data('last explode omitted',lambda v:row(v,positive)['events'].pop(62))
    data('CY smoke/flame reordered',lambda v:row(v,positive)['events'][3].__setitem__('name','flame'))
    data('LS.V replaced by smoke',lambda v:row(v,positive)['events'][43].__setitem__('name','smoke'))
    data('delay changed during source effects',lambda v:row(v,positive)['events'][4].__setitem__('delay',222))
    data('own Random omitted',lambda v:row(v,positive)['events'][2]['entity'].pop('random'))
    data('own Random seed normalized',lambda v:row(v,positive)['events'][2]['entity']['random'].__setitem__('seed',True))
    data('Gaussian cache omitted',lambda v:row(v,positive)['events'][2]['entity']['random'].pop('cached'))
    data('position corrupted',lambda v:row(v,positive)['events'][2]['entity']['position_bits'].__setitem__(0,'7ff0000000000000'))
    data('wrong concrete Pig',lambda v:row(v,positive)['events'][2]['entity'].__setitem__('class','sl'))
    data('World identity lost',lambda v:row(v,positive)['events'][2]['entity'].__setitem__('world_same',False))
    data('source ID changed',lambda v:row(v,positive)['events'][2]['entity'].__setitem__('id',999))
    data('World publication count wrong',lambda v:row(v,positive)['events'][2].__setitem__('world_entity_count',0))
    data('World time changed',lambda v:row(v,0)['after'].__setitem__('world_time',9))
    data('extra claimed state',lambda v:row(v,0)['after'].__setitem__('complete_native_parity',True))
    data('loaded CY identity lost',lambda v:row(v,0)['after']['active_tiles'][0].__setitem__('same_selected',False))
    data('Player fabricated',lambda v:row(v,0)['after']['players'].append({'label':'fake','class':'dc','id':99}))
    data('Chunk removed',lambda v:row(v,0)['after']['chunks'].pop())
    data('positive count summary repaired incorrectly',lambda v:mode(v)['summary'].__setitem__('observed_successful_publications',0))
    data('deterministic common projection changed',lambda v:mode(v)['summary']['common_projection'][0].__setitem__('counter_delta',1))
    data('raw constructor entropy omitted',lambda v:row(v,positive)['events'][2]['entity'].pop('living_ctor_bits'))
    data('negative input profile changed',lambda v:row(v,0)['profile'].__setitem__(2,'Pig'))
    for file,name,edit in (
        ('provenance.json','invented native parity',lambda v:v.__setitem__('native_parity_claimed',True)),
        ('provenance.json','invented internal trace',lambda v:v.__setitem__('internal_trace_claimed',True)),
        ('provenance.json','unsupported World tick scope',lambda v:v.__setitem__('scope','Complete FD.g parity')),
        ('provenance.json','saving exclusion removed',lambda v:v['excluded'].remove('original save or bidirectional roundtrip')),
        ('provenance.json','invented rerun',lambda v:v.__setitem__('original_processes',4)),
        ('provenance.json','failed prep mixed into cohort',lambda v:v['failed_preparations'][1].__setitem__('completed_tick_rows',10)),
        ('provenance.json','failed prep omitted',lambda v:v['failed_preparations'].pop()),
        ('provenance.json','historical JVM options understated',lambda v:v.__setitem__('keys_present',['JAVA_TOOL_OPTIONS'])),
        ('provenance.json','executed Java changed',lambda v:v['executed_inputs']['observer-java']['after'].__setitem__('sha256','0'*64)),
        ('provenance.json','archive method count overstated',lambda v:v.__setitem__('code_method_count',827)),
        ('provenance.json','material boundary expands to roundtrip',lambda v:v.__setitem__('raw_material_scope','Complete bidirectional save roundtrip')),
        ('provenance.json','failed preparation promoted to evidence',lambda v:v['failed_preparations'][1].__setitem__('reason','Original complete tick parity')),
        ('receipts.json','incomplete output pipes',lambda v:v['default'].__setitem__('pipe_capture_complete',False)),
        ('receipts.json','malformed exit type',lambda v:v['default'].__setitem__('exit',False)),
        ('receipts.json','reader failure ignored',lambda v:v['default'].__setitem__('reader_errors',{'stdout':['error']})),
        ('receipts.json','output overflow',lambda v:v['default'].__setitem__('output_overflow',True)),
        ('receipts.json','child timeout',lambda v:v['default'].__setitem__('timed_out',True)),
        ('receipts.json','wrong VM mode',lambda v:v['interpreter']['command_roles'].remove('-Xint')),
        ('receipts.json','swapped copied World root',lambda v:v['default']['command_roles'].__setitem__(-2,'OWNED_COPY/interpreter/saves')),
        ('receipts.json','wrong declared profiles',lambda v:v['default']['command_roles'].__setitem__(-1,'ANOTHER_INPUT')),
        ('receipts.json','changed stdout hash',lambda v:v['default'].__setitem__('stdout_sha256','0'*64)),
        ('receipts.json','changed stdout count',lambda v:v['default'].__setitem__('stdout_bytes',1)),
        ('receipts.json','negative stderr count',lambda v:v['default'].__setitem__('stderr_bytes',-1)),
        ('receipts.json','actual stderr hash changed',lambda v:v['default'].__setitem__('stderr_sha256','0'*64)),
        ('receipts.json','actual raw process receipt changed',lambda v:v['default']['raw_receipt'].__setitem__('sha256','0'*64)),
        ('source-pins.json','CY Code changed',lambda v:next(m for m in v['classes']['cy']['methods'] if m['name']=='n_').__setitem__('code_sha256','0'*64)),
        ('source-pins.json','LS.V Code changed',lambda v:next(m for m in v['classes']['ls']['methods'] if m['name']=='V').__setitem__('code_bytes',1)),
        ('source-pins.json','other actual Class byte pin changed',lambda v:v['classes']['sn'].__setitem__('class_sha256','0'*64)),
        ('source-manifests.json','fixture source file lost',lambda v:v['source_after'].pop('case00/level.dat')),
        ('source-manifests.json','authored data directory lost',lambda v:v['copy_directories_after']['default'].remove('case00/data')),
        ('source-manifests.json','copied region changed',lambda v:v['copies_after']['default']['case00/region/r.0.0.mcr'].__setitem__('sha256','0'*64)),
        ('source-manifests.json','retained native save changed',lambda v:v['base_save_after']['alpha/level.dat'].__setitem__('sha256','0'*64))):meta(file,name,edit)
    bad=dict(blobs);bad['native-invariants.h']=bad['native-invariants.h'].replace(b'{0, 0, true',b'{0, 9, true')
    manifest=P.parse(bad['publication.json']);manifest['files']['native-invariants.h']={'bytes':len(bad['native-invariants.h']),'sha256':P.digest(bad['native-invariants.h'])};bad['publication.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    controls.append(rejected(lambda:P.audit(bad),'C projection changed despite repaired publication manifest'))
    for name in ('unexpected.class','raw.stdout','world.dat'):
        bad=dict(blobs);bad[name]=b'unlisted material';controls.append(rejected(lambda bad=bad:P.audit(bad),'unexpected '+name))
    with tempfile.TemporaryDirectory() as directory:
        (Path(directory)/'nested').mkdir();controls.append(rejected(lambda:P.files(directory),'nested binary/World directory'))
    pins=P.parse(blobs['source-pins.json'])['classes'];cases=P.parse(blobs['cases.json']);ending=P.parse(blobs['provenance.json'])['stdout_line_ending']
    wire=P.stdout_transport(finite['observations']['interpreter'],pins,ending);lines=wire.splitlines(keepends=True)
    P.C.output(wire,pins,cases)
    for name,value in [('missing footer',b''.join(lines[:-1])),('duplicate footer',wire+lines[-1]),('moved row',b''.join(lines[:3]+[lines[4],lines[3]]+lines[5:])),('unexpected raw line',wire+b'EXTRA\n')]:
        controls.append(rejected(lambda value=value:P.C.output(value,pins,cases),'authored parser transport '+name))
    print('PASS: '+str(len(controls))+' expired-Spawner packet corruption controls; no runtime')
if __name__=='__main__':main()
