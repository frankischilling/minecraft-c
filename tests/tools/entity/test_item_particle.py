"""Genuine PB/XW construction and real client particle owners.

Authored resources and Regions are execution inputs. Original constructor
entropy is independent. Optional original records are fingerprinted before
execution, semantically compared only afterwards, and never feed execution.
Post-array legacy current color is unspecified: native observations retain the
actual logical known flag and no invented value. Source's raw legacy device
color remains source-only at that phase; it earns no current-color parity credit.
"""
import argparse
import ast
import copy
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess
import struct
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[3]
REFERENCE=ROOT/"tests/reference/client/item_particle"
sys.dont_write_bytecode=True

def need(value,message):
    if not value:raise AssertionError(message)

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def digest(path):
    with path.open("rb") as source:return hashlib.file_digest(source,"sha256").hexdigest()

def authored_assets(path):
    png=load("item_particle_png",ROOT/"tests/tools/client/test_startup.py").authored_png
    for name,width,height in (("pack.png",64,32),("gui/items.png",256,256)):
        target=path/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(png(width,height,lambda x,y:((x^y)&255,x&255,y&255,255)))

def authored_world(path):
    import sys
    directory=ROOT/"tests/tools/entity";sys.path.insert(0,str(directory))
    try:load("item_particle_world",directory/"test_mob_tick.py").authored_world(path)
    finally:sys.path.pop(0)

def inventory(paths):return {str(path.resolve()):digest(path) for path in sorted(set(paths))}

def fp(value):
    kind,word=next(iter(value.items()));return struct.unpack(">f" if kind=="F" else ">d",bytes.fromhex(word))[0]

def f32(value):return struct.unpack(">f",struct.pack(">f",value))[0]

def word(value,kind):return {kind:struct.pack(">f" if kind=="F" else ">d",value).hex()}

def constructor_oracles(rows):
    """Independent scalar checks on observed native draws, never execution inputs."""
    states={(r["case"],r["phase"]):r for r in rows if r["event"]=="state"};checks=0
    for receipt in (r for r in rows if r["event"]=="constructor_random"):
        case=receipt["case"];state=states[case,"exception_receiver" if case in (13,14) else "constructor"]
        need(receipt["default_entropy_rebound"] is False and len(receipt["math_values"])==5 and len(receipt["own_values"])==4,"actual complete independent entropy receipt")
        shared=list(map(fp,receipt["math_values"]));own=list(map(fp,receipt["own_values"]))
        raw=[float(f32(f32(2*v-1)*f32(.4))) for v in shared[:3]]
        speed=f32(f32(shared[3]+shared[4]+1)*f32(.15));length=f32(math.sqrt(sum(v*v for v in raw)))
        velocity=[((v/length)*speed)*f32(.4) for v in raw];velocity[1]+=f32(.1)
        size=f32(f32(f32(own[2]*f32(.5))+f32(.5))*2)
        expected={name:word(value,"D") for name,value in zip(("sn.aP","sn.aQ","sn.aR"),velocity)}
        expected.update({"xw.c":word(f32(own[0]*3),"F"),"xw.d":word(f32(own[1]*3),"F"),"xw.g":word(size if case in (13,14) else f32(size/2),"F"),"xw.f":int(f32(4/f32(f32(own[3]*f32(.9))+f32(.1))))})
        for name,value in expected.items():need(state["fields"][name]==value,f"native constructor scalar oracle {case}/{name}");checks+=1
    return checks

def binary_class(name):
    dimensions=0
    while name.endswith("[]"):dimensions+=1;name=name[:-2]
    primitives={"boolean":"Z","byte":"B","char":"C","short":"S","int":"I","long":"J","float":"F","double":"D"}
    return "["*dimensions+primitives.get(name,"L"+name+";") if dimensions else name

