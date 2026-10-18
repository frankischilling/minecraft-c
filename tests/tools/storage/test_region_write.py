"""Java-free original-fixture replay and constructed writer-protocol guards."""
from __future__ import annotations

import argparse
import base64
import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib

ROOT=Path(__file__).resolve().parents[3]
FIXTURE=ROOT/"tests/reference/storage/region_write_fixtures.json"
spec=importlib.util.spec_from_file_location("_region_write_reference",ROOT/"scripts/reference/storage/compare_region_write.py")
region=importlib.util.module_from_spec(spec);spec.loader.exec_module(region)
export_spec=importlib.util.spec_from_file_location("_region_write_export",ROOT/"scripts/reference/storage/export_region_write.py")
exporter=importlib.util.module_from_spec(export_spec)
with mock.patch.dict(sys.modules,{"compare_region_write":region}):
    export_spec.loader.exec_module(exporter)


class Reader:
    def __init__(self,raw): self.raw,self.offset=raw,0
    def take(self,n):
        if n<0 or n>len(self.raw)-self.offset: raise ValueError("truncated binary frame")
        result=self.raw[self.offset:self.offset+n];self.offset+=n;return result
    def numbers(self,n): return struct.unpack(">"+"I"*n,self.take(n*4))
    def end(self):
        if self.offset!=len(self.raw): raise ValueError("trailing binary bytes")


def input_frames(raw):
    reader=Reader(raw)
    if reader.take(4)!=b"MRW1": raise ValueError("input magic")
    mode=reader.take(1)[0];size,=reader.numbers(1);initial=reader.take(size);count,=reader.numbers(1)
    if mode>1 or mode==0 and size or count>32: raise ValueError("input header")
    rows=[]
    for _ in range(count):
        x,z,timestamp,length=reader.numbers(4);rows.append((x,z,timestamp,reader.take(length)))
    reader.end();return mode,initial,rows


def output_frames(raw):
    reader=Reader(raw)
    if reader.take(4)!=b"MRS1": raise ValueError("output magic")
    count,=reader.numbers(1)
    if not 1<=count<=33: raise ValueError("output count")
    rows=[]
    for _ in range(count):
        status,growth,length=reader.numbers(3)
        if status>6 or length>region.MAX_RECORD: raise ValueError("output bounds")
        rows.append((status,growth,reader.take(length)))
    reader.end();return rows


def change_record(fixture,side,mode,change):
    result=copy.deepcopy(fixture)
    record=region.strict_json(region.unpack(result["originals"][side][mode]))
    change(record)
    result["originals"][side][mode]=region.packed(json.dumps(record,separators=(",",":")).encode())
    return result


