"""Java-free original-evidence and native-prefix protocol corruption controls."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('workbench_drawing_public_packet',HERE/'packet.py')
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
 def replay(self,events):return B.audit_events(self.evidence['ordinary_stdout'],self.evidence['jdi_stdout'],events)
 def test_01_published_original(self):
  self.assertEqual(B.audit_public()['active_words'],108)
 def test_02_own_different_backing(self):
  P.compare(self.native(),self.original)
 def test_03_source_scope(self):
  self.reject(self.original)
 def test_04_duplicate_json(self):
  with self.assertRaises(ValueError):P.rows('WORKBENCH_DRAW_GEOMETRY {"profile":0,"profile":1}')
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
 def test_24_caught_exception_false_success(self):
  events=deepcopy(self.evidence['events']);events.append({'event':'exception','profile':0})
  with self.assertRaises(ValueError):self.replay(events)
 def test_25_missing_slot_return(self):
  events=deepcopy(self.evidence['events']);events.pop(next(i for i,e in enumerate(events) if e['event']=='return' and e['method']=='id.a(Lgp;)V'))
  with self.assertRaises(ValueError):self.replay(events)
 def test_26_missing_draw_return(self):
  events=deepcopy(self.evidence['events']);events.pop(next(i for i,e in enumerate(events) if e['event']=='nw-return'))
  with self.assertRaises(ValueError):self.replay(events)
 def test_27_missing_success_continuation(self):
  events=deepcopy(self.evidence['events']);events.pop(next(i for i,e in enumerate(events) if e['event']=='phase' and e['pc']==490))
  with self.assertRaises(ValueError):self.replay(events)
 def test_28_wrong_texture(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='entry' and e['method']=='ji.b(Ljava/lang/String;)I' and e['arguments']==['/gui/crafting.png']);e['arguments']=['/gui/inventory.png']
  with self.assertRaises(ValueError):self.replay(events)
 def test_29_wrong_foreground_label(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='entry' and e['method']=='sj.b(Ljava/lang/String;III)V' and e['arguments'][0]=='Crafting');e['arguments'][1]=29
  with self.assertRaises(ValueError):self.replay(events)
 def test_30_owner_replacement(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='phase');e['owners']['active-screen-same']=False
  with self.assertRaises(ValueError):self.replay(events)
 def test_31_extra_slot_visit(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='entry' and e['method']=='id.a(Lgp;)V');events.append(deepcopy(e))
  with self.assertRaises(ValueError):self.replay(events)
 def test_32_missing_cursor_child(self):
  events=deepcopy(self.evidence['events']);i=next(i for i,e in enumerate(events) if e['event']=='entry' and e['profile']==1 and e['method']=='bb.a(Lsj;Lji;Liz;II)V' and any(f=={'class':'id','method':'a','descriptor':'(IIF)V','pc':266} for f in e['frames']));events.pop(i)
  with self.assertRaises(ValueError):self.replay(events)
 def test_33_tooltip_translation(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='return' and e['method']=='nh.b(Ljava/lang/String;)Ljava/lang/String;');e['return-value']='Stone'
  with self.assertRaises(ValueError):self.replay(events)
 def test_34_tooltip_name(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='return' and e['method']=='iz.l()Ljava/lang/String;');e['return-value']='tile.stone'
  with self.assertRaises(ValueError):self.replay(events)
 def test_35_wrong_texture_callsite(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='entry' and e['method']=='ji.b(Ljava/lang/String;)I' and e['arguments']==['/gui/crafting.png']);next(f for f in e['frames'] if f['class']=='oo')['pc']=8
  with self.assertRaises(ValueError):self.replay(events)
 def test_36_wrong_label_callsite(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='entry' and e['method']=='sj.b(Ljava/lang/String;III)V' and e['arguments'][0]=='Crafting');next(f for f in e['frames'] if f['class']=='oo')['pc']=11
  with self.assertRaises(ValueError):self.replay(events)
 def test_37_other_batch_inactive(self):
  events=deepcopy(self.evidence['events']);e=next(e for e in events if e['event']=='nw-other-draw');e['words'][7]=f"{int(e['words'][7],16)^1:08x}"
  with self.assertRaises(ValueError):self.replay(events)
 def test_38_other_batch_return(self):
  events=deepcopy(self.evidence['events']);events.pop(next(i for i,e in enumerate(events) if e['event']=='nw-return' and e['kind']=='other'))
  with self.assertRaises(ValueError):self.replay(events)
 def test_39_boolean_phase(self):
  events=deepcopy(self.evidence['events']);next(e for e in events if e['event']=='phase')['pc']=False
  with self.assertRaises(ValueError):self.replay(events)
 def test_40_boolean_NW_order(self):
  events=deepcopy(self.evidence['events']);next(e for e in events if e['event']=='nw-draw')['ordinal']=False
  with self.assertRaises(ValueError):self.replay(events)
 def mutate_target(self,marker,change):
  lines=self.evidence['ordinary_stdout'].splitlines()
  for i,line in enumerate(lines):
   if line.startswith(marker+' '):
    row=json.loads(line.removeprefix(marker+' '));change(row);lines[i]=marker+' '+json.dumps(row);break
  with self.assertRaises(ValueError):B.typed_target('\n'.join(lines))
 def test_41_declared_input_damage_type(self):self.mutate_target('WORKBENCH_DRAW_INPUT',lambda r:r['scene']['main'][0].__setitem__(2,False))
 def test_42_signed_side_boolean(self):
  lines=self.evidence['ordinary_stdout'].splitlines()
  for i,line in enumerate(lines):
   if line.startswith('WORKBENCH_BLOCK_LEAF '):
    row=json.loads(line.removeprefix('WORKBENCH_BLOCK_LEAF '))
    if row['method']=='b(I)I' and row['arguments']==[0]:row['arguments']=[False];lines[i]='WORKBENCH_BLOCK_LEAF '+json.dumps(row);break
  with self.assertRaises(ValueError):B.typed_target('\n'.join(lines))
 def test_43_VG_negative_side(self):
  lines=self.evidence['ordinary_stdout'].splitlines()
  for i,line in enumerate(lines):
   if line.startswith('WORKBENCH_BLOCK_LEAF '):
    row=json.loads(line.removeprefix('WORKBENCH_BLOCK_LEAF '))
    if row['method']=='a(II)I' and row['arguments']==[-1,1]:row['return']=21;lines[i]='WORKBENCH_BLOCK_LEAF '+json.dumps(row);break
  with self.assertRaises(ValueError):B.typed_target('\n'.join(lines))
 def test_44_gl_return_error(self):self.mutate_target('WORKBENCH_DRAW_RETURN',lambda r:r.update({'gl-error':1280}))
 def test_45_inventory_mutation(self):self.mutate_target('WORKBENCH_DRAW_RETURN',lambda r:r['scene']['main'][0].__setitem__(1,2))
 def test_46_failed_cohort_history(self):
  expected=B.load('provenance.json')['failed_cohorts']
  for mutation in ('omit','reclassify'):
   with self.subTest(mutation=mutation):
    value=deepcopy(expected)
    if mutation=='omit':value.pop()
    else:value[0]['classification']='source game frontier'
    with self.assertRaises(ValueError):B.failed_cohorts(value)

if __name__=='__main__':unittest.main()
