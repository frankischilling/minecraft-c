"""Re-audit external original evidence before writing a sanitized full packet."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import zipfile
sys.dont_write_bytecode=True
import packet as P
C=P.C
DATA={'finite.json','input-world.json','source-pins.json','source-manifests.json','receipts.json','diagnostics.json','provenance.json','native-observations.h'}
def read(path):return P.parse(path.read_bytes())
def pin(path):return {'bytes':path.stat().st_size,'sha256':C.C.digest(path)}
def serialize(v):return (json.dumps(v,indent=2,ensure_ascii=True)+'\n').encode('utf8')
def one_input(report,predicate):
    paths=[Path(name) for name,value in report['inputs_before'].items() if predicate(Path(name),value)]
    P.need(len(paths)==1,'one exact retained pinned input')
    return paths[0]
def expected_commands(report,directory,decoded):
    java=Path(report['java-version']['command'][0]);javac=Path(report['javac-version']['command'][0])
    P.need(str(java) in report['inputs_before'] and str(javac) in report['inputs_before'] and java.parent==javac.parent,'same actual pinned JDK executable directory')
    jar=one_input(report,lambda p,v:v['sha256']==C.R.JAR_HASH)
    libraries=[one_input(report,lambda p,v:v['sha256']==sha) for sha in C.R.LIBRARIES.values()]
    observer=one_input(report,lambda p,v:p.name=='NamedFactoryObservation.java')
    cp=os.pathsep.join(map(str,[directory/'classes',jar,*libraries]))
    expected={'compile':[str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(directory/'classes'),str(observer)],
        'java-version':[str(java),'-version'],'javac-version':[str(javac),'-version']}
    for mode in P.MODES:expected[mode]=C.command(java,cp,jar,directory,mode,decoded['metadata']['name'],decoded['metadata']['seed'])
    return expected,jar
def diagnostic_projection(value):
    blocks=value['original_stack_traces']
    P.need(len(value['warning_lines'])==4 and len(blocks)==2,'all original warnings and caught traces retained')
    for block in blocks:
        P.need(block[0]=='java.lang.InstantiationException' and len(block)==6,'complete actual abstract-constructor trace')
        P.need(block[-2:] == ['\tat jc.a(SourceFile:62)','\tat NamedFactoryObservation.main(NamedFactoryObservation.java:107)'],'actual original and exact executed helper source frames')
        P.need(all(re.fullmatch(r'\tat java\.base/[A-Za-z0-9_.$]+\([A-Za-z0-9_.$]+\.java:[0-9]+\)',line) for line in block[1:4]),'actual JDK reflection frame syntax')
    return {'guard_warning_count':4,'caught_exception_count':2,'exception_class':'java.lang.InstantiationException',
        'original_source_frame':{'class':'jc','method':'a','file':'SourceFile','line':62},
        'observer_source_frame':{'class':'NamedFactoryObservation','method':'main','file':'NamedFactoryObservation.java','line':107}}
def verify_capture(directory):
    directory=Path(directory).resolve(strict=True);report=read(directory/'capture.json')
    P.need(report['status']=='PASS' and all(report[k] is True for k in ('source_unchanged','inputs_unchanged','compiled_unchanged')),'successful guarded original capture')
    source=Path(report['source']).resolve(strict=True)
    P.need(C.R.inventory(source)==report['source_before']==report['source_after'] and len(report['source_before'])==13,'all retained source files remain unchanged')
    P.need(set(report['inputs_before'])==set(report['inputs_after']) and len(report['inputs_before'])==20,'complete exact selected input census')
    for name,value in report['inputs_before'].items():P.need(pin(Path(name))==value==report['inputs_after'][name],'actual selected archive/helper/JDK/input bytes before/after')
    products=list((directory/'classes').iterdir())
    P.need(len(products)==2 and {p.name for p in products}=={'NamedFactoryObservation.class','NamedFactoryObservation$1.class'} and all(p.is_file() and not p.is_symlink() for p in products),'only two authored classpath products')
    current={str(p.relative_to(directory)).replace('\\','/'):pin(p) for p in products}
    normalize=lambda values:{n.replace('\\','/'):v for n,v in values.items()}
    P.need(current==normalize(report['compiled_before'])==normalize(report['compiled_after']),'actual compiled products remain exact')
    declaration=read(directory/'declaration.json');P.declaration(declaration)
    P.need(P.same(declaration,read(P.HERE/'cases.json')),'original declaration matches public exact input')
    observer=one_input(report,lambda p,v:p.name=='NamedFactoryObservation.java')
    P.need(pin(observer)==pin(P.HERE/'NamedFactoryObservation.java'),'public observer source is byte-identical to executed Java')
    decoded=C.R.disk_projection(source)
    P.need(P.same(decoded,read(directory/'independent-source.json')),'fresh independent typed NBT source decode')
    decoded=decoded['selected_projection'];P.expected_world(decoded)
    expected,jar=expected_commands(report,directory,decoded)
    with zipfile.ZipFile(jar) as archive:pins={name:C.I.inspect_class(archive.read(name+'.class')) for name in C.CORE}
    P.need(P.same(pins,report['class_pins']),'actual archive Class/Code inventory re-read')
    for stem in ('compile','java-version','javac-version',*P.MODES):
        receipt=report[stem]
        P.need(receipt['command']==expected[stem],'exact declared tool/runtime command '+stem)
        C.transport(directory,receipt)
        P.need(read(directory/(stem+'.capture.json'))==receipt,'stored bounded process receipt equals report')
    P.need(report['compile']['stdout_bytes']==report['compile']['stderr_bytes']==0,'clean authored observer compilation')
    P.need(re.search(r'openjdk version "17\.',(directory/'java-version.stderr').read_text(encoding='utf8')) is not None,'actual JDK17 process version')
    values={};diagnostics={}
    for mode in P.MODES:
        value=C.stdout((directory/(mode+'.stdout')).read_bytes(),declaration,pins)
        err=C.stderr((directory/(mode+'.stderr')).read_bytes(),directory/'classes')
        P.need(P.same(value,report['observations'][mode]) and P.same(err,report['stderr_observations'][mode]),'both complete original streams reparse exactly to recorded observations')
        P.observations(value,declaration,decoded);diagnostics[mode]=diagnostic_projection(err);values[mode]=value
        inv=C.R.inventory(directory/mode/'saves')
        P.need(inv==report['copies_after'][mode]==report['all_copies_after'][mode] and set(inv)==set(report['source_before']),'all actual copied save files retained')
        P.need(sorted(n for n,v in inv.items() if v!=report['source_before'][n])==['alpha/session.lock'] and inv['alpha/session.lock']['bytes']==8,'only owned copied SessionLock differs')
    P.need(P.same(C.stable(values['interpreter']),C.stable(values['default'])) and P.same(report['stable_projection'],C.stable(values['interpreter'])),'declared stable projection agrees with raw entropy retained')
    return report,values,diagnostics,decoded
def input_role(path,value):
    if value['sha256']==C.R.JAR_HASH:return 'client-jar'
    for index,sha in enumerate(C.R.LIBRARIES.values()):
        if value['sha256']==sha:return 'dependency-'+str(index)
    name=path.name
    roles={'java.exe':'jdk-java','java':'jdk-java','javac.exe':'jdk-javac','javac':'jdk-javac','jvm.dll':'jdk-jvm','libjvm.so':'jdk-jvm','modules':'jdk-modules',
        'NamedFactoryObservation.java':'observer-java','cases.json':'cases','transport.py':'transport-support','classfile.py':'classfile-support',
        'decode.py':'nbt-support','profiles.tsv':'declared-profiles','declaration.json':'captured-declaration','independent-source.json':'decoded-source'}
    if name=='capture.py':return 'world-loader-support' if path.parent.name=='furnace_save_readback' else 'executed-capture-driver'
    if name in ('capture.json','source-manifests.json'):return 'source-manifest-context'
    P.need(name in roles,'closed sanitized input role '+name)
    return roles[name]
def header(stable):
    lines=['/* Original finite Pig fields only. No native parity, entropy or whole-frame claim. */','#ifndef MC_NAMED_FACTORY_ORIGINAL_OBSERVATIONS_H','#define MC_NAMED_FACTORY_ORIGINAL_OBSERVATIONS_H',
        '#include <stdbool.h>','#include <stdint.h>','','typedef struct mc_named_factory_pig_reference {','    uint32_t case_index;','    int32_t expected_id;','    bool world_nonnull;',
        '    uint32_t width_bits, height_bits;','    int32_t health;','    int8_t watcher0, watcher16;','    const char *texture_ascii;','} mc_named_factory_pig_reference;','',
        'static const mc_named_factory_pig_reference mc_named_factory_pig_references[3] = {']
    for row in stable['rows']:
        if row['result'] is None:continue
        s=row['result']['stable']
        lines.append('    {%d, %d, %s, UINT32_C(0x%s), UINT32_C(0x%s), %d, %d, %d, "%s"},'%(row['index'],s['id'],'true' if s['world'] is not None else 'false',s['width'],s['height'],s['health'],s['watcher']['0']['value'],s['watcher']['16']['value'],s['texture']))
    lines+=['};','','/* Same original E0 receiver: NULL World before, actual FD World after. */','typedef struct mc_named_factory_world_setter_reference {','    int32_t expected_id;',
        '    bool world_before_nonnull, world_after_nonnull;','} mc_named_factory_world_setter_reference;','',
        'static const mc_named_factory_world_setter_reference mc_named_factory_world_setter_reference_value =','    {0, false, true};','',
        '/* Original cases4/8 return NULL after caught InstantiationException and real',' * printStackTrace. A native print52 frontier is not replay of those outcomes. */','#endif','']
    return '\n'.join(lines).encode('utf8')
def project_verified(directory):
    directory=Path(directory).resolve(strict=True)
    report,observed,diagnostics,source=verify_capture(directory)
    finite={'schema':'ORIGINAL_CLIENT_NAMED_FACTORY_V1','observations':observed,'stable_projection':C.stable(observed['interpreter'])}
    receipts={}
    for stem in ('compile','java-version','javac-version',*P.MODES):
        receipt=report[stem]
        roles=P.command_roles(stem,source['metadata']['name'],source['metadata']['seed']) if stem in P.MODES else (
            ['JDK/bin/javac','-encoding','UTF-8','-implicit:none','-classpath','CLASSES;CLIENT_JAR;LIBRARIES','-d','CLASSES','OBSERVER_JAVA'] if stem=='compile' else ['JDK/bin/'+('java' if stem=='java-version' else 'javac'),'-version'])
        receipts[stem]={key:receipt[key] for key in ('exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stderr_bytes','stdout_sha256','stderr_sha256','stdout_limit','stderr_limit')}
        receipts[stem].update(command_representation='role projection, not verbatim private command',command_roles=roles,raw_receipt=pin(directory/(stem+'.capture.json')))
    inputs={}
    for name,value in report['inputs_before'].items():
        role=input_role(Path(name),value);P.need(role not in inputs,'unique sanitized selected input role')
        inputs[role]={'before':value,'after':report['inputs_after'][name]}
    products={Path(n).name:{'before':value,'after':report['compiled_after'][n]} for n,value in report['compiled_before'].items()}
    pins=report['class_pins']
    provenance={'scope':'Direct original client JC named factory and original SN World setter; same NULL World operand used at AG48, without entering AG.',
        'excluded':P.EXCLUDED,'original_processes':2,'original_modes':list(P.MODES),'calls_per_mode':10,'rebinds_per_mode':1,'native_parity_claimed':False,
        'public_java_is_executed_source':True,'packaging_scope':'Portable runner, publication validator, controls, header and prose were authored after execution; they were not used to run the retained two VMs.',
        'observation_projection_sha256':P.digest(P.encoded(finite)),'archive_pins_sha256':P.digest(P.encoded(pins)),'transport_projection_sha256':P.digest(P.encoded(receipts)),
        'original_class_count':len(pins),'declared_method_count':sum(len(v['methods']) for v in pins.values()),'code_method_count':sum('code_sha256' in m for v in pins.values() for m in v['methods']),
        'retained_capture_json':pin(directory/'capture.json'),'executed_inputs':inputs,'compiled_products':products,
        'stdout_line_ending':'\r\n' if b'\r\n' in (directory/'interpreter.stdout').read_bytes() else '\n',
        'java_version_lines':(directory/'java-version.stderr').read_text(encoding='utf8').splitlines(),
        'inherited_jvm_option_keys_removed_if_present':['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'],'keys_present':report['removed_environment_names'],
        'stream_scope':'Raw stdout/stderr, process receipts with private paths, classes, archives and save bytes remain external/private. Published receipt/stream hashes and finite structured projections were checked against all retained bytes.'}
    data={'finite.json':finite,'input-world.json':source,'source-pins.json':{'jar_sha256':C.R.JAR_HASH,'classes':pins,'meaning':'Archive Class/Code and actual loaded-resource identities; no entered/live-Code or call-order trace.'},
        'source-manifests.json':{k:report[k] for k in ('source_before','source_after','copies_after')},'receipts.json':receipts,'diagnostics.json':diagnostics,'provenance.json':provenance}
    blobs={name:(header(finite['stable_projection']) if name=='native-observations.h' else serialize(data[name])) for name in DATA}
    blobs.update({name:(P.HERE/name).read_bytes() for name in P.FILES-DATA})
    for name,raw in blobs.items():P.need(not re.search(rb'[A-Za-z]:[\\/](?:Users|Program Files)|/mnt/[a-z]/|\.local[\\/]',raw),'no private path in '+name)
    blobs['publication.json']=serialize({'meaning':'Postexecution publication-only helper/data fingerprints.','files':{n:{'bytes':len(v),'sha256':P.digest(v)} for n,v in sorted(blobs.items())}})
    P.audit(blobs)
    again=verify_capture(directory)
    P.need(P.same(report,again[0]) and P.same(observed,again[1]) and P.same(diagnostics,again[2]) and P.same(source,again[3]),'final physical/both-stream re-audit before any output write')
    return blobs
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    output=args.output.resolve();P.need(not output.exists(),'new publication output only')
    blobs=project_verified(args.capture)
    output.mkdir(parents=True,exist_ok=False)
    for name,raw in blobs.items():(output/name).write_bytes(raw)
    P.audit(P.files(output));print('PASS: sanitized complete original named-factory packet written after physical/both-stream audit')
if __name__=='__main__':main()
