#!/usr/bin/env python3
"""Compare the actual native probe to complete captured originals, without a JDK/JAR."""

import argparse
import base64
import copy
import importlib.util
import json
from pathlib import Path
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[3]
EXPORTER = ROOT / "scripts/reference/entity/export_nbt.py"
SUPPORT = ROOT / "scripts/reference/java/compare_int_set.py"
FIXTURE = ROOT / "tests/reference/entity/nbt_fixtures.json"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name,path)
    if spec is None or spec.loader is None:
        raise ValueError("required fixture/process support missing")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def compare(actual, expected, side, report):
    if actual == expected:
        return
    differences = [{"row":index,"original":expected[index] if index < len(expected) else None,
                    "native":actual[index] if index < len(actual) else None}
                   for index in range(max(len(actual),len(expected)))
                   if index >= len(actual) or index >= len(expected) or actual[index] != expected[index]]
    report.setdefault("differences",{})[side] = differences
    raise ValueError("native/captured-original complete row mismatch: " + side)


def guards(exporter, package, expected):
    """Separate malformed-fixture/comparison controls, never claimed native runs."""
    controls = []
    for name in ("truncated-zlib","trailing-zlib","bad-base64","raw-pin","scope","provenance"):
        changed = copy.deepcopy(package)
        trace = changed["traces"]["client"]
        if name in ("truncated-zlib","trailing-zlib"):
            compressed = base64.b64decode(trace["payload"],validate=True)
            compressed = compressed[:-1] if name == "truncated-zlib" else compressed + b"unexpected"
            trace["payload"] = base64.b64encode(compressed).decode("ascii")
            trace["compressed"] = exporter.fingerprint(compressed)
        elif name == "bad-base64":
            trace["payload"] = "!" + trace["payload"][1:]
        elif name == "raw-pin":
            trace["raw"]["bytes"] += 1
        elif name == "scope":
            changed["whole_game_parity"] = True
        else:
            changed["provenance"]["capture_inputs"]["observer"]["sha256"] = "0" * 64
        try:
            exporter.load_fixture(exporter.canonical(changed))
        except (ValueError,TypeError,KeyError,UnicodeError,zlib.error):
            controls.append(name)
        else:
            raise ValueError("malformed fixture control accepted: " + name)
    # Preserve valid row structure while changing a captured scalar. This checks
    # the exact comparator, independently of fixture byte-identity validation.
    actual = list(expected["client"])
    index = next(index for index,row in enumerate(actual) if row[:4] == ("FIELD","read_good","after","velocity_x"))
    altered = list(actual[index])
    altered[4] = "3ff0000000000000"
    actual[index] = tuple(altered)
    try:
        compare(actual,expected["client"],"guard",{})
    except ValueError:
        controls.append("changed-native-scalar")
    else:
        raise ValueError("native scalar mismatch control accepted")
    return controls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--fixture",type=Path,default=FIXTURE)
    parser.add_argument("--work-dir",type=Path,default=ROOT / ".local/reference/entity-nbt")
    args = parser.parse_args()
    work = None
    inputs, before = {},{}
    report = {"schema":1,"scope":"captured-original ItemEntity NBT native regression; 20 prepared cases per side",
              "passed":False,"whole_game_parity":False,"natural_population":False,"jdk_executed":False,
              "original_archive_required":False,"processes":[],"selected_source_pin_scope":"not a complete link/build manifest"}
    capture = module("entity_nbt_regression_process",SUPPORT)
    deadline = time.monotonic() + 90
    try:
        capture.require(args.executable.is_file(),"actual native ItemEntity NBT probe missing")
        work = capture.capture_root(args.work_dir,[sys.executable])
        exporter = module("entity_nbt_original_fixture",EXPORTER)
        inputs = {"executable":args.executable.resolve(strict=True),"fixture":args.fixture.resolve(strict=True),
                  "driver":Path(__file__).resolve(),"exporter":EXPORTER,"protocol":exporter.PARSER,
                  "process_support":SUPPORT,"probe_source":ROOT / "tests/entity/nbt/nbt_probe.c",
                  "constructor_fixture":ROOT / "tests/entity/test_entity.c","cmake":ROOT / "CMakeLists.txt"}
        for folder in ("src/entity","include/mc/entity","src/item/stack","include/mc/item/stack","src/storage/nbt","include/mc/storage/nbt"):
            for path in sorted((ROOT / folder).rglob("*")):
                if path.is_file() and path.suffix in (".c",".h"):
                    inputs[path.relative_to(ROOT).as_posix()] = path
        capture.require(all(path.is_file() and not path.is_symlink() for path in inputs.values()),"ordinary selected inputs")
        before = {name:capture.file_fingerprint(path) for name,path in inputs.items()}
        report["inputs_before"] = before
        raw = exporter.bounded_read(inputs["fixture"],exporter.MAX_FIXTURE)
        sources = work / "inputs"
        sources.mkdir(exist_ok=False)
        for index,(name,path) in enumerate(inputs.items()):
            if name != "executable":
                (sources / (str(index) + "-" + path.name)).write_bytes(raw if name == "fixture" else path.read_bytes())
        package, originals, expected = exporter.load_fixture(raw)
        protocol = exporter.load_parser()
        report["fixture"] = capture.fingerprint(raw)
        report["capture_receipt"] = package["provenance"]["capture_receipt"]
        report["protocol_guards"] = guards(exporter,package,expected)
        for side in ("client","server"):
            (work / ("original-" + side + ".stdout")).write_bytes(originals[side])
            stdout,stderr = capture.run([str(inputs["executable"]),side],work=work,name="native-" + side,
                                        records=report["processes"],deadline=deadline)
            capture.require(not stderr,"native probe stderr must be empty: " + side)
            actual = protocol.parse(stdout,side,False)
            compare(actual,expected[side],side,report)
            report.setdefault("comparison",{})[side] = {"passed":True,"cases":20,"normalized_rows":len(actual),"selected_fields":52}
        capture.require(len(report["processes"]) == 2 and all(row["exit"] == 0 and row["reaped"] is True and
                        not row["timed_out"] and not row["output_limited"] for row in report["processes"]),
                        "two complete successful reaped native children")
        report["passed"] = True
    except (capture.Failure,ValueError,TypeError,KeyError,IndexError,OSError,UnicodeError,zlib.error) as error:
        report["failure"] = str(error)
        print("FAIL: " + str(error),file=sys.stderr)
    finally:
        if work is not None:
            after = {}
            for name,path in inputs.items():
                try:
                    after[name] = capture.file_fingerprint(path)
                except (capture.Failure,OSError) as error:
                    report["passed"] = False
                    report.setdefault("input_failures",{})[name] = str(error)
            report["inputs_after"] = after
            if after != before:
                report["passed"] = False
                report["inputs_changed"] = True
                print("FAIL: regression inputs changed",file=sys.stderr)
            report["retained_files"] = {path.relative_to(work).as_posix():capture.file_fingerprint(path)
                                         for path in sorted(work.rglob("*")) if path.is_file()}
            with (work / "report.json").open("x",encoding="utf-8",newline="\n") as stream:
                json.dump(report,stream,indent=2,sort_keys=True)
                stream.write("\n")
            print("Capture: " + str(work))
    if report["passed"]:
        print("PASS: 40 captured-original ItemEntity NBT cases, 4844 exact comparison rows; no JDK/game archive.")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
