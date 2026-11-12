"""Strict actual ordinary Windows current-normal supplement admission.

The primary Linux ordinary/read-only-JDI packet remains the body authority.
Only matching-device current-normal values may supplement that packet.
"""
import importlib.util,json,re
from pathlib import Path
BASE=Path(__file__).resolve().parent;ROOT=next(p for p in BASE.parents if (p/'CMakeLists.txt').is_file())
def module(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
analysis=module('supplement_primary_analysis',BASE/'original.py');shared=module('supplement_pair',ROOT/'tests/tools/client/test_creature_draw.py');need=analysis.need


def admit(directory,primary_capture,modes,projection):
 proof=analysis.public_inputs()['windows_normal_supplement'];p=directory/'receipt.json'
 need(analysis.sha(p)==proof['receipt_sha256'],'exact actual ordinary supplement receipt')
 d=json.loads(p.read_text(encoding='utf-8'))
 need(d['input_before_sha256']==proof['input_sha256'] and d['artifacts_sha256']==proof['artifacts_sha256'] and d['helper_sha256']==proof['helper_sha256'],'complete exact supplement input/helper/artifact inventories')
 need(d['status']=='PASS' and d['ordinary_only'] and d['no_JDI_credit'] and d['exit']==0 and d['inputs_unchanged'] and d['input_before_sha256']==d['input_after_sha256'],'actual successful ordinary-only supplement')
 need(d['primary_original_receipt_sha256']==analysis.sha(primary_capture/'receipt.json'),'same immutable primary source capture')
 primary=json.loads((primary_capture/'receipt.json').read_text());need(d['helper_sha256']==primary['helper_before_sha256'],'all actual helper bytes equal the captured primary')
 analysis.fingerprints(proof['input_sha256'])
 for n,pin in d['artifacts_sha256'].items():need(analysis.sha(directory/n.replace('\\','/'))==pin,'immutable actual supplement artifact: '+n)
 authority=json.loads((primary_capture/'authority.json').read_text())['classes'];stdout=(directory/'normal/process.stdout').read_text();binds={}
 for line in stdout.splitlines():
  if line.startswith('BIND '):
   _,name,origin,size,pin=line.split();need(name not in binds,'unique supplementary source binding');binds[name]=(origin,int(size),pin)
 need(set(binds)==set(authority),'all747 actual supplementary runtime origins')
 for name,(origin,size,pin) in binds.items():need(origin=='file:/C:/Users/admin/minecraft/b1.7.3.jar' and size==authority[name]['class_bytes'] and pin==authority[name]['class_sha256'],'same actual original whole-class bytes from supplied JAR')
 rows=analysis.rows(directory/'normal/target.ndjson');draws=[r for r in rows if r['event']=='draw'];devices=[r for r in rows if r['event']=='device'];need(devices==d['devices'] and len(devices)==1,'actual supplement device identity')
 need([r['index'] for r in draws]==list(range(7)) and all(r['outcome']==1 and r['failure'] is None and r['gl_error']==0 for r in draws),'all seven actual supplementary calls returned')
 pair=shared.Pair();left_tex=[r for r in rows if r['event']=='texture_lookup'];right_tex=[r for r in analysis.rows(primary_capture/'debugger/target.ndjson') if r['event']=='texture_lookup'];need(len(left_tex)==len(right_tex)==6,'all six actual supplementary uploads')
 for a,b in zip(left_tex,right_tex):
  need(a['index']==b['index'] and a['key']==b['key'],'same actual upload profile');pair.name(a['name'],b['name'],'texture');pair.name(a['cached_name'],b['cached_name'],'texture');pair.value(a['transfer_hex'],b['transfer_hex'],'supplement/actual-decoded-transfer')
 normals={};constructor_raw=[]
 for a,b in zip(draws,modes['debugger']['draws']):
  need(a['input']==b['input'],'same seven authored declared source inputs')
  need(a['before']['entropy']==a['after']['entropy'],'supplement raw streams unchanged per actual call')
  need(a['before']['actor']==a['after']['actor'] and a['before']['camera']==a['after']['camera'], 'each actual supplementary complete actor/camera state remains unchanged through its own call')
  for phase in ['before','after']:
   left,right=a[phase],b[phase];label=f'case{a["index"]}/{phase}/gl'
   pair.graph(left['graph'],right['graph'],'supplement/'+label+'/graph')
   for key in ['parts','pool','pool_storage']:pair.value(left[key],right[key],'supplement/'+label+'/'+key)
   gl_a,gl_b=dict(left['gl']),dict(right['gl']);normal_a,normal_b=gl_a.pop('normal'),gl_b.pop('normal');pair.gl(gl_a,gl_b,'supplement/'+label)
   normals[a['index'],phase]=normal_a
   for owner in ['actor','camera']:
    aa,bb=left[owner],right[owner];pair.value(aa['actual'],bb['actual'],'supplement/'+label+'/'+owner+'/actual')
    selected={'ls.aa','ls.ad'} | ({'gb.a','gb.b'} if a['kind']=='gb' and owner=='actor' else set())
    for key in sorted(selected):
     pair.value(aa['fields'][key],bb['fields'][key],'supplement/'+label+'/'+owner+'/selected-field/'+key)
    for key in aa['fields']:
     if key not in selected and aa['fields'][key]!=bb['fields'][key]:
      constructor_raw.append({'case':a['index'],'phase':phase,'owner':owner,'field':key,'supplement':aa['fields'][key],'primary':bb['fields'][key]})
    for key in ['bounds','watcher','watcher_dirty','watcher_records']:pair.value(aa[key],bb[key],'supplement/'+label+'/'+owner+'/'+key)
  for key in ['outcome','nw_position','nw_words','gl_error']:pair.value(a[key],b[key],'supplement/'+str(a['index'])+'/'+key)
  for phase in ['before','after']:need(d['normal_words'][str(a['index'])+'/'+phase]==normals[a['index'],phase],'receipt actual current-normal words agree with raw observed state')
 return {'device':devices[0],'normals':normals,'typed_cpu_state_checks':pair.checks,'identity_pairs':len(pair.forward),'receipt_sha256':analysis.sha(p),'constructor_raw_exclusions':constructor_raw,'scope':'Actual same-frozen-helper ordinary source CPU/graph/state agreement with primary; Only selected aa/ad/fuse fields are paired across processes; raw other-field differences retained explicitly. Complete actor/camera state invariance is checked separately inside each actual VM. Only raw actual current-normal values may supplement matching-driver Native GL. No supplementary JDI or pixel credit.'}


def matching_device(native,supplement):
 actual=supplement['device'];need(native['vendor']==actual['vendor'] and native['renderer']==actual['renderer'],'actual identical GPU vendor/renderer for supplementary normals')
 driver=lambda text:re.search(r'\bNVIDIA [0-9]+\.[0-9]+$',text)
 a,b=driver(native['version']),driver(actual['version']);need(a and b and a[0]==b[0]=='NVIDIA 581.42','actual identical observed driver suffix; no cross-driver supplement')
 return {'native':native,'ordinary_source':actual,'driver':a[0]}

if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--supplement',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();_,modes,_,projection=analysis.audit(a.capture);r=admit(a.supplement,a.capture,modes,projection);r['normals']={str(k[0])+'/'+k[1]:v for k,v in r['normals'].items()};a.output.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8',newline='\n');print('SUPPLEMENT_PASS',r['typed_cpu_state_checks'],r['identity_pairs'],r['device'])
