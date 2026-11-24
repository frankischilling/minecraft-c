"""Physically re-audit the retained cohort before any sanitized public write."""
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
DATA={'finite.json','source-pins.json','source-manifests.json','receipts.json','provenance.json','native-invariants.h'}
def read(p):return P.parse(p.read_bytes())
def pin(p):return {'bytes':p.stat().st_size,'sha256':C.C.digest(p)}
def serialize(v):return (json.dumps(v,indent=2,ensure_ascii=True)+'\n').encode('utf8')
def one(report,predicate):
    found=[Path(n) for n,v in report['inputs_before'].items() if predicate(Path(n),v)]
    P.need(len(found)==1,'one unique pinned actual input');return found[0]

def verify_capture(directory,base_save):
    directory=Path(directory).resolve(strict=True);base_save=Path(base_save).resolve(strict=True);report=read(directory/'capture.json')
    P.need(report['status']=='PASS' and all(report[k] is True for k in ('source_unchanged','base_save_unchanged','inputs_unchanged','compiled_unchanged')),'successful guarded original cohort')
    P.need(report['original_modes']==list(P.MODES) and report['original_processes']==2,'exact successful ordinary two-mode cohort')
    source=Path(report['source']).resolve(strict=True)
    P.need(C.R.inventory(source)==report['source_before']==report['source_after'] and C.directories(source)==report['source_directories_before']==report['source_directories_after'],'all actual authored saved inputs unchanged')
    P.need(C.R.inventory(base_save)==report['base_save_before']==report['base_save_after'] and len(report['base_save_before'])==9,'actual retained native baseline save unchanged')
    P.need(len(report['inputs_before'])==21 and set(report['inputs_before'])==set(report['inputs_after']),'all selected executed input census')
    for path,value in report['inputs_before'].items():P.need(pin(Path(path))==value==report['inputs_after'][path],'actual archive/helper/JDK/input current/before/after bytes')
    for role,filename in [('SpawnerTickObservation.java','SpawnerTickObservation.java'),('cases.json','cases.json'),('profiles.tsv','profiles.tsv'),('source-manifest.json','input-manifest.json')]:
        original=one(report,lambda p,v:p.name==role);P.need(original.read_bytes()==(P.HERE/filename).read_bytes(),'public exact executed authored input '+filename)
    declaration,decoded=C.fixture_preflight(source)
    P.need(P.same(declaration,read(directory/'declaration.json')) and P.same(decoded,read(directory/'independent-input.json')),'fresh independent saved-NBT preflight and exact declaration')
    products=list((directory/'classes').iterdir())
    P.need({p.name for p in products}=={'SpawnerTickObservation.class','SpawnerTickObservation$1.class','SpawnerTickObservation$Sink.class'} and all(p.is_file() and not p.is_symlink() for p in products),'only three authored classpath products')
    normalized=lambda v:{n.replace('\\','/'):p for n,p in v.items()}
    P.need({p.relative_to(directory).as_posix():pin(p) for p in products}==normalized(report['compiled_before'])==normalized(report['compiled_after']),'actual complete authored product before/after/current pins')
    jar=one(report,lambda p,v:v['sha256']==C.R.JAR_HASH)
    libraries=[one(report,lambda p,v:v['sha256']==sha) for sha in C.R.LIBRARIES.values()]
    java=one(report,lambda p,v:p.name in ('java.exe','java'));javac=one(report,lambda p,v:p.name in ('javac.exe','javac'))
    observer=one(report,lambda p,v:p.name=='SpawnerTickObservation.java');profiles=one(report,lambda p,v:p.name=='profiles.tsv')
    cp=os.pathsep.join(map(str,[directory/'classes',jar,*libraries]))
    commands={'compile':[str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(directory/'classes'),str(observer)],
        'java-version':[str(java),'-version'],'javac-version':[str(javac),'-version']}
    for mode in P.MODES:commands[mode]=[str(java),*(['-Xint'] if mode=='interpreter' else []),'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow',
        '--add-opens=java.base/java.lang=ALL-UNNAMED','--add-opens=java.base/java.util=ALL-UNNAMED','-classpath',cp,'SpawnerTickObservation',str(jar),str(directory/mode/'saves'),str(profiles)]
    with zipfile.ZipFile(jar) as z:pins={n:C.I.inspect_class(z.read(n+'.class')) for n in C.CORE}
    P.need(P.same(pins,report['class_pins']),'actual unchanged original archive Class/Code inventory')
    for stem in ('compile','java-version','javac-version',*P.MODES):
        P.need(report[stem]['command']==commands[stem],'exact complete actual tool/runtime command '+stem)
        C.transport(directory,report[stem])
    P.need(report['compile']['stdout_bytes']==report['compile']['stderr_bytes']==0 and 'openjdk version "17.0.17"' in (directory/'java-version.stderr').read_text(),'actual clean compile/JDK17 process version')
    observed={}
    for mode in P.MODES:
        P.need((directory/(mode+'.stderr')).read_text(encoding='utf8').splitlines()==C.warnings(directory/'classes'),'all four actual guard warnings unsuppressed')
        value=C.output((directory/(mode+'.stdout')).read_bytes(),pins,declaration)
        P.need(P.same(value,report['observations'][mode]),'both complete original raw stdout streams reparse exactly to report')
        P.need(C.R.inventory(directory/mode/'saves')==report['copies_after'][mode]==report['all_copies_after'][mode] and C.directories(directory/mode/'saves')==report['all_copy_directories_after'][mode]==report['source_directories_before'],'all actual owned-copy files/directories retained')
        P.need(sorted(n for n,v in report['copies_after'][mode].items() if v!=report['source_before'][n])==['case%02d/session.lock'%i for i in range(10)],'only ten copied session locks differ')
        observed[mode]=value
    return report,observed

