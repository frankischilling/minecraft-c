"""Java-free rejection controls for original catch/print/escape and warm evidence."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parent
S=importlib.util.spec_from_file_location('preview_test_packet',HERE/'packet.py');B=importlib.util.module_from_spec(S);S.loader.exec_module(B)
class Packet(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.data=B.load('observations.json');cls.metadata=B.load('line-metadata.json')
 def reject(self,change,mode='cold'):
  record=deepcopy(self.data['modes'][mode]);change(record)
  with self.assertRaises(ValueError):B.select_mode(mode,record,self.metadata)
 def event(self,r,event,**fields):return next(v for v in r['events'] if v['event']==event and all(v.get(k)==value for k,value in fields.items()))
 def printed(self,r):return next(v['caught'] for v in r['events'] if v['event']=='phase' and v['source']['class']=='gv' and v['source']['pc']==750)
 def rewrite_target(self,r,marker,change):
  for key in ('ordinary_stdout','jdi_stdout'):
   lines=r[key].splitlines(keepends=True)
   for i,line in enumerate(lines):
    if line.startswith('PLAYER_PREVIEW_'+marker+' '):
     value=json.loads(line.split(' ',1)[1]);change(value);lines[i]='PLAYER_PREVIEW_'+marker+' '+json.dumps(value)+'\r\n';break
   r[key]=''.join(lines)
 def test_01_public(self):self.assertEqual(B.audit_public()['cold_UE_returns'],0)
 def test_02_duplicate_json(self):
  with self.assertRaises(ValueError):json.loads('{"profile":0,"profile":1}',object_pairs_hook=B.A.unique)
 def test_03_caught_class(self):self.reject(lambda r:self.printed(r).update({'class':'java.lang.Exception'}))
 def test_04_replaced_caught_object(self):self.reject(lambda r:self.printed(r).update({'id':1}))
 def test_05_native_description_is_not_message(self):self.reject(lambda r:self.printed(r).update({'extended-message':'Living draw null downloaded texture Engine'}))
 def test_06_detail_message_not_fabricated(self):self.reject(lambda r:self.printed(r).update({'detail-message':'anything'}))
 def test_07_cause_identity(self):self.reject(lambda r:self.printed(r).update({'cause-self':False}))
 def test_08_lost_heap_trace(self):self.reject(lambda r:self.printed(r).update({'stack-trace':[]}))
 def test_09_stack_line(self):self.reject(lambda r:self.printed(r)['stack-trace'][0].update({'lineNumber':35}))
 def test_10_probe_frame_relabelled(self):self.reject(lambda r:self.printed(r)['stack-trace'][-1].update({'fileName':'SourceFile'}))
 def test_11_get_message_return(self):self.reject(lambda r:self.event(r,'jdk-return',method='java.lang.NullPointerException.getMessage()Ljava/lang/String;',origin='source-GV747').update({'return-value':'NPE'}))
 def test_12_missing_print_return(self):self.reject(lambda r:r['events'].remove(self.event(r,'jdk-return',method='java.lang.Throwable.printStackTrace()V')))
 def test_13_wrong_declaring_print_body(self):self.reject(lambda r:self.event(r,'jdk-entry',method='java.lang.Throwable.printStackTrace()V').update({'method':'java.lang.Exception.printStackTrace()V'}))
 def test_14_wrong_throw_pc(self):self.reject(lambda r:self.event(r,'exception',**{'source-GV-caught':True})['throw'].update({'pc':10}))
 def test_15_wrong_handler(self):self.reject(lambda r:self.event(r,'exception',**{'source-GV-caught':True})['catch'].update({'pc':750}))
 def test_16_original_caller_pc_line(self):self.reject(lambda r:self.event(r,'exception',**{'source-GV-caught':True})['frames'][1].update({'line':67}))
 def test_17_two_exceptions_not_collapsed(self):
  def change(r):
   first=self.event(r,'exception',**{'source-GV-caught':True})['throwable']['id'];next(e for e in r['events'] if e['event']=='exception' and e['throw']['class']=='sn')['throwable']['id']=first
  self.reject(change)
 def test_18_missing_second_creation(self):self.reject(lambda r:r['events'].remove(next(e for e in r['events'] if e['event']=='exception' and e['throw']['class']=='sn')))
 def test_19_missing_JVM_uncaught_diagnostic(self):
  def change(r):
   for key in ('ordinary_stderr','jdi_stderr'):r[key]=r[key].split('Exception in thread "main" ')[0]
  self.reject(change)
 def test_20_rewritten_Source_diagnostic(self):
  def change(r):
   for key in ('ordinary_stderr','jdi_stderr'):r[key]=r[key].replace('bw.a(SourceFile:36)','bw.a(SourceFile:35)')
  self.reject(change)
 def test_21_no_fake_cold_return(self):self.reject(lambda r:self.rewrite_target(r,'CALL_EXIT',lambda v:v.update({'returned':True})))
 def test_22_no_bE_repair(self):self.reject(lambda r:self.rewrite_target(r,'CALL_EXIT',lambda v:v['pose'].update({'bE':B.A.f(0)})))
 def test_23_no_matrix_repair(self):self.reject(lambda r:self.rewrite_target(r,'CALL_EXIT',lambda v:v['gl'].update({'modelview-depth':1})))
 def test_24_no_rescale_repair(self):self.reject(lambda r:self.rewrite_target(r,'CALL_EXIT',lambda v:v['gl'].update({'rescale-normal':False})))
 def test_25_no_warmup_before_cold(self):
  frame=next(e for e in self.data['modes']['warm']['events'] if e['event']=='entry' and e['method']=='th.a(Lfd;Lji;Lsj;Lls;Lkv;F)V')
  self.reject(lambda r:r['events'].append(deepcopy(frame)))
 def test_26_no_cold_model_fallback(self):
  part=next(e for e in self.data['modes']['warm']['events'] if e['event']=='return' and e['method']=='ps.a(F)V');part=deepcopy(part);part['profile']=0
  self.reject(lambda r:r['events'].append(part))
 def test_27_no_fake_UE_continuation(self):
  phase=next(e for e in self.data['modes']['warm']['events'] if e['event']=='phase' and e['source']['class']=='ue' and e['source']['pc']==350);phase=deepcopy(phase);phase['profile']=0
  self.reject(lambda r:r['events'].append(phase))
 def test_28_no_fake_DS_cleanup(self):
  phase=next(e for e in self.data['modes']['warm']['events'] if e['event']=='phase' and e['source']['class']=='ds' and e['source']['pc']==139);phase=deepcopy(phase);phase['profile']=0
  self.reject(lambda r:r['events'].append(phase))
 def test_29_boolean_profile(self):self.reject(lambda r:r['events'][0].update({'profile':True}))
 def test_30_warm_Engine_store(self):
  self.reject(lambda r:next(e for e in r['events'] if e['event']=='phase' and e['profile']==4 and e['source']['pc']==10)['pose'].update({'manager-engine-null':True}),'warm')
 def test_31_warm_pose_restore(self):self.reject(lambda r:self.rewrite_target(r,'RETURN',lambda v:v['pose'].update({'H':B.A.f(1)})),'warm')
 def test_32_warm_GL_restore(self):self.reject(lambda r:self.rewrite_target(r,'RETURN',lambda v:v['gl'].update({'modelview-depth':2})),'warm')
 def test_33_missing_warm_part(self):self.reject(lambda r:r['events'].remove(self.event(r,'return',method='ps.a(F)V')),'warm')
 def test_34_warm_flags(self):self.reject(lambda r:self.rewrite_target(r,'RETURN',lambda v:v['models'][0].update({'held':True})),'warm')
 def test_35_catch_table(self):
  value=deepcopy(self.metadata);value['original']['gv']['methods']['a(Lls;DDDFF)V']['handlers'][0]['catch']='java/lang/NullPointerException'
  with self.assertRaises(ValueError):B.metadata_invariants(value)
 def test_36_local_variable_metadata(self):
  value=deepcopy(self.metadata);value['original']['bw']['methods']['a(Ljava/lang/String;Ljava/lang/String;)Z']['local_variable_table']=True
  with self.assertRaises(ValueError):B.metadata_invariants(value)

if __name__=='__main__':unittest.main()