class Pair:
    def __init__(self,bridge):
        self.bridge=bridge;self.forward={};self.reverse={};self.checks=0;self.matrices=[];self.unknown_colors=[];self.textures={0:0};self.texture_reverse={0:0}
    def source_ref(self,value):
        if value is None:return None
        need(isinstance(value,dict) and "class" in value,"actual typed Source reference")
        if "oid" in value:return binary_class(value["class"]),value["oid"]
        need(str(value["identity"]) in self.bridge,"every Source reference resolves through actual JDI bridge")
        item=self.bridge[str(value["identity"])];need(binary_class(item["class"])==binary_class(value["class"]),"actual Source reference class consistency")
        return binary_class(item["class"]),item["oid"]
    def bind(self,actual,expected,label,raw=False):
        self.checks+=1;source=self.source_ref(expected)
        if actual is None or source is None:need(actual is None and source is None,"reference null differs "+label);return
        native=("native_storage",actual) if raw else (actual["class"],actual["identity"])
        if not raw:need(native[0]==source[0],"actual object class differs "+label)
        need(native not in self.forward or self.forward[native]==source,"one actual native object changed Source identity "+label)
        need(source not in self.reverse or self.reverse[source]==native,"actual alias/temporal identity split "+label)
        self.forward[native]=source;self.reverse[source]=native
    def value(self,actual,expected,label):
        if isinstance(expected,dict) and "class" in expected:
            self.bind(actual,expected,label);return
        if isinstance(actual,dict) and set(actual)=={"actual","utf16"} and isinstance(expected,str):
            expected={"utf16":list(struct.unpack(">"+"H"*(len(expected.encode("utf-16-be","surrogatepass"))//2),expected.encode("utf-16-be","surrogatepass")))};actual={"utf16":actual["utf16"]}
        self.checks+=1
        need(type(actual)==type(expected),"typed value differs "+label)
        if isinstance(expected,dict):
            need(set(actual)==set(expected),"field inventory differs "+label)
            for key in expected:self.value(actual[key],expected[key],label+"/"+key)
        elif isinstance(expected,list):
            need(len(actual)==len(expected),"array extent differs "+label)
            for index,(a,b) in enumerate(zip(actual,expected)):self.value(a,b,label+"/"+str(index))
        else:need(actual==expected,"value differs "+label+": "+repr(actual)+" != "+repr(expected))
    def matrix(self,actual,expected,label):
        need(len(actual)==len(expected)==16,"complete prepared matrix")
        for index,(a,b) in enumerate(zip(actual,expected)):
            self.checks+=1
            if a==b:continue
            av,bv=fp({"F":a}),fp({"F":b});ai,bi=int(a,16),int(b,16)
            ordered=lambda n:(~n&0xffffffff) if n&0x80000000 else n|0x80000000
            distance=abs(ordered(ai)-ordered(bi));need(av==bv or math.isfinite(av) and math.isfinite(bv) and distance<=4,"prepared matrix beyond four ULP "+label)
            self.matrices.append({"location":label+"/"+str(index),"native":a,"source":b,"ulp":distance,"opposite_signed_zero":av==bv==0})
    def texture(self,actual,expected,label):
        self.checks+=1
        need(type(actual)==type(expected)==int and (actual==0)==(expected==0) and (actual not in self.textures or self.textures[actual]==expected) and (expected not in self.texture_reverse or self.texture_reverse[expected]==actual),"texture handle bijection including literal zero "+label)
        self.textures[actual]=expected;self.texture_reverse[expected]=actual

def compare_particle(pair,actual,source,label,raw=False,landing=False):
    pair.bind(actual["actual"],source["actual"],label+"/actual")
    need(set(actual["fields"])==set(source["fields"]),"all actual PB/SN field names "+label)
    independent={"sn.aP","sn.aQ","sn.aR","xw.c","xw.d","xw.f","xw.g"} if raw else set()
    if landing:independent|={"sn.aM","sn.aO"}
    for name in source["fields"]:
        if name not in independent:pair.value(actual["fields"][name],source["fields"][name],label+"/"+name)
    for name in source["bounds"]:
        if not landing or name in ("eq.b","eq.e"):pair.value(actual["bounds"][name],source["bounds"][name],label+"/"+name)
    records=source["watcher_records"];need(len(records)==actual["watcher"]["size"]==1,"complete PB Byte0 watcher")
    record=records[0];fields=record["fields"]
    pair.value(actual["watcher"]["index"],fields["ma.b"],label+"/watcher/index")
    pair.value(actual["watcher_type"],fields["ma.a"],label+"/watcher/type")
    pair.value(actual["watcher"]["byte"],record.get("typed_value",fields["ma.c"]),label+"/watcher/byte")
    pair.value(actual["watcher"]["record_dirty"],fields["ma.d"],label+"/watcher/record_dirty")
    pair.value(actual["watcher"]["dirty"],source.get("watcher_dirty",source.get("watcher_fields",{}).get("ud.c")),label+"/watcher/dirty")
    pair.bind(actual["watcher_record"],record.get("record",record.get("actual")),label+"/watcher/record")

def compare_globals(pair,actual,source,label):
    for name,expected in (("world",source["camera"]["fields"]["sn.aI"]),("camera",source["camera"]["actual"]),("renderer",source["renderer"]),("renderer_world",source["renderer_world"]),("app_dn",source["app_dn"])):pair.bind(actual[name],expected,label+"/"+name)
    for name in ("origins","box_cursor","vector_cursor","world_access_count"):pair.value(actual[name],source[name],label+"/"+name)
    dn=actual["effects"];expected=source["effects"]
    for name,value in (("actual",expected["actual"]),("world",expected["fields"]["dn.a"]),("engine",expected["fields"]["dn.c"]),("random",expected["fields"]["dn.d"])):pair.bind(dn[name],value,label+"/DN/"+name)
    pair.bind(dn["array_identity"],expected["array"],label+"/DN/array",raw=True)
    need(len(dn["lists"])==len(expected["lists"])==4,"all four real DN Lists")
    for index,(a,b) in enumerate(zip(dn["lists"],expected["lists"])):
        pair.bind(a["actual"],b["actual"],label+f"/DN/list{index}")
        pair.value(a["size"],b["size"],label+f"/DN/size{index}")
        pair.value(a["values"],b["values"][:b["size"]],label+f"/DN/values{index}")
    nw=actual["nw"];expected=source["nw"];pair.bind(nw["actual"],expected["actual"],label+"/NW")
    need(set(nw["fields"])==set(expected["fields"])-{"nw.d","nw.e","nw.f","nw.g","nw.y"},"complete selected NW field inventory")
    for name,value in nw["fields"].items():pair.value(value,expected["fields"][name],label+"/"+name)
    for index,name in enumerate(("d","e","f","g","y")):pair.bind(nw["reference_identities"][index],expected["fields"]["nw."+name],label+"/NW/"+name,raw=True)
    for name in ("position","selected_words","backing_prefix"):pair.value(nw[name],expected[name],label+"/NW/"+name)

def compare_gl(pair,actual,expected,label):
    need(actual["color_known"] is False and actual["color"] is None,"preserved unspecified post-array logical current color")
    pair.unknown_colors.append({"location":label,"source_raw_legacy_color":expected["color"],"native_color_known":False})
    for name in ("modelview","projection"):pair.matrix(actual[name],expected[name],label+"/prepared/"+name)
    for name in ("normal","mode","modelview_depth","projection_depth","depth_mask","depth_function","blend_src","blend_dst","caps"):pair.value(actual[name],expected[name],label+"/"+name)
    pair.texture(actual["texture"],expected["texture"],label+"/texture")

def comparison(native,source):
    need([r["case"] for r in source["operation_records"]]==list(range(17)) and len(source["debugger_operation_records"])==len(source["case_records"])==17,"complete seventeen original ordinary/JDI operations")
    originals={r["case"]:r for r in source["operation_records"]};pairs={};states={};groups={}
    for mode,rows in native.items():
        bridge={}
        for case in source["case_records"]:
            if case["mode"]==mode:
                for table in (case["entry_bridge"],case["after_bridge"]):
                    for key,value in table.items():need(key not in bridge or bridge[key]==value,"Source temporal bridge integrity");bridge[key]=value
        pairs[mode]=Pair(bridge);states[mode]={(r["case"],r["phase"]):r for r in rows if r["event"]=="state"}
        groups[mode]={}
        for row in rows:groups[mode].setdefault(row["event"],[]).append(row)
    p=pairs["main"];s=states["main"]
    compare_globals(p,groups["main"]["context"][0]["globals"],originals[0]["before"],"0/before")
    for case in (0,1,2):
        a=s[case,"after" if case==0 else "constructor"];b=originals[case]["after"]
        compare_particle(p,a,b["particle"],str(case),raw=True);compare_globals(p,a["globals"],b,str(case))
    for context in range(100,105):
        original=next(r for r in source["constructor_records"]["main"] if r["constructor_case"]==context)
        compare_particle(p,s[context,"constructor"],original["end"]["particle"],str(context),raw=True)
    for case in (3,4,5,6,7,8,9,12):
        for phase in ("before","after"):
            a=s[case,phase];b=originals[case][phase]
            compare_particle(p,a,b["particle"],f"{case}/{phase}");compare_globals(p,a["globals"],b,f"{case}/{phase}")
    for index in range(8):
        a=s[10,f"emitted_{index}"];b=originals[10]["after"]
        compare_particle(p,a,b["layer_particles"][index],f"10/{index}",raw=True,landing=True);compare_globals(p,a["globals"],b,f"10/{index}")
        for phase in ("before","after"):
            a=s[11,f"layer_{phase}_{index}"];b=originals[11][phase]
            compare_particle(p,a,b["layer_particles"][index],f"11/{phase}/{index}");compare_globals(p,a["globals"],b,f"11/{phase}/{index}")
    for row in groups["main"]["draw"]:
        case=row["case"];expected=originals[case]["after"]
        p.value(row["words"],expected["nw"]["selected_words"],f"{case}/draw/words")
        p.value(row["backing_prefix"],expected["nw"]["backing_prefix"],f"{case}/draw/backing")
        p.bind(row["DN"],expected["effects"]["actual"],f"{case}/draw/DN")
        p.bind(row["engine"],expected["effects"]["fields"]["dn.c"],f"{case}/draw/engine")
        compare_gl(p,row["gl"],expected["gl"],f"{case}/GL")
    atlas=groups["main"]["texture"][0];source_atlas=originals[7]["after"]["texture"]["cache"]["/gui/items.png"]
    p.value(atlas["transfer_hex"],originals[7]["after"]["texture"]["transfer_hex"],"actual decoded atlas transfer")
    p.texture(atlas["name"],source_atlas,"atlas actual lookup name");p.texture(atlas["cached_name"],source_atlas,"atlas cache hit name")
    for mode,case in (("null_item",13),("null_snow",14),("null_world_tick",15),("null_world_draw",16)):
        pair=pairs[mode];state=states[mode];expected=originals[case]
        failure=groups[mode]["failure"][0];exception=source["case_records"][case]["exceptions"][0]
        need(failure["source_class"]==exception["location"]["class"] and failure["method"]==exception["location"]["method"] and failure["pc"]==exception["location"]["pc"] and failure["exception_class"]==exception["class"].replace(".","/"),"actual Source exception class/method/BCI")
        if case<15:
            a=state[case,"exception_receiver"];compare_particle(pair,a,exception["particles"][0],f"{case}/partial",raw=True);compare_globals(pair,a["globals"],expected["after"],str(case))
        else:
            constructor=source["constructor_records"][mode][0]
            compare_particle(pair,state[105 if case==15 else 106,"constructor"],constructor["end"]["particle"],str(case)+"/constructor",raw=True)
            for phase in ("before","after"):
                a=state[case,phase];b=expected[phase];compare_particle(pair,a,b["particle"],f"{case}/{phase}");compare_globals(pair,a["globals"],b,f"{case}/{phase}")
            if case==16:compare_particle(pair,state[case,"exception_receiver"],exception["particles"][0],"16/actual partial receiver")
    # Exact unchanged stream progress is paired separately from independent
    # default constructor seeds. These operations draw from none of the four.
    stream_checks=0
    for mode,cases in (("main",(3,4,5,6,7,8,9,12)),("null_world_tick",(15,)),("null_world_draw",(16,))):
        for case in cases:
            a,b=states[mode][case,"before"],states[mode][case,"after"]
            for nk,sk in (("own","particle"),("world","world"),("math","math"),("uniquifier","uniquifier")):
                need(a["streams"][nk]==b["streams"][nk] and originals[case]["before"]["streams"][sk]==originals[case]["after"]["streams"][sk],"actual independent stream unchanged "+str(case)+"/"+nk);stream_checks+=1
    for mode in native:
        actual=[r["globals"]["effects"]["random_seed"] for r in native[mode] if r["event"] in ("state","context")]
        expected=[r[phase]["effects"]["random_seed"] for r in source["operation_records"] if r["input"][1]==mode for phase in ("before","after")]
        need(actual and expected and len(set(actual))==len(set(expected))==1,"actual independent DN Random remains unchanged "+mode);stream_checks+=1
    for index in range(8):
        need(s[11,f"layer_before_{index}"]["streams"]["own"]==s[11,f"layer_after_{index}"]["streams"]["own"],"actual eight PB own streams unchanged")
    need(not any(e["event"]=="random" for e in source["case_records"][11]["events"]),"actual Source complete eight-PB DN update has no Random call")
    constructors=source["constructor_records"];need(sum(map(len,constructors.values()))==20 and all(r["source_scalar_oracles"] and not r["constructor_entropy_rebound"] and len(r["shared_draws"])==5 and len(r["private_draws"])==4 for rows in constructors.values() for r in rows),"all twenty actual Source constructor intervals/oracles")
    io=source["image_io_entropy"];counts={"main":1,"null_item":0,"null_snow":0,"null_world_tick":0,"null_world_draw":1}
    need(set(io)==set(counts) and {mode:len(rows) for mode,rows in io.items()}==counts,"exact five-mode ImageIO event counts1/0/0/0/1")
    for mode,case in (("main",7),("null_world_draw",16)):
        event=io[mode][0];frames={(r["class"],r["method"],r["pc"]) for r in event["producer_frames"]}
        need(event["case"]==case and event["method"]=="nextLong" and event["receiver"]["class"]=="java.security.SecureRandom" and {("java.nio.file.TempFileHelper","generatePath",3),("java.nio.file.Files","createTempFile",4),("javax.imageio.stream.FileCacheImageInputStream","<init>",76)}<=frames,"actual narrow JDK image-decoder producer; no generic RNG mask")
    return {"operations":17,"full_returns":13,"source_failures":4,"typed_checks":sum(p.checks for p in pairs.values()),"identity_pairs":sum(len(p.forward) for p in pairs.values()),"stream_progress_checks":stream_checks,"source_constructor_intervals":20,"complete_draw_words":480,"backing_prefix_words_per_observation":1024,"source_ImageIO_entropy_events":2,"prepared_matrix_differences":[x for p in pairs.values() for x in p.matrices],"unspecified_current_colors":[x for p in pairs.values() for x in p.unknown_colors]}

def validate_manifest(capture,audit,receipt,observations,primary_current,output_current):
    need(capture["status"]==audit["status"]=="PASS" and capture["input_before_sha256"]==capture["input_after_sha256"] and capture["helper_before_sha256"]==capture["helper_after_sha256"],"audited actual original capture inputs/helpers")
    need(len(capture["input_after_sha256"])==audit["current_inputs"]==28 and len(capture["helper_after_sha256"])==audit["current_helpers"]==4,"exact original28inputs/fourhelpers")
    paths={str(Path(name).name) for name in capture["input_after_sha256"]}
    others={"java","javac","libjvm.so","modules","python3.12","Xvfb","xvfb-run","xauth","lwjgl-2.9.0.jar","lwjgl_util-2.9.0.jar","jinput-2.0.5.jar","jutils-1.0.0.jar","lwjgl-platform-2.9.0-natives-linux.jar","liblwjgl64.so","b1.7.3.jar","support.py","classfile.py"}
    public={p.name for p in REFERENCE.iterdir() if p.is_file()};need(len(public)==11 and paths==public|others,"complete mandatory original input schema")
    need(set(capture["helper_after_sha256"])=={"ConstructorJson.class","ItemParticleJdi.class","ItemParticleTarget.class","ItemParticleTarget$1.class"},"all four actual compiled helper classes")
    for name,expected in capture["input_after_sha256"].items():need(primary_current.get(name)==expected,"current Source input pin "+name)
    for name,expected in capture["helper_after_sha256"].items():need(primary_current.get(str(receipt.parent/"classes"/name))==expected,"current actual helper pin "+name)
    need(audit["capture_sha256"]==digest(receipt) and audit["observations_sha256"]==capture["observations"]["sha256"]==digest(observations) and audit["provenance_sha256"]==digest(REFERENCE/"provenance.json"),"exact actual capture/observations/provenance lineage")
    need(audit["all11_public_files_LF_and_frozen"] is True and all(b"\r" not in p.read_bytes() for p in REFERENCE.iterdir() if p.is_file()),"all eleven frozen public text files LF")
    need(len(audit["source_output_inventory"])==234 and set(audit["source_output_inventory"])==set(output_current),"exact post-capture current234-output inventory; not capture-start evidence")
    need({"capture.json","observations.json","normal/main/target.ndjson","debugger/main/checkpoints.ndjson"}<=set(output_current),"all consumed primary evidence present")
    need(audit["source_output_inventory"]==output_current,"post-capture current Source artifact hashes")

def original_audit(observations,audit_path):
    helper=load("item_particle_source_audit",ROOT/"tests/tools/client/test_creature_draw.py")
    local,primary=helper.capture_audit(observations)
    receipt=observations.with_name("capture.json");capture=json.loads(receipt.read_text());audit=json.loads(audit_path.read_text())
    files=[p for p in receipt.parent.rglob("*") if p.is_file()]
    current={p.relative_to(receipt.parent).as_posix():digest(p) for p in files}
    validate_manifest(capture,audit,receipt,observations,primary["current_sha256"],current)
    return list(set(local+files+[audit_path,ROOT/"tests/tools/client/test_creature_draw.py"])),{"capture":str(receipt),"capture_sha256":digest(receipt),"observations_sha256":digest(observations),"post_capture_current_audit":str(audit_path),"post_capture_current_audit_sha256":digest(audit_path),"input_count":28,"helper_count":4,"post_capture_current_outputs":234,"current_input_helper_sha256":primary["current_sha256"],"post_capture_current_output_sha256":current},(capture,audit,primary["current_sha256"],current)

def manifest_negatives(bundle,observations):
    capture,audit,inputs,outputs=bundle;receipt=observations.with_name("capture.json");rejected=[]
    for name in ("missing_observations_output","changed_target_hash","missing_JAR_input","changed_helper_hash"):
        c,a=copy.deepcopy(capture),copy.deepcopy(audit)
        if name=="missing_observations_output":del a["source_output_inventory"]["observations.json"]
        elif name=="changed_target_hash":a["source_output_inventory"]["normal/main/target.ndjson"]="0"*64
        elif name=="missing_JAR_input":
            key=next(k for k in c["input_after_sha256"] if Path(k).name=="b1.7.3.jar")
            del c["input_after_sha256"][key];del c["input_before_sha256"][key]
        else:c["helper_after_sha256"]["ItemParticleTarget.class"]="0"*64;c["helper_before_sha256"]["ItemParticleTarget.class"]="0"*64
        try:validate_manifest(c,a,receipt,observations,inputs,outputs)
        except AssertionError as error:
            expected={"missing_observations_output":"exact post-capture current234-output inventory","changed_target_hash":"post-capture current Source artifact hashes","missing_JAR_input":"exact original28inputs/fourhelpers","changed_helper_hash":"current actual helper pin"}[name]
            need(expected in str(error),"manifest corruption failed for an unrelated reason: "+name+"/"+str(error));rejected.append(name)
        else:raise AssertionError("manifest corruption was accepted: "+name)
    return rejected

def comparator_negatives(native,source):
    rejected=[]
    for name in ("wrong_PB_class","wrong_World_alias","fresh_Random_replacement","wrong_watcher_type","normal_backing_word","zero_texture","split_texture_handle","constructor_formula"):
        rows=copy.deepcopy(native)
        pick=lambda case,phase:next(r for r in rows["main"] if r["event"]=="state" and r["case"]==case and r["phase"]==phase)
        if name=="wrong_PB_class":pick(3,"after")["actual"]["class"]="em"
        elif name=="wrong_World_alias":pick(3,"after")["fields"]["sn.aI"]["identity"]="foreign_world"
        elif name=="fresh_Random_replacement":pick(3,"after")["fields"]["sn.bs"]["identity"]="fresh_same_value_random"
        elif name=="wrong_watcher_type":pick(3,"after")["watcher_type"]=1
        elif name=="normal_backing_word":next(r for r in rows["main"] if r["event"]=="draw")["words"][6]="deadbeef"
        elif name=="zero_texture":next(r for r in rows["main"] if r["event"]=="draw")["gl"]["texture"]=0
        elif name=="split_texture_handle":[r for r in rows["main"] if r["event"]=="draw"][1]["gl"]["texture"]=987654
        else:pick(1,"constructor")["fields"]["xw.g"]={"F":"00000000"}
        try:
            if name=="constructor_formula":constructor_oracles(rows["main"])
            else:comparison(rows,source)
        except AssertionError as error:
            expected={"wrong_PB_class":"actual object class differs 3/after/actual","wrong_World_alias":"actual alias/temporal identity split 3/after/sn.aI","fresh_Random_replacement":"actual alias/temporal identity split 3/after/sn.bs","wrong_watcher_type":"value differs 3/after/watcher/type","normal_backing_word":"value differs 7/draw/words/6","zero_texture":"texture handle bijection including literal zero 7/GL/texture","split_texture_handle":"texture handle bijection including literal zero 8/GL/texture","constructor_formula":"native constructor scalar oracle 1/xw.g"}[name]
            need(expected in str(error),"paired corruption failed for an unrelated reason: "+name+"/"+str(error));rejected.append(name)
        else:raise AssertionError("paired corruption was accepted: "+name)
    return rejected

def native_records(path):
    rows=[json.loads(line) for line in (path/"native.ndjson").read_text().splitlines()]
    groups={}
    for row in rows:
        need(row.get("event") in {"state","constructor_random","N_emit","landing","draw","texture","context","control"},"known native record kind")
        groups.setdefault(row["event"],[]).append(row)
    states={}
    for row in groups.get("state",[]):
        key=(row["case"],row["phase"]);need(key not in states,"unique native state");states[key]=row
    required={(0,"after"),(1,"constructor"),(2,"constructor"),(104,"constructor")}
    for case in range(3,7):required|={(97+case,"constructor"),(case,"before"),(case,"after")}
    for case in (7,8,12):required|={(case,"before"),(case,"after")}
    required|={(9,"before"),(9,"after")}
    for ordinal in range(8):required|={(10,f"emitted_{ordinal}"),(11,f"layer_before_{ordinal}"),(11,f"layer_after_{ordinal}")}
    need(set(states)==required,"complete native state groups; no truncation or extra states")
    need(len(groups.get("context",[]))==1 and groups["context"][0]["case"]==0 and groups["context"][0]["phase"]=="before","actual before-enqueue context")
    need({r["case"] for r in groups.get("constructor_random",[])}=={1,2,100,101,102,103,104} and len(groups["constructor_random"])==7,"seven actual constructor entropy receipts")
    emits=groups.get("N_emit",[]);need([r["case"] for r in emits]==[0,9],"both actual N callback controls")
    need(emits[0]["before_size"]==0 and emits[0]["after_size"]==1 and not emits[0]["dn_null"],"real N enqueued PB")
    need(emits[1]["before_size"]==emits[1]["after_size"]==1 and emits[1]["dn_null"] and emits[1]["DN"] is None,"initial null DN returned without enqueue")
    need(all(r["registrations"]==1 for r in emits),"genuine N WorldAccess registration")
    landings=groups.get("landing",[]);need(len(landings)==1 and landings[0]["case"]==10 and landings[0]["grounded"] and landings[0]["layer2_size"]==8,"actual Slime landing emitted eight")
    need(len({r["identity"] for r in landings[0]["particles"]})==8,"eight distinct actual PB objects")
    draws=groups.get("draw",[]);need([r["case"] for r in draws]==[7,8,12],"all three actual DN draws")
    pixels={}
    for row in draws:
        count=384 if row["case"]==12 else 48
        need(row["position"]==len(row["words"])==count and len(row["backing_prefix"])==1024,"complete selected NW words and backing prefix")
        need(row["words"]==row["backing_prefix"][:count],"selected words are actual shared backing")
        need(row["gl"]["color_known"] is False and row["gl"]["color"] is None,"unspecified post-array current color preserved")
        baseline=(path/f'baseline{row["case"]}.rgba').read_bytes();actual=(path/f'draw{row["case"]}.rgba').read_bytes()
        need(len(baseline)==len(actual)==256*256*4,"complete native pixel readbacks")
        changed=sum(baseline[i:i+4]!=actual[i:i+4] for i in range(0,len(actual),4));need(changed>0,"visible genuine PB draw")
        pixels[str(row["case"])]=changed
    need(len(groups.get("texture",[]))==1,"actual atlas transfer and later cache-hit check")
    rgba=bytes(v for y in range(256) for x in range(256) for v in ((x^y)&255,x,y,255))
    need(bytes.fromhex(groups["texture"][0]["transfer_hex"])==rgba,"actual authored decoded atlas RGBA formula")
    need([r["cache_before"] for r in draws]==[False,True,True] and all(r["cache_after"] for r in draws),"actual first DN lookup uploads cold; later draws hit")
    controls=groups.get("control",[]);need([r["name"] for r in controls]==["pending_N_App_DN_World_Math","nonempty_DN_terminal"],"both actual native owner controls executed")
    need(controls[0]["native_policy"] and not controls[0]["source_exception"] and [controls[0][k] for k in ("N_pc","XW_pc","IN_pc","shared_draws","private_draws")]==[702,164,2,5,0] and all(controls[0][k] for k in ("WA_retire_refused","App_retire_refused","native_prefix_preserved")),"late-Math native policy and retained owner proof")
    need(controls[1]["PB_occurrences"]==9 and controls[1]["distinct_PB"]==8 and controls[1]["total_entities"]==10 and controls[1]["world_bindings"]==12 and controls[1]["external_refused"],"actual duplicate DN occurrence and terminal audit")
    return {"native_record_count":len(rows),"native_state_records":len(states),"native_draw_records":len(draws),"native_landing_particles":8,"native_visible_changed_pixels":pixels,"native_ownership_controls":controls}

def native_failure_records(path,mode):
    cases={"null_item":13,"null_snow":14,"null_world_tick":15,"null_world_draw":16};case=cases[mode]
    rows=[json.loads(line) for line in (path/"native.ndjson").read_text().splitlines()]
    need(all(r["event"] in {"state","constructor_random","failure"} for r in rows),"isolated failure complete record types")
    states=[r for r in rows if r["event"]=="state"];failures=[r for r in rows if r["event"]=="failure"];random=[r for r in rows if r["event"]=="constructor_random"]
    expected={(case,"exception_receiver")} if case<15 else {(case,"before"),(case,"after"),(105 if case==15 else 106,"constructor")}
    if case==16:expected.add((case,"exception_receiver"))
    need(len(states)==len(expected) and {(r["case"],r["phase"]) for r in states}==expected,"every expected isolated partial receiver state")
    need(len(random)==1 and random[0]["case"]==(case if case<15 else 105 if case==15 else 106),"actual isolated constructor entropy receipt")
    need(len(failures)==1 and failures[0]["case"]==case and failures[0]["pc"]=={13:17,14:42,15:394,16:118}[case] and failures[0]["exception_class"]=="java/lang/NullPointerException" and failures[0]["source_exception"] and not failures[0]["native_policy"] and failures[0]["prefix_preserved_through_cleanup"],"exact source failure and cleanup prefix proof")
    return {"records":len(rows),"states":len(states),"case":case,"source_failure":failures[0]}

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--executable",type=Path,required=True);parser.add_argument("--work-dir",type=Path,required=True);parser.add_argument("--observations",type=Path);parser.add_argument("--original-audit",type=Path);args=parser.parse_args()
    need(bool(args.observations)==bool(args.original_audit),"--observations requires its explicit post-capture --original-audit")
    args.work_dir.mkdir(parents=True,exist_ok=True);out=Path(tempfile.mkdtemp(prefix="item-particle-",dir=args.work_dir.resolve()))
    assets=out/"assets";world=out/"world";authored_assets(assets);authored_world(world)
    modes=("main","null_item","null_snow","null_world_tick","null_world_draw");directories={"main":out};worlds={"main":world}
    for mode in modes[1:]:
        directories[mode]=out/mode;directories[mode].mkdir();worlds[mode]=directories[mode]/"world";authored_world(worlds[mode])
    executable=args.executable.resolve(strict=True);resources=ROOT/"tests/client/startup/fixtures"
    inputs=[executable,Path(__file__),ROOT/"tests/entity/test_item_particle.c",ROOT/"tests/tools/client/test_startup.py",ROOT/"tests/tools/entity/test_mob_tick.py",ROOT/"tests/tools/entity/test_mob_construction.py"]
    inputs.extend(ROOT/"tests/client/startup"/name for name in ("test_application_startup.c","controller_boundary_observer.h","application_startup_statistics_controls.inc","application_startup_effects_controls.inc","application_startup_rendering_controls.inc"))
    for directory in (ROOT/"src",ROOT/"include",resources,assets):inputs.extend(p for p in directory.rglob("*") if p.is_file())
    for value in worlds.values():inputs.extend(value.rglob("*.mcr"))
    before=None;original_before=None;bundle=None
    report={"status":"FAIL","native_execution_started":False,"scope":"genuine native PB/XW/App/World/N/DN, actual Slime landing, and four isolated source-exception prefixes; actual raw constructor entropy independent","observation_limits":[
        "Raw constructor entropy fields sn.aP/sn.aQ/sn.aR and xw.c/xw.d/xw.f/xw.g are not cross-VM equal. Actual eleven direct native receipts independently qualify their scalar formulas; all twenty Source constructor intervals retain separate original scalar proofs. N-contained native constructor receipts are not claimed as eleven additional direct observations.",
        "Raw landing particle current x/z (sn.aM/sn.aO) and bounds eq.a/eq.c/eq.d/eq.f depend on the Slime's independent private Random floats at47/61 before IN sin74/cos90 and are not cross-VM equal. All subsequently declared prepared particle fields/bounds are strict pairs.",
        "Default private/shared/World/DN/profile initial seeds are independent. Prepared operations' actual unchanged progress is compared separately; Source ImageIO's two exact SecureRandom/NIO temporary-file events are retained source-only and not treated as game streams.",
        "Post-array legacy current color is unspecified. Native logical color_known=false/color=null and raw Source legacy device values are retained separately, without equality credit or a substituted core attribute.",
        "NW native buffer/array reference alias and temporal relations are compared, but concrete JDK DirectBuffer classes/layouts/cursors beyond actual IntBuffer position are not. Java DN List capacity/modCount/backing layout are unpaired; logical occurrence order/identity is paired.",
        "ma record class/identity/type/index/typed Byte0 value/dirty and UD dirty are paired. Source boxed-Byte object class/identity/alias and ordinary flattened String identity are not established; flattened Strings compare UTF16 values only.",
        "Texture scope is decoded atlas transfer, actual lookup/cache-hit handle relation and selected bound atlas. Texture dimensions/sampler/transfer-buffer cursor/cache-map graph are unpaired here; existing texture subsystem tests remain separate.",
        "Prepared view/projection matrices alone permit four float ULP or numeric equality of opposite signed zeros, with raw differences recorded. All other selected values are exact. No native/source pixel, full GL-call-order or whole-frame equality is claimed."]}
    try:
        before=inventory(inputs);report["input_before_sha256"]=before
        if args.observations:
            args.observations=args.observations.resolve(strict=True);args.original_audit=args.original_audit.resolve(strict=True)
            report["original_requested"]={"observations":str(args.observations),"post_capture_current_audit":str(args.original_audit)}
            additional,original_before,bundle=original_audit(args.observations,args.original_audit);inputs.extend(additional)
            before=inventory(inputs);report["input_before_sha256"]=before
            report["original_audit_before"]=original_before;report["manifest_corruption_controls"]=manifest_negatives(bundle,args.observations)
        report["processes"]={};total=0
        for mode in modes:
            directory=directories[mode];env=os.environ.copy();env["ALSOFT_DRIVERS"]="null"
            for name in ("APPDATA","HOME","XDG_DATA_HOME","XDG_CONFIG_HOME","XDG_CACHE_HOME"):
                target=directory/name.lower();target.mkdir();env[name]=str(target)
            command=[str(executable),str(resources),str(assets),str(worlds[mode]),str(directory),mode]
            report["native_execution_started"]=True
            result=subprocess.run(command,cwd=directory,env=env,capture_output=True,text=True,timeout=180)
            (directory/"native.stdout.txt").write_text(result.stdout,newline="\n");(directory/"native.stderr.txt").write_text(result.stderr,newline="\n")
            record={"command":command,"exit":result.returncode,"stdout_file":str(directory/"native.stdout.txt"),"stderr_file":str(directory/"native.stderr.txt"),"stderr_sha256":digest(directory/"native.stderr.txt"),"stderr_excerpt":result.stderr[:8000]};report["processes"][mode]=record
            need(result.returncode==0,"native item-particle assertions failed "+mode)
            need(result.stderr in ("","[ALSOFT] (WW) No capture backend available!\n"),"no undeclared native diagnostic "+mode)
            lines=result.stdout.splitlines();need(len(lines)==3 and lines[:2]==["151 recipes","16 achievements"],"only exact declared setup output pair and one footer "+mode)
            footer=re.fullmatch(r"Item particle: (\d+) checks; genuine PB/XW/World/DN",lines[2]);need(footer is not None,"actual assertion footer "+mode)
            record["checks"]=int(footer[1]);total+=record["checks"]
            if mode=="main":report.update(native_records(directory));report["native_main_checks"]=record["checks"]
            else:record.update(native_failure_records(directory,mode))
        native={mode:[json.loads(line) for line in (directories[mode]/"native.ndjson").read_text().splitlines()] for mode in modes}
        report["native_constructor_oracle_intervals"]=11
        report["native_constructor_scalar_checks"]=sum(constructor_oracles(rows) for rows in native.values())
        if args.observations:
            source=json.loads(args.observations.read_text())
            report["original_comparison"]=comparison(native,source)
            report["comparator_corruption_controls"]=comparator_negatives(native,source)
        report["native_checks"]=total;report["source_failure_controls"]=4;report["status"]="PASS"
    except Exception as error:
        report["status"]="FAIL";report["error"]=type(error).__name__+": "+str(error)[:8000]
    finally:
        if original_before is not None:
            try:
                _,original_after,_=original_audit(args.observations,args.original_audit);report["original_audit_after"]=original_after
                need(original_after==original_before,"original current input/helper/artifact drift")
            except Exception as error:
                report["status"]="FAIL";report["original_final_audit_error"]=type(error).__name__+": "+str(error)[:8000]
        try:
            report["input_after_sha256"]=inventory(inputs)
            need(before is not None and before==report["input_after_sha256"],"input/executable drift")
        except Exception as error:
            report["status"]="FAIL";report["final_input_audit_error"]=type(error).__name__+": "+str(error)[:8000]
        (out/"run.json").write_text(json.dumps(report,indent=2)+"\n",newline="\n")
    if report["status"]!="PASS":raise SystemExit("Item particle validation failed; "+str(out/"run.json"))
    print(f"Item particle: {report['native_checks']} checks across five fresh processes");print(out/"run.json")

if __name__=="__main__":main()
