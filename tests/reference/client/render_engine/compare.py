"""Reproduce genuine ji/ge constructors and optional original texture prefixes."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile
from urllib.parse import unquote,urlparse
import zipfile
from support import require,run,constructor_rows,texture_rows
HERE=Path(__file__).resolve().parent


def fp(path):
    data=path.read_bytes();return {"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()}


def bindings(lines,pins,jar):
    observed={}
    for line in lines:
        if not line.startswith(b"BIND\t"):continue
        _,name,origin,digest=line.decode("utf-8").split("\t")
        parsed=urlparse(origin);path=unquote(parsed.path)
        if re.match(r"^/[A-Za-z]:/",path):path=path[1:]
        require(parsed.scheme=="file" and Path(path).resolve()==jar,"actual class JAR origin")
        require(name not in observed and digest==pins["classes"][name]["class_sha256"],"actual class bytes")
        observed[name]=digest
    require(list(observed)==list(pins["classes"]),"exact original binding order/inventory")
    return observed


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ["client-jar","constructor","java","javac","libraries","work-dir"]:parser.add_argument("--"+n,type=Path,required=True)
    parser.add_argument("--textures",type=Path);parser.add_argument("--native-dir",type=Path)
    args=parser.parse_args();repo=HERE.parents[3]
    require(args.java.resolve().parent==args.javac.resolve().parent,"matching explicit JDK17")
    for key in ["JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS","FREETYPE_PROPERTIES"]:require(not os.environ.get(key),"ambient option "+key)
    env=os.environ.copy()
    for key in ["JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS","CLASSPATH"]:env.pop(key,None)
    require(not args.work_dir.resolve().is_relative_to(HERE.parents[2]),"private capture outside source tests")
    args.work_dir.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix="ji-original-",dir=args.work_dir.resolve()))
    pins=json.loads((HERE/"source-pins.json").read_text());proof=json.loads((HERE/"provenance.json").read_text())
    refs={n:fp(HERE/n) for n in proof["reference_files"]};native={n:fp(repo/n) for n in proof["native_sources"]};controls={n:fp(repo/n) for n in proof["controller_files"]}
    require(refs==proof["reference_files"] and native==proof["native_sources"] and controls==proof["controller_files"],"complete source/probe pins")
    jar=args.client_jar.resolve(strict=True);require(fp(jar)==pins["client_jar"],"whole original JAR")
    libraries=[args.libraries.resolve(strict=True)/n for n in pins["libraries"]]
    for f in libraries:require(fp(f)==pins["libraries"][f.name],"original library "+f.name)
    spec=importlib.util.spec_from_file_location("render_classfile",HERE.parent/"notice/classfile.py");require(spec is not None and spec.loader is not None,"class parser")
    classfile=importlib.util.module_from_spec(spec);spec.loader.exec_module(classfile)
    with zipfile.ZipFile(jar) as z:
        jar_classes={n[:-6] for n in z.namelist() if n.endswith(".class")}
        for n,pin in pins["classes"].items():require(classfile.inspect_class(z.read(n+".class"))==pin,"class/Code "+n)
    products=[args.constructor.resolve(strict=True),args.java.resolve(strict=True),args.javac.resolve(strict=True),jar,*libraries]
    if args.textures is not None:products.append(args.textures.resolve(strict=True))
    if args.textures is not None:
        require(args.native_dir is not None,"native library directory")
        products += [args.native_dir.resolve()/n for n in pins["windows_native"]]
    before={str(f):fp(f) for f in products}
    for label,tool in [("java",args.java),("javac",args.javac)]:
        r=run([tool.resolve(),"-version"],work,label+"-version",env=env)
        require(r.returncode==0 and re.search(rb'(?:version "|javac )17\.',r.stdout+r.stderr) is not None,"actual explicit JDK17")
    report={"status":"PENDING","scope":"actual ji constructor/default-font raster and separately optional real GL prefixes; no normal bootstrap/ik/effect tick claim","inputs":before,"reference_sources":refs,"native_sources":native,"controllers":controls,"captures":{}}
    helper_pins={}
    def observe(name,arguments,options=()):
        helpers=work/(name+"-helpers");helpers.mkdir()
        cp=os.pathsep.join(map(str,[helpers,jar,*libraries]))
        compiled=run([args.javac.resolve(),"--release","17","-encoding","UTF-8","-cp",cp,"-d",helpers,HERE/(name+".java")],work,name+"-compile",env=env)
        require(compiled.returncode==0 and compiled.stderr==b"","authored helper compile")
        hp={f.relative_to(helpers).as_posix():fp(f) for f in helpers.rglob("*") if f.is_file()};require(set(hp)=={name+".class"},"helper-only classpath")
        command=[args.java.resolve(),"-Xint","-Xverify:all","-Xmx256m","-Xlog:class+init=info:file="+name+"-class-init.log",*options,"-cp",cp,name,*arguments]
        actual=run(command,work,name,env=env)
        require(actual.returncode==0 and actual.stderr==b"","actual original exit/diagnostics")
        initialized=re.findall(r"Initializing '([^']+)'",(work/(name+"-class-init.log")).read_text(encoding="utf-8"))
        initialized=[n for n in initialized if n in jar_classes]
        require(initialized==(["ji","ge"] if name=="FontObservation" else ["kv","qb","ji","ge"]),"exact actual original initialization inventory/order")
        report.setdefault("original_initialization",{})[name]=initialized
        require(hp=={f.relative_to(helpers).as_posix():fp(f) for f in helpers.rglob("*") if f.is_file()},"helper products unchanged")
        helper_pins[name]=hp;report["captures"][name]=actual.capture_report
        return actual
    font=observe("FontObservation",[work/"font-image"],["-Djava.awt.headless=true","--add-opens=java.desktop/sun.font=ALL-UNNAMED"])
    lines=font.stdout.splitlines();report["font_bindings"]=bindings(lines,pins,jar)
    require(len(lines)==28,"complete original font stdout inventory")
    require(lines[4].startswith(b"FONT ") and lines[5].startswith(b"HINTS ") and lines[6]==b"FONT2D sun.font.CompositeFont" and lines[7].startswith(b"FONTFILE\t"),"ordered actual font/provider rows")
    require([line.split()[0] for line in lines[8:18]]==[b"GLYPH"]*10 and lines[18].startswith(b"IMAGE 64 64 BLACK "),"ordered glyph/image inventory")
    require(lines[19:24]==[("OWNER %s 0"%n).encode() for n in ["b","c","d","g","h"]] and lines[24:26]==[b"BUFFER e 0 1 1 true",b"BUFFER f 0 1048576 1048576 true"],"original collection/buffer controls")
    require(lines[-1:]==[b"DONE actual ji constructor, direct JDK default-font control"],"font original completion")
    actual=run([args.constructor.resolve(),"--trace-image"],work,"native-constructor",env=env);selected=constructor_rows(actual,True)
    require(selected["font"].endswith(" 2.13.2"),"actual pinned FreeType provider version")
    image=(work/"font-image/missing.argb").read_bytes();require(len(image)==16384 and selected["pixels"]==image,"all actual/native ARGB bytes")
    glyphs=[]
    for line in lines:
        if not line.startswith(b"GLYPH "):continue
        m=re.fullmatch(rb"GLYPH ([0-9]+) ([0-9]+) Point2D.Float\[([0-9]+)\.0, 0\.0\] ([0-9]+)\.0 java.awt.Rectangle\[x=(-?[0-9]+),y=(-?[0-9]+),width=([0-9]+),height=([0-9]+)\]",line)
        require(m is not None and int(m[1])==len(glyphs),"ordered actual glyph metric")
        glyphs.append(tuple(int(m[i]) for i in range(2,9)))
    require(len(glyphs)==10 and glyphs==selected["metrics"],"actual/native glyph IDs positions advances bounds")
    require([x for x in lines if x.startswith(b"NAMESPACE ")]==[b"NAMESPACE ji-false ge-empty-distinct buffers-native-zero"],"original namespace/buffer controls")
    fontfiles=[x.decode("utf-8").split("\t") for x in lines if x.startswith(b"FONTFILE\t")];require(len(fontfiles)==1,"actual physical font file")
    physical=Path(fontfiles[0][1]);require(fp(physical)["sha256"]==fontfiles[0][2],"actual system font bytes")
    report["system_font"]={"path":str(physical),**fp(physical)};report["font_metrics"]=[x.decode("utf-8") for x in lines if x.startswith(b"FONT ")];report["glyphs"]=glyphs
    report["font_image"]={"bytes":len(image),"sha256":hashlib.sha256(image).hexdigest(),"native_checks":selected["checks"]};report["captures"]["native-constructor"]=actual.capture_report
    if args.textures is not None:
        require(os.name=="nt" and args.native_dir is not None,"qualified original compatibility GL profile is Windows x64")
        for n,pin in pins["windows_native"].items():require(fp(args.native_dir/n)==pin,"native library pin")
        original=observe("TextureObservation",[],["-Dorg.lwjgl.librarypath="+str(args.native_dir.resolve())])
        rows=original.stdout.splitlines();report["texture_bindings"]=bindings(rows,pins,jar)
        require(len(rows)==18 and all(rows[i].startswith(prefix) for i,prefix in enumerate([b"GL_VENDOR ",b"GL_RENDERER ",b"GL_VERSION "])),"complete actual original GL stdout")
        require(all(rows[3+i].startswith(b"BIND\t"+n.encode()+b"\t") for i,n in enumerate(pins["classes"])),"ordered original GL origins")
        require(rows[-1:]==[b"TEXTURE_ORIGINAL completed"],"actual GL completion")
        device=run([args.textures.resolve(),"--reference"],work,"native-textures",env=env);observed=texture_rows(device,True)
        actual_rows=[x for x in rows if x.startswith((b"REG ",b"GPU ",b"TRANSFER "))]
        require(observed["rows"]==actual_rows,"all actual/native device and source-buffer prefixes")
        report["texture_rows"]=[x.decode() for x in actual_rows];report["texture_checks"]=observed["checks"];report["captures"]["native-textures"]=device.capture_report;report["driver"]=[x.decode() for x in rows[:3]]
    require(before=={n:fp(Path(n)) for n in before},"input product bytes unchanged")
    require(refs=={n:fp(HERE/n) for n in refs} and native=={n:fp(repo/n) for n in native} and controls=={n:fp(repo/n) for n in controls},"source bytes unchanged")
    require(fp(physical)=={k:report["system_font"][k] for k in ("bytes","sha256")},"system font unchanged")
    report["helper_products"]=helper_pins;report["status"]="PASS";report["work_directory"]=str(work)
    (work/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"status":"PASS","work_directory":str(work),"font_image":report["font_image"],"texture_rows":len(report.get("texture_rows",[]))}))


if __name__=="__main__":main()
