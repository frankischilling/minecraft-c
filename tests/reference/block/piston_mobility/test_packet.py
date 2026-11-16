"""Portable synthetic corruption controls; no original/native runtime."""
import copy
import json
import packet as P

def repair(files):
    manifest=P.parse(files['publication.json']);manifest['files']={n:P.pin(raw) for n,raw in files.items() if n!='publication.json'};files['publication.json']=P.encoded(manifest)
def changed(files,name,action):
    value=P.parse(files[name]);action(value);files[name]=P.encoded(value);repair(files)
def observed(files,action):
    finite=P.parse(files['finite.json']);action(finite);files['finite.json']=P.encoded(finite)
    observations=P.parse(files['observations.json'])
    for mode in P.MODES:
        lines=['16 achievements','151 recipes','PISTON_MOBILITY_ORIGINS '+json.dumps(finite['origins'],separators=(',',':'))]
        lines+=['PISTON_MOBILITY_ROW '+json.dumps(r,separators=(',',':')) for r in finite['rows']]
        lines+=['PISTON_MOBILITY_COMPLETE '+json.dumps(finite['complete'],separators=(',',':'))]
        observations[mode]='\r\n'.join(lines)+'\r\n'
    files['observations.json']=P.encoded(observations)
    provenance=P.parse(files['provenance.json'])
    for mode in P.MODES:
        q=P.pin(observations[mode].encode());provenance['runs'][mode].update(stdout_bytes=q['bytes'],stdout_sha256=q['sha256'])
    files['provenance.json']=P.encoded(provenance);repair(files)
