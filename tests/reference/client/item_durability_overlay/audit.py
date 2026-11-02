"""Read-only original source receipts and each process's actual NW packing audit."""
import importlib.util,json,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('durability_projection',HERE/'compare.py');P=importlib.util.module_from_spec(spec);spec.loader.exec_module(P)
def require(v,text):
 if not v:raise AssertionError(text)
def tagged(text,prefix):return[json.loads(l.removeprefix(prefix+' '),object_pairs_hook=P.unique)for l in text.splitlines()if l.startswith(prefix+' ')]
def i32(v):return(v+2**31)%2**32-2**31
def fword(v):return struct.pack('>f',float(v)).hex()
def audit(report,ordinary,target,events):
 for text in(ordinary,target):
  owners=tagged(text,'DURABILITY_OWNERS');require(len(owners)==1,'one owner receipt')
  require(all(owners[0].get(k)==v for k,v in {'app':'kq','settings':'kv','packs':'ik','engine':'ji','font':'sj','renderer':'bb','nw':'nw'}.items())and owners[0]['canonical_app']is True and owners[0]['raw_allocations']==0,'genuine original constructor graph')
  done=tagged(text,'DURABILITY_COMPLETE');require(len(done)==1 and done[0]['profiles']==9 and done[0]['network']==0 and all(done[0][k]is False for k in('game_substitution','random_rebinding','source_backing_cleared','source_flags_repaired'))and done[0]['raw_allocations']==0,'ordinary/JDI completion without source repair')
  inputs=tagged(text,'DURABILITY_INPUT');returns=tagged(text,'DURABILITY_RETURN')
  require([(r['profile'],r['id'],r['damage'],r['max'],r['count'],r['x'],r['y'])for r in inputs]==[(n,i,d,m,1,7,11)for n,i,d,m in P.PROFILES],'nine declared genuine Stack inputs')
  require([r['index']for r in returns]==list(range(9))and all(r['gl_error']==0 and r['count']==1 and r['nw_after']['o']==0 for r in returns),'nine real returned source calls')
  initial=tagged(text,'DURABILITY_INITIAL');require(len(initial)==1 and initial[0]['static']=={'b':True,'c':False}and P.words(initial[0]['words'])and any(w!='00000000'for w in initial[0]['words']),'actual Font-used NW defaults and stale backing')
 for label in('DURABILITY_INITIAL','DURABILITY_INPUT','DURABILITY_RETURN','DURABILITY_DEVICE'):
  require(tagged(ordinary,label)==tagged(target,label),'ordinary/read-only JDI exact observed state '+label)
 inputs=tagged(target,'DURABILITY_INPUT');returns=tagged(target,'DURABILITY_RETURN')
 records=[e for e in events if e['event']!='live-code']
 require(sum(e['event']=='overlay-entry'for e in records)==9 and sum(e['event']=='overlay-return'for e in records)==9,'actual original entry/return count')
 entries=[e for e in records if e['event']=='overlay-entry']
 for k in('receiver','font','engine'):require(len({e[k]for e in entries})==1,'same actual constructor owner '+k)
 require(len({e['stack']for e in entries})==9,'nine distinct genuine Stacks')
 rows=[];draw_receiver=None
 for index,(name,item,damage,maximum)in enumerate(P.PROFILES):
  group=[e for e in records if e['profile']==index]
  require(group[0]['event']=='overlay-entry'and group[-1]['event']=='overlay-return','complete bracketed original overlay')
  require([(e['id'],e['damage'],e['count'],e['x'],e['y'])for e in group if e['event']=='overlay-entry']==[(item,damage,1,7,11)],'source entry actual arguments')
  gets=[e for e in group if e['event']=='getter'];g=lambda c,m:[e['value']for e in gets if(e['class'],e['method'])==(c,m)]
  damaged=damage>0;require(g('iz','e')==[True]and g('iz','g')==[damaged],'actual IZ damage predicates')
  require(g('gm','f')==[maximum]*(3 if damaged else 1)and g('iz','h')==[damage]*(2 if damaged else 0)and g('iz','j')==[maximum]*(2 if damaged else 0),'fresh independently observed getter values')
  initial=[e for e in group if e['event']=='initial-damage'];require(len(initial)==1 and initial[0]['damage']==damage and initial[0]['source-frames'][0]=={'class':'iz','method':'g','descriptor':'()Z','pc':8},'actual initial damage getfield8 operand')
  rounds=[e for e in group if e['event']=='math-round'];require(len(rounds)==(2 if damaged else 0),'actual Math.round return count')
  for e,pc in zip(rounds,(116,141)):require(e['source-frames'][0]=={'class':'bb','method':'b','descriptor':'(Lsj;Lji;Liz;II)V','pc':pc},'actual round producer source callsite')
  width=i32(rounds[0]['value'])if damaged else None;intensity=i32(rounds[1]['value'])if damaged else None
  rect_entries=[e for e in group if e['event']=='rectangle-entry'];draws=[e for e in group if e['event']=='nw-draw'];rect_returns=[e for e in group if e['event']=='rectangle-return']
  require(len(rect_entries)==len(draws)==len(rect_returns)==(3 if damaged else 0),'three real entered/drawn/returned rectangles')
  rects=[];prior=inputs[index]['before']
  for batch,(entry,draw,returned,pc)in enumerate(zip(rect_entries,draws,rect_returns,(220,240,260))):
   require(entry['batch']==draw['batch']==returned['batch']==batch and entry['before']==prior,'same-process untouched raw prior backing')
   require(entry['source-frames']==[{'class':'bb','method':'a','descriptor':'(Lnw;IIIII)V','pc':0},{'class':'bb','method':'b','descriptor':'(Lsj;Lji;Liz;II)V','pc':pc}],'actual rectangle entry original callsite')
   require(draw['source-frames']==[{'class':'bb','method':'a','descriptor':'(Lnw;IIIII)V','pc':67},{'class':'bb','method':'b','descriptor':'(Lsj;Lji;Liz;II)V','pc':pc}],'actual NW.draw67 original callers')
   require(draw['receiver']==entry['nw'],'same actual NW receipt receiver')
   if draw_receiver is None:draw_receiver=draw['receiver']
   require(draw['receiver']==draw_receiver,'same singleton NW throughout')
   s=draw['state'];require(all(s[k]=='0000000000000000'for k in('s','t','u')),'actual zero source NW translation')
   r={k:entry[k]for k in('x','y','width','height','color','before')};r.update(words=draw['words']);r.update({k:s[k]for k in('h','p','o','r','l','m','n')})
   require((r['x'],r['y'],r['width'],r['height'])==(9,24,(13,12,width)[batch],(2,1,1)[batch]),'actual original rectangle dimensions')
   red=(r['color']>>16)&255;green=(r['color']>>8)&255;blue=r['color']&255;packed=f'{0xff000000|(blue<<16)|(green<<8)|red:08x}'
   x,y,w,h=r['x'],r['y'],r['width'],r['height'];vertices=((x,y),(x,y+h),(x+w,y+h),(x,y),(x+w,y+h),(x+w,y))
   for v,(a,b)in enumerate(vertices):require([r['words'][v*8+j]for j in P.ACTIVE]==[fword(a),fword(b),'00000000',packed],'actual NW source XYZ/color packing and quad duplication')
   rects.append(r);prior=draw['words']
  require(returns[index]['words']==prior,'actual returned NW raw backing retained')
  samples=[{'damage':initial[0]['damage'],'maximum':g('gm','f')[0]}]
  if damaged:samples.extend({'damage':d,'maximum':m}for d,m in zip(g('iz','h'),g('iz','j')))
  rows.append({'scope':'completed-source-draws','profile':name,'id':item,'damage':damage,'max':maximum,'x':7,'y':11,'draws':len(draws),'rectangle_prefixes':len(rect_entries),'damageable':True,'damaged':damaged,'samples':samples,'bar_width':width,'bar_intensity':intensity,'rectangles':rects})
 P.validate(rows)
 pins=[e for e in events if e['event']=='live-code'];required={('bb','b','(Lsj;Lji;Liz;II)V'),('bb','a','(Lnw;IIIII)V'),('nw','a','()V'),('iz','g','()Z'),('iz','e','()Z'),('iz','h','()I'),('iz','j','()I'),('gm','f','()I')}
 require({(e['class'],e['method'],e['descriptor'])for e in pins}==required and len(pins)==8,'eight actually reached original Code pins')
 for pin in pins:
  m=next(m for m in report['pins']['classes'][pin['class']]['methods']if(m['name'],m['descriptor'])==(pin['method'],pin['descriptor']))
  require((m['code_sha256'],m['code_bytes'])==(pin['sha256'],pin['bytes']),'live Code unchanged from original archive')
 return{'rows':rows,'live_code':pins,'raw_words':24*48,'active_words':24*24,'source_vertices':24*4,'packed_vertices':24*6,'draws':24,'initial':tagged(ordinary,'DURABILITY_INITIAL')[0],'owners':tagged(ordinary,'DURABILITY_OWNERS')[0],'device':tagged(ordinary,'DURABILITY_DEVICE')[0]}
