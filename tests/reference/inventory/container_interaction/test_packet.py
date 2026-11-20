"""Java-free corruption controls for genuine original Container observations."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import struct
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('container_packet_audit',HERE/'audit.py')
A=importlib.util.module_from_spec(spec);spec.loader.exec_module(A)
P=A.P

class Packet(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.finite=A.load('finite.json')['rows'];cls.observed=A.load('observations.json')
 def row(self,name):
  i=next(i for i,r in enumerate(self.finite) if r['profile']==name)
  rows=list(self.finite);rows[i]=deepcopy(rows[i]);return rows,rows[i]
 def reject(self,rows):
  with self.assertRaises(ValueError):P.compare(rows,self.finite)
 def test_01_complete_packet(self):
  self.assertEqual(A.audit()['cases'],73)
 def test_02_duplicate_json(self):
  with self.assertRaises(ValueError):P.rows('CONTAINER_INTERACTION {"profile":"a","profile":"b"}')
 def test_03_omitted_case(self):self.reject(self.finite[:-1])
 def test_04_reordered_case(self):self.reject([self.finite[1],self.finite[0],*self.finite[2:]])
 def test_05_duplicate_case(self):self.reject([self.finite[0],*self.finite[:-1]])
 def test_06_boolean_as_integer(self):
  rows,r=self.row('empty-left');r['after']['changed']=1;self.reject(rows)
 def test_07_unknown_state_field(self):
  rows,r=self.row('empty-left');r['after']['invented']=False;self.reject(rows)
 def test_08_wrong_count(self):
  rows,r=self.row('merge-63-7-left');r['after']['stacks']['A'][1]+=1;self.reject(rows)
 def test_09_wrong_animation(self):
  rows,r=self.row('pickup-7-left');r['after']['stacks'][r['return']][3]=5;self.reject(rows)
 def test_10_return_alias(self):
  rows,r=self.row('pickup-7-left');r['return']='A';self.reject(rows)
 def test_11_shared_input_alias(self):
  rows,r=self.row('alias-main-slots-left');r['after']['main'][1]=None;self.reject(rows)
 def test_12_wrong_statistic(self):
  rows,r=self.row('output-to-same-cursor-left');r['after']['stats']['16842757']=4;self.reject(rows)
 def test_13_shift_zero_statistic(self):
  rows,r=self.row('shift-output-planks');r['after']['stats']['16842757']=4;self.reject(rows)
 def test_14_dirty_callback_effect(self):
  rows,r=self.row('empty-left');r['after']['changed']=False;self.reject(rows)
 def test_15_cursor_close_alias(self):
  rows,r=self.row('close-cursor-grid-alias');r['drops'][1]['ref']='B';r['events'][1]['ref']='B';self.reject(rows)
 def test_16_source_exception(self):
  rows,r=self.row('negative-slot');r['exception']='none';self.reject(rows)
 def test_17_rng_progression(self):
  rows,r=self.row('outside-left');r['player-random-after']=r['player-random-before'];self.reject(rows)
 def test_18_world_rng(self):
  rows,r=self.row('outside-left');r['world-random-after']=f"{(int(r['world-random-after'],16)+1)&A.E.MASK:016x}";self.reject(rows)
 def test_19_exact_velocity_word(self):
  rows,r=self.row('outside-left');word=r['drops'][0]['velocity'][0];r['drops'][0]['velocity'][0]=f'{int(word,16)^1:016x}';self.reject(rows)
 def test_20_coherent_forged_trig(self):
  rows,r=self.row('outside-left');r['drops'][0]['trig']['cos']='0000000000000000';r['drops'][0]['velocity'][0]='0000000000000000';self.reject(rows)
 def test_21_publication_membership(self):
  rows,r=self.row('outside-right');r['drops'][0]['chunk-members']=0;self.reject(rows)
 def test_22_ordered_callbacks(self):
  expected=[{'index':r['index'],'calls':r['calls']} for r in self.observed['call_audit']]
  actual=deepcopy(expected);i=next(i for i,r in enumerate(actual) if len(r['calls'])>2)
  actual[i]['calls'][0],actual[i]['calls'][1]=actual[i]['calls'][1],actual[i]['calls'][0]
  with self.assertRaises(ValueError):A.compare_callback_trace(actual,expected)
 def test_23_constructor_entropy(self):
  observed=deepcopy(self.observed['call_audit']);i=next(i for i,r in enumerate(observed) if r['constructors'])
  observed[i]['constructors'][0]['math-values'][0]='0000000000000000'
  with self.assertRaises(ValueError):A.E.constructors(self.observed['jdi_rows'],observed)
 def test_24_method_owner(self):
  names=A.load('item-name-inventory.json');names['resolved_methods']['qd']['owner']='qd'
  with self.assertRaises(ValueError):A.name_inventory(names,A.load('source-pins.json'))

if __name__=='__main__':unittest.main()
