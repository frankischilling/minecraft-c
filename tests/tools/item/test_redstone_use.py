"""Compare 42 Item331 wrappers and 12 SM predicates with an admitted original packet."""
import argparse
import copy
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = ROOT / "tests/item/use/redstone"
INPUTS = ROOT / "tests/reference/item/redstone_use"
CALLBACK_NATIVE = "NATIVE_CALLBACK_REFUSAL"
POLICY_KEYS = {"index", "domain", "status", "native_policy", "source_exception", "source_error",
               "observer_refused", "source_pc", "class", "method", "descriptor", "operation",
               "world_status", "support_count", "predicate_query_count", "retained_producers"}
EFFECT_KEYS = set("changes clicked_after clicked_before events exception health_after health_before "
                  "index name queues_after queues_before registered_block_class registered_block_id "
                  "registered_item_class registered_item_slot registered_owners_same remote result "
                  "set_after set_before stack_after stack_before target_after target_before "
                  "unexpected_adds used331_delta used332_delta values world_o world_random_same "
                  "world_rng_unchanged".split())
OWN_KEYS = {"index", "name", "world_before", "world_after", "world_random_same", "unexpected_entities"}



def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result


C = module("rd_native_capture", ROOT / "tests/reference/client/controller/capture.py")
P = module("rd_native_world_inputs", ROOT / "tests/reference/client/picking/observe.py")
S = module("rd_native_stats_resources", ROOT / "tests/reference/client/player_statistics/support.py")
N = module("rd_native_products", ROOT / "tests/tools/client/native_product_capture.py")
KNOWN_NATIVE_PRELUDE = ["16 achievements", "151 recipes"]


def require(value, message):
    if not value:
        raise AssertionError(message)


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="ascii", newline="\n")


def tree(path):
    return {p.relative_to(path).as_posix(): C.digest(p) for p in sorted(path.rglob("*")) if p.is_file()}


def original_rows():
    """The packet admits all four unchanged-original runs before returning rows."""
    packet = module("rd_original_packet", INPUTS / "packet.py")
    result = packet.read_packet(INPUTS)
    require(set(result) == {"use", "support"} and len(result["use"]) == 42 and
            len(result["support"]) == 12, "complete admitted original packet")
    return result


def strict_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate native JSON key: " + key)
        result[key] = value
    return result


def invalid_constant(value):
    raise ValueError("non-JSON native constant: " + value)


def loads(value):
    return json.loads(value, object_pairs_hook=strict_object, parse_constant=invalid_constant)


def same(left, right):
    """Literal JSON equality without Python's bool/int or int/float coercion."""
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return set(left) == set(right) and all(same(left[key], right[key]) for key in left)
    if type(left) is list:
        return len(left) == len(right) and all(same(a, b) for a, b in zip(left, right))
    return left == right


def policy_types(value):
    require(type(value) is dict and set(value) == POLICY_KEYS, "exact native policy schema")
    for key in ("index", "status", "source_pc", "world_status", "support_count",
                "predicate_query_count", "retained_producers"):
        require(type(value[key]) is int, "native policy integer: " + key)
    for key in ("native_policy", "source_exception", "source_error", "observer_refused"):
        require(type(value[key]) is bool, "native policy boolean: " + key)
    require(type(value["domain"]) is str, "native policy domain string")
    for key in ("class", "method", "descriptor", "operation"):
        require(value[key] is None or type(value[key]) is str, "native policy string/null: " + key)


def audit_entropy(rows, own):
    require(same([[r["index"],r["name"]] for r in rows], [[r["index"],r["name"]] for r in own]), "own-stream identity/order")
    for effect,raw in zip(rows,own):
        require(set(raw) == OWN_KEYS, "own-stream exact schema")
        require(raw["world_random_same"] is True and effect["world_random_same"] is True and effect["world_rng_unchanged"] is True,
                "actual Random owner identity/equality")
        require(same(raw["world_before"], raw["world_after"]) and same(raw["unexpected_entities"], []) and
                type(effect["unexpected_adds"]) is int and effect["unexpected_adds"] == 0,
                "no stream consumption or unexpected entity in this captured scope")
        for state in (raw["world_before"],raw["world_after"]):
            require(set(state) == {"seed","gaussian","have"} and isinstance(state["have"],bool), "actual Random state schema")
            require(re.fullmatch("[0-9a-f]{12}",state["seed"]) is not None and
                    re.fullmatch("[0-9a-f]{16}",state["gaussian"]) is not None, "actual Random raw bit fields")
    return len(rows)


