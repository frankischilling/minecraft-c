"""Pure admission controls. Synthetic parser inputs are never game evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location("torch_packet_test",HERE/"packet.py")
P=importlib.util.module_from_spec(spec);spec.loader.exec_module(P)


class PureControls(unittest.TestCase):
    def rejected(self,function,*args):
        with self.assertRaises((AssertionError,ValueError,KeyError,TypeError,IndexError,OSError)):
            function(*args)

    def test_json_and_literal_types(self):
        for wire in ('{"a":1,"a":2}','{"x":NaN}','{"x":Infinity}','{"x":-Infinity}'):
            with self.subTest(wire=wire):self.rejected(P.loads,wire)
        self.assertFalse(P.same(True,1));self.assertFalse(P.same([False],[0]))
        self.assertFalse(P.same({"value":True},{"value":1}))
        self.assertTrue(P.same({"value":[1,False,None]},{"value":[1,False,None]}))

    def test_authored_inputs_and_helper(self):
        declaration=P.validate_inputs()
        self.assertEqual([len(declaration["cohorts"][c]) for c in P.COHORTS],[70,32,78,23])
        self.assertEqual(P.sha(HERE/"RedstoneTorchObservation.java"),
                         "88cd0df874ba4e6c447e8ca7b94e6770df19c70d9087827d92c86fa6c044d66a")
        self.assertNotIn(b".local/",(HERE/"entropy.py").read_bytes())

    def test_world_producer_copies_exact_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"fresh"
            self.assertEqual(len(P.W.create(path)),6)
            for role in P.W.audit().values():
                for name,pin in role["files"].items():self.assertEqual(P.sha(path/role["name"]/name),pin)
            self.rejected(P.W.create,path)
            self.rejected(P.W.create,Path(directory)/"bad",[True])
            self.rejected(P.W.create,Path(directory)/"bad",[0,0])

    def test_exact_byte_attributes_precede_capture(self):
        text=(HERE/".gitattributes").read_text()
        self.assertLess(text.index("*.json text"),text.index("*.capture.json -text"))
        for item in ("*.stdout -text","*.stderr -text","*.capture.json -text","world-inputs/** -text"):
            self.assertIn(item,text)

    def test_process_parser_corruptions(self):
        raw=b"synthetic-parser-control\n"
        receipt=dict(command=["declared-parser-input"],exit=0,timeout_seconds=90,timed_out=False,
                     pipe_capture_complete=True,reader_errors={},output_overflow=False,
                     stdout_file="original.stdout",stderr_file="original.stderr",
                     stdout_bytes=len(raw),stderr_bytes=0,stdout_sha256=__import__("hashlib").sha256(raw).hexdigest(),
                     stderr_sha256=__import__("hashlib").sha256(b"").hexdigest(),
                     stdout_limit=4194304,stderr_limit=1048576)
        P.process(receipt,raw,b"","original",90)
        changes={"exit":True,"timeout_seconds":True,"timed_out":True,"pipe_capture_complete":1,
                 "reader_errors":{"stdout":["failure"]},"output_overflow":True,"stdout_bytes":True,
                 "stdout_sha256":"0"*64,"stdout_file":"../original.stdout","stdout_limit":1,
                 "command":[1],"unknown":0}
        for key,value in changes.items():
            bad=copy.deepcopy(receipt);bad[key]=value
            with self.subTest(key=key):self.rejected(P.process,bad,raw,b"","original",90)
        for key in receipt:
            bad=copy.deepcopy(receipt);del bad[key]
            with self.subTest(missing=key):self.rejected(P.process,bad,raw,b"","original",90)
        for key in ("stdout_limit","stderr_limit"):
            bad=copy.deepcopy(receipt);bad[key]=float(bad[key])
            with self.subTest(float_limit=key):self.rejected(P.process,bad,raw,b"","original",90)

    def test_manifest_binding_has_one_narrow_projection(self):
        raw=(HERE/"packet.py").read_bytes();unbound=P.unbound_packet(raw)
        self.assertEqual(P.unbound_packet(unbound),unbound)
        self.rejected(P.unbound_packet,unbound+ b"\nMANIFEST_SHA256 = 'UNFROZEN'\n")
        if P.MANIFEST_SHA256=="UNFROZEN":self.rejected(P.audit)

    def test_native_jdk_declaration_has_no_code(self):
        pins=P.load(HERE/"metadata-probe-pins.json")
        native=next(m for m in pins["classes"]["jdk/internal/misc/Unsafe"]["methods"]
                    if m["name"]=="shouldBeInitialized0")
        actual=dict(name=native["name"],descriptor=native["descriptor"],access=native["flags"])
        P.method_metadata(actual,native)
        for key,value in (("access",1),("code_bytes",0),("code_sha256","0"*64),("unknown",0)):
            changed=copy.deepcopy(actual);changed[key]=value
            with self.subTest(key=key):self.rejected(P.method_metadata,changed,native)

    def test_jdk_method_metadata_scalar_types(self):
        pins=P.load(HERE/"jdk-accounting-pins.json")
        method=next(m for m in pins["classes"]["java/util/Random"]["methods"]
                    if m["name"]=="<init>" and m["descriptor"]=="()V")
        actual=dict(name=method["name"],descriptor=method["descriptor"],access=method["flags"],
                    code_bytes=method["length"],code_sha256=method["sha256"])
        P.method_metadata(actual,method)
        for key,value in (("access",True),("code_bytes",True)):
            changed=copy.deepcopy(actual);changed[key]=value
            with self.subTest(key=key):self.rejected(P.method_metadata,changed,method)


@unittest.skipIf(P.MANIFEST_SHA256=="UNFROZEN","requires eight separately executed fresh public captures")
class CapturedControls(PureControls):
    @classmethod
    def setUpClass(cls):
        cls.cohorts=P.reference_cohorts()
        cls.declared=P.declared_inputs()

    def test_complete_fresh_admission(self):
        self.assertEqual(sum(len(x["rows"]) for x in self.cohorts.values()),203)
        self.assertFalse(P.audit()["full_entropy_qualification"])

    def test_actual_capture_tool_roles_require_arrays(self):
        original_load=P.load
        for role in ("libraries","java_dlls"):
            def changed(path):
                value=original_load(path)
                if Path(path).name=="capture.json":
                    value["paths"][role]={name:123 for name in value["paths"][role]}
                return value
            with self.subTest(role=role),patch.object(P,"load",side_effect=changed):
                self.rejected(P.admit_capture,HERE,HERE)

    def test_stdout_closed_order_and_schema(self):
        raw=(HERE/"lifecycle-ordinary-0/original.stdout").read_bytes()
        profiles=self.declared["cohorts"]["lifecycle"]
        roles=[self.declared["world_roles"]["0"]]
        lines=raw.decode("ascii").splitlines()
        at=next(i for i,line in enumerate(lines) if line.startswith("DB\t"))
        def encode(values):return ("\n".join(values)+"\n").encode("ascii")
        changes=[lines[:-1],lines+["UNKNOWN"],lines+[lines[-1]],lines[1:]]
        swap=lines.copy();swap[at],swap[at+1]=swap[at+1],swap[at];changes.append(swap)
        for key,value in (("index",True),("unknown",0),("result","true")):
            bad=lines.copy();row=P.loads(bad[at].split("\t",1)[1]);row[key]=value
            bad[at]="DB\t"+json.dumps(row,separators=(",",":"));changes.append(bad)
        bad=lines.copy();bad[at]=bad[at].replace('"index":0','"index":0,"index":0',1);changes.append(bad)
        for index,changed in enumerate(changes):
            with self.subTest(index=index):self.rejected(P.O.decode,encode(changed),"lifecycle",profiles,roles)

    def test_observed_stream_corruptions(self):
        cohort=self.cohorts["lifecycle"]
        def audit(rows,own):
            return P.module("torch_control_fresh_accounting",HERE/"accounting.py").audit(rows,own)
        audit(cohort["rows"],cohort["own"])
        mutations=[]
        def case(function):
            rows,own=copy.deepcopy((cohort["rows"],cohort["own"]));function(rows,own);mutations.append((rows,own))
        case(lambda r,e:e[0].__setitem__("mode",True))
        case(lambda r,e:e[0].__setitem__("index",False))
        case(lambda r,e:e[0].__setitem__("supplied_alias",True))
        case(lambda r,e:e[0].__setitem__("unknown",None))
        case(lambda r,e:e[0].__setitem__("seed_uniquifier_before","0"))
        case(lambda r,e:e[1].__setitem__("seed_uniquifier_before",format(int(e[1]["seed_uniquifier_before"],16)^1,"016x")))
        case(lambda r,e:e[58]["owners_after"]["worlds"][0]["state"].__setitem__("seed","000000000000"))
        case(lambda r,e:r[58]["values"].__setitem__(2,75))
        case(lambda r,e:e[58]["math_after"].__setitem__("have",True))
        cold=next(i for i,e in enumerate(cohort["own"]) if e["math_before"] is None and e["math_after"] is not None)
        case(lambda r,e:e[cold]["math_after"].__setitem__("gaussian","8000000000000000"))
        case(lambda r,e:e[cold]["math_after"].__setitem__("have",1))
        for index,(rows,own) in enumerate(mutations):
            with self.subTest(index=index):self.rejected(audit,rows,own)

    def test_source_topology_not_native_fabrication(self):
        row=copy.deepcopy(self.cohorts["lifecycle"]["rows"][0]);roles=[self.declared["world_roles"]["0"]]
        P.T.validate(row,roles)
        for key,value in (("size",True),("capacity",-1),("unknown",0)):
            changed=copy.deepcopy(row);changed["history_before"][key]=value
            with self.subTest(key=key):self.rejected(P.T.validate,changed,roles)


if __name__=="__main__":unittest.main()
