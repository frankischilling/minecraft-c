"""Portable synthetic corruptions and authored-input controls, no game runtime."""
import copy
import gzip
import importlib.util
import json
import re
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
import zlib

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
def module(n,p):
    s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
P=module('activation_public_packet',HERE/'packet.py');W=module('activation_public_inputs',HERE/'write_world.py')
def dump(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf8')
def edit(n,fn):
    def action(root):v=P.read(root/n);fn(v);dump(root/n,v)
    return action
def repair(root):
    m=P.read(root/'publication.json');m['files']={n:P.pin(root/n) for n in sorted(P.FILES) if (root/n).is_file()};dump(root/'publication.json',m)
BAD=[]
def bad(n,f):BAD.append((n,f))
bad('missing_stream_projection',lambda r:(r/'observations.json').unlink())
bad('unexpected_raw_jar',lambda r:(r/'client.jar').write_bytes(b'synthetic'))
bad('unexpected_directory',lambda r:(r/'private').mkdir())
bad('duplicate_json_key',lambda r:(r/'cases.json').write_text('{"schema":1,"schema":2}'))
bad('changed_executed_helper',lambda r:(r/'PistonActivationObservation.java').write_text('class Changed {}'))
bad('changed_json_helper',lambda r:(r/'ConstructorJson.java').write_text('class Changed {}'))
bad('changed_authorer',lambda r:(r/'write_world.py').write_text('raise RuntimeError()'))
bad('truncated_profiles',lambda r:(r/'profiles.tsv').write_bytes((r/'profiles.tsv').read_bytes()[:-1]))
bad('case_order',edit('cases.json',lambda v:v['cases'].reverse()))
bad('case_missing',edit('cases.json',lambda v:v['cases'].pop()))
bad('case_13cap_input',edit('cases.json',lambda v:v['cases'][3].__setitem__('count',12)))
bad('case_material',edit('cases.json',lambda v:v['cases'][4].__setitem__('carried',1)))
bad('case_constructor_seed',edit('cases.json',lambda v:v['constructor_inputs'].__setitem__('seed',23)))
bad('wrong_jar',edit('source-pins.json',lambda v:v.__setitem__('jar_sha256','0'*64)))
bad('removed_class',edit('source-pins.json',lambda v:v['classes'].pop('xr')))
bad('changed_code',edit('source-pins.json',lambda v:v['classes']['xr']['methods'][0].__setitem__('code_sha256','0'*64)))
bad('missing_mode',edit('observations.json',lambda v:v['modes'].pop('default')))
bad('missing_row',edit('observations.json',lambda v:v['modes']['default']['rows'].pop()))
bad('missing_phase',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'].pop(2)))
bad('phase_order',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'].reverse()))
bad('false_return',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1].__setitem__('returned',False)))
bad('wrong_activation_result',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1].__setitem__('activation_result',False)))
bad('world_time',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1]['state'].__setitem__('world_time',1)))
bad('saved_name_assumption',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][0]['state'].__setitem__('world_name','Publish')))
bad('wrong_seed',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][0]['state'].__setitem__('world_seed',-99)))
bad('fake_player',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][0]['state'].__setitem__('player_count',1)))
bad('cell_repair',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1]['state']['cells'][1].__setitem__(3,1)))
bad('tile_class',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1]['state']['retained'][0].__setitem__('class','ow')))
bad('progress_repair',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1]['state']['retained'][0]['piston'].__setitem__('current_bits','3f800000')))
bad('retired_identity',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][4]['state']['retained'][0].__setitem__('invalid',False)))
bad('tile_map_order',edit('observations.json',lambda v:v['modes']['default']['rows'][2]['steps'][1]['state']['map'].reverse()))
bad('tile_list_order',edit('observations.json',lambda v:v['modes']['default']['rows'][2]['steps'][1]['state']['active'].reverse()))
bad('chunk_hash',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][1]['state']['chunk_array_sha256'].__setitem__('Blocks','0'*64)))
bad('suppression_flag',edit('observations.json',lambda v:next(e for e in v['modes']['default']['rows'][0]['events'] if e['kind']=='sound')['state'].__setitem__('suppression',False)))
bad('missing_callback',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['events'].pop()))
bad('extra_callback',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['events'].append(copy.deepcopy(v['modes']['default']['rows'][0]['events'][-1]))))
bad('callback_stage',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['events'][0].__setitem__('stage','initial')))
bad('sound_name',edit('observations.json',lambda v:next(e for e in v['modes']['default']['rows'][0]['events'] if e['kind']=='sound').__setitem__('name','synthetic.sound')))
bad('sound_position',edit('observations.json',lambda v:next(e for e in v['modes']['default']['rows'][1]['events'] if e['kind']=='sound' and e['name']=='tile.piston.in')['position_bits'].__setitem__(2,'3ff8000000000000')))
bad('nan_sound',edit('observations.json',lambda v:next(e for e in v['modes']['default']['rows'][0]['events'] if e['kind']=='sound').__setitem__('pitch_bits','7fc00000')))
bad('invalid_random_seed',edit('observations.json',lambda v:v['modes']['default']['rows'][0]['steps'][0]['state']['world_random'].__setitem__('seed',1<<48)))
bad('native_table_changed',edit('native-observations.json',lambda v:v['rows'][0]['steps'][1]['state']['cells'][1].__setitem__(3,1)))
bad('wire_manifest_changed',edit('authored-worlds.json',lambda v:next(iter(v['files'].values())).__setitem__('sha256','0'*64)))
bad('payload_manifest_changed',edit('authored-worlds.json',lambda v:next(iter(v['payloads'].values())).__setitem__('sha256','0'*64)))
bad('transport_failure',edit('provenance.json',lambda v:v['streams']['default'].__setitem__('exit',1)))
bad('transport_timeout',edit('provenance.json',lambda v:v['streams']['default'].__setitem__('timed_out',True)))
bad('transport_incomplete',edit('provenance.json',lambda v:v['streams']['default'].__setitem__('pipe_capture_complete',False)))
bad('transport_reader_error',edit('provenance.json',lambda v:v['streams']['default'].__setitem__('reader_errors',{'stdout':'failed'})))
bad('transport_overflow',edit('provenance.json',lambda v:v['streams']['default'].__setitem__('output_overflow',True)))
bad('vm_options',edit('provenance.json',lambda v:v['runtime']['target_options'].append('-Dextra=true')))
bad('vm_environment',edit('provenance.json',lambda v:v['runtime']['cleared_environment'].pop()))
bad('source_call_count',edit('provenance.json',lambda v:v['independent_physical_audit'].__setitem__('direct_FD_g_per_process',32)))
for claim in P.EXCLUDED:
    bad('unsupported_'+re.sub('[^a-z0-9]+','_',claim.lower()),edit('provenance.json',lambda v,x=claim:v['scope']['excluded'].remove(x)))