def role(path,value):
    if value['sha256']==C.R.JAR_HASH:return 'client-jar'
    for i,sha in enumerate(C.R.LIBRARIES.values()):
        if value['sha256']==sha:return 'dependency-'+str(i)
    names={'java.exe':'jdk-java','java':'jdk-java','javac.exe':'jdk-javac','javac':'jdk-javac','jvm.dll':'jdk-jvm','libjvm.so':'jdk-jvm','modules':'jdk-modules',
        'SpawnerTickObservation.java':'observer-java','prepare.py':'authored-input-writer','cases.json':'cases','profiles.tsv':'profiles','source-manifest.json':'input-manifest',
        'transport.py':'bounded-capture-support','classfile.py':'classfile-support','decode.py':'nbt-support','declaration.json':'captured-declaration','independent-input.json':'independent-input'}
    if path.name=='capture.py':return 'world-loader-support' if path.parent.name=='furnace_save_readback' else 'executed-capture-driver'
    P.need(path.name in names,'closed sanitized input role');return names[path.name]

def failed_preparations(directory):
    parent=directory.parent;out=[]
    for name,stem,kind,vm_count,reason in [
        ('capture-20261116T145803.338947Z','compile','compile-only',0,'Authored observer compilation failed before any original component VM.'),
        ('capture-20261116T145826.059514Z','interpreter','runtime-preparation',1,'Missing authored data directories caused the write guard to stop original storage preparation before Player or CY entry.')]:
        failed=parent/name;r=read(failed/'capture.json');receipt=read(failed/(stem+'.capture.json'))
        P.need(r['status']=='FAIL' and receipt==r[stem] and type(receipt['exit']) is int and receipt['exit']==1 and receipt['pipe_capture_complete'] is True and receipt['reader_errors']=={} and receipt['timed_out'] is False and receipt['output_overflow'] is False,'retained excluded preparation failure status')
        for stream in ('stdout','stderr'):P.need(pin(failed/receipt[stream+'_file'])=={'bytes':receipt[stream+'_bytes'],'sha256':receipt[stream+'_sha256']},'retained failed-preparation complete raw stream')
        for p in (failed/'failed-preparation-sources').iterdir():
            expected=[v for n,v in r['inputs_before'].items() if Path(n).name==p.name and ('spawner-tick-reference-tools' in str(Path(n).parent))]
            P.need(len(expected)==1 and pin(p)==expected[0],'exact failed-preparation authored helper bytes preserved separately')
        out.append({'kind':kind,'original_vm_count':vm_count,'completed_tick_rows':0,'capture_json':pin(failed/'capture.json'),'process_receipt':pin(failed/(stem+'.capture.json')),
            'stdout':pin(failed/receipt['stdout_file']),'stderr':pin(failed/receipt['stderr_file']),'reason':reason})
    return out

