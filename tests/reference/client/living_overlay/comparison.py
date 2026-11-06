"""Finite overlay comparison; ordinary/JDI source and actual Native stay separate.

Public entry points: admit_original, admit_build, recheck, compare_native.
The caller admits original/build inputs before independent Native execution,
then compares its output. No Original value becomes a target execution input.
"""
import argparse,copy,importlib.util,json,re
from pathlib import Path
BASE=Path(__file__).resolve().parent
ROOT=next(p for p in BASE.parents if (p/'CMakeLists.txt').is_file())
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
original=module('living_overlay_original',BASE/'original.py')
supplement=module('living_overlay_supplement',BASE/'supplement.py')
build=module('living_overlay_build',BASE/'build.py')
shared=module('living_overlay_shared_pair',ROOT/'tests/tools/client/test_creature_draw.py')
need=original.need


def admit_original(path,device_path=None):
    receipt,modes,events,projection=original.audit(path)
    for mode in modes:modes[mode]['all_rows']=original.rows(path/mode/'target.ndjson')
    supplement_data=supplement.admit(device_path,path,modes,projection) if device_path else None
    return {'original_manifest_negative_controls':original.manifest_negatives(receipt,original.public_inputs()['primary']),'path':path,'receipt':receipt,'modes':modes,'events':events,'projection':projection,'supplement':supplement_data,'device_path':device_path}


def admit_build(path,executable):return build.admit(path,executable)


def recheck(admitted,compiled=None):
    proof=original.public_inputs()['primary'];original.fingerprints(proof['input_sha256'])
    for name,pin in proof['artifacts_sha256'].items():need(original.sha(admitted['path']/name)==pin,'final immutable original output: '+name)
    need(original.sha(admitted['path']/'receipt.json')==proof['receipt_sha256'],'final original receipt')
    if admitted['device_path']:
        proof=original.public_inputs()['windows_normal_supplement'];original.fingerprints(proof['input_sha256'])
        for name,pin in proof['artifacts_sha256'].items():need(original.sha(admitted['device_path']/name.replace('\\','/'))==pin,'final matching-device output')
        need(original.sha(admitted['device_path']/'receipt.json')==proof['receipt_sha256'],'final matching-device receipt')
    if compiled:build.recheck(compiled)


def same_primary_device(native,source):
    need(native['vendor']==source['vendor'] and native['renderer']==source['renderer'],'actual Native and primary source GPU vendor/renderer')
    suffix=lambda v:re.search(r'\bMesa .+$',v)
    a,b=suffix(native['version']),suffix(source['version']);need(a and b and a[0]==b[0],'actual matching Mesa driver; cross-driver normals need explicit ordinary supplement')
    return {'native':native,'ordinary_source':source,'driver':a[0]}


class Pair(shared.Pair):
    def __init__(self,normal_data=None):
        super().__init__();self.normal_data=normal_data;self.normal_supplements=[]
    def gl(self,actual,expected,label):
        right=dict(expected)
        if self.normal_data:
            m=re.fullmatch(r'case([0-6])/(before|after|actualFinalGL)(?:/gl)?',label);need(m is not None,'finite actual current-normal location')
            key=int(m[1]),'after' if m[2]=='actualFinalGL' else m[2];same=self.normal_data['normals'][key]
            self.normal_supplements.append({'location':label+'/normal','Native_raw':actual['normal'],'primary_Linux_raw':expected['normal'],'matching_device_ordinary_raw':same})
            right['normal']=same
        super().gl(actual,right,label)


