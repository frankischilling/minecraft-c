"""Java-free positive gates and independent corruption controls."""
import base64
import copy
import importlib.util
import json
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parent
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
Q=module('auto_close_test_packet',HERE/'packet.py')
S=module('auto_close_test_sdk',HERE/'sdk_compare.py')

class PacketTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.finite=Q.load('finite.json')['rows'];cls.observed=Q.load('observations.json');cls.cases=Q.load('declared-inputs.json')['cases']

 def test_public_packet(self):self.assertEqual(Q.audit_public()['drops'],9)
 def test_independent_rows(self):self.assertEqual(Q.P.compare(self.observed['rows'],self.finite)['composed-gs-ticks'],4)
 def test_structural_two_profiles(self):
  rows=[S.structural_projection(r) for r in self.finite[:2]];self.assertEqual(S.compare(rows,self.finite)['rows'],2)
 def test_structural_selected_profile(self):
  for row in self.finite[:2]:self.assertEqual(S.compare([S.structural_projection(row)],self.finite)['profiles'],[row['profile']])
 def test_duplicate_json_key(self):
  with self.assertRaises(ValueError):Q.P.rows('WORKBENCH_AUTO_CLOSE {"profile":"x","profile":"y"}')
 def test_source_positions_are_declared(self):
  rows=copy.deepcopy(self.finite);rows[4]['before']['player']['sn.aM']='4030800000000001'
  with self.assertRaises(ValueError):Q.typed_rows(rows,self.cases)
 def test_predicate_return_type(self):
  rows=copy.deepcopy(self.finite);rows[5]['return']=0
  with self.assertRaises(ValueError):Q.typed_rows(rows,self.cases)
 def test_same_aa_receiver(self):self.bad_receiver('aa.a(Lgs;)V')
 def test_same_it_receiver(self):self.bad_receiver('it.a(Lgs;)V')
 def test_same_player_receiver(self):self.bad_receiver('dc.r()V')
 def bad_receiver(self,method):
  events=copy.deepcopy(self.observed['events'])
  for event in events:
   if event['index']==0 and event.get('method')==method and event['event'] in ('entry','return'):event['receiver']['id']+=99999
  with self.assertRaises(ValueError):Q.trace_identities(self.observed['rows'],events)
 def test_grid_reads_ascending(self):
  events=copy.deepcopy(self.observed['events']);event=next(e for e in events if e['index']==0 and e['event']=='entry' and e['method']=='mq.f_(I)Liz;' and e['caller']=={'method':'it.a(Lgs;)V','pc':29});event['arguments']=[8]
  with self.assertRaises(ValueError):Q.trace_identities(self.observed['rows'],events)
 def test_raw_stdout_preserved(self):
  observed=copy.deepcopy(self.observed);item=observed['transport']['original.stdout'];item['base64']=base64.b64encode(b'changed\n').decode()
  with self.assertRaises(ValueError):Q.transports(self.finite,observed)
 def test_raw_events_preserved(self):
  observed=copy.deepcopy(self.observed);observed['events'][0]['state']['player']+=1
  with self.assertRaises(ValueError):Q.transports(self.finite,observed)

def row_mutation(name,change):
 def test(self):
  rows=copy.deepcopy(self.finite);change(rows)
  with self.assertRaises(ValueError):Q.typed_rows(rows,self.cases)
 test.__name__='test_rows_'+name;setattr(PacketTests,test.__name__,test)

