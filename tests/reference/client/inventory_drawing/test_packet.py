"""Java-free original-evidence and native-prefix protocol corruption controls."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('inventory_drawing_public_packet',HERE/'packet.py')
B=importlib.util.module_from_spec(spec);spec.loader.exec_module(B)
P=B.P

class Packet(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.original=B.load('finite.json')['rows'];cls.evidence=B.load('observations.json')
 def native(self):
  # Authored JSON transport control only, not a native execution receipt.
  result=deepcopy(self.original)
  for row in result:
   row['completed-draws']=0
   row['prior']=[f'{0x11220000+i:08x}' for i in range(48)]
   for i in range(48):
    if i not in P.active_indices(row):row['words'][i]=row['prior'][i]
  return result
 def reject(self,actual):
  with self.assertRaises(ValueError):P.compare(actual,self.original)
 def replay(self,events):return B.A.audit({},self.evidence['ordinary_stdout'],self.evidence['jdi_stdout'],events)
 def test_01_published_original(self):
  self.assertEqual(B.audit_public()['active_words'],108)
 def test_02_own_different_backing(self):
  P.compare(self.native(),self.original)
 def test_03_source_scope(self):
  self.reject(self.original)
 def test_04_duplicate_json(self):
  with self.assertRaises(ValueError):P.rows('INVENTORY_DRAW_GEOMETRY {"profile":0,"profile":1}')
 def test_05_missing_row(self):self.reject(self.native()[:-1])
 def test_06_reordered_rows(self):
  rows=self.native();rows[0],rows[1]=rows[1],rows[0];self.reject(rows)
 def test_07_duplicate_profile(self):
  rows=self.native();rows[2]=deepcopy(rows[0]);self.reject(rows)
 def test_08_boolean_count(self):
  rows=self.native();rows[0]['state']['source-vertices']=True;self.reject(rows)
 def test_09_boolean_profile(self):
  rows=self.native();rows[2]['profile']=True;self.reject(rows)
 def test_10_wrong_word_count(self):
  rows=self.native();rows[0]['words'].pop();self.reject(rows)
 def test_11_wrong_backing_count(self):
  rows=self.native();rows[0]['prior'].pop();self.reject(rows)
 def test_12_unknown_field(self):
  rows=self.native();rows[0]['invented']=0;self.reject(rows)
 def test_13_nonhex(self):
  rows=self.native();rows[0]['words'][0]='43D58000';self.reject(rows)
 def test_14_active_position(self):
  rows=self.native();rows[0]['words'][0]='43d58001';self.reject(rows)
 def test_15_active_depth(self):
  rows=self.native();rows[1]['words'][2]='3f800000';self.reject(rows)
 def test_16_active_gradient_color(self):
  rows=self.native();rows[0]['words'][5]='c0101011';self.reject(rows)
 def test_17_active_tile_uv(self):
  rows=self.native();rows[1]['words'][4]='3f260001';self.reject(rows)
 def test_18_color_flag(self):
  rows=self.native();rows[1]['state']['color']=True;self.reject(rows)
 def test_19_normal_flag(self):
  rows=self.native();rows[0]['state']['normal']=True;self.reject(rows)
 def test_20_translation(self):
  rows=self.native();rows[0]['state']['translation'][1]='3ff0000000000000';self.reject(rows)
 def test_21_inactive_uv(self):
  rows=self.native();rows[0]['words'][3]='00000000';self.reject(rows)
 def test_22_inactive_normal(self):
  rows=self.native();rows[1]['words'][6]='00000000';self.reject(rows)
 def test_23_inactive_reserved(self):
  rows=self.native();rows[0]['words'][7]='00000000';self.reject(rows)
 def test_24_caught_renderer_false_success(self):
  events=deepcopy(self.evidence['events']);events.append({'event':'gv-caught-exception','profile':0})
  with self.assertRaises(ValueError):self.replay(events)
 def test_25_missing_part_return(self):
  events=deepcopy(self.evidence['events']);i=next(i for i,e in enumerate(events) if e['event']=='return' and e['method']=='ps.a(F)V' and e['primary-part']);events.pop(i)
  with self.assertRaises(ValueError):self.replay(events)
 def test_26_missing_draw_return(self):
  events=deepcopy(self.evidence['events']);events.pop(next(i for i,e in enumerate(events) if e['event']=='nw-return'))
  with self.assertRaises(ValueError):self.replay(events)
 def test_27_missing_success_continuation(self):
  events=deepcopy(self.evidence['events']);events.pop(next(i for i,e in enumerate(events) if e['event']=='ue-phase' and e['pc']==408))
  with self.assertRaises(ValueError):self.replay(events)
 def test_28_constant_sneaking_substitute(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='return' and e['method']=='dc.t()Z');e['return-value']=True
  with self.assertRaises(ValueError):self.replay(events)


 def test_29_pose_restore(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='ue-phase' and e['pc']==408);e['pose']['H']='3f800000'
  with self.assertRaises(ValueError):self.replay(events)
 def test_30_input_owner(self):
  text=self.evidence['ordinary_stdout'].replace('"input":"lr"','"input":"uo"')
  self.assertNotEqual(text,self.evidence['ordinary_stdout'])
  with self.assertRaises(ValueError):B.A.target(text)
 def test_31_previous_mouse_lag(self):
  lines=self.evidence['ordinary_stdout'].splitlines()
  for i,line in enumerate(lines):
   if line.startswith('INVENTORY_DRAW_INPUT '):
    row=json.loads(line.removeprefix('INVENTORY_DRAW_INPUT '))
    if row['profile']==1:
     row['pose']['mouse-x']='c2700000';lines[i]='INVENTORY_DRAW_INPUT '+json.dumps(row);break
  with self.assertRaises(ValueError):B.A.target('\n'.join(lines))

if __name__=='__main__':unittest.main()
