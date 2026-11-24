"""Post-capture strict pairing of seven real BW fire-billboard calls.
Original observations are never Native execution inputs. Independent raw
constructor entropy stays in each VM; only same-process invariance is compared.
"""
import argparse,copy,importlib.util,json,re
from pathlib import Path
BASE=Path(__file__).resolve().parent
ROOT=next(p for p in BASE.parents if(p/'CMakeLists.txt').is_file())
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
original=module('burning_billboard_original',BASE/'original.py')
build=module('burning_billboard_build',BASE/'build.py')
shared=module('burning_billboard_shared_pair',ROOT/'tests/tools/client/test_creature_draw.py')
need=original.need

def admit_original(path,device_path=None):
    need(device_path is None,'no unobserved matching-device source supplement is admitted')
    receipt,modes,events,projection=original.audit(path)
    return {'path':path,'receipt':receipt,'modes':modes,'events':events,'projection':projection,'original_manifest_negative_controls':original.manifest_negatives(receipt,original.public_inputs()['primary'])}

def admit_build(path,executable):return build.admit(path,executable)

def recheck(admitted,compiled=None):
    proof=original.public_inputs()['primary'];original.fingerprints(proof['input_sha256'])
    need(original.sha(admitted['path']/'receipt.json')==proof['receipt_sha256'],'current immutable source receipt')
    for name,pin in proof['artifacts_sha256'].items():need(original.sha(admitted['path']/name)==pin,'current immutable source output '+name)
    if compiled:build.recheck(compiled)

def matching_device(actual,expected):
    need(actual['vendor']==expected['vendor']and actual['renderer']==expected['renderer'],'actual Native/source driver vendor and renderer')
    driver=lambda s:re.search(r'\bMesa .+$',s)
    a,b=driver(actual['version']),driver(expected['version']);need(a and b and a[0]==b[0],'same actual Mesa driver; no cross-driver live-state admission')
    need(actual['framebuffer']==expected['framebuffer'],'exact actual selected effective framebuffer attributes')
    return {'native':actual,'ordinary_source':expected,'driver':a[0]}

