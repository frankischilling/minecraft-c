"""Validate actual original tick results, separate entropy contexts and selected source prefixes."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE=Path(__file__).resolve().parent
MULT=0x5deece66d
MASK=(1<<48)-1
def need(ok,message):
    if not ok:raise ValueError(message)
def rows(path):
    need(path.stat().st_size<128*1024*1024,'bounded complete source journal')
    return[json.loads(line)for line in path.read_text().splitlines()]
def table(name):return[line.split('\t')for line in(HERE/name).read_text().splitlines()[1:]]
def internal(seed,bits):seed=(seed*MULT+11)&MASK;return seed,seed>>(48-bits)
def integer(seed,bound):
    while True:
        seed,bits=internal(seed,31);value=(bound*bits)>>31 if bound&(bound-1)==0 else bits%bound
        if bound&(bound-1)==0 or bits-value+bound-1<1<<31:return seed,value
def rng(seed,method,args):
    if method=='nextInt':return integer(seed,args[0])
    if method=='nextFloat':
        seed,bits=internal(seed,24);return seed,{'F':struct.pack('>f',bits/(1<<24)).hex()}
    if method=='nextDouble':
        seed,a=internal(seed,26);seed,b=internal(seed,27);return seed,{'D':struct.pack('>d',((a<<27)+b)/(1<<53)).hex()}
    raise ValueError('Unqualified reached RNG '+method)

def canonical_state(state):
    roles={}
    for role,ref in state.get('reference_roles',{}).items():
        key=ref.get('oid',ref.get('identity'));roles.setdefault(key,role)
    def visit(value):
        if isinstance(value,list):return[visit(v)for v in value]
        if isinstance(value,dict):
            if'class'in value and('identity'in value or'oid'in value):
                key=value.get('oid',value.get('identity'));need(key in roles,'all compared references have actual canonical roles')
                return{'class':value['class'],'role':roles[key]}
            return{k:visit(v)for k,v in value.items()if k not in('math_seed','reference_roles','sn.aD')}
        return value
    return visit(state)

def validate(directory,pins):
    directory=Path(directory);normal=rows(directory/'normal/target.ndjson');debug=rows(directory/'debugger/target.ndjson')
    need(len(normal)==len(debug)==62,'all authored tick profiles complete transport')
    profiles=table('profiles.tsv');qualification=table('qualification.tsv');records=rows(directory/'debugger/checkpoints.ndjson')
    code=[r for r in records if r['event']=='code-pin'];entries=[r for r in records if r['event']=='entry'];returns=[r for r in records if r['event']=='return'];draws=[r for r in records if r['event']=='random'];prefixes=[r for r in records if r['event']=='prefix'];exceptions=[r for r in records if r['event']=='exception']
    starts={r['case']:r for r in records if r['event']=='active-entry'};ends={r['case']:r for r in records if r['event']=='active-return'}
    need(set(starts)==set(ends)==set(range(62)),'complete actual active call lifetime observations')
    for r in code:
        m=next(m for m in pins['classes'][r['class']]['methods']if(m['name'],m['descriptor'])==(r['method'],r['descriptor']))
        need((r['bytes'],r['sha256'])==(m['code_bytes'],m['code_sha256']),'actual live reached original Code')
    need(len(code)>=200 and len({r['class']for r in code})>=30,'broad actual reached body closure')
    known={};last_u=last_nano=None;default_count=0;return_count=0
    for r in records:
        event=r['event']
        if event=='clock':
            if r['kind']=='seedUniquifier':last_u=int(r['value'],16);last_nano=None
            else:last_nano=int(r['value'],16)
        elif event=='default-random':
            need(last_u is not None and last_nano is not None,'actual qualified default constructor U/nano pair')
            seed=(last_u^last_nano^MULT)&MASK;need(r['seed']==f'{seed:012x}','actual default constructor seed formula')
            known[r['oid']]=seed;last_u=last_nano=None;default_count+=1
        elif event=='active-entry':
            s=r['state'];ref=s['reference_roles'].get('actor.random')
            if ref is not None:known[ref['oid']]=int(s['own_seed'],16)
        elif event=='random':
            oid=r['oid']
            if r['phase']=='tick'and r['role']in('world','math'):
                # The exact pre-call World stream is an authored caller input;
                # shared Math is an observed, unmodified constructor context.
                if not any(x.get('event')=='random'and x.get('phase')=='tick'and x.get('case')==r['case']and x.get('role')==r['role']and x['sequence']<r['sequence']for x in draws):known[oid]=int(starts[r['case']]['state'][r['role']+'_seed'],16)
            need(oid in known,'observed RNG start state available without entropy injection')
            seed,value=rng(known[oid],r['method'],r['arguments']);need(value==r['result']and r['seed']==f'{seed:012x}','all actual RNG arguments/results/poststates independently replayed');known[oid]=seed;return_count+=1
    need(default_count>=68 and return_count==len(draws),'default constructor and RNG context census')
    comparisons=[];events=[]
    for i,(n,d,p,q)in enumerate(zip(normal,debug,profiles,qualification)):
        need(n['case']==d['case']==i and n['input']==d['input']==p,'exact authored profile identity')
        need(n['kind']==d['kind']and n['scenario']==d['scenario']and n['outcome']==d['outcome']and n['failure']==d['failure'],'ordinary/JDI source outcomes')
        need(canonical_state(n['before'])==canonical_state(d['before']),'prepared active operands equal across independent constructor entropy')
        need(n['before']['own_seed']==d['before']['own_seed']and n['after']['own_seed']==d['after']['own_seed']and n['after']['world_seed']==d['after']['world_seed'],'complete seeded active RNG states')
        need(n['events']==d['events'],'complete ordered ordinary/JDI effects')
        need(not n['default_constructor_entropy_rebound']and n['actual_entity_published'],'actual ordinary Entity publication and no default profile rebinding')
        need(n['before']['fields']['sn.aD']==n['after']['fields']['sn.aD'],'actual Entity ID stable across each call')
        need(n['constructor_contexts']and all(not c['default_entropy_rebound']for c in n['constructor_contexts']),'unnormalized raw constructor contexts retained')
        need(canonical_state(d['before'])==canonical_state(starts[i]['state']),'actual JDI pre-call fields/watchers/bounds/roles/pools')
        mode=q[1]
        if mode!='prefix':
            need(canonical_state(n['after'])==canonical_state(d['after']),'ordinary/JDI complete prepared active fields')
            need(canonical_state(d['after'])==canonical_state(ends[i]['state']),'actual JDI post-call fields/watchers/bounds/pools')
            comparisons.append({'case':i,'comparison':mode,'state':n['after'],'events':n['events'],'source_outcome':n['outcome'],'source_exception':n['failure']})
        else:
            candidate=[r for r in prefixes if r['case']==i and(r['site']['class'],r['site']['method'],r['site']['descriptor'],str(r['site']['pc']))==(q[2],q[3],q[4],q[5])]
            need(candidate,'selected actual entered source boundary available');selected=candidate[0]
            need(n['events'][:len(selected['events'])]==selected['events'],'prefix preserves only already-emitted actual callbacks')
            comparisons.append({'case':i,'comparison':'prefix','site':selected['site'],'source_frames':selected['source_frames'],'state':selected['state'],'events':selected['events'],'arguments':selected['arguments'],'argument_vectors':selected['argument_vectors'],'source_outcome':n['outcome'],'source_exception':n['failure']})
        for ordinal,event in enumerate(n['events']):
            token=event.split();need(len(token)>=3 and token[1:3]==[str(i),str(ordinal)],'actual ordered emission ordinal');events.append(event)
    # Source branch controls and their first reached dependency identities.
    need(normal[41]['after']['fields']['ls.Y']==normal[41]['before']['fields']['ls.Y'],'actual Wolf eye boundary avoids suffocation')
    need(any(r['case']==43 and r['location']['class']=='ar'and r['location']['method']=='d_'for r in entries),'Squid true drowning exemption reached only after actual immersion')
    need(normal[43]['after']['fields']['sn.bz']==300,'Squid air reset after genuine water query')
    need(normal[42]['events'][:8]and all(e.startswith('PARTICLE')and' bubble 'in e for e in normal[42]['events'][:8]),'actual eight drowning bubbles precede damage')
    for i,owner,method,pc in[(53,'ls','U',19),(54,'sn','k_',32),(55,'sn','k_',13),(56,'sn','b',5)]:need(any(r['case']==i and r['class']=='java.lang.NullPointerException'and(r['location']['class'],r['location']['method'],r['location']['pc'])==(owner,method,pc)for r in exceptions),'actual source null-field failure BCI')
    need(normal[13]['after']['fields']['sn.be']is True and normal[51]['after']['fields']['sn.be']is False and normal[52]['after']['fields']['sn.be']is False,'peaceful post-super/remote/failed-super distinction')
    need(normal[48]['before']['fields']['ii.d']==normal[48]['before']['fields']['ls.b'],'actual target/look alias preserved')
    need(normal[60]['after']['fields']['ii.d']is not None,'actual Wolf hunt stores real Sheep target')
    for mode in('normal','debugger'):
        actual=set(re.findall(r"Initializing '([^']+)'",(directory/mode/'class-init.log').read_text()))&set(pins['jar_class_names']);need(actual==set(pins['initialized_original_classes']),'exact initialized original class closure')
        need(not{'net/minecraft/client/Minecraft','yo','vy','px'}&actual,'excluded client device bootstrap')
        bindings=[s.split()for s in(directory/mode/'target.stdout').read_text().splitlines()if s.startswith('BIND ')];need(len(bindings)==len(pins['classes']),'all runtime original class origin/resource bindings')
        for _,name,extent,digest in bindings:need((int(extent),digest)==(pins['classes'][name]['class_bytes'],pins['classes'][name]['class_sha256']),'actual runtime class bytes')
    coverage={'profiles':62,'mob_kinds':11,'full_controls':48,'callback_failures':2,'source_null_failures':4,'missing_child_prefixes':8,'class_pins':len(pins['classes']),'initialized_original_classes':len(pins['initialized_original_classes']),'live_method_pins':len(code),'original_entries':len(entries),'original_returns':len(returns),'RNG_returns':len(draws),'default_Random_occurrences':default_count,'constructor_contexts':sum(len(r['constructor_contexts'])for r in normal),'prefix_records':len(prefixes),'exception_events':len(exceptions)}
    result={'tick_records':normal,'debugger_tick_records':debug,'prefix_records':prefixes,'comparison_records':comparisons,'event_rows':events,'coverage':coverage,'source_phase_records':[r for r in records if r['event']in('entry','return')and r['location']['method']in('w_','U','o','f_','T','K')and r['location']['descriptor']=='()V'],'entropy_records':[r for r in records if r['event']in('clock','default-random','construction-return')or(r['event']=='random'and(r['phase']=='construction'or r['role']=='math'))],'exception_records':exceptions,'limits':['Raw constructor/default and shared Math entropy is retained, never rebound or compared as identical application profiles.','Prepared active operands derive only from authored tables and actual setters/publication.','Full original final states remain separate from selected missing-child prefix states; original successful outcomes are never rewritten as native failures.','Callback Throwable classes are actual Java observation adapters, not native IO equivalence.','Absolute Entity ID sn.aD is preserved and stable per call; cross-process comparisons use a separate identity context.','Finite cached dark/wet/terrain/reference controls do not prove arbitrary combat, paths, rendering or unrestricted gameplay.']}
    (directory/'observations.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
    return coverage
