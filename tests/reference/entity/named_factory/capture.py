"""Capture external original JC named factory + SN World setter observations."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import traceback
import zipfile
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
R=module('named_factory_readback_support',ROOT/'tests/reference/world/furnace_save_readback/capture.py')
C,I=R.C,R.I
CORE=('jc','sn','ls','ii','bg','wh','ud','ma','eq','fd','mx','ei','lm','rh','xa','ok','ld','qj','rj','sk','ow','uu','gm')
def read(path):return json.loads(path.read_text(encoding='utf8'),object_pairs_hook=R.unique)
def pin(path):return {'bytes':path.stat().st_size,'sha256':C.digest(path)}
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')
def command(java,cp,jar,output,mode,name,seed):
    return [str(java),*(['-Xint'] if mode=='interpreter' else []),'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true',
        '-Djava.security.manager=allow','--add-opens=java.base/java.util=ALL-UNNAMED','-classpath',cp,'NamedFactoryObservation',
        str(jar),str(output/mode/'saves'),str(output/'profiles.tsv'),name,str(seed)]
def transport(output,record):
    C.require_success(record)
    C.ensure(type(record['exit']) is int and record['exit']==0 and record['timed_out'] is False and record['pipe_capture_complete'] is True and record['output_overflow'] is False and record['reader_errors']=={},'complete exact bounded child')
    for stream in ('stdout','stderr'):
        C.ensure(record[stream+'_file']==Path(record[stream+'_file']).name,'local child stream')
        C.ensure(pin(output/record[stream+'_file'])=={'bytes':record[stream+'_bytes'],'sha256':record[stream+'_sha256']},'actual stream counts/hashes re-read')
def stderr(raw,classes):
    lines=raw.decode('utf8').splitlines()
    uri=classes.as_uri().replace('file:///','file:/')+'/'
    warnings=['WARNING: A terminally deprecated method in java.lang.System has been called',
        'WARNING: System::setSecurityManager has been called by NamedFactoryObservation ('+uri+')',
        'WARNING: Please consider reporting this to the maintainers of NamedFactoryObservation',
        'WARNING: System::setSecurityManager will be removed in a future release']
    C.ensure(lines[:4]==warnings,'exact JDK17 guard warnings remain present')
    blocks=[]
    for line in lines[4:]:
        if line=='java.lang.InstantiationException':blocks.append([line])
        else:
            C.ensure(blocks and line.startswith('\tat '),'only actual original abstract-Mob stack trace frames')
            blocks[-1].append(line)
    C.ensure(len(blocks)==2 and all(any(re.fullmatch(r'\tat jc\.a\(SourceFile(?::[0-9]+)?\)',line) for line in b) and
        any(line.startswith('\tat NamedFactoryObservation.main(') for line in b) for b in blocks),'two actual caught abstract-Mob traces at original JC + observer caller')
    return {'warning_lines':warnings,'original_stack_traces':blocks}
def stdout(raw,cases,pins):
    lines=raw.decode('utf8').splitlines()
    C.ensure(lines[:2]==['16 achievements','151 recipes'],'normal original registry output retained')
    parsed=[]
    for line in lines[2:]:
        prefix,payload=line.split(' ',1)
        parsed.append((prefix,json.loads(payload,object_pairs_hook=R.unique)))
    expected=['NAMED_FACTORY_ORIGINS']+['NAMED_FACTORY_ROW']*7+['NAMED_FACTORY_WORLD']+['NAMED_FACTORY_ROW']*3+['NAMED_FACTORY_REBIND','NAMED_FACTORY_REGISTRY','NAMED_FACTORY_COMPLETE']
    C.ensure([p for p,v in parsed]==expected,'complete ordered ten-call/world/rebind/registry/footer transport')
    origins=parsed[0][1]
    C.ensure(origins=={'jar_sha256':R.JAR_HASH,'classes':{n:v['class_sha256'] for n,v in pins.items()}},'all actual original JAR loaded-resource identities')
    rows=[v for p,v in parsed if p=='NAMED_FACTORY_ROW']
    for row,case in zip(rows,cases['cases']):
        units=None if case['name'] is None else list(map(ord,case['name']))
        C.ensure(row['index']==case['index'] and row['name_utf16']==units and row['world_argument']==case['world'] and row['distinct_string'] is case['distinct_string'],'exact authored inputs reached original named entry')
        C.ensure(row['returned'] is True and row['escaped_class'] is None and row['escaped_message'] is None,'original named method returned without escaping')
    completion=parsed[-1][1]
    C.ensure(completion['rows']==10 and completion['rebinds']==1 and completion['retained_entities']==3 and all(type(completion[k]) is int and completion[k]==0 for k in ('network','native','execs','denied_writes')),'complete actual guarded component')
    C.ensure(completion['write_checks'] and all(v in ({'operation':'write','role':'owned-session-lock'},{'operation':'write','role':'existing-world-directory'},{'operation':'write','role':'existing-copied-region-rw-open'}) for v in completion['write_checks']),'only enumerated owned copy write checks')
    return {'rows':rows,'world':next(v for p,v in parsed if p=='NAMED_FACTORY_WORLD'),'rebind':next(v for p,v in parsed if p=='NAMED_FACTORY_REBIND'),
        'registry':next(v for p,v in parsed if p=='NAMED_FACTORY_REGISTRY'),'completion':completion}
def check_observations(value,decoded):
    rows=value['rows']
    C.ensure([i for i,r in enumerate(rows) if r['result'] is not None]==[5,6,7],'actual positive constructor results only in declared Pig cases')
    for index in (5,6,7):
        r=rows[index]['result']['stable']
        C.ensure(r['class']=='wh' and r['label']=='E'+str(index-5) and r['removed'] is False and r['health']==10 and r['texture']=='/mob/pig.png' and
            r['width']==r['height']=='3f666666' and r['world']==(None if index<7 else 'W0'),'observed genuine Pig selected source fields')
        C.ensure(set(rows[index]['result']['nondeterministic'])=={'ls.aj','ls.G','ls.F','sn.aS','sn.bs.seed'},'closed predeclared constructor entropy fields')
    C.ensure(rows[6]['name_same_as_first_pig'] is False and rows[6]['name_equals_first_pig'] is True,'actual distinct equal String argument')
    C.ensure(rows[6]['entity_id_delta']==rows[7]['entity_id_delta']==1,'actual source ID increments across fresh constructors')
    world=value['world']
    C.ensure(world=={'class':'fd','remote':False,'time':decoded['metadata']['time'],'name':decoded['metadata']['name'],
        'provider_class':'rh','provider_fields':[False,False,False,0],'chunk_provider_class':'ok','chunk':[0,0],'chunk_loaded':True,
        'block':decoded['block'],'metadata':decoded['block_metadata'],'entities':[],'players':[],
        'tiles':[{'class':'sk','xyz':[0,64,2]}],'pending_tiles':0},'genuine original World matches independently decoded copied source and normal source owners')
    for r in rows:
        C.ensure(r['world_before']==r['world_after']==(None if r['index']<7 else world),'no factory publication/tick/World mutation')
    rb=value['rebind']
    C.ensure(rb['operation']=='sn.a(Lfd;)V' and rb['world_before']==rb['world_after']==world,'actual original rebind leaves World lists/fields unchanged')
    before,after=rb['before'],rb['after']
    C.ensure(before['stable']['label']==after['stable']['label']=='E0' and before['stable']['world'] is None and after['stable']['world']=='W0' and after['stable']['world_class']=='fd','actual same receiver original World store')
    changed=json.loads(json.dumps(before));changed['stable']['world']='W0';changed['stable']['world_class']='fd'
    C.ensure(changed==after,'only original World field changed within selected exact rebind projection, including raw entropy')
    reg=value['registry']
    C.ensure(reg['after_first_call']==reg['after_all_calls'] and [r['size'] for r in reg['after_first_call']]==[24]*4 and
        all(r['class']=='java.util.HashMap' for r in reg['after_first_call']),'actual four source maps retained with24 unchanged registrations')
def stable(value):
    value=json.loads(json.dumps(value))
    for row in value['rows']:
        if row['result'] is not None:del row['result']['nondeterministic']
    for stage in ('before','after'):del value['rebind'][stage]['nondeterministic']
    return value
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','jar','libraries','java-home','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--source-manifest',type=Path,default=HERE/'source-manifests.json')
    args=parser.parse_args()
    source=args.source.resolve(strict=True);prior=args.source_manifest.resolve(strict=True)
    output=args.output.resolve()
    C.ensure(source!=output and source not in output.parents,'output outside retained source')
    output.mkdir(parents=True,exist_ok=False);classes=output/'classes';classes.mkdir()
    report={'status':'FAIL','scope':__doc__,'output':str(output),'source':str(source),'source_before':R.inventory(source),'observations':{},
        'limitations':['No AG draw or CY tick entered. Direct JC named overload receives the same NULL World operand AG48 supplies.',
            'Archive Class/Code and loaded-resource identities only; no JDI/live-Code claim.',
            'Raw entropy is retained. Only the five predeclared constructor entropy fields are excluded from cross-VM equality.']}
    try:
        C.ensure(report['source_before']==read(prior)['source_before'] and len(report['source_before'])==13,'unchanged earlier owned Furnace SDK source thirteen files')
        decoded=R.disk_projection(source);write(output/'independent-source.json',decoded)
        cases=read(HERE/'cases.json');write(output/'declaration.json',cases)
        declared=output/'profiles.tsv';declared.write_text(''.join('\t'.join((str(c['index']),'~null' if c['name'] is None else c['name'],'~null' if c['world'] is None else c['world'],str(c['distinct_string']).lower()))+'\n' for c in cases['cases']),encoding='utf8',newline='\n')
        jar=args.jar.resolve(strict=True);C.ensure(C.digest(jar)==R.JAR_HASH,'actual verified unchanged original client')
        libs=[(args.libraries/name).resolve(strict=True) for name in R.LIBRARIES]
        C.ensure(all(C.digest(p)==h for p,h in zip(libs,R.LIBRARIES.values())),'pinned existing dependencies')
        jdk=args.java_home.resolve(strict=True);suffix='.exe' if os.name=='nt' else '';java,javac=[jdk/'bin'/(n+suffix) for n in ('java','javac')];jvm=jdk/'bin/server/jvm.dll' if os.name=='nt' else jdk/'lib/server/libjvm.so'
        helpers=[Path(__file__).resolve(),HERE/'cases.json',HERE/'NamedFactoryObservation.java',ROOT/'tests/reference/world/furnace_save_readback/capture.py',
            ROOT/'tests/reference/world/furnace_save_readback/transport.py',ROOT/'tests/reference/world/furnace_save_readback/classfile.py',ROOT/'tests/reference/world/furnace_save_readback/decode.py',prior,declared,output/'declaration.json',output/'independent-source.json']
        inputs=[jar,*libs,java,javac,jvm,jdk/'lib/modules',*helpers]
        report['inputs_before']={str(p):pin(p) for p in inputs}
        with zipfile.ZipFile(jar) as z:report['class_pins']={n:I.inspect_class(z.read(n+'.class')) for n in CORE}
        env=os.environ.copy();report['removed_environment_names']=[n for n in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS') if env.pop(n,None) is not None]
        cp=os.pathsep.join(map(str,[classes,jar,*libs]))
        for mode in ('interpreter','default'):
            work=output/mode;work.mkdir();shutil.copytree(source,work/'saves')
            C.ensure(R.inventory(work/'saves')==report['source_before'],'complete exact isolated copy before source open')
        report['compile']=C.capture([str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(classes),str(HERE/'NamedFactoryObservation.java')],output,'compile',60,env)
        transport(output,report['compile']);C.ensure(report['compile']['stdout_bytes']==report['compile']['stderr_bytes']==0,'clean authored-only compile')
        products=sorted(classes.rglob('*.class'));C.ensure(products and all(p.name.startswith('NamedFactoryObservation') for p in products),'only authored helper products')
        report['compiled_before']={str(p.relative_to(output)):pin(p) for p in products}
        for executable,label in ((java,'java-version'),(javac,'javac-version')):
            report[label]=C.capture([str(executable),'-version'],output,label,20,env);transport(output,report[label])
        for mode in ('interpreter','default'):
            work=output/mode;child=env.copy()
            for n in ('APPDATA','TEMP','TMP','USERPROFILE'):
                p=work/n.lower();p.mkdir();child[n]=str(p)
            cmd=command(java,cp,jar,output,mode,decoded['selected_projection']['metadata']['name'],decoded['selected_projection']['metadata']['seed'])
            report[mode]=C.capture(cmd,output,mode,90,child,work);transport(output,report[mode])
            C.ensure(report[mode]['command']==cmd,'exact declared original mode/input/classpath command')
            value=stdout((output/(mode+'.stdout')).read_bytes(),cases,report['class_pins']);report['observations'][mode]=value
            report.setdefault('stderr_observations',{})[mode]=stderr((output/(mode+'.stderr')).read_bytes(),classes)
            check_observations(value,decoded['selected_projection'])
            inv=R.inventory(work/'saves');report.setdefault('copies_after',{})[mode]=inv
            C.ensure(set(inv)==set(report['source_before']) and all(inv[n]==v for n,v in report['source_before'].items() if n!='alpha/session.lock') and inv['alpha/session.lock']['bytes']==8,'only owned copied alpha session lock may differ')
        C.ensure(stable(report['observations']['interpreter'])==stable(report['observations']['default']),'literal stable projection agrees across fresh ordinary modes with declared raw entropy retained')
        report['stable_projection']=stable(report['observations']['interpreter'])
        for mode in ('interpreter','default'):
            transport(output,report[mode]);C.ensure(stdout((output/(mode+'.stdout')).read_bytes(),cases,report['class_pins'])==report['observations'][mode],'second actual raw stdout parse equals report')
            C.ensure(stderr((output/(mode+'.stderr')).read_bytes(),classes)==report['stderr_observations'][mode],'second actual raw stderr parse equals report')
        report['status']='PASS'
    except BaseException as e:
        report['error']={'type':type(e).__name__,'message':str(e),'traceback':traceback.format_exc()}
    finally:
        report['source_after']=R.inventory(source);report['source_unchanged']=report['source_before']==report['source_after']
        if not report['source_unchanged']:report['status']='FAIL'
        report['all_copies_after']={m:R.inventory(output/m/'saves') for m in ('interpreter','default') if (output/m/'saves').is_dir()}
        if 'inputs_before' in report:
            report['inputs_after']={n:pin(Path(n)) for n in report['inputs_before']};report['inputs_unchanged']=report['inputs_before']==report['inputs_after']
            if not report['inputs_unchanged']:report['status']='FAIL'
        if 'compiled_before' in report:
            report['compiled_after']={str(p.relative_to(output)):pin(p) for p in sorted(classes.rglob('*.class'))};report['compiled_unchanged']=report['compiled_before']==report['compiled_after']
            if not report['compiled_unchanged']:report['status']='FAIL'
        write(output/'capture.json',report)
        print(json.dumps({'status':report['status'],'output':str(output),'source_unchanged':report['source_unchanged'],'error':report.get('error')},indent=2))
    return 0 if report['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