def compare_rows(data,admitted):
    modes=admitted['modes'];source=modes['debugger']['draws'];projected=admitted['projection']['cases'];pair=shared.Pair()
    groups={k:[r for r in data if r['event']==k]for k in ['texture','device','input','state','batch','terrain','fire','control']}
    need(sum(map(len,groups.values()))==len(data),'complete declared Native transport')
    need([r['index']for r in groups['texture']]==list(range(6)),'six real baseline texture setups')
    need(len(groups['device'])==1,'one actual device row');device=matching_device(groups['device'][0],modes['debugger']['device'])
    for k in ['input','terrain','fire']:need([r['case']for r in groups[k]]==list(range(7)),'seven actual ordered '+k+' rows')
    states=groups['state'];need([(r['case'],r['phase'])for r in states]==[(i,p)for i in range(7)for p in ('before','after')],'all ordered before/after snapshots')
    need(len(groups['batch'])==13,'all thirteen real source/native driver batches')
    # Native-only policy/fault controls have no Original exception equivalence credit.
    need(not groups['control'],'ordinary transport contains no separate-process Native-only controls')
    reference_textures=[r for r in modes['debugger']['all_rows']if r['event']=='texture_lookup']
    for actual,expected in zip(groups['texture'],reference_textures):
        pair.value(actual['key'],expected['key'],'texture/key');pair.name(actual['name'],expected['name'],'texture');pair.name(actual['cached_name'],expected['cached_name'],'texture');pair.value(actual['transfer_hex'],expected['transfer_hex'],'texture/actualTransfer')
    geometry_words=0;receiver_checks=0;slot_checks=0;live_batches=0
    for i,o in enumerate(source):
        pr=projected[i];before,after=states[2*i:2*i+2]
        need(before['entropy']==after['entropy'],'Native same-process raw streams preserved; no cross-process seed equality')
        for actual,phase in [(before,'before'),(after,'after')]:
            expected=o[phase];pair.graph(actual['graph'],expected['graph'],f'case{i}/{phase}/graph')
            for k in ['parts','pool','pool_storage']:pair.value(actual[k],expected[k],f'case{i}/{phase}/{k}')
            pair.gl(actual['gl'],expected['gl'],f'case{i}/{phase}/gl')
        n=groups['input'][i];oi=dict(o['input']);renderer=next(r for r in o['before']['graph']['objects']if shared.reference(r)==shared.reference(oi['renderer']));oi['shadow_size']=renderer['fields']['bw.c']
        for nk,ok,tag in [('width','width','F'),('height','height','F'),('y','y','D'),('min_y','bounds_min_y','D'),('view_yaw','manager_yaw','F'),('shadow_size','shadow_size','F')]:pair.value(n[nk],oi[ok][tag],f'case{i}/actualInput/{nk}')
        pair.value(n['actor'],o['kind'],f'case{i}/actualActorClass');pair.value(n['bv'],oi['bv'],f'case{i}/actualBv')
        bridge_rows=[r for r in admitted['events']if r['event']=='draw-entry'and r['case']==i];need(len(bridge_rows)==1,'actual entry identity bridge');bridge={shared.reference(r['actual']):r['identity']for r in bridge_rows[0]['identity_bridge']}
        def actual_ref(v):
            ref=shared.reference(v);need(ref in bridge,'actual entry operand belongs to selected source graph');return {'class':ref[0],'identity':bridge[ref]}
        f=groups['fire'][i];need({'receiver','actor'}<=set(f),'actual Native fire entry receiver/actor transport is required')
        for k,v,role in [('receiver',pr['entry']['receiver'],'renderer'),('actor',pr['entry']['arguments'][0],'actor')]:
            expected=actual_ref(v);need(expected==oi[role],'actual Source entry operand aliases declared graph role');pair.value(f[k],expected,f'case{i}/actualFireEntry/{k}');receiver_checks+=1
        initial,final=pr['initial_slots'],pr['final_slots']
        observed={'layers':pr['layers'],'vertices':len(pr['vertices']),'loop_tests':sum(r['location']['pc']==193 for r in pr['slots']),'texture':initial['9'],'scale':initial['16']['F'],'initial_height':initial['20']['F'],'remaining':final['20']['F'],'initial_y_offset':initial['21']['F'],'z_offset':pr['z_arguments'][2]['F'],'view_yaw':oi['manager_yaw']['F']}
        for k,v in observed.items():pair.value(f[k],v,f'case{i}/actualSlotOrInvocation/{k}');slot_checks+=1
        pair.gl(f['gl'],o['after']['gl'],f'case{i}/actualFireFinalGL')
        batches=[r for r in groups['batch']if r['case']==i];need([r['ordinal']for r in batches]==list(range(len(batches))),'actual per-call driver ordinals')
        need(len(batches)==len(pr['batches']),'actual shadow/fire batch count')
        for j,(a,b)in enumerate(zip(batches,pr['batches'])):
            raw=b['source']['batch'];gl=b['gl'];label=f'case{i}/batch{j}'
            for k,v in [('fire',b['fire']),('mode',gl['mode']),('first',gl['first']),('count',gl['count'])]:pair.value(a[k],v,label+'/'+k)
            expected_nw={k:raw['nw.'+k]for k in ['h','o','p','r','w','l','m','n','q']};expected_nw['translation']=[raw['nw.'+k]['D']for k in ['s','t','u']];pair.value(a['nw'],expected_nw,label+'/actualNW')
            pair.value(a['words'],raw['words'],label+'/all8RawWords');geometry_words+=len(raw['words'])
            pair.gl({k:a[k]for k in ['modelview','projection','color','texture']},{k:gl[k]for k in ['modelview','projection','color','texture']},label+'/actualDriverGL')
            caps={r['cap']:r['enabled']for r in gl['caps']}
            for k,v in [('matrix_depth',gl['matrix_depth']),('lighting',caps[2896]),('texture_enabled',caps[3553]),('alpha',caps[3008])]:pair.value(a[k],v,label+'/'+k)
            live_batches+=1
        terrain=groups['terrain'][i];pair.name(terrain['name'],o['terrain_cache'],'texture');pair.value(terrain['keys'],[r['arguments'][0]for r in pr['texture_lookups']],f'case{i}/actualTextureLookupOrder');pair.value(terrain['calls'],len(pr['texture_lookups']),f'case{i}/actualTextureCallCount')
        if i==0:pair.value(terrain['transfer_hex'],o['terrain_transfer']['bytes'],'case0/actualDecodedTerrainTransfer')
    return {'status':'ACTUAL_ORIGINAL_BURNING_BILLBOARD_COMPARISON_PASS','typed_checks':pair.checks,'identity_pairs':len(pair.forward),'receiver_operand_checks':receiver_checks,'actual_slot_or_invocation_checks':slot_checks,'actual_driver_batches':live_batches,'all8_raw_NW_words':geometry_words,'texture_name_pairs':len(pair.textures),'matrix_maximum_absolute_error':pair.matrix_maximum,'matrix_signed_zero_differences':pair.matrix_signed_zeros,'device':device,'scope':'Seven exact post-only calls: selected modelgraph/parts/pool/GL, actual entry operands/aliases, input dimensions, Code-slot/loop results, texture order/decoded transfer, raw all8 NW words and real physical shadow/fire mode/count with live Native compatibility fields paired to Source driver state. Matrices use inherited finite4ULP rule only; all words/colors/enums exact. Raw entropy excluded cross-process and preserved within each process. No full Native actor/watcher, complete constructor trace, powered branch, pixels/general gameplay equivalence.'}