class TransportTests(unittest.TestCase):
    def test_complete_wire_bits_and_zero_write_schedule(self):
        raw=region.encode_replay("fresh",b"",[dict(x=-2147483648,z=-1,timestamp=0xffffffff,payload=b"opaque")])
        self.assertEqual(input_frames(raw),(0,b"",[(0x80000000,0xffffffff,0xffffffff,b"opaque")]))
        self.assertEqual(input_frames(region.encode_replay("fresh",b"",[])),(0,b"",[]))
        self.assertEqual(input_frames(region.encode_replay("import-copy",bytes(8192),[])),(1,bytes(8192),[]))

    def test_encoder_rejects_coercions_omissions_and_schedule_limits(self):
        base=dict(x=0,z=0,timestamp=0,payload=b"x")
        for name in ("x","z","timestamp","payload","missing"):
            row=base.copy()
            if name=="missing": del row["payload"]
            else: row[name]=True if name!="payload" else "x"
            with self.subTest(name=name),self.assertRaises(region.Failure): region.encode_replay("fresh",b"",[row])
        for mode,initial,writes in (("unknown",b"",[]),("fresh",b"x",[]),("import-copy",b"",[]),("fresh",b"",[base]*33)):
            with self.subTest(mode=mode,size=len(initial),writes=len(writes)),self.assertRaises(region.Failure): region.encode_replay(mode,initial,writes)

    def test_complete_output_frame_and_domain_failure_shape(self):
        raw=region.encode_states([dict(status=0,growth=8192,image=bytes(8192)),dict(status=4,growth=0,image=bytes(8192))])
        self.assertEqual(output_frames(raw),[(0,8192,bytes(8192)),(4,0,bytes(8192))])
        for changed in (raw[:-1],raw+b"x",b"BAD1"+raw[4:],raw[:4]+struct.pack(">I",0)+raw[8:]):
            with self.assertRaises(ValueError): output_frames(changed)
        for row in (dict(status=True,growth=0,image=b""),dict(status=0,growth=-1,image=b""),dict(status=0,growth=0,image="")):
            with self.assertRaises(region.Failure): region.encode_states([row])

    def test_lossless_packing_rejects_corruption_tails_and_bombs(self):
        original=region.packed(bytes(1024));self.assertEqual(region.unpack(original,1024),bytes(1024))
        stream=base64.b64decode(original["zlib_base64"])
        for name in ("digest","length","base64","truncated","tail","member","bomb"):
            row=original.copy()
            if name=="digest": row["sha256"]="0"*64
            elif name=="length": row["bytes"]-=1
            elif name=="base64": row["zlib_base64"]="!"
            else:
                changed=stream[:-1] if name=="truncated" else stream+b"x" if name=="tail" else stream+stream
                if name=="bomb": changed=zlib.compress(bytes(1048576))
                row["zlib_base64"]=base64.b64encode(changed).decode()
            with self.subTest(name=name),self.assertRaises(region.Failure): region.unpack(row,1024)
        for raw in (b'{"x":1,"x":2}',b'{"x":NaN}',b'{"x":1e999}'):
            with self.assertRaises(region.Failure): region.strict_json(raw)

    def test_process_schema_rejects_numeric_aliases_extras_and_unbounded_duration(self):
        process=dict(command=["tool"],returncode=0,timed_out=False,stop_reason=None,seconds=0.,
                     stdout_file="compile.stdout",stderr_file="compile.stderr",
                     stdout=region.blob(b""),stderr=region.blob(b""))
        region.successful(process,label="compile",stdout_limit=32768)
        changes=[lambda x:x.update(extra="C:/unattributed/path"),
                 lambda x:x.update(returncode=False),lambda x:x.update(returncode=0.0),
                 lambda x:x.update(command="tool"),lambda x:x.update(command=[True]),
                 lambda x:x.update(command=[]),lambda x:x.update(command=["x"*4097]),
                 lambda x:x["stdout"].update(bytes=False),lambda x:x["stderr"].update(bytes=0.0),
                 lambda x:x["stdout"].update(extra="unattributed fingerprint"),
                 lambda x:x.update(stdout_file=7),lambda x:x.pop("stderr")]
        for mutate in changes:
            value=copy.deepcopy(process);mutate(value)
            with self.assertRaises(region.Failure): region.successful(value,label="compile",stdout_limit=32768)
        for seconds in (True,-1,120.01,float("nan"),float("inf"),float("-inf"),10**400):
            value=copy.deepcopy(process);value["seconds"]=seconds
            with self.subTest(seconds=seconds),self.assertRaises(region.Failure): region.successful(value)
        for field in ("stdout","stderr"):
            value=copy.deepcopy(process);value[field]["bytes"]=32769
            with self.assertRaises(region.Failure): region.successful(value,stdout_limit=32768)

    def test_exported_streams_require_canonical_files_even_for_identical_empty_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for label in ("compile","java","javac","child"):
                process=dict(command=["tool"],returncode=0,timed_out=False,stop_reason=None,seconds=0.,
                             stdout_file=label+".stdout",stderr_file=label+".stderr",
                             stdout=region.blob(b""),stderr=region.blob(b""))
                for field in ("stdout","stderr"): (root/process[field+"_file"]).write_bytes(b"")
                self.assertEqual(exporter.retained_streams(root,process,label),{"stdout":b"","stderr":b""})
                for field in ("stdout","stderr"):
                    value=copy.deepcopy(process);canonical=root/process[field+"_file"]
                    alias=root/(label+"-alias."+field);alias.write_bytes(b"")
                    value[field+"_file"]=alias.name;canonical.unlink()
                    with self.subTest(label=label,field=field),self.assertRaisesRegex(region.Failure,"canonical .* stream identity"):
                        exporter.retained_streams(root,value,label)
                    canonical.write_bytes(b"")
                swapped=copy.deepcopy(process)
                swapped["stdout_file"],swapped["stderr_file"]=swapped["stderr_file"],swapped["stdout_file"]
                with self.assertRaisesRegex(region.Failure,"canonical .* stdout stream identity"):
                    exporter.retained_streams(root,swapped,label)

    def test_artifact_fingerprints_are_typed_before_empty_stream_equality(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"compile.stdout").write_bytes(b"")
            self.assertEqual(exporter.artifact(root,"compile.stdout",region.blob(b"")),b"")
            for count in (False,0.0):
                expected=dict(region.blob(b""),bytes=count)
                with self.assertRaisesRegex(region.Failure,"fingerprint bytes"):
                    exporter.artifact(root,"compile.stdout",expected)


class FixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.fixture,cls.cases=region.load_fixture(FIXTURE)

    def test_all_four_continuous_profiles_derive_without_java_or_jars(self):
        with mock.patch.object(region,"process",side_effect=AssertionError("no process")),mock.patch.object(region,"class_records",side_effect=AssertionError("no JAR")):
            _,cases=region.load_fixture(FIXTURE)
        self.assertEqual(len(cases),4)
        for row in cases:
            mode,initial,writes=input_frames(row["input"]);states=output_frames(row["expected"])
            self.assertEqual(len(writes),12 if mode==0 else 5);self.assertEqual(len(states),len(writes)+1)
            self.assertEqual(states[0][1],8192 if mode==0 else 0)
            self.assertEqual(states[-2][0],4);self.assertEqual(states[-1][0],4)
            self.assertEqual(states[-3][2],states[-2][2]);self.assertEqual(states[-2][2],states[-1][2])
            self.assertEqual(initial,b"" if mode==0 else states[0][2])

    def test_corpus_omissions_duplicates_sides_order_and_counts_fail(self):
        for name in ("missing","duplicate","side","profile","count","index","extra"):
            value=copy.deepcopy(self.fixture)
            if name=="missing": value["cases"].pop()
            elif name=="duplicate": value["cases"][1]=copy.deepcopy(value["cases"][0])
            elif name=="extra": value["cases"].append(copy.deepcopy(value["cases"][0]))
            else: value["cases"][0][name if name!="count" else "writes"]={"side":"unknown","profile":"unknown","count":11,"index":True}[name]
            with self.subTest(name=name),self.assertRaises(region.Failure): region.validate_public_metadata(value)

    def test_incomplete_or_weak_provenance_and_class_bindings_fail(self):
        for name in ("mode","jar","source","source_digest","exporter","helper","class_version","class_version_float","original_class","tool","counts","controls"):
            value=copy.deepcopy(self.fixture);p=value["provenance"]
            if name=="mode": p["mode"]="compare"
            elif name=="jar": p["jar_sha256"]["client"]="0"*64
            elif name=="source": del p["sources"]["CMakeLists.txt"]
            elif name=="source_digest": p["sources"]["CMakeLists.txt"]["sha256"]="invalid"
            elif name=="exporter": p["exporter_sha256"]="0"*64
            elif name=="helper": p["helper_classes"]={}
            elif name=="class_version": p["helper_class_version"]=49
            elif name=="class_version_float": p["helper_class_version"]=61.0
            elif name=="original_class": p["original_classes"]["client"]["qj"]["sha256"]="0"*64
            elif name=="tool": p["tools"]["java"]["major"]=8
            elif name=="counts": p["counts"]["replay_writes"]=True
            else: p["controls"]={}
            with self.subTest(name=name),self.assertRaises(region.Failure): region.validate_public_metadata(value)

    def test_repacked_input_and_expected_bytes_are_rederived_from_originals(self):
        for name in ("input","expected"):
            value=copy.deepcopy(self.fixture);raw=bytearray(region.unpack(value["cases"][0][name],region.MAX_OUTPUT));raw[-1]^=1
            value["cases"][0][name]=region.packed(bytes(raw))
            with self.subTest(name=name),self.assertRaises(region.Failure): region.validate_fixture_value(value)

    def test_actual_protocol_method_origin_schedule_and_owner_mutations_fail(self):
        def mutation(record,name):
            if name=="descriptor": record["bindings"]["methods"]["commit"]["descriptor"]="()V"
            elif name=="origin": record["class_origins"]["qj"]="supplied:server"
            elif name=="omission": record["events"].pop(0)
            elif name=="missing_events": del record["events"]
            elif name=="bad_snapshot_files": next(e for e in record["events"] if e["kind"]=="snapshot")["files"]=None
            elif name=="order": record["events"][0],record["events"][1]=record["events"][1],record["events"][0]
            elif name=="cache_owner": next(e for e in record["events"] if e["kind"]=="snapshot" and e["label"]=="cache-full-256")["state"]["cache_entries_raw_order"][0]["object"]=0
            elif name=="closed_owner": next(e for e in record["final_state"]["held_regions"] if e["object"]==7)["locations"][0]^=256
            elif name=="time":
                row=next(e for e in record["events"] if e["kind"]=="stream_close" and e["label"]=="A-same-sector-rewrite");row["before_ms"]+=60000;row["after_ms"]+=60000
            else: next(e for e in record["events"] if e["kind"]=="read")["payload"]["sha256"]="0"*64
        for name in ("descriptor","origin","omission","missing_events","bad_snapshot_files","order","cache_owner","closed_owner","time","read_payload"):
            value=change_record(self.fixture,"client","observed",lambda r:mutation(r,name))
            with self.subTest(name=name),self.assertRaises(region.Failure): region.validate_fixture_value(value)

    def test_public_raw_blob_and_size_guards_are_lossless(self):
        for name in ("missing","digest","extra"):
            value=copy.deepcopy(self.fixture);first=next(iter(value["blobs"]))
            if name=="missing": del value["blobs"][first]
            elif name=="digest": value["blobs"][first]["sha256"]="0"*64
            else: value["blobs"]["0"*64]=region.packed(b"unattributed")
            with self.subTest(name=name),self.assertRaises(region.Failure): region.validate_fixture_value(value)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"oversized.json"
            with path.open("wb") as stream: stream.truncate(region.MAX_FIXTURE+1)
            with self.assertRaises(region.Failure): region.load_fixture(path)