def project_verified(directory,base_save):
    directory=Path(directory).resolve(strict=True);report,observed=verify_capture(directory,base_save)
    finite={'schema':'ORIGINAL_CLIENT_EXPIRED_SPAWNER_V1','observations':observed};pins=report['class_pins'];receipts={}
    for stem in ('compile','java-version','javac-version',*P.MODES):
        r=report[stem];projected={k:r[k] for k in ('exit','timeout_seconds','timed_out','pipe_capture_complete','reader_errors','output_overflow','stdout_bytes','stderr_bytes','stdout_sha256','stderr_sha256','stdout_limit','stderr_limit')}
        projected.update(raw_receipt=pin(directory/(stem+'.capture.json')),command_representation='Role projection, not verbatim private command.',
            command_roles=P.command_roles(stem) if stem in P.MODES else ['JDK/bin/javac','-encoding','UTF-8','-implicit:none','-classpath','CLASSES;CLIENT_JAR;DEPENDENCIES','-d','CLASSES','OBSERVER_JAVA'] if stem=='compile' else ['JDK/bin/'+('java' if stem=='java-version' else 'javac'),'-version'])
        receipts[stem]=projected
    inputs={}
    for path,value in report['inputs_before'].items():
        name=role(Path(path),value);P.need(name not in inputs,'unique sanitized input role');inputs[name]={'before':value,'after':report['inputs_after'][path]}
    products={Path(n).name:{'before':p,'after':report['compiled_after'][n]} for n,p in report['compiled_before'].items()}
    provenance={'scope':P.SCOPE,'excluded':P.EXCLUDED,'original_processes':2,'ticks_per_mode':10,'modes':list(P.MODES),'native_parity_claimed':False,'internal_trace_claimed':False,'public_java_is_executed_source':True,
        'packaging_scope':'Portable runner, publisher, offline validator, controls, C projection and prose were authored after execution. Exact Java, cases, profiles and input manifest match executed inputs.',
        'executed_driver_sha256':inputs['executed-capture-driver']['before']['sha256'],'selected_input_count':len(inputs),'executed_inputs':inputs,'compiled_products':products,
        'archive_class_count':len(pins),'declared_method_count':sum(len(v['methods']) for v in pins.values()),'code_method_count':sum('code_sha256' in m for v in pins.values() for m in v['methods']),
        'finite_sha256':P.digest(P.encoded(finite)),'class_pins_sha256':P.digest(P.encoded(pins)),'receipts_sha256':P.digest(P.encoded(receipts)),
        'retained_capture_json':pin(directory/'capture.json'),'stdout_line_ending':'\r\n' if b'\r\n' in (directory/'interpreter.stdout').read_bytes() else '\n',
        'jvm_option_keys_removed_if_present':['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'],'keys_present':report['removed_environment_names'],
        'java_version_lines':(directory/'java-version.stderr').read_text(encoding='utf8').splitlines(),'failed_preparations':failed_preparations(directory),
        'raw_material_scope':'Raw original streams, private commands/receipts, JAR/classes/dependencies/assets and World bytes remain external. Complete finite outputs and raw receipt/stream hashes were physically joined before publication.'}
    saves={k:report[k] for k in ('source_before','source_after','source_directories_before','source_directories_after','base_save_before','base_save_after','copies_after')}
    saves['copy_directories_after']=report['all_copy_directories_after']
    data={'finite.json':finite,'source-pins.json':{'jar_sha256':C.R.JAR_HASH,'classes':pins,'meaning':'Archive Class/Code identities and runtime checked original CodeSource/resource hashes; not live-Code or entered-PC evidence.'},
        'source-manifests.json':saves,'receipts.json':receipts,'provenance.json':provenance}
    blobs={n:P.native_header(observed) if n=='native-invariants.h' else serialize(data[n]) for n in DATA}
    blobs.update({n:(P.HERE/n).read_bytes() for n in P.FILES-DATA})
    for n,raw in blobs.items():P.need(not re.search(rb'[A-Za-z]:[\\/](?:Users|Program Files)|/mnt/[a-z]/|\.local[\\/]',raw),'no private path in '+n)
    blobs['publication.json']=serialize({'meaning':'Postexecution publication-only fingerprints; not original execution inputs.','files':{n:{'bytes':len(raw),'sha256':P.digest(raw)} for n,raw in sorted(blobs.items())}})
    P.audit(blobs)
    again=verify_capture(directory,base_save)
    P.need(P.same(report,again[0]) and P.same(observed,again[1]),'final actual complete capture/both-stream re-audit before any public write')
    failed_preparations(directory)
    return blobs

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','base-save','output'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();out=args.output.resolve();P.need(not out.exists(),'new publication directory only')
    blobs=project_verified(args.capture,args.base_save);out.mkdir(parents=True,exist_ok=False)
    for n,raw in blobs.items():(out/n).write_bytes(raw)
    P.audit(P.files(out));print('PASS: original expired-Spawner packet published after physical/both-stream audit')
if __name__=='__main__':main()