def compare(cohort, expected, actual, native, own):
    count = 42 if cohort == "use" else 12
    require(len(expected) == len(actual) == len(native) == count, "complete literal/native cohort")
    audit_entropy(actual,own)
    callbacks = 0
    for reference,observed,policy in zip(expected,actual,native):
        policy_types(policy)
        require(set(observed) == set(reference) and set(policy) == POLICY_KEYS, "exact logical/native schemas")
        require(type(observed["index"]) is int, "logical record integer index")
        require(reference["index"] == observed["index"] == policy["index"] and policy["domain"] == ("item" if cohort=="use" else "placement"), "exact row/domain identity")
        callback = cohort == "use" and reference["values"][15] == 1
        if callback:
            require(reference["exception"] == "java.lang.IllegalStateException" and observed["exception"] == CALLBACK_NATIVE,
                    "disjoint original callback exception/native refusal")
            require(policy["status"] == 7 and policy["world_status"] == 7 and policy["native_policy"] is True and
                    policy["observer_refused"] is True and policy["source_exception"] is False and policy["source_error"] is False and
                    policy["source_pc"] == 126 and policy["class"] == "ie", "actual native callback cause")
            require(same({k:v for k,v in observed.items() if k!="exception"}, {k:v for k,v in reference.items() if k!="exception"}),
                    "complete callback effect prefix with exception-class equality unqualified")
            callbacks += 1
        else:
            require(same(observed, reference), "literal logical record differs: "+reference["name"])
            require(policy["observer_refused"] is False and policy["source_error"] is False, "no unexpected refusal/error")
            if reference["exception"] == "NONE":
                require(policy["status"] == 0 and policy["native_policy"] is False and policy["source_exception"] is False,
                        "ordinary return native policy")
                require(policy["source_pc"] == (39 if cohort=="use" else 10), "actual wrapper/base return site")
            else:
                require(reference["exception"] == "java.lang.NullPointerException" and policy["status"] != 0 and
                        policy["source_exception"] is True and policy["native_policy"] is False, "reached source NPE policy")
                if cohort == "support":
                    require(policy["class"] == "sm" and policy["source_pc"] == 7, "actual SM null-World prefix")
                else:
                    wanted = ("ie",7) if reference["values"][14]&1 else ("iz",1) if reference["values"][14]&2 else ("iz",34)
                    require((policy["class"],policy["source_pc"]) == wanted, "actual IE/IZ null prefix")
        if cohort == "support":
            support_count = 0 if reference["values"][5] else 1
        else:
            v = reference["values"]
            support_count = 0 if v[14]&3 or (v[10] != 78 and v[12] != 0) else 1
        require(policy["predicate_query_count"] == 0 and policy["support_count"] == support_count and
                type(policy["retained_producers"]) is int and policy["retained_producers"] >= 0, "retained support/query graph bounds")
    return {"literal_effect_rows":count,"full_outcomes":count-callbacks,"callback_prefixes":callbacks,
            "callback_exception_class_equality":"UNQUALIFIED" if callbacks else "not_reached"}


