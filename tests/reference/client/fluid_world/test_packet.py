"""Corruption controls for frozen source data and independent effect validators."""
import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from packet import HERE, FROZEN, CAPTURED, read_packet, validate_rows, validate_events, lines


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_packet()

    def clone(self):
        temp = tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=Path(temp.name)
        for name in (*FROZEN,*CAPTURED,'provenance.json'):
            shutil.copyfile(HERE/name,root/name)
        return root

    def mutate_event(self,select,mutate,reason):
        cohort=next(c for c in self.packet['cohorts'] if any(select(e) for e in c['events']))
        events=copy.deepcopy(cohort['events']);mutate(next(e for e in events if select(e)))
        with self.assertRaisesRegex(ValueError,reason):
            validate_events(events,cohort['observed'],self.packet['pins'],cohort['live'])

    def mutate_rows(self,index,tag,mutate,reason):
        cohort=next(c for c in self.packet['cohorts'] if any(r['profile']['index']==index for r in c['observed']))
        selected=lines(HERE/('selected-'+str(cohort['index'])+'.txt'))
        at=next(i for i,l in enumerate(selected) if l.startswith(tag+'\t'+str(index)+'\t'))
        fields=selected[at].split('\t');mutate(fields);selected[at]='\t'.join(fields)
        with self.assertRaisesRegex(ValueError,reason):
            validate_rows(selected,[r['profile'] for r in cohort['observed']],cohort['index'])

    def test_complete(self):
        self.assertEqual([len(c['observed']) for c in self.packet['cohorts']],[35,35,34])
        self.assertEqual(len(self.packet['rows']),104)

    def test_changed_captured_word(self):
        root=self.clone();p=root/'selected-0.txt';s=p.read_text();line=next(l for l in s.splitlines() if l.startswith('FLUID\t'));fields=line.split('\t');words=fields[15].split(',');words[0]=f'{int(words[0],16)^1:08x}';fields[15]=','.join(words);p.write_text(s.replace(line,'\t'.join(fields),1))
        with self.assertRaisesRegex(ValueError,'changed captured'):read_packet(root)

    def test_rebound_profile(self):
        root=self.clone();p=root/'profiles.txt';p.write_text(p.read_text().replace('base-8-meta-0 8 0','base-8-meta-0 8 1',1))
        with self.assertRaisesRegex(ValueError,'changed captured'):read_packet(root)

    def test_duplicate_json(self):
        root=self.clone();p=root/'provenance.json';p.write_text(p.read_text().replace('"profiles": 104,','"profiles": 104, "profiles": 104,',1))
        with self.assertRaisesRegex(ValueError,'duplicate JSON'):read_packet(root)

    def test_boolean_count(self):
        root=self.clone();p=root/'provenance.json';d=json.loads(p.read_text());d['profiles']=True;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'profile count'):read_packet(root)

    def test_false_cohort_query_receipt(self):
        root=self.clone();p=root/'provenance.json';d=json.loads(p.read_text());d['cohorts'][0]['queries']+=1;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'cohort receipt'):read_packet(root)

    def test_mismatched_original_streams(self):
        root=self.clone();p=root/'provenance.json';d=json.loads(p.read_text());d['cohorts'][0]['stdout_sha256'][1]='0'*64;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'identical complete original streams'):read_packet(root)

    def test_false_aggregate_word_receipt(self):
        root=self.clone();p=root/'provenance.json';d=json.loads(p.read_text());d['total_active_words']-=1;p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError,'aggregate receipt'):read_packet(root)

    def test_false_hidden_return(self):
        self.mutate_rows(64,'FLUID',lambda f:f.__setitem__(3,'true'),'return and reached null')

    def test_false_success_bounds(self):
        self.mutate_rows(98,'FLUID',lambda f:f.__setitem__(11,f[10]),'bounds0/1 success')

    def test_false_input_cell(self):
        def change(f):
            cells=f[2].split(';');v=cells[0].split(',');v[3]='1';cells[0]=','.join(v);f[2]=';'.join(cells)
        self.mutate_rows(0,'FLUID_CELLS',change,'entered cells')

    def test_false_backing_count(self):
        self.mutate_rows(0,'FLUID_BACKING',lambda f:f.__setitem__(2,str(int(f[2])-1)),'own-prior inactive')

    def test_false_pool_size(self):
        def change(f):
            d=json.loads(f[3]);d['size']+=1;f[3]=json.dumps(d)
        self.mutate_rows(0,'FLUID_POOL',change,'typed Vector pool')

    def test_query_receiver(self):
        self.mutate_event(lambda e:e['event']=='entry' and e['data']['at'].startswith('ew.'),lambda e:e['data'].__setitem__('receiver','ref:fakeCache'),'genuine EW')

    def test_query_coordinate(self):
        self.mutate_event(lambda e:e['event']=='entry' and e['data']['at'].startswith('ew.e('),lambda e:e['data']['args'].__setitem__(0,e['data']['args'][0]+1),'query|coordinate')

    def test_query_boolean_nibble(self):
        self.mutate_event(lambda e:e['event']=='return' and e['data']['at'].startswith('ew.e('),lambda e:e['data'].__setitem__('result',False),'nibble query')

    def test_material_identity(self):
        self.mutate_event(lambda e:e['event']=='return' and e['data']['at'].startswith('ew.f('),lambda e:e['data'].__setitem__('result','material:replacement'),'Material query')

    def test_final_flow_block_identity(self):
        self.mutate_event(lambda e:e['event']=='field-read' and e['field']=='uu.B',lambda e:e.__setitem__('value','block:9'),'final flow Block')

    def test_height_corner_order(self):
        self.mutate_event(lambda e:e['event']=='entry' and e['data']['at']=='cv.a(IIILln;)F@0',lambda e:e['data']['args'].__setitem__(0,e['data']['args'][0]+1),'corner-height')

    def test_bound_store_pc(self):
        self.mutate_event(lambda e:e['event']=='field-store' and e['field']=='uu.bt',lambda e:e.__setitem__('at','cv.k(Luu;III)Z@1232'),'bounds store')

    def test_bound_store_value(self):
        self.mutate_event(lambda e:e['event']=='field-store' and e['field']=='uu.bw',lambda e:e.__setitem__('after','0000000000000000'),'bounds store')

    def test_vector_cursor_increment(self):
        self.mutate_event(lambda e:e['event']=='field-store' and e['field']=='bt.e',lambda e:e.__setitem__('after',e['after']+1),'cursor increment')

    def test_vector_identity(self):
        self.mutate_event(lambda e:e['event']=='profile-end' and e['pool']['size']>0,lambda e:e['pool']['entries'][0].__setitem__(0,'v9999'),'profile pool/NW')

    def test_uv_operands(self):
        self.mutate_event(lambda e:e['event']=='entry' and e['data']['at']=='nw.a(DD)V@0',lambda e:e['data']['args'].__setitem__(0,'3ff0000000000000'),'UV stores')

    def test_vertex_quad_growth(self):
        self.mutate_event(lambda e:e['event']=='return' and e['data']['at'].startswith('nw.a(DDD)V@'),lambda e:e['data']['nw'].__setitem__('h',e['data']['nw']['h']+1),'quad expansion')

    def test_raw_word_semantic_control(self):
        c=self.packet['cohorts'][0];observed=copy.deepcopy(c['observed']);observed[0]['words'][0]=f'{int(observed[0]["words"][0],16)^1:08x}'
        with self.assertRaisesRegex(ValueError,'raw packed words'):validate_events(c['events'],observed,self.packet['pins'],c['live'])

    def test_null_pc(self):
        self.mutate_event(lambda e:e['event']=='exception' and e['profile']==100,lambda e:e.__setitem__('at','cv.k(Luu;III)Z@13'),'failure PC')

    def test_native_method_classification(self):
        self.mutate_event(lambda e:e['event']=='entry',lambda e:e['data'].__setitem__('native',True),'native method flag')

    def test_actual_argument_type(self):
        self.mutate_event(lambda e:e['event']=='entry',lambda e:e['data'].__setitem__('args',None),'typed source arguments')

    def test_observed_java_wrappers(self):
        for cohort in self.packet['cohorts']:
            self.assertTrue(all(line.startswith('FW_LIVE_CODE\t') for line in cohort['live']))
            self.assertFalse(any(e['event']=='entry' and e['data']['native']
                                 for e in cohort['events']))
        math = self.packet['pins']['classes']['java.lang.Math']['methods']
        sqrt = next(m for m in math if m['name']=='sqrt' and m['descriptor']=='(D)D')
        self.assertFalse(sqrt['access'] & 256)
        self.assertEqual(sqrt['code_bytes'],5)

    def test_live_code_hash(self):
        c=self.packet['cohorts'][0];live=list(c['live']);i=next(i for i,l in enumerate(live) if l.startswith('FW_LIVE_CODE\t'));live[i]=live[i][:-1]+('1' if live[i][-1]!='1' else '0')
        with self.assertRaisesRegex(ValueError,'live Code'):validate_events(c['events'],c['observed'],self.packet['pins'],live)


if __name__=='__main__':
    unittest.main()