def main():
    base={p.name:p.read_bytes() for p in P.HERE.iterdir() if p.is_file()};result=P.audit(base);checks=1
    def reject(label,mutate):
        nonlocal checks
        files=copy.deepcopy(base);mutate(files)
        try:P.audit(files)
        except (ValueError,AssertionError,KeyError,TypeError,UnicodeError,IndexError):checks+=1;return
        raise RuntimeError('accepted synthetic corruption: '+label)
    reject('missing stream',lambda f:changed(f,'observations.json',lambda v:v.pop('default')))
    reject('missing file',lambda f:f.pop('finite.json'))
    reject('unlisted binary',lambda f:f.update({'game.jar':b'not an original binary'}))
    reject('physical helper hash',lambda f:f.update({'PistonMobilityObservation.java':f['PistonMobilityObservation.java']+b' '}))
    reject('manifest scope',lambda f:changed(f,'publication.json',lambda v:v.update(scope='live-Code proof')))
    reject('manifest malformed hash',lambda f:f.update({'publication.json':P.encoded({**P.parse(f['publication.json']),'files':{**P.parse(f['publication.json'])['files'],'finite.json':{'bytes':1,'sha256':'0'*64}}})}))
    reject('declaration extra operation',lambda f:changed(f,'cases.json',lambda v:v['forbidden'].remove('tick')))
    reject('reversed VM declaration',lambda f:changed(f,'cases.json',lambda v:v['modes'].reverse()))
    reject('World scope claim',lambda f:changed(f,'cases.json',lambda v:v.update(scope='World gameplay parity')))
    for mode in P.MODES:
        for field,value in [('exit',False),('exit',1),('timed_out',True),('pipe_capture_complete',False),('output_overflow',True),('reader_errors',{'stdout':['error']}),('stdout_bytes',1),('stdout_sha256','0'*64),('stderr_bytes',0),('stderr_sha256','0'*64),('command',['java','other-helper']),('timeout_seconds',61)]:
            reject(mode+' '+field+' '+str(value),lambda f,m=mode,k=field,v=value:changed(f,'provenance.json',lambda x:x['runs'][m].update({k:v})))
    reject('missing receipt',lambda f:changed(f,'provenance.json',lambda v:v['runs'].pop('default')))
    reject('missing compiler stdout',lambda f:changed(f,'provenance.json',lambda v:v['compile'].update(stdout_bytes=1)))
    reject('wrong compiler command',lambda f:changed(f,'provenance.json',lambda v:v['compile'].update(command=['javac'])))
    reject('compiler failed',lambda f:changed(f,'provenance.json',lambda v:v['compile'].update(exit=1)))
    reject('changed compiled product',lambda f:changed(f,'provenance.json',lambda v:v['products_after']['PistonMobilityObservation.class'].update(sha256='0'*64)))
    reject('changed original input',lambda f:changed(f,'provenance.json',lambda v:v['inputs_after']['original-jar'].update(sha256='0'*64)))
    reject('JVM environment options restored',lambda f:changed(f,'provenance.json',lambda v:v.update(removed_environment=[])))
    reject('unsupported live-Code claim',lambda f:changed(f,'provenance.json',lambda v:v.update(scope='Live-Code native gameplay parity')))
    reject('excluded native claim removed',lambda f:changed(f,'provenance.json',lambda v:v['excluded'].remove('native execution or parity')))
    reject('wrong class count',lambda f:changed(f,'provenance.json',lambda v:v['archive_inventory'].update(classes=77)))
    reject('wrong Code count',lambda f:changed(f,'provenance.json',lambda v:v['archive_inventory'].update(Code_methods=808)))
    reject('omitted original row with repaired stream receipts',lambda f:observed(f,lambda v:v['rows'].pop()))
    reject('duplicate original row with repaired stream receipts',lambda f:observed(f,lambda v:v['rows'].append(v['rows'][0])))
    reject('reordered original rows with repaired stream receipts',lambda f:observed(f,lambda v:v['rows'].reverse()))
    reject('boolean original id',lambda f:observed(f,lambda v:v['rows'][0].update(id=True)))
    reject('Material used for bed override',lambda f:observed(f,lambda v:v['rows'][25].update(mobility=0,repeated_mobility=0)))
    reject('Material used for detector rail override',lambda f:observed(f,lambda v:v['rows'][27].update(mobility=1,repeated_mobility=1)))
    reject('forged override owner',lambda f:observed(f,lambda v:v['rows'][27].update(declaring_class='ph')))
    reject('receiver changed',lambda f:observed(f,lambda v:v['rows'][0].update(registered_identity=False)))
    reject('Material alias changed',lambda f:observed(f,lambda v:v['rows'][0].update(material_same=False)))
    reject('different generic leaf with all row/stream pins repaired',lambda f:observed(f,lambda v:v['rows'][0].update(material_mobility=1,mobility=1,repeated_mobility=1)))
    reject('denied source write',lambda f:observed(f,lambda v:v['complete'].update(writes=1)))
    reject('missing completion operation',lambda f:observed(f,lambda v:v['complete'].update(h_calls=191)))
    reject('extra origin field',lambda f:observed(f,lambda v:v['origins'].update(live_Code=True)))
    reject('archive override Code changed',lambda f:changed(f,'source-pins.json',lambda v:next(m for m in v['classes']['bv']['methods'] if m['name']=='h' and m['descriptor']=='()I').update(code_sha256='0'*64)))
    reject('archive unrelated method changed',lambda f:changed(f,'source-pins.json',lambda v:v['classes']['uu']['methods'][0].update(code_sha256='0'*64)))
    reject('native table substitution',lambda f:(f.update({'native-observations.h':f['native-observations.h'].replace(b'{26, 1}',b'{26, 0}')}),repair(f)))
    reject('native table row missing',lambda f:(f.update({'native-observations.h':f['native-observations.h'].replace(b'{96, 0},',b'')}),repair(f)))
    reject('stream footer missing',lambda f:changed(f,'observations.json',lambda v:v.update(default=v['default'].split('PISTON_MOBILITY_COMPLETE')[0])))
    reject('stream banner duplicated',lambda f:changed(f,'observations.json',lambda v:v.update(default='151 recipes\r\n'+v['default'])))
    reject('raw stream byte changed',lambda f:changed(f,'observations.json',lambda v:v.update(default=v['default'].replace('PISTON_MOBILITY_ROW','PISTON_MOBILITY_ROX',1))))
    # Public JSON must reject duplicate keys even if ordinary json.loads would
    # silently replace one. This is independent from the packet hash manifest.
    try:P.parse(b'{"value":1,"value":2}')
    except (ValueError,AssertionError):checks+=1
    else:raise RuntimeError('duplicate JSON key accepted')
    print('PASS: '+str(checks)+' Piston mobility packet controls; original-vms='+str(result['original_vms'])+'; original-blocks='+str(result['registered_blocks'])+'; synthetic-reader-only=1')
if __name__=='__main__':main()
