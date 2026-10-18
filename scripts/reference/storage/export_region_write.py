#!/usr/bin/env python3
"""Export lossless region image-write fixtures from successful original-only runs."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re

import compare_region_write as region


def artifact(directory, relative, expected, limit=region.MAX_RECORD):
    region.validate_fingerprint(expected,limit)
    path=directory/region.safe_relative(relative)
    raw=region.checked_bytes(path,limit)
    region.check(region.blob(raw)==expected,"stable retained attributed artifact: "+relative)
    return raw


def retained_streams(directory, process, label, limit=32768):
    region.successful(process,label=label,stdout_limit=limit)
    return {field:artifact(directory,label+"."+field,process[field],
                           limit if field=="stdout" else 32768)
            for field in ("stdout","stderr")}


def export(manifest_path):
    manifest_raw=region.checked_bytes(manifest_path)
    report=region.strict_json(manifest_raw);directory=manifest_path.resolve().parent
    region.keys(report,("schema","scope","mode","status","tested_commit","source_label","python","platform","original_runs","controls","cases","errors","limits",
                        "inputs_before","inputs_after","source_fingerprints","original_classes","tools","compile","classes_before","classes_after","client/server"),"complete original-only manifest without C results")
    region.integer(report["schema"],1,1,"producer schema")
    region.check(report.get("scope")=="region-memory-write" and
                 report.get("mode")=="original-only" and report.get("status")=="pass" and report.get("errors")==[],"complete successful original-only producer")
    region.check(type(report["inputs_before"]) is dict and report["inputs_before"] and region.same_json(report["inputs_before"],report["inputs_after"]) and
                 set(report.get("source_fingerprints",{}))==set(region.SOURCES),"exact unchanged source/input closure")
    jars={}
    for name,expected in report["inputs_before"].items():
        region.validate_fingerprint(expected,128*1024*1024,1)
        region.check(region.fingerprint(Path(name),128*1024*1024)==expected,"retained actual source/tool/archive bytes")
        for side,digest in region.JAR_HASHES.items():
            if expected["sha256"]==digest:
                region.check(side not in jars,"unique supplied archive attribution");jars[side]=Path(name).resolve()
    region.check(set(jars)==set(region.JAR_HASHES),"both supplied originals attributed")
    for name,expected in report["source_fingerprints"].items():
        region.validate_fingerprint(expected,2*1024*1024,1)
        region.check(report["inputs_before"].get(str(region.ROOT/name))==expected and
                     region.fingerprint(directory/"sources"/name,2*1024*1024)==expected,"exact copied project source attribution")
    compile_record=report["compile"];compile_streams=retained_streams(directory,compile_record,"compile")
    region.check(compile_record["command"]==[report["tools"]["javac"]["path"],"--release","17","-encoding","UTF-8","-d",str(directory/"classes"),str(directory/"sources"/region.JAVA_SOURCE)],"exact helper compiler command")
    for kind in ("stdout","stderr"):
        region.check(compile_record[kind]["bytes"]==0,"silent compiler output")
        region.check(compile_streams[kind]==b"","silent retained compiler stream")
    classes=region.compiled_classes(directory/"classes")
    region.check(region.same_json(classes,report["classes_before"]) and region.same_json(classes,report["classes_after"]),"actual compiled helper inventory and bytes preserved")
    originals={side:region.class_records(jar,side) for side,jar in jars.items()}
    region.check(region.same_json(originals,report["original_classes"]),"actual classfiles from pinned archives")
    region.keys(report["tools"],("java","javac"),"complete required JDK tools")
    for name,row in report["tools"].items():
        region.keys(row,("path","fingerprint","major","output","process"),"complete JDK tool attribution")
        region.integer(row["major"],17,99,"actual JDK tool major")
        region.validate_fingerprint(row["fingerprint"],128*1024*1024,1)
        region.check(type(row["path"]) is str and 1<=len(row["path"])<=4096 and
                     type(row["output"]) is str and 0<len(row["output"])<=32768,"bounded JDK tool path and output")
        streams=retained_streams(directory,row["process"],name)
        region.check(row["process"]["command"]==[row["path"],"-version"],"actual required JDK tool version invocation")
        region.check(region.fingerprint(Path(row["path"]),128*1024*1024)==row["fingerprint"] and report["inputs_before"].get(row["path"])==row["fingerprint"],"attributed exact tool executable")
        retained=b"".join(streams[field] for field in ("stdout","stderr"))
        region.check(retained.decode("utf8",errors="replace")==row["output"],"complete actual tool version output")
    region.check(set(report["tools"])=={"java","javac"},"all required JDK tool records")
    pool,records,files,public={}, {}, {}, {}
    def retain(raw):
        digest=hashlib.sha256(raw).hexdigest()
        if digest in pool: region.check(pool[digest]==raw,"identical content-addressed evidence")
        pool[digest]=raw
    region.check(type(report.get("original_runs")) is list and len(report["original_runs"])==4,"all four original/control executions")
    for index,(side,mode) in enumerate((s,m) for s in region.JAR_HASHES for m in ("observed","control")):
        row=report["original_runs"][index]
        region.keys(row,("side","mode","directory","process","record","summary"),"complete original execution")
        region.check(row["side"]==side and row["mode"]==mode and row["directory"]==side+"-"+mode,"fixed unique original execution order")
        case=directory/row["directory"];process=row["process"];streams=retained_streams(case,process,"child")
        expected_command=[report["tools"]["java"]["path"],"-Xmx512m","-cp",str(directory/"classes"),"RegionWriteObservation",str(jars[side]),side,mode,str(case/"storage"),str(case/"record.json"),str(case/"snapshots")]
        region.check(process["command"]==expected_command,"exact untouched original child command")
        for field in ("stdout","stderr"):
            region.check(process[field]["bytes"]==0 and process[field+"_file"]=="child."+field,"silent exact child stream identity")
            region.check(streams[field]==b"","silent retained child stream")
        raw=artifact(case,"record.json",row["record"])
        record=region.strict_json(raw);evidence=region.Evidence(root=case)
        summary=region.validate_original(record,evidence,side,mode,jars[side]);region.check(region.same_json(summary,row["summary"]),"complete validated original execution summary")
        for event in record["events"]:
            if event["kind"]=="snapshot":
                for name,expected in event["files"].items(): retain(artifact(case,"snapshots/"+event["directory"]+"/"+name,expected,region.MAX_FILE))
            for field,suffix in (("plain",".plain"),("compressed",".zlib")):
                if field in event: retain(artifact(case,"snapshots/inputs/"+event[field]["sha256"]+suffix,event[field],region.MAX_FILE))
        for name,expected in record["final_files"].items():
            retain(artifact(case,"snapshots/final/"+name,expected,region.MAX_FILE))
            region.check(region.fingerprint(case/"storage"/name,region.MAX_FILE)==expected,"complete original live files still equal final retained copies")
        region.check(set(region.inventory(case/"storage",count_limit=300,byte_limit=8*1024*1024))==set(record["final_files"]),"no extra or missing original live files")
        records[side,mode]=record;files[side,mode]=evidence
        redacted=copy.deepcopy(record);redacted["class_origins"]={name:"supplied:"+side for name in record["class_origins"]}
        public.setdefault(side,{})[mode]=region.packed(json.dumps(redacted,separators=(",",":"),ensure_ascii=True).encode())
        public[side]["raw_"+mode]=region.blob(raw)
    for side in region.JAR_HASHES:
        comparison=region.compare(records[side,"observed"],files[side,"observed"],records[side,"control"],files[side,"control"])
        region.check(region.same_json(report["controls"][list(region.JAR_HASHES).index(side)],{"side":side,**comparison}),"exact revalidated original/control comparison")
    region.check(region.same_json(report["client/server"],region.compare(records["client","observed"],files["client","observed"],records["server","observed"],files["server","observed"])) ,"complete original side comparison")
    region.check(type(report.get("cases")) is list and len(report["cases"])==4,"complete original-only profile corpus")
    cases=[]
    for index,(side,mode) in enumerate(region.CASE_LAYOUT):
        row=report["cases"][index]
        region.keys(row,("index","side","profile","writes","input_file","expected_file","input","expected"),"original-only case without C output")
        derived=region.derive_profiles(records[side,"observed"],files[side,"observed"])[mode]
        region.integer(row["index"],index,index,"exact original profile index")
        region.integer(row["writes"],derived["writes"],derived["writes"],"exact original profile write count")
        region.check((row["index"],row["side"],row["profile"],row["writes"],row["input_file"],row["expected_file"])==(index,side,mode,derived["writes"],f"case-{index}.mrw",f"case-{index}.mrs"),"exact original profile identity")
        region.check(artifact(directory,row["input_file"],row["input"])==derived["input"] and
                     artifact(directory,row["expected_file"],row["expected"],region.MAX_OUTPUT)==derived["expected"],"rebuild actual MRW1/MRS1 exclusively from original evidence")
        cases.append(dict(index=index,side=side,profile=mode,writes=derived["writes"],input=region.packed(derived["input"]),expected=region.packed(derived["expected"])))
    result={"schema":1,"format":"MRW1/MRS1","scope":"region-memory-write",
            "status_semantics":"Original write methods return void. Status4 denotes the C record-too-large diagnostic for the observed unchanged-image branch; other retained writes use status0.",
            "provenance":{"mode":"original-only","manifest_sha256":hashlib.sha256(manifest_raw).hexdigest(),"tested_commit":report["tested_commit"],
                "sources":report["source_fingerprints"],"helper_classes":classes,"helper_class_version":61,"original_classes":originals,
                "jar_sha256":region.JAR_HASHES,"tools":{name:{k:row[k] for k in ("fingerprint","major","output")} for name,row in report["tools"].items()},
                "exporter_sha256":region.fingerprint(Path(__file__),2*1024*1024)["sha256"],
                "counts":{"original_runs":4,"observed_snapshots":54,"protocol_actions":1364,"profiles":4,"replay_writes":34,"replay_successes":26,"replay_rejections":8},
                "controls":{side:public[side]["raw_control"] for side in region.JAR_HASHES}},
            "originals":public,"blobs":{digest:region.packed(raw) for digest,raw in sorted(pool.items())},"cases":cases}
    region.validate_public_metadata(result)
    return result


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest",type=Path,required=True)
    parser.add_argument("--output",type=Path,default=region.ROOT/"tests/reference/storage/region_write_fixtures.json")
    args=parser.parse_args(argv)
    result=export(args.manifest)
    region.validate_fixture_value(result)
    raw=(json.dumps(result,sort_keys=True,separators=(",",":"))+"\n").encode()
    region.check(len(raw)<=region.MAX_FIXTURE,"bounded public fixture file")
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open("xb") as stream: stream.write(raw)
    print(str(args.output)+" "+str(len(raw))+" bytes SHA256 "+hashlib.sha256(raw).hexdigest())
    return 0


if __name__=="__main__":
    raise SystemExit(main())