def parser_controls(expected):
    """Declared synthetic protocol controls only, never native observations/qualification."""
    rejected = 0
    for cohort,rows in expected.items():
        observed = copy.deepcopy(rows); native=[]; own=[]
        for row in rows:
            callback = cohort=="use" and row["values"][15]==1
            if callback: observed[row["index"]]["exception"] = CALLBACK_NATIVE
            normal = row["exception"] == "NONE"
            cls,pc = ("iz",39) if cohort=="use" else ("sm",10)
            if callback: cls,pc="ie",126
            elif not normal:
                cls,pc=("sm",7) if cohort=="support" else ("ie",7) if row["values"][14]&1 else ("iz",1) if row["values"][14]&2 else ("iz",34)
            native.append(dict(index=row["index"],domain="item" if cohort=="use" else "placement",status=7 if callback else 0 if normal else 9,
                native_policy=callback,source_exception=not normal and not callback,source_error=False,observer_refused=callback,
                source_pc=pc,**{"class":cls},method="a",descriptor="declared synthetic control",operation="synthetic" if not normal else None,
                world_status=7 if callback else 0,support_count=(0 if row["values"][5] else 1) if cohort=="support" else (0 if row["values"][14]&3 or (row["values"][10]!=78 and row["values"][12]!=0) else 1),predicate_query_count=0,retained_producers=0))
            own.append(dict(index=row["index"],name=row["name"],world_before=dict(seed="000000000001",gaussian="0000000000000000",have=False),
                world_after=dict(seed="000000000001",gaussian="0000000000000000",have=False),world_random_same=True,unexpected_entities=[]))
        compare(cohort,rows,observed,native,own)
        wire=list(KNOWN_NATIVE_PRELUDE)
        for a,n,o in zip(observed,native,own):
            wire += [cohort.upper()+"\t"+json.dumps(a),"OWN_ENTROPY\t"+json.dumps(o),"REDSTONE_NATIVE\t"+json.dumps(n)]
        wire += ["COMPLETE\t"+cohort+"\t"+str(len(rows)),"REDSTONE_NATIVE_COMPLETE\t"+cohort+"\t1"]
        parse_native(wire,cohort,len(rows))
        bad_wires=[wire[:2]+["UNKNOWN\tactual"]+wire[2:],wire+["extra"],wire[:2]+wire[3:],
                   wire[:2]+[wire[2]]+wire[2:],list(reversed(wire[:2]))+wire[2:],wire[:-1],
                   wire[:3]+[wire[4],wire[3]]+wire[5:]]
        # JSON is closed before comparison: duplicate keys at any depth,
        # unknown keys, non-object records, constants and bool indices fail.
        for position in (2, 3, 4):
            duplicate = list(wire)
            tag, payload = duplicate[position].split("\t", 1)
            duplicate[position] = tag + "\t" + '{"index":0,' + payload[1:]
            bad_wires.append(duplicate)
            unknown = list(wire)
            payload = json.loads(unknown[position].split("\t", 1)[1])
            payload["unknown_key"] = 0
            unknown[position] = tag + "\t" + json.dumps(payload)
            bad_wires.append(unknown)
            non_object = list(wire)
            non_object[position] = tag + "\t[]"
            bad_wires.append(non_object)
        nested = list(wire)
        nested[3] = nested[3].replace('"seed": "000000000001"',
                                     '"seed":"000000000001","seed":"000000000001"', 1)
        require(nested != wire, "declared nested duplicate control")
        bad_wires.append(nested)
        for value in ("NaN", "Infinity", "-Infinity"):
            constant = list(wire)
            constant[2] = constant[2].replace('"index": 0', '"index": ' + value, 1)
            bad_wires.append(constant)
        boolean = list(wire)
        boolean[2] = boolean[2].replace('"index": 0', '"index": false', 1)
        bad_wires.append(boolean)
        boolean_one = list(wire)
        boolean_one[5] = boolean_one[5].replace('"index": 1', '"index": true', 1)
        bad_wires.append(boolean_one)
        boolean_status = list(wire)
        boolean_status[4] = boolean_status[4].replace('"status": 0', '"status": false', 1)
        bad_wires.append(boolean_status)
        for bad in bad_wires:
            try: parse_native(bad,cohort,len(rows))
            except (AssertionError,ValueError,KeyError): rejected += 1
            else: raise AssertionError("accepted malformed closed native envelope")
        mutations = [("effects",0,"world_o",True),("effects",0,"events",[]),("effects",0,"set_after",{}),
                     ("effects",0,"queues_after",[[1,9,9,9,9,9,9]]),("effects",0,"registered_owners_same",False),
                     ("policy",0,"source_pc",999),("policy",0,"native_policy",True),("policy",0,"source_exception",True),
                     ("entropy",0,"world_random_same",False),("entropy",0,"unexpected_entities",[{"synthetic":True}])]
        mutations += [("effects",1,"index",True), ("effects",0,"remote",int(observed[0]["remote"]))]
        for key in ("index", "status", "source_pc", "world_status", "support_count",
                    "predicate_query_count", "retained_producers"):
            mutations.append(("policy", 0, key, bool(native[0][key])))
        for key in ("native_policy", "source_exception", "source_error", "observer_refused"):
            mutations.append(("policy", 0, key, int(native[0][key])))
        for key in ("domain", "class", "method", "descriptor", "operation"):
            mutations.append(("policy", 0, key, 0))
        if cohort == "support": mutations[1] = ("effects",0,"events",[{"synthetic":True}])
        for target,index,key,value in mutations:
            a,n,o = copy.deepcopy((observed,native,own))
            {"effects":a,"policy":n,"entropy":o}[target][index][key] = value
            try: compare(cohort,rows,a,n,o)
            except (AssertionError,ValueError,KeyError,TypeError): rejected += 1
            else: raise AssertionError("accepted synthetic corruption: "+cohort+":"+key)
        for cut in (observed[:-1],observed+[observed[0]]):
            try: compare(cohort,rows,cut,native,own)
            except AssertionError: rejected += 1
            else: raise AssertionError("accepted missing/duplicate synthetic row")
    return {"status":"PASS_PARSER_CONTROLS_ONLY","negative_controls":rejected,"native_executed":False,"synthetic_positive_controls":True}


