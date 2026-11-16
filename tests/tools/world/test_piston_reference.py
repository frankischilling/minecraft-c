"""Replay the finite original Piston activation projection with a native fixture.

--controls exercises synthetic reader inputs only; it launches no game runtime.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import sys
import tempfile
import traceback
import unittest

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[3]
PACKET=ROOT/'tests/reference/world/piston_activation'
FIXTURE=ROOT/'tests/world/runtime/piston/test_piston_reference.c'
SUPPORT=ROOT/'tests/world/runtime/test_entity_publication.c'
TRANSPORT=ROOT/'tests/reference/world/furnace_save_readback/transport.py'
DECODER=ROOT/'tests/reference/world/furnace_save_readback/decode.py'
TIMEOUT=150
OUTPUT_LIMIT=32*1024*1024
PLANES={'Blocks':32768,'Data':16384,'SkyLight':16384,'BlockLight':16384,'HeightMap':256}
EXCLUSIONS=['Chunk Map encounter-order equality; keyed coordinates and complete Tile identities are compared',
            'World Random state equality and Random draw-count claims',
            'Piston sound-pitch equality; actual bits are retained and checked for finite Source range',
            'full World ticks, original save/roundtrip, rendering and native failure-policy equivalence']


def require(value,message):
    if not value:raise ValueError(message)


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


def unique(pairs):
    out={}
    for key,value in pairs:
        require(key not in out,'duplicate JSON key: '+key);out[key]=value
    return out


def parse(raw):
    require(type(raw) is bytes and 0<len(raw)<=OUTPUT_LIMIT,'bounded complete native JSON file')
    return json.loads(raw.decode('utf8'),object_pairs_hook=unique,
                      parse_constant=lambda s:(_ for _ in ()).throw(ValueError('nonfinite JSON '+s)))


def pin(path):
    raw=Path(path).read_bytes();return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}


def write(path,value):
    Path(path).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')


def native_stdout(raw):
    require(type(raw) is bytes and raw.endswith(b'\n'),'complete native stdout footer line')
    canonical=raw.replace(b'\r\n',b'\n')
    require(b'\r' not in canonical and raw in (canonical,canonical.replace(b'\n',b'\r\n')),
            'coherent native LF or CRLF transport')
    lines=canonical[:-1].split(b'\n')
    # The frozen fixture initializes client and server Block Registries for
    # each of six cases. The existing nine-case run confirms this exact pair.
    require(lines[:-1]==[b'16 achievements',b'151 recipes']*12,
            'exact twelve ordered Registry initialization banner pairs')
    match=re.fullmatch(rb'PASS: Piston reference replay; cases=6; checks=([1-9][0-9]{0,9}); cleanup=1; source-field-repair=0',lines[-1])
    require(match is not None and int(match[1])<=4294967295,
            'unique terminal replay footer with positive uint32 check count')
    return int(match[1])


def validate_transport(directory,receipt,command):
    directory=Path(directory)
    require(receipt==parse((directory/'native.capture.json').read_bytes()),'exact retained native receipt')
    require(receipt.get('command')==command and receipt.get('stdout_file')=='native.stdout' and
            receipt.get('stderr_file')=='native.stderr','actual invoked command and raw pipe names')
    for key,value in [('exit',0),('timeout_seconds',TIMEOUT),('stdout_limit',4194304),('stderr_limit',1048576)]:
        require(type(receipt.get(key)) is int and receipt[key]==value,'exact capture '+key)
    require(receipt.get('timed_out') is False and receipt.get('output_overflow') is False and
            receipt.get('pipe_capture_complete') is True and receipt.get('reader_errors')=={},
            'complete joined successful native transport')
    for stream in ('stdout','stderr'):
        actual=pin(directory/receipt[stream+'_file'])
        require(type(receipt.get(stream+'_bytes')) is int and receipt[stream+'_bytes']==actual['bytes'] and
                receipt.get(stream+'_sha256')==actual['sha256'],'physical complete pipe hash/count '+stream)
    require(receipt['stderr_bytes']==0,'native stderr must be empty')
    return native_stdout((directory/'native.stdout').read_bytes())


def same(actual,expected,path):
    require(type(actual) is type(expected),path+': typed value mismatch')
    if type(expected) is dict:
        require(set(actual)==set(expected),path+': exact field census')
        for key in expected:same(actual[key],expected[key],path+'.'+key)
    elif type(expected) is list:
        require(len(actual)==len(expected),path+': exact ordered length')
        for i,(a,b) in enumerate(zip(actual,expected)):same(a,b,path+'['+str(i)+']')
    else:require(actual==expected,path+': value mismatch')


def _finite_bits(bits,width,path):
    require(type(bits) is str and re.fullmatch('[0-9a-f]{%d}'%(width//4),bits),path+': canonical raw bits')
    value=struct.unpack('>f' if width==32 else '>d',bytes.fromhex(bits))[0]
    require(math.isfinite(value),path+': finite Source numeric value');return value


def _state(actual,path,packet):
    require(type(actual) is dict and 'chunk_array_hex' in actual and 'chunk_array_sha256' not in actual,
            path+': native raw planes required, no supplied digest substitute')
    arrays=actual['chunk_array_hex'];require(type(arrays) is dict and set(arrays)==set(PLANES),path+': all five raw planes')
    raw={}
    for name,length in PLANES.items():
        text=arrays[name];require(type(text) is str and len(text)==length*2 and re.fullmatch('[0-9a-f]+',text),path+': complete canonical '+name)
        raw[name]=bytes.fromhex(text)
    out={k:v for k,v in actual.items() if k!='chunk_array_hex'}
    out['chunk_array_sha256']={k:hashlib.sha256(v).hexdigest() for k,v in raw.items()}
    packet._state(out)
    require([t['label'] for t in out['retained']]==['T'+str(i) for i in range(len(out['retained']))],
            path+': contiguous first-encounter retained identities')
    for cell in out['cells']:
        x,y,z,block,metadata=cell;require(0<=x<16 and 0<=z<16 and 0<=y<128,path+': bounded actual cell')
        at=(x<<11)|(z<<7)|y
        require(raw['Blocks'][at]==block and (raw['Data'][at//2]>>((at&1)*4))&15==metadata,
                path+': independently decoded cell matches raw planes')
    # LM.i is independent stored state. A genuine NBT-loaded Chunk can keep
    # i=0 while the loaded HeightMap minimum is64; compare the observed field
    # and the complete array digest separately, without recomputing either.
    random=out['world_random'];_finite_bits(random['gaussian_bits'],64,path+'.world_random.gaussian_bits')
    return out


def _map(entries,path):
    require(type(entries) is list,path+': typed Map entries');keyed={};order=[]
    for entry in entries:
        require(type(entry) is dict and set(entry)=={'key_class','key_fields','tile'} and
                entry['key_class']=='wf' and type(entry['key_fields']) is dict and
                set(entry['key_fields'])=={'a','b','c'},path+': exact actual coordinate key fields')
        key=tuple(entry['key_fields'][k] for k in ('a','b','c'))
        require(all(type(n) is int for n in key) and key not in keyed,path+': unique typed coordinate keys')
        require(list(key)==entry['tile']['xyz'],path+': actual Map coordinate owns that Tile')
        keyed[key]=entry;order.append({'xyz':list(key),'tile':entry['tile']['label']})
    return keyed,order


def _state_compare(actual,expected,path,map_orders):
    a={k:v for k,v in actual.items() if k not in ('map','world_random')}
    b={k:v for k,v in expected.items() if k not in ('map','world_random')}
    same(a,b,path)
    ak,ao=_map(actual['map'],path+'.native_map');bk,bo=_map(expected['map'],path+'.original_map')
    require(set(ak)==set(bk),path+': Map keyed coordinate membership')
    for key in bk:same(ak[key],bk[key],path+'.map'+str(key))
    map_orders.append({'location':path,'native':ao,'original':bo,'encounter_order_equal':ao==bo,
                       'comparison':'keyed complete coordinate-to-Tile contents; encounter-order equality excluded'})


def compare_document(document,reference,packet):
    require(type(document) is dict and set(document)=={'schema','rows','cleanup','source_field_repairs'} and
            document['schema']=='NATIVE_PISTON_ACTIVATION_REPLAY_V1' and document['cleanup'] is True and
            type(document['source_field_repairs']) is int and document['source_field_repairs']==0,
            'complete native replay top-level terminal record')
    require(type(document['rows']) is list and len(document['rows'])==len(reference)==6,'all six native replay cases')
    maps=[];entropy=[];planes=0;steps=0;callbacks=0;sounds=0
    for row,expected in zip(document['rows'],reference):
        require(type(row) is dict and set(row)==set(expected)=={'index','profile','steps','events'},'closed native case record')
        path='case'+str(expected['index']);same(row['index'],expected['index'],path+'.index');same(row['profile'],expected['profile'],path+'.profile')
        require(type(row['steps']) is list and len(row['steps'])==len(expected['steps']),path+': every declared phase')
        for i,(step,want) in enumerate(zip(row['steps'],expected['steps'])):
            location=path+'.steps['+str(i)+']';require(type(step) is dict and set(step)==set(want),location+': exact step fields')
            same({k:v for k,v in step.items() if k!='state'},{k:v for k,v in want.items() if k!='state'},location)
            state=_state(step['state'],location+'.state',packet);_state_compare(state,want['state'],location+'.state',maps)
            entropy.append({'location':location+'.state','world_random':state['world_random']});planes+=5;steps+=1
        require(type(row['events']) is list and len(row['events'])==len(expected['events']),path+': complete ordered callbacks')
        for i,(event,want) in enumerate(zip(row['events'],expected['events'])):
            location=path+'.events['+str(i)+']';require(type(event) is dict,location+': typed callback')
            if want['kind']=='sound':
                require(set(event)==set(want)|{'pitch_bits'},location+': full actual sound record and raw pitch')
                pitch=_finite_bits(event['pitch_bits'],32,location+'.pitch_bits')
                # An admissible range from the original source formula, not
                # a reconstructed draw or a claim of matching Random state.
                if want['name'] in ('tile.piston.out','tile.piston.in'):
                    require(0.6000000238418579<=pitch<=(0.8500000238418579 if want['name'].endswith('out') else 0.75),location+': Source Piston pitch envelope')
                metadata={k:v for k,v in event.items() if k not in ('state','pitch_bits')}
                expected_metadata={k:v for k,v in want.items() if k not in ('state','pitch_bits')}
                same(metadata,expected_metadata,location)
                if 'pitch_bits' in want:same(event['pitch_bits'],want['pitch_bits'],location+'.fixed_click_pitch')
                state=_state(event['state'],location+'.state',packet);_state_compare(state,want['state'],location+'.state',maps)
                entropy.append({'location':location,'name':event['name'],'pitch_bits':event['pitch_bits'],'world_random':state['world_random']});planes+=5;sounds+=1
            else:same(event,want,location)
            callbacks+=1
    return {'cases':6,'phase_snapshots':steps,'callbacks':callbacks,'sound_callbacks':sounds,
            'raw_planes_independently_hashed':planes,'native_entropy':entropy,'map_orders':maps,
            'map_order_differences':[m for m in maps if not m['encounter_order_equal']],
            'exclusions':EXCLUSIONS}


def empty_nbt_list(value,path):
    # An empty List's unused element-type byte is not Entity membership.
    require(type(value) is list and len(value)==2 and type(value[0]) is int and value[0]==9 and
            type(value[1]) is list and len(value[1])==2 and type(value[1][0]) is int and
            0<=value[1][0]<=10 and type(value[1][1]) is list and len(value[1][1])==0,
            path+': valid empty NBT List required')
    return value[1][0]


def disk_inputs(root,declaration,reference,decoder):
    root=Path(root);files={p.relative_to(root).as_posix():pin(p) for p in root.rglob('*') if p.is_file()}
    expected_names={c['folder']+'/world0/'+n for c in declaration['cases'] for n in
                    ('level.dat','session.lock','region/r.0.0.mcr')}
    require(set(files)==expected_names,'exact native-authored stable saved input file census')
    checked=[]
    for case,row in zip(declaration['cases'],reference):
        folder=root/case['folder']/'world0';info=decoder.inflate((folder/'level.dat').read_bytes(),31)
        types={'LevelName':8,'RandomSeed':4,'Time':4,'LastPlayed':4,'SizeOnDisk':4,'raining':1,'thundering':1}
        expected={'Data':[10,{k:[types.get(k,3),v] for k,v in declaration['serialized_metadata'].items()}]}
        same(info,expected,case['folder']+'.saved_metadata')
        require((folder/'session.lock').read_bytes()==(137).to_bytes(8,'big',signed=True),'declared native fixture SessionLock token only')
        raw=(folder/'region/r.0.0.mcr').read_bytes();require(len(raw)%4096==0 and len(raw)>=12288,'complete native region sectors')
        entry=struct.unpack_from('>I',raw,0)[0];offset=entry>>8;count=entry&255
        require(offset>=2 and count>0 and offset+count==len(raw)//4096 and not any(raw[4:4096]) and
                struct.unpack_from('>I',raw,4096)[0]==0xfedcba98 and not any(raw[4100:8192]),'single native authored Chunk ownership/timestamp')
        at=offset*4096;n=struct.unpack_from('>I',raw,at)[0]
        require(1<n<=count*4096-4 and raw[at+4]==2,'bounded native compressed Chunk record')
        chunk=decoder.inflate(raw[at+5:at+4+n],15)
        require(type(chunk) is dict and set(chunk)=={'Level'} and chunk['Level'][0]==10,'typed native Chunk root')
        level=chunk['Level'][1]
        require(set(level)=={'xPos','zPos','LastUpdate','Blocks','Data','SkyLight','BlockLight','HeightMap','TerrainPopulated','Entities','TileEntities'},'complete declared Chunk field census')
        for key,value in {'xPos':[3,0],'zPos':[3,0],'LastUpdate':[4,0],'TerrainPopulated':[1,1]}.items():same(level[key],value,case['folder']+'.'+key)
        children={key:empty_nbt_list(level[key],case['folder']+'.'+key) for key in ('Entities','TileEntities')}
        for name,length in PLANES.items():
            require(level[name][0]==7 and type(level[name][1]) is bytes and len(level[name][1])==length,
                    'whole native authored '+name)
            require(hashlib.sha256(level[name][1]).hexdigest()==row['steps'][0]['state']['chunk_array_sha256'][name],
                    'complete typed native input plane equals actual original initial '+name)
        checked.append({'case':case['index'],'metadata_typed_exact':True,'initial_planes_exact':True,
                        'entities_empty':True,'tiles_empty':True,'empty_list_child_types':children,
                        'unused_empty_list_child_type_compared':False,'compression_bytes_compared':False})
    return {'files':files,'cases':checked,'scope':'C-authored stable inputs remain on disk after cleanup; no native or original save/roundtrip claim'}


def selected_inputs(executable):
    paths=[Path(executable),Path(__file__).resolve(),FIXTURE,SUPPORT,TRANSPORT,DECODER,*[p for p in PACKET.iterdir() if p.is_file()]]
    paths.extend(p for directory in ('src','include') for p in (ROOT/directory).rglob('*') if p.is_file())
    return {str(p):pin(p) for p in sorted(set(paths))}


def run(args):
    executable=args.executable.resolve(strict=True);require(executable.is_file(),'native executable file')
    packet=load('piston_replay_original_packet',PACKET/'packet.py');admitted=packet.read_packet()
    decoder=load('piston_replay_nbt_decoder',DECODER);transport=load('piston_replay_capture',TRANSPORT)
    args.work_dir.mkdir(parents=True,exist_ok=True);out=Path(tempfile.mkdtemp(prefix='piston-reference-',dir=args.work_dir.resolve()))
    worlds=out/'worlds';worlds.mkdir();product=out/'native.json'
    command=[str(executable),str(worlds).encode('utf8').hex(),str(product)]
    before=selected_inputs(executable);report={'schema':'NATIVE_PISTON_REFERENCE_RUN_V1','status':'FAIL','command':command,
        'inputs_before':before,'publication':pin(PACKET/'publication.json'),'execution_inputs':{'world_root_initially_empty':not any(worlds.iterdir()),'native_output_initially_absent':not product.exists(),'expected_state_passed':False,'captured_RNG_passed':False},'comparison_exclusions':EXCLUSIONS}
    try:
        require(report['execution_inputs']['world_root_initially_empty'] and report['execution_inputs']['native_output_initially_absent'],'fresh owned native execution outputs')
        report['process']=transport.capture(command,out,'native',TIMEOUT,working_directory=out)
        report['checks']=validate_transport(out,report['process'],command)
        require(product.is_file() and not product.is_symlink() and 0<product.stat().st_size<=OUTPUT_LIMIT,'complete bounded real native JSON product')
        report['native_product']=pin(product);document=parse(product.read_bytes())
        report['comparison']=compare_document(document,admitted['expected'],packet)
        require(report['comparison']['phase_snapshots']==48 and report['comparison']['callbacks']==94 and report['comparison']['sound_callbacks']==15 and report['comparison']['raw_planes_independently_hashed']==315,'complete original finite comparison census')
        report['native_authored_inputs']=disk_inputs(worlds,admitted['declaration'],admitted['expected'],decoder)
        original=packet.read(PACKET/'observations.json')['modes']
        report['original_entropy']={mode:[{'case':r['index'],'steps':[{'stage':s['stage'],'world_random':s['state']['world_random']} for s in r['steps']],'sounds':[{'index':e['index'],'name':e['name'],'pitch_bits':e['pitch_bits'],'world_random':e['state']['world_random']} for e in r['events'] if e['kind']=='sound']} for r in observed['rows']] for mode,observed in original.items()}
        report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)};report['traceback']=traceback.format_exc()
    finally:
        try:
            report['inputs_after']=selected_inputs(executable);report['inputs_unchanged']=before==report['inputs_after']
            require(report['inputs_unchanged'],'native executable, complete Source and public packet unchanged')
            packet.read_packet();report['packet_unchanged']=True
            if 'native_product' in report:require(pin(product)==report['native_product'],'native JSON product unchanged after independent readback')
        except BaseException as error:
            report['status']='FAIL';report['continuity_error']={'type':type(error).__name__,'message':str(error)}
        report['retained_files']={p.relative_to(out).as_posix():pin(p) for p in out.rglob('*') if p.is_file()}
        write(out/'run.json',report);print('Native Piston reference artifacts: '+str(out),flush=True)
    require(report['status']=='PASS','native Piston reference FAIL; retained '+str(out))
    differences=len(report['comparison']['map_order_differences'])
    print('PASS: Piston reference comparison; cases=6; phases=48; callbacks=94; map-order-excluded=1; map-order-differences='+str(differences))


def controls():
    """Synthetic native-shaped reader values, never execution observations."""
    packet=load('piston_replay_controls_packet',PACKET/'packet.py')
    original=packet.read_packet()['expected']
    # Replace every plane digest with a clearly authored control plane. This
    # creates reader test inputs, not a claim that a native fixture ran.
    reference=copy.deepcopy(original);raw={name:bytes(length) for name,length in PLANES.items()}
    for name in ('Blocks','Data','HeightMap'):
        # Populate input-specific cells below; all control states stay synthetic.
        raw[name]=bytes(PLANES[name])
    def synthetic(state):
        arrays={name:bytearray(value) for name,value in raw.items()}
        for x,y,z,block,metadata in state['cells']:
            at=(x<<11)|(z<<7)|y;arrays['Blocks'][at]=block;shift=(at&1)*4;arrays['Data'][at//2]=(arrays['Data'][at//2]&~(15<<shift))|(metadata<<shift)
        arrays['HeightMap'][:]=bytes([state['chunk_flags']['minimum_height']])*256
        state['chunk_array_sha256']={n:hashlib.sha256(v).hexdigest() for n,v in arrays.items()}
        return {n:bytes(v).hex() for n,v in arrays.items()}
    native={'schema':'NATIVE_PISTON_ACTIVATION_REPLAY_V1','rows':copy.deepcopy(reference),'cleanup':True,'source_field_repairs':0}
    for r,want in zip(native['rows'],reference):
        for collection in ('steps','events'):
            for record,expected in zip(r[collection],want[collection]):
                if 'state' in record:
                    arrays=synthetic(expected['state']);record['state'].pop('chunk_array_sha256');record['state']['chunk_array_hex']=arrays;record['state']['world_random']={'seed':0,'cached':False,'gaussian_bits':'0000000000000000'}
                if record.get('kind')=='sound' and 'pitch_bits' not in record:record['pitch_bits']='3f333333'
    stdout=b'16 achievements\n151 recipes\n'*12+b'PASS: Piston reference replay; cases=6; checks=123; cleanup=1; source-field-repair=0\n'
    def authored_disk(temp):
        writer=load('piston_replay_synthetic_input_writer',PACKET/'write_world.py')
        declaration=packet.read_packet()['declaration'];source=Path(temp)/'authored';writer.write_worlds(source)
        destination=Path(temp)/'native-shaped-inputs';destination.mkdir()
        import shutil
        for case in declaration['cases']:
            folder=destination/case['folder']/'world0';shutil.copytree(source/case['folder'],folder)
            (folder/'session.lock').write_bytes((137).to_bytes(8,'big',signed=True))
        return destination,declaration,packet.read_packet()['expected'],writer,load('piston_replay_synthetic_nbt_reader',DECODER)
    def loaded_height_state(temp):
        root,declaration,expected,writer,decoder=authored_disk(temp)
        region=(root/'case00/world0/region/r.0.0.mcr').read_bytes();n=struct.unpack_from('>I',region,8192)[0]
        level=decoder.inflate(region[8197:8196+n],15)['Level'][1]
        raw={name:level[name][1] for name in PLANES}
        wanted=copy.deepcopy(expected[0]['steps'][0]['state'])
        actual=copy.deepcopy(wanted);actual.pop('chunk_array_sha256')
        actual['chunk_array_hex']={name:value.hex() for name,value in raw.items()}
        actual['world_random']={'seed':0,'cached':False,'gaussian_bits':'0000000000000000'}
        return actual,wanted,raw
    class ReaderControls(unittest.TestCase):
        def test_synthetic_reader_only_positive(self):
            result=compare_document(native,reference,packet);self.assertEqual(result['raw_planes_independently_hashed'],315)
        def test_footer_lf_crlf(self):
            self.assertEqual(native_stdout(stdout),123);self.assertEqual(native_stdout(stdout.replace(b'\n',b'\r\n')),123)
        def test_map_order_explicitly_excluded(self):
            value=copy.deepcopy(native);value['rows'][0]['steps'][1]['state']['map'].reverse()
            result=compare_document(value,reference,packet);self.assertEqual(len(result['map_order_differences']),1)
        def test_no_nonfinite_json_or_duplicate_keys(self):
            for raw in (b'{"x":1,"x":2}',b'{"x":NaN}',b'{}extra',b''):
                with self.assertRaises((ValueError,json.JSONDecodeError)):parse(raw)
        def test_synthetic_transport_reader_only(self):
            with tempfile.TemporaryDirectory() as temp:
                directory=Path(temp);command=['synthetic-native','726f6f74','native.json']
                (directory/'native.stdout').write_bytes(stdout);(directory/'native.stderr').write_bytes(b'')
                receipt={'command':command,'exit':0,'timeout_seconds':TIMEOUT,'timed_out':False,
                         'pipe_capture_complete':True,'reader_errors':{},'output_overflow':False,
                         'stdout_file':'native.stdout','stderr_file':'native.stderr',
                         'stdout_bytes':len(stdout),'stderr_bytes':0,'stdout_limit':4194304,
                         'stderr_limit':1048576,'stdout_sha256':hashlib.sha256(stdout).hexdigest(),
                         'stderr_sha256':hashlib.sha256(b'').hexdigest()}
                write(directory/'native.capture.json',receipt)
                self.assertEqual(validate_transport(directory,receipt,command),123)
        def test_synthetic_saved_input_gate_only(self):
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp)
                result=disk_inputs(root,declaration,expected,decoder)
                self.assertEqual(len(result['cases']),6);self.assertEqual(len(result['files']),18)
        def test_saved_metadata_compound_order_and_compression_not_identity(self):
            import gzip
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp);path=root/'case00/world0/level.dat'
                tag=decoder.inflate(path.read_bytes(),31);tag['Data'][1]=dict(reversed(list(tag['Data'][1].items())))
                path.write_bytes(gzip.compress(writer.nbt(tag),compresslevel=1,mtime=0))
                self.assertEqual(len(disk_inputs(root,declaration,expected,decoder)['cases']),6)
        def test_saved_metadata_corruption(self):
            import gzip
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp);path=root/'case00/world0/level.dat'
                tag=decoder.inflate(path.read_bytes(),31);tag['Data'][1]['RandomSeed']=[4,24]
                path.write_bytes(gzip.compress(writer.nbt(tag),mtime=0))
                with self.assertRaises(ValueError):disk_inputs(root,declaration,expected,decoder)
        def test_saved_block_plane_corruption(self):
            import zlib
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp);path=root/'case00/world0/region/r.0.0.mcr';region=bytearray(path.read_bytes());at=8192;n=struct.unpack_from('>I',region,at)[0]
                tag=decoder.inflate(region[at+5:at+4+n],15);blocks=bytearray(tag['Level'][1]['Blocks'][1]);blocks[(8<<11)|(1<<7)|64]=1;tag['Level'][1]['Blocks'][1]=bytes(blocks)
                payload=zlib.compress(writer.nbt(tag));record=struct.pack('>I',len(payload)+1)+b'\x02'+payload;region[at:]=record+bytes(4096-len(record));path.write_bytes(region)
                with self.assertRaises(ValueError):disk_inputs(root,declaration,expected,decoder)
        def test_saved_fixture_session_token(self):
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp);(root/'case00/world0/session.lock').write_bytes(bytes(8))
                with self.assertRaises(ValueError):disk_inputs(root,declaration,expected,decoder)
        def test_observed_loaded_minimum_height_is_independent_of_array_minimum(self):
            with tempfile.TemporaryDirectory() as temp:
                actual,wanted,raw=loaded_height_state(temp)
                self.assertEqual(min(raw['HeightMap']),64)
                self.assertEqual(wanted['chunk_flags']['minimum_height'],0)
                normalised=_state(actual,'synthetic-observed-loaded-state',packet)
                _state_compare(normalised,wanted,'synthetic-observed-loaded-state',[])
        def test_stored_minimum_height_still_compared_exactly(self):
            with tempfile.TemporaryDirectory() as temp:
                actual,wanted,raw=loaded_height_state(temp)
                actual['chunk_flags']['minimum_height']=64
                normalised=_state(actual,'synthetic-wrong-stored-minimum',packet)
                with self.assertRaisesRegex(ValueError,'minimum_height'):
                    _state_compare(normalised,wanted,'synthetic-wrong-stored-minimum',[])
        def test_heightmap_digest_still_compared_independently(self):
            with tempfile.TemporaryDirectory() as temp:
                actual,wanted,raw=loaded_height_state(temp)
                actual['chunk_array_hex']['HeightMap']=bytes([65]*256).hex()
                normalised=_state(actual,'synthetic-wrong-heightmap',packet)
                with self.assertRaisesRegex(ValueError,'HeightMap'):
                    _state_compare(normalised,wanted,'synthetic-wrong-heightmap',[])
        def test_valid_empty_nbt_list_children_keep_no_membership(self):
            for child in range(11):
                self.assertEqual(empty_nbt_list([9,[child,[]]],'synthetic-empty'),child)
        def test_invalid_or_nonempty_nbt_lists_rejected(self):
            for value in ([9,[-1,[]]],[9,[11,[]]],[9,[True,[]]],[9,[1,[1]]],
                          [9,[10,[{}]]],[7,[10,[]]],[9,[10,b'']],[9,[10]]):
                with self.assertRaises(ValueError):empty_nbt_list(value,'synthetic-invalid')
        def test_saved_empty_byte_lists_are_semantically_empty(self):
            import zlib
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp);path=root/'case00/world0/region/r.0.0.mcr';region=bytearray(path.read_bytes());n=struct.unpack_from('>I',region,8192)[0]
                tag=decoder.inflate(region[8197:8196+n],15)
                for key in ('Entities','TileEntities'):tag['Level'][1][key]=[9,[1,[]]]
                payload=zlib.compress(writer.nbt(tag));record=struct.pack('>I',len(payload)+1)+b'\x02'+payload;region[8192:]=record+bytes(4096-len(record));path.write_bytes(region)
                checked=disk_inputs(root,declaration,expected,decoder)
                self.assertEqual(checked['cases'][0]['empty_list_child_types'],{'Entities':1,'TileEntities':1})
        def test_saved_nonempty_entity_list_is_not_accepted(self):
            import zlib
            with tempfile.TemporaryDirectory() as temp:
                root,declaration,expected,writer,decoder=authored_disk(temp);path=root/'case00/world0/region/r.0.0.mcr';region=bytearray(path.read_bytes());n=struct.unpack_from('>I',region,8192)[0]
                tag=decoder.inflate(region[8197:8196+n],15);tag['Level'][1]['Entities']=[9,[10,[{}]]]
                payload=zlib.compress(writer.nbt(tag));record=struct.pack('>I',len(payload)+1)+b'\x02'+payload;region[8192:]=record+bytes(4096-len(record));path.write_bytes(region)
                with self.assertRaisesRegex(ValueError,'valid empty NBT List'):
                    disk_inputs(root,declaration,expected,decoder)
        def test_source_retract_pitch_endpoint_admitted_without_entropy_equality(self):
            value=copy.deepcopy(native)
            event=next(e for e in value['rows'][0]['events'] if e['kind']=='sound' and e['name']=='tile.piston.in')
            event['pitch_bits']='3f400000'
            self.assertEqual(compare_document(value,reference,packet)['cases'],6)
        def test_next_float_above_source_retract_pitch_envelope_rejected(self):
            value=copy.deepcopy(native)
            event=next(e for e in value['rows'][0]['events'] if e['kind']=='sound' and e['name']=='tile.piston.in')
            event['pitch_bits']='3f400001'
            with self.assertRaisesRegex(ValueError,'pitch envelope'):
                compare_document(value,reference,packet)
    negatives=[('missing_case',lambda v:v['rows'].pop()),('case_order',lambda v:v['rows'].reverse()),
        ('phase_missing',lambda v:v['rows'][0]['steps'].pop()),('phase_order',lambda v:v['rows'][0]['steps'].reverse()),
        ('cleanup',lambda v:v.__setitem__('cleanup',False)),('field_repair',lambda v:v.__setitem__('source_field_repairs',1)),
        ('typed_field_repair',lambda v:v.__setitem__('source_field_repairs',False)),
        ('world_time',lambda v:v['rows'][0]['steps'][0]['state'].__setitem__('world_time',1)),
        ('typed_world_time',lambda v:v['rows'][0]['steps'][0]['state'].__setitem__('world_time',False)),
        ('seed',lambda v:v['rows'][0]['steps'][0]['state'].__setitem__('world_seed',-99)),
        ('name',lambda v:v['rows'][0]['steps'][0]['state'].__setitem__('world_name','Publish')),
        ('player',lambda v:v['rows'][0]['steps'][0]['state'].__setitem__('player_count',1)),
        ('cell',lambda v:v['rows'][0]['steps'][1]['state']['cells'][1].__setitem__(3,1)),
        ('raw_plane_truncate',lambda v:v['rows'][0]['steps'][1]['state']['chunk_array_hex'].__setitem__('SkyLight','00')),
        ('raw_plane_corrupt',lambda v:v['rows'][0]['steps'][1]['state']['chunk_array_hex'].__setitem__('SkyLight','01'*16384)),
        ('digest_substitute',lambda v:v['rows'][0]['steps'][1]['state'].__setitem__('chunk_array_sha256',{})),
        ('active_order',lambda v:v['rows'][2]['steps'][1]['state']['active'].reverse()),
        ('root_order',lambda v:v['rows'][2]['steps'][1]['state']['retained'].reverse()),
        ('retired',lambda v:v['rows'][0]['steps'][4]['state']['retained'][0].__setitem__('invalid',False)),
        ('progress',lambda v:v['rows'][0]['steps'][1]['state']['retained'][0]['piston'].__setitem__('current_bits','3f800000')),
        ('map_duplicate',lambda v:v['rows'][0]['steps'][1]['state']['map'].append(copy.deepcopy(v['rows'][0]['steps'][1]['state']['map'][0]))),
        ('map_identity',lambda v:v['rows'][0]['steps'][1]['state']['map'][0]['tile'].__setitem__('label','T1')),
        ('map_coordinate',lambda v:v['rows'][0]['steps'][1]['state']['map'][0]['key_fields'].__setitem__('c',9)),
        ('callback_missing',lambda v:v['rows'][0]['events'].pop()),('callback_order',lambda v:v['rows'][0]['events'].reverse()),
        ('callback_stage',lambda v:v['rows'][0]['events'][0].__setitem__('stage','quiet-2')),
        ('callback_range',lambda v:v['rows'][0]['steps'][1].__setitem__('event_end',0)),
        ('random_seed_range',lambda v:v['rows'][0]['steps'][0]['state']['world_random'].__setitem__('seed',1<<48)),
        ('nan_gaussian',lambda v:v['rows'][0]['steps'][0]['state']['world_random'].__setitem__('gaussian_bits','7ff8000000000000'))]
    for name,change in negatives:
        def test(self,change=change):
            value=copy.deepcopy(native);change(value)
            with self.assertRaises((ValueError,KeyError)):compare_document(value,reference,packet)
        setattr(ReaderControls,'test_reject_'+name,test)
    for name,key,value in [('exit','exit',1),('typedexit','exit',False),
                           ('timeout','timed_out',True),('unfinished','pipe_capture_complete',False),
                           ('overflow','output_overflow',True),('reader','reader_errors',{'stdout':['failed']}),
                           ('deadline','timeout_seconds',1),('limit','stdout_limit',1),
                           ('count','stdout_bytes',0),('hash','stdout_sha256','0'*64),
                           ('command','command',['other','726f6f74','native.json'])]:
        def test(self,key=key,value=value):
            with tempfile.TemporaryDirectory() as temp:
                directory=Path(temp);command=['synthetic-native','726f6f74','native.json']
                (directory/'native.stdout').write_bytes(stdout);(directory/'native.stderr').write_bytes(b'')
                receipt={'command':command,'exit':0,'timeout_seconds':TIMEOUT,'timed_out':False,
                         'pipe_capture_complete':True,'reader_errors':{},'output_overflow':False,
                         'stdout_file':'native.stdout','stderr_file':'native.stderr',
                         'stdout_bytes':len(stdout),'stderr_bytes':0,'stdout_limit':4194304,
                         'stderr_limit':1048576,'stdout_sha256':hashlib.sha256(stdout).hexdigest(),
                         'stderr_sha256':hashlib.sha256(b'').hexdigest()}
                receipt[key]=value;write(directory/'native.capture.json',receipt)
                with self.assertRaises(ValueError):validate_transport(directory,receipt,command)
        setattr(ReaderControls,'test_transport_reject_'+name,test)
    for name,change in [('name',lambda e:e.__setitem__('name','synthetic.sound')),
                        ('position',lambda e:e['position_bits'].__setitem__(0,'0000000000000000')),
                        ('volume',lambda e:e.__setitem__('volume_bits','3f800000')),
                        ('nonfinite_pitch',lambda e:e.__setitem__('pitch_bits','7fc00000')),
                        ('pitch_range',lambda e:e.__setitem__('pitch_bits','42c80000'))]:
        def test(self,change=change):
            value=copy.deepcopy(native);event=next(e for e in value['rows'][0]['events'] if e['kind']=='sound');change(event)
            with self.assertRaises(ValueError):compare_document(value,reference,packet)
        setattr(ReaderControls,'test_sound_reject_'+name,test)
    for name,old,new in [('zero',b'123',b'0'),('negative',b'123',b'-1'),('leadingzero',b'123',b'0123'),
                         ('overflow',b'123',b'4294967296'),('decimal',b'123',b'1.0'),
                         ('wrongcases',b'cases=6',b'cases=5'),('wrongcleanup',b'cleanup=1',b'cleanup=0'),
                         ('repairs',b'repair=0',b'repair=1')]:
        def test(self,old=old,new=new):
            with self.assertRaises(ValueError):native_stdout(stdout.replace(old,new))
        setattr(ReaderControls,'test_footer_reject_'+name,test)
    for name,transform in [('missing_banner',lambda b:b.split(b'\n',2)[2]),('extra_banner',lambda b:b'16 achievements\n151 recipes\n'+b),
                           ('swapped_banner',lambda b:b.replace(b'16 achievements\n151 recipes',b'151 recipes\n16 achievements',1)),
                           ('duplicate_footer',lambda b:b+b.splitlines(keepends=True)[-1]),('trailing',lambda b:b+b'extra\n'),
                           ('missing_newline',lambda b:b[:-1]),('mixed_newlines',lambda b:b.replace(b'\n',b'\r\n',1))]:
        def test(self,transform=transform):
            with self.assertRaises(ValueError):native_stdout(transform(stdout))
        setattr(ReaderControls,'test_stdout_reject_'+name,test)
    result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(ReaderControls))
    require(result.wasSuccessful(),'synthetic Piston reader-only controls')
    print('PASS: '+str(result.testsRun)+' synthetic Piston replay reader controls; native-executions=0; original-executions=0')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--controls',action='store_true');parser.add_argument('--executable',type=Path);parser.add_argument('--work-dir',type=Path);args=parser.parse_args()
    if args.controls:
        require(args.executable is None and args.work_dir is None,'reader controls accept no native execution inputs');controls()
    else:
        require(args.executable is not None and args.work_dir is not None,'--executable and --work-dir required');run(args)


if __name__=='__main__':main()
