"""Corruption controls for original durability operands and explicit active NW projection."""
import copy,hashlib,importlib.util,json
from pathlib import Path
import sys,unittest
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
def module(name):
 s=importlib.util.spec_from_file_location('durability_test_'+name,HERE/(name+'.py'));v=importlib.util.module_from_spec(s);s.loader.exec_module(v);return v
P,A=module('compare'),module('audit')
def read(name):return json.loads((HERE/name).read_text(),object_pairs_hook=P.unique)
class PacketTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.rows=read('finite.json')['rows'];cls.obs=read('observations.json')
 def audit(self,value=None):
  v=self.obs if value is None else value
  return A.audit({'pins':read('source-pins.json')},v['ordinary_stdout'],v['debugger_stdout'],v['events'])
 def test_actual_original_source_and_complete_geometry(self):
  result=self.audit();self.assertEqual(result['rows'],self.rows)
  self.assertEqual((result['draws'],result['raw_words'],result['active_words']),(24,1152,576))
  self.assertEqual((result['source_vertices'],result['packed_vertices']),(96,144));P.compare(self.rows,self.rows)
 def test_all_active_slots_and_last_vertex(self):
  for profile in range(1,9):
   for batch in range(3):
    for slot in P.ACTIVE:
     rows=copy.deepcopy(self.rows);r=rows[profile]['rectangles'][batch];index=40+slot;r['words'][index]=f'{int(r["words"][index],16)^1:08x}'
     with self.subTest(profile=profile,batch=batch,slot=slot),self.assertRaises(ValueError):P.compare(rows,self.rows)
 def test_each_own_inactive_slot_is_not_silently_changed(self):
  for slot in P.INACTIVE:
   rows=copy.deepcopy(self.rows);r=rows[1]['rectangles'][0];r['words'][slot]=f'{int(r["words"][slot],16)^1:08x}'
   with self.subTest(slot=slot),self.assertRaises(ValueError):P.compare(rows,self.rows)
 def test_independent_native_prior_and_honest_prefix_scope(self):
  rows=copy.deepcopy(self.rows)
  for row in rows:
   row['scope']='entered-rectangle-prefix';row['draws']=0
   for rect in row['rectangles']:
    for i in range(48):
     if i%8 in P.INACTIVE:rect['before'][i]=rect['words'][i]='00000000'
  P.compare(rows,self.rows)
  rows[1]['draws']=3
  with self.assertRaises(ValueError):P.compare(rows,self.rows)
 def test_schema_order_extent_duplicates_and_exact_types(self):
  for kind in ('order','type','scope','word','short','flag','extra','sample','skip','draws'):
   rows=copy.deepcopy(self.rows);r=rows[1];rect=r['rectangles'][0]
   if kind=='order':rows[1],rows[2]=rows[2],rows[1]
   if kind=='type':r['id']=True
   if kind=='scope':r['scope']='complete-native'
   if kind=='word':rect['words'][0]='4110000A'
   if kind=='short':rect['before'].pop()
   if kind=='flag':rect['m']=True
   if kind=='extra':rect['extra']=0
   if kind=='sample':r['samples'][1]['damage']=True
   if kind=='skip':rows[0]['bar_width']=0
   if kind=='draws':r['draws']=0
   with self.subTest(kind=kind),self.assertRaises(ValueError):P.compare(rows,self.rows)
  text='ITEM_DURABILITY '+json.dumps(self.rows[0])[:-1]+',"id":267}'
  with self.assertRaises(ValueError):P.rows(text)
 def test_actual_getter_round_source_calls_and_live_code(self):
  for kind in ('round','getter','pc','flags','alias','missing','pin','prior'):
   data=copy.deepcopy(self.obs);events=data['events'];draw=next(e for e in events if e['event']=='nw-draw')
   if kind=='round':next(e for e in events if e['event']=='math-round')['value']+=1
   if kind=='getter':next(e for e in events if e['event']=='getter'and e['method']=='f')['value']+=1
   if kind=='pc':draw['source-frames'][0]['pc']+=1
   if kind=='flags':draw['state']['n']=True
   if kind=='alias':draw['receiver']+=1
   if kind=='missing':events.pop()
   if kind=='pin':next(e for e in events if e['event']=='live-code')['sha256']='0'*64
   if kind=='prior':next(e for e in events if e['event']=='rectangle-entry')['before'][3]='00000000'
   with self.subTest(kind=kind),self.assertRaises((AssertionError,ValueError)):self.audit(data)
 def test_exact_observed_round_boundary_and_signed_color(self):
  self.assertEqual([(r['bar_width'],r['bar_intensity'])for r in self.rows],[(None,None),(13,254),(7,128),(0,1),(0,0),(0,-1),(0,0),(13,254),(0,0)])
  self.assertEqual(self.rows[5]['rectangles'][2]['color'],-256)
  self.assertTrue(any(w!='00000000'for w in self.obs['initial']['words']))
 def test_source_fixture_continuity_and_read_only_observer(self):
  proof=read('provenance.json')
  for section in ('source_files','fixtures'):
   for name,expected in proof[section].items():self.assertEqual(hashlib.sha256((HERE/name).read_bytes()).hexdigest(),expected)
  self.assertEqual(proof['status'],'CAPTURED_ORIGINAL');self.assertTrue(proof['capture_inputs_unchanged'])
  for name in proof['captured_source_files']:
   self.assertNotIn(b'\r\n',(HERE/name).read_bytes())
  source=(HERE/'DurabilityOverlayJdi.java').read_text()
  for api in ('invokeMethod','setValues','redefineClasses','forceEarlyReturn','popFrames','allocateInstance'):
   self.assertNotIn(api,source)
if __name__=='__main__':unittest.main()