class ExecutionTests(unittest.TestCase):
    def guard(self,scenario):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);java=root/"java";javac=root/"javac";client=root/"client.jar";server=root/"server.jar"
            for path in (java,javac,client,server): path.write_bytes(path.name.encode())
            real_fingerprint=region.fingerprint
            def fp(path,limit=region.MAX_RECORD):
                path=Path(path)
                if path in (client,server) and path.read_bytes()==path.name.encode():
                    return {"bytes":path.stat().st_size,"sha256":region.JAR_HASHES["client" if path==client else "server"]}
                return real_fingerprint(path,limit)
            def process(command,where,label,timeout=90,stdin=None,stdout_limit=32768):
                command=list(map(str,command));stdout=b"";stderr=b"";code=0
                if label in ("java","javac"):
                    stdout=("openjdk version \"17.0.17\"\r\n" if label=="java" else "javac 17.0.17\r\n").encode()
                    if scenario=="version77": code=77
                elif label=="compile":
                    self.assertEqual(command[1:6],["--release","17","-encoding","UTF-8","-d"])
                    if scenario=="compile77": code=77
                    else: (where/"classes/RegionWriteObservation.class").write_bytes(bytes.fromhex("cafebabe0000003d")+b"constructed guard class")
                else:
                    self.assertEqual(command[1:5],["-Xmx512m","-cp",str(where.parent/"classes"),"RegionWriteObservation"])
                    code=77
                    if scenario=="helper_changed":
                        with (where.parent/"classes/RegionWriteObservation.class").open("ab") as stream: stream.write(b"changed")
                    elif scenario=="helper_truncated":
                        (where.parent/"classes/RegionWriteObservation.class").write_bytes(bytes.fromhex("cafebabe"))
                for name,raw in (("stdout",stdout),("stderr",stderr)): (where/(label+"."+name)).write_bytes(raw)
                result=dict(command=command,returncode=code,timed_out=False,stop_reason=None,seconds=0.,stdout_file=label+".stdout",stderr_file=label+".stderr",
                            stdout=real_fingerprint(where/(label+".stdout")),stderr=real_fingerprint(where/(label+".stderr")))
                if label=="compile" and scenario=="compiler_numeric_alias": result["stdout"]["bytes"]=False
                return result
            args=["--mode","original-only","--client",str(client),"--server",str(server),"--java",str(java),"--javac",str(javac),"--work-dir",str(root/"work")]
            env={k:v for k,v in os.environ.items() if k not in region.JAVA_ENVIRONMENT}
            if scenario=="environment": env["JAVA_TOOL_OPTIONS"]="-Dconstructed=true"
            if scenario=="wrong_jar": client.write_bytes(b"wrong")
            if scenario=="source_id": args += ["--tested-commit","not-a-source-id"]
            def missing_tool(*unused):
                if scenario=="changed_before77": client.write_bytes(b"changed")
                raise region.Unavailable("constructed missing JDK")
            patches=[mock.patch.dict(os.environ,env,clear=True),mock.patch.object(region,"fingerprint",side_effect=fp),
                     mock.patch.object(region,"process",side_effect=process),mock.patch.object(region,"class_records",return_value={})]
            if scenario in ("missing_tool","changed_before77"): patches.append(mock.patch.object(region,"tool_path",side_effect=missing_tool))
            if scenario=="renamed_source": patches.append(mock.patch.object(region,"__file__",str(root/"renamed.py")))
            with contextlib.ExitStack() as stack:
                for patch in patches: stack.enter_context(patch)
                errors=io.StringIO()
                with contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(errors): code=region.main(args)
            reports=list((root/"work").glob("capture-*/manifest.json"))
            if reports:
                self.assertEqual(len(reports),1)
                return code,json.loads(reports[0].read_bytes())
            self.assertFalse((root/"work").exists())
            return code,json.loads(errors.getvalue())

    def test_missing_prerequisite_is_unavailable(self):
        region._latest_process = {"stale": "prior invocation"}
        code,row=self.guard("missing_tool");self.assertEqual(code,77);self.assertEqual(row["status"],"unavailable")
        self.assertIsNone(region._latest_process);self.assertNotIn("failed_process",row)
    def test_failed_tool_process_is_not_unavailable(self):
        code,row=self.guard("version77");self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
    def test_compile77_is_failure(self):
        code,row=self.guard("compile77");self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
    def test_original_constructor77_never_receives_a_setup_exception(self):
        code,row=self.guard("child77");self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
        self.assertEqual(row["tools"]["java"]["output"],"openjdk version \"17.0.17\"\r\n")
        self.assertEqual(row["tools"]["javac"]["output"],"javac 17.0.17\r\n")
    def test_wrong_archive_and_java_environment_fail_before_unavailable(self):
        for name in ("wrong_jar","environment"):
            with self.subTest(name=name): code,row=self.guard(name);self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
    def test_invalid_source_label_and_renamed_driver_fail(self):
        for name in ("source_id","renamed_source"):
            with self.subTest(name=name): code,row=self.guard(name);self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
    def test_preservation_failure_overrides_an_earlier_unavailable(self):
        code,row=self.guard("changed_before77");self.assertEqual(code,1);self.assertTrue(any("preservation" in e for e in row["errors"]))
    def test_class_bytes_cannot_change_even_after_child_failure(self):
        code,row=self.guard("helper_changed");self.assertEqual(code,1);self.assertTrue(any("helper preservation" in e for e in row["errors"]))
    def test_truncated_helper_header_retains_failure_manifest_after_child_failure(self):
        code,row=self.guard("helper_truncated");self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
        self.assertTrue(any("Java17 helper class version" in e for e in row["errors"]))
    def test_numeric_compiler_stream_fingerprint_fails_before_original_execution(self):
        code,row=self.guard("compiler_numeric_alias");self.assertEqual(code,1);self.assertEqual(row["status"],"fail")
        self.assertTrue(any("fingerprint bytes" in e for e in row["errors"]))
        self.assertNotIn("classes_before",row)


class ProcessObservationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="mc-region-process-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def fake_child(self, returncode, *, running=False):
        child = mock.Mock()
        child.returncode = None if running else returncode
        child.poll.side_effect = lambda: child.returncode
        child.kill.side_effect = lambda: setattr(child, "returncode", returncode)
        child.wait.side_effect = lambda **kwargs: child.returncode
        return child

    def test_live_inventory_disappearance_at_real_process_callsite(self):
        victim = self.root / "disappearing"
        victim.write_bytes(b"owned temporary artifact")
        child = self.fake_child(0)
        child.poll.side_effect = [None, 0]
        real_stat, real_inventory = os.stat, region.inventory
        removed, phases = [], []

        def disappearing_stat(path, *args, **kwargs):
            if Path(path) == victim and not removed:
                victim.unlink()
                removed.append(True)
            return real_stat(path, *args, **kwargs)

        def inventory(*args, **kwargs):
            phases.append((kwargs.get("live_poll", False), child.wait.called))
            return real_inventory(*args, **kwargs)

        with mock.patch.object(region.subprocess, "Popen", return_value=child), \
             mock.patch.object(region.time, "sleep"), \
             mock.patch.object(region.os, "stat", side_effect=disappearing_stat), \
             mock.patch.object(region, "inventory", side_effect=inventory):
            result = region.process(["constructed-owned-child"], self.root, "disappearing")
        self.assertEqual(result["returncode"], 0)
        self.assertEqual(removed, [True])
        self.assertEqual(phases, [(True, False), (False, True)])
        child.kill.assert_not_called()
        child.wait.assert_called_once_with(timeout=10)
        self.assertIsNone(region._latest_process["supervision_error"])

    def test_live_inventory_never_hashes_or_hides_nonmissing_errors(self):
        with self.assertRaisesRegex(region.Failure, "cannot hash final state"):
            region.inventory(self.root, hashes=True, live_poll=True)
        bad = self.root / "entry"
        bad.write_bytes(b"x")
        real_stat = os.stat
        for kind in ("permission", "not-directory", "symlink", "reparse", "special"):
            child = self.fake_child(-9, running=True)

            def invalid_stat(path, *args, **kwargs):
                if Path(path) != bad:
                    return real_stat(path, *args, **kwargs)
                if kind == "permission":
                    raise PermissionError("controlled denied stat")
                if kind == "not-directory":
                    raise NotADirectoryError("controlled replaced directory")
                actual = real_stat(path, *args, **kwargs)
                if kind != "reparse":
                    class Typed:
                        st_mode = 0o120000 if kind == "symlink" else 0o010000
                        st_size, st_file_attributes = actual.st_size, 0
                    return Typed()
                class Reparse:
                    st_mode, st_size, st_file_attributes = actual.st_mode, actual.st_size, 0x400
                return Reparse()

            error = PermissionError if kind == "permission" else NotADirectoryError if kind == "not-directory" else region.Failure
            with self.subTest(kind=kind), \
                 mock.patch.object(region.subprocess, "Popen", return_value=child), \
                 mock.patch.object(region.os, "stat", side_effect=invalid_stat):
                with self.assertRaises(error):
                    region.process(["constructed-owned-child"], self.root, kind)
            child.kill.assert_called_once()
            child.wait.assert_called_once_with(timeout=10)
            self.assertEqual(region._latest_process["returncode"], -9)
            self.assertIsNotNone(region._latest_process["supervision_error"])

    def test_live_inventory_budget_counts_disappeared_entries_and_bytes(self):
        bad = self.root / "large"
        bad.write_bytes(b"x")
        real_stat, real_scandir = os.stat, os.scandir

        class Entries:
            def __enter__(self):
                return self
            def __exit__(self, *unused):
                return False
            def __iter__(self):
                for index in range(6001):
                    class Entry:
                        path = str(bad.parent / ("missing-" + str(index)))
                    yield Entry()

        for kind in ("missing-count", "bytes"):
            child = self.fake_child(-9, running=True)

            def resource_stat(path, *args, **kwargs):
                path = Path(path)
                if kind == "missing-count" and path.name.startswith("missing-") and path.name[8:].isdigit():
                    raise FileNotFoundError("controlled disappeared entry")
                info = real_stat(path, *args, **kwargs)
                if kind == "bytes" and path == bad:
                    class Large:
                        st_mode, st_size, st_file_attributes = info.st_mode, 160 * 1024 * 1024 + 1, 0
                    return Large()
                return info

            def entries(path):
                return Entries() if kind == "missing-count" and Path(path) == self.root else real_scandir(path)

            with self.subTest(kind=kind), \
                 mock.patch.object(region.subprocess, "Popen", return_value=child), \
                 mock.patch.object(region.os, "stat", side_effect=resource_stat), \
                 mock.patch.object(region.os, "scandir", side_effect=entries):
                with self.assertRaisesRegex(region.Failure, "bounded"):
                    region.process(["constructed-owned-child"], self.root, kind)
            child.kill.assert_called_once()
            child.wait.assert_called_once_with(timeout=10)
            self.assertEqual(region._latest_process["returncode"], -9)

    def test_strict_final_inventory_after_reap_rejects_types_errors_and_byte_limit(self):
        real_file, real_dir, real_link, real_stat = Path.is_file, Path.is_dir, Path.is_symlink, Path.stat
        for kind in ("special", "link", "permission", "bytes"):
            case = self.root / kind
            case.mkdir()
            bad = case / "final-entry"
            child = self.fake_child(0)
            count = [0]

            def poll():
                count[0] += 1
                if count[0] == 1:
                    return None
                bad.write_bytes(b"appears only as child exits")
                return 0

            child.poll.side_effect = poll
            def is_file(path):
                return False if Path(path) == bad and kind == "special" else real_file(path)
            def is_dir(path):
                return False if Path(path) == bad and kind == "special" else real_dir(path)
            def is_link(path):
                return True if Path(path) == bad and kind == "link" else real_link(path)
            def final_stat(path, *args, **kwargs):
                if Path(path) == bad and kind == "permission":
                    raise PermissionError("controlled final inventory failure")
                info = real_stat(path, *args, **kwargs)
                if Path(path) == bad and kind == "bytes":
                    values = list(info)
                    values[6] = 160 * 1024 * 1024 + 1
                    return os.stat_result(values)
                return info

            expected = PermissionError if kind == "permission" else region.Failure
            with self.subTest(kind=kind), \
                 mock.patch.object(region.subprocess, "Popen", return_value=child), \
                 mock.patch.object(region.time, "sleep"), \
                 mock.patch.object(Path, "is_file", is_file), \
                 mock.patch.object(Path, "is_dir", is_dir), \
                 mock.patch.object(Path, "is_symlink", is_link), \
                 mock.patch.object(Path, "stat", final_stat):
                with self.assertRaises(expected):
                    region.process(["constructed-owned-child"], case, "final")
            self.assertTrue(child.wait.called)
            self.assertEqual(region._latest_process["returncode"], 0)
            self.assertIsNotNone(region._latest_process["supervision_error"])


    def test_actual_child_result_survives_timeout_and_stream_validation(self):
        for name, code, running in (("nonzero", 7, False), ("timeout", -9, True)):
            child = self.fake_child(code, running=running)
            with mock.patch.object(region.subprocess, "Popen", return_value=child), \
                 mock.patch.object(region.time, "monotonic", side_effect=iter(range(10))):
                result = region.process(["owned-child"], self.root, name, timeout=.1)
            observed = region._latest_process
            self.assertEqual((observed["returncode"], observed["timed_out"], observed["stop_reason"]),
                             (code, running, "deadline" if running else None))
            self.assertEqual((observed["stdout_observed"], observed["stderr_observed"]), (0, 0))
            self.assertEqual(observed["command"], ["owned-child"])
            self.assertNotIn("failed_process", result)
            with self.assertRaisesRegex(region.Failure, "complete successful process"):
                region.successful(result)
        child = self.fake_child(0)
        with mock.patch.object(region.subprocess, "Popen", return_value=child), \
             mock.patch.object(region, "fingerprint", side_effect=OSError("owned hashing failure")):
            with self.assertRaisesRegex(OSError, "hashing failure"):
                region.process(["owned-child"], self.root, "hashing")
        self.assertEqual(region._latest_process["returncode"], 0)
        self.assertEqual(region._latest_process["label"], "hashing")
        def oversized(*args, **kwargs):
            kwargs['stdout'].write(b'oversized')
            kwargs['stdout'].flush()
            return self.fake_child(0)
        with mock.patch.object(region.subprocess, "Popen", side_effect=oversized):
            with self.assertRaisesRegex(region.Failure, "bounded complete child streams"):
                region.process(["owned-child"], self.root, "oversized", stdout_limit=4)
        self.assertEqual(region._latest_process["returncode"], 0)
        self.assertEqual(region._latest_process["stdout_observed"], 9)

    def test_supervision_exception_is_observed_after_reaping(self):
        child = self.fake_child(-9, running=True)
        with mock.patch.object(region.subprocess, "Popen", return_value=child), \
             mock.patch.object(region, "inventory", side_effect=region.Failure("owned inventory failure")):
            with self.assertRaisesRegex(region.Failure, "owned inventory failure"):
                region.process(["owned-child"], self.root, "supervision")
        child.kill.assert_called_once()
        child.wait.assert_called_once_with(timeout=10)
        observed = region._latest_process
        self.assertEqual(observed["returncode"], -9)
        self.assertEqual(observed["inventory_polls"], 1)
        self.assertEqual(observed["supervision_error"],
                         {"class": "Failure", "message": "owned inventory failure"})

    def test_main_retains_failure_only_and_resets_each_invocation(self):
        def prepare(args, report):
            report["inputs_before"] = {}
            return ()
        def run(args, directory, report, prepared):
            result = region.process(["owned-child"], directory, "child")
            region.successful(result)
        args = ["--mode", "original-only", "--work-dir", str(self.root/"captures")]
        for code in (7, 0):
            before = set((self.root/"captures").glob("capture-*/manifest.json"))
            with mock.patch.object(region, "prepare", side_effect=prepare), \
                 mock.patch.object(region, "run", side_effect=run), \
                 mock.patch.object(region.subprocess, "Popen", return_value=self.fake_child(code)), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(region.main(args), 1 if code else 0)
            path, = set((self.root/"captures").glob("capture-*/manifest.json")) - before
            report = json.loads(path.read_bytes())
            self.assertEqual("failed_process" in report, bool(code))
            if code:
                self.assertEqual(report["failed_process"]["returncode"], code)
                self.assertEqual(report["failed_process"]["label"], "child")
        stderr = io.StringIO()
        with mock.patch.object(region, "prepare", side_effect=region.Unavailable("owned missing JDK")), \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
            self.assertEqual(region.main(args), 77)
        self.assertNotIn("failed_process", json.loads(stderr.getvalue()))
        self.assertIsNone(region._latest_process)


class OutputPathTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="mc-region-output-");self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();self.tools={};self.jars={}
        for name in ("java","javac"):
            path=self.root/"jdk/bin"/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"synthetic tool");self.tools[name]=path
        for side in region.JAR_HASHES:
            path=self.root/(side+".jar");path.write_bytes(b"synthetic archive");self.jars[side]=path

    def preflight(self,work,*,missing=False,overrides=False):
        original=region.fingerprint
        def fingerprint(path,limit=region.MAX_RECORD):
            value=original(path,limit)
            for side,jar in self.jars.items():
                if Path(path).resolve()==jar.resolve():value["sha256"]=region.JAR_HASHES[side]
            return value
        def tool(selected,name):
            if missing:raise region.Unavailable("missing synthetic JDK")
            return self.tools[name]
        args=["--mode","original-only","--client",str(self.jars["client"]),"--server",str(self.jars["server"]),"--work-dir",str(work)]
        if overrides:
            for name,path in self.tools.items():args += ["--"+name,str(path)]
        stdout,stderr=io.StringIO(),io.StringIO()
        env={k:v for k,v in os.environ.items() if k not in region.JAVA_ENVIRONMENT}
        with mock.patch.dict(os.environ,env,clear=True),mock.patch.object(region,"fingerprint",side_effect=fingerprint), \
             mock.patch.object(region,"tool_path",side_effect=tool),mock.patch.object(region,"process") as child, \
             contextlib.redirect_stdout(stdout),contextlib.redirect_stderr(stderr):
            code=region.main(args)
        child.assert_not_called();self.assertEqual(stdout.getvalue(),"")
        return code,json.loads(stderr.getvalue())

    def test_public_new_descendant_is_refused_before_output_creation(self):
        project=self.root/"project";work=project/"src/new-reference-output"
        with mock.patch.object(region,"ROOT",project):code,report=self.preflight(work)
        self.assertEqual((code,report["status"]),(1,"fail"));self.assertIn("public source/input layout",report["errors"][0])
        self.assertFalse(project.exists())

    def test_default_and_override_tool_roots_are_protected_before_mkdir(self):
        for overrides in (False,True):
            before=sorted(path.relative_to(self.root/"jdk").as_posix() for path in (self.root/"jdk").rglob("*"))
            code,report=self.preflight(self.root/"jdk",overrides=overrides)
            self.assertEqual((code,report["status"]),(1,"fail"));self.assertTrue(any("attributed input" in value for value in report["errors"]))
            self.assertEqual(before,sorted(path.relative_to(self.root/"jdk").as_posix() for path in (self.root/"jdk").rglob("*")))

    def test_input_file_output_and_missing_prerequisite_emit_json_without_captures(self):
        original=self.jars["client"].read_bytes();code,report=self.preflight(self.jars["client"])
        self.assertEqual((code,report["status"]),(1,"fail"));self.assertEqual(self.jars["client"].read_bytes(),original)
        work=self.root/"missing-captures";code,report=self.preflight(work,missing=True)
        self.assertEqual((code,report["status"]),(77,"unavailable"));self.assertFalse(work.exists())

    def test_static_link_or_junction_output_is_refused(self):
        target,link=self.root/"owned-target",self.root/"linked-output";target.mkdir()
        try:link.symlink_to(target,target_is_directory=True)
        except OSError:
            if os.name!="nt":self.skipTest("directory links are unavailable")
            import _winapi
            create=getattr(_winapi,"CreateJunction",None)
            if create is None:self.skipTest("directory links and junctions are unavailable")
            try:create(str(target),str(link))
            except OSError:self.skipTest("directory links and junctions are unavailable")
        code,report=self.preflight(link/"nested")
        self.assertEqual((code,report["status"]),(1,"fail"));self.assertIn("link/reparse",report["errors"][0]);self.assertEqual(list(target.iterdir()),[])