def parse_native(lines, cohort, count):
    # Exact constructor envelope observed in both genuine native fixture modes.
    require(lines[:2] == KNOWN_NATIVE_PRELUDE, "native constructor envelope differs; retain actual result for review")
    position = 2
    effects, own, policy = [], [], []
    for index in range(count):
        for prefix, target in ((cohort.upper(),effects),("OWN_ENTROPY",own),("REDSTONE_NATIVE",policy)):
            require(position < len(lines) and lines[position].startswith(prefix+"\t"),
                    "missing/extra/out-of-order native record at "+str(position))
            value = loads(lines[position].split("\t",1)[1])
            keys = EFFECT_KEYS if prefix == cohort.upper() else OWN_KEYS if prefix == "OWN_ENTROPY" else POLICY_KEYS
            require(type(value) is dict and set(value) == keys, "exact native record schema")
            require(type(value["index"]) is int and value["index"] == index, "native record index/order")
            if prefix == "REDSTONE_NATIVE":
                policy_types(value)
            target.append(value); position += 1
    require(position+2 == len(lines) and lines[position] == "COMPLETE\t"+cohort+"\t"+str(count),
            "exact native completion and no extra output")
    require(re.fullmatch(r"REDSTONE_NATIVE_COMPLETE\t"+cohort+r"\t[1-9][0-9]*",lines[position+1]) is not None,
            "native checked-retirement completion")
    return effects, policy, own