def compare_native(path,admitted,compiled=None):
    before=original.sha(path);result=compare_rows(original.rows(path),admitted);need(original.sha(path)==before,'Native observation bytes unchanged');recheck(admitted,compiled);result['native_output_sha256']=before;result['original_receipt_sha256']=admitted['projection']['capture_receipt_sha256'];result['original_manifest_negative_controls']=admitted['original_manifest_negative_controls']
    if compiled:result['actual_build_output_binding']={k:str(compiled[k])for k in ['receipt','receipt_sha256','executable','executable_sha256','source_input_count','output_scope']}
    else:result['producer_scope']='Read-only supplied observation comparison; no Native execution/build provenance credit.'
    return result

def negatives(data,admitted):
    compare_rows(data,admitted)  # A failing baseline cannot earn negative-control credit.
    rejected=[]
    def reject(name,change):
        bad=copy.deepcopy(data);change(bad)
        try:compare_rows(bad,admitted)
        except AssertionError:rejected.append(name)
        else:raise AssertionError('admitted copied observation corruption: '+name)
    fire=lambda d:next(r for r in d if r['event']=='fire')
    batch=lambda d:next(r for r in d if r['event']=='batch'and r['fire'])
    reject('wrong_scale',lambda d:fire(d).__setitem__('scale','00000000'))
    reject('wrong_actual_remaining_local',lambda d:fire(d).__setitem__('remaining','00000000'))
    reject('wrong_receiver_class',lambda d:fire(d)['receiver'].__setitem__('class','m'))
    reject('wrong_actor_alias',lambda d:fire(d)['actor'].__setitem__('identity',0))
    reject('wrong_UV_word',lambda d:batch(d)['words'].__setitem__(3,'00000000'))
    reject('wrong_inactive_raw_word',lambda d:batch(d)['words'].__setitem__(5,f"{int(batch(d)['words'][5],16)^1:08x}"))
    reject('missing_real_draw',lambda d:d.remove(batch(d)))
    reject('wrong_actual_lighting',lambda d:batch(d).__setitem__('lighting',True))
    reject('wrong_terrain_transfer',lambda d:next(r for r in d if r['event']=='terrain').__setitem__('transfer_hex','changed'))
    reject('wrong_framebuffer_alpha',lambda d:next(r for r in d if r['event']=='device')['framebuffer'].__setitem__('alpha',8))
    return rejected

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--build-receipt',type=Path);p.add_argument('--executable',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();need(bool(a.build_receipt)==bool(a.executable),'build receipt/executable supplied together');admitted=admit_original(a.original);compiled=admit_build(a.build_receipt,a.executable)if a.build_receipt else None;r=compare_native(a.native,admitted,compiled);r['rejected_copied_observations']=negatives(original.rows(a.native),admitted);a.output.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({k:r[k]for k in ['status','typed_checks','receiver_operand_checks','all8_raw_NW_words','rejected_copied_observations']}))
