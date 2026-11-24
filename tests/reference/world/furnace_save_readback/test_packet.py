"""Portable packet corruption controls; no original runtime required."""
import copy
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import packet as P
import publish as U
import tempfile
def replace(blobs,name,value):
    out=dict(blobs)
    out[name]=(json.dumps(value,indent=2)+'\n').encode('utf8')
    manifest=P.parse(out['publication.json'])
    manifest['files'][name]={'bytes':len(out[name]),'sha256':P.digest(out[name])}
    out['publication.json']=(json.dumps(manifest,indent=2)+'\n').encode('utf8')
    return out
def rejected(operation,name):
    try: operation()
    except (AssertionError,ValueError,KeyError,TypeError,IndexError): return name
    raise AssertionError('corruption accepted: '+name)
def main():
    blobs=P.files(Path(__file__).resolve().parent)
    P.audit(blobs)
    results=[]
    bad=dict(blobs);bad['default.stdout']=bad['default.stdout'].replace(b'1397',b'1398')
    results.append(rejected(lambda:P.audit(bad),'raw original stream bytes changed'))
    bad=P.parse(blobs['finite.json']);bad['default']['row']['furnace']['fields'][0]+=1
    bad=replace(blobs,'finite.json',bad)
    results.append(rejected(lambda:P.audit(bad),'report row changed with repaired publication hash'))
    bad=P.parse(blobs['capture-receipts.json']);bad['default']['pipe_capture_complete']=False
    bad=replace(blobs,'capture-receipts.json',bad)
    results.append(rejected(lambda:P.audit(bad),'incomplete child capture'))
    bad=P.parse(blobs['capture-receipts.json']);bad['default']['stdout_bytes']+=1
    bad=replace(blobs,'capture-receipts.json',bad)
    results.append(rejected(lambda:P.audit(bad),'stdout count changed'))
    bad=P.parse(blobs['capture-receipts.json']);bad['default']['stdout_sha256']='0'*64
    bad=replace(blobs,'capture-receipts.json',bad)
    results.append(rejected(lambda:P.audit(bad),'stdout hash changed'))
    bad=P.parse(blobs['save-manifests.json']);bad['copies_after']['default']['alpha/region/r.0.0.mcr']['sha256']='0'*64
    bad=replace(blobs,'save-manifests.json',bad)
    results.append(rejected(lambda:P.audit(bad),'owned region changed'))
    bad=P.parse(blobs['save-manifests.json']);del bad['source_after']['bravo/preserve-chunk.dat']
    bad=replace(blobs,'save-manifests.json',bad)
    results.append(rejected(lambda:P.audit(bad),'missing retained native control file'))
    bad=P.parse(blobs['provenance.json']);bad['executed_inputs']['FurnaceReadback.java']['after']['sha256']='0'*64
    bad=replace(blobs,'provenance.json',bad)
    results.append(rejected(lambda:P.audit(bad),'executed Java changed'))
    bad=P.parse(blobs['provenance.json']);bad['compiled_products']['FurnaceReadback.class']['after_sha256']='0'*64
    bad=replace(blobs,'provenance.json',bad)
    results.append(rejected(lambda:P.audit(bad),'compiled helper changed'))
    bad=P.parse(blobs['provenance.json']);bad['failed_preparations'][0]['excluded_from_final_two_process_cohort']=False
    bad=replace(blobs,'provenance.json',bad)
    results.append(rejected(lambda:P.audit(bad),'failed preparation included in final cohort'))
    bad=P.parse(blobs['source-pins.json']);bad['classes']['sk']['class_sha256']='0'*64
    bad=replace(blobs,'source-pins.json',bad)
    results.append(rejected(lambda:P.audit(bad),'loaded SK class identity changed'))
    bad=P.parse(blobs['source-pins.json']);bad['classes']['ix']['methods'][0]['code_sha256']='0'*64
    bad=replace(blobs,'source-pins.json',bad)
    results.append(rejected(lambda:P.audit(bad),'IX archive Code identity changed'))
    bad=dict(blobs);del bad['interpreter.stdout']
    results.append(rejected(lambda:P.audit(bad),'missing first original stream'))
    bad=dict(blobs);bad['unexpected.class']=b'fake'
    results.append(rejected(lambda:P.audit(bad),'unexpected binary artifact'))
    receipts=P.parse(blobs['capture-receipts.json'])
    expected={name:value['command_roles'] for name,value in receipts.items()}
    report={name:{'command':value} for name,value in expected.items()}
    U.require_commands(report,expected)
    for mode,change in (('interpreter','mode'),('default','name'),('default','root'),('default','classpath')):
        bad=copy.deepcopy(report)
        if change=='mode': bad[mode]['command'].remove('-Xint')
        elif change=='name': bad[mode]['command'][-1]=''
        elif change=='root': bad[mode]['command'][-6]='OWNED_COPY/interpreter/saves'
        else: bad[mode]['command'][bad[mode]['command'].index('-classpath')+1]='SWAPPED_CLASSES'
        results.append(rejected(lambda:U.require_commands(bad,expected),'swapped runtime '+change))
    bad=P.parse(blobs['provenance.json']);bad['archive_Code_method_count']-=1
    bad=replace(blobs,'provenance.json',bad)
    results.append(rejected(lambda:P.audit(bad),'wrong archive method summary'))
    bad=P.parse(blobs['provenance.json']);bad['archive_method_count']-=1
    bad=replace(blobs,'provenance.json',bad)
    results.append(rejected(lambda:P.audit(bad),'wrong declared method summary'))
    bad=P.parse(blobs['provenance.json']);bad['modes']['default'].append('-Xint')
    bad=replace(blobs,'provenance.json',bad)
    results.append(rejected(lambda:P.audit(bad),'wrong published VM options'))
    for field,value in (('world_folder','bravo'),('name_source','postconstruction field repair')):
        bad=P.parse(blobs['declaration.json']);bad[field]=value
        bad=replace(blobs,'declaration.json',bad)
        results.append(rejected(lambda:P.audit(bad),'changed declared '+field))
    bad=P.parse(blobs['declaration.json']);bad['operations'][0],bad['operations'][1]=bad['operations'][1],bad['operations'][0]
    bad=replace(blobs,'declaration.json',bad)
    results.append(rejected(lambda:P.audit(bad),'moved declared operation'))
    bad=P.parse(blobs['declaration.json']);bad['prohibited_operations'].remove('render')
    bad=replace(blobs,'declaration.json',bad)
    results.append(rejected(lambda:P.audit(bad),'removed prohibited operation'))
    for claim in ('original tick','original save','concrete original Player entity','JDI/live-Code trace','bidirectional roundtrip'):
        bad=P.parse(blobs['declaration.json']);bad['operations'].append(claim)
        bad=replace(blobs,'declaration.json',bad)
        results.append(rejected(lambda:P.audit(bad),'unsupported declared claim: '+claim))
        bad=P.parse(blobs['provenance.json']);bad['excluded'].remove(claim)
        bad=replace(blobs,'provenance.json',bad)
        results.append(rejected(lambda:P.audit(bad),'unsupported scope expansion: '+claim))
    with tempfile.TemporaryDirectory() as temporary:
        (Path(temporary)/'nested').mkdir()
        results.append(rejected(lambda:P.files(temporary),'nested unlisted artifact directory'))
    print('PASS: '+str(len(results))+' original readback packet corruption controls')
if __name__=='__main__': main()
