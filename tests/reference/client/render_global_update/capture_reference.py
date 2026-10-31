"""Capture original N/MD/DK behavior with read-only JDI method events."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import zipfile
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def updates(text):
    rows=[line for line in text.splitlines() if line.startswith("UPDATE ")]
    cases=json.loads((HERE/"inputs.json").read_text())["updates"]
    ensure(len(rows)==len(cases)==42,"all42 update outcomes required")
    for i,(row,case) in enumerate(zip(rows,cases)):
        cells=row.split()
        ensure(len(cells)==16 and cells[:3]==["UPDATE",str(i),case["name"]],"update row identity/shape")
        ensure(cells[3] in ("-1","0","1") and cells[4] in ("NONE","NPE","IAE"),"source result/throw encoding")
        ensure((cells[3]=="-1")== (cells[4]!="NONE"),"failed call cannot masquerade as false")
        ensure(int(cells[13]) in (199,242,340,411,442,519,697),"reached original selector PC")
    return rows

def encode_cases(inputs):
    lines=[]
    def camera(value):
        return "N" if value is None else ",".join(struct.pack(">d",float(v)).hex() for v in value)
    for kind,key in [("UPDATE","updates"),("SORT","sorts"),("COMPARE","compares")]:
        for i,case in enumerate(inputs[key]):
            queue=case.get("queue",case.get("order",[case.get("left"),case.get("right")]))
            if kind=="UPDATE" and "queue" not in case:queue=list(range(1,len(case["actors"])+1))
            specs=",".join(":".join(str(v) for v in [a["q"],a["r"],a["s"],a["w"],int(a["visible"]),int(a.get("dirty",False))]) for a in case["actors"])
            lines.append("\t".join([kind,str(i),case["name"],str(int(case.get("visible_only",False))),str(int(case.get("null_list",False))),camera(case["camera"]),",".join(str(v or 0) for v in queue),specs]))
    return "\n".join(lines)+"\n"

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--client-jar",type=Path,required=True)
    parser.add_argument("--library-root",type=Path,required=True)
    parser.add_argument("--java",default=shutil.which("java"))
    parser.add_argument("--javac",default=shutil.which("javac"))
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    java,javac=[Path(shutil.which(p) or "").resolve(strict=True) for p in [args.java,args.javac]]
    ensure(java.parent.samefile(javac.parent),"matching JDK17")
    ensure(digest(args.client_jar)==JAR,"unchanged original client JAR")
    libraries=[args.library_root/name for name in LIBRARIES]
    ensure(all(digest(p)==h for p,h in zip(libraries,LIBRARIES.values())),"pinned SDK metadata")
    pins=json.loads((HERE/"source-pins.json").read_text())
    with zipfile.ZipFile(args.client_jar) as jar:
        for name,pin in pins["classes"].items():
            actual=inspect_class(jar.read(name+".class"));wanted={(m["name"],m["descriptor"]) for m in pin["methods"]}
            actual["methods"]=[m for m in actual["methods"] if (m["name"],m["descriptor"]) in wanted]
            ensure(actual==pin,"original class/Code identity: "+name)
    inputs_file=args.output/"cases.tsv";inputs_file.write_text(encode_cases(json.loads((HERE/"inputs.json").read_text())),encoding="ascii",newline="\n")
    dependencies=[java,javac,java.parent.parent/"lib/modules",java.parent.parent/"release",args.client_jar,*libraries,inputs_file,HERE/"inputs.json",HERE/"source-pins.json",HERE/"UpdateObservation.java",HERE/"UpdateController.java",HERE/"capture_reference.py",SUPPORT/"capture.py",SUPPORT/"classfile.py"]
    before={str(p.resolve()):digest(p) for p in dependencies}
    report={"status":"FAIL","input_before_sha256":before,"scope":"Untouched original N selector, MD comparator, DK clean compiler return and genuine dirty null-World EW failure. Original method bytecodes observed through read-only JDI. No class transformation, replacement game method or forced return. Loopback JDWP transport is outside the measured game's deny guard; no SDK/GL/native-library calls."}
    env=os.environ.copy()
    for key in ["JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS"]:env.pop(key,None)
    classes=args.output/"classes";classes.mkdir()
    try:
        for label,tool in [("java",java),("javac",javac)]:
            value=capture([str(tool),"-version"],args.output,label,10,env);require_success(value)
            text=(args.output/value["stdout_file"]).read_text()+(args.output/value["stderr_file"]).read_text();ensure(re.search(r'(?:version "|javac )17\.',text),"JDK17 required");report[label+"_version"]=text
        cp=os.pathsep.join(str(p.resolve()) for p in [classes,args.client_jar,*libraries])
        compiled=capture([str(javac),"--add-modules","jdk.jdi","-encoding","UTF-8","-classpath",cp,"-d",str(classes),str(HERE/"UpdateObservation.java"),str(HERE/"UpdateController.java")],args.output,"compile",30,env);require_success(compiled,empty_stderr=True)
        helper={p.relative_to(classes).as_posix():digest(p) for p in classes.rglob("*.class")};report["helper_before_sha256"]=helper
        ensure(set(helper)=={"UpdateObservation.class","UpdateObservation$1.class","UpdateController.class"},"only declared observer/controller classes")
        receipt=capture([str(java),"--add-modules","jdk.jdi","-classpath",str(classes.resolve()),"UpdateController",cp,str(args.client_jar.resolve()),str(inputs_file.resolve())],args.output,"original",180,env,args.output.resolve());report["original"]=receipt;require_success(receipt)
        text=(args.output/receipt["stdout_file"]).read_text();ensure(text.splitlines()[-1]=="Original N update capture: 64 cases; untouched methods; read-only JDI","controller completion")
        observed=updates(text);sorts=[l for l in text.splitlines() if l.startswith("SORT ")];compares=[l for l in text.splitlines() if l.startswith("COMPARE ")];traces=[l for l in text.splitlines() if l.startswith("TRACE ")];cmps=[l for l in text.splitlines() if l.startswith("CMPS ")]
        ensure(len(sorts)==14 and len(compares)==8 and len(traces)==len(cmps)==64,"all source cohorts/traces")
        code={}
        for line in text.splitlines():
            if not line.startswith("CODE "):continue
            _,owner,name,desc,size,hashvalue=line.split();code[owner+"."+name+desc]={"bytes":int(size),"sha256":hashvalue}
            if owner in pins["classes"]:
                method=next(m for m in pins["classes"][owner]["methods"] if m["name"]==name and m["descriptor"]==desc)
                ensure(method["code_bytes"]==int(size) and method["code_sha256"]==hashvalue,"loaded original Code unchanged")
        ensure("n.a(Lls;Z)Z" in code and "md.a(Ldk;Ldk;)I" in code and any(k.startswith("java.util.TimSort.") for k in code),"actual selector/comparator/JDK sort Code receipts")
        fixture={"rows":observed,"sort_rows":sorts,"compare_rows":compares,"comparison_traces":cmps,"exception_traces":traces,"runtime":[l for l in text.splitlines() if l.startswith("RUNTIME ")],"loaded_code":code}
        (args.output/"finite.json").write_text(json.dumps(fixture,indent=2)+"\n",encoding="utf-8",newline="\n")
        report.update(status="PASS",updates=42,sorts=14,compares=8,fixture_sha256=digest(args.output/"finite.json"),loaded_code=code)
    finally:
        report["input_after_sha256"]={str(p.resolve()):digest(p) for p in dependencies};report["helper_after_sha256"]={p.relative_to(classes).as_posix():digest(p) for p in classes.rglob("*.class")}
        same=before==report["input_after_sha256"] and ("helper_before_sha256" not in report or helper==report["helper_after_sha256"])
        if not same:report["status"]="FAIL"
        (args.output/"capture.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n");ensure(same,"capture inputs changed")
    print("Original N update: 42 update/14 sort/8 comparator cases; "+str(args.output/"capture.json"))

if __name__=="__main__":main()