class Controls(unittest.TestCase):
    def test_positive_packet(self):
        v=P.read_packet();self.assertEqual(len(v['cases']),6);self.assertEqual(sum(len(r['steps']) for r in v['expected']),48)
    def test_authorer_complete_payloads(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';actual=W.write_worlds(root);v=W.audit_worlds(root)
            self.assertTrue(v['payloads_exact']);self.assertEqual(v['wire_files'],actual)
            self.assertEqual(W.payload_pins(root),P.read(HERE/'authored-worlds.json')['payloads'])
    def test_authorer_no_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileExistsError):W.write_worlds(temp)
    def test_authorer_recompression_report(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root);p=root/'case00/level.dat';before=p.read_bytes()
            p.write_bytes(gzip.compress(W._inflate(before,31),compresslevel=1,mtime=0));v=W.audit_worlds(root)
            self.assertTrue(v['payloads_exact']);self.assertFalse(v['wire_exact']);self.assertIn('case00/level.dat',v['wire_different_files'])
    def test_changed_valid_compressed_level_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root);p=root/'case00/level.dat'
            p.write_bytes(gzip.compress(W._inflate(p.read_bytes(),31).replace(b'Publish',b'Changed'),mtime=0))
            with self.assertRaisesRegex(ValueError,'payload mismatch'):W.audit_worlds(root)
    def test_changed_valid_compressed_block_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root);p=root/'case00/region/r.0.0.mcr';r=bytearray(p.read_bytes());n=struct.unpack_from('>I',r,8192)[0];raw=bytearray(W._inflate(r[8197:8196+n],15));at=raw.index(b'Blocks')+len(b'Blocks')+4;raw[at+(8<<11)+(1<<7)+64]=1;compressed=zlib.compress(raw);record=struct.pack('>I',len(compressed)+1)+b'\x02'+compressed;r[8192:]=record+bytes(4096-len(record));p.write_bytes(r)
            with self.assertRaisesRegex(ValueError,'payload mismatch'):W.audit_worlds(root)
    def test_trailing_compressed_data(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root);p=root/'case00/level.dat';p.write_bytes(p.read_bytes()+b'extra')
            with self.assertRaisesRegex(ValueError,'trailing compressed'):W.audit_worlds(root)
    def test_region_timestamp_or_ownership(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root);p=root/'case00/region/r.0.0.mcr';r=bytearray(p.read_bytes());r[4096]=0;p.write_bytes(r)
            with self.assertRaisesRegex(ValueError,'ownership/timestamp'):W.audit_worlds(root)
    def test_independent_all_typed_inputs(self):
        N=module('activation_independent_nbt',HERE.parents[3]/'tests/reference/world/furnace_save_readback/decode.py')
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root)
            for c in P.read(HERE/'cases.json')['cases']:
                folder=root/c['folder'];info=N.inflate((folder/'level.dat').read_bytes(),31)['Data'][1]
                self.assertEqual(info['LevelName'],[8,'Publish']);self.assertEqual(info['RandomSeed'],[4,23]);self.assertEqual(info['Time'],[4,0])
                region=(folder/'region/r.0.0.mcr').read_bytes();n=struct.unpack_from('>I',region,8192)[0];level=N.inflate(region[8197:8196+n],15)['Level'][1]
                self.assertEqual(level['Entities'],[9,[10,[]]]);self.assertEqual(level['TileEntities'],[9,[10,[]]])
                self.assertEqual(level['SkyLight'],[7,b'\xff'*16384]);self.assertEqual(level['BlockLight'],[7,bytes(16384)])
                blocks=level['Blocks'][1];data=level['Data'][1]
                for z in range(16):
                    at=(8<<11)|(z<<7)|64;self.assertEqual(blocks[at],c['base'] if z==1 else c['carried'] if 2<=z<2+c['count'] else 0)
                    self.assertEqual(data[at//2]&15,3 if z==1 else 0)


def make(action):
    def test(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'packet';shutil.copytree(HERE,root,ignore=shutil.ignore_patterns('__pycache__'));action(root);repair(root)
            with self.assertRaises((ValueError,KeyError,TypeError,IndexError,StopIteration)):P.read_packet(root)
    return test
for name,action in BAD:setattr(Controls,'test_repaired_manifest_rejects_'+name,make(action))
if __name__=='__main__':unittest.main(verbosity=1)
