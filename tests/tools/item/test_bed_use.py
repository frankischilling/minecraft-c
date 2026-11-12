"""Replay all72 direct LP observations and separate authored IZ wrapper controls."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / "tests/reference/item/bed_use"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


P = module("bed_use_original_packet", PACKET / "packet.py")
C = module("bed_use_capture", ROOT / "tests/reference/client/controller/capture.py")
S = module("bed_use_resources", ROOT / "tests/reference/client/player_statistics/support.py")


def need(v, message):
    if not v:
        raise ValueError(message)


def equal(a, b):
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(equal(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(equal(x, y) for x, y in zip(a, b))
    return a == b


def tree(path):
    return {p.relative_to(path).as_posix(): C.digest(p) for p in sorted(path.rglob("*")) if p.is_file()}


def guarded(executable, profiles, resources):
    source = [Path(__file__).resolve(), ROOT / "tests/item/use/bed_reference.c",
              ROOT / "tests/client/statistics/test_player_dispatch.c", ROOT / "src/item/use/use.c",
              ROOT / "src/item/use/bed.inc", ROOT / "src/item/use/bed_internal.h",
              ROOT / "include/mc/item/use/use.h", ROOT / "src/world/runtime/lighting.c",
              ROOT / "src/world/runtime/lighting_internal.h"]
    return {"executable": C.digest(executable), "profiles": C.digest(profiles),
            "source": {p.relative_to(ROOT).as_posix(): C.digest(p) for p in source},
            "packet": tree(PACKET), "resources": tree(resources)}


def parse(payload):
    lines = payload.decode("utf-8", errors="strict").splitlines()
    need(len(lines) == 147 and lines[:2] == ["16 achievements", "151 recipes"] and
         re.fullmatch(r"PASS: 72 Bed direct profiles and wrapper controls, [1-9][0-9]* actual-owner checks", lines[-1]) is not None,
         "complete native bootstrap/144row/footer inventory")
    rows, policies = [], []
    for index in range(72):
        for line, prefix, target in ((lines[2+index*2], "BED_PLACEMENT\t", rows),
                                     (lines[3+index*2], "BED_PLACEMENT_NATIVE\t", policies)):
            need(line.startswith(prefix), "ordered native effect/policy row tags")
            target.append(json.loads(line[len(prefix):], object_pairs_hook=P.unique))
    return rows, policies


def compare(expected, actual, policies, events):
    need(len(expected) == len(actual) == len(policies) == 72, "all72 original/native profiles")
    groups = P.check_events(events, expected)
    policy_keys = {"index", "status", "native_policy", "source_exception", "source_error", "observer_refused",
                   "source_pc", "foot_returned", "foot_value", "head_returned", "head_value"}
    for index, (wanted, got, policy, group) in enumerate(zip(expected, actual, policies, groups)):
        need(set(got) == set(wanted) and set(policy) == policy_keys and
             type(policy["index"]) is int and policy["index"] == index, "exact native schema/identity")
        callback = wanted["exception"] == "BedPlacementObservation$DeclaredCallbackFailure"
        projected = dict(wanted)
        if callback:
            projected["exception"] = "NATIVE_CALLBACK_REFUSAL"
        need(equal(got, projected), "LP effects differ for " + wanted["name"] + ": " +
             ",".join(k for k in wanted if got.get(k) != projected[k]))
        need(type(policy["status"]) is int and type(policy["source_pc"]) is int and
             all(type(policy[k]) is bool for k in policy_keys - {"index", "status", "source_pc"}),
             "exact native policy scalar types")
        need(policy["source_error"] is False and policy["observer_refused"] is callback,
             "callback native refusal remains distinct from original Java exception")
        if callback:
            need(policy["status"] == 7 and policy["native_policy"] is True and policy["source_exception"] is False and
                 policy["source_pc"] == (167 if wanted["message"] == "foot" else 194), "typed setter native failure prefix")
        elif wanted["exception"] != "NONE":
            pc = 92 if wanted["name"] == "null_world" else 200 if wanted["name"] == "null_stack" else 23
            need(policy["status"] == 9 and policy["native_policy"] is False and policy["source_exception"] is True and
                 policy["source_pc"] == pc, "typed actual source null prefix")
        else:
            need(policy["status"] == 0 and policy["source_exception"] is False and policy["native_policy"] is False and
                 policy["source_pc"] == (209 if wanted["result"] == "true" else 7 if wanted["name"].startswith("face_guard_") else 211),
                 "normal direct LP completion")
        returned = {int(e["caller"].rsplit("@",1)[1]): e["value"] for e in group if e["event"] == "return" and
                    e.get("caller") in (P.LP+"167", P.LP+"194")}
        for label, pc in (("foot",167),("head",194)):
            need(policy[label+"_returned"] is (pc in returned) and policy[label+"_value"] is returned.get(pc,False),
                 "exact observed setter return/boolean: " + wanted["name"])
    return {"effect_rows":72,"full_outcome_rows":70,"callback_native_failure_prefixes":2,
            "callback_exception_class_equivalence":"UNQUALIFIED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    expected = P.read_reference()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="bed-use-",dir=args.work_dir.resolve()))
    resources, game = out/"resources", out/"game"
    S.resources_at(resources)
    game.mkdir()
    profiles = out/"profiles.tsv"
    profiles.write_text("".join("\t".join(map(str,[r["name"],*r["values"],r["direction"]]))+"\n" for r in expected),encoding="ascii",newline="\n")
    before = guarded(executable,profiles,resources)
    report = {"status":"FAIL","input_before":before,"scope":"Direct shared LP body; actual IZ wrapper and context/lifetime controls are separate authored native assertions."}
    try:
        receipt = C.capture([str(executable),str(resources),str(game),str(profiles)],out,"native",180)
        report["native"] = receipt
        C.require_success(receipt,empty_stderr=True)
        rows, policies = parse((out/receipt["stdout_file"]).read_bytes())
        events = P.read_journal()
        report["comparison"] = compare(expected,rows,policies,events)
        # Prove that nonlocal byte changes, late count, ordering and typed prefix
        # corruption cannot be hidden by the callback exception projection.
        corruptions=[]
        for label,index,key,value in (("count",67,"count_after",98),("direction",1,"direction",0),
                                      ("partial-half",68,"foot_after",[0,0]),("queue",0,"light_queue",999)):
            changed=copy.deepcopy(rows);changed[index][key]=value
            try:compare(expected,changed,policies,events)
            except ValueError:corruptions.append(label)
            else:raise ValueError("accepted effect corruption: "+label)
        changed=copy.deepcopy(policies);changed[51]["foot_value"]=True
        try:compare(expected,rows,changed,events)
        except ValueError:corruptions.append("discarded-boolean")
        else:raise ValueError("accepted setter boolean corruption")
        report["negative_controls"] = corruptions
        report["status"] = "PASS"
    except BaseException as e:
        report["error"] = {"type":type(e).__name__,"message":str(e)}
        raise
    finally:
        report["input_after"] = guarded(executable,profiles,resources)
        report["input_unchanged"] = report["input_after"] == before
        report["game_outputs"] = tree(game)
        if not report["input_unchanged"]:
            report["status"] = "FAIL"
        (out/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    need(report["status"] == "PASS", "native LP replay completion/input guard")
    print("PASS:72 original LP rows and native wrapper controls; "+str(out/"report.json"))


if __name__ == "__main__":
    main()
