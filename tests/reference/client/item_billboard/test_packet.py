"""Java-free Item billboard Source and native-prefix corruption controls."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import unittest
HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('billboard_packet_test',HERE/'packet.py');B=importlib.util.module_from_spec(S);S.loader.exec_module(B);P=B.P
class Packet(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.original=B.load('finite.json')['rows'];cls.evidence=B.load('observations.json')
 def native(self):
  rows=deepcopy(self.original)
  for row in rows:
   row['completed-draws']=0;row['prior']=[f'{0x24680000+i:08x}' for i in range(48)]
   for i in range(48):
    if i not in P.active_indices(row):row['words'][i]=row['prior'][i]
  return rows
 def reject(self,change):
  rows=self.native();change(rows)
  with self.assertRaises(ValueError):P.compare(rows,self.original)
 def reject_events(self,change):
  events=deepcopy(self.evidence['events']);change(events)
  with self.assertRaises(ValueError):B.audit_events(self.evidence['ordinary_stdout'],self.evidence['jdi_stdout'],events)
 def test_01_public(self):self.assertEqual(B.audit_public()['active_words'],340)
 def test_02_different_own_prior(self):P.compare(self.native(),self.original)
 def test_03_no_native_draw_claim(self):self.reject(lambda r:r[0].update({'completed-draws':1}))
 def test_04_duplicate_JSON(self):
  with self.assertRaises(ValueError):P.rows('ITEM_BILLBOARD_GEOMETRY {"profile":0,"profile":1}')
 def test_05_missing_row(self):self.reject(lambda r:r.pop())
 def test_06_reordered_copy(self):self.reject(lambda r:r.reverse())
 def test_07_duplicate_copy(self):self.reject(lambda r:r.__setitem__(2,deepcopy(r[1])))
 def test_08_boolean_profile(self):self.reject(lambda r:r[1].update(profile=True))
 def test_09_boolean_copy(self):self.reject(lambda r:r[0].update(copy=False))
 def test_10_unknown_field(self):self.reject(lambda r:r[0].update(extra=1))
 def test_11_word_count(self):self.reject(lambda r:r[0]['words'].pop())
 def test_12_prior_count(self):self.reject(lambda r:r[0]['prior'].pop())
 def test_13_uppercase_hex(self):self.reject(lambda r:r[0]['words'].__setitem__(0,'BF000000'))
 def test_14_position(self):self.reject(lambda r:r[0]['words'].__setitem__(0,'bf000001'))
 def test_15_UV(self):self.reject(lambda r:r[0]['words'].__setitem__(3,'3ea00001'))
 def test_16_normal(self):self.reject(lambda r:r[0]['words'].__setitem__(6,'00000000'))
 def test_17_inactive_color(self):self.reject(lambda r:r[0]['words'].__setitem__(5,'00000000'))
 def test_18_duplicate_normal(self):self.reject(lambda r:r[0]['words'].__setitem__(30,'00007f00'))
 def test_19_reserved(self):self.reject(lambda r:r[0]['words'].__setitem__(7,'00000000'))
 def test_20_normal_flag(self):self.reject(lambda r:r[0]['state'].update(normal=False))
 def test_21_color_flag(self):self.reject(lambda r:r[0]['state'].update(color=True))
 def test_22_source_count(self):self.reject(lambda r:r[0]['state'].update({'source-vertices':True}))
 def test_23_translation(self):self.reject(lambda r:r[0]['state']['translation'].__setitem__(0,'3ff0000000000000'))
 def test_24_icon(self):self.reject(lambda r:r[0].update(icon=54))
 def test_25_UV_scalar(self):self.reject(lambda r:r[0]['uv'].__setitem__(0,'3ea00001'))
 def test_26_source_exception(self):self.reject_events(lambda e:e.append({'event':'exception','profile':0}))
 def test_27_constructor_phase(self):self.reject_events(lambda e:next(v for v in e if v['event']=='constructor').update(phase='00000000'))
 def test_28_constructor_entropy(self):self.reject_events(lambda e:next(v for v in e if v['event']=='constructor')['math-values'].__setitem__(0,'3ff0000000000000'))
 def test_29_seed_argument(self):self.reject_events(lambda e:next(v for v in e if v['event']=='entry' and v['method']=='java.util.Random.setSeed(J)V')['arguments'].__setitem__(0,188))
 def test_30_copy_random_return(self):self.reject_events(lambda e:next(v for v in e if v['event']=='return' and v['method']=='java.util.Random.nextFloat()F').update({'return-value':'00000000'}))
 def test_31_copy_offset(self):
  self.reject_events(lambda e:next(v for v in e if v['event']=='gl-entry' and v['method']=='org.lwjgl.opengl.GL11.glTranslatef(FFF)V' and any(f['class']=='bb' and f['pc']==652 for f in v['frames']))['arguments'].__setitem__(0,'00000000'))
 def test_32_bob_not_normalized(self):
  self.reject_events(lambda e:next(v for v in e if v['event']=='gl-entry' and v['method']=='org.lwjgl.opengl.GL11.glTranslatef(FFF)V' and any(f['class']=='bb' and f['pc']==124 for f in v['frames']))['arguments'].__setitem__(1,B.A.f(2.5)))
 def test_33_real_Normal_arguments(self):self.reject_events(lambda e:next(v for v in e if v['event']=='entry' and v['method']=='nw.b(FFF)V')['arguments'].__setitem__(1,B.A.f(0)))
 def test_34_actual_tail(self):self.reject_events(lambda e:next(v for v in e if v['event']=='nw-draw')['words'].__setitem__(48,'00000001'))
 def test_35_missing_completed_draw(self):self.reject_events(lambda e:e.pop(next(i for i,v in enumerate(e) if v['event']=='return' and v['method']=='nw.a()V')))
 def test_36_missing_parent_return(self):self.reject_events(lambda e:e.pop(next(i for i,v in enumerate(e) if v['event']=='return' and v['method']=='bb.a(Lhl;DDDFF)V')))
 def test_37_actual_GL_submission(self):self.reject_events(lambda e:next(v for v in e if v['event']=='gl-entry' and v['method']=='org.lwjgl.opengl.GL11.glDrawArrays(III)V')['arguments'].__setitem__(0,7))
 def test_38_real_vertex(self):self.reject_events(lambda e:next(v for v in e if v['event']=='entry' and v['method']=='nw.a(DDDDD)V')['arguments'].__setitem__(0,B.A.d(0)))
 def test_39_real_color_leaf(self):self.reject_events(lambda e:next(v for v in e if v['event']=='return' and v['method']=='gm.f(I)I').update({'return-value':0}))
 def test_40_real_camera_rotation(self):self.reject_events(lambda e:next(v for v in e if v['event']=='gl-entry' and v['method']=='org.lwjgl.opengl.GL11.glRotatef(FFFF)V')['arguments'].__setitem__(0,B.A.f(0)))

if __name__=='__main__':unittest.main()
