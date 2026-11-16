"""Offline corruption controls over retained original finite projections only."""
import copy
import json
from pathlib import Path
import sys
import tempfile
sys.dont_write_bytecode=True
import packet as P
import run_native as N
def updated(blobs,name,value,repair_projection=False):
    out=dict(blobs);out[name]=(json.dumps(value,indent=2)+'\n').encode('utf8')
    changed=[name]
    if repair_projection:
        provenance=P.parse(out['provenance.json']);provenance['observation_projection_sha256']=P.digest(P.encoded(value))
        out['provenance.json']=(json.dumps(provenance,indent=2)+'\n').encode('utf8');changed.append('provenance.json')
    manifest=P.parse(out['publication.json'])
    for key in changed:manifest['files'][key]={'bytes':len(out[key]),'sha256':P.digest(out[key])}
    out['publication.json']=(json.dumps(manifest,indent=2)+'\n').encode('utf8')
    return out
def rejected(operation,name):
    try:operation()
    except (ValueError,AssertionError,KeyError,TypeError,IndexError,StopIteration):return name
    raise ValueError('corruption accepted: '+name)
def main():
    blobs=P.files(Path(__file__).resolve().parent);finite=P.audit(blobs);controls=[]
    def finite_case(name,edit,repair=True):
        value=copy.deepcopy(finite);edit(value)
        controls.append(rejected(lambda:P.audit(updated(blobs,'finite.json',value,repair)),name))
    def metadata_case(filename,name,edit):
        value=P.parse(blobs[filename]);edit(value)
        controls.append(rejected(lambda:P.audit(updated(blobs,filename,value)),name))
    finite_case('changed actual Entity ID',lambda v:v['observations']['default']['rows'][5]['result']['stable'].__setitem__('id',9))
    finite_case('changed actual ID delta',lambda v:v['observations']['default']['rows'][6].__setitem__('entity_id_delta',2))
    finite_case('wrong constructor World alias',lambda v:v['observations']['default']['rows'][7]['result']['stable'].__setitem__('world',None))
    finite_case('fake World publication',lambda v:v['observations']['default']['rows'][7]['world_after']['entities'].append('E2'))
    finite_case('extra receiver mutation by setter',lambda v:v['observations']['default']['rebind']['after']['stable'].__setitem__('health',11))
    finite_case('setter changes raw Random seed',lambda v:v['observations']['default']['rebind']['after']['nondeterministic'].__setitem__('sn.bs.seed',1))
    finite_case('constructor raw entropy omitted',lambda v:v['observations']['default']['rows'][5]['result']['nondeterministic'].pop('ls.F'))
    finite_case('typed entropy corrupted',lambda v:v['observations']['default']['rows'][5]['result']['nondeterministic'].__setitem__('sn.bs.seed',True))
    finite_case('original Mob NULL result fabricated',lambda v:v['observations']['default']['rows'][4].__setitem__('result',copy.deepcopy(v['observations']['default']['rows'][5]['result'])))
    finite_case('registry changed',lambda v:v['observations']['default']['registry']['after_all_calls'][0]['entries'].__setitem__('Pig','ls'))
    finite_case('claimed source failure escape',lambda v:v['observations']['default']['rows'][4].__setitem__('returned',False))
    finite_case('declared String identity lost',lambda v:v['observations']['default']['rows'][6].__setitem__('name_same_as_first_pig',True))
    finite_case('stable field normalized away',lambda v:v['stable_projection']['rows'][5]['result']['stable'].pop('id'))
    metadata_case('diagnostics.json','caught exception diagnostic omitted',lambda v:v['default'].__setitem__('caught_exception_count',0))
    metadata_case('diagnostics.json','wrong actual source stack frame',lambda v:v['default']['original_source_frame'].__setitem__('line',63))
    metadata_case('input-world.json','changed independently decoded World time',lambda v:v['metadata'].__setitem__('time',212))
    metadata_case('cases.json','changed input name',lambda v:v['cases'][3].__setitem__('name','Pig'))
    metadata_case('cases.json','expanded entropy exclusion',lambda v:v['nondeterministic_fields_excluded_only_from_cross_vm_equality'].append('id'))
    metadata_case('cases.json','unsupported AG operation',lambda v:v['forbidden'].remove('AG draw'))
    metadata_case('cases.json','wrong source World preparation order',lambda v:v['startup'].reverse())
    metadata_case('provenance.json','unsupported native parity claim',lambda v:v.__setitem__('native_parity_claimed',True))
    metadata_case('provenance.json','unsupported live-Code scope',lambda v:v['excluded'].remove('JDI/live-Code or call-order proof'))
    metadata_case('provenance.json','invented original rerun',lambda v:v.__setitem__('original_processes',4))
    metadata_case('provenance.json','additional whole-frame claim',lambda v:v.__setitem__('whole_spawner_frame_passed',True))
    metadata_case('provenance.json','executed observer byte identity changed',lambda v:v['executed_inputs']['observer-java']['after'].__setitem__('sha256','0'*64))
    metadata_case('source-manifests.json','owned region changed',lambda v:v['copies_after']['default']['alpha/region/r.0.0.mcr'].__setitem__('sha256','0'*64))
    metadata_case('source-manifests.json','retained source file removed',lambda v:v['source_after'].pop('bravo/preserve-chunk.dat'))
    metadata_case('receipts.json','incomplete pipe capture',lambda v:v['default'].__setitem__('pipe_capture_complete',False))
    metadata_case('receipts.json','missing -Xint mode',lambda v:v['interpreter']['command_roles'].remove('-Xint'))
    metadata_case('receipts.json','swapped copied World root',lambda v:v['default']['command_roles'].__setitem__(-4,'OWNED_COPY/interpreter/saves'))
    metadata_case('receipts.json','wrong constructor name argument',lambda v:v['default']['command_roles'].__setitem__(-2,''))
    metadata_case('receipts.json','changed actual raw stdout hash',lambda v:v['default'].__setitem__('stdout_sha256','0'*64))
    metadata_case('receipts.json','changed actual raw stdout byte count',lambda v:v['default'].__setitem__('stdout_bytes',1))
    metadata_case('receipts.json','changed actual raw stderr hash',lambda v:v['default'].__setitem__('stderr_sha256','0'*64))
    metadata_case('receipts.json','changed actual raw stderr byte count',lambda v:v['default'].__setitem__('stderr_bytes',1))
    metadata_case('receipts.json','changed retained receipt hash',lambda v:v['default']['raw_receipt'].__setitem__('sha256','0'*64))
    metadata_case('receipts.json','original classes allowed to recompile',lambda v:v['compile']['command_roles'].remove('-implicit:none'))
    metadata_case('source-pins.json','named overload archive Code changed',lambda v:next(m for m in v['classes']['jc']['methods'] if m['descriptor']=='(Ljava/lang/String;Lfd;)Lsn;').__setitem__('code_sha256','0'*64))
    bad=dict(blobs);bad['native-observations.h']=bad['native-observations.h'].replace(b'{5, 0, false',b'{5, 9, false')
    manifest=P.parse(bad['publication.json']);manifest['files']['native-observations.h']={'bytes':len(bad['native-observations.h']),'sha256':P.digest(bad['native-observations.h'])}
    bad['publication.json']=(json.dumps(manifest,indent=2)+'\n').encode('utf8')
    controls.append(rejected(lambda:P.audit(bad),'C expected header differs from actual finite fields'))
    bad=dict(blobs);bad['unexpected.class']=b'not original evidence'
    controls.append(rejected(lambda:P.audit(bad),'unexpected binary material'))
    with tempfile.TemporaryDirectory() as directory:
        (Path(directory)/'nested').mkdir();controls.append(rejected(lambda:P.files(directory),'nested unlisted material'))
    # A reconstructed transport exercises parser closure only. It is not an extra VM or raw stream publication.
    pins=P.parse(blobs['source-pins.json'])['classes'];cases=P.parse(blobs['cases.json'])
    line_ending=P.parse(blobs['provenance.json'])['stdout_line_ending']
    wire=P.stdout_transport(finite['observations']['interpreter'],pins,line_ending)
    P.C.stdout(wire,cases,pins)
    lines=wire.splitlines(keepends=True)
    controls.append(rejected(lambda:P.C.stdout(b''.join(lines[:-1]),cases,pins),'reconstructed transport missing footer'))
    controls.append(rejected(lambda:P.C.stdout(wire+lines[-1],cases,pins),'reconstructed transport duplicate footer'))
    controls.append(rejected(lambda:P.C.stdout(b''.join(lines[:-2]+[lines[-1],lines[-2]]),cases,pins),'reconstructed transport moved stage'))
    # Authored transports test only the native runner's gate, never native/original outcomes.
    footer=b'PASS: 321 named Entity factory/World binding checks\n'
    banner_pair=b'151 recipes\n16 achievements\n';prefix=banner_pair*2
    P.need(N.native_stdout(prefix+footer)==321,'native footer positive transport control')
    for name,wire in (
        ('native missing terminal footer',prefix),
        ('native malformed count',prefix+footer.replace(b'321',b'321x')),
        ('native zero count',prefix+footer.replace(b'321',b'0')),
        ('native duplicate footer',prefix+footer+footer),
        ('native moved footer',prefix+footer+b'151 recipes\n'),
        ('native unexpected prefix',prefix+b'unknown class output\n'+footer),
        ('native duplicate banner',prefix+b'151 recipes\n'+footer),
        ('native wrong banner order',b'16 achievements\n151 recipes\n'*2+footer),
        ('native extra banner pair',prefix+banner_pair+footer),
        ('native missing banner pair',banner_pair+footer),
        ('native missing one banner',banner_pair+b'151 recipes\n'+footer)):
        controls.append(rejected(lambda wire=wire:N.native_stdout(wire),name))
    with tempfile.TemporaryDirectory() as directory:
        directory=Path(directory);command=['NATIVE_EXE','AUTHORED_RESOURCES','WORLD_A','WORLD_B']
        wire=prefix+footer
        (directory/'native.stdout').write_bytes(wire);(directory/'native.stderr').write_bytes(b'')
        receipt={'command':command,'exit':0,'timeout_seconds':110,'timed_out':False,'pipe_capture_complete':True,
            'reader_errors':{},'output_overflow':False,'stdout_file':'native.stdout','stderr_file':'native.stderr',
            'stdout_bytes':len(wire),'stderr_bytes':0,'stdout_sha256':P.digest(wire),'stderr_sha256':P.digest(b''),
            'stdout_limit':4194304,'stderr_limit':1048576}
        def save(value):(directory/'native.capture.json').write_text(json.dumps(value),encoding='utf8')
        save(receipt);P.need(N.validate_transport(directory,receipt,command)==321,'native complete receipt positive control')
        for name,edit in (
            ('native failed child exit',lambda r:r.__setitem__('exit',1)),
            ('native timeout',lambda r:r.__setitem__('timed_out',True)),
            ('native incomplete pipes',lambda r:r.__setitem__('pipe_capture_complete',False)),
            ('native reader error',lambda r:r.__setitem__('reader_errors',{'stdout':['authored error']})),
            ('native overflow',lambda r:r.__setitem__('output_overflow',True)),
            ('native changed stream bytes',lambda r:r.__setitem__('stdout_sha256','0'*64)),
            ('native swapped command',lambda r:r['command'].__setitem__(-1,'ANOTHER_WORLD'))):
            changed=copy.deepcopy(receipt);edit(changed);save(changed)
            controls.append(rejected(lambda changed=changed:N.validate_transport(directory,changed,command),name))
        save(receipt);(directory/'native.stderr').write_bytes(b'authored failure\n')
        controls.append(rejected(lambda:N.validate_transport(directory,receipt,command),'native stderr changed after receipt'))
        (directory/'native.stderr').write_bytes(b'');save({**receipt,'exit':1})
        controls.append(rejected(lambda:N.validate_transport(directory,receipt,command),'native stored receipt changed'))
    print('PASS: '+str(len(controls))+' named-factory packet/native-transport corruption controls; no original runtime')
if __name__=='__main__':main()