row_mutation('missing',lambda r:r.pop())
row_mutation('order',lambda r:r.reverse())
row_mutation('extra_field',lambda r:r[0].__setitem__('extra',0))
row_mutation('active_default',lambda r:r[0]['after'].__setitem__('active-default',False))
row_mutation('screen_retained',lambda r:r[0]['after'].__setitem__('app-screen-null',False))
row_mutation('cursor_retained',lambda r:r[0]['after'].__setitem__('cursor','cursor'))
row_mutation('grid_cleared',lambda r:r[0]['after']['craft'].__setitem__(0,None))
row_mutation('drop_order',lambda r:r[0]['drops'].reverse())
row_mutation('drop_count',lambda r:r[0]['drops'].pop())
row_mutation('alias_lost',lambda r:r[2]['drops'][1].__setitem__('ref','grid0'))
row_mutation('stack_value',lambda r:r[0]['drops'][0]['value'].__setitem__(1,8))
row_mutation('chunk_members',lambda r:r[0]['drops'][0].__setitem__('chunk-members',2))
row_mutation('world_member',lambda r:r[0]['drops'][0].__setitem__('world-member',False))
row_mutation('pickup_delay',lambda r:r[0]['drops'][0].__setitem__('pickup-delay',39))
row_mutation('source_velocity',lambda r:r[0]['drops'][0]['velocity'].__setitem__(0,'0000000000000000'))
row_mutation('trig_angle',lambda r:r[0]['drops'][0]['trig'].__setitem__('angle','00000000'))
row_mutation('player_seed',lambda r:r[0].__setitem__('player-random-after','0000000000000000'))
row_mutation('world_seed',lambda r:r[0].__setitem__('world-random-after','0000000000000000'))
row_mutation('source_exception',lambda r:r[0].__setitem__('exception','java.lang.NullPointerException'))
row_mutation('gl_error',lambda r:r[0].__setitem__('gl-error',1282))
row_mutation('predicate_over64',lambda r:r[5].__setitem__('return',True))
row_mutation('operation_conflated',lambda r:r[6].__setitem__('operation','gs-tick'))

def event_mutation(name,change):
 def test(self):
  events=copy.deepcopy(self.observed['events']);change(events)
  with self.assertRaises(ValueError):Q.A.observations(self.finite,self.observed['rows'],events,self.cases)
 test.__name__='test_events_'+name;setattr(PacketTests,test.__name__,test)

def phase(events,pc):return next(e for e in events if e['index']==0 and e['event']=='phase' and e['pc']==pc)
def entry(events,method):return next(e for e in events if e['index']==0 and e['event']=='entry' and e['method']==method)
def returned(events,method):return next(e for e in events if e['index']==0 and e['event']=='return' and e['method']==method)
event_mutation('missing_fresh_store151',lambda e:e.remove(phase(e,151)))
event_mutation('missing_suffix382',lambda e:e.remove(phase(e,382)))
event_mutation('missing_return402',lambda e:e.remove(phase(e,402)))
event_mutation('wrong_dc_caller143',lambda e:entry(e,'dc.r()V')['caller'].__setitem__('pc',142))
event_mutation('wrong_it_caller15',lambda e:entry(e,'it.a(Lgs;)V')['caller'].__setitem__('pc',1))
event_mutation('missing_ls_return',lambda e:e.remove(returned(e,'ls.w_()V')))
event_mutation('caught_exception',lambda e:e.insert(1,{'event':'exception','index':0,'class':'java.lang.NullPointerException'}))
event_mutation('private_int_value',lambda e:returned(e,'java.util.Random.nextInt(I)I').__setitem__('value',-1))
event_mutation('private_float_value',lambda e:returned(e,'java.util.Random.nextFloat()F').__setitem__('value','00000000'))
event_mutation('constructor_math',lambda e:returned(e,'java.lang.Math.random()D').__setitem__('value','0000000000000000'))
event_mutation('constructor_private_stream',lambda e:returned(e,'hl.<init>(Lfd;DDDLiz;)V')['constructor'].__setitem__('private-random','0000000000000000'))
event_mutation('focus_recursion_missing',lambda e:entry(e,'vy.a()V')['caller'].__setitem__('pc',0))

def sdk_mutation(name,change):
 def test(self):
  rows=[S.structural_projection(r) for r in self.finite[:2]];change(rows)
  with self.assertRaises(ValueError):S.compare(rows,self.finite)
 test.__name__='test_sdk_'+name;setattr(PacketTests,test.__name__,test)

sdk_mutation('alias',lambda r:r[0].__setitem__('grid0-retained-same',False))
sdk_mutation('drop_alias',lambda r:r[0]['drop-stack-same-input'].__setitem__(0,False))
sdk_mutation('drop_order',lambda r:r[0]['drop-values'].reverse())
sdk_mutation('grid_count',lambda r:r[0]['grid0'].__setitem__(1,3))
sdk_mutation('bool_type',lambda r:r[0].__setitem__('active-default',1))
sdk_mutation('unknown_profile',lambda r:r[0].__setitem__('profile','direct-dc-removed'))
sdk_mutation('duplicate_profile',lambda r:r[1].__setitem__('profile','tick-removed'))
sdk_mutation('extra_scope',lambda r:r[0].__setitem__('whole-world-identical',True))

if __name__=='__main__':unittest.main()
