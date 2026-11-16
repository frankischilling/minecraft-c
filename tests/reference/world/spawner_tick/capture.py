"""Portable capture of unchanged CY.n_ over externally supplied hash-pinned saved inputs.
Authored after the retained cohort; the exact Java observer is unchanged.
"""
import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import traceback
import zipfile
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
spec=importlib.util.spec_from_file_location('spawner_tick_read_support',ROOT/'tests/reference/world/furnace_save_readback/capture.py')
R=importlib.util.module_from_spec(spec);spec.loader.exec_module(R);C,I=R.C,R.I
CORE=('cy','ow','fd','mx','fm','ei','lm','rh','xa','ok','ld','qj','rj','rw','nu','sp','ij','as','uu','gm','jc','sn','ls','ii','bg','wh','sl','gs','dc','gr','pm','eq','ud','ma')
MODES=('interpreter','default')
def read(p):return json.loads(p.read_text(encoding='utf8'),object_pairs_hook=R.unique)
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf8')
def pin(p):return {'bytes':Path(p).stat().st_size,'sha256':C.digest(p)}
def directories(path):return sorted(p.relative_to(path).as_posix() for p in path.rglob('*') if p.is_dir())
def same(a,b):return json.dumps(a,sort_keys=True,separators=(',',':'))==json.dumps(b,sort_keys=True,separators=(',',':'))
def rng(v):
    C.ensure(set(v)=={'seed','cached','gaussian_bits'} and type(v['seed']) is int and 0<=v['seed']<1<<48 and type(v['cached']) is bool and re.fullmatch('[0-9a-f]{16}',v['gaussian_bits']),'typed retained raw Random state')
def vectors(v):
    C.ensure(type(v) is list and len(v)==3 and all(type(x) is str and re.fullmatch('[0-9a-f]{16}',x) for x in v),'raw double-bit vector')
    return [struct.unpack('>d',bytes.fromhex(x))[0] for x in v]
def entity(v):
    C.ensure(set(v)=={'label','class','id','world_same','removed','position_bits','yaw_bits','pitch_bits','size_bits','bounds_bits','random','living_ctor_bits'},'closed actual Entity fields')
    C.ensure(type(v['id']) is int and v['world_same'] is True and v['removed'] is False and re.fullmatch('E[0-9]+',v['label']),'actual Entity identity/World state')
    vectors(v['position_bits']);rng(v['random'])
    C.ensure(all(re.fullmatch('[0-9a-f]{8}',x) for x in [v['yaw_bits'],v['pitch_bits'],*v['size_bits'],*v['living_ctor_bits']]) and len(v['size_bits'])==3 and len(v['bounds_bits'])==6 and all(re.fullmatch('[0-9a-f]{16}',x) for x in v['bounds_bits']),'all retained float/bounds/constructor bits')

