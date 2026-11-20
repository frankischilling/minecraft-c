"""Pure synthetic protocol/receipt controls; never execute an observation child."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True


def frame(pc=0):
    return {'class':None, 'method':None, 'descriptor':None, 'pc':pc}


def native_diagnostic(status=0, pc=0, helper=False):
    result = {'status':status, 'site':frame(pc)}
    if not helper:
        result.update(dependency=frame(), operation=None, native_policy=False,
                      caught_source_io=False, frames=[], pack_status=0,
                      cleanup_pack_status=0, array_status=0, array_site=frame(),
                      texture_status=0, texture_pc=0, buffer_status=0, owner_status=0)
    return result


def array(identity=1, length=1):
    return {'id':identity, 'length':length, 'hex':'01abcdef' * length}


def state():
    return {'b':[], 'c':[], 'd_count':0, 'j':False, 'k':False, 'ledger':[],
            'e':[0,1,1], 'e0':0, 'f':[0,1,1], 'l_null':False, 'selected_null':False}


def fixture(api, mode, profile, runtime):
    # Arbitrary parser data. These are structural contracts, not game answers.
    events = []
    for at, label in enumerate(api.PROFILES[mode][profile]):
        static = profile in ('statics','first-setter') and label != 'cache-mutated'
        helper = profile == 'helpers'
        row = {'label':label, 'outcome':'return'}
        if static:
            row['array'] = array(1)
            if label.endswith('-initial'):
                row['array'] = {'id':at+10,'length':65536,'hex':'00000000'*65536}
            if label.endswith('-set'): row['same'] = True
        else:
            row['exception'] = None
            if mode == 'color':
                row['array'] = array(1 if label in ('water','cache','cache-mutated') else at+2)
                if helper:
                    length = {'fill-6':6,'fill-8':8,'fill-0':0,'fill-1':1,'fill-5':5,'null-image-fill':1}.get(label)
                    row['destination'] = None if length is None else array(at+20,length)
                    if label.startswith('fill-') and length is not None: row['array'] = copy.deepcopy(row['destination'])
                    if label == 'new': row['image_plane_independent'] = True
                    if label == 'fill-null': row['array'] = None
                else: row['state'] = state()
            else:
                row.update(id=23, state=state(), bound_before=0, bound_after=0,
                           width=1, height=1, gpu='01234567', transfer='89abcdef', gl_error=0)
            null_call = label in ('nonimage','null-key','null-list-cache','null-pack-miss','null-image-new','null-image-fill')
            short = helper and label in ('fill-0','fill-1','fill-5')
            frontier = runtime == 'native' and (mode,profile,label) in api.FRONTIERS
            if null_call or short:
                row.update(outcome='throw', exception='java.lang.NullPointerException' if null_call else 'java.lang.ArrayIndexOutOfBoundsException')
            if frontier: row.update(outcome='frontier', exception=None)
            if row['outcome'] != 'return':
                if mode == 'color': row['array'] = None
                else: row.update(id=None, width=None, height=None, gpu=None, transfer=None)
            if runtime == 'native':
                pc = 20 if helper else 52 if mode == 'color' and label == 'strip' else 193 if mode == 'color' else 77 if label == 'strip' else 221 if label == 'corrupt' else 52
                status = {'return':0,'throw':6,'frontier':8 if helper else 11}[row['outcome']]
                row['native'] = native_diagnostic(status, pc if frontier else 0, helper)
        events.append(row)
    return {'schema':1,'runtime':runtime,'mode':mode,'profile':profile,
            'setup':{'actual_app':True,'app_z_alias':True,'pack':'vp' if profile == 'zip' else 'pa','engine_i_null':mode == 'color'},
            'device':None if mode == 'color' else {'vendor':'synthetic','renderer':'parser','version':'data'},'events':events}


def invalid_cases(api):
    result = []
    def add(name, record, edit):
        value = copy.deepcopy(record); edit(value)
        result.append((name,value))
    # Exercise every field in every emitted shape, rather than just the first row.
    for mode, profiles in api.PROFILES.items():
        for profile in profiles:
            for runtime in ('original','native'):
                value = fixture(api,mode,profile,runtime)
                for at, row in enumerate(value['events']):
                    for field in row:
                        add('missing/'+mode+'/'+profile+'/'+runtime+'/'+row['label']+'/'+field,
                            value, lambda x,at=at,field=field:x['events'][at].pop(field))
    native = fixture(api,'color','default','native')
    original = fixture(api,'color','default','original')
    for status in (7,0.0,False,True):
        add('native-return-status/'+repr(status), native, lambda x,status=status:x['events'][2]['native'].update(status=status))
    for field in native['events'][2]['native']:
        add('missing-native-diagnostic/'+field, native, lambda x,field=field:x['events'][2]['native'].pop(field))
    for field in ('pack_status','cleanup_pack_status','array_status','texture_status','texture_pc','buffer_status','owner_status'):
        for invalid in (False,0.0,-1,1 << 40):
            add('native-child-type/'+field+'/'+repr(invalid), native, lambda x,field=field,invalid=invalid:x['events'][2]['native'].update({field:invalid}))
    for field in ('native_policy','caught_source_io'):
        for invalid in (0,1,None):
            add('native-Boolean/'+field+'/'+repr(invalid), native, lambda x,field=field,invalid=invalid:x['events'][2]['native'].update({field:invalid}))
    for invalid in (False,0.0,'0'):
        add('native-frame-pc/'+repr(invalid),native,lambda x,invalid=invalid:x['events'][2]['native']['site'].update(pc=invalid))
    add('native-frame-missing-class',native,lambda x:x['events'][2]['native']['site'].pop('class'))
    add('native-frame-wrong-text',native,lambda x:x['events'][2]['native']['site'].update(method=3))
    add('native-frames-typed',native,lambda x:x['events'][2]['native'].update(frames=[False]))
    add('native-operation-typed',native,lambda x:x['events'][2]['native'].update(operation=7))
    for runtime, value in (('original',original),('native',native)):
        for invalid in ('java.lang.Error',False,0,{},[]):
            add('return-exception/'+runtime+'/'+repr(invalid),value,lambda x,invalid=invalid:x['events'][2].update(exception=invalid))
        add('return-array-null/'+runtime,value,lambda x:x['events'][2].update(array=None))
        add('foreign-destination/'+runtime,value,lambda x:x['events'][2].update(destination=None))
    thrown = fixture(api,'color','priority','native')
    for invalid in (None,False,'','java/lang/NullPointerException','java.lang.Error'):
        add('throw-exception/'+repr(invalid),thrown,lambda x,invalid=invalid:x['events'][1].update(exception=invalid))
    for invalid in (0,6.0,True,11):
        add('throw-status/'+repr(invalid),thrown,lambda x,invalid=invalid:x['events'][1]['native'].update(status=invalid))
    for runtime in ('original','native'):
        texture = fixture(api,'texture','default',runtime)
        for field in ('id','bound_before','bound_after','width','height','gl_error'):
            for invalid in (False,1.0,'1'):
                add('texture-integer/'+runtime+'/'+field+'/'+repr(invalid),texture,lambda x,field=field,invalid=invalid:x['events'][1].update({field:invalid}))
        add('texture-id-null/'+runtime,texture,lambda x:x['events'][1].update(id=None))
        add('texture-transfer-null/'+runtime,texture,lambda x:x['events'][1].update(transfer=None))
        add('texture-gpu-null-with-extents/'+runtime,texture,lambda x:x['events'][1].update(gpu=None))
        add('texture-unbound-extent/'+runtime,texture,lambda x:x['events'][1].update(width=None))
        failed = fixture(api,'texture','priority',runtime)
        for field, invalid in (('id',23),('transfer',''),('gpu','')):
            add('failed-texture-payload/'+runtime+'/'+field,failed,lambda x,field=field,invalid=invalid:x['events'][1].update({field:invalid}))
    frontier = fixture(api,'color','frontiers','native')
    for invalid in (11.0,True,0,7):
        add('frontier-status/'+repr(invalid),frontier,lambda x,invalid=invalid:x['events'][1]['native'].update(status=invalid))
    add('frontier-exception',frontier,lambda x:x['events'][1].update(exception='java.lang.Exception'))
    add('frontier-array',frontier,lambda x:x['events'][1].update(array=array(30)))
    helpers = fixture(api,'color','helpers','native')
    add('helper-wrong-destination-length',helpers,lambda x:x['events'][1].update(destination=array(80,7)))
    add('helper-return-array-not-alias',helpers,lambda x:x['events'][1].update(array=array(80,6)))
    add('helper-null-destination',helpers,lambda x:x['events'][1].update(destination=None))
    add('helper-status-Boolean',helpers,lambda x:x['events'][1]['native'].update(status=False))
    add('helper-dropped-site',helpers,lambda x:x['events'][1]['native'].pop('site'))
    add('helper-forged-whole-state',helpers,lambda x:x['events'][1].update(state=state()))
    return result


def root_five(api):
    results=[]
    for label in ('missing-array','missing-state','missing-exception','nonzero-success-status','floating-success-status'):
        original=fixture(api,'color','default','original'); native=fixture(api,'color','default','native')
        if label.startswith('missing-'):
            field=label[len('missing-'):]
            original['events'][2].pop(field); native['events'][2].pop(field)
        else: native['events'][2]['native']['status']=7 if label == 'nonzero-success-status' else 0.0
        try:
            result=api.compare(original,native); admission=result['status']; rejected=False
        except (ValueError,TypeError,KeyError) as e:
            admission=str(e); rejected=True
        results.append({'case':label,'rejected':rejected,'actual_admission':admission,'synthetic_parser_data_only':True})
    return results


def replay_controls(api, capture, work):
    # These are inert text artifacts and synthetic receipts, never executable,
    # class, ZIP, resource or game fixtures. captured_native must never run().
    work.mkdir()
    executable = work/'inert-program.txt'; executable.write_text('inert identity\n')
    config = work/'inert-config.txt'; config.write_text('inert configuration\n')
    qualification = work/'qualification.json'; qualification.write_text('{"synthetic":true}\n')
    manifest = work/'products.json'
    products = {'executable':{'path':str(executable.resolve()),'pin':api.fingerprint(executable)},
                'artifacts':{str(p.resolve()):api.fingerprint(p) for p in (executable,config)},
                'qualification_receipt':{'path':str(qualification.resolve()),'pin':api.fingerprint(qualification)}}
    manifest.write_text(json.dumps(products))
    manifest_sha = api.fingerprint(manifest)['sha256']
    run = work/'closed'; run.mkdir()
    stdout = run/'native.stdout'; stdout.write_bytes(b'synthetic-stream-A\n')
    stderr = run/'native.stderr'; stderr.write_bytes(b'')
    command = [str(executable.resolve()),'synthetic-resources','synthetic-directory','default']
    recorded = {'command':command,'exit':0,'reaped':True,'pipe_capture_complete':True,
                'reader_errors':{},'timed_out':False,'output_overflow':False,
                'stdout_bytes':stdout.stat().st_size,'stderr_bytes':0,'timeout_seconds':180,'limit':api.LIMIT}
    cap = run/'native.capture.json'; cap.write_text(json.dumps(recorded))
    banners = run/'native-source-stdout.raw'; banners.write_bytes(b'')
    context = {'mode':'color','profile':'default','source_root':str(work.resolve()),
               'source_before':{'synthetic.txt':{'bytes':1,'sha256':'0'*64}},
               'resources_before':{'inert.txt':{'bytes':1,'sha256':'1'*64}},'directory_before':{'inert.txt':{'bytes':1,'sha256':'2'*64}}}
    native = {'capture':recorded,'stdout':api.fingerprint(stdout),'stderr':api.fingerprint(stderr),
              'artifact_manifest':{'path':str(manifest.resolve()),'pin':api.fingerprint(manifest)},'actual_product_map':products}
    report = dict(context,status='NATIVE_OBSERVATION_ADMITTED',source_after=context['source_before'],native=native,
                  native_source_banners=api.fingerprint(banners),numeric_ID_mapping_performed=False,
                  constructor_startup_or_playable_claim=False,input_profiles_executed=1)
    receipt = run/'REPORT.json'; receipt.write_text(json.dumps(report))
    anchor = api.fingerprint(receipt)['sha256']
    saved = {p:p.read_bytes() for p in (stdout,stderr,cap,banners,receipt,config)}
    # Supporting an original receipt must authenticate those streams too.
    for suffix, data in (('stdout',b'synthetic-original\n'),('stderr',b'synthetic-logs\n'),('source-stdout.raw',b'synthetic-banner\n')):
        (run/('original.'+suffix)).write_bytes(data)
    original_capture = dict(recorded,stdout_bytes=len(b'synthetic-original\n'),stderr_bytes=len(b'synthetic-logs\n'))
    (run/'original.capture.json').write_text(json.dumps(original_capture))
    results = []
    def invoke(output, expected_anchor=anchor, current_context=context):
        if not hasattr(capture,'closed_replay'):
            return capture.captured_native(work,executable,command[1:],output,{},manifest,manifest_sha,stdout)
        return capture.captured_native(work,executable,command[1:],output,{},manifest,manifest_sha,stdout,receipt,expected_anchor,current_context)
    def attempt(label, mutate=None, expected=False, legacy=False, context_override=None):
        for path, data in saved.items(): path.write_bytes(data)
        if mutate: mutate()
        output = work/('out-'+str(len(results))); output.mkdir()
        try:
            if legacy:
                capture.captured_native(work,executable,command[1:],output,{},manifest,manifest_sha,stdout)
            else:
                invoke(output,anchor,context if context_override is None else context_override)
            accepted = True; error = None
        except (ValueError,TypeError,KeyError) as error_value:
            accepted = False; error = str(error_value)
        results.append({'case':label,'accepted':accepted,'expected':expected,'error':error})
    attempt('unauthenticated-legacy-replay',legacy=True)
    attempt('trusted-closed-replay',expected=True)
    attempt('same-length-stdout-substitution',lambda:stdout.write_bytes(b'synthetic-stream-B\n'))
    def retie():
        stdout.write_bytes(b'synthetic-stream-B\n'); changed=copy.deepcopy(report)
        changed['native']['stdout']=api.fingerprint(stdout); receipt.write_text(json.dumps(changed))
    attempt('retied-stream-and-report-with-old-anchor',retie)
    attempt('retied-capture-same-extents',lambda:cap.write_text(json.dumps(dict(recorded,command=command+['forged']))))
    attempt('capture-float-integer-alias',lambda:cap.write_text(json.dumps(dict(recorded,exit=0.0))))
    attempt('product-changed',lambda:config.write_text('changed configuration\n'))
    for status in ('RUNNING','FAIL_OR_REFUSAL','MISMATCH','REFUSAL','unrecognized'):
        # Even a separately anchored rejected-status report is inadmissible.
        def rejected(status=status):
            changed=dict(report,status=status); receipt.write_text(json.dumps(changed))
        for path,data in saved.items(): path.write_bytes(data)
        rejected(); bad_anchor=api.fingerprint(receipt)['sha256']
        output=work/('out-'+str(len(results))); output.mkdir()
        try:
            invoke(output,bad_anchor,context)
            accepted=True; error=None
        except (ValueError,TypeError,KeyError) as e: accepted=False; error=str(e)
        results.append({'case':'anchored-status-'+status,'accepted':accepted,'expected':False,'error':error})
    for field in ('mode','profile','source_root','source_before','resources_before','directory_before'):
        changed=copy.deepcopy(context); changed[field]='different' if isinstance(changed[field],str) else {}
        attempt('current-context-'+field,context_override=changed)
    # An anchored valid report that contains original evidence must reject any
    # changed original stream, even though native streams remain byte-identical.
    for path,data in saved.items(): path.write_bytes(data)
    original = {'capture':original_capture,'stdout':api.fingerprint(run/'original.stdout'),
                'stderr':api.fingerprint(run/'original.stderr'),'source_banners':api.fingerprint(run/'original.source-stdout.raw')}
    # Actual source banner path uses a hyphen, matching controller.py.
    (run/'original-source-stdout.raw').write_bytes((run/'original.source-stdout.raw').read_bytes())
    original_report=dict(report,original=original); receipt.write_text(json.dumps(original_report))
    original_anchor=api.fingerprint(receipt)['sha256']
    for label, tamper in [('original-complete',False),('original-same-length-substitution',True)]:
        output=work/('out-'+str(len(results))); output.mkdir()
        if tamper: (run/'original.stdout').write_bytes(b'synthetic-substitX\n')
        try:
            invoke(output,original_anchor,context)
            accepted=True; error=None
        except (ValueError,TypeError,KeyError) as e: accepted=False; error=str(e)
        results.append({'case':label,'accepted':accepted,'expected':not tamper,'error':error})
    return results


def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--implementation',type=Path,required=True)
    parser.add_argument('--work',type=Path); args=parser.parse_args()
    sys.path.insert(0,str(args.implementation.resolve(strict=True)))
    import protocol as api
    import capture
    accepted=[]; rejected_valid=[]; total=0
    for mode,profiles in api.PROFILES.items():
        for profile in profiles:
            for runtime in ('original','native'):
                value=fixture(api,mode,profile,runtime)
                try: api.validate(value,mode,profile,runtime)
                except (ValueError,TypeError,KeyError) as e:
                    rejected_valid.append({'case':mode+'/'+profile+'/'+runtime,'error':str(e)})
    for name,value in invalid_cases(api):
        total+=1
        try: api.validate(value,value['mode'],value['profile'],value['runtime'])
        except (ValueError,TypeError,KeyError): pass
        else: accepted.append(name)
    five=root_five(api)
    if args.work is None:
        import tempfile
        with tempfile.TemporaryDirectory(prefix='mc-lookup-admission-') as temporary:
            replay=replay_controls(api,capture,Path(temporary)/'controls')
    else:
        replay=replay_controls(api,capture,args.work)
    result={'scope':'Pure synthetic Python admission controls only; no observer/game/native/JVM/compiler/fixture execution',
            'optimization':sys.flags.optimize,'protocol':api.fingerprint(Path(api.__file__)),
            'capture':api.fingerprint(Path(capture.__file__)),
            'valid_profiles':sum(len(profiles)*2 for profiles in api.PROFILES.values()),
            'rejected_valid':rejected_valid,'invalid_cases':total,'admitted_invalid':accepted,'root_five':five,'replay_cases':replay}
    print(json.dumps(result,indent=2))
    return 1 if rejected_valid or accepted or not all(x['rejected'] for x in five) or any(x['accepted'] != x['expected'] for x in replay) else 0


if __name__ == '__main__': sys.exit(main())
