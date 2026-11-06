"""Java-free rejection controls for the published Workbench observations."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('workbench_test_audit',HERE/'audit.py')
A=importlib.util.module_from_spec(S);S.loader.exec_module(A)
P=A.P
class Packet(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.rows=A.load('finite.json')['rows'];cls.observed=A.load('observations.json')
 def rejected(self,mutate):
  values=copy.deepcopy(self.rows);mutate(values)
  with self.assertRaises(ValueError):P.compare(values,self.rows)
 def test_public_audit(self):self.assertEqual(A.audit()['cases'],116)
 def test_ordinary_jdi(self):P.compare(self.observed['jdi_rows'],self.rows)
 def test_duplicate_json(self):
  with self.assertRaises(ValueError):P.rows('WORKBENCH_INTERACTION {"profile":"a","profile":"b"}')
 def test_duplicate_case(self):self.rejected(lambda r:r.append(copy.deepcopy(r[0])))
 def test_missing_case(self):self.rejected(lambda r:r.pop())
 def test_reorder_case(self):self.rejected(lambda r:r.reverse())
 def test_unknown_field(self):self.rejected(lambda r:r[0].update(extra=1))
 def test_missing_field(self):self.rejected(lambda r:r[0].pop('events'))
 def test_bool_as_int(self):self.rejected(lambda r:r[0]['after'].update(selected=False))
 def test_grid_length(self):self.rejected(lambda r:r[0]['after']['craft'].pop())
 def test_cached_length(self):self.rejected(lambda r:r[0]['after']['cached'].pop())
 def test_active_owner(self):self.rejected(lambda r:r[0]['after'].update({'active-same':False}))
 def test_default_owner(self):self.rejected(lambda r:r[0]['after'].update({'default-same':True}))
 def test_alias(self):
  self.rejected(lambda r:next(v for v in r if v['profile']=='alias-slot-cursor-left')['after'].update(cursor='A'))
 def test_grid_retention(self):
  self.rejected(lambda r:next(v for v in r if v['profile']=='close-grid')['after']['craft'].__setitem__(0,None))
 def test_drop_omission(self):self.rejected(lambda r:next(v for v in r if v['drops'])['drops'].pop())
 def test_drop_velocity(self):self.rejected(lambda r:next(v for v in r if v['drops'])['drops'][0]['velocity'].__setitem__(0,'0000000000000000'))
 def test_drop_trig(self):self.rejected(lambda r:next(v for v in r if v['drops'])['drops'][0]['trig'].update(cos='0000000000000000'))
 def test_private_entropy(self):self.rejected(lambda r:r[0].update({'player-random-after':'0000000000000000'}))
 def test_world_entropy(self):self.rejected(lambda r:r[0].update({'world-random-after':'0000000000000000'}))
 def test_return_clone(self):
  self.rejected(lambda r:next(v for v in r if v['return'] is not None).update({'return':None}))
 def test_statistics(self):
  def mutate(rows):
   state=next(r for r in rows if r['after']['stats'])['after']['stats'];key=next(iter(state));state[key]+=1
  self.rejected(mutate)
 def test_publication_order(self):self.rejected(lambda r:next(v for v in r if len(v['drops'])>1)['drops'].reverse())
 def test_publication_alias(self):self.rejected(lambda r:next(v for v in r if v['profile']=='screen-close-cursor-grid-alias')['drops'][0].update(ref='B'))
 def test_chunk_membership(self):self.rejected(lambda r:next(v for v in r if v['drops'])['drops'][0].update({'chunk-members':2}))
 def test_callback_omission(self):self.rejected(lambda r:next(v for v in r if v['events'])['events'].pop())
 def test_declared_alias(self):
  values=copy.deepcopy(self.rows);values[0]['before']['main'][0]='A'
  with self.assertRaises(ValueError):A.declared_inputs(values,A.load('cases.json')['cases'])
 def test_graph_slot(self):
  value=copy.deepcopy(self.observed['graph']);value['slots'][45]['index']=7
  with self.assertRaises(ValueError):A.graph(value)
 def test_constructor_math(self):
  value=copy.deepcopy(self.observed['call_audit']);next(v for v in value if v['constructors'])['constructors'][0]['math-values'][0]='3ff0000000000000'
  with self.assertRaises(ValueError):A.E.constructors(self.observed['jdi_rows'],value)
 def test_callback_order(self):
  expected=[{'index':v['index'],'calls':v['calls']} for v in self.observed['call_audit']]
  value=copy.deepcopy(expected);next(v for v in value if len(v['calls'])>1)['calls'].reverse()
  with self.assertRaises(ValueError):A.compare_callback_trace(value,expected)

if __name__=='__main__':unittest.main()