def fixture_preflight(fixtures):
    declaration=read(HERE/'cases.json');manifest=read(HERE/'input-manifest.json')
    C.ensure(R.inventory(fixtures)==manifest['fixtures'] and len(manifest['fixtures'])==60 and directories(fixtures)==manifest['fixture_directories'],'exact authored sixty-file input census')
    selected=[]
    for case in declaration['cases']:
        folder=fixtures/case['folder'];info=R.value(R.N.inflate((folder/'level.dat').read_bytes(),31),'Data',10)
        C.ensure(R.value(info,'LevelName',8)=='Original Spawner tick %02d'%case['index'] and R.value(info,'RandomSeed',4)==123 and R.value(info,'Time',4)==8,'actual serialized constructor name/seed/time')
        for cx,cz in declaration['chunks']:
            raw=(folder/'region'/('r.%d.%d.mcr'%(cx>>5,cz>>5))).read_bytes();slot=(cx&31)+32*(cz&31);entry=struct.unpack_from('>I',raw,slot*4)[0]
            at=(entry>>8)*4096;count=entry&255;length=struct.unpack_from('>I',raw,at)[0]
            C.ensure(entry>>8>=2 and count>0 and (entry>>8)+count==len(raw)//4096 and 1<length<=count*4096-4 and raw[at+4]==2,'bounded authored region record')
            chunk=R.value(R.N.inflate(raw[at+5:at+4+length],15),'Level',10)
            C.ensure([R.value(chunk,k,3) for k in ('xPos','zPos')]==[cx,cz] and R.value(chunk,'TerrainPopulated',1)==1,'actual authored saved Chunk coordinates')
            C.ensure(R.value(chunk,'Entities',9)==[10,[]],'no authored Entity membership substitute')
            expected=[] if (cx,cz)!=(0,0) else [{'id':[8,'MobSpawner'],'x':[3,0],'y':[3,64],'z':[3,2],'EntityId':[8,case['name']],'Delay':[2,0]}]
            C.ensure(R.value(chunk,'TileEntities',9)==[10,expected],'exact authored CY factory input')
            blocks=R.value(chunk,'Blocks',7);C.ensure(len(blocks)==32768,'whole Blocks')
            for x in range(16):
                for z in range(16):
                    C.ensure(blocks[(x<<11)|(z<<7)|63]==(1 if case['terrain']=='stone' else 2),'actual authored terrain plane')
            C.ensure(R.value(chunk,'SkyLight',7)==b'\xff'*16384 and R.value(chunk,'BlockLight',7)==bytes(16384),'actual authored light arrays, no callee replacement')
        selected.append({'index':case['index'],'metadata':{k:info[k] for k in ('LevelName','RandomSeed','Time')},'profile':case})
    return declaration,selected

def check_rows(rows,declaration):
    C.ensure(len(rows)==10,'all ten actual expired-tick rows')
    common=[];successes=0
    for case,row in zip(declaration['cases'],rows):
        i=case['index'];before=row['before'];after=row['after'];events=row['events'];prep=case['prep_pigs']
        C.ensure(set(row)=={'index','profile','before','returned','failure','events','after'} and type(row['index']) is int and row['index']==i,'closed ordered original row')
        C.ensure(row['profile']==[str(i),case['folder'],case['name'],str(prep),'Original Spawner tick %02d'%i,'123'],'exact case runtime arguments')
        C.ensure(before['delay']==0 and before['rotation_bits']==before['previous_rotation_bits']=='0000000000000000' and before['name']==after['name']==case['name'],'exact original saved Tile input')
        C.ensure(before['world_time']==after['world_time']==8 and before['world_L'] is after['world_L'] is False and before['players']==after['players'] and before['active_tiles']==after['active_tiles'] and before['pending_tiles']==after['pending_tiles']==[],'selected direct-tick World state')
        C.ensure(after['rotation_bits']=='4014000000000000' and after['previous_rotation_bits']=='0000000000000000','actual active rotation prefix')
        C.ensure(len(before['entities'])==1+prep and len(before['players'])==1 and before['players'][0]['class']=='dc','actual prepared Player/Pig membership')
        for state in (before,after):
            rng(state['world_random']);rng(state['math_random'])
            for e in state['retained_entities']:entity(e)
        delta=after['counter']-before['counter'];C.ensure(delta==[0,1,1,4][i] if i<4 else delta==4,'actual global ID-counter delta')
        if i==1:
            C.ensure(row['returned'] is False and row['failure']['class']=='java.lang.ClassCastException' and row['failure']['frames'][0]['class']=='cy' and row['failure']['frames'][0]['method']=='n_','real nonLiving original caller cast failure')
        else:C.ensure(row['returned'] is True and row['failure'] is None,'actual original return without fabricated failure')
        C.ensure(len(events)>=2 and [e['event'] for e in events[:2]]==['particle','particle'] and [e['name'] for e in events[:2]]==['smoke','flame'],'actual initial particle prefix')
        C.ensure(events[0]['position_bits']==events[1]['position_bits'] and events[0]['world_random']==events[1]['world_random'] and all(e['delay']==0 and e['velocity_bits']==['0000000000000000']*3 for e in events[:2]),'initial pair actual arguments/state')
        at=2;published=[]
        while at<len(events):
            group=events[at:at+61];C.ensure(len(group)==61 and group[0]['event']=='entity_added' and all(e['event']=='particle' for e in group[1:]),'complete actual publication/effect group')
            actor=group[0]['entity'];entity(actor);C.ensure(actor['class']=='wh' and before['counter']<=actor['id']<after['counter'],'actual newly constructed Pig identity')
            xyz=vectors(actor['position_bits']);C.ensure(all(math.isfinite(x) for x in xyz) and -4<xyz[0]<4 and xyz[1] in (63.0,64.0,65.0) and -2<xyz[2]<6,'actual posed candidate range')
            C.ensure(actor['size_bits']==['3f666666','3f666666','00000000'],'actual Pig size/offset')
            C.ensure(group[0]['world_entity_count']==len(before['entities'])+len(published)+1,'publication callback after actual World append')
            C.ensure([e['name'] for e in group[1:41]]==['smoke','flame']*20 and [e['name'] for e in group[41:]]==['explode']*20,'actual ordered forty CY particles then twenty LS.V particles')
            C.ensure(all(e['delay']==group[0]['delay'] for e in group) and (group[0]['delay']==0 if not published else 200<=group[0]['delay']<=799),'sampled delay remains unchanged through each source effect group')
            for a,b in zip(group[1:41:2],group[2:41:2]):C.ensure(a['position_bits']==b['position_bits'] and a['world_random']==b['world_random'] and a['velocity_bits']==b['velocity_bits']==['0000000000000000']*3,'actual smoke/flame pair values')
            C.ensure(all(e['world_random']==group[41]['world_random'] for e in group[41:]),'actual LS.V callbacks do not advance World Random')
            for event in group[1:]:vectors(event['position_bits']);vectors(event['velocity_bits']);rng(event['world_random'])
            final=next(e for e in after['retained_entities'] if e['label']==actor['label']);a=dict(actor);b=dict(final);a.pop('random');b.pop('random')
            C.ensure(a==b and actor['random']!=final['random'],'same published Pig selected state retained; genuine own Random advances')
            published.append(actor);at+=61
        C.ensure(len(published)<=4 and after['entities'][:len(before['entities'])]==before['entities'] and len(after['entities'])==len(before['entities'])+len(published),'actual same World membership prefix and new publications')
        if i<4:C.ensure(not published and len(events)==2 and (200<=after['delay']<=799 if i==2 else after['delay']==0),'finite unknown/cast/cap/reject result')
        else:C.ensure(200<=after['delay']<=799 if published else after['delay']==0,'actual delay agrees with reached success effects')
        successes+=len(published)
        common.append({'index':i,'counter_delta':delta,'returned':row['returned'],'failure_class':None if row['failure'] is None else row['failure']['class'],'rotation_bits':after['rotation_bits'],'world_time':after['world_time'],'players':after['players'],'name':after['name']})
    return {'common_projection':common,'observed_successful_publications':successes}

def output(raw,pins,declaration):
    lines=raw.decode('utf8').splitlines();C.ensure(len(lines)==14 and lines[:2]==['16 achievements','151 recipes'],'all complete original records and normal prefixes')
    origins=json.loads(lines[2].removeprefix('SPAWNER_TICK_ORIGINS '),object_pairs_hook=R.unique)
    C.ensure(lines[2].startswith('SPAWNER_TICK_ORIGINS ') and origins=={'jar_sha256':R.JAR_HASH,'classes':{n:pins[n]['class_sha256'] for n in CORE}},'actual original Class origin/resource proof')
    rows=[]
    for line in lines[3:13]:
        C.ensure(line.startswith('SPAWNER_TICK_ROW '),'closed original row protocol');rows.append(json.loads(line[len('SPAWNER_TICK_ROW '):],object_pairs_hook=R.unique))
    C.ensure(lines[-1].startswith('SPAWNER_TICK_COMPLETE '),'unique terminal original footer')
    footer=json.loads(lines[-1][len('SPAWNER_TICK_COMPLETE '):],object_pairs_hook=R.unique)
    C.ensure(set(footer)=={'rows','network','native','execs','denied_writes','write_checks'} and type(footer['rows']) is int and footer['rows']==10 and all(type(footer[k]) is int and footer[k]==0 for k in ('network','native','execs','denied_writes')),'all successful external-attempt guards')
    C.ensure(sum(e=={'operation':'write','role':'owned-session-lock'} for e in footer['write_checks'])==10 and all(e in ({'operation':'write','role':'owned-session-lock'},{'operation':'write','role':'existing-world-directory'},{'operation':'write','role':'existing-owned-region-rw-open'}) for e in footer['write_checks']),'only declared owned-copy write/rw-open checks')
    return {'rows':rows,'footer':footer,'summary':check_rows(rows,declaration)}

def transport(directory,r):
    R.transport_record(directory,r);C.ensure(read(directory/(Path(r['stdout_file']).stem+'.capture.json'))==r,'raw child receipt retained exactly')
def warnings(classes):
    uri=classes.as_uri().replace('file:///','file:/')+'/'
    return ['WARNING: A terminally deprecated method in java.lang.System has been called','WARNING: System::setSecurityManager has been called by SpawnerTickObservation ('+uri+')',
        'WARNING: Please consider reporting this to the maintainers of SpawnerTickObservation','WARNING: System::setSecurityManager will be removed in a future release']
def command(java,cp,jar,directory,mode):
    return [str(java),*(['-Xint'] if mode=='interpreter' else []),'-Xverify:all','-Xmx256m','-XX:-UsePerfData','-Djava.awt.headless=true','-Djava.security.manager=allow',
        '--add-opens=java.base/java.lang=ALL-UNNAMED','--add-opens=java.base/java.util=ALL-UNNAMED','-classpath',cp,'SpawnerTickObservation',str(jar),str(directory/mode/'saves'),str(HERE/'profiles.tsv')]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('fixtures','base-save','output','jar','libraries','java-home'):parser.add_argument('--'+n,required=True,type=Path)
    args=parser.parse_args();source=args.fixtures.resolve(strict=True);directory=args.output.resolve()
    C.ensure(not directory.exists() and source!=directory and source not in directory.parents,'new output outside retained fixtures')
    directory.mkdir(parents=True,exist_ok=False);classes=directory/'classes';classes.mkdir()
    jar=args.jar.resolve(strict=True);libs=[(args.libraries/n).resolve(strict=True) for n in R.LIBRARIES]
    jdk=args.java_home.resolve(strict=True);suffix='.exe' if os.name=='nt' else '';java=jdk/'bin'/('java'+suffix);javac=jdk/'bin'/('javac'+suffix)
    base=args.base_save.resolve(strict=True)
    report={'status':'FAIL','scope':__doc__,'source':str(source),'output':str(directory),'source_before':R.inventory(source),'source_directories_before':directories(source),'base_save_before':R.inventory(base),'original_modes':list(MODES),'original_processes':2}
    try:
        declaration,decoded=fixture_preflight(source);write(directory/'declaration.json',declaration);write(directory/'independent-input.json',decoded)
        C.ensure(C.digest(jar)==R.JAR_HASH and all(C.digest(p)==sha for p,sha in zip(libs,R.LIBRARIES.values())),'actual original archive/dependencies')
        helpers=[HERE/n for n in ('SpawnerTickObservation.java','capture.py','cases.json','profiles.tsv','input-manifest.json')]
        helpers += [ROOT/'tests/reference/world/furnace_save_readback'/n for n in ('capture.py','transport.py','classfile.py','decode.py')]
        inputs=[jar,*libs,java,javac,(jdk/'bin/server/jvm.dll' if os.name=='nt' else jdk/'lib/server/libjvm.so'),jdk/'lib/modules',*helpers,directory/'declaration.json',directory/'independent-input.json']
        report['inputs_before']={str(p):pin(p) for p in inputs}
        with zipfile.ZipFile(jar) as z:report['class_pins']={n:I.inspect_class(z.read(n+'.class')) for n in CORE}
        env=os.environ.copy();report['removed_environment_names']=[n for n in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS') if env.pop(n,None) is not None]
        cp=os.pathsep.join(map(str,[classes,jar,*libs]))
        for mode in MODES:
            work=directory/mode;work.mkdir();shutil.copytree(source,work/'saves');C.ensure(R.inventory(work/'saves')==report['source_before'],'complete exact owned copy before original entry');C.ensure(directories(work/'saves')==report['source_directories_before'],'exact complete saved directory schema')
        report['compile']=C.capture([str(javac),'-encoding','UTF-8','-implicit:none','-classpath',cp,'-d',str(classes),str(HERE/'SpawnerTickObservation.java')],directory,'compile',60,env)
        transport(directory,report['compile']);C.ensure(report['compile']['stdout_bytes']==report['compile']['stderr_bytes']==0,'authored-only clean observer compile')
        products=sorted(classes.iterdir());C.ensure({p.name for p in products}=={'SpawnerTickObservation.class','SpawnerTickObservation$1.class','SpawnerTickObservation$Sink.class'} and all(p.is_file() for p in products),'only three authored products')
        report['compiled_before']={str(p.relative_to(directory)):pin(p) for p in products}
        for executable,stem in ((java,'java-version'),(javac,'javac-version')):
            report[stem]=C.capture([str(executable),'-version'],directory,stem,20,env);transport(directory,report[stem])
        C.ensure('openjdk version "17.0.17"' in (directory/'java-version.stderr').read_text(),'actual JDK17 version receipt')
        for mode in MODES:
            work=directory/mode;child=env.copy()
            for n in ('APPDATA','TEMP','TMP','USERPROFILE'):
                p=work/n.lower();p.mkdir();child[n]=str(p)
            cmd=command(java,cp,jar,directory,mode);report[mode]=C.capture(cmd,directory,mode,90,child,work);transport(directory,report[mode])
            C.ensure((directory/(mode+'.stderr')).read_text(encoding='utf8').splitlines()==warnings(classes),'all expected unsuppressed guard warnings, no hidden errors')
            observed=output((directory/(mode+'.stdout')).read_bytes(),report['class_pins'],declaration);report.setdefault('observations',{})[mode]=observed
            after=R.inventory(work/'saves');report.setdefault('copies_after',{})[mode]=after
            C.ensure(set(after)==set(report['source_before']) and sorted(n for n,p in after.items() if p!=report['source_before'][n])==[c['folder']+'/session.lock' for c in declaration['cases']] and all(after[c['folder']+'/session.lock']['bytes']==8 for c in declaration['cases']),'only ten owned SessionLocks change')
        C.ensure(same(report['observations']['interpreter']['summary']['common_projection'],report['observations']['default']['summary']['common_projection']),'predeclared deterministic common projection; raw mode outcomes retained')
        for mode in MODES:
            transport(directory,report[mode]);C.ensure(output((directory/(mode+'.stdout')).read_bytes(),report['class_pins'],declaration)==report['observations'][mode],'complete raw original stream reparses exactly')
        report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error),'traceback':traceback.format_exc()}
    finally:
        report['source_after']=R.inventory(source);report['source_unchanged']=report['source_before']==report['source_after'];report['source_directories_after']=directories(source);report['base_save_after']=R.inventory(base);report['base_save_unchanged']=report['base_save_after']==report['base_save_before']
        if report['source_directories_before']!=report['source_directories_after'] or not report['source_unchanged'] or not report['base_save_unchanged']:report['status']='FAIL'
        report['all_copy_directories_after']={m:directories(directory/m/'saves') for m in MODES if (directory/m/'saves').is_dir()}
        report['all_copies_after']={m:R.inventory(directory/m/'saves') for m in MODES if (directory/m/'saves').is_dir()}
        if 'inputs_before' in report:
            report['inputs_after']={n:pin(Path(n)) for n in report['inputs_before']};report['inputs_unchanged']=report['inputs_before']==report['inputs_after']
            if not report['inputs_unchanged']:report['status']='FAIL'
        if 'compiled_before' in report:
            report['compiled_after']={str(p.relative_to(directory)):pin(p) for p in sorted(classes.iterdir())};report['compiled_unchanged']=report['compiled_before']==report['compiled_after']
            if not report['compiled_unchanged']:report['status']='FAIL'
        write(directory/'capture.json',report);print(json.dumps({'status':report['status'],'output':str(directory),'error':report.get('error')},indent=2))
    return 0 if report['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