def compare_rows(data,admitted):
    modes,projection=admitted['modes'],admitted['projection'];source=modes['debugger']['draws']
    textures=[r for r in data if r['event']=='texture'];states=[r for r in data if r['event']=='state'];overlays=[r for r in data if r['event']=='overlay'];draws=[r for r in data if r['event']=='draw'];controls=[r for r in data if r['event']=='control']
    need(len(textures)==6 and [r['index'] for r in textures]==list(range(6)),'six actual ordered Native texture setups')
    need([r['case'] for r in states]==[i for i in range(7) for p in range(2)] and [r['phase'] for r in states]==['before','after']*7,'complete ordered Native before/after states')
    need([(r['case'],r['kind']) for r in overlays]==[(i,k) for i in range(7) for k in ('hurt','packed')],'all ordered actual overlay receipts')
    need([r['case'] for r in draws]==list(range(7)),'all seven actual draw completions')
    need([r['name'] for r in controls]==['zero_alpha_white_rgb','visible_hurt_pixels','floating_prefix'] and all(r['event']=='control' for r in data[-3:]),'three separate Native-only controls; no source equivalence credit')
    need(len(data)==6+14+14+7+3,'no omitted or undeclared Native transport records')
    native_device=draws[0].get('device');need(type(native_device) is dict and {'vendor','renderer','version','major','minor','profile'}<=set(native_device),'actual Native target device metadata')
    if admitted['supplement']:device=supplement.matching_device(native_device,admitted['supplement'])
    else:device=same_primary_device(native_device,modes['debugger']['device'][0])
    pair=Pair(admitted['supplement']);model_arguments=0;model_receivers=0;overlay_checks=0
    reference_textures=[r for r in modes['debugger']['all_rows'] if r['event']=='texture_lookup'];need(len(reference_textures)==6,'six actual source texture setups')
    for actual,expected in zip(textures,reference_textures):
        pair.value(actual['key'],expected['key'],'texture/key');pair.name(actual['name'],expected['name'],'texture');pair.name(actual['cached_name'],expected['cached_name'],'texture')
        pair.value(actual['transfer_hex'],expected['transfer_hex'],'texture/actual-decoded-transfer')
    for i,expected in enumerate(source):
        before,after=states[2*i:2*i+2];projected=projection['cases'][i]
        need(before['entropy']==after['entropy'],'Native actual five raw entropy contexts unchanged; never source seed equality')
        for native,phase in [(before,'before'),(after,'after')]:
            observed=expected[phase];pair.graph(native['graph'],observed['graph'],f'case{i}/{phase}/graph')
            for key in ['parts','pool','pool_storage']:pair.value(native[key],observed[key],f'case{i}/{phase}/{key}')
            pair.gl(native['gl'],observed['gl'],f'case{i}/{phase}/gl')
        bridge={shared.reference(r['actual']):r['identity'] for r in projected['entry_identity_bridge']}
        def observed_model(value):
            ref=shared.reference(value);need(ref in bridge,'actual model receiver exists in observed source graph identity bridge');return {'class':ref[0],'identity':bridge[ref]}
        for j,label in enumerate(['hurt','packed']):
            n=overlays[2*i+j];o=projected['overlays'][label];pair.value(n['entered'],o['entered'],f'case{i}/{label}/entered');overlay_checks+=1
            if o['entered']:
                pair.value(n['rgba'],[v['F'] for v in o['rgba']],f'case{i}/{label}/actualRGBA')
                results=[p['result'] for p in o['predicates']];pair.value(n['passes'],results,f'case{i}/{label}/actualVirtualB')
                primary=o['primary_calls'][0];pair.value(n['primary_model'],observed_model(primary['receiver']),f'case{i}/{label}/actualPrimaryReceiver');model_receivers+=1
                pair.value(n['primary_arguments'],[v['F'] for v in primary['arguments']],f'case{i}/{label}/actualSixPrimaryArguments');model_arguments+=6
                primary_pc=488 if label=='hurt' else 648;pair.value(n['primary_parts'],sum(r['model_caller']['pc']==primary_pc for r in o['source_list_calls']),f'case{i}/{label}/actualPrimaryPartCalls')
                selected=iter(o['secondary_calls'])
                need(len(n['secondary_models'])==len(n['secondary_arguments'])==4,'all actual secondary receipt slots')
                for p,called in enumerate(results):
                    if called:
                        call=next(selected);need(call['sequence']>o['predicates'][p]['sequence'],'actual selected model follows its true returned b predicate')
                        pair.value(n['secondary_models'][p],observed_model(call['receiver']),f'case{i}/{label}/actualSecondaryReceiver{p}');model_receivers+=1
                        pair.value(n['secondary_arguments'][p],[v['F'] for v in call['arguments']],f'case{i}/{label}/actualSixSecondaryArguments{p}');model_arguments+=6
                    else:pair.value(n['secondary_models'][p],None,f'case{i}/{label}/noSecondaryReceiver{p}');pair.value(n['secondary_arguments'][p],[],f'case{i}/{label}/noSecondaryArguments{p}')
                overlay_checks+=7
            else:
                for key,value in [('primary_parts',0),('primary_model',None),('primary_arguments',[]),('passes',[False]*4),('secondary_models',[None]*4),('secondary_arguments',[[]]*4)]:pair.value(n[key],value,f'case{i}/{label}/absent-'+key)
        for key,source_key in [('aa','ls.aa'),('ad','ls.ad')]:pair.value(draws[i][key],projected['selected_fields'][source_key],f'case{i}/actual-selected-'+key)
        pair.value(draws[i]['color'],projected['color_hook_result'],f'case{i}/actual-virtual-color-result')
        pair.value(draws[i]['batches'],6*len(projected['source_list_calls']),f'case{i}/actualPhysicalEqualBatches')
        pair.gl(draws[i]['gl'],expected['after']['gl'],f'case{i}/actualFinalGL')
    return {'status':'ORIGINAL_SELECTED_OVERLAY_COMPARISON_PASS','typed_checks':pair.checks,'overlay_checks':overlay_checks,'model_receiver_checks':model_receivers,'actual_model_argument_words':model_arguments,'identity_pairs':len(pair.forward),'list_name_pairs':len(pair.lists),'texture_name_pairs':len(pair.textures),'matrix_maximum_absolute_error':pair.matrix_maximum,'matrix_signed_zero_differences':pair.matrix_signed_zeros,'device':device,'current_normal_supplement':pair.normal_supplements,'scope':'Actual seven-profile selected modelgraph/parts/pool/GL, raw per-process stream invariance, aa/ad/color results, actual overlay receiver class/role/aliases and six arguments, virtual-b outcomes, real part-list/physical batch counts. Full Native actor/watcher, Native live GL invocation order, ordinary source pixels, driver cross-comparison and whole-game equivalence are not claimed. Independent constructor entropy stays raw.'}


