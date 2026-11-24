"""Java-free Source trace, line/catch metadata and cold/warm outcome audit."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
def module(name,file):
 spec=importlib.util.spec_from_file_location(name,HERE/file);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
A=module('preview_caught_executed_audit','audit.py');L=module('preview_caught_lines','line_metadata.py')
def require(value,text):A.require(value,'packet '+text)
def load(name):return json.loads((HERE/name).read_text(encoding='utf8'),object_pairs_hook=A.unique)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def scope(frame,metadata):
 name=frame.get('class',frame.get('declaringClass')).replace('.','/')
 if name in metadata['original']:return 'original-client'
 if name in metadata['authored']:return 'authored-probe'
 return 'JVM'

def verify_location(frame,metadata):
 require(type(frame) is dict and set(frame)=={'class','method','descriptor','pc','line','file'} and all(type(frame[k]) is str for k in ('class','method','descriptor')) and type(frame['pc']) is int and type(frame['line']) is int,'typed actual frame/location')
 kind=scope(frame,metadata)
 if kind=='JVM':return kind
 owner=metadata['original' if kind=='original-client' else 'authored'][frame['class'].replace('.','/')]
 require(frame['method']+frame['descriptor'] in owner['methods'],'actual original/probe method descriptor')
 method=owner['methods'][frame['method']+frame['descriptor']]
 require(frame['file']==owner['source_file'] and frame['line']==L.source_line(method,frame['pc']),'actual original/probe SourceFile and LineNumberTable')
 return kind

def render_stack(throwable,frames,thread_header=False):
 header=('Exception in thread "main" ' if thread_header else '')+throwable['class']+': '+throwable['message']+'\r\n'
 return header+''.join('\tat '+frame['class']+'.'+frame['method']+'('+frame['file']+':'+str(frame['line'])+')\r\n' for frame in frames)

def metadata_invariants(metadata):
 handlers=metadata['original']['gv']['methods']['a(Lls;DDDFF)V']['handlers']
 require(handlers==[{'start':76,'end':740,'handler':743,'catch':'java/lang/Exception'}],'actual GV catch range/type')
 require(metadata['original']['bw']['methods']['a(Ljava/lang/String;Ljava/lang/String;)Z']['local_variable_table'] is False,'original missing local-variable metadata underlying helpful local3 label')


def select_mode(mode,record,metadata):
 metadata_invariants(metadata)
 summary=A.audit_mode(mode,record['ordinary_stdout'],record['jdi_stdout'],record['events'],record['ordinary_exit'],record['jdi_exit'],record['ordinary_stderr'],record['jdi_stderr'])
 events=record['events']
 for event in events:
  require(type(event['profile']) is int and event['profile'] in (0,1,2,3,4,5),'typed source/profile boundary')
  if event['event']=='exception':
   verify_location(event['throw'],metadata)
   if event['catch'] is not None:verify_location(event['catch'],metadata)
  if event['event']=='phase':verify_location(event['source'],metadata)
  for frame in event.get('frames',[]):verify_location(frame,metadata)
 parsed=summary['first']
 if mode=='warm':
  frame_events=[e for e in events if e['event']=='phase' and e['profile']==4 and e['source']['descriptor']=='(Lfd;Lji;Lsj;Lls;Lkv;F)V']
  require([e['source']['pc'] for e in frame_events]==[0,7,10,27] and [e['pose']['manager-engine-null'] for e in frame_events]==[True,True,False,False],'genuine frame engine store7 and remaining owner publication')
  for profile in (2,3):
   parts={e['receiver'] for e in events if e['event']=='return' and e['profile']==profile and e['method']=='ps.a(F)V' and e['primary-part']}
   compiled={e['receiver'] for e in events if e['event']=='return' and e['profile']==profile and e['method']=='ps.d(F)V' and e['primary-part']}
   require(compiled==parts if profile==2 else not compiled,'first actual warm model compilation only; no field resets')
   after=next(row for row in parsed['returns'] if row['profile']==profile)
   require(all(m['held'] is False and m['sneak'] is False for m in after['models']),'actual warm DS flags cleaned up')
  return {key:summary[key] for key in ('mode','source_target_exit','attempted_profiles','returned_profiles','source_caught_exceptions')}|{'frame_input':parsed['frame-input'][0],'frame_return':parsed['frame-return'][0],'inputs':parsed['inputs'],'returns':parsed['returns']}
 require(summary['source_target_exit']==1 and summary['attempted_profiles']==[0] and summary['returned_profiles']==[],'actual first cold source escape; second cold call is not fabricated')
 candidates=[e for e in events if e['event']=='exception' and e['source-GV-caught']];require(len(candidates)==1,'one genuine Source caught exception');caught=candidates[0];identity=caught['throwable']['id']
 printed=next(e['caught'] for e in events if e['event']=='phase' and e['source']['class']=='gv' and e['source']['pc']==750)
 require(type(identity) is int and printed['id']==identity and printed['class']=='java.lang.NullPointerException' and printed['detail-message'] is None and printed['cause-self'] is True and printed['stack-field-null'] is False,'same actual caught Throwable and materialized original state')
 message='Cannot invoke "ji.a(String, String)" because "<local3>" is null'
 require(printed['extended-message']==message,'actual JDK17 helpful-NPE message, not the native descriptive error')
 get_message=[e for e in events if e['event']=='jdk-return' and e['method']=='java.lang.NullPointerException.getMessage()Ljava/lang/String;' and e['origin']=='source-GV747']
 require(len(get_message)==1 and get_message[0]['return-value']==message and get_message[0]['throwable']['id']==identity,'real Source print calls actual getMessage on same Throwable')
 frames=caught['frames'];trace=printed['stack-trace']
 require(len(frames)==len(trace)==9,'all eight original frames and actual authored caller retained')
 require([(f['class'],f['method'],f['file'],f['line']) for f in frames]==[(f['declaringClass'],f['methodName'],f['fileName'],f['lineNumber']) for f in trace],'actual heap StackTraceElement array agrees with original creation frames')
 require([scope(f,metadata) for f in frames]==['original-client']*8+['authored-probe'],'explicit original versus authored caller attribution')
 first_text=render_stack({'class':printed['class'],'message':message},frames)
 candidates=[e for e in events if e['event']=='exception' and e['throw']['class']=='sn' and e['throw']['method']=='f'];require(len(candidates)==1,'one genuine distinct camera exception creation');escaped=candidates[0]
 require(escaped['throw']=={'class':'sn','method':'f','descriptor':'(Lsn;)F','pc':5,'line':678,'file':'SourceFile'} and escaped['source-GV-caught'] is False and escaped['throwable']['class']=='java.lang.NullPointerException' and escaped['throwable']['id']!=identity,'distinct actual camera distance Source NPE escapes GV suffix')
 second_message='Cannot read field "aM" because "<parameter1>" is null'
 second_text=render_stack({'class':'java.lang.NullPointerException','message':second_message},escaped['frames'],True)
 warnings=record['ordinary_stderr'].splitlines(keepends=True)[:4]
 require(len(warnings)==4 and all(line.startswith('WARNING:') for line in warnings),'only JDK SecurityManager warnings precede original diagnostics')
 require(record['ordinary_stderr']==''.join(warnings)+first_text+second_text,'exact stderr: real Source print block then separate JVM uncaught block; no rewritten frames')
 phases=[e['source']['pc'] for e in events if e['event']=='phase' and e['profile']==0 and e['source']['class']=='gv']
 require(phases==[0,743,745,747,750,753,756,759,766],'actual catch/print/cull/pop/suffix prefix; no fabricated GV return')
 ue_phases=[e['source']['pc'] for e in events if e['event']=='phase' and e['profile']==0 and e['source']['class']=='ue']
 require(ue_phases==[0,347] and not any(e['event']=='return' and e['method'] in ('ue.a(F)V','ue.a(IIF)V','id.a(IIF)V','gv.a(Lls;DDDFF)V','ds.a(Lgs;DDDFF)V') for e in events),'actual failure prevents Source return, DS cleanup and UE restoration')
 require([e['source']['pc'] for e in events if e['event']=='phase' and e['profile']==0 and e['source']['class']=='ds' and e['source']['descriptor']=='(Lgs;DDDFF)V']==[0],'Source DS cleanup is not reached')
 require(not any(e['event'] in ('entry','return') and e['profile']==0 and e['method'] in ('fh.a(FFFFFF)V','ps.a(F)V','ps.d(F)V') for e in events),'cold model body/Parts are not reached')
 exit_row=parsed['exits'][0]
 require(exit_row['pose']['bE']==A.f(1) and exit_row['pose']['mouse-x']==exit_row['pose']['mouse-y']==A.f(0) and exit_row['gl']['modelview-depth']==2 and exit_row['gl']['rescale-normal'] is True,'actual leaked temporary pose/matrix state retained, never repaired')
 require(not any(e['event']=='entry' and e['method']=='th.a(Lfd;Lji;Lsj;Lls;Lkv;F)V' for e in events),'no frame/cache bind before cold attempt')
 require(any(e['event']=='jdk-entry' and e['origin']=='JVM-uncaught-handler' and e['throwable']['id']==escaped['throwable']['id'] for e in events),'separate actual JVM uncaught printer observed')
 return {key:summary[key] for key in ('mode','source_target_exit','attempted_profiles','returned_profiles','source_caught_exceptions')}|{
  'caught':{'creation':caught,'after_real_print':printed,'source_frames':frames[:8],'authored_caller_frames':frames[8:],'printed_stderr':first_text},
  'escaped':{'creation':escaped,'message_from_actual_JVM_stderr':second_message,'source_frames':[f for f in escaped['frames'] if scope(f,metadata)=='original-client'],'authored_caller_frames':[f for f in escaped['frames'] if scope(f,metadata)=='authored-probe'],'JVM_uncaught_stderr':second_text},
  'GV_prefix_PCs':phases,'UE_prefix_PCs':ue_phases,'caller_finally_exit':exit_row}

def audit_public():
 observed=load('observations.json');finite=load('finite.json');pins=load('source-pins.json');metadata=load('line-metadata.json');provenance=load('provenance.json')
 require(observed['schema']==finite['schema']==pins['schema']==1 and pins['client_jar_sha256']==A.JAR,'pinned original archive/schema')
 require(set(observed['modes'])==set(finite['modes'])=={'cold','warm'},'both independent constructor contexts retained')
 for owner,meta in metadata['original'].items():
  require(owner in pins['classes'] and meta['class_sha256']==pins['classes'][owner]['class_sha256'],'same archive class identity for Code and line metadata')
  for method in pins['classes'][owner]['methods']:
   if 'code_sha256' in method:
    original=meta['methods'][method['name']+method['descriptor']]
    require(all(original[k]==method[k] for k in ('code_bytes','code_sha256')),'independent Code/line readers agree')
 for owner,meta in metadata['authored'].items():require(meta['class_sha256']==provenance['compiled_before'][owner+'.class'],'actual compiled authored caller metadata')
 jdk={key:value for record in observed['modes'].values() for key,value in record['live_Code'].items() if value['bootstrap-loader']}
 require(load('jdk-live-code.json')['methods']==jdk,'unmodified selection of actual bootstrap JDK bodies')
 for mode,record in observed['modes'].items():
  require(mode in ('cold','warm') and select_mode(mode,record,metadata)==finite['modes'][mode],'exact raw-to-finite observation selection')
  for key,pin in record['live_Code'].items():
   if pin['bootstrap-loader']:continue
   prefix,descriptor=key.split('(',1);owner,name=prefix.rsplit('.',1);method=metadata['original'][owner]['methods'][name+'('+descriptor]
   require(all(method[k]==pin[k] for k in ('code_bytes','code_sha256')),'original live Code identity')
   for frame in pin['lines']:verify_location(frame,metadata)
 handler=metadata['original']['gv']['methods']['a(Lls;DDDFF)V']['handlers']
 require(handler==[{'start':76,'end':740,'handler':743,'catch':'java/lang/Exception'}],'actual GV source catch range/type')
 require(provenance['failed_cohort']==load('declared-inputs.json')['historical-failed-cohort'] and provenance['failed_cohort']['reused'] is False,'retained failed observer cohort')
 for kind in ('captured_helpers','publication_helpers','outputs'):
  for name,expected in provenance[kind].items():require(digest(HERE/name)==expected,'unchanged '+kind+' '+name)
 for mode,record in observed['modes'].items():
  for key in ('ordinary_stdout','ordinary_stderr','jdi_stdout','jdi_stderr'):
   require(hashlib.sha256(record[key].encode('utf8')).hexdigest()==provenance['streams'][mode][key],'exact UTF8 stream bytes including original line endings')
 require(provenance['inputs_before']==provenance['inputs_after'] and provenance['compiled_before']==provenance['compiled_after'],'executed input/helper continuity')
 return {'cold_attempts':1,'cold_UE_returns':0,'caught_print_returns':1,'distinct_source_exceptions':2,'warm_frame_calls':1,'warm_UE_returns':2,'warm_primary_part_returns':14,'original_live_Code_pins':len({key for record in observed['modes'].values() for key,value in record['live_Code'].items() if not value['bootstrap-loader']}),'JDK_live_Code_pins':len({key for record in observed['modes'].values() for key,value in record['live_Code'].items() if value['bootstrap-loader']})}

audit=audit_public
if __name__=='__main__':print(audit_public())
