#!/usr/bin/env python3
"""Bounded native C/original copied-world storage roundtrips.

Original disk NBT supplies expected data. Explicit clock inputs, generic opaque
transport and explicit-provider dimensions are finite storage evidence only.
"""
from __future__ import annotations
from pathlib import Path
import argparse
import copy
import hashlib
import importlib.util
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import time
import uuid
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
JAVA_SOURCES = ("tests/reference/storage/WorldStorageObservation.java", "tests/reference/storage/WorldOpaqueObservation.java")
COORDS = [(0,0),(1,0),(-1,-1),(31,31),(32,32),(-32,-33),(-(1<<31),(1<<31)-1),((1<<31)-1,-(1<<31))]
SLOTS = [(dim,x,z) for dim in (0,-1) for x,z in COORDS]
CLOCK, TIMESTAMP, TOKEN = 1660000000123, 0x89abcdef, "0123456789abcdef"
PIN = {"client":"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c", "server":"033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS","JDK_JAVAC_OPTIONS","CLASSPATH")
LOADED_PYTHON = {}

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    raw=Path(path).read_bytes()
    if len(raw)>2*1024*1024:raise ValueError("bounded executing Python source")
    LOADED_PYTHON[str(Path(path).resolve())]={"bytes":len(raw),"sha256":hashlib.sha256(raw).hexdigest()}
    exec(compile(raw,str(Path(path).resolve()),"exec"),mod.__dict__)
    return mod

for _module in ("compare_level","export_level","compare_chunk_storage","export_chunk_storage"):
    load(_module,ROOT/"scripts/reference/storage"/(_module+".py"))
t = load("_world_storage_transport", ROOT/"tests/tools/storage/test_store.py")
r = load("_world_storage_process", ROOT/"scripts/reference/storage/compare_region_write.py")
level = t.level
Failure, Unavailable, check = r.Failure, r.Unavailable, r.check
m = a = sys.modules[__name__]

def fp(raw):return {"bytes":len(raw),"sha256":hashlib.sha256(raw).hexdigest()}

def read(path,limit=32*1024*1024):
    check(path.is_file() and not path.is_symlink() and path.stat().st_size<=limit,"bounded ordinary input")
    raw=path.read_bytes();check(len(raw)<=limit,"complete bounded input");return raw

def inventory(root):
    result={};total=0
    for path in sorted(root.rglob("*")):
        check(not path.is_symlink(),"owned ordinary tree")
        if path.is_file():
            raw=read(path,8*1024*1024);total+=len(raw)
            check(len(result)<1024 and total<=64*1024*1024,"bounded complete world inventory")
            result[path.relative_to(root).as_posix()]=fp(raw)
    return result

def helper_classes(directory):
    result={}
    for path in sorted(directory.rglob("*.class")):
        raw=read(path);check(len(raw)>=8 and raw[:4]==b"\xca\xfe\xba\xbe" and struct.unpack_from(">H",raw,6)[0]==61,"actual Java17 helper class")
        result[path.relative_to(directory).as_posix()]=fp(raw)
    check(set(result)=={"WorldStorageObservation.class","WorldStorageObservation$Context.class","WorldOpaqueObservation.class"},"exact helper class inventory")
    return result

def raw_metadata(raw):
    stream=zlib.decompressobj(31);plain=stream.decompress(raw,65537)
    check(stream.eof and not stream.unconsumed_tail and not stream.unused_data and len(plain)<=65536,"complete metadata gzip")
    reader=level.Reader(plain);tree=reader.tag();check(reader.p==len(plain),"full metadata NBT")
    data=level.child(tree,"Data");clock=level.child(data,"LastPlayed")
    check(clock["type"]==4,"actual LastPlayed long")
    marker=b"\x04\x00\x0aLastPlayed";check(plain.count(marker)==1,"unique measured LastPlayed payload")
    position=plain.index(marker)+len(marker)
    return plain,clock["value"],(position,position+8),tree

def parse_blob(value):
    check(type(value) is dict and set(value)=={"bytes","sha256","hex"},"complete raw blob")
    check(type(value["bytes"]) is int and 0<=value["bytes"]<=32*1024*1024 and
          type(value["hex"]) is str and re.fullmatch(r"[0-9a-f]*",value["hex"]) is not None and
          len(value["hex"])==value["bytes"]*2 and type(value["sha256"]) is str and
          re.fullmatch(r"[0-9a-f]{64}",value["sha256"]) is not None,"exact bounded canonical blob")
    raw=bytes.fromhex(value["hex"]);check(t.same(fp(raw),{k:value[k] for k in ("bytes","sha256")}),"actual blob fingerprint")
    return raw

def disk_chunk(parent,dimension,x,z):
    directory=parent/"world"
    if dimension==-1:directory=directory/"DIM-1"
    raw=read(directory/"region"/f"r.{x>>5}.{z>>5}.mcr")
    index=(x&31)+32*(z&31);location=struct.unpack_from(">I",raw,index*4)[0]
    start=(location>>8)*4096;count=location&255
    check(location and start>=8192 and count and start+count*4096<=len(raw),"complete baseline slot")
    length=struct.unpack_from(">I",raw,start)[0];selector=raw[start+4]
    check(length>=1 and length+4<=count*4096 and selector in (1,2),"complete typed baseline payload")
    stream=zlib.decompressobj(31 if selector==1 else 15)
    plain=stream.decompress(raw[start+5:start+4+length],2*1024*1024+1)
    check(stream.eof and not stream.unconsumed_tail and len(plain)<=2*1024*1024,"bounded actual chunk NBT")
    reader=level.Reader(plain);tree=reader.tag();check(reader.p==len(plain),"complete chunk NBT tree")
    return level.child(tree,"Level")

def expected_loaded(parent,block_map,dimension,x,z):
    value=disk_chunk(parent,dimension,x,z)
    check(level.child(value,"xPos")["value"]==x and level.child(value,"zPos")["value"]==z,"known matching storage coordinates")
    arrays={}
    for role,key,size in (("blocks","Blocks",32768),("metadata","Data",16384),("skylight","SkyLight",16384),
                          ("block_light","BlockLight",16384),("heightmap","HeightMap",256)):
        tag=level.child(value,key);check(tag["type"]==7,"actual typed chunk array")
        raw=bytes.fromhex(tag["value"]);check(len(raw)==size,"complete baseline known array")
        if role=="blocks":raw=bytes(block_map[byte] for byte in raw)
        arrays[role]=fp(raw)
    terrain=level.child(value,"TerrainPopulated")["value"]!=0
    return arrays,{"c":False,"n":terrain,"o":False,"p":False,"q":False}

def walk(value):
    yield value
    if isinstance(value,dict):
        for item in value.values():yield from walk(item)
    elif isinstance(value,list):
        for item in value:yield from walk(item)