def guarded(executable, resources):
    # Complete packet admission is repeated before and after the fixed cohort.
    original_rows()
    paths = [SOURCE / "reference.c", SOURCE / "statistics_setup.inc", SOURCE / "support_setup.inc",
             SOURCE / "statistics_teardown.inc", SOURCE / "terminal_cleanup.inc", Path(__file__).resolve(),
             ROOT / "tests/client/statistics/test_player_dispatch.c", ROOT / "tests/entity/player/test_construction.c",
             ROOT / "tests/reference/client/controller/capture.py", ROOT / "tests/reference/client/picking/observe.py",
             ROOT / "tests/tools/client/test_picking.py", ROOT / "tests/reference/client/player_statistics/support.py",
             ROOT / "tests/tools/client/native_product_capture.py",
             ROOT / "src/item/use/use.c", ROOT / "src/world/runtime/placement.c",
             ROOT / "src/item/stack/stack.c", ROOT / "src/item/stack/source_internal.h"]
    return {"executable": C.digest(executable),
            "actual_compile_source_inputs": {p.relative_to(ROOT).as_posix(): C.digest(p) for p in paths},
            "original_packet": tree(INPUTS), "resources": tree(resources)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute",action="store_true")
    parser.add_argument("--self-test",action="store_true")
    parser.add_argument("--executable",type=Path)
    parser.add_argument("--work-dir",type=Path)
    args=parser.parse_args(); expected=original_rows()
    if args.self_test:
        require(not args.execute,"parser controls do not launch native")
        print(json.dumps(parser_controls(expected)));return
    require(args.execute and args.executable is not None and args.work_dir is not None,"native replay requires --execute --executable --work-dir")
    executable=args.executable.resolve(strict=True);args.work_dir.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix="redstone-item-use-",dir=args.work_dir.resolve()))
    resources=output/"resources";S.resources_at(resources)
    before=guarded(executable,resources);report={"status":"PREPARED","inputs_before":before,"runs":[]}
    write(output/"run.json",report)
    primary = None
    try:
        launch_errors = {}
        for cohort in ("use","support"):
            run=output/cohort;run.mkdir();world=run/"world";P.world(world)
            profiles=INPUTS/(cohort+"-profiles.tsv")
            command=[str(executable),str(resources),str(world),str(profiles),cohort]
            item={"cohort":cohort,"world_before":tree(world),"command":command}
            report["runs"].append(item)
            try:
                item["native_products"]=N.capture_native_products(command,run)
                item["capture"]=C.capture(command,run,"native",90,working_directory=run)
            except Exception as error:
                item["launch_failure"]={"type":type(error).__name__,"message":str(error)}
                if isinstance(error,N.NativeProductCaptureError):
                    item["native_products"]=error.receipt
                launch_errors[cohort]=error
            finally:
                try:
                    item["world_after"]=tree(world)
                    products=item.get("native_products",{}).get("products",[])
                    item["products_after"]={p["source"]:C.digest(Path(p["source"])) for p in products if Path(p["source"]).is_file()}
                    item["products_unchanged"]=all(item["products_after"].get(p["source"])==p.get("before",{}).get("sha256") for p in products)
                except Exception as error:
                    item["post_capture_guard_error"]={"type":type(error).__name__,"message":str(error)}
                try: write(output/"run.json",report)
                except Exception as error:
                    item["receipt_write_error"]={"type":type(error).__name__,"message":str(error)}
        # Both fixed process opportunities finish before interpreting any result.
        for item in report["runs"]:
            cohort=item["cohort"];run=output/cohort
            if cohort in launch_errors: raise launch_errors[cohort]
            C.require_success(item["capture"],empty_stderr=True)
            require(item["native_products"]["status"]=="PASS" and item.get("products_unchanged",False),"retained native launch products/continuity")
            lines=(run/"native.stdout").read_bytes().decode("ascii").splitlines()
            actual,policy,own=parse_native(lines,cohort,len(expected[cohort]))
            item["comparison"]=compare(cohort,expected[cohort],actual,policy,own)
            require({k:v for k,v in item["world_before"].items() if k!="session.lock"} ==
                    {k:v for k,v in item["world_after"].items() if k!="session.lock"}, "saved World changes beyond original session lock")
        report["status"]="PASS"
    except Exception as error:
        primary=error;report["status"]="FAIL";report["failure"]={"type":type(error).__name__,"message":str(error)}
    finally:
        try:
            report["inputs_after"]=guarded(executable,resources);report["input_guards_pass"]=before==report["inputs_after"]
            if not report["input_guards_pass"]:
                error=AssertionError("native input/product guard")
                report["status"]="FAIL";report["guard_failure"]=str(error)
                if primary is None: primary=error
        except Exception as error:
            report["status"]="FAIL";report["guard_failure"]={"type":type(error).__name__,"message":str(error)}
            if primary is None: primary=error
        try: write(output/"run.json",report)
        except Exception as error:
            if primary is None: primary=error
    if primary is not None: raise primary
    print("PASS actual54 literal effects,53full outcomes+1native callback prefix;",output/"run.json")


if __name__=="__main__":
    main()
