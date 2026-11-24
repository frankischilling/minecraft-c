"""Replay qualified retained original direct MovingPiston geometry without a new JVM.

The complete original family is retained and reverified, including its explicit
post-observation bookkeeping failure. Native GAME owners bind the actual pool;
only source-unused-World paths are compared. This is not installed Tile parity.
All successful 23-field payload bytes compare exactly; actual body failures
compare their full effect prefix plus explicit native status/PC diagnostics.
Only uniform LF/CRLF record terminators may differ between host streams.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import itertools
import json
import os
from pathlib import Path, PureWindowsPath
import random
import re
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
DEFAULT_REPO = HERE.parents[3]

FROZEN_ORIGINAL_REPORT = (15969, "51acf521306b0486fa4b10eb683c9be86527e94727e3e4ce43fc0d7a50f66359")

JARS = {
    "CLIENT": ("b1.7.3.jar",1465375,"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"),
    "SERVER": ("b1.7.3_server.jar",503100,"033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"),
}

SPECIAL = (
    "00000000","80000000","00000001","80000001","007fffff","807fffff",
    "00800000","80800000","3f000000","bf000000","3f800000","bf800000",
    "3f800001","3f7fffff","7f7fffff","ff7fffff","7f800000","ff800000",
    "7fc00001","7fc00042","ffc00001","ffc00042",
    "7f800001","7f800042","ff800001","ff800042",
)

NULL_GETTERS = {6,8,9,10,11,27,28,30,31,32,37,38,39,40,50,51,55,59,63,66,68,69,70,72,75,76,77,83,90}

WORLD_GETTERS = {64,65,71,78,92,96}

CARRIERS = (1,44,85,81,88,60,53)

COORDINATES = ((0,0,0),(7,-9,13),(-1,255,-1),(16777217,-16777217,16777217),
               (2147483647,)*3,(-2147483648,)*3,(2147483647,-2147483648,-1))

DIRECTION = "a=d0:1,0,3,2,5,4;b=d1:0,0,0,0,-1,1;c=d2:-1,1,0,0,0,0;d=d3:0,0,-1,1,0,0"

def pin(path):
    path = Path(path).resolve()
    data = path.read_bytes()
    return {"path":str(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()}

def write(path,value):
    path.write_text(json.dumps(value,sort_keys=True,indent=2)+"\n",encoding="utf-8")

def corpus():
    rows = []
    def add(label,carried=1,*,receiver="moving",xyz=(0,0,0),bounds=None,
            progress="3e800000",facing=0,pool="reset",repeats=1,category="shifted-positive"):
        if bounds is None:
            bounds = "ctor" if 1<=carried<=96 else "-"
        row = {"id":f"g{len(rows):06d}","label":label,"receiver":receiver,
            "carried":carried,"xyz":list(xyz),"bounds":bounds,"progress":progress,
            "facing":facing,"pool":pool,"repeats":repeats,"category":category}
        row["line"] = "\t".join(map(str,[row["id"],receiver,carried,*xyz,bounds,progress,facing,pool,repeats]))
        rows.append(row)
    for carried,progress,facing in itertools.product((0,36),("7f800042","7f800000"),(-1,6,-2147483648,2147483647)):
        add("early-zero-or-self-before-Direction",carried,progress=progress,facing=facing,category="early-null")
    for carried in (0,36,1):
        add("reflection-null-receiver-admission",carried,receiver="null",progress="7f800042",facing=2147483647,
            category="reflection-admission")
    for carried in (-2147483648,-1,256,2147483647,97,255):
        add("registered-array-index-or-null-receiver-before-Direction",carried,
            progress="7f800042",facing=-1,category="carrier-failure")
    for carried in sorted(NULL_GETTERS):
        for facing in (-1,2147483647):
            add("literal-null-carrier-before-Direction",carried,progress="7f800042",facing=facing,
                category="literal-null")
    for carried in sorted(WORLD_GETTERS):
        for facing in (-1,5):
            add("actual-null-World-carrier-failure",carried,progress="7f800042",facing=facing,
                category="null-World-exclusion")
    # This is the first genuine route that reaches Direction getstatic b45.
    add("first-reached-Direction-invalid-face-after-pooling",1,progress="7f800042",facing=-1,
        category="first-Direction-reach")
    for carried in range(1,97):
        add("all-registered-constructor-carriers",carried,category="registry-inventory")
    for carried,xyz,facing,progress in itertools.product(CARRIERS,COORDINATES,range(6),SPECIAL):
        add("seven-carriers-seven-coordinates-six-faces-26-progress",carried,xyz=xyz,facing=facing,
            progress=progress,category="raw-progress-matrix")
    generator = random.Random(0x1736E0A1)
    for index in range(256):
        value = generator.getrandbits(32)
        while value&0x7f800000 == 0x7f800000:
            value = generator.getrandbits(32)
        add("seeded-finite-raw-progress",generator.choice(CARRIERS),xyz=generator.choice(COORDINATES),
            facing=generator.randrange(6),progress=f"{value:08x}",category="finite-random")
    for index,bound in enumerate(SPECIAL[-8:]):
        other = SPECIAL[-8:][(index+3)%8]
        for progress,facing,form in itertools.product(SPECIAL,range(6),("uniform","split")):
            raw = [bound]*6 if form=="uniform" else [bound,other,bound,other,bound,other]
            add("actual-float-setter-two-NaN-context-"+form,1,bounds=",".join(raw),progress=progress,
                facing=facing,category="setter-NaN-matrix")
    for index in range(48):
        raw = [generator.choice(SPECIAL) for _ in range(6)]
        for facing in range(6):
            add("actual-float-setter-mixed-edges",1,bounds=",".join(raw),progress=generator.choice(SPECIAL),
                xyz=generator.choice(COORDINATES),facing=facing,category="setter-mixed")
    for carried,mode,progress,facing in itertools.product(CARRIERS,("alias","used","between"),
            ("3e800000","7f800000","7f800042"),(0,5)):
        add("actual-pool-reuse-and-live-alias",carried,progress=progress,facing=facing,pool=mode,repeats=2,
            category="pool-identity")
    for carried,facing in itertools.product((0,36,1,6,64,97),(-2147483648,-1,6,2147483647)):
        add("reached-order-invalid-facing",carried,facing=facing,pool="alias",category="failure-order")
    return rows

def records(data):
    if not data.endswith(b"\n") or b"\r" in data.replace(b"\r\n",b""):
        raise ValueError("incomplete record or bare CR")
    separator = b"\r\n" if b"\r\n" in data else b"\n"
    if separator == b"\r\n" and b"\n" in data.replace(separator,b""):
        raise ValueError("mixed LF/CRLF framing")
    return data[:-len(separator)].split(separator),"CRLF" if len(separator)==2 else "LF"

def expected(case):
    carried = case["carried"]
    if case["receiver"] == "null":
        return "java.lang.NullPointerException","reflection-null-receiver-before-body","reflection admission; no original method body"
    if carried in (0,36):
        return "OK","none","source early null before carrier and Direction"
    if not 0<=carried<256:
        return "java.lang.ArrayIndexOutOfBoundsException","original-body","source carrier array index21 before Direction"
    if carried>=97:
        return "java.lang.NullPointerException","original-body","actual null registered carrier at virtual getter27"
    if carried in NULL_GETTERS:
        return "OK","none","actual literal-null getter; returns38 before Direction"
    if carried in WORLD_GETTERS:
        return "java.lang.NullPointerException","original-body","actual null-World getter failure; exclude nonnull-World behavior"
    if not 0<=case["facing"]<6:
        return "java.lang.ArrayIndexOutOfBoundsException","original-body","reached Direction iaload50 after actual pooled carrier"
    return "OK","none","source-unused World; actual direct shifted helper only"

def validate(case,fields,first_reach):
    if len(fields)!=23 or fields[0]!=case["id"]:
        return False,"row identity/field count"
    outcome,origin,_ = expected(case)
    if fields[1:4] != [outcome,origin,case["progress"]]:
        return False,"actual outcome/origin/raw progress"
    try:
        before,after,size_before,size_after = map(int,fields[4:8])
        if not 0<=before<=size_before<=size_after or not 0<=after<=size_after:
            return False,"actual pool bounds"
        for field in (9,10,11,13,17):
            if fields[field]!="-" and not re.fullmatch(r"[0-9a-f]{16}(?:,[0-9a-f]{16}){5}",fields[field]):
                return False,"raw binary64 coordinates"
        for index in (8,12,16):
            value = fields[index]
            if value!="-" and not (re.fullmatch(r"p[0-9]+",value) and int(value[1:])<size_after):
                return False,"actual returned identity"
        if fields[18] != (DIRECTION if int(case["id"][1:])>=first_reach else "unobserved"):
            return False,"actual reached Direction values/identities"
        if case["bounds"] not in ("-","keep","ctor") and fields[19]!=case["bounds"]:
            return False,"actual boxed setter inputs"
        if case["bounds"] in ("-","keep") and fields[19]!=case["bounds"]:
            return False,"unexpected setter control"
        if case["bounds"]=="ctor" and not re.fullmatch(r"[0-9a-f]{8}(?:,[0-9a-f]{8}){5}",fields[19]):
            return False,"actual constructor float setter reread"
        if fields[20]!=str(case["repeats"] if outcome=="OK" else 0):
            return False,"actual completed call count"
        if fields[21:] != ["-" if case["receiver"]=="null" else "b36","g0,stable"]:
            return False,"actual receiver/pool identity"
        returns_null = case["carried"] in ({0,36}|NULL_GETTERS)
        positive = outcome=="OK" and not returns_null
        if positive != (fields[8]!="-" and fields[9]!="-"):
            return False,"actual positive/null outcome"
        if outcome!="OK" or returns_null:
            if fields[8:10]!=["-","-"] or fields[16:18]!=["-","-"]:
                return False,"failed/null result invented"
        if positive:
            expected_cursor = 1 if case["pool"]=="between" else before+case["repeats"]
            if after!=expected_cursor or fields[8]!=f"p{after-1}":
                return False,"actual pooled result/cursor"
        if case["pool"]=="alias" and positive and case["repeats"]==1:
            if fields[8:10]!=fields[12:14]:
                return False,"actual live anchor alias"
    except (ValueError,IndexError):
        return False,"malformed actual fields"
    return True,None


def verify_original(original,repo,retained):
    """Rejoin full raw evidence directly, including the retained outer FAIL."""
    original_report = pin(original/"report.json")
    if (original_report["bytes"],original_report["sha256"]) != FROZEN_ORIGINAL_REPORT:
        raise RuntimeError("unsupported or changed frozen original geometry report")
    receipt = json.loads((original/"report.json").read_text("utf-8"))
    if receipt["status"]!="FAIL" or receipt["errors"]!=["TypeError: 'WindowsPath' object is not subscriptable"]:
        raise RuntimeError("expected retained post-JAR-pin bookkeeping defect changed")
    if not receipt["paired_rows_byte_identical"] or "jars_after" in receipt:
        raise RuntimeError("incomplete/different retained capture state")
    recorded_root = receipt["processes"][0]["cwd"]
    windows = bool(PureWindowsPath(recorded_root).drive)
    path_type = PureWindowsPath if windows else Path
    recorded_output = str(path_type(receipt["input_before"]["path"]).parent)
    def resolve(text):
        selected = path_type(text)
        for before,after in ((recorded_output,original),(recorded_root,repo)):
            try:
                return after.joinpath(*selected.relative_to(path_type(before)).parts)
            except ValueError:
                pass
        if windows and os.name!="nt":
            return Path("/mnt")/selected.drive[0].lower()/Path(*selected.parts[1:])
        return Path(text)
    def verify(item):
        actual = pin(resolve(item["path"]))
        if any(actual[key]!=item[key] for key in ("bytes","sha256")):
            raise RuntimeError("original evidence changed: "+item["path"])
        retained.append(actual)
        return actual
    retained.append(original_report)
    for key in ("producer","helper","java","javac","class","input"):
        if not receipt[key+"_stable"] or receipt[key+"_before"]!=receipt[key+"_after"]:
            raise RuntimeError("original input instability: "+key)
        verify(receipt[key+"_before"])
    if receipt["producer_before"]["sha256"]!="eb7ebb357fd9e3aa4c420125e6b81b30e78f2452c0a135a805a634fcce600e86" or \
            receipt["helper_before"]["sha256"]!="65dbc9984e88328205b6c19ad506f93a155c78c659c4b028ecbc16725462dfd2":
        raise RuntimeError("original producer/helper identity mismatch")
    if 'report["jars_after"] = {side:pin(ROOT[value[0]]) for side,value in JARS.items()}' not in \
            resolve(receipt["producer_before"]["path"]).read_text("utf-8"):
        raise RuntimeError("retained bookkeeping defect does not match source")
    for side,(filename,length,digest) in JARS.items():
        actual = verify(receipt["jars_before"][side])
        current = pin(repo/filename)
        if (current["bytes"],current["sha256"])!=(length,digest) or \
                any(current[key]!=actual[key] for key in ("bytes","sha256")):
            raise RuntimeError("direct AFTER JAR pin mismatch")
        retained.append(current)
    for item in receipt["authority"]+[receipt["source_plan"],receipt["cases"]]+receipt["frozen_source_copies"]:
        verify(item)
    for name,key in (("capture.py","producer"),("PistonGeometryObservation.java","helper")):
        if (original/"sources"/name).read_bytes()!=resolve(receipt[key+"_before"]["path"]).read_bytes():
            raise RuntimeError("frozen source copy mismatch")
    if sorted(p.name for p in (original/"classes").iterdir())!=["PistonGeometryObservation.class"] or \
            pin(original/"classes/PistonGeometryObservation.class")["sha256"]!=receipt["class_before"]["sha256"]:
        raise RuntimeError("unexpected compiled engine/helper class inventory")
    cases = corpus()
    if len(cases)!=11026 or cases!=json.loads((original/"cases.json").read_text("utf-8")):
        raise RuntimeError("full original corpus regeneration mismatch")
    generated_input = ("\n".join(case["line"] for case in cases)+"\n").encode("ascii")
    if (original/"input.tsv").read_bytes()!=generated_input or \
            hashlib.sha256(generated_input).hexdigest()!="6721a455f1cef939a3c4e075c6a9e6aab08a0fe6ed4ce3b459862fbe7eff8d8b":
        raise RuntimeError("full original input mismatch")
    java,javac = receipt["java_before"]["path"],receipt["javac_before"]["path"]
    classes_path = str(path_type(receipt["class_before"]["path"]).parent)
    commands = {"java-version":[java,"-version"],"javac-version":[javac,"-version"],
        "compile-helper":[javac,"-encoding","UTF-8","-d",classes_path,receipt["helper_before"]["path"]]}
    for side in JARS:
        commands[side.lower()+"-original"] = [java,"-Xint","-Dfile.encoding=US-ASCII","-cp",
            classes_path,"PistonGeometryObservation",receipt["jars_before"][side]["path"],side,
            str(path_type(recorded_output)/(side.lower()+".metadata.tsv"))]
    if [process["label"] for process in receipt["processes"]]!=list(commands):
        raise RuntimeError("original process inventory mismatch")
    for process in receipt["processes"]:
        label = process["label"]
        if process["command"]!=commands[label] or process["cwd"]!=recorded_root or \
                process["returncode"]!=0 or not process["reaped"] or process["terminated"] or \
                process["end_ns"]<process["start_ns"] or \
                process["stdin"]!=(receipt["input_before"] if label.endswith("-original") else None):
            raise RuntimeError("original command/stdin/natural outcome mismatch")
        for suffix in ("command","live","result"):
            path = original/(label+"."+suffix+".json")
            document = json.loads(path.read_text("utf-8"))
            if any(process.get(key)!=value for key,value in document.items()) or \
                    suffix=="result" and document!=process:
                raise RuntimeError("original process receipt join mismatch")
            retained.append(pin(path))
        verify(process["stdout"])
        verify(process["stderr"])
    references = {}
    for side in JARS:
        details = receipt["sides"][side]
        if details["status"]!="PASS":
            raise RuntimeError("original side validation did not pass")
        for key in ("rows_capture","results","metadata","bootstrap"):
            verify(details[key])
        payloads,ending = records((original/(side.lower()+"-original.stdout")).read_bytes())
        if payloads[:3]!=[b"16 achievements",b"151 recipes",("PISTON_GEOMETRY_V1\t"+side).encode("ascii")]:
            raise RuntimeError("actual original Registry bootstrap/header mismatch")
        separator = b"\r\n" if ending=="CRLF" else b"\n"
        if separator.join(payloads[:2])+separator!=(original/(side.lower()+".bootstrap.stdout")).read_bytes() or \
                separator.join(payloads[3:])+separator!=(original/(side.lower()+".rows.stdout")).read_bytes() or \
                len(payloads[3:])!=11026:
            raise RuntimeError("original raw stream position/record join mismatch")
        inventory,_ = records((original/(side.lower()+".metadata.tsv")).read_bytes())
        if len(inventory)!=99 or inventory[0]!=b"PISTON_GEOMETRY_REGISTRY_V1" or \
                inventory[-2]!=b"POOL\tg0\tjava.util.ArrayList\t0\t0" or \
                inventory[-1]!=b"DIRECTION\tno-static-value-read-before-source-reach":
            raise RuntimeError("actual original registry/pool/Direction setup mismatch")
        for id,line in enumerate(inventory[1:97],1):
            f = line.decode("ascii").split("\t")
            if len(f)!=4 or f[0]!=str(id) or not re.fullmatch(r"[0-9a-f]{16}(?:,[0-9a-f]{16}){5}",f[3]):
                raise RuntimeError("actual original registered Block inventory mismatch")
        results = json.loads((original/(side.lower()+".results.json")).read_text("utf-8"))
        if len(results)!=11026:
            raise RuntimeError("original case result count mismatch")
        failures = []
        for case,payload,result in zip(cases,payloads[3:],results):
            line = payload.decode("ascii")
            fields = line.split("\t")
            valid,reason = validate(case,fields,95)
            if not valid or result!={"id":case["id"],"line":line,"valid":True,"failure":None,
                    "classification":expected(case)[2]}:
                raise RuntimeError("original row/result validation mismatch: "+case["id"]+" "+str(reason))
            if fields[1]!="OK":
                failures.append((case,fields))
        trace_records,_ = records((original/(side.lower()+"-original.stderr")).read_bytes())
        if len(trace_records)!=len(failures) or len(failures)!=40:
            raise RuntimeError("original exception trace inventory mismatch")
        traces = {}
        for detail,(case,fields) in zip(trace_records,failures):
            parts = detail.decode("ascii").split("\t")
            if len(parts)!=4 or parts[:3]!=["EXCEPTION",case["id"],fields[2]]:
                raise RuntimeError("original exception trace identity mismatch")
            trace = bytes.fromhex(parts[3]).decode("utf-8")
            if not trace.startswith(fields[1]) or "\tat " not in trace or \
                    "PistonGeometryObservation.row" not in trace or \
                    fields[2]=="original-body" and ("ut.a(" if side=="CLIENT" else "mz.a(") not in trace:
                raise RuntimeError("original actual exception trace mismatch")
            traces[case["id"]] = trace
        references[side] = {"payloads":payloads[3:],"ending":ending,"separator":separator,"traces":traces}
    if references["CLIENT"]["payloads"]!=references["SERVER"]["payloads"] or \
            (original/"client.rows.stdout").read_bytes()!=(original/"server.rows.stdout").read_bytes():
        raise RuntimeError("paired original raw row mismatch")
    return receipt,cases,references


def execute(output,label,command,cwd,input_path):
    record = {"label":label,"command":command,"cwd":str(cwd),"stdin":pin(input_path),
        "executable_before":pin(command[0]),"start_ns":time.time_ns()}
    write(output/(label+".command.json"),record)
    child = None
    try:
        with input_path.open("rb") as source,(output/(label+".stdout")).open("xb") as stdout, \
                (output/(label+".stderr")).open("xb") as stderr:
            child = subprocess.Popen(command,cwd=cwd,stdin=source,stdout=stdout,stderr=stderr)
            record["pid"] = child.pid
            try:
                write(output/(label+".live.json"),record)
            finally:
                # Once launched, retain streams until natural completion,
                # including failed live-receipt writes and Ctrl-C.
                while True:
                    try:
                        record["returncode"] = child.wait()
                        break
                    except KeyboardInterrupt:
                        record["interrupted"] = True
                record["reaped"],record["terminated"] = True,False
    except OSError as error:
        record["launch_error" if child is None else "receipt_error"] = str(error)
        if child is None:
            record["returncode"] = None
    record["end_ns"] = time.time_ns()
    record["executable_after"] = pin(command[0])
    record["stdout"],record["stderr"] = pin(output/(label+".stdout")),pin(output/(label+".stderr"))
    write(output/(label+".result.json"),record)
    print(label,record["returncode"],flush=True)
    return record


def source_snapshot(repo):
    paths = {repo/"CMakeLists.txt",repo/".clang-format"}
    for directory in ("src","include","tests"):
        paths.update(path for path in (repo/directory).rglob("*") if path.is_file() and
                     path.suffix in (".c",".h",".py",".java"))
    return [pin(path) for path in sorted(paths)]


def native_prefix(case,side):
    carried = case["carried"]
    if carried<0 or carried>=256:
        pc,message = 21,"Block runtime captured m index"
    elif carried>=97:
        pc,message = 27,"-"
    else:
        pc,message = 50,"Direction captured index"
    return "\t".join(["PREFIX",case["id"],"5",str(pc),"ut" if side=="CLIENT" else "mz",str(pc),
        "0" if pc==27 else "6",message,"Piston carried Block receiver" if pc==27 else "Moving-piston geometry Registry"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo",type=Path,default=DEFAULT_REPO)
    parser.add_argument("--original",type=Path,required=True,help="retained complete original execution directory; never rerun")
    parser.add_argument("--probe",type=Path,required=True,help="separately built/qualified native executable")
    parser.add_argument("--output",type=Path,required=True,help="new output directory; failures remain retained")
    args = parser.parse_args()
    repo,original,native,output = (getattr(args,key).resolve() for key in ("repo","original","probe","output"))
    if output.exists():
        parser.error("output must be fresh")
    output.mkdir(parents=True,exist_ok=False)
    retained = []
    report = {"status":"FAIL","scope":"retained original direct shifted helper/native GAME owner replay",
        "producer_before":pin(__file__),"python_before":pin(sys.executable),"native_before":pin(native),
        "processes":[],"sides":{},"errors":[],"limitations":[
            "no new original family; complete prior original FAIL receipt/streams remain unchanged",
            "original World is unused/null in selected bodies; native GAME World binds the actual pooled owner",
            "not installed Piston Tile collision, nonnull-World metadata behavior, ticking or whole game acceptance",
            "all successful23fields exact;15failures compare effect fields plus native status/PC, no fabricated Java exception",
            "only LF/CRLF record terminators may differ; no field/float/identity normalization",
            "binary source/build qualification is external; this tool pins selected executable and current source inputs"]}
    try:
        report["source_inputs_before"] = source_snapshot(repo)
        receipt,cases,references = verify_original(original,repo,retained)
        report["original_capture"] = pin(original/"report.json")
        report["retained_capture_defect"] = {"original_outer_exit_code":1,"all_five_children_natural0":True,
            "expression":"pin(ROOT[value[0]])","effect":"AFTER JAR pin bookkeeping only; independently checked here"}
        for side in JARS:
            selected,excluded,expected_rows = [],[],[]
            for case,payload in zip(cases,references[side]["payloads"]):
                if case["receiver"]=="null" or case["carried"] in WORLD_GETTERS:
                    fields = payload.decode("ascii").split("\t")
                    if fields[1]!="java.lang.NullPointerException" or case["id"] not in references[side]["traces"]:
                        raise RuntimeError("exclusion is not an actual observed failure")
                    excluded.append({"id":case["id"],"category":case["category"],"carried":case["carried"],
                        "classification":expected(case)[2],"actual_payload":payload.decode("ascii"),
                        "actual_trace":references[side]["traces"][case["id"]]})
                else:
                    selected.append(case)
                    expected_rows.append(payload)
            if len(selected)!=11001 or len(excluded)!=25 or sum(row["receiver"]=="null" for row in cases)!=3 or \
                    sum(row["carried"] in WORLD_GETTERS for row in cases)!=22:
                raise RuntimeError("precise selected/excluded scope mismatch")
            failures = [(case,payload) for case,payload in zip(selected,expected_rows) if payload.split(b"\t")[1]!=b"OK"]
            if len(failures)!=15:
                raise RuntimeError("original actual body failure inventory mismatch")
            write(output/(side.lower()+".selected.json"),selected)
            write(output/(side.lower()+".excluded.json"),excluded)
            input_path = output/(side.lower()+".input.tsv")
            input_path.write_bytes(("\n".join(case["line"] for case in selected)+"\n").encode("ascii"))
            if pin(input_path)["sha256"]!="c69e9d41ef016379dfdbb8b8654d8f474dfb98ecc452a4970c541c6e4579b491":
                raise RuntimeError("selected original input join mismatch")
            expected_path = output/(side.lower()+".original-selected.stdout")
            expected_path.write_bytes(references[side]["separator"].join(expected_rows)+references[side]["separator"])
            retained.extend(pin(path) for path in (input_path,expected_path,output/(side.lower()+".selected.json"),output/(side.lower()+".excluded.json")))
            process = execute(output,side.lower()+"-native",[str(native),"piston-geometry",side,
                str(output/(side.lower()+"-world")).encode("utf-8").hex()],repo,input_path)
            report["processes"].append(process)
            if process.get("reaped") is not True or process.get("terminated") is not False or \
                    process.get("returncode") != 0 or any(key in process for key in \
                    ("launch_error","receipt_error","interrupted")):
                raise RuntimeError("native process did not complete naturally: "+side)
            if process["executable_before"]!=report["native_before"] or process["executable_after"]!=report["native_before"]:
                raise RuntimeError("native executable changed across invocations")
            raw = (output/(side.lower()+"-native.stdout")).read_bytes()
            pieces = raw.split(b"\n")
            if len(pieces)<4 or pieces[-1]!=b"":
                raise RuntimeError("incomplete native bootstrap/protocol")
            bootstrap = b"\n".join(pieces[:2])+b"\n"
            protocol = b"\n".join(pieces[2:])
            bootstrap_rows,bootstrap_ending = records(bootstrap)
            payloads,native_ending = records(protocol)
            framing = bootstrap_rows==[b"16 achievements",b"151 recipes"] and \
                payloads[0]==("PISTON_GEOMETRY_V1\t"+side).encode("ascii")
            native_rows = payloads[1:]
            (output/(side.lower()+".native-bootstrap.stdout")).write_bytes(bootstrap)
            (output/(side.lower()+".native-protocol.stdout")).write_bytes(protocol)
            comparisons,mismatches = [],[]
            for position,(case,expected_payload) in enumerate(zip(selected,expected_rows)):
                actual = native_rows[position] if position<len(native_rows) else None
                original_fields = expected_payload.split(b"\t")
                native_fields = actual.split(b"\t") if actual is not None else []
                success = original_fields[1]==b"OK"
                match = actual==expected_payload if success else len(native_fields)==23 and \
                    native_fields[:3]==[original_fields[0],b"NATIVE_FAILURE",b"native-body-prefix"] and \
                    native_fields[3:]==original_fields[3:]
                if not match:
                    mismatches.append(case["id"])
                comparisons.append({"id":case["id"],"scope":"exact23field-success" if success else "native-status-PC+exact-effect-prefix",
                    "match":bool(match),"original_payload":expected_payload.decode("ascii"),
                    "native_payload":actual.decode("ascii") if actual is not None else None})
            write(output/(side.lower()+".results.json"),comparisons)
            stderr_payloads,stderr_ending = records((output/(side.lower()+"-native.stderr")).read_bytes())
            actual_diagnostics = [line.decode("ascii") for line in stderr_payloads[:-1]]
            prefix_diagnostics = actual_diagnostics==[native_prefix(case,side) for case,_ in failures]
            complete = re.fullmatch(rf"PASS: piston geometry replay; side={side}; rows=11001; prefixes=15; checks=[1-9][0-9]*; cleanup=OK",
                                   stderr_payloads[-1].decode("ascii")) is not None
            report["sides"][side] = {"status":"PASS" if framing and len(native_rows)==11001 and not mismatches and
                process["returncode"]==0 and prefix_diagnostics and complete else "FAIL", "original_complete_rows":11026,
                "selected_rows":11001,"exact_success_rows":10986,"native_body_failure_prefix_rows":15,
                "reflection_admission_exclusions":3,"actual_null_World_exclusions":22,"native_rows":len(native_rows),
                "mismatch_ids":mismatches,"exact_success_and_effect_payloads":not mismatches,
                "precise_native_status_PC_diagnostics":prefix_diagnostics,"native_cleanup_completion":complete,
                "original_line_ending":references[side]["ending"],"native_protocol_line_ending":native_ending,
                "native_bootstrap_line_ending":bootstrap_ending,"native_stderr_line_ending":stderr_ending,
                "input":pin(input_path),"original_selected":pin(expected_path),
                "native_bootstrap":pin(output/(side.lower()+".native-bootstrap.stdout")),
                "native_protocol":pin(output/(side.lower()+".native-protocol.stdout")),
                "selected":pin(output/(side.lower()+".selected.json")),"excluded":pin(output/(side.lower()+".excluded.json")),
                "results":pin(output/(side.lower()+".results.json"))}
        report["retained_inputs"] = retained
        if any(pin(item["path"])!=item for item in retained):
            raise RuntimeError("original/selected replay evidence changed during comparison")
        report["producer_after"],report["python_after"],report["native_after"] = pin(__file__),pin(sys.executable),pin(native)
        report["source_inputs_after"] = source_snapshot(repo)
        report["stable"] = all(report[key+"_before"]==report[key+"_after"] for key in ("producer","python","native","source_inputs"))
        if report["stable"] and len(report["sides"])==2 and all(side["status"]=="PASS" for side in report["sides"].values()):
            report["status"] = "PASS"
    except (Exception,KeyboardInterrupt) as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
    write(output/"report.json",report)
    print(json.dumps({"status":report["status"],"sides":report["sides"],"errors":report["errors"],"output":str(output)}),flush=True)
    return 0 if report["status"]=="PASS" else 1


if __name__=="__main__":
    raise SystemExit(main())
