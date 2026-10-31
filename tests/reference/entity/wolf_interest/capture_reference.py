"""Private headless unchanged-original CPU observation; no native/Display work."""
import argparse,hashlib,json,os,re,subprocess,sys,zipfile
from pathlib import Path
sys.dont_write_bytecode=True
BASE=Path(__file__).resolve().parent;ROOT=BASE.parents[3]
sys.path.insert(0,str(BASE))
from classfile import inspect_class
JAR='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
def need(v,s):
    if not v:raise AssertionError(s)
def sha(p):
    with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def dump(p,v):p.write_text(json.dumps(v,indent=2)+'\n',newline='\n')
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--jdk',type=Path,default=Path(r'C:\Program Files\Eclipse Adoptium\jdk-17.0.17.10-hotspot'));a=p.parse_args()
    need(not sys.flags.optimize and sys.platform=='win32','actual unoptimized Windows CPU toolchain')
    out=a.output.resolve();need(out.is_relative_to(ROOT/'.local') and not out.is_relative_to(BASE),'fresh capture below .local outside reference inputs');out.mkdir(parents=True,exist_ok=False)
    inputs=BASE;jar=ROOT/'b1.7.3.jar';freeze=BASE/'provenance.json';need(sha(jar)==JAR,'unchanged original JAR')
    frozen=json.loads(freeze.read_text());need(all(sha(inputs/n)==s and b'\r' not in (inputs/n).read_bytes() for n,s in frozen['reference_files'].items()),'all current LF reference inputs')
    java=a.jdk/'bin/java.exe';javac=a.jdk/'bin/javac.exe'
    observed_inputs=[*sorted(p for p in inputs.iterdir() if p.is_file()),java,javac,a.jdk/'bin/server/jvm.dll',a.jdk/'lib/modules',jar]
    before={str(p.resolve()):sha(p)for p in observed_inputs};report={'status':'FAIL','scope':'Fresh reproduction of exported original Wolf component; not historical packet replacement or Native parity','input_before_sha256':before,'default_constructor_entropy_rebound':False,'no_owner_seed_replay':True,'timeouts':{'normal':300,'debugger':1200},'commands':[]}
    env=os.environ.copy()
    for k in ['JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH']:env.pop(k,None)
    def run(name,cmd,cwd,timeout):
        report['commands'].append({'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'timeout':timeout})
        try:r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=env,capture_output=True,timeout=timeout)
        except subprocess.TimeoutExpired as e:
            (cwd/(name+'.stdout')).write_bytes(e.stdout or b'');(cwd/(name+'.stderr')).write_bytes(e.stderr or b'');raise
        (cwd/(name+'.stdout')).write_bytes(r.stdout);(cwd/(name+'.stderr')).write_bytes(r.stderr);need(r.returncode==0,name+' process '+str(r.returncode));return r
    failure=None
    try:
        version=run('java-version',[java,'-version'],out,30);need(re.search(rb'version "17\.',version.stdout+version.stderr),'actual JDK17')
        authority={}
        with zipfile.ZipFile(jar)as z:
            for name in z.namelist():
                if name.endswith('.class'):authority[name[:-6]]=inspect_class(z.read(name))
        dump(out/'authority.json',{'jar_sha256':JAR,'classes':authority})
        classes=out/'classes';classes.mkdir()
        result=run('compile',[javac,'-g:none','-encoding','UTF-8','--add-modules','jdk.jdi','-classpath',jar,'-d',classes,*sorted(inputs.glob('*.java'))],out,120)
        need(not result.stderr and not result.stdout,'clean authored helper compile')
        expected={'ConstructorJson.class','MobTickTarget.class','MobTickTarget$Observer.class','WolfInterestTarget.class','WolfInterestJdi.class'}
        need({p.name for p in classes.iterdir()}==expected,'helper-only directory; no original replacements')
        helpers={p.name:sha(p)for p in sorted(classes.iterdir())};need(helpers==frozen['compiled_helpers_b_sha256'],'same qualified compiled helper bytes after declared export');report['helper_before_sha256']=helpers
        dump(out/'helper-Code.json',{p.name:inspect_class(p.read_bytes())for p in sorted(classes.iterdir())})
        for mode in ['normal','debugger']:
            d=out/mode;d.mkdir();(d/'classes.txt').write_text('\n'.join(sorted(authority))+'\n',newline='\n')
        normal=out/'normal';cp=str(classes)+os.pathsep+str(jar)
        run('target',[java,'-Xint','-Xverify:all','-Xmx512m','-Djava.awt.headless=true','--add-opens=java.base/java.util=ALL-UNNAMED','--add-opens=java.base/java.lang=ALL-UNNAMED','-Dmobtick.classes='+str(normal/'classes.txt'),'-Xlog:class+init=info:file=class-init.log','-classpath',cp,'WolfInterestTarget',normal/'target.ndjson',jar,inputs],normal,300)
        debugger=out/'debugger'
        run('controller',[java,'-Xmx512m','--add-modules','jdk.jdi','-classpath',classes,'WolfInterestJdi',jar,classes,debugger,a.jdk,inputs],debugger,1200)
        modes={}
        for mode in ['normal','debugger']:
            d=out/mode;rows=[json.loads(line)for line in(d/'target.ndjson').read_text().splitlines()];need([r['case']for r in rows]==list(range(9)),mode+' complete actual nine rows')
            stdout=(d/('target.stdout')).read_text();need('ORIGINAL_WOLF_INTEREST_COMPLETE'in stdout,mode+' actual completion')
            bindings={}
            for line in stdout.splitlines():
                if not line.startswith('BIND '):continue
                _,n,origin,size,digest=line.split();need(n not in bindings and n in authority,mode+' actual binding key')
                from urllib.parse import unquote,urlparse
                path=unquote(urlparse(origin).path)
                if re.match(r'^/[A-Za-z]:/',path):path=path[1:]
                need(Path(path).resolve()==jar.resolve(),mode+' real original archive origin')
                need(int(size)==authority[n]['class_bytes']and digest==authority[n]['class_sha256'],mode+' whole class pin '+n)
                bindings[n]={'origin':origin,'bytes':int(size),'sha256':digest}
            initialized={n for n in re.findall(r"Initializing '([^']+)'",(d/'class-init.log').read_text())if n in authority}
            need(initialized<=set(bindings),mode+' full actual initialized original closure pinned')
            modes[mode]={'rows':rows,'bindings':bindings,'initialized_original_classes':sorted(initialized),'post_observation_binding_unavailable':[line for line in stdout.splitlines()if line.startswith('BIND_UNAVAILABLE ')]}
        events=[json.loads(line)for line in(debugger/'checkpoints.ndjson').read_text().splitlines()]
        pins=[r for r in events if r['event']=='code-pin']
        for row in pins:
            owner=row['class'].replace('.','/');candidates=[m for m in authority[owner]['methods']if m['name']==row['method']and m['descriptor']==row['descriptor']];need(len(candidates)==1 and candidates[0].get('code_bytes')==row['bytes']and candidates[0].get('code_sha256')==row['sha256'],'actual JDI live Code pin')
        need([r['case']for r in events if r['event']=='active-entry']==list(range(9))and[r['case']for r in events if r['event']=='active-return']==list(range(9)),'all actual read-only active boundaries')
        report.update(status='OBSERVED_NINE_WOLF_INTEREST_CPU_CASES',modes=modes,live_Code=len(pins),JDI_events=len(events),ordinary_only=False,read_only_JDI=True,post_observation_binding_scope='Class.forName(false) resources only; unavailable optional classes outside initialized closure retained raw')
    except BaseException as e:failure=e;report['failure']=repr(e)
    finally:
        report['input_after_sha256']={p:sha(p)for p in before};report['inputs_unchanged']=report['input_after_sha256']==before
        if 'helpers'in locals():report['helper_after_sha256']={p.name:sha(p)for p in sorted(classes.iterdir())};report['helpers_unchanged']=report['helper_after_sha256']==helpers
        if not report['inputs_unchanged']or not report.get('helpers_unchanged'):report['status']='FAIL'
        report['artifacts_sha256']={p.relative_to(out).as_posix():{'bytes':p.stat().st_size,'sha256':sha(p)}for p in sorted(out.rglob('*'))if p.is_file()and p.name!='receipt.json'}
        dump(out/'receipt.json',report);print(out/'receipt.json',report['status'],report.get('failure'))
    if failure is not None or report['status']=='FAIL':raise SystemExit(1)
if __name__=='__main__':main()
