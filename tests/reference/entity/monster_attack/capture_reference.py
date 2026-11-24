"""Explicit opt-in Original recorder; results remain unqualified until independent review."""
import argparse,hashlib,importlib.util,json,os,re,signal,subprocess,sys,time,traceback,zipfile
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import url2pathname
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in Path(__file__).resolve().parents if (p/"CMakeLists.txt").is_file())
JAR="af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
HELPERS=["ConstructorJson","MonsterTarget","MonsterJdi"]
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(ok,message):
    if not ok:raise RuntimeError(message)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record-unqualified-prototype",action="store_true",required=True)
    parser.add_argument("--client-jar",type=Path,required=True)
    parser.add_argument("--jdk-home",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();require(not sys.flags.optimize,"unoptimized validation")
    jar=args.client_jar.resolve(strict=True);jdk=args.jdk_home.resolve(strict=True)
    java=jdk/"bin"/("java.exe" if os.name=="nt" else "java")
    javac=jdk/"bin"/("javac.exe" if os.name=="nt" else "javac")
    require(digest(jar)==JAR,"unchanged pinned archive")
    parser_path=ROOT/"tests/reference/client/controller/classfile.py"
    spec=importlib.util.spec_from_file_location("classfile",parser_path)
    cf=importlib.util.module_from_spec(spec);spec.loader.exec_module(cf)
    declared=json.loads((HERE/"source-pins.json").read_text())
    with zipfile.ZipFile(jar) as archive:
        for name,pin in declared["classes"].items():require(cf.inspect_class(archive.read(name+".class"))==pin,"primary Code pin "+name)
        inventory={n:hashlib.sha256(archive.read(n)).hexdigest() for n in archive.namelist() if n.endswith(".class")}
    args.output.mkdir(parents=True,exist_ok=False);out=args.output.resolve();classes=out/"classes";classes.mkdir()
    paths=[java,javac,jdk/"lib/modules",jar,parser_path,Path(__file__).resolve(),HERE/"profiles.tsv",HERE/"source-pins.json",*(HERE/(n+".java") for n in HELPERS)]
    before={str(p):digest(p) for p in paths}
    report={"status":"FAIL","scope":"UNQUALIFIED private prototype; no Native or original parity credit","input_before_sha256":before,"commands":[],"original_class_inventory":inventory}
    env=os.environ.copy()
    for key in ["JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS","CLASSPATH"]:env.pop(key,None)
    def run(argv,cwd,name,timeout):
        argv=list(map(str,argv));stdout=cwd/(name+".stdout");stderr=cwd/(name+".stderr")
        row={"argv":argv,"cwd":str(cwd),"timeout_seconds":timeout,"stdout":str(stdout.relative_to(out)),"stderr":str(stderr.relative_to(out))};report["commands"].append(row)
        start=time.monotonic();reason=None
        with stdout.open("wb") as so,stderr.open("wb") as se:
            kwargs={"creationflags":subprocess.CREATE_NEW_PROCESS_GROUP} if os.name=="nt" else {"start_new_session":True}
            proc=subprocess.Popen(argv,cwd=cwd,env=env,stdin=subprocess.DEVNULL,stdout=so,stderr=se,**kwargs)
            while proc.poll() is None:
                # A bounded whole-private-output cap also covers the JDI child trace.
                sizes=[p.stat().st_size for p in out.rglob("*") if p.is_file()]
                if time.monotonic()-start>timeout:reason="process-tree timeout"
                elif max(sizes,default=0)>64*1024*1024 or sum(sizes)>96*1024*1024:reason="private output bound"
                if reason:
                    if os.name=="nt":
                        kill=["taskkill","/PID",str(proc.pid),"/T","/F"]
                        stopped=subprocess.run(kill,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=20)
                        row["tree_termination"]={"argv":kill,"exit":stopped.returncode,"stdout":stopped.stdout.decode(errors="replace")[:2048],"stderr":stopped.stderr.decode(errors="replace")[:2048]}
                    else:os.killpg(proc.pid,signal.SIGKILL)
                    proc.wait(timeout=20);break
                time.sleep(.1)
            row.update(exit=proc.wait(),seconds=time.monotonic()-start,termination_reason=reason)
        require(reason is None and row["exit"]==0,"command failed; full artifact "+str(stderr))
        return stdout.read_bytes(),stderr.read_bytes()
    failed=None
    try:
        for name,tool in [("java",java),("javac",javac)]:
            so,se=run([tool,"-version"],out,name,30)
            require(re.search(rb'(?:version "|javac )17\.',so+se),"actual JDK17")
        so,se=run([javac,"-g:none","-encoding","UTF-8","--add-modules","jdk.jdi","-classpath",jar,"-d",classes,*(HERE/(n+".java") for n in HELPERS)],out,"compile",60)
        require(not so and not se,"clean helper compilation")
        require({p.stem for p in classes.glob("*.class")}==set(HELPERS),"only authored helper classes")
        require(not any(n+".class" in inventory for n in HELPERS),"no helper/original class collision")
        report["helper_before_sha256"]={p.name:digest(p) for p in classes.glob("*.class")}
        report["helper_code_before"]={p.name:cf.inspect_class(p.read_bytes()) for p in classes.glob("*.class")}
        common=["-Xint","-Xverify:all","-Xmx512m","--add-opens=java.base/java.util=ALL-UNNAMED","--add-opens=java.base/java.lang=ALL-UNNAMED","-Xlog:class+load=info,class+init=info:file=class-init.log"]
        normal=out/"normal";normal.mkdir()
        so,se=run([java,*common,"-classpath",str(classes)+os.pathsep+str(jar),"MonsterTarget","target.ndjson",HERE/"profiles.tsv"],normal,"target",90)
        require(not se,"ordinary target stderr")
        debugger=out/"debugger";debugger.mkdir()
        so,se=run([java,"--add-modules","jdk.jdi","-classpath",classes,"MonsterJdi",jar,classes,debugger,jdk,HERE/"profiles.tsv"],debugger,"controller",150)
        require(not so and not se and not (debugger/"target.stderr").read_bytes(),"read-only observer/target stderr")
        bindings={};original_pins={}
        with zipfile.ZipFile(jar) as archive:
            for mode in ["normal","debugger"]:
                rows=[]
                for line in (out/mode/"class-init.log").read_text().splitlines():
                    m=re.search(r'\[class,load\] ([A-Za-z0-9_.$]+) source: (.+)$',line)
                    if not m or m[1].replace(".","/")+".class" not in inventory:continue
                    name=m[1];parsed=urlparse(m[2]);require(parsed.scheme=="file" and Path(url2pathname(parsed.path)).resolve()==jar,"actual Original class origin "+name)
                    rows.append({"class":name,"origin":m[2]});original_pins[name]=cf.inspect_class(archive.read(name.replace(".","/")+".class"))
                require(rows,"actual Original runtime class closure");bindings[mode]=rows
        live=[json.loads(line) for line in (debugger/"checkpoints.ndjson").read_text().splitlines()]
        checked=0
        for row in live:
            if row.get("event")!="code-pin" or row["class"] not in original_pins:continue
            method=next(m for m in original_pins[row["class"]]["methods"] if m["name"]==row["method"] and m["descriptor"]==row["descriptor"])
            require(method["code_bytes"]==row["bytes"] and method["code_sha256"]==row["sha256"],"actual live Original Code pin")
            checked+=1
        (out/"runtime-bindings.json").write_text(json.dumps({"bindings":bindings,"original_class_pins":original_pins,"live_original_code_matches":checked},indent=2)+"\n")
        report["status"]="PROTOTYPE_RECORDED_UNQUALIFIED"
    except BaseException as error:
        failed=error;report["failure"]={"type":type(error).__name__,"message":str(error)[:2048]}
        (out/"driver.failure.txt").write_text(traceback.format_exc())
    finally:
        report["input_after_sha256"]={str(p):digest(p) for p in paths}
        report["helper_after_sha256"]={p.name:digest(p) for p in classes.glob("*.class")}
        report["helper_code_after"]={p.name:cf.inspect_class(p.read_bytes()) for p in classes.glob("*.class")}
        same=before==report["input_after_sha256"]
        helpers_same=report.get("helper_before_sha256")==report["helper_after_sha256"] if "helper_before_sha256" in report else None
        report["helpers_unchanged"]=helpers_same
        report["inputs_unchanged"]=same
        report["output_artifacts"]={p.relative_to(out).as_posix():{"bytes":p.stat().st_size,"sha256":digest(p)} for p in sorted(out.rglob("*")) if p.is_file() and p.name!="capture.json"}
        if not same or helpers_same is False:report["status"]="FAIL"
        (out/"capture.json").write_text(json.dumps(report,indent=2)+"\n")
    require(failed is None and same and helpers_same is not False,"prototype failed; inspect "+str(out/"capture.json"))
    print(out/"capture.json")
if __name__=="__main__":main()
