"""Admit fresh original DB observations; unpublished Entity state stays unqualified."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True

HERE=Path(__file__).resolve().parent
MANIFEST_SHA256 = '43eec19c874adc2fc89382b4d2e070d5eb0d0f6175b07ac52e719be97bfc2ea8'
JAR="af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
COHORTS=("lifecycle","powered","burnout","aging")
COUNTS=(70,32,78,23)
HELPERS={"RedstoneTorchObservation.class","RedstoneTorchObservation$1.class","RedstoneTorchObservation$Observer.class"}
FLAGS=["-Xint","-Xverify:all","-Xmx256m","-Djava.awt.headless=true","-Djava.security.manager=allow",
       "--add-opens","java.base/java.util=ALL-UNNAMED","--add-opens","java.base/java.lang=ALL-UNNAMED"]


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
    return result


O=module("torch_packet_observation",HERE/"observation.py")
W=module("torch_packet_worlds",HERE/"world_inputs.py")
T=module("torch_packet_topology",HERE/"topology.py")
loads=O.loads
same=O.same
need=O.require


def load(path):return loads(Path(path).read_text(encoding="utf-8"))
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unbound_packet(raw):
    matches=re.findall(rb"(?m)^MANIFEST_SHA256 = '(UNFROZEN|[0-9a-f]{64})'$",raw)
    need(len(matches)==1,"single declared publication binding")
    return re.sub(rb"(?m)^MANIFEST_SHA256 = '(?:UNFROZEN|[0-9a-f]{64})'$",
                  b"MANIFEST_SHA256 = 'UNFROZEN'",raw,count=1)


def declared_inputs(directory=HERE):
    directory=Path(directory)
    value=load(directory/"declared-inputs.json")
    need(tuple(value["cohorts"])==COHORTS and tuple(value["counts"][c] for c in COHORTS)==COUNTS,
         "exact four input cohorts")
    need(value["columns"]=="world entry receiver x y z metadata center_id layout remote null_world random_mode record neighbor_id observer_mode".split(),
         "exact input columns")
    for cohort,count in zip(COHORTS,COUNTS):
        data=(directory/(cohort+"-profiles.tsv")).read_bytes()
        need(data.endswith(b"\n") and b"\r" not in data,"exact LF profile bytes")
        rows=[]
        for line in data.decode("ascii").splitlines():
            cells=line.split("\t")
            need(len(cells)==16 and re.fullmatch("[a-z0-9-]+",cells[0]),"profile fields/name")
            need(all(re.fullmatch("0|-?[1-9][0-9]*",x) for x in cells[1:]),"canonical input integers")
            values=list(map(int,cells[1:]))
            need(all(-2**31<=x<2**31 for x in values),"signed input width")
            rows.append(dict(name=cells[0],values=values))
        need(len(rows)==count and len({r["name"] for r in rows})==count and
             same(rows,value["cohorts"][cohort]),"frozen unique input order")
    need(same(value["world_roles"],W.audit(directory)),"exact synthetic World metadata")
    return value


def validate_inputs(directory=HERE):
    directory=Path(directory)
    frozen=load(directory/"input-freeze.json")
    need(frozen["format"]==1 and frozen["client_jar_sha256"]==JAR,"authored closure format")
    execution=set(".gitattributes RedstoneTorchObservation.java capture_reference.py observation.py accounting.py entropy.py rng.py witness.py topology.py declared-inputs.json protocol.json worlds.tsv source-pins.json helper-source-pins.json metadata-probe-pins.json jdk-accounting-pins.json jdk-methods.json constructor-proof.json world-inputs.json".split())
    execution.update(c+"-profiles.tsv" for c in COHORTS)
    execution.update("world-inputs/"+name+"/"+file for name,_ in W.ROLES
                     for file in ("level.dat","session.lock","region/r.0.0.mcr"))
    need(type(frozen["files"]) is dict and set(frozen["files"])==execution,
         "closed immutable execution inputs; packet/tests/publication metadata are separate")
    for name,pin in frozen["files"].items():
        need(type(name) is str and not Path(name).is_absolute() and ".." not in Path(name).parts,
             "safe relative authored identity")
        data=(directory/name).read_bytes()
        need(hashlib.sha256(data).hexdigest()==pin,"changed frozen input "+name)
    need(sha(directory/"RedstoneTorchObservation.java")==
         "88cd0df874ba4e6c447e8ca7b94e6770df19c70d9087827d92c86fa6c044d66a",
         "executed helper bytes remain unchanged")
    pins=load(directory/"source-pins.json")
    need(pins["jar_sha256"]==JAR,"original archive identity")
    methods={("c","(Lfd;III)V"):(112,"8f5ae147df201e8c05268e92dcfb6a9e2c111d3fecd8b0a35a2dc6ce36fbbdde"),
             ("b","(Lfd;III)V"):(92,"ebaf9235e464eca1caf5707e3067cf072e8f628721589c5e1364c3c4dd5de592"),
             ("b","(Lfd;IIII)V"):(28,"4767d715b7783bc40af8da981838eb942f68bb6f19bb956657858155f8380865"),
             ("a","(Lfd;IIILjava/util/Random;)V"):(288,"c522fb59072270b0d8acd21d8d41d14fb0d117638495bfc4c883b61a0a6365c4"),
             ("h","(Lfd;III)Z"):(122,"df91b36d3e1df6227f1a1ac9aa27197f42213e1a36aa376df22c611e8be54c16"),
             ("a","(Lfd;IIIZ)Z"):(111,"2818fe443d6b82337d0204d84c3ed138eea25987d854ecba804434e7dd2272a3")}
    for key,expected in methods.items():
        selected=[m for m in pins["classes"]["db"]["methods"] if (m["name"],m["descriptor"])==key]
        need(len(selected)==1 and (selected[0]["length"],selected[0]["sha256"])==expected,
             "exact DB source method metadata")
    return declared_inputs(directory)


def process(receipt,stdout,stderr,name,timeout):
    fields=set("command exit timeout_seconds timed_out pipe_capture_complete reader_errors output_overflow stdout_file stderr_file stdout_bytes stderr_bytes stdout_sha256 stderr_sha256 stdout_limit stderr_limit".split())
    need(type(receipt) is dict and set(receipt)==fields and type(receipt["command"]) is list and
         all(type(x) is str for x in receipt["command"]),"closed process receipt")
    need(type(receipt) is dict and receipt["exit"]==0 and type(receipt["exit"]) is int and
         receipt["timed_out"] is False and receipt["output_overflow"] is False and
         receipt["pipe_capture_complete"] is True and receipt["reader_errors"]=={},
         "complete successful bounded process")
    need(type(receipt["timeout_seconds"]) is int and receipt["timeout_seconds"]==timeout and
         type(receipt["stdout_limit"]) is int and type(receipt["stderr_limit"]) is int and
         receipt["stdout_limit"]==4194304 and receipt["stderr_limit"]==1048576,"exact original bounds")
    for channel,data in (("stdout",stdout),("stderr",stderr)):
        need(type(receipt[channel+"_bytes"]) is int and receipt[channel+"_file"]==name+"."+channel and
             receipt[channel+"_bytes"]==len(data) and receipt[channel+"_sha256"]==hashlib.sha256(data).hexdigest(),
             "actual raw receipt join")


def join(root,name):
    separator="\\" if re.match(r"^[A-Za-z]:\\",root) else "/"
    return root.rstrip("/\\")+separator+name.replace("/",separator)


def method_metadata(observed,method):
    fields={"name","descriptor","access"}
    if "length" in method:fields|={"code_bytes","code_sha256"}
    need(type(observed) is dict and set(observed)==fields,"closed actual JDK method metadata")
    need(observed["name"]==method["name"] and observed["descriptor"]==method["descriptor"] and
         type(observed["access"]) is int and observed["access"]==method["flags"],"actual JDK method declaration")
    if "length" in method:
        need(type(observed.get("code_bytes")) is int and observed.get("code_bytes")==method["length"] and
             observed.get("code_sha256")==method["sha256"],"actual JDK Code metadata")
    else:
        need(observed["access"] & 0x0100 and "code_bytes" not in observed and
             "code_sha256" not in observed,"actual native JDK declaration has no Code")


def admit_capture(capture_directory,prepared_directory=HERE):
    """Unbound evidence layer for a newly completed capture and corruption controls."""
    capture_directory,prepared_directory=Path(capture_directory),Path(prepared_directory)
    declaration=validate_inputs(prepared_directory)
    report=load(capture_directory/"capture.json");freeze=load(capture_directory/"freeze.json")
    need(set(freeze)==set("inputs_before input_freeze_sha256 jdk_code_pins_sha256 compile_argv ordinary_argv mutable_worlds_before removed_environment budget ordinary_flags".split()),
         "closed pre-execution freeze")
    need(freeze["input_freeze_sha256"]==sha(prepared_directory/"input-freeze.json") and
         same(freeze["ordinary_flags"],FLAGS) and
         same(freeze["budget"],dict(compile=30,ordinary=90,stdout=4194304,stderr=1048576)) and
         type(freeze["removed_environment"]) is list and
         len(set(freeze["removed_environment"]))==len(freeze["removed_environment"]) and
         set(freeze["removed_environment"])<={"JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS"},
         "frozen inputs, flags and unexpanded bounds")
    need(report["status"]=="PASS_WITH_UNOBSERVED_ENTITY_FRONTIER" and
         report["full_entropy_qualification"] is False and report["public_fresh_capture"] is True and
         report["no_game_class_replacements"] is True and report["prepared_relative"]=="tests/reference/world/redstone_torch",
         "fresh public source scope; never relabel a private predecessor")
    need(report["inputs_unchanged"] is True and report["helpers_unchanged"] is True and
         same(report["inputs_before"],report["inputs_after"]) and
         same(report["inputs_before"],freeze["inputs_before"]),"actual source/tool/input continuity")
    need(set(report["helpers_before"])==HELPERS and same(report["helpers_before"],report["helpers_after"]) and
         all(re.fullmatch("[0-9a-f]{64}",x) for x in report["helpers_before"].values()),"only unchanged three authored classes")
    repo,prepared,capture=report["repository_root"],report["prepared_root"],report["capture_root"]
    need(prepared==join(repo,"tests/reference/world/redstone_torch"),"actual public source path")
    input_paths=report["inputs_before"]
    need(input_paths[join(repo,"b1.7.3.jar")]==JAR,"actual original archive bytes")
    authored=load(prepared_directory/"input-freeze.json")["files"]
    for name in [*authored,"input-freeze.json"]:
        need(input_paths[join(prepared,name)]==sha(prepared_directory/name),"actual frozen execution input binding")
    paths=report["paths"];need(set(paths)=={"java","javac","jvm","jmod","unsupported","modules","libraries","java_dlls"},
                             "closed actual tool roles")
    need(all(type(paths[key]) is str and paths[key] for key in
             ("java","javac","jvm","jmod","unsupported","modules")) and
         all(type(paths[key]) is list and all(type(p) is str and p for p in paths[key]) and
             len(set(paths[key]))==len(paths[key]) for key in ("libraries","java_dlls")),
         "exact string paths and unique path arrays")
    need(len(paths["libraries"])==4 and report["classpath_separator"] in (":",";"),"pinned SDK role inventory")
    support=("tests/reference/client/controller/capture.py","tests/reference/client/controller/classfile.py",
             "tests/reference/client/picking/observe.py","tests/tools/client/test_picking.py",
             "tests/tools/world/test_source_saving.py")
    support_pins=load(prepared_directory/"input-freeze.json")["support_sha256"]
    need(set(support_pins)==set(support) and all(input_paths[join(repo,n)]==p for n,p in support_pins.items()),
         "unchanged actual capture/class/input producer dependencies")
    expected_inputs={join(repo,"b1.7.3.jar"),*(join(prepared,n) for n in [*authored,"input-freeze.json"]),
        *(join(repo,n) for n in support),paths["java"],paths["javac"],paths["jvm"],paths["jmod"],
        paths["unsupported"],paths["modules"],*paths["libraries"],*paths["java_dlls"]}
    need(set(input_paths)==expected_inputs and all(type(x) is str and re.fullmatch("[0-9a-f]{64}",x)
         for x in input_paths.values()),"complete closed actual input/tool closure")
    sdk=('c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a','474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad','79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018','c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5')
    need(tuple(input_paths[x] for x in paths["libraries"])==sdk,"exact original SDK dependencies")
    code=load(capture_directory/"jdk-code-pins.json")
    need(code["jmod_sha256"]==input_paths[paths["jmod"]] and
         code["unsupported_sha256"]==input_paths[paths["unsupported"]] and
         sha(capture_directory/"jdk-code-pins.json")==freeze["jdk_code_pins_sha256"],
         "actual JDK module/class metadata")
    for source in ("metadata-probe-pins.json","jdk-accounting-pins.json"):
        for name,value in load(prepared_directory/source)["classes"].items():
            need(code["classes"][name]["class_sha256"]==value["class_sha256"],
                 "actual passive metadata/holder source authority")
            for method in value.get("methods",[]):
                observed=[m for m in code["classes"][name]["methods"] if
                          m["name"]==method["name"] and m["descriptor"]==method["descriptor"]]
                need(len(observed)==1,"one actual JDK method")
                method_metadata(observed[0],method)

    cp=report["classpath_separator"].join([join(capture,"classes"),join(repo,"b1.7.3.jar"),*paths["libraries"]])
    for name,timeout in (("java",10),("javac",10),("compile",30)):
        receipt=load(capture_directory/(name+".capture.json"))
        stdout,stderr=[(capture_directory/(name+"."+c)).read_bytes() for c in ("stdout","stderr")]
        need(same(receipt,report[name]),"actual tool report join");process(receipt,stdout,stderr,name,timeout)
        if name=="compile":
            need(receipt["command"]==[paths["javac"],"-encoding","UTF-8","-implicit:none","-cp",cp,
                 "-d",join(capture,"classes"),join(prepared,"RedstoneTorchObservation.java")] and
                 stdout==stderr==b"" and same(freeze["compile_argv"],receipt["command"]),"one unchanged helper compilation")
        else:need(receipt["command"]==[paths[name],"-version"] and b"17.0.17" in stdout+stderr,"actual JDK17.0.17")
    need([(r["cohort"],r["repeat"]) for r in report["runs"]]==[(c,n) for c in COHORTS for n in (0,1)],
         "all eight fixed ordinary processes")
    accounting=module("torch_packet_fresh_accounting",prepared_directory/"accounting.py")
    result={};all_rows={};summaries={}
    for item in report["runs"]:
        cohort,repeat=item["cohort"],item["repeat"];name=cohort+"-ordinary-"+str(repeat)
        directory=capture_directory/name
        raw,stderr=[(directory/("original."+c)).read_bytes() for c in ("stdout","stderr")]
        receipt=load(directory/"original.capture.json")
        need(same(receipt,item["capture"]),"actual process report join");process(receipt,raw,stderr,"original",90)
        need(receipt["command"]==[paths["java"],*FLAGS,"-cp",cp,"RedstoneTorchObservation",join(repo,"b1.7.3.jar"),
             join(capture,name+"/worlds"),join(prepared,cohort+"-profiles.tsv"),cohort],
             "ordinary source invocation without code/clock/stream replacement")
        need(same(item["command"],receipt["command"]) and
             same(freeze["ordinary_argv"][cohort+"-"+str(repeat)],receipt["command"]),
             "actual ordinary argv matches prelaunch freeze")
        roles=list(declaration["world_roles"].values()) if cohort=="aging" else [declaration["world_roles"]["0"]]
        rows,own,exceptions=O.decode(raw,cohort,declaration["cohorts"][cohort],roles)
        selected="".join("DB\t"+json.dumps(row,separators=(",",":"))+"\n" for row in rows).encode("ascii")
        need((directory/"selected.txt").read_bytes()==selected and item["selected_sha256"]==
             hashlib.sha256(selected).hexdigest() and type(item["rows"]) is int and item["rows"]==len(rows),
             "declared canonical LF logical projection retains every literal Source value")
        for filename,values in (("own-entropy.jsonl",own),("source-exceptions.jsonl",exceptions)):
            projected="".join(json.dumps(v,separators=(",",":"))+"\n" for v in values).encode("ascii")
            need((directory/filename).read_bytes()==projected,"complete raw-value projection "+filename)
        need(stderr.decode().splitlines()==[
            "WARNING: A terminally deprecated method in java.lang.System has been called",
            "WARNING: System::setSecurityManager has been called by RedstoneTorchObservation ("+report["helper_directory_url"]+")",
            "WARNING: Please consider reporting this to the maintainers of RedstoneTorchObservation",
            "WARNING: System::setSecurityManager will be removed in a future release"],"closed JDK diagnostic")
        for row in rows:T.validate(row,roles)
        own_summary=accounting.audit(rows,own)
        need(own_summary["inferred_unpublished_HL"]==(4 if cohort=="lifecycle" else 0) and
             own_summary["default_constructor_steps"]==(17 if cohort=="lifecycle" else 0) and
             own_summary["full_entropy_qualification"] is False,"exact bounded accounting and unobserved frontier")
        need(same(own_summary,item["own_entropy"]),"actual fresh accounting summary")
        before,after=item["world_before"],item["world_after"]
        expected={role["name"]+"/"+n:v for role in roles for n,v in role["files"].items()}
        need(same(before,freeze["mutable_worlds_before"][cohort+"-"+str(repeat)]) and
             same(before,expected) and set(before)==set(after) and
             all(type(v) is str and re.fullmatch("[0-9a-f]{64}",v) for v in after.values()) and
             {k:v for k,v in before.items() if not k.endswith("/session.lock")}==
             {k:v for k,v in after.items() if not k.endswith("/session.lock")},"exact authored saved World continuity")
        all_rows[cohort,repeat]=rows;summaries[name]=own_summary
        if repeat==0:result[cohort]=dict(rows=rows,own=own,exceptions=exceptions)
    need(all(same(all_rows[c,0],all_rows[c,1]) for c in COHORTS),"full literal fresh repeat equality")
    need(set(freeze["ordinary_argv"])=={c+"-"+str(n) for c in COHORTS for n in (0,1)} and
         set(freeze["mutable_worlds_before"])==set(freeze["ordinary_argv"]),
         "exactly eight frozen opportunities")
    return dict(status="PASS_WITH_UNOBSERVED_ENTITY_FRONTIER",full_entropy_qualification=False,
                cohorts=result,summaries=summaries)


def audit(directory=HERE):
    directory=Path(directory)
    need(re.fullmatch("[0-9a-f]{64}",MANIFEST_SHA256) is not None,"packet is not sealed after fresh public captures")
    need(sha(directory/"manifest.json")==MANIFEST_SHA256,"sealed packet manifest")
    manifest=load(directory/"manifest.json")
    need(set(manifest)=={"format","client_jar_sha256","packet_body_sha256","files_sha256"} and manifest["format"]==1 and
         manifest["client_jar_sha256"]==JAR,"closed manifest authority")
    need(hashlib.sha256(unbound_packet((directory/"packet.py").read_bytes())).hexdigest()==
         manifest["packet_body_sha256"],"sealed parser body with only the declared manifest literal unbound")
    expected=set(manifest["files_sha256"])|{"packet.py","manifest.json"}
    actual={p.relative_to(directory).as_posix() for p in directory.rglob("*") if p.is_file() and
            "__pycache__" not in p.parts}
    need(actual==expected,"closed packet, no classes/JAR/JDK/assets")
    for name,pin in manifest["files_sha256"].items():
        need(not Path(name).is_absolute() and ".." not in Path(name).parts and
             re.fullmatch("[0-9a-f]{64}",pin) and sha(directory/name)==pin,"changed sealed artifact "+name)
    return admit_capture(directory,directory)


def reference_cohorts(directory=HERE):return audit(directory)["cohorts"]
def read_packet(directory=HERE):return reference_cohorts(directory)
