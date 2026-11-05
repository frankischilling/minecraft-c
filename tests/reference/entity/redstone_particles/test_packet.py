"""Public corruption controls over literal records and independent own entropy."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('redstone_controls',HERE/'packet.py');K=importlib.util.module_from_spec(spec);spec.loader.exec_module(K)

def constructors():return json.loads((HERE/'ordinary-0.constructors.json').read_text(encoding='utf-8'))

class PacketControls(unittest.TestCase):
 def test_complete_original_and_own_streams(self):
  result=K.qualify();self.assertEqual((result['ordinary_vms'],result['constructors_per_vm'],result['components_per_vm']),(2,37,30))
  self.assertEqual([(r['shared_double_returns'],r['private_float_returns_inferred']) for r in result['own_constructor_audits']],[(370,148),(370,148)])
  self.assertNotEqual((HERE/'ordinary-0.constructors.json').read_bytes(),(HERE/'ordinary-1.constructors.json').read_bytes())
  self.assertEqual(json.loads((HERE/'ordinary-0.components.json').read_text()),json.loads((HERE/'ordinary-1.components.json').read_text()))

 def test_every_artifact_pin_and_missing_input(self):
  with tempfile.TemporaryDirectory() as tmp:
   with self.assertRaises(OSError):K.qualify(tmp)
   copied=Path(tmp)/'packet';shutil.copytree(HERE,copied)
   for name in K.PINS:
    with self.subTest(artifact=name):
     path=copied/name;prior=path.read_bytes();path.write_bytes(prior+b'x')
     with self.assertRaises(ValueError):K.qualify(copied)
     path.write_bytes(prior)

 def test_full_literal_comparison_rejects_each_field(self):
  expected=K.reference_components()
  for index,row in enumerate(expected):
   for key in row:
    with self.subTest(profile=index,field=key):
     changed=copy.deepcopy(expected);changed[index][key]="changed" if row[key] is None else None
     with self.assertRaises(ValueError):K.P.compare(changed,expected)
  for changed in (expected[:-1],expected+[expected[-1]],[expected[1],expected[0],*expected[2:]]):
   with self.assertRaises(ValueError):K.P.compare(changed,expected)

 def test_independent_constructor_formula_corruptions(self):
  original=constructors()
  changes=[]
  for key in ('math_before','math_after','own_after'):
   changed=copy.deepcopy(original);changed[1][key]=f"{int(changed[1][key],16)^(1<<40):012x}";changes.append(changed)
  for key in ('velocity','rgb'):
   changed=copy.deepcopy(original);changed[1]['state'][key][0]='0000000000000000' if key=='velocity' else '00000000';changes.append(changed)
  for key in ('c','d','scale','base_scale'):
   changed=copy.deepcopy(original);changed[1]['state'][key]='00000000';changes.append(changed)
  for key in ('lifetime','age','texture'):
   changed=copy.deepcopy(original);changed[1]['state'][key]+=1;changes.append(changed)
  changed=copy.deepcopy(original);changed[1]['state']['no_clip']=True;changes.append(changed)
  changed=copy.deepcopy(original);changed[2]['math_before']=original[0]['math_after'];changes.append(changed)
  changed=copy.deepcopy(original);changed[1]['kind']='flame';changes.append(changed)
  for number,changed in enumerate(changes):
   with self.subTest(case=number):
    with self.assertRaises(ValueError):K.A.ordinary(changed)

 def test_complete_constructor_state_schema_and_defaults(self):
  state=constructors()[0]['state']
  for key in K.STATE:
   changed=copy.deepcopy(state);del changed[key]
   with self.subTest(missing=key):
    with self.assertRaises(ValueError):K.constructor_fields(changed)
  for key,value in (('age',True),('dead',0),('collision',[False]*6),('previous',['0000000000000001']*3),('size',['00000000']*3),('bounds',['0000000000000000']*6)):
   changed=copy.deepcopy(state);changed[key]=value
   with self.subTest(changed=key):
    with self.assertRaises(ValueError):K.constructor_fields(changed)

 def test_nan_and_zero_lifetime_quirks_retained(self):
  rows={r['name']:r for r in K.reference_components()}
  self.assertEqual(rows['draw-nan-partial']['after']['scale'],'7fc00000')
  self.assertEqual(rows['draw-zero-life']['after']['scale'],'ffc00000')
  self.assertEqual(rows['tick-zero-life']['exception'],'ArithmeticException')
  self.assertTrue(rows['tick-zero-life']['after']['dead'])
  self.assertEqual(rows['tick-zero-life']['after']['age'],1)
  self.assertEqual(rows['tick-null-world']['exception'],'none')

if __name__=='__main__':unittest.main()