def run_probe(executable,raw):
    with tempfile.TemporaryFile() as output,tempfile.TemporaryFile() as errors,tempfile.TemporaryFile() as stdin:
        stdin.write(raw);stdin.seek(0)
        child=subprocess.run([str(executable)],stdin=stdin,stdout=output,stderr=errors,timeout=30,check=False)
        region.check(output.tell()<=region.MAX_OUTPUT and errors.tell()<=32768,"bounded C replay streams")
        output.seek(0);errors.seek(0);return child.returncode,output.read(),errors.read()


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--executable",type=Path,required=True);parser.add_argument("--fixtures",type=Path,default=FIXTURE)
    args=parser.parse_args(argv);_,cases=region.load_fixture(args.fixtures)
    for row in cases:
        status,output,errors=run_probe(args.executable,row["input"])
        region.check(status==0 and not errors and output==row["expected"],"complete original/C image write profile")
    raw=cases[0]["input"]
    malformed=[b"BAD1"+raw[4:],raw[:-1],raw+b"x",raw[:4]+b"\x02"+raw[5:],raw[:9]+struct.pack(">I",33)+raw[13:],raw[:5]+struct.pack(">I",1)+raw[9:]]
    for changed in malformed:
        status,output,_=run_probe(args.executable,changed);region.check(status==2 and not output,"malformed MRW1 rejected before any state output")
    # Constructed C contracts are separate from original parity: valid framing with domain failures.
    constructed=[("fresh",b"",[dict(x=-1,z=0,timestamp=0,payload=b"x")],[(0,8192,bytes(8192)),(2,0,bytes(8192))]),
                 ("import-copy",bytes(8192-1),[],[(3,0,b"")]),("fresh",b"",[],[(0,8192,bytes(8192))])]
    for mode,initial,writes,expected in constructed:
        status,output,errors=run_probe(args.executable,region.encode_replay(mode,initial,writes))
        region.check(status==0 and not errors and output_frames(output)==expected,"constructed C status framing and unchanged-image contract")
    print("region writer: 4 original-derived profiles,34 writes,complete image/growth frames;6 malformed and3 constructed domain guards")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