def compare_native(path,admitted,compiled=None):
    before=original.sha(path);data=original.rows(path);result=compare_rows(data,admitted);need(original.sha(path)==before,'actual supplied Native output remains unchanged');result['native_output_sha256']=before;result['original_receipt_sha256']=admitted['projection']['capture_receipt_sha256'];result['original_manifest_negative_controls']=admitted['original_manifest_negative_controls'];recheck(admitted,compiled)
    if compiled:result['actual_build_output_binding']={k:str(compiled[k]) for k in ['receipt','receipt_sha256','executable','executable_sha256','source_input_count','output_scope']}
    else:result['producer_scope']='Read-only comparison of supplied Native observations; target execution/build provenance is not established by this call.'
    return result


def negatives(data,admitted):
    controls=[]
    def reject(name,mutation):
        changed=copy.deepcopy(data);mutation(changed)
        try:compare_rows(changed,admitted)
        except AssertionError:controls.append(name)
        else:raise AssertionError('copied-observation comparator admitted corruption: '+name)
    first=lambda d:next(r for r in d if r['event']=='overlay' and r['entered'])
    reject('wrong_actual_RGBA',lambda d:first(d)['rgba'].__setitem__(3,'00000000'))
    reject('wrong_model_argument',lambda d:first(d)['primary_arguments'].__setitem__(0,'00000000'))
    reject('wrong_model_class',lambda d:first(d)['primary_model'].__setitem__('class','il'))
    reject('missing_physical_batch',lambda d:next(r for r in d if r['event']=='draw' and r['batches']).__setitem__('batches',0))
    reject('forged_Creeper_secondary_pass',lambda d:next(r for r in d if r['event']=='overlay' and r['case']==3 and r['kind']=='packed')['passes'].__setitem__(0,True))
    reject('wrong_hurt_field',lambda d:next(r for r in d if r['event']=='draw').__setitem__('aa',0))
    reject('changed_matching_device',lambda d:next(r for r in d if r['event']=='draw')['device'].__setitem__('renderer','unobserved-device'))
    return controls


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);p.add_argument('--device-original',type=Path);p.add_argument('--native',type=Path,required=True);p.add_argument('--build-receipt',type=Path);p.add_argument('--executable',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    need(bool(a.build_receipt)==bool(a.executable),'build receipt and actual executable are supplied together')
    admitted=admit_original(a.original,a.device_original);compiled=admit_build(a.build_receipt,a.executable) if a.build_receipt else None;r=compare_native(a.native,admitted,compiled);r['rejected_copied_observations']=negatives(original.rows(a.native),admitted);a.output.write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({k:r[k] for k in ['status','typed_checks','model_receiver_checks','actual_model_argument_words','identity_pairs','rejected_copied_observations']}))
