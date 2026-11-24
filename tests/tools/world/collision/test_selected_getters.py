"""Bounded native component checks for the actual client selected-box getters."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("selected_getter_capture",
    ROOT / "scripts/reference/java/compare_int_set.py")
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)
CASES = HERE / "selected_getter_cases.json"
MAX_INPUT = 2 * 1024 * 1024

def bounded(path, maximum=MAX_INPUT):
    CAPTURE.require(path.is_file() and not path.is_symlink(), "ordinary input file: " + str(path))
    CAPTURE.require(path.stat().st_size <= maximum, "bounded input before read")
    raw = path.read_bytes()
    CAPTURE.require(len(raw) <= maximum, "bounded input after read")
    return raw

def digest(path):
    return CAPTURE.file_fingerprint(path)

def inputs(executable):
    paths = {ROOT / "CMakeLists.txt", Path(__file__).resolve(), Path(SPEC.origin), CASES,
        ROOT / "tests/world/runtime/collision/test_selected_getters.c",
        ROOT / "tests/world/runtime/collision/selected_getter_cases.inc",
        ROOT / "tests/world/runtime/collision/test_boxes.c",
        ROOT / "tests/world/runtime/test_entity_publication.c",
        ROOT / "tests/support/java_random.h"}
    for directory in ("src", "include"):
        paths.update(p for p in (ROOT / directory).rglob("*")
            if p.is_file() and p.suffix in (".c", ".h", ".inc"))
    rows = {p.relative_to(ROOT).as_posix():digest(p) for p in sorted(paths)}
    rows["native_executable"] = digest(executable)
    return rows

def audit(stdout, corpus):
    text = stdout.decode("ascii").replace("\r\n", "\n")
    lines = text.splitlines()
    cases = corpus["cases"]
    CAPTURE.require(len(cases) == 40 and len({c["name"] for c in cases}) == 40,
                    "fixed40-case authored inventory")
    CAPTURE.require(len(lines) == len(cases) + 4 and
                    lines[:2] == ["16 achievements", "151 recipes"],
                    "complete one-client bootstrap and result inventory")
    observations = []
    for line, case in zip(lines[2:42], cases):
        CAPTURE.require(line.startswith("SELECTED ") and len(line) <= 16384,
                        "bounded ordered selected row")
        row = CAPTURE.strict_json(line[len("SELECTED "):])
        expected = case["expected"]
        keys = {"name","status","success","source_null","native_policy","returned","out_null",
            "output_preserved","same_marker","cursor","holds","bounds","fields_before",
            "fields_after","prototype_id","prototype_same_as_registered","prototype_fields",
            "collision_child","frames","selected_child"}
        CAPTURE.require(isinstance(row,dict) and set(row) == keys and row["name"] == case["name"],
                        "exact selected row schema/name/order")
        for key in ("success","source_null","native_policy","returned","out_null",
                    "output_preserved","same_marker","prototype_same_as_registered"):
            CAPTURE.require(type(row[key]) is bool, "actual boolean: " + key)
        CAPTURE.require(type(row["prototype_id"]) is int and
            all(type(v) is list and len(v)==2 and all(type(x) is int and x>=0 for x in v)
                for v in (row["cursor"],row["holds"])), "typed actual identity/cursor/hold values")
        CAPTURE.require(type(row["frames"]) is list and len(row["frames"])<=64,
                        "bounded actual selected frame list")
        for frame in row["frames"]:
            CAPTURE.require(type(frame) is dict and set(frame)==
                {"class","pc","receiver_id","reference_owner","bounds_store","metadata_returned",
                 "metadata","state_shape_returned","returned"}, "exact frame schema")
            CAPTURE.require(all(type(frame[k]) is int for k in
                ("pc","receiver_id","bounds_store","metadata")) and
                all(type(frame[k]) is bool for k in
                ("metadata_returned","state_shape_returned","returned")) and
                all(type(frame[k]) is str for k in ("class","reference_owner")),
                "typed actual frame operands/flags")
        CAPTURE.require(type(row["status"]) is int and
                        (row["status"] == 0) == case["success"], "actual status/success")
        for key in ("bounds","fields_before","fields_after","prototype_fields"):
            value = row[key]
            CAPTURE.require((key == "bounds" and value is None and not case["success"]) or
                (isinstance(value,list) and len(value)==6 and all(type(x) is str and
                re.fullmatch(r"[0-9a-f]{16}",x) for x in value)), "six raw binary64 coordinates")
            CAPTURE.require(value == expected[key], case["name"] + ": exact " + key)
        for key in ("frames","source_null","native_policy","selected_child","collision_child",
                    "prototype_id"):
            CAPTURE.require(row[key] == expected[key], case["name"] + ": exact " + key)
        success = case["success"]
        CAPTURE.require(row["success"] == success and row["returned"] ==
                        (success and not case["collision"]), "actual getter completion")
        CAPTURE.require(row["cursor"] == [0,1 if success else 0] and
                        row["holds"] == [1,2 if success else 1],
                        "actual pool cursor and overlapping marker/output ownership")
        CAPTURE.require(row["out_null"] == (not success and not case["output_guard"]) and
                        row["output_preserved"] == (not success) and
                        row["same_marker"] == success, "actual output preservation or pool identity")
        CAPTURE.require(row["prototype_same_as_registered"] == (expected["prototype_id"] != -1),
                        "actual Stairs prototype identity")
        observations.append(row)
    CAPTURE.require(lines[42].startswith("SELECTED_CLEANUP "), "actual cleanup receipt")
    cleanup = CAPTURE.strict_json(lines[42][len("SELECTED_CLEANUP "):])
    CAPTURE.require(set(cleanup) == {"marker_release_ok","fixture_cleanup_ok","cases","checks"} and
        cleanup["marker_release_ok"] is True and cleanup["fixture_cleanup_ok"] is True and
        cleanup["cases"] == 40 and type(cleanup["checks"]) is int and cleanup["checks"] > 200,
        "actual balanced marker release and complete native fixture destruction")
    CAPTURE.require(lines[43] == f"PASS: {cleanup['checks']} selected getter checks (selected-getters)",
                    "complete native PASS record")
    return {"cases":observations,"cleanup":cleanup,"source_successes":sum(c['success'] for c in cases),
        "normal_selected":sum(c['success'] and not c['collision'] for c in cases),
        "collision_controls":sum(c['collision'] for c in cases),
        "scope":"Native component vectors and ownership controls; no original parity claim"}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--work-dir",type=Path,required=True)
    args=parser.parse_args()
    work=None
    report={"passed":False,"processes":[],"original_parity":False}
    try:
        executable=args.executable.resolve(strict=True)
        corpus=CAPTURE.strict_json(bounded(CASES))
        work=CAPTURE.capture_root(args.work_dir,[sys.executable])
        world=work/"world-owned"
        CAPTURE.require(not world.exists(),"fresh component World root")
        before=inputs(executable)
        report["input_before"]=before
        stdout,stderr=CAPTURE.run([str(executable),"selected-getters",str(world).encode('utf-8').hex()],
            work=work,name="native",records=report["processes"],deadline=time.monotonic()+70)
        CAPTURE.require(not stderr,"no native component stderr")
        report["audit"]=audit(stdout,corpus)
        after=inputs(executable)
        report["input_after"]=after
        CAPTURE.require(before==after,"source/helper/executable inputs unchanged")
        report["inputs_unchanged"]=True
        products={}
        count=0
        for p in sorted(world.rglob('*')):
            count+=1
            CAPTURE.require(count<=64 and not p.is_symlink(),"bounded ordinary owned artifact inventory")
            if p.is_file():
                CAPTURE.require(p.stat().st_size<=2*1024*1024,"bounded fixture artifact before hash")
                products[p.relative_to(world).as_posix()]=digest(p)
        CAPTURE.require(bool(products),"retained real World/Store products")
        report["world_products"]=products
        report["passed"]=True
        print(f"PASS: 40 independently checked selected getter component cases; {report['audit']['cleanup']['checks']} native checks")
        return 0
    except (CAPTURE.Failure,OSError,ValueError,UnicodeError) as error:
        report["failure"]=str(error)
        print("FAIL: "+str(error),file=sys.stderr)
        return 1
    finally:
        if work is not None:
            with (work/"REPORT.json").open('x',encoding='utf-8') as stream:
                json.dump(report,stream,indent=2)
                stream.write('\n')
            print("Capture: "+str(work))

if __name__ == "__main__":
    raise SystemExit(main())
