"""Root-released two-VM captures for each of two separate original cohorts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module("redstone_capture", ROOT / "tests/reference/client/controller/capture.py")
I = module("redstone_classfile", ROOT / "tests/reference/client/controller/classfile.py")
P = module("redstone_saved_world", ROOT / "tests/reference/client/picking/observe.py")


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="ascii", newline="\n")


def inventory(path):
    return {p.relative_to(path).as_posix(): C.digest(p) for p in sorted(path.rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--jar", type=Path, default=ROOT / "b1.7.3.jar")
    parser.add_argument("--libraries", type=Path, default=Path(os.environ.get("APPDATA", "")) / ".minecraft/libraries")
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    C.ensure(args.execute, "Preparation only; Root must release --execute")
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, "verified unchanged original JAR")
    java, javac = [Path(shutil.which(p) or p).resolve(strict=True) for p in (args.java, args.javac)]
    C.ensure(java.parent == javac.parent, "matching JDK")
    jdk = java.parent.parent
    jvm = java.parent / "server/jvm.dll"
    if not jvm.is_file():
        jvm = jdk / "lib/server/libjvm.so"
    jmod = jdk / "jmods/java.base.jmod"
    libraries = [(args.libraries / name).resolve(strict=True) for name in P.LIBRARIES]
    C.ensure(all(C.digest(p) == pin for p,pin in zip(libraries,P.LIBRARIES.values())), "pinned libraries")
    declaration = json.loads((HERE / "declared-inputs.json").read_text())
    input_freeze = json.loads((HERE / "input-freeze.json").read_text())
    C.ensure(all(C.digest(HERE / name) == pin for name,pin in input_freeze["files"].items()), "roster frozen before helper")
    pins = json.loads((HERE / "source-pins.json").read_text())
    C.ensure(C.digest(jmod) == pins["jdk_module_sha256"], "qualified JDK module")
    with zipfile.ZipFile(jar) as archive:
        C.ensure(all(I.inspect_class(archive.read(name+".class")) == info
                     for name,info in pins["classes"].items()), "all source class/Code pins")
    with zipfile.ZipFile(jmod) as archive:
        C.ensure(all(I.inspect_class(archive.read("classes/"+name+".class")) == info
                     for name,info in pins["jdk_classes"].items()), "all selected JDK class/Code pins")
    authored = [HERE / name for name in (
        ".gitattributes", "RedstoneUseObservation.java", "capture_reference.py",
        "declared-inputs.json", "input-freeze.json", "source-pins.json",
        "support-profiles.tsv", "use-profiles.tsv")]
    inputs = [jar,*libraries,java,javac,jvm.resolve(strict=True),jmod,jdk/"lib/modules",*java.parent.glob("*.dll"),*authored,
              ROOT/"tests/reference/client/controller/capture.py",ROOT/"tests/reference/client/controller/classfile.py",
              ROOT/"tests/reference/client/picking/observe.py",ROOT/"tests/tools/client/test_picking.py"]
    before = {str(p.resolve()): C.digest(p) for p in sorted(set(inputs))}
    output = args.output.resolve(); output.mkdir(parents=True,exist_ok=False)
    classes = output / "classes"; classes.mkdir()
    environment = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS"):
        environment.pop(name,None)
    cp = os.pathsep.join(map(str,[classes,jar,*libraries]))
    worlds = {}
    for cohort in ("use","support"):
        for repeat in range(2):
            run = output / (cohort+"-ordinary-"+str(repeat)); run.mkdir()
            world = run/"world"; P.world(world); worlds[(cohort,repeat)] = (run,world,inventory(world))
    write(output/"freeze.json",{"inputs_before":before,"world_inputs":{
        cohort+"-"+str(repeat):entry[2] for (cohort,repeat),entry in worlds.items()}})
    report = {"status":"FAIL","runs":[],"inputs_before":before,"scope":"42 genuine IZ wrappers and12 genuine direct SM predicates in separate fresh World cohorts; two ordinary VMs each. No native/GUI/JDI qualification."}
    try:
        for role,tool in (("java",java),("javac",javac)):
            captured=C.capture([str(tool),"-version"],output,role,10,environment)
            report[role]=captured; C.require_success(captured)
            version=(output/(role+".stdout")).read_text()+(output/(role+".stderr")).read_text()
            C.ensure("17.0.17" in version,"qualified JDK17.0.17")
        compiled=C.capture([str(javac),"-encoding","UTF-8","-implicit:none","-cp",cp,"-d",str(classes),
                            str(HERE/"RedstoneUseObservation.java")],output,"compile",30,environment)
        report["compile"]=compiled; C.require_success(compiled,empty_stderr=True)
        helpers=inventory(classes); report["helpers_before"]=helpers
        C.ensure(set(helpers)=={"RedstoneUseObservation.class","RedstoneUseObservation$1.class",
                                "RedstoneUseObservation$Observer.class"},"only authored helper classes")
        projections={}
        for cohort in ("use","support"):
            prefix="USE" if cohort=="use" else "SUPPORT"
            expected=declaration[cohort+"_rows"]
            for repeat in range(2):
                run,world,world_before=worlds[(cohort,repeat)]
                command=[str(java),"-Xint","-Xverify:all","-Xmx256m","-Djava.awt.headless=true",
                         "-Djava.security.manager=allow","--add-opens","java.base/java.util=ALL-UNNAMED",
                         "--add-opens","java.base/java.lang=ALL-UNNAMED","-cp",cp,"RedstoneUseObservation",
                         str(jar),str(world),str(HERE/(cohort+"-profiles.tsv")),cohort]
                captured=C.capture(command,run,"original",90,environment,run)
                item={"cohort":cohort,"repeat":repeat,"capture":captured,"world_before":world_before,
                      "world_after":inventory(world)}
                report["runs"].append(item)
                # Keep all four fixed outcomes, including an early process failure.
        for item in report["runs"]:
            cohort,repeat=item["cohort"],item["repeat"]
            prefix="USE" if cohort=="use" else "SUPPORT"
            expected=declaration[cohort+"_rows"]
            run,world,world_before=worlds[(cohort,repeat)]
            captured=item["capture"]
            data=(run/"original.stdout").read_bytes(); lines=data.decode("ascii").splitlines()
            selected=[line for line in lines if line.startswith(prefix+"\t") or line.startswith("COMPLETE\t")]
            own=[json.loads(line.split("\t",1)[1]) for line in lines if line.startswith("OWN_ENTROPY\t")]
            rows=[json.loads(line.split("\t",1)[1]) for line in selected if line.startswith(prefix+"\t")]
            projection=("\n".join(selected)+"\n").encode("ascii")
            (run/"selected.txt").write_bytes(projection)
            for tag,name in (("OWN_ENTROPY","own-entropy.jsonl"),("SOURCE_EXCEPTION","source-exceptions.jsonl")):
                (run/name).write_bytes(("\n".join(line for line in lines if line.startswith(tag+"\t"))+"\n").encode("ascii"))
            item["selected_sha256"]=C.digest(run/"selected.txt")
            item["rows"]=len(rows); item["source_exceptions"]=sum(r["exception"]!="NONE" for r in rows)
            item["passed"]=(captured["exit"]==0 and not captured["timed_out"] and not captured["output_overflow"] and captured["pipe_capture_complete"] and not captured["reader_errors"]
                and [{"name":r["name"],"values":r["values"]} for r in rows]==expected
                and len([line for line in lines if line.startswith("GUARD\t")])==2
                and [(r["index"],r["name"]) for r in own]==[(i,r["name"]) for i,r in enumerate(expected)]
                    and all(r["world_before"]==r["world_after"] and r["world_random_same"] and not r["unexpected_entities"] for r in own)
                and all(r["unexpected_adds"]==0 for r in rows)
                and selected[-1:]==["COMPLETE\t"+cohort+"\t"+str(len(expected))]
                and {k:v for k,v in world_before.items() if k!="session.lock"}=={k:v for k,v in item["world_after"].items() if k!="session.lock"})
            projections[(cohort,repeat)]=projection
        C.ensure(all(r["passed"] for r in report["runs"]) and all(projections[(c,0)]==projections[(c,1)] for c in ("use","support")),"complete two-repeat logical cohort comparisons; all fixed outcomes retained")
        report["status"]="PASS"
    finally:
        report["inputs_after"]={p:C.digest(Path(p)) for p in before}
        report["helpers_after"]=inventory(classes)
        report["inputs_unchanged"]=report["inputs_after"]==before
        report["helpers_unchanged"]=report.get("helpers_before")==report["helpers_after"]
        if not report["inputs_unchanged"] or not report["helpers_unchanged"]:
            report["status"]="FAIL"
        write(output/"capture.json",report)
    C.ensure(report["status"]=="PASS","complete original capture with input/product continuity")
    print("PASS original42use+12support x2ordinaryVMs;",output/"capture.json")


if __name__=="__main__":
    main()
