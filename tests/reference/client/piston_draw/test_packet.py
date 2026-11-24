"""Portable synthetic corruption controls; no original/native runtime runs."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('piston_public_packet', HERE / 'packet.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
spec = importlib.util.spec_from_file_location('piston_authored_worlds', HERE / 'write_world.py')
W = importlib.util.module_from_spec(spec)
spec.loader.exec_module(W)


def dump(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf8')


def edit(name, change):
    def action(root):
        value = P.read(root / name)
        change(value)
        dump(root / name, value)
    return action


def repair(root):
    value = P.read(root / 'publication.json')
    value['files'] = {n: P.pin(root / n) for n in sorted(P.FILES) if (root / n).is_file()}
    dump(root / 'publication.json', value)


BAD = []
def bad(name, action):
    BAD.append((name, action))


bad('missing_geometry', lambda r: (r / 'geometry.json').unlink())
bad('extra_private_material', lambda r: (r / 'client.jar').write_bytes(b'not-an-original-jar'))
bad('subdirectory', lambda r: (r / 'unpublished').mkdir())
bad('duplicate_json_key', lambda r: (r / 'cases.json').write_text('{"schema":1,"schema":2}'))
bad('changed_executed_java', lambda r: (r / 'PistonDrawObservation.java').write_text('class Changed {}'))
bad('changed_debugger', lambda r: (r / 'PistonDrawJdi.java').write_text('class Changed {}'))
bad('changed_authorer', lambda r: (r / 'write_world.py').write_text('raise RuntimeError()'))
bad('truncated_profiles', lambda r: (r / 'profiles.tsv').write_bytes((r / 'profiles.tsv').read_bytes()[:-1]))
bad('profiles_duplicate', lambda r: (r / 'profiles.tsv').write_bytes((r / 'profiles.tsv').read_bytes()*2))
bad('wrong_capture_count', edit('provenance.json', lambda v: v['independent_physical_audit'].__setitem__('processes', 2)))
bad('unrun_mode', edit('provenance.json', lambda v: v['streams'].pop('ordinary-default')))
bad('child_failure', edit('provenance.json', lambda v: v['streams']['ordinary-default'].__setitem__('exit', 1)))
bad('child_timeout', edit('provenance.json', lambda v: v['streams']['ordinary-default'].__setitem__('timed_out', True)))
bad('incomplete_pipe', edit('provenance.json', lambda v: v['streams']['ordinary-default'].__setitem__('pipe_capture_complete', False)))
bad('reader_failure', edit('provenance.json', lambda v: v['streams']['ordinary-default'].__setitem__('reader_errors', {'stdout':'failure'})))
bad('overflow', edit('provenance.json', lambda v: v['streams']['ordinary-default'].__setitem__('output_overflow', True)))
bad('jdi_child_failure', edit('provenance.json', lambda v: v['streams']['jdi-default']['target_status'].__setitem__('target_exit', 1)))
bad('missing_jdi_returns', edit('provenance.json', lambda v: v['streams']['jdi-default']['target_status'].__setitem__('returns', 26)))
bad('invented_jdi_batch', edit('provenance.json', lambda v: v['streams']['jdi-default']['target_status'].__setitem__('draws', 27)))
bad('vm_option', edit('provenance.json', lambda v: v['runtime']['target_options'].append('-Dextra=true')))
bad('uncleared_vm_environment', edit('provenance.json', lambda v: v['runtime']['cleared_environment'].pop()))
for field, value in [('boundary','ordinary FD.g gameplay'),('geometry','whole renderer parity'),('environment','visible monitor window')]:
    bad('scope_' + field, edit('provenance.json', lambda v, k=field, x=value: v['scope'].__setitem__(k,x)))
for claim in ['ordinary World tick','pixels','native parity','original save','archive hash treated as live Code']:
    bad('scope_removes_' + claim.replace(' ','_'), edit('provenance.json', lambda v, x=claim: v['scope']['excluded'].remove(x)))
bad('exclusion_assets', edit('provenance.json', lambda v: v['publication_excludes'].remove('raw assets/textures')))
bad('wrong_jar', edit('source-pins.json', lambda v: v.__setitem__('client_jar_sha256','0'*64)))
bad('removed_class', edit('source-pins.json', lambda v: v['classes'].pop('hy')))
bad('wrong_hy_code', edit('source-pins.json', lambda v: v['classes']['hy']['methods'][1].__setitem__('code_sha256','0'*64)))
bad('case_missing', edit('cases.json', lambda v: v['cases'].pop()))
bad('case_order', edit('cases.json', lambda v: v['cases'].reverse()))
bad('case_progress_input', edit('cases.json', lambda v: v['cases'][0].__setitem__('ticks', 3)))
bad('case_worldtime', edit('cases.json', lambda v: v.__setitem__('time', 9)))
bad('case_coordinate', edit('cases.json', lambda v: v['tile_xyz'].__setitem__(0,9)))
bad('world_pin', edit('authored-worlds.json', lambda v: next(iter(v['files'].values())).__setitem__('sha256','0'*64)))
bad('world_file_missing', edit('authored-worlds.json', lambda v: v['files'].pop('case00/level.dat')))
bad('world_payload_pin', edit('authored-worlds.json', lambda v: next(iter(v['payloads'].values())).__setitem__('sha256','0'*64)))
bad('missing_original_row', edit('observations.json', lambda v: v['observation']['rows'].pop()))
bad('changed_original_tile', edit('observations.json', lambda v: v['observation']['rows'][0]['after']['tile'].__setitem__('facing', 1)))
bad('changed_original_world', edit('observations.json', lambda v: v['observation']['rows'][0]['after'].__setitem__('world_time', 9)))
bad('original_failure', edit('observations.json', lambda v: v['observation']['rows'][0].__setitem__('returned', False)))
bad('original_gl_error', edit('observations.json', lambda v: v['observation']['rows'][0]['after']['gl'].__setitem__('error', 1280)))
bad('changed_live_code', edit('geometry.json', lambda v: v['events'][0].__setitem__('sha256','0'*64)))
bad('missing_call', edit('geometry.json', lambda v: v['events'].pop(30)))
bad('extra_call', edit('geometry.json', lambda v: v['events'].append(copy.deepcopy(v['events'][-1]))))
bad('missing_batch', edit('geometry.json', lambda v: v['events'].remove(next(e for e in v['events'] if e['event']=='nw-draw'))))
bad('batch_wrong_caller', edit('geometry.json', lambda v: next(e for e in v['events'] if e['event']=='nw-draw')['source_frames'][1].__setitem__('pc', 352)))
bad('batch_wrong_count', edit('geometry.json', lambda v: next(e for e in v['events'] if e['event']=='nw-draw')['state'].__setitem__('o', 8)))
bad('batch_changed_word', edit('geometry.json', lambda v: next(e for e in v['events'] if e['event']=='nw-draw')['words'].__setitem__(0,'00000000')))
bad('batch_truncated_word', edit('geometry.json', lambda v: next(e for e in v['events'] if e['event']=='nw-draw')['words'].pop()))
bad('native_wrong_count', edit('native-observations.json', lambda v: v['rows'][0].__setitem__('vertices', 1)))
bad('native_truncated', edit('native-observations.json', lambda v: v['rows'].pop()))
bad('native_word', edit('native-observations.json', lambda v: v['rows'][0]['words'].__setitem__(0,'ffffffff')))
bad('native_cull_fake_vertices', edit('native-observations.json', lambda v: v['rows'][19].__setitem__('vertices', 60)))
bad('native_retained_state', edit('native-observations.json', lambda v: v['rows'][0]['retained_state'].__setitem__('head_texture', 106)))


class PacketControls(unittest.TestCase):
    def test_positive_packet(self):
        value = P.read_packet()
        self.assertEqual(len(value['cases']), 27)
        self.assertEqual(sum(r['vertices'] > 0 for r in value['expected']), 24)

    def test_positive_authorer_complete_payloads_and_wire_report(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'saves'
            got = W.write_worlds(output)
            result = W.audit_worlds(output)
            self.assertTrue(result['payloads_exact'])
            self.assertEqual(result['wire_files'],got)
            self.assertEqual(result['wire_exact'],got==P.read(HERE/'authored-worlds.json')['files'])
            self.assertEqual(W.payload_pins(output),P.read(HERE/'authored-worlds.json')['payloads'])

    def test_authorer_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileExistsError):
                W.write_worlds(temp)

    def test_independent_authored_region_and_nbt(self):
        root = HERE.parents[3]
        spec = importlib.util.spec_from_file_location('piston_nbt_independent',
            root / 'tests/reference/world/furnace_save_readback/decode.py')
        N = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(N)
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'saves'
            W.write_worlds(output)
            for case in P.read(HERE / 'cases.json')['cases']:
                folder = output / case['folder']
                data = N.inflate((folder / 'level.dat').read_bytes(),31)['Data'][1]
                self.assertEqual(data['LevelName'], [8,'Original Piston draw %02d'%case['index']])
                self.assertEqual(data['RandomSeed'], [4,123])
                self.assertEqual(data['Time'], [4,8])
                region = (folder / 'region/r.0.0.mcr').read_bytes()
                used = []
                for x,z in ((0,0),(0,1),(1,0),(1,1)):
                    entry = struct.unpack_from('>I',region,4*(x+32*z))[0]
                    at=(entry>>8)*4096
                    length=struct.unpack_from('>I',region,at)[0]
                    self.assertEqual(region[at+4],2)
                    level=N.inflate(region[at+5:at+4+length],15)['Level'][1]
                    self.assertEqual((level['xPos'],level['zPos']),([3,x],[3,z]))
                    self.assertEqual(level['Entities'],[9,[10,[]]])
                    self.assertEqual(level['TileEntities'],[9,[10,[]]])
                    self.assertEqual(level['SkyLight'],[7,b'\xff'*16384])
                    self.assertEqual(level['BlockLight'],[7,bytes(16384)])
                    cell=(8<<11)|(8<<7)|64
                    self.assertEqual(level['Blocks'][1][cell],36 if (x,z)==(0,0) else 0)
                    if (x,z)==(0,0):self.assertEqual(level['Data'][1][cell//2]&15,case['metadata'])
                    used.append((entry>>8,entry&255))
                self.assertEqual(used,[(2,1),(3,1),(4,1),(5,1)])

    def test_authorer_rejects_valid_compression_changed_level_nbt(self):
        import gzip
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root)
            path=root/'case00/level.dat'
            raw=W._inflate(path.read_bytes(),31).replace(b'Original Piston draw 00',b'Original Piston draw 99')
            path.write_bytes(gzip.compress(raw,mtime=0))
            with self.assertRaisesRegex(ValueError,'uncompressed NBT'):
                W.audit_worlds(root)

    def test_authorer_reports_recompressed_wire_without_changing_payload(self):
        import gzip
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root)
            path=root/'case00/level.dat';before=path.read_bytes()
            path.write_bytes(gzip.compress(W._inflate(before,31),compresslevel=1,mtime=0))
            self.assertNotEqual(before,path.read_bytes())
            result=W.audit_worlds(root)
            self.assertTrue(result['payloads_exact'])
            self.assertFalse(result['wire_exact'])
            self.assertIn('case00/level.dat',result['wire_different_files'])

    def test_authorer_rejects_valid_compression_changed_block_array(self):
        import zlib
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root)
            path=root/'case00/region/r.0.0.mcr';region=bytearray(path.read_bytes());at=8192
            length=struct.unpack_from('>I',region,at)[0]
            raw=bytearray(W._inflate(region[at+5:at+4+length],15))
            start=raw.index(b'Blocks')+len(b'Blocks')+4
            raw[start+(8<<11)+(8<<7)+64]=1
            data=zlib.compress(bytes(raw));record=struct.pack('>I',len(data)+1)+b'\x02'+data
            region[at:at+4096]=record+bytes(4096-len(record));path.write_bytes(region)
            with self.assertRaisesRegex(ValueError,'uncompressed NBT'):
                W.audit_worlds(root)

    def test_authorer_rejects_trailing_compressed_stream(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root)
            path=root/'case00/level.dat';path.write_bytes(path.read_bytes()+b'extra')
            with self.assertRaisesRegex(ValueError,'trailing compressed'):
                W.audit_worlds(root)

    def test_authorer_rejects_region_ownership_and_padding(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'saves';W.write_worlds(root)
            path=root/'case00/region/r.0.0.mcr';raw=bytearray(path.read_bytes())
            raw[-1]=1;path.write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'Chunk record'):
                W.audit_worlds(root)


def make_test(action):
    def test(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'packet'
            shutil.copytree(HERE,root,ignore=shutil.ignore_patterns('__pycache__'))
            action(root)
            repair(root)
            with self.assertRaises((ValueError,KeyError,StopIteration,TypeError)):
                P.read_packet(root)
    return test


for name,action in BAD:
    setattr(PacketControls,'test_repaired_manifest_rejects_'+name,make_test(action))

if __name__=='__main__':
    unittest.main(verbosity=1)