def validate_record(record,parent,baseline=None):
    check(record["schema"]==1 and record["jar_sha256"]==PIN[record["side"]],"original identity")
    check(len(parse_blob(record["block_map"]))==256,"actual postload registry map")
    clocks=set();writes=[];full_saves=[]
    for item in walk(record):
        if not isinstance(item,dict):continue
        if set(item)=={"bytes","sha256","hex"}:parse_blob(item)
        if item.get("kind")=="handler":
            start,end=item["interval_ms"];token=int(item["session_token"],16)
            check(start<=token<=end,"session token actual constructor interval")
            check(parse_blob(item["roots"]["session.lock"])==token.to_bytes(8,"big"),"actual session bytes")
        if item.get("kind")=="direct_write":
            start,end=item["interval_ms"];check(start//1000<=item["timestamp"]<=end//1000,"raw region timestamp clock interval")
            writes.append(item)
        if item.get("kind")=="full_save":
            full_saves.append(item)
            check(item["before"]["info_id"]==item["after"]["info_id"],"world save retains Info owner")
            check(item["before"]["metadata"]["last_played"]==item["after"]["metadata"]["last_played"],"save does not overwrite loaded LastPlayed field")
            if item["after_roots"] is not None:
                _,clock,_,_=raw_metadata(parse_blob(item["after_roots"]["level.dat"]))
                check(item["interval_ms"][0]<=clock<=item["interval_ms"][1],"saved LastPlayed actual world-save interval");clocks.add(clock)
        if item.get("kind")=="load_controls":
            check(item["primary_provider_state_unchanged"] and item["snapshot"]["queue_count"]==0,"finite loaded provider/queue evidence")
        if "roots" in item and isinstance(item["roots"],dict):
            for name,blob in item["roots"].items():
                if name.startswith("level.dat") and blob is not None:
                    _,clock,_,_=raw_metadata(parse_blob(blob));clocks.add(clock)
    actual=inventory(parent/"world")
    check(actual==record["result"]["final_files"],"complete final actual files")
    if record["phase"]=="prepare":
        check(len(full_saves)==4 and len(writes)==10,"explicit2dimension preparation routes")
        for name in actual:
            if not name.endswith(".mcr"):continue
            raw=read(parent/"world"/name);dimension=-1 if name.startswith("DIM-1/") else 0
            for index in range(1024):
                location=struct.unpack_from(">I",raw,index*4)[0]
                if location==0:continue
                timestamp=struct.unpack_from(">I",raw,4096+index*4)[0]
                eligible=[event for event in writes if event["dimension"]==dimension and
                    ((event["coordinates"][0]&31)+32*(event["coordinates"][1]&31))==index and
                    name.endswith(f"r.{event['coordinates'][0]>>5}.{event['coordinates'][1]>>5}.mcr")]
                if not eligible:eligible=[event for event in full_saves if event["dimension"]==dimension and event["label"]=="prepare-first-metadata-before-cache-save"]
                check(any(event["interval_ms"][0]//1000<=timestamp<=event["interval_ms"][1]//1000 for event in eligible),
                      "every preparation region timestamp has its actual write/save interval")
    if record["phase"]=="roundtrip":
        check(len(writes)==16 and len(full_saves)==2,"fixed2dimension directwrites/fullsave route")
        for name,event in (("level.dat",full_saves[-1]),("level.dat_old",full_saves[-2])):
            _,clock,_,_=raw_metadata(parse_blob(record["result"]["final_roots"][name]))
            check(event["interval_ms"][0]<=clock<=event["interval_ms"][1],"final current/old LastPlayed bound to its exact save interval")
            clocks.add(clock)
        for event in writes:
            x,z=event["coordinates"];path=parent/"world"
            if event["dimension"]==-1:path=path/"DIM-1"
            raw=read(path/"region"/f"r.{x>>5}.{z>>5}.mcr")
            timestamp=struct.unpack_from(">I",raw,4096+4*((x&31)+32*(z&31)))[0]
            check(timestamp==event["timestamp"],"final raw region clock equals the observed written cell")
        reload=next(e for e in record["events"] if e["kind"]=="genuine_reload")
        check(len(reload["direct_chunks"])==16,"all16 complete direct reload records")
        if baseline is not None:
            block_map=parse_blob(record["block_map"])
            for chunk in reload["direct_chunks"]:
                dimension=chunk["dimension"];x,z=chunk["coordinates"]
                arrays,flags=expected_loaded(baseline,block_map,dimension,x,z)
                check({role:fp(parse_blob(blob)) for role,blob in chunk["data"].items()}==arrays,
                      "genuine reload all5arrays exactly equal original/C-created baseline after actual postload map")
                check(chunk["flags"]==flags and chunk["minimum"]==0 and chunk["last_save"]=="0000000000000000",
                      "genuine direct reload transient defaults and persisted populated flag")
                final=disk_chunk(parent,dimension,x,z)
                check(level.child(final,"LastUpdate")["value"]==(1234 if dimension==0 else 5678),"actual writer records explicit live world time")
        check(sum(1 for e in walk(record) if isinstance(e,dict) and e.get("kind")=="direct_load")==32,"all32 direct slots proved before cache acquisitions")
        check(sum(1 for e in walk(record) if isinstance(e,dict) and e.get("kind")=="acquire")==24,"all24 finite genuine cache loads")
        if baseline is not None:
            _,clock,_,_=raw_metadata(read(baseline/"world/level.dat"));clocks.add(clock)
        for item in walk(record):
            if isinstance(item,dict) and isinstance(item.get("metadata"),dict) and "last_played" in item["metadata"]:
                check(int(item["metadata"]["last_played"],16) in clocks,"every loaded Info clock retains a measured metadata origin")
    return {"files":len(actual),"bytes":sum(x["bytes"] for x in actual.values()),"direct_writes":len(writes),"full_saves":len(full_saves),"baseline_exact_known_reloads":16 if record["phase"]=="roundtrip" else 0}

def compare_files(left,right,writes):
    a,b=inventory(left/"world"),inventory(right/"world");check(set(a)==set(b),"same complete control file inventory")
    changed=[];timestamp_ranges={}
    for item in writes:
        x,z=item["coordinates"];name=("DIM-1/" if item["dimension"]==-1 else "")+f"region/r.{x>>5}.{z>>5}.mcr"
        start=4096+4*((x&31)+32*(z&31));timestamp_ranges.setdefault(name,[]).append((start,start+4))
    for name in a:
        l,r=read(left/"world"/name),read(right/"world"/name)
        if name=="session.lock":
            check(len(l)==len(r)==8,"exact measured session tokens");changed.append({"file":name,"kind":"independently range-checked session tokens"});continue
        if name in ("level.dat","level.dat_old"):
            lb,lc,lr,_=raw_metadata(l);rb,rc,rr,_=raw_metadata(r)
            check(len(lb)==len(rb) and lr==rr and lb[:lr[0]]==rb[:rr[0]] and lb[lr[1]:]==rb[rr[1]:],"all metadata NBT bytes equal except explicit LastPlayed long")
            changed.append({"file":name,"kind":"LastPlayed raw clock only; gzip bytes retained separately","left":lc,"right":rc});continue
        check(len(l)==len(r),"same full region lengths")
        diffs=[i for i,(x,y) in enumerate(zip(l,r)) if x!=y]
        check(all(any(start<=i<end for start,end in timestamp_ranges.get(name,[])) for i in diffs),"all control region bytes equal outside written timestamp cells")
        if diffs:changed.append({"file":name,"kind":"independently range-checked written region timestamp bytes","offsets":diffs})
    return changed

def control_projection(record,parent):
    value=copy.deepcopy(record)
    def clean(item):
        if isinstance(item,list):return [clean(x) for x in item]
        if not isinstance(item,dict):return item
        result={}
        for key,sub in item.items():
            if key in ("interval_ms","session_token","timestamp","roots","before_roots","after_roots","chunk","region_before","region_after","final_files","final_roots","files","last_played","threads_before","threads_after"):continue
            if key=="mode":result[key]="same-call-control-pair"
            elif key=="loader_root":
                path=Path(sub);check(path.resolve().is_relative_to(parent.resolve()),"actual owned loader path");result[key]=path.relative_to(parent).as_posix()
            else:result[key]=clean(sub)
        return result
    return clean(value)

def bits(value):
    return f"{value & ((1 << 64) - 1):016x}"

def nbt(raw):
    reader = level.Reader(raw)
    tree = reader.tag()
    check(reader.p == len(raw), "complete NBT")
    return tree

def record_raw(parent, dim, x, z):
    path = parent / "world" / ("DIM-1/region" if dim == -1 else "region") / f"r.{x >> 5}.{z >> 5}.mcr"
    raw = read(path)
    index = (x & 31) + 32 * (z & 31)
    location = struct.unpack_from(">I", raw, index * 4)[0]
    start, count = (location >> 8) * 4096, location & 255
    check(start >= 8192 and count and start + count * 4096 <= len(raw), "actual region extent")
    length = struct.unpack_from(">I", raw, start)[0]
    check(1 <= length <= count * 4096 - 4 and raw[start + 4] in (1, 2), "actual record header")
    stream = zlib.decompressobj(31 if raw[start + 4] == 1 else 15)
    plain = stream.decompress(raw[start + 5:start + 4 + length], 2 * 1024 * 1024 + 1)
    check(stream.eof and not stream.unconsumed_tail and not stream.unused_data and len(plain) <= 2 * 1024 * 1024,
          "bounded complete compressed record")
    nbt(plain)
    return plain

def expected_info(parent):
    _, _, _, tree = m.raw_metadata(read(parent / "world/level.dat"))
    return t.info_projection(level.expected_info(level.child(tree, "Data")))

def packed(x, z):
    value = ((0x80000000 if x < 0 else 0) | ((x & 32767) << 16) |
             (32768 if z < 0 else 0) | (z & 32767))
    return value if value < (1 << 31) else value - (1 << 32)

def expected_chunk(baseline, block_map, dim, x, z, active=False):
    value = level.child(nbt(record_raw(baseline, dim, x, z)), "Level")
    arrays = {}
    for role, key, size in (("blocks", "Blocks", 32768), ("metadata", "Data", 16384),
                            ("skylight", "SkyLight", 16384), ("block_light", "BlockLight", 16384),
                            ("heightmap", "HeightMap", 256)):
        tag = level.child(value, key)
        check(tag["type"] == 7, "original typed byte array")
        raw = bytes.fromhex(tag["value"])
        check(len(raw) == size, "original complete byte array")
        if role == "blocks":
            raw = bytes(block_map[byte] for byte in raw)
        arrays[role] = {"length": size, "values": raw.hex()}
    arrays["minimum_height"] = 0
    check(not level.child(value, "Entities")["value"]["items"] and
          not level.child(value, "TileEntities")["value"]["items"], "starter empty opaque records")
    return {"coordinates": [x, z], "blank": False,
            "flags": {"c": active, "n": level.child(value, "TerrainPopulated")["value"] != 0,
                      "o": False, "p": False, "q": False},
            "last_save_time": bits((1234 if dim == 0 else 5678) if active else 0), "data": arrays}

def preservation(before, parent, slots, metadata_write=False):
    """Every preexisting byte outside selected header/payload extents must survive."""
    after = inventory(parent / "world")
    if not slots and not metadata_write:
        check(set(before) - {"session.lock"} == set(after) - {"session.lock"}, "read-only complete non-session file set retained")
    changed = []
    for name, old in before.items():
        if name == "session.lock" or ((slots or metadata_write) and name in ("level.dat", "level.dat_old")):
            continue
        check(name in after, "preexisting file retained")
        now = read(parent / "world" / name)
        if not name.endswith(".mcr"):
            check(old == now, "unrelated raw file retained")
            continue
        affected = [(x, z) for dim, x, z in slots if name ==
                    ("DIM-1/" if dim == -1 else "") + f"region/r.{x >> 5}.{z >> 5}.mcr"]
        if not affected:
            check(old == now, "unrelated whole region retained")
            continue
        permitted = bytearray(len(old))
        for x, z in affected:
            index = (x & 31) + 32 * (z & 31)
            for start in (index * 4, 4096 + index * 4):
                permitted[start:start + 4] = b"\1" * 4
            location=struct.unpack_from(">I",now,index*4)[0]
            start,count=(location>>8)*4096,location&255
            check(start>=8192 and count and start+count*4096<=len(now),"complete selected written allocation")
            length=struct.unpack_from(">I",now,start)[0]
            check(1<=length and length+4<=count*4096,"complete selected written record")
            # Only the new record prefix is written; old payload suffixes and
            # sector padding remain observable even inside selected allocations.
            end=min(len(old),start+4+length)
            if start<end:permitted[start:end]=b"\1"*(end-start)
        check(len(now) >= len(old) and all(a == b or permitted[i] for i, (a, b) in enumerate(zip(old, now))),
              "unrelated header cells, sectors and dead bytes retained")
        changed.append({"file": name, "old_bytes": len(old), "new_bytes": len(now),
                        "outside_selected_ranges_equal": True})
    return changed

def validate_live_info(actual, source, physical, create=False):
    expected = copy.deepcopy(source)
    expected["version"] = 19132
    expected["size_on_disk"] = bits((0 if create else int(source["size_on_disk"], 16)) + physical)
    check(t.same(actual, expected), "every retained live Info field equals source with only explicit version/size updates; loaded LastPlayed retained")
    return expected

def validate_written_disk(parent, baseline, block_map, live_info):
    disk_info = copy.deepcopy(live_info)
    disk_info["last_played"] = bits(CLOCK)
    check(t.same(expected_info(parent), disk_info), "all serialized metadata fields exact; only explicit version/size/saved clock differ")
    for dim, x, z in SLOTS:
        path = parent / "world" / ("DIM-1/region" if dim == -1 else "region") / f"r.{x >> 5}.{z >> 5}.mcr"
        raw = read(path)
        timestamp=struct.unpack_from(">I",raw,4096+4*((x&31)+32*(z&31)))[0]
        validate_written_chunk(nbt(record_raw(parent,dim,x,z)),nbt(record_raw(baseline,dim,x,z)),block_map,
                               dim,x,z,1234 if dim==0 else 5678,timestamp,TIMESTAMP)


def validate_written_chunk(stored_tree, source_tree, block_map, dimension, x, z,
                           world_time, actual_timestamp, expected_timestamp):
    check(type(block_map) is bytes and len(block_map)==256,"actual 256-byte post-load map")
    check(type(dimension) is int and dimension in (0,-1) and all(type(v) is int and -(1<<31)<=v<(1<<31) for v in (x,z)),"exact storage coordinates")
    check(type(world_time) is int and -(1<<63)<=world_time<(1<<63),"explicit signed world time")
    check(type(actual_timestamp) is type(expected_timestamp) is int and 0<=expected_timestamp<=0xffffffff and actual_timestamp==expected_timestamp,"exact selected raw u32 timestamp")
    source,stored=level.child(source_tree,"Level"),level.child(stored_tree,"Level")
    for key,expected in (("xPos",x),("zPos",z)):
        tag=level.child(stored,key);check(tag is not None and type(tag["type"]) is int and tag["type"]==3 and type(tag["value"]) is int and tag["value"]==expected,"stored exact typed coordinates")
    stamp=level.child(stored,"LastUpdate");check(stamp is not None and type(stamp["type"]) is int and stamp["type"]==4 and type(stamp["value"]) is int and stamp["value"]==world_time,"stored explicit LastUpdate")
    terrain=level.child(stored,"TerrainPopulated");expected=level.child(source,"TerrainPopulated")["value"]!=0
    check(terrain is not None and type(terrain["type"]) is int and terrain["type"]==1 and type(terrain["value"]) is int and terrain["value"]==int(expected),"stored populated byte")
    for key,size in (("Blocks",32768),("Data",16384),("SkyLight",16384),("BlockLight",16384),("HeightMap",256)):
        original,value=level.child(source,key),level.child(stored,key)
        check(original is not None and value is not None and type(original["type"]) is type(value["type"]) is int and original["type"]==value["type"]==7,"complete stored byte arrays")
        expected=bytes.fromhex(original["value"])
        if key=="Blocks":expected=bytes(block_map[byte] for byte in expected)
        check(len(expected)==size and value["value"]==expected.hex(),"raw written "+key+" equals map-once original source")


def validate_chunk_snapshot(value):
    r.keys(value,("id","dimension","coordinates","world_matches","data","minimum","flags","last_save","tiles","entities"),"complete starter chunk snapshot")
    check(type(value["id"]) is int and 0<=value["id"]<=4096 and type(value["dimension"]) is int and value["dimension"] in (0,-1),"bounded exact chunk identity/dimension")
    check(type(value["coordinates"]) is list and len(value["coordinates"])==2 and all(type(v) is int and -(1<<31)<=v<(1<<31) for v in value["coordinates"]),"exact chunk coordinates")
    check(value["world_matches"] is True and type(value["minimum"]) is int and 0<=value["minimum"]<=128,"actual world owner and height minimum")
    r.keys(value["flags"],tuple("cnopq"),"complete starter chunk flags")
    check(all(type(v) is bool for v in value["flags"].values()),"exact boolean chunk flags")
    check(type(value["last_save"]) is str and re.fullmatch(r"[0-9a-f]{16}",value["last_save"]) is not None,"exact raw r bits")
    check(type(value["tiles"]) is int and value["tiles"]==0 and type(value["entities"]) is list and len(value["entities"])==8 and all(type(v) is int and v==0 for v in value["entities"]),"empty opaque starter only")
    r.keys(value["data"],("blocks","metadata","skylight","block_light","heightmap"),"complete chunk arrays")
    for role,size in (("blocks",32768),("metadata",16384),("skylight",16384),("block_light",16384),("heightmap",256)):
        check(len(parse_blob(value["data"][role]))==size,"complete original "+role)
    return value


def validate_state(value, parent=None):
    r.keys(value,("dimension","world_id","info_id","storage_id","metadata","map","insertions","cached_headers","pending","queue_count","loader_root","no_sky"),"complete actual world/cache state")
    check(type(value["dimension"]) is int and value["dimension"] in (0,-1),"exact provider dimension")
    check(all(type(value[key]) is int and 0<=value[key]<=4096 for key in ("world_id","info_id","storage_id")) and
          len({value[key] for key in ("world_id","info_id","storage_id")})==3,"distinct actual world/info/storage owners")
    check(value["no_sky"] is (value["dimension"]==-1) and type(value["queue_count"]) is int and 0<=value["queue_count"]<=131072,"provider no-sky and bounded actual light queue")
    info=value["metadata"]
    r.keys(info,("seed","spawn_x","spawn_y","spawn_z","time","last_played","size_on_disk","player","player_dimension","level_name","version","raining","rain_time","thundering","thunder_time"),"complete actual Info snapshot")
    for key in ("seed","time","last_played","size_on_disk"):check(type(info[key]) is str and re.fullmatch(r"[0-9a-f]{16}",info[key]) is not None,"raw Info long bits")
    for key in ("spawn_x","spawn_y","spawn_z","player_dimension","version","rain_time","thunder_time"):check(type(info[key]) is int and -(1<<31)<=info[key]<(1<<31),"exact Info integer")
    check(info["player"] is False and type(info["level_name"]) is str and all(type(info[key]) is bool for key in ("raining","thundering")),"empty player starter and exact Info fields")
    check(type(value["map"]) is list and type(value["insertions"]) is list and type(value["cached_headers"]) is list and
          len(value["map"])==len(value["insertions"])==len(value["cached_headers"])<=64 and value["pending"]==[],"finite complete cache graph")
    headers=value["cached_headers"];ids=[];expected={}
    for row in headers:
        r.keys(row,("id","coordinates","flags","minimum","last_save"),"complete cached header")
        check(type(row["id"]) is int and 0<=row["id"]<=4096 and row["id"] not in ids,"distinct cached identity");ids.append(row["id"])
        coords=row["coordinates"];check(type(coords) is list and len(coords)==2 and all(type(v) is int and -(1<<31)<=v<(1<<31) for v in coords),"cached exact coordinates")
        r.keys(row["flags"],tuple("cnopq"),"complete cached flags");check(all(type(v) is bool for v in row["flags"].values()),"exact cached flags")
        check(type(row["minimum"]) is int and 0<=row["minimum"]<=128 and type(row["last_save"]) is str and re.fullmatch(r"[0-9a-f]{16}",row["last_save"]) is not None,"cached minimum/r bits")
        expected[packed(*coords)]=row["id"]
    check(t.same(ids,value["insertions"]),"exact cached insertion/header identity order")
    actual={}
    for row in value["map"]:
        check(type(row) is list and len(row)==2 and all(type(v) is int for v in row) and row[0] not in actual,"exact unique raw packed cache entries");actual[row[0]]=row[1]
    check(t.same(actual,expected),"actual packed cache keys resolve matching retained identities")
    check(type(value["loader_root"]) is str,"actual loader root path")
    if parent is not None:
        expected_root=Path(parent)/"world"
        if value["dimension"]==-1:expected_root=expected_root/"DIM-1"
        check(Path(value["loader_root"]).resolve()==expected_root.resolve(),"actual exact dimension region root")
    return value


def validate_interval(value):
    check(type(value) is list and len(value)==2 and all(type(v) is int and 0<=v<(1<<63) for v in value) and
          value[0]<=value[1] and value[1]-value[0]<=120000,"bounded exact measured clock interval")
    return value


def original_info(tree):
    source=level.expected_info(level.child(tree,"Data"))
    return {"seed":source["random_seed_hex"],"spawn_x":source["spawn"][0],"spawn_y":source["spawn"][1],"spawn_z":source["spawn"][2],
            "time":source["time_hex"],"last_played":source["last_played_hex"],"size_on_disk":source["size_on_disk_hex"],
            "player":source["player_present"],"player_dimension":source["dimension"],"level_name":bytes.fromhex(source["level_name_utf16"]).decode("utf-16be","surrogatepass"),
            "version":source["version"],"raining":source["raining"],"rain_time":source["rain_time"],"thundering":source["thundering"],"thunder_time":source["thunder_time"]}


def validate_original_info(actual,source,world_time=None,physical_growth=0,version=None):
    check(type(source) is dict and type(actual) is dict,"complete original Info values")
    check(type(physical_growth) is int and 0<=physical_growth<=64*1024*1024,"independent physical region growth")
    expected=copy.deepcopy(source)
    expected["size_on_disk"]=bits(int(source["size_on_disk"],16)+physical_growth)
    if world_time is not None:
        check(type(world_time) is int and -(1<<63)<=world_time<(1<<63),"explicit original world-time API value");expected["time"]=bits(world_time)
    if version is not None:
        check(type(version) is int and -(1<<31)<=version<(1<<31),"explicit original save version");expected["version"]=version
    check(t.same(actual,expected),"every original Info field bound to loaded source and explicit time/physical-growth/version transitions")
    return expected


def validate_saved_original_info(tree,live_info,interval):
    """Bind metadata-first serialization to pre-save live Info, including size lag."""
    validate_interval(interval)
    check(type(tree) is dict and type(tree.get("type")) is int and tree["type"]==10,"serialized named metadata root")
    data=level.child(tree,"Data");check(data is not None and type(data["type"]) is int and data["type"]==10,"serialized metadata Data compound")
    for name,kind in (("RandomSeed",4),("SpawnX",3),("SpawnY",3),("SpawnZ",3),("Time",4),("LastPlayed",4),
                      ("SizeOnDisk",4),("LevelName",8),("version",3),("raining",1),("rainTime",3),("thundering",1),("thunderTime",3)):
        tag=level.child(data,name)
        check(tag is not None and type(tag["type"]) is int and tag["type"]==kind,"exact serialized known metadata field type: "+name)
        if kind!=8:check(type(tag["value"]) is int,"exact serialized metadata numeric value")
        if name in ("raining","thundering"):check(tag["value"] in (0,1),"canonical serialized metadata boolean byte")
    saved=original_info(tree);clock=int(saved["last_played"],16)
    check(interval[0]<=clock<=interval[1],"serialized LastPlayed belongs to this exact full-save interval")
    expected=copy.deepcopy(live_info);expected["last_played"]=bits(clock);expected["version"]=19132
    check(t.same(saved,expected),"all serialized original Info fields equal pre-save live values except sampled LastPlayed/save version")
    return saved


def validate_stage_chunk(snapshot, baseline, block_map, dimension, x, z, stage, expected_id=None):
    check(stage in ("direct_load","acquire","direct_write"),"known snapshot stage")
    check(type(dimension) is int and dimension in (0,-1) and all(type(value) is int and -(1<<31)<=value<(1<<31) for value in (x,z)),"exact observed stage dimension/coordinates")
    validate_chunk_snapshot(snapshot)
    active=stage=="acquire" or (stage=="direct_write" and (x,z) in COORDS[:6])
    expected=expected_chunk(baseline,block_map,dimension,x,z,active)
    actual={"coordinates":snapshot["coordinates"],"blank":False,"flags":snapshot["flags"],
            "last_save_time":snapshot["last_save"],"data":{role:{"length":blob["bytes"],"values":blob["hex"]} for role,blob in snapshot["data"].items()}}
    actual["data"]["minimum_height"]=snapshot["minimum"]
    check(snapshot["dimension"]==dimension and t.same(actual,expected),"every observed "+stage+" payload/transient equals exact original disk/stage")
    if expected_id is not None:check(type(expected_id) is int and snapshot["id"]==expected_id,"stable same-stage chunk identity")
    return snapshot["id"]

def snapshot_files(parent):
    return {path: read(parent / "world" / path) for path in inventory(parent / "world")}

def exercise(target, parent, baseline, create=False, write=True):
    before = snapshot_files(parent) if (parent / "world").exists() else {}
    info = expected_info(baseline)
    if create:
        _, _, _, tree = m.raw_metadata(read(baseline / "world/level.dat"))
        level.child(level.child(tree, "Data"), "SizeOnDisk")["value"] = 0
        commands = [f"INIT_INFO {bits(CLOCK)} {level.encode(tree).hex()}"]
    else:
        commands = ["SESSION", "LOAD_INFO"]
    for dim, x, z in SLOTS:
        commands.append((f"RETAIN {dim} {record_raw(baseline, dim, x, z).hex()}" if create else f"LOAD {dim} {x} {z}"))
    commands += ["CACHE_RESET"]
    for dim in (0, -1):
        if dim == -1:
            commands.append("CACHE_RESET")
        commands += [f"ACQUIRE {dim} {x} {z} {bits(1234 if dim == 0 else 5678)}" for x, z in COORDS[:6]] if not create else []
    if write:
        commands += [f"WRITE {i + 1} {bits(1234 if dim == 0 else 5678)} {TIMESTAMP}" for i, (dim, x, z) in enumerate(SLOTS)]
        commands += [f"SAVE_INFO {bits(CLOCK)}"]
    commands += ["CHUNK 1", "INFO", "CHECK"]
    values = c_batch(target, parent, commands, "c-write" if write else "c-readback")
    handles = 0
    graphs = []
    final_info = None
    growth = 0
    for command, value in values:
        name = command.split()[0]
        if name == "LOAD_INFO":
            check(value["info"] == info, "C loaded metadata equals actual original disk fields")
        if name in ("LOAD", "RETAIN", "ACQUIRE"):
            handles += 1
            if name == "RETAIN":
                dim, x, z = SLOTS[handles - 1]
            else:
                dim, x, z = map(int, command.split()[1:4])
            check(value["handle"] == handles and value["chunk"] == expected_chunk(baseline, BLOCK_MAP, dim, x, z, name == "ACQUIRE"),
                  "C full chunk arrays, persisted flag and stage transient state from frozen original disk")
            if name == "ACQUIRE":
                if not graphs or graphs[-1][0] != dim:
                    graphs.append((dim, []))
                entries = graphs[-1][1]
                entries.append([packed(x, z), handles])
                expected = {"status": 0, "loader_status": 0, "entries": entries, "insertions": [row[1] for row in entries],
                            "pending": [], "callbacks": [{"kind": "tiles", "handle": handles, "active": True, "slice": None}] +
                            [{"kind": "entities", "handle": handles, "active": True, "slice": i} for i in range(8)]}
                check(value["acquire"] == expected, "finite load-aware acquisition graph and nine activation callbacks")
        if name == "WRITE":
            handle = int(command.split()[1]);dim, x, z = SLOTS[handle - 1]
            check(value["chunk"] == expected_chunk(baseline, BLOCK_MAP, dim, x, z) and value["handle"] == handle,
                  "write retains source arrays and every transient state field")
            growth += value["growth"]
        if name == "CHUNK":
            check(value["handle"] == 1 and value["chunk"] == expected_chunk(baseline, BLOCK_MAP, *SLOTS[0]),
                  "retained source stable after all later loads/acquisitions/writes")
        if name == "INFO":
            final_info = value["info"]
    after = inventory(parent / "world")
    if write:
        physical = sum(row["bytes"] for name, row in after.items() if name.endswith(".mcr")) - sum(len(raw) for name, raw in before.items() if name.endswith(".mcr"))
        check(growth == physical, "actual explicit growth equals independent physical byte delta")
        live_info = validate_live_info(final_info, info, physical, create)
        validate_written_disk(parent, baseline, BLOCK_MAP, live_info)
    return {"loads_or_retains": 16, "acquisitions": 0 if create else 12, "writes": 16 if write else 0,
            "before": {name: fp(raw) for name, raw in before.items()}, "after": after,
            "physical_growth": growth, "preservation": preservation(before, parent, SLOTS if write else [])}

def originals(target, scenario, immutable, source):
    for side in ("client", "server") if scenario == "created" else (source,):
        stem = f"{target}-{scenario}-{side}"
        rows, parents = {}, {}
        before = inventory(immutable / "world")
        for mode in ("observed", "control"):
            parent = CAPTURE / (stem + "-" + mode)
            parent.mkdir()
            shutil.copytree(immutable / "world", parent / "world")
            output = parent / "observation.json"
            command = [JAVA, "-Xmx512m", "-cp", CLASSES, "WorldStorageObservation",
                       JARS[side], side,
                       "roundtrip", mode, parent, output]
            run("original", command, parent, cap=32768)
            record = level.strict_json(read(output))
            summary = m.validate(record, parent, immutable)
            rows[mode], parents[mode] = record, parent
            REPORT["originals"].append({"target": target, "scenario": scenario, "side": side, "mode": mode,
                                        "record": fp(read(output)), "summary": summary,
                                        "before": before, "after": inventory(parent / "world")})
        check(m.control_projection(rows["observed"], parents["observed"]) == m.control_projection(rows["control"], parents["control"]), "same-call control projection diagnostic")
        for suffix in ("stdout", "stderr"):
            check(read(parents["observed"] / ("original." + suffix)) == read(parents["control"] / ("original." + suffix)), "raw original same-call stream equality")
        writes = [event for event in rows["observed"]["events"] if event.get("kind") == "direct_write"]
        differences = m.compare_files(parents["observed"], parents["control"], writes)
        check(inventory(immutable / "world") == before, "immutable C result retained")
        check(BLOCK_MAP == m.parse_blob(rows["observed"]["block_map"]), "actual original registry map retained")
        readback = CAPTURE / (stem + "-readback")
        readback.mkdir()
        shutil.copytree(parents["observed"] / "world", readback / "world")
        summary = exercise(target, readback, parents["observed"], write=False)
        REPORT["roundtrips"].append({"target": target, "scenario": scenario, "side": side,
                                     "control_diagnostic_equal": True, "clock_only_file_differences": differences,
                                     "fresh_C_readback": summary})

def canon(tree):
    return level.order_only(tree)

def arrays(tree, normalize=False):
    value = level.child(tree, "Level")
    output = {}
    for role, key in (("blocks", "Blocks"), ("metadata", "Data"), ("skylight", "SkyLight"),
                      ("block_light", "BlockLight"), ("heightmap", "HeightMap")):
        raw = bytes.fromhex(level.child(value, key)["value"])
        if normalize and role == "blocks":
            raw = bytes(a.BLOCK_MAP[byte] for byte in raw)
        output[role] = {"length": len(raw), "values": raw.hex()}
    output["minimum_height"] = 0
    return output

def projected_chunk(tree):
    value = level.child(tree, "Level")
    return {"coordinates": [1, 0], "blank": False, "flags": {"c": False,
            "n": level.child(value, "TerrainPopulated")["value"] != 0, "o": False, "p": False, "q": True},
            "last_save_time": "0000000000000000", "data": arrays(tree, True)}

def C_opaque(target, parent, tree):
    before = a.snapshot_files(parent)
    old_info = a.expected_info(parent)
    commands = ["LOAD_INFO", "LOAD 0 1 0", f"WRITE 1 {a.bits(1234)} {a.TIMESTAMP}",
                "CHUNK 1", f"SAVE_INFO {a.bits(a.CLOCK)}", "INFO", "CHECK"]
    values = a.c_batch(target, parent, commands, "opaque-C")
    expected_chunk = projected_chunk(tree)
    growth = None
    for command, value in values:
        if command.startswith(("LOAD 0", "WRITE", "CHUNK")):
            check(value["handle"] == 1 and value["chunk"] == expected_chunk,
                  "generic C transport all arrays/default flags/hasEntities and stable identity exact; no activation")
        if command.startswith("WRITE"):
            growth = value["growth"]
    after_tree = a.nbt(a.record_raw(parent, 0, 1, 0))
    expected_tree = copy.deepcopy(tree)
    chunk = level.child(expected_tree, "Level")
    level.child(chunk, "Blocks")["value"] = expected_chunk["data"]["blocks"]["values"]
    level.child(chunk, "LastUpdate")["value"] = 1234
    check(canon(after_tree) == canon(expected_tree), "all typed opaque root/Level/Boat/Sign NBT retained byte-values, exact types and list order")
    region = read(parent / "world/region/r.0.0.mcr")
    check(struct.unpack_from(">I", region, 4100)[0] == a.TIMESTAMP, "exact C opaque selected region timestamp")
    expected_disk = copy.deepcopy(old_info)
    expected_disk.update(version=19132, last_played=a.bits(a.CLOCK),
                         size_on_disk=a.bits(int(old_info["size_on_disk"], 16) + growth))
    check(a.expected_info(parent) == expected_disk, "all opaque-container metadata known fields exact explicit save changes")
    return {"before": {name: fp(raw) for name, raw in before.items()}, "after": inventory(parent / "world"),
            "growth": growth, "typed_tree_retained": True, "raw_input": fp(level.encode(tree)),
            "raw_output": fp(a.record_raw(parent, 0, 1, 0)),
            "preservation": a.preservation(before, parent, [(0, 1, 0)])}

def original_chunk(value, expected_data, entity_class, tile_class):
    r.keys(value,("id","coordinates","world_matches","data","flags","minimum","last_save","entities","tiles"),"complete original opaque chunk")
    check(type(value["id"]) is int and 0<=value["id"]<=4096,"finite opaque returned identity")
    for blob in value["data"].values():parse_blob(blob)
    check(t.same(value["coordinates"],[1,0]) and value["world_matches"] is True and type(value["minimum"]) is int and value["minimum"]==0,
          "exact typed original opaque coordinates/owner/minimum")
    check(t.same(value["flags"],{"c":False,"n":False,"o":False,"p":False,"q":True}),"exact typed original opaque flags")
    check(type(value["entities"]) is list and type(value["tiles"]) is list and len(value["entities"])==len(value["tiles"])==1,"finite complete original opaque actor inventories")
    entity,tile=value["entities"][0],value["tiles"][0]
    r.keys(entity,("slice","class","id"),"complete original entity row");r.keys(tile,("class","id","key_class","position"),"complete original tile row")
    check(type(entity["id"]) is int and type(tile["id"]) is int and 0<=entity["id"]<=4096 and 0<=tile["id"]<=4096 and
          len({value["id"],entity["id"],tile["id"]})==3,"distinct actual chunk/entity/tile identities")
    check(type(entity["slice"]) is int and entity["slice"]==4 and entity["class"]==entity_class and tile["class"]==tile_class,"exact original actor class/slice")
    expected_key={"yk":"wf","pq":"oa"}.get(tile_class)
    check(expected_key is not None and tile["key_class"]==expected_key and t.same(tile["position"],{"a":0,"b":64,"c":0}),"exact original Sign local-position key")
    check(value["coordinates"] == [1, 0] and value["world_matches"], "actual loader coordinates/world owner")
    check({role: {"length": blob["bytes"], "values": blob["hex"]} for role, blob in value["data"].items()} ==
          {key: val for key, val in expected_data.items() if key != "minimum_height"}, "genuine factory load all arrays exact")
    check(value["minimum"] == 0 and value["last_save"] == "0000000000000000" and
          value["flags"] == {"c": False, "n": False, "o": False, "p": False, "q": True}, "genuine direct loader transient defaults, populated false and hasEntities true")
    check(len(value["entities"]) == len(value["tiles"]) == 1 and value["entities"][0]["class"] == entity_class and
          value["entities"][0]["slice"] == 4 and value["tiles"][0]["class"] == tile_class,
          "finite original Boat/Sign factories instantiate expected concrete types")


def validate_opaque_record(record,parent,side,mode,expected_data,entity_class,tile_class):
    r.keys(record,("schema","scope","side","mode","jar_sha256","class_origins","bindings","fields","passes"),"complete opaque original observation")
    check(type(record["schema"]) is int and record["schema"]==1 and record["side"]==side and record["mode"]==mode and
          record["jar_sha256"]==PIN[side],"exact opaque record context")
    expected_origin=JARS[side].resolve().as_uri().replace("file:///","file:/",1)
    check(type(record["class_origins"]) is dict and entity_class in record["class_origins"] and tile_class in record["class_origins"] and
          all(value==expected_origin for value in record["class_origins"].values()),"actual opaque factory classes from supplied archive")
    bindings,fields=expected_bindings(side,"roundtrip");client=side=="client"
    del bindings[("ok.c(II)Llm;" if client else "jb.c(II)Lhi;")]
    del bindings[("wd.<init>()V" if client else "nz.<init>()V")]
    del fields[("ok.b" if client else "jb.b")]
    del fields[("vi.a" if client else "nn.a")]
    check(t.same(record["bindings"],bindings) and t.same(record["fields"],fields),"exact opaque original runtime descriptors/modifiers")
    with zipfile.ZipFile(JARS[side]) as archive:names={name[:-6].replace("/",".") for name in archive.namelist() if name.endswith(".class")}
    check(type(record["passes"]) is list and len(record["passes"])==2,"one direct write then one fresh factory reload")
    for index,item in enumerate(record["passes"]):
        r.keys(item,("pass","before","after","write_interval_ms","events","files","roots","threads"),"complete opaque pass")
        check(type(item["pass"]) is int and item["pass"]==index,"ordered opaque pass")
        if index==0:validate_interval(item["write_interval_ms"])
        else:check(item["write_interval_ms"] is None,"fresh opaque reload performs no second direct write")
        validate_threads(item["threads"],names)
        check([event.get("kind") for event in item["events"]]==(["handler","world","time","full_save","close_regions"] if index==0 else ["handler","world","time","close_regions"]),"exact ordered opaque original event route")
        original_chunk(item["before"],expected_data,entity_class,tile_class);original_chunk(item["after"],expected_data,entity_class,tile_class)
        check(t.same(item["before"],item["after"]),"original generic writer retains every loaded field and identity")
        clock=raw_metadata(parse_blob(item["events"][0]["roots"]["level.dat"]))[1]
        source_info=original_info(raw_metadata(parse_blob(item["events"][0]["roots"]["level.dat"]))[3]);world_time=None
        for event in item["events"]:
            if event["kind"]=="handler":validate_handler(event)
            if "interval_ms" in event:validate_interval(event["interval_ms"])
            if event["kind"]=="time":check(event["value"]==bits(1234) and event["descriptor"]=="(J)V","exact opaque world time stage");world_time=1234
            for value in walk(event):
                if isinstance(value,dict) and "metadata" in value and "world_id" in value:
                    validate_state(value,parent)
                    validate_original_info(value["metadata"],source_info,world_time)
                    check(value["map"]==value["insertions"]==value["cached_headers"]==[] and value["queue_count"]==0,"direct opaque route never activates/cache-publishes or finishes")
                if isinstance(value,dict) and "last_played" in value and "seed" in value:check(value["last_played"]==bits(clock),"opaque Info retains its exact loaded clock")
            if event.get("kind")=="full_save":
                saved_raw=parse_blob(event["after_roots"]["level.dat"]) if mode=="observed" else parse_blob(item["roots"]["level.dat"])
                actual=raw_metadata(saved_raw)[1]
                check(event["interval_ms"][0]<=actual<=event["interval_ms"][1],"opaque serialized metadata clock exact save interval")
                validate_saved_original_info(raw_metadata(saved_raw)[3],event["before"]["metadata"],event["interval_ms"])
        check(sum(event.get("kind")=="full_save" for event in item["events"])==(1 if index==0 else 0),"fixed opaque metadata save count")
    check(inventory(parent/"world")==record["passes"][-1]["files"],"opaque complete final world inventory")
    return record


def validate_handler(event):
    r.keys(event,("kind","interval_ms","session_token","roots"),"complete actual handler entry")
    validate_interval(event["interval_ms"])
    check(event["kind"]=="handler" and type(event["session_token"]) is str and re.fullmatch(r"[0-9a-f]{16}",event["session_token"]) is not None,"exact handler session token")
    token=int(event["session_token"],16);check(event["interval_ms"][0]<=token<=event["interval_ms"][1],"constructor token exact measured clock interval")
    check(parse_blob(event["roots"]["session.lock"])==token.to_bytes(8,"big"),"actual handler session-lock bytes")


def validate_opaque_preparation(record,parent,side,mode,raw_input):
    r.keys(record,("schema","scope","side","mode","jar_sha256","class_origins","bindings","fields","threads","input","write_interval_ms","events","files","roots"),"complete original opaque preparation")
    check(type(record["schema"]) is int and record["schema"]==1 and record["side"]==side and record["mode"]==mode and mode in ("prepare","metadata") and record["jar_sha256"]==PIN[side],"exact opaque preparation context")
    check(parse_blob(record["input"])==raw_input,"actual original supplied raw fixture bytes")
    validate_interval(record["write_interval_ms"])
    origin=JARS[side].resolve().as_uri().replace("file:///","file:/",1)
    check(type(record["class_origins"]) is dict and len(record["class_origins"])>=20 and all(value==origin for value in record["class_origins"].values()),"all preparation classes from supplied archive")
    client=side=="client";w,l,p,c,n,u,g=("fd","ld","wd","lm","nu","as","rj") if client else ("dj","hb","nz","hi","iq","ag","kq")
    bindings,fields=expected_bindings(side,"roundtrip")
    for key in (w+".a(J)V",w+".a(ZL"+("yb" if client else "pj")+";)V",p+".<init>()V",l+".a(L"+w+";II)L"+c+";",l+".a(L"+w+";L"+c+";)V",("ok.c(II)Llm;" if client else "jb.c(II)Lhi;")):del bindings[key]
    for key in list(fields):
        if key.startswith(c+".") or key in (("ok.b" if client else "jb.b"),("vi.a" if client else "nn.a"),("wi.a" if client else "ob.a")):del fields[key]
    if mode=="metadata":
        allowed_owners={"hi" if client else "ie"};bindings={key:value for key,value in bindings.items() if value["class"] in allowed_owners}
        fields={key:value for key,value in fields.items() if key in (("fm.e" if client else "dq.e"),g+".a")}
    def method(owner,name,desc):bindings[owner+"."+name+desc]={"class":owner,"name":name,"descriptor":desc,"modifiers":9}
    method(u,"a","(Ljava/io/DataInput;)L"+n+";")
    method(u,"a","(L"+n+";"+("Ljava/io/OutputStream;" if mode=="metadata" else "Ljava/io/DataOutput;")+")V")
    if mode=="prepare":method(g,"d","(Ljava/io/File;II)Ljava/io/DataOutputStream;")
    check(t.same(record["bindings"],bindings) and t.same(record["fields"],fields),"exact original opaque preparation descriptors/modifiers")
    with zipfile.ZipFile(JARS[side]) as archive:names={name[:-6].replace("/",".") for name in archive.namelist() if name.endswith(".class")}
    validate_threads(record["threads"],names)
    check([event.get("kind") for event in record["events"]]==(["handler","world","close_regions"] if mode=="prepare" else ["handler","close_regions"]),"exact original opaque preparation route")
    validate_handler(record["events"][0])
    check(inventory(parent/"world")==record["files"],"complete actual preparation files")
    for name,blob in record["roots"].items():
        path=parent/"world"/name
        check((blob is None and not path.exists()) or (blob is not None and parse_blob(blob)==read(path)),"exact preparation root artifact")
    return record

def metadata_unknown(target, source):
    parent = CAPTURE / (target + "-metadata-opaque-C")
    parent.mkdir()
    shutil.copytree(source / "world", parent / "world")
    # Actual original generic-NBT-written metadata; Info save would drop unknown root/Data fields.
    raw = read(source / "world/level.dat")
    tree = m.raw_metadata(raw)[3]
    check(level.child(level.child(tree, "Data"), "Player") is not None and
          level.child(tree, "UnknownRoot") is not None, "actual original fixture Player+unknown source")
    before = a.snapshot_files(parent)
    old_info = a.expected_info(parent)
    commands = ["LOAD_INFO", f"SAVE_INFO {a.bits(a.CLOCK)}", "INFO", "CHECK"]
    values = a.c_batch(target, parent, commands, "metadata-C")
    for command, value in values:
        if command == "LOAD_INFO":
            check(value["info"] == old_info, "all C opaque loaded metadata fields match actual fixture")
        if command == "INFO":
            expected = copy.deepcopy(old_info);expected["version"] = 19132
            check(value["info"] == expected, "C live metadata retains original loaded LastPlayed and all fields")
    expected_tree = copy.deepcopy(tree)
    data = level.child(expected_tree, "Data")
    level.child(data, "version")["value"] = 19132
    level.child(data, "LastPlayed")["value"] = a.CLOCK
    # Known boolean fields have an explicit decoded-bool -> byte1 encode policy.
    level.child(data, "raining")["value"] = 1
    level.child(data, "thundering")["value"] = 1
    actual_tree = m.raw_metadata(read(parent / "world/level.dat"))[3]
    check(canon(actual_tree) == canon(expected_tree), "C LOAD_INFO/SAVE_INFO preserves complete typed Player and unknown root/Data state")
    check(read(parent / "world/level.dat_old") == raw, "metadata rollover retains exact raw original current as old")
    return {"target": target, "scope": "C generic metadata transport only; no original player activation",
            "before": {name: fp(value) for name, value in before.items()}, "after": inventory(parent / "world"),
            "typed_opaque_retained": True, "old_file_exact": True,
            "preservation": a.preservation(before, parent, [], metadata_write=True)}

def opaque_roundtrips(report, classpath):
    classes = CLASSES
    fixtures = t.fixture_inputs()
    for side, level_record, files, chunk_record, artifacts in fixtures:
        original_case = next(row for row in chunk_record["cases"] if row["name"] == "valid_boat_and_sign")
        input_raw = artifacts[original_case["input"]["artifact"]]
        expected_raw = artifacts[original_case["region"]["output_root"]["artifact"]]
        input_tree, expected_tree = a.nbt(input_raw), a.nbt(expected_raw)
        before_chunk = original_case["region"]["chunk_before_write"]
        entity_class = next(row["entries"][0]["class"] for row in before_chunk["entity_slices"] if row["entries"])
        tile_class = before_chunk["tile_map"]["entries"][0]["object"]["class"]
        preparation = CAPTURE / (side + "-preparation");preparation.mkdir();shutil.copytree(a.PRODUCER / (side + "-preparation/world"), preparation / "world")
        (preparation / ".opaque-world-owned").write_bytes(b"private copied opaque world\n")
        raw_path = preparation / "fixture-input.nbt";raw_path.write_bytes(input_raw)
        out = preparation / "observation.json"
        jar = JARS[side]
        a.run("prepare", [a.JAVA, "-Xmx512m", "-cp", classpath, "WorldOpaqueObservation", jar, side, preparation, "prepare", out, raw_path], preparation, cap=32768)
        prepared = a.nbt(a.record_raw(preparation, 0, 1, 0))
        check(canon(prepared) == canon(input_tree), "actual original installer preserves every original fixture typed value")
        prepare_record = level.strict_json(read(out));validate_opaque_preparation(prepare_record,preparation,side,"prepare",input_raw);start, end = prepare_record["write_interval_ms"]
        raw_region = read(preparation / "world/region/r.0.0.mcr")
        check(start // 1000 <= struct.unpack_from(">I", raw_region, 4100)[0] <= end // 1000, "actual original install timestamp precise interval")
        report["preparations"].append({"side": side, "record": fp(read(out)), "input": fp(input_raw),
                                      "files": inventory(preparation / "world"), "expected_original_writer": fp(expected_raw)})
        for target in ("native",):
            intermediate = CAPTURE / (target + "-" + side + "-C");intermediate.mkdir();shutil.copytree(preparation / "world", intermediate / "world")
            Csummary = C_opaque(target, intermediate, input_tree)
            intermediates = inventory(intermediate / "world")
            records, parents = {}, {}
            for mode in ("observed", "control"):
                parent = CAPTURE / (target + "-" + side + "-" + mode);parent.mkdir();shutil.copytree(intermediate / "world", parent / "world")
                (parent / ".opaque-world-owned").write_bytes(b"private copied opaque world\n")
                output = parent / "observation.json"
                a.run("original", [a.JAVA, "-Xmx512m", "-cp", classpath, "WorldOpaqueObservation", jar, side, parent, mode, output], parent, cap=32768)
                record = level.strict_json(read(output));validate_opaque_record(record,parent,side,mode,arrays(input_tree,True),entity_class,tile_class)
                for item in record["passes"]:
                    original_chunk(item["before"], arrays(input_tree, True), entity_class, tile_class)
                    original_chunk(item["after"], arrays(input_tree, True), entity_class, tile_class)
                    check(all(not row["original_frame"] for row in item["threads"]), "finite original frame inventory")
                check(canon(a.nbt(a.record_raw(parent, 0, 1, 0))) == canon(expected_tree), "actual original factory/writer full typed output equals frozen actual original reference; no unknown-field normalization")
                interval = record["passes"][0]["write_interval_ms"]
                actual_region = read(parent / "world/region/r.0.0.mcr")
                check(interval[0] // 1000 <= struct.unpack_from(">I", actual_region, 4100)[0] <= interval[1] // 1000, "actual original opaque write timestamp precise interval")
                records[mode], parents[mode] = record, parent
                report["cases"].append({"target": target, "side": side, "mode": mode, "record": fp(read(output)),
                                         "C_transport": Csummary, "before": intermediates, "after": inventory(parent / "world"),
                                         "original_output": fp(a.record_raw(parent, 0, 1, 0)), "factory_classes": [entity_class, tile_class]})
            for index in (0, 1):
                for field in ("before", "after"):
                    check(records["observed"]["passes"][index][field] == records["control"]["passes"][index][field], "all complete same-call opaque chunk snapshots equal")
            for suffix in ("stdout", "stderr"):
                check(read(parents["observed"] / ("original." + suffix)) == read(parents["control"] / ("original." + suffix)), "complete raw opaque same-call original streams")
            m.compare_files(parents["observed"], parents["control"], [{"dimension": 0, "coordinates": [1, 0]}])
            check(inventory(intermediate / "world") == intermediates, "C intermediate immutable")
            # Fresh generic C readback of actual original serialized Boat/Sign; no WRITE/activation.
            readback = CAPTURE / (target + "-" + side + "-readback");readback.mkdir();shutil.copytree(parents["observed"] / "world", readback / "world")
            before_readback = a.snapshot_files(readback)
            responses = a.c_batch(target, readback, ["LOAD_INFO", "LOAD 0 1 0", "CHUNK 1", "CHECK"], "opaque-readback")
            for command, response in responses:
                if command.startswith(("LOAD 0", "CHUNK")):
                    check(response["chunk"] == projected_chunk(expected_tree), "fresh C readback exact actual serialized Boat/Sign known state")
            a.preservation(before_readback, readback, [])
            check(level.child(input_tree, "OpaqueRoot") is not None and level.child(expected_tree, "OpaqueRoot") is None and
                  level.child(level.child(input_tree, "Level"), "OpaqueLevel") is not None and
                  level.child(level.child(expected_tree, "Level"), "OpaqueLevel") is None,
                  "specific actual original root/Level unknown drops explicitly retained")
    metadata_preparation = CAPTURE / "metadata-preparation";metadata_preparation.mkdir()
    shutil.copytree(PRODUCER / "client-preparation/world", metadata_preparation / "world")
    (metadata_preparation / ".opaque-world-owned").write_bytes(b"private copied opaque world\n")
    metadata_input = metadata_preparation / "fixture-input.nbt"
    metadata_input.write_bytes(m.raw_metadata(fixtures[0][2]["handler-primary/world/level.dat"])[0])
    a.run("metadata-prepare", [a.JAVA, "-Xmx512m", "-cp", classpath, "WorldOpaqueObservation", JARS["client"],
                              "client", metadata_preparation, "metadata", metadata_preparation / "observation.json", metadata_input], metadata_preparation, cap=32768)
    validate_opaque_preparation(level.strict_json(read(metadata_preparation/"observation.json")),metadata_preparation,"client","metadata",read(metadata_input))
    report["metadata_preparation"] = {"record": fp(read(metadata_preparation / "observation.json")),
                                      "input": fp(read(metadata_input)), "files": inventory(metadata_preparation / "world")}
    for target in ("native",):
        report["metadata"].append(metadata_unknown(target, metadata_preparation))


def validate_threads(rows, original_names=None):
    check(type(rows) is list and 1<=len(rows)<=64,"finite complete thread inventory")
    ids=set()
    for row in rows:
        r.keys(row,("id","name","daemon","state","original_frame","frames"),"complete thread")
        check(type(row["id"]) is int and row["id"]>0 and row["id"] not in ids,"distinct exact thread ids");ids.add(row["id"])
        check(type(row["name"]) is str and 0<len(row["name"])<=512 and type(row["daemon"]) is bool and
              (row["daemon"] or row["name"]=="main") and row["original_frame"] is False,"no concurrent original/non-daemon thread")
        check(row["state"] in ("NEW","RUNNABLE","BLOCKED","WAITING","TIMED_WAITING","TERMINATED") and
              type(row["frames"]) is list and len(row["frames"])<=128 and all(type(v) is str and len(v)<=4096 for v in row["frames"]),"bounded actual thread frames")
        if original_names is not None:
            check(not any(any(frame.startswith(name+".") or ("/"+name+".") in frame for name in original_names) for frame in row["frames"]),"actual thread frames contain no original class")


def expected_bindings(side, phase):
    client=side=="client"
    w,p,c,i,s,n,h,l,cache,schema,iface,loader_iface,nibble,regions,normalize=(
        ("fd","xa","lm","ei","wt","nu","fm","ld","ok","to","cl","bf","wi","rj","vi") if client else
        ("dj","os","hi","ct","om","iq","dq","hb","jb","mg","bl","an","ob","kq","nn"))
    bindings={};fields={}
    def method(owner,name,desc,mod=1):bindings[owner+"."+name+desc]={"class":owner,"name":name,"descriptor":desc,"modifiers":mod}
    def field(owner,name,desc,mod):fields[owner+"."+name]={"class":owner,"name":name,"descriptor":desc,"modifiers":mod}
    method(w,"<init>","(L"+s+";Ljava/lang/String;JL"+p+";)V")
    method(w,"a","(J)V");method(w,"a","(ZL"+("yb" if client else "pj")+";)V")
    method(h,"c","()L"+i+";");method(l,"a","(L"+w+";L"+c+";)V");method(cache,"c","(II)L"+c+";")
    for provider in (("rh","wd") if client else ("kp","nz")):method(provider,"<init>","()V")
    if client:
        method("hi","<init>","(Ljava/io/File;)V");method("hi","a","(Ljava/lang/String;Z)Lwt;");method("hi","c","()V")
    else:method("ie","<init>","(Ljava/io/File;Ljava/lang/String;Z)V");method("ie","e","()V")
    if phase=="prepare":
        method(n,"<init>","()V");method(n,"a","(Ljava/lang/String;I)V")
        method(schema,"a","(L"+w+";L"+n+";)L"+c+";",9);method(schema,"a","(L"+c+";L"+w+";L"+n+";)V",9)
    else:method(l,"a","(L"+w+";II)L"+c+";")
    for name,desc in zip("abcdefghijklmno",("J","I","I","I","J","J","J","L"+n+";","I","Ljava/lang/String;","I","Z","I","Z","I")):field(i,name,desc,2)
    for name,desc,mod in (("C","Ljava/util/List;",2),("s","Z",1),("t","L"+p+";",17),("v","L"+iface+";",4),("w","L"+s+";",20),("x","L"+i+";",4)):field(w,name,desc,mod)
    field(h,"e","J",18);field(l,"a","Ljava/io/File;",18)
    for name,desc in (("b","[B"),("c","Z"),("d","L"+w+";"),("e","L"+nibble+";"),("f","L"+nibble+";"),("g","L"+nibble+";"),("h","[B"),("i","I"),("l","Ljava/util/Map;"),("m","[Ljava/util/List;"),("n","Z"),("o","Z"),("p","Z"),("q","Z"),("r","J")):field(c,name,desc,1)
    field(c,"j","I",17);field(c,"k","I",17)
    for name,desc in (("a","Ljava/util/Set;"),("c","L"+iface+";"),("d","L"+loader_iface+";"),("e","Ljava/util/Map;"),("f","Ljava/util/List;"),("g","L"+w+";")):field(cache,name,desc,2)
    if phase=="roundtrip":field(cache,"b","L"+c+";",2)
    field(regions,"a","Ljava/util/Map;",26);field(normalize,"a","[B",10);field(nibble,"a","[B",17);field(p,"e","Z",1);field(p,"g","I",1)
    return bindings,fields


def validate_original(record, parent, baseline=None):
    r.keys(record,("schema","phase","side","mode","jar_sha256","class_origins","block_map","events","result","bindings","fields","threads_before","threads_after","scope","control_scope"),"complete original world observation")
    check(type(record["schema"]) is int and record["schema"]==1 and record["phase"] in ("prepare","roundtrip") and
          record["side"] in PIN and record["mode"] in ("observed","control"),"exact original record header")
    original_names=None
    if "JARS" in globals():
        with zipfile.ZipFile(JARS[record["side"]]) as archive:original_names={name[:-6].replace("/",".") for name in archive.namelist() if name.endswith(".class")}
    validate_threads(record["threads_before"],original_names);validate_threads(record["threads_after"],original_names)
    check(type(record["class_origins"]) is dict and 20<=len(record["class_origins"])<=64,"finite actual class origins")
    if "JARS" in globals():
        expected=JARS[record["side"]].resolve().as_uri().replace("file:///","file:/",1)
        check(all(type(name) is str and value==expected for name,value in record["class_origins"].items()),"every actual class from supplied pinned archive")
    for name in ("bindings","fields"):
        check(type(record[name]) is dict and 10<=len(record[name])<=512,"complete observed "+name)
        for key,row in record[name].items():
            r.keys(row,("class","name","descriptor","modifiers"),"exact runtime binding")
            check(row["class"] in record["class_origins"] and type(row["descriptor"]) is str and row["descriptor"] and
                  type(row["modifiers"]) is int and 0<=row["modifiers"]<=0xffff and type(key) is str,"original exact binding provenance")
    bindings,fields=expected_bindings(record["side"],record["phase"])
    check(t.same(record["bindings"],bindings) and t.same(record["fields"],fields),"all exact original method/field descriptors and modifiers")
    for item in walk(record):
        if isinstance(item,dict):
            for key in ("interval_ms","write_interval_ms"):
                if key in item and item[key] is not None:validate_interval(item[key])
    summary=validate_record(record,parent,baseline)
    for event in record["events"]:
        if event.get("kind")=="full_save" and record["mode"]=="observed":
            tree=raw_metadata(parse_blob(event["after_roots"]["level.dat"]))[3]
            validate_saved_original_info(tree,event["before"]["metadata"],event["interval_ms"])
    if record["phase"]=="prepare":
        acquired=[];ids=set()
        for event in record["events"]:
            kind=event.get("kind")
            if kind not in ("prepare_acquire","direct_write"):continue
            dim=event["dimension"];x,z=event["coordinates"]
            check(type(dim) is int and dim in (0,-1) and (x,z) in COORDS,"fixed preparation slot")
            if kind=="prepare_acquire":acquired.append((dim,x,z))
            value=event["chunk"]
            if record["mode"]=="control":check(value is None,"preparation control omits declared intermediate chunk only");continue
            validate_chunk_snapshot(value)
            check(value["dimension"]==dim and t.same(value["coordinates"],[x,z]),"preparation snapshot exact slot")
            disk=m.disk_chunk(parent,dim,x,z)
            for role,key in (("blocks","Blocks"),("metadata","Data"),("skylight","SkyLight"),("block_light","BlockLight"),("heightmap","HeightMap")):
                check(parse_blob(value["data"][role])==bytes.fromhex(level.child(disk,key)["value"]),"preparation complete array equals genuine final raw disk (no registry remap)")
            active=kind=="prepare_acquire" or (x,z) in COORDS[:6]
            check(t.same(value["flags"],{"c":active,"n":False,"o":kind=="prepare_acquire" and dim==0,"p":False,"q":False}) and value["last_save"]==bits(0),"finite preparation flags and unstamped generation/direct clone")
            expected_min=min(parse_blob(value["data"]["heightmap"])) if kind=="prepare_acquire" else 0
            check(value["minimum"]==expected_min,"preparation observed minimum matches heightmap/loaded clone stage")
            if kind=="prepare_acquire":check(value["id"] not in ids,"distinct finite generated preparation identities");ids.add(value["id"])
        check(t.same(acquired,[(dim,x,z) for dim in (0,-1) for x,z in COORDS[:6]]),"complete ordered twelve preparation acquisitions")
    if record["phase"]=="roundtrip":
        check(baseline is not None,"roundtrip expected original disk baseline required")
        block_map=parse_blob(record["block_map"])
        phases=[record["events"]]
        reloads=[event for event in record["events"] if event.get("kind")=="genuine_reload"]
        check(len(reloads)==1,"exact fresh original reload phase");phases.append(reloads[0]["events"])
        phase_summaries=[]
        for phase_index,events in enumerate(phases):
            check(type(events) is list and 1<=len(events)<=128,"bounded complete phase events")
            handler=events[0];check(handler.get("kind")=="handler","phase begins with actual handler")
            validate_handler(handler)
            for name in ("level.dat","level.dat_old","level.dat_new"):
                path=(baseline if phase_index==0 else parent)/"world"/name
                actual=handler["roots"][name]
                check((actual is None and not path.exists()) or (actual is not None and path.exists() and parse_blob(actual)==read(path)),"phase handler metadata roots exactly match real source/final files")
            source_clock=raw_metadata(parse_blob(handler["roots"]["level.dat"]))[1]
            source_info=original_info(raw_metadata(parse_blob(handler["roots"]["level.dat"]))[3])
            check([event.get("kind") for event in events[:3]]==["handler","world","world"] and
                  [event["dimension"] for event in events[1:3]]==[0,-1],"ordered actual handler/base/Nether constructors")
            owners={event["dimension"]:{key:event["snapshot"][key] for key in ("world_id","info_id","storage_id")} for event in events[1:3]}
            check(owners[0]["storage_id"]==owners[-1]["storage_id"] and owners[0]["world_id"]!=owners[-1]["world_id"] and owners[0]["info_id"]!=owners[-1]["info_id"],"one handler with separate dimension world/Info owners")
            times={0:None,-1:None};growth={0:0,-1:0};images={}
            initial={};acquired={};schedule=[]
            for event in events:
                kind=event.get("kind")
                if kind=="genuine_reload":continue
                if kind=="time":
                    dim=event["dimension"];target=1234 if dim==0 else 5678
                    check(event["value"]==bits(target) and event["descriptor"]=="(J)V","exact actual world time setter stage");times[dim]=target
                if kind=="direct_write":
                    dim=event["dimension"];x,z=event["coordinates"]
                    before_bytes,after_bytes=event["region_before_bytes"],event["region_after_bytes"]
                    check(type(before_bytes) is type(after_bytes) is int and 8192<=before_bytes<=after_bytes<=8*1024*1024,"independent actual direct-write physical file lengths")
                    growth[dim]+=after_bytes-before_bytes
                    if record["mode"]=="observed":
                        before_image,after_image=parse_blob(event["region_before"]),parse_blob(event["region_after"])
                        check(len(before_image)==before_bytes and len(after_image)==after_bytes,"complete actual direct-write images match measured lengths")
                        name=("DIM-1/" if dim==-1 else "")+f"region/r.{x>>5}.{z>>5}.mcr"
                        check(before_image==images.get(name,read(baseline/"world"/name)),"each original region before-image equals exact prior source/stage image")
                        images[name]=after_image
                        check(struct.unpack_from(">I",after_image,4096+4*((x&31)+32*(z&31)))[0]==event["timestamp"],"actual original timestamp belongs to retained after-image")
                    else:check(event["region_before"] is None and event["region_after"] is None,"control omits only declared raw region observations")
                    validate_original_info(event["metadata_after"],source_info,times[dim],growth[dim])
                if kind=="full_save" and record["mode"]=="control":
                    path=parent/"world"/("level.dat_old" if event["dimension"]==0 else "level.dat")
                    validate_saved_original_info(raw_metadata(read(path))[3],event["before"]["metadata"],event["interval_ms"])
                for item in walk(event):
                    if isinstance(item,dict) and "metadata" in item and "world_id" in item:
                        validate_state(item,parent);dim=item["dimension"]
                        check(t.same({key:item[key] for key in owners[dim]},owners[dim]),"stable actual world/Info/handler identities throughout phase")
                        validate_original_info(item["metadata"],source_info,times[dim],growth[dim])
                        if kind in ("world",):check(item["map"]==item["insertions"]==item["cached_headers"]==[],"actual loaded constructor cache begins empty")
                        else:
                            check(len(item["cached_headers"])==6 and item["queue_count"]==0,"all six finite acquired chunks retained in live cache")
                            for row,(cx,cz) in zip(item["cached_headers"],COORDS[:6]):
                                expected=expected_chunk(baseline,block_map,dim,cx,cz,True)
                                check(t.same(row["coordinates"],[cx,cz]) and t.same(row["flags"],expected["flags"]) and row["minimum"]==0 and row["last_save"]==expected["last_save_time"],"live cached order/coordinates/flags/stamp equal acquired stage")
                                if record["mode"]=="observed":check(row["id"]==acquired[(dim,cx,cz)],"live cache headers retain exact acquired identities")
                    if isinstance(item,dict) and "last_played" in item and "seed" in item:
                        check(item["last_played"]==bits(source_clock),"each phase Info retains its precise loaded metadata clock")
                if kind not in ("direct_load","acquire","direct_write"):continue
                dim=event["dimension"];x,z=event["coordinates"]
                schedule.append((kind,dim,x,z))
                check(dim in (0,-1) and (x,z) in COORDS,"fixed actual slot schedule")
                value=event["chunk"]
                if record["mode"]=="control":check(value is None,"control omits only declared intermediate snapshot");continue
                identity=validate_stage_chunk(value,baseline,block_map,dim,x,z,kind)
                key=(dim,x,z)
                if kind=="direct_load":check(identity not in initial.values(),"distinct fresh direct loaded identities");initial[key]=identity
                if kind=="acquire":check(identity not in initial.values() and identity not in acquired.values(),"actual acquire has fresh loaded identity");acquired[key]=identity
                if kind=="direct_write":check(identity==(acquired[key] if (x,z) in COORDS[:6] else initial[key]),"direct writer uses retained acquired/loaded identity")
            expected=[]
            for dim in (0,-1):
                expected += [("direct_load",dim,x,z) for x,z in COORDS]
                expected += [("acquire",dim,x,z) for x,z in COORDS[:6]]
                if phase_index==0:expected += [("direct_write",dim,x,z) for x,z in COORDS]
            check(t.same(schedule,expected),"complete ordered original stage/slot schedule")
            if record["mode"]=="observed" and phase_index==0:
                for name,image in images.items():check(read(parent/"world"/name)==image,"retained final original region image equals last measured write")
            phase_summaries.append((owners,source_info,times,growth,initial,acquired))
        saved=record["result"]["saved"]
        initial_clock=raw_metadata(parse_blob(record["events"][0]["roots"]["level.dat"]))[1]
        reload_clock=raw_metadata(parse_blob(phases[1][0]["roots"]["level.dat"]))[1]
        for dim in ("base","nether"):
            check(saved[dim]["metadata"]["last_played"]==bits(initial_clock) and reloads[0][dim]["metadata"]["last_played"]==bits(reload_clock),"saved/fresh reload Info clocks bound to exact handler phases")
            dimension=0 if dim=="base" else -1
            for index,state in ((0,saved[dim]),(1,reloads[0][dim])):
                owners,source,times,growth,_,acquired=phase_summaries[index]
                validate_state(state,parent);check(t.same({key:state[key] for key in owners[dimension]},owners[dimension]),"summary retains exact phase owners")
                validate_original_info(state["metadata"],source,times[dimension],growth[dimension])
                check(len(state["insertions"])==6 and (record["mode"]=="control" or state["insertions"]==[acquired[dimension,x,z] for x,z in COORDS[:6]]),"summary retains all acquired identities in order")
                check(state["queue_count"]==0,"summary loaded route leaves light queue empty")
                for row,(cx,cz) in zip(state["cached_headers"],COORDS[:6]):
                    expected=expected_chunk(baseline,block_map,dimension,cx,cz,True)
                    check(t.same(row["coordinates"],[cx,cz]) and t.same(row["flags"],expected["flags"]) and row["minimum"]==0 and row["last_save"]==expected["last_save_time"],"summary cached headers retain complete acquired stage state")
        for chunk in reloads[0]["direct_chunks"]:
            validate_chunk_snapshot(chunk);dim=chunk["dimension"];x,z=chunk["coordinates"]
            if record["mode"]=="observed":check(chunk["id"]==phase_summaries[1][4][dim,x,z],"fresh reload summary preserves direct-loaded identities")
        for roots in (record["result"]["final_roots"],record["result"]["saved"]["roots"]):
            for name in ("level.dat","level.dat_old","level.dat_new"):
                path=parent/"world"/name;blob=roots[name]
                check((blob is None and not path.exists()) or (blob is not None and parse_blob(blob)==read(path)),"every final/saved metadata root equals actual untouched-after-save bytes")
        check(t.same(record["events"][3],{"kind":"info_ownership","shared":False,"base":record["events"][1]["snapshot"]["info_id"],"nether":record["events"][2]["snapshot"]["info_id"]}),"explicit separate initial metadata owners")
    return summary


validate = validate_original


def check_child_streams(directory, process, expected_command, label="original", stdout_limit=32768):
    r.successful(process,label=label,stdout_limit=stdout_limit)
    check(t.same(process["command"],list(map(str,expected_command))),"exact actual child command")
    for field,limit in (("stdout",stdout_limit),("stderr",32768)):
        path=Path(directory)/process[field+"_file"]
        check(fp(read(path,limit))==process[field],"actual canonical retained "+field+" bytes")
    return process


def run(label, command, directory, stdin=None, cap=128*1024*1024):
    directory.mkdir(parents=True,exist_ok=True)
    r._latest_process=None
    try:
        result=r.process(command,directory,label,120,stdin,cap)
        REPORT["processes"].append({"label":label,"process":result,
                                     "stdin":None if stdin is None else fp(read(directory/(label+".stdin")))})
        check_child_streams(directory,result,command,label,cap)
        return read(directory/result["stdout_file"],cap)
    except BaseException:
        if r._latest_process is not None:
            REPORT["failed_process"]=copy.deepcopy(r._latest_process)
        raise


def c_batch(target, parent, commands, label):
    check(target=="native","one supplied native executable per run")
    rows=[f"OPEN 0 {str((parent/'world').resolve()).encode('utf8').hex()} {TOKEN} 0 {BLOCK_MAP.hex()}",*commands,"CLOSE","CLEAR","QUIT"]
    raw=run(label,[EXE],parent,("\n".join(rows)+"\n").encode("ascii"))
    values=[t.validate_response(line) for line in raw.splitlines(keepends=True)]
    check(len(values)==len(rows) and all(value["status"]==0 for value in values),"one successful complete C response per request")
    check(read(parent/(label+".stderr"),32768)==b"","clean C stderr and sanitizer output")
    return list(zip(rows,values))


def source_paths():
    names={"CMakeLists.txt",*JAVA_SOURCES,"tests/world/storage/store_probe.c","tests/tools/storage/test_store.py","tests/tools/storage/test_world_storage.py",
           "tests/reference/storage/level_fixtures.json","tests/reference/storage/chunk_storage_fixtures.json"}
    for folder,pattern in (("include","*.h"),("src","*.c"),("src","*.h"),("scripts/reference/storage","*.py")):
        names.update(path.relative_to(ROOT).as_posix() for path in (ROOT/folder).rglob(pattern))
    # Imported existing storage drivers themselves import these project comparison helpers.
    for folder in ("caves","chunk","cache","generation","climate","noise","surface"):
        directory=ROOT/"scripts/reference"/folder
        if directory.exists():names.update(path.relative_to(ROOT).as_posix() for path in directory.glob("*.py"))
    return tuple(sorted(names))


def compiled_classes(directory):
    return helper_classes(directory)


def prepare_sources(args, directory, report):
    check(Path(__file__).resolve()==ROOT/"scripts/reference/storage/compare_world_storage.py","canonical executing driver source identity")
    for name in JAVA_ENVIRONMENT:check(name not in os.environ,"injected Java environment: "+name)
    sources={name:fp(read(ROOT/name,8*1024*1024)) for name in source_paths()}
    report["source_fingerprints"]=sources
    report["inputs_before"]={str(ROOT/name):value for name,value in sources.items()}
    for name,loaded in LOADED_PYTHON.items():
        check(name in report["inputs_before"] and t.same(loaded,report["inputs_before"][name]),"attributed imported source bytes exactly equal actual compiled Python bytes")
    _,executed=level.helpers()
    for row in executed:
        path=str(Path(row["path"]).resolve())
        check(path in report["inputs_before"] and t.same({key:row[key] for key in ("bytes","sha256")},report["inputs_before"][path]),"actual dynamically executed helper bytes belong to attributed closure")
    report["executed_python"]={**LOADED_PYTHON,**{str(Path(row["path"]).resolve()):{key:row[key] for key in ("bytes","sha256")} for row in executed}}
    for name,value in sources.items():
        path=directory/"sources"/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(read(ROOT/name,8*1024*1024))
    if args.tested_commit is not None:
        check(type(args.tested_commit) is str and re.fullmatch(r"[0-9a-f]{40}",args.tested_commit) is not None,"explicit full40 tested commit")
        command=["git","cat-file","-t",args.tested_commit]
        kind_process=r.process(command,directory,"git-kind",20,stdout_limit=32768);check_child_streams(directory,kind_process,command,"git-kind")
        check(read(directory/"git-kind.stdout",32768)==b"commit\n","tested source object must be an actual Git commit")
        result=r.process(["git","ls-tree","-r","-z",args.tested_commit],directory,"git-source",20,stdout_limit=4*1024*1024)
        check_child_streams(directory,result,["git","ls-tree","-r","-z",args.tested_commit],"git-source",4*1024*1024)
        tree={}
        for entry in read(directory/"git-source.stdout",4*1024*1024).split(b"\0"):
            if not entry:continue
            header,name=entry.split(b"\t",1);mode,object_kind,digest=header.split(b" ");tree[name.decode("utf8")]=(mode,object_kind,digest.decode("ascii"))
        for name in sources:
            raw=read(ROOT/name,8*1024*1024);expected=hashlib.sha1(b"blob "+str(len(raw)).encode()+b"\0"+raw).hexdigest()
            check(name in tree and tree[name][0] in (b"100644",b"100755") and tree[name][1]==b"blob" and tree[name][2]==expected,"every executing source/fixture bound to tested Git tree: "+name)
        report["git_source_binding"]={"commit":args.tested_commit,"files":len(sources),"all_blob_bytes_match":True,"kind_process":kind_process,"process":result}
    return sources


def execute(args, directory, report):
    global CAPTURE, REPORT, PRODUCER, CLASSES, JAVA, JARS, EXE, BLOCK_MAP
    CAPTURE,REPORT=directory,report
    prepare_sources(args,directory,report)
    JARS={}
    for side in PIN:
        value=getattr(args,side)
        if value is None or not Path(value).is_file():raise Unavailable("missing locally supplied "+side+" archive")
        path=Path(value).resolve();check(fp(read(path,4*1024*1024))["sha256"]==PIN[side],"wrong supplied "+side+" archive")
        JARS[side]=path
        report["inputs_before"][str(path)]=fp(read(path,4*1024*1024))
    check(args.executable is not None and Path(args.executable).is_file(),"built native world store probe required")
    EXE=Path(args.executable).resolve()
    report["inputs_before"][str(EXE)]=fp(read(EXE,128*1024*1024))
    JAVA=r.tool_path(args.java,"java")
    report["inputs_before"][str(JAVA)]=fp(read(JAVA,128*1024*1024))
    javac=r.tool_path(args.javac,"javac")
    for path in (javac,Path(sys.executable).resolve()):report["inputs_before"][str(path)]=fp(read(path,128*1024*1024))
    check(JAVA.parent.parent==javac.parent.parent,"java and javac from one attributed JDK")
    for relative in ("release","lib/modules","lib/ct.sym"):
        path=JAVA.parent.parent/relative
        if not path.is_file():raise Unavailable("missing required attributed JDK file: "+relative)
        report["inputs_before"][str(path)]=fp(read(path,256*1024*1024))
    report["tools"]={}
    for name,path in (("java",JAVA),("javac",javac)):
        result=r.process([path,"-version"],directory,name,20);check_child_streams(directory,result,[path,"-version"],name)
        text=read(directory/(name+".stdout"),32768)+read(directory/(name+".stderr"),32768)
        match=re.search(rb'(?:version\s+"|javac\s+)(\d+)',text);check(match is not None,"actual JDK version")
        if int(match.group(1))<17:raise Unavailable("JDK17+ required")
        report["tools"][name]={"path":str(path),"fingerprint":fp(read(path,128*1024*1024)),"major":int(match.group(1)),"process":result}
    PRODUCER=directory/"original-source";PRODUCER.mkdir();CLASSES=PRODUCER/"classes";CLASSES.mkdir()
    empty=PRODUCER/"compiler-empty";empty.mkdir()
    command=[javac,"-proc:none","-implicit:none","-Xlint:all","-Werror","--release","17","-encoding","UTF-8",
             "-classpath",empty,"-sourcepath",empty,"--module-path",empty,"-d",CLASSES,*(directory/"sources"/name for name in JAVA_SOURCES)]
    run("compile",command,PRODUCER,cap=32768)
    check(read(PRODUCER/"compile.stdout",32768)==read(PRODUCER/"compile.stderr",32768)==b"","silent exact helper compilation")
    report["classes_before"]=compiled_classes(CLASSES)
    for side,jar in JARS.items():
        records={};parents={}
        for mode in ("observed","control"):
            parent=PRODUCER/(side+"-preparation"+("-control" if mode=="control" else ""));parent.mkdir()
            output=PRODUCER/(side+"-prepare-"+mode+".json")
            report["measurement_started"]=True
            run(side+"-prepare-"+mode,[JAVA,"-Xmx512m","-cp",CLASSES,"WorldStorageObservation",jar,side,"prepare",mode,parent,output],PRODUCER,cap=32768)
            record=level.strict_json(read(output));summary=validate_original(record,parent)
            records[mode],parents[mode]=record,parent
            report["preparations"].append({"side":side,"mode":mode,"record":fp(read(output)),"summary":summary,"files":inventory(parent/"world")})
        check(t.same(control_projection(records["observed"],parents["observed"]),control_projection(records["control"],parents["control"])),"complete same-call preparation control projection")
        compare_files(parents["observed"],parents["control"],[{"dimension":dim,"coordinates":[x,z]} for dim,x,z in SLOTS])
    BLOCK_MAP=parse_blob(records["observed"]["block_map"])
    client_map=parse_blob(level.strict_json(read(PRODUCER/"client-prepare-observed.json"))["block_map"])
    check(BLOCK_MAP==client_map,"paired actual normalization maps equal")
    report["baselines_before"]={side:inventory(PRODUCER/(side+"-preparation/world")) for side in PIN}
    for side in PIN:
        baseline=PRODUCER/(side+"-preparation");parent=directory/("native-copied-"+side+"-C");parent.mkdir();shutil.copytree(baseline/"world",parent/"world")
        shutil.copyfile(parent/"world/region/r.0.0.mcr",parent/"world/region/r.12345.12345.mcr")
        (parent/"world/data").mkdir(exist_ok=True);(parent/"world/data/transport.bin").write_bytes(bytes(range(256)))
        report["C_inputs"].append({"scenario":"copied","side":side,"summary":exercise("native",parent,baseline)})
        originals("native","copied",parent,side)
    parent=directory/"native-created-C";parent.mkdir()
    report["C_inputs"].append({"scenario":"created","summary":exercise("native",parent,PRODUCER/"client-preparation",create=True)})
    originals("native","created",parent,None)
    opaque_roundtrips(report,str(CLASSES))
    report["baselines_after"]={side:inventory(PRODUCER/(side+"-preparation/world")) for side in PIN}
    check(report["baselines_before"]==report["baselines_after"],"all original source world bytes immutable")
    original_classes={side:{} for side in PIN}
    for path in directory.rglob("observation.json"):
        value=level.strict_json(read(path));side=value["side"]
        with zipfile.ZipFile(JARS[side]) as archive:
            for name in value["class_origins"]:original_classes[side][name]=fp(archive.read(name+".class"))
    report["original_classes"]=original_classes


def validate_work_dir(work_dir, protected=()):
    work=Path(os.path.abspath(work_dir))
    for component in reversed((work,*work.parents)):
        try:info=component.lstat()
        except FileNotFoundError:continue
        check(not stat.S_ISLNK(info.st_mode) and not (getattr(info,"st_file_attributes",0)&0x400),"output link/reparse component refused")
        check(stat.S_ISDIR(info.st_mode),"output must have ordinary directory components")
    resolved=work.resolve()
    if resolved.is_relative_to(ROOT):
        parts=resolved.relative_to(ROOT).parts
        check(parts and (parts[0]==".local" or parts[0]=="build" or parts[0].startswith("build-")),"output inside public source/input layout refused")
    for selected in protected:
        if selected is None:continue
        path=Path(selected).resolve()
        check(resolved!=path and resolved not in path.parents,"output would contain/alias an attributed input")
        if work.exists() and path.exists():check(not os.path.samefile(work,path),"output aliases an input")
    return work


MAX_MANIFEST_BYTES = 8*1024*1024


def encode_manifest(report):
    """Strict report or a bounded explicit failure envelope, never byte coercion."""
    def encoded(value):
        raw=(json.dumps(value,indent=2,allow_nan=False)+"\n").encode("utf8")
        check(len(raw)<=MAX_MANIFEST_BYTES,"bounded manifest serialization")
        return raw
    errors=(TypeError,ValueError,OverflowError,RecursionError,Failure)
    try:return encoded(report),False
    except errors as error:
        primary=[];primary_complete=True;primary_budget=32*1024
        if type(report.get("errors")) is list:
            primary_complete=len(report["errors"])<=32
            for value in report["errors"][:32]:
                if type(value) is not str:primary_complete=False;continue
                size=len(json.dumps(value).encode("utf8"))
                if size>primary_budget:primary_complete=False;continue
                primary.append(value);primary_budget-=size
        else:primary_complete=False
        primary.append("manifest serialization failed: "+type(error).__name__+": "+str(error)[:1024])
        retained={};excluded=[];budget=MAX_MANIFEST_BYTES-128*1024
        controlled={"schema","scope","status","errors","tested_commit","measurement_started"}
        for index,(name,value) in enumerate(report.items()):
            if type(name) is str and name in controlled:continue
            if index>=128:
                excluded.append({"name":None,"type":"remaining-fields","reason":"bounded diagnostic field count"});break
            reason=None
            if type(name) is not str or len(name)>256:reason="invalid diagnostic field name"
            else:
                try:
                    raw=encoded({name:value})
                    if len(raw)>budget:reason="bounded retained evidence budget"
                except errors as field_error:reason=type(field_error).__name__+": "+str(field_error)[:256]
            if reason is not None:
                excluded.append({"name":name if type(name) is str and len(name)<=256 else None,"type":type(value).__name__,"reason":reason})
            else:retained[name]=value;budget-=len(raw)
        label=report.get("tested_commit")
        fallback={"schema":1,"scope":"whole-world-storage-boundary","status":"fail",
                  "tested_commit":label if type(label) is str and re.fullmatch(r"[0-9a-f]{40}",label) else None,
                  "measurement_started":report.get("measurement_started") is True,"errors":primary,
                  "report_serialization_failure":{"type":type(error).__name__,"message":str(error)[:1024],"primary_errors_complete":primary_complete,"excluded_fields":excluded},
                  "retained_report":retained}
        return encoded(fallback),True


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client",type=Path);parser.add_argument("--server",type=Path)
    parser.add_argument("--java");parser.add_argument("--javac");parser.add_argument("--executable",type=Path)
    parser.add_argument("--work-dir",type=Path,default=ROOT/".local/reference-world-storage")
    parser.add_argument("--tested-commit")
    args=parser.parse_args(argv)
    directory=None
    report={"schema":1,"scope":"whole-world-storage-boundary","status":"running","tested_commit":args.tested_commit,
            "measurement_started":False,"errors":[],"processes":[],"preparations":[],"originals":[],"roundtrips":[],"C_inputs":[],"cases":[],"metadata":[],
            "limits":{"java_heap_mib":512,"process_seconds":120,"stdin_bytes":32*1024*1024,"stdout_bytes":128*1024*1024},
            "scope_limits":["explicit providers, not gameplay dimension transition","finite Boat/Sign generic storage, no C activation/subtype/scheduling","original serializer unknown-field drops retained","clock differences bound to individual calls; compression bytes not parity"]}
    code=1
    try:
        protected=[ROOT/name for name in source_paths()]+[args.client,args.server,args.executable,args.java,args.javac,Path(sys.executable)]
        for selected,name in ((args.java,"java"),(args.javac,"javac")):
            tool=selected or shutil.which(name)
            if tool is not None:
                path=Path(tool).resolve();protected.extend((path,path.parent.parent/"release",path.parent.parent/"lib/modules",path.parent.parent/"lib/ct.sym"))
        work=validate_work_dir(args.work_dir,protected);work.mkdir(parents=True,exist_ok=True)
        directory=(work/("capture-"+uuid.uuid4().hex)).resolve();directory.mkdir()
        execute(args,directory,report);report["status"]="pass";code=0
    except Unavailable as error:
        if report["measurement_started"]:report["status"]="fail";report["errors"].append(str(error));code=1
        else:report["status"]="unavailable";report["unavailable_reason"]=str(error);code=77
    except (Failure,level.Failure,t.chunks.Failure,AssertionError,OSError,ValueError,KeyError,TypeError,UnicodeError,struct.error,zlib.error,subprocess.SubprocessError,zipfile.BadZipFile) as error:
        report["status"]="fail";report["errors"].append(type(error).__name__+": "+str(error));code=1
    finally:
        try:
            if "inputs_before" in report:
                report["inputs_after"]={name:fp(read(Path(name),256*1024*1024)) for name in report["inputs_before"]}
                check(t.same(report["inputs_before"],report["inputs_after"]),"attributed sources/tools/JARs/executable preserved after every outcome")
            if "classes_before" in report:
                report["classes_after"]=compiled_classes(CLASSES);check(t.same(report["classes_before"],report["classes_after"]),"all compiled project helpers preserved")
        except (Failure,AssertionError,OSError) as error:
            report["status"]="fail";report["errors"].append(str(error));code=1
        manifest,serialization_failed=encode_manifest(report)
        if serialization_failed:code=1
        if directory is not None:
            try:
                (directory/"manifest.json").write_bytes(manifest)
                print(directory/"manifest.json")
            except OSError as error:
                report["status"]="fail";report["errors"].append("cannot retain manifest: "+str(error));code=1
                manifest,_=encode_manifest(report);print(manifest.decode("utf8"),file=sys.stderr,end="")
        else:print(manifest.decode("utf8"),file=sys.stderr,end="")
    return code


if __name__=="__main__":raise SystemExit(main())
