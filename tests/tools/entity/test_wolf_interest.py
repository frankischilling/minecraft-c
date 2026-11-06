"""GI.o tail controls with optional audited Original leaf pairing.
Original is audited independently and compared after Native; never execution inputs.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import re
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EXPECTED = [False, True, False, True, False, False, False]
SCENES = ["empty_stack", "untamed_bone", "tame_bone", "tame_pork", "tame_apple",
          "tame_book", "untamed_pork", "null_inventory", "null_main_array"]


def need(value, reason):
    if not value:
        raise AssertionError(reason)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(paths):
    return {str(p.resolve()): sha(p) for p in paths}


def validate(rows):
    profiles = [r for r in rows if r.get("record") == "profile"]
    controls = [r for r in rows if r.get("record") == "native_control"]
    need(len(rows) == 12 and len(profiles) == 9 and len(controls) == 3, "complete typed record set")
    for i, r in enumerate(profiles):
        need(r["case"] == i and r["scene"] == SCENES[i], "actual case order")
        need(r["scope"] == "GI.o4..186-only" and r["parent_return_credit"] is False,
             "closed Source range, no unexecuted parent credit")
        need(all(r[k] is True for k in ["all_streams_unchanged", "entity_holds_balanced",
             "registry_math_holds_balanced", "failed_fields_preserved", "cleanup_returned"]),
             "actual C ownership/stream/prefix assertions")
        need(all(re.fullmatch(r"[0-9a-f]{12}", r[k]) for k in
                 ["own_seed", "player_seed", "world_seed", "math_seed"]), "actual independent streams")
        need(re.fullmatch(r"[0-9a-f]{16}", r["U"]), "actual default-constructor context")
        if i < 7:
            need(r["status"] == 0 and r["pc"] == 186 and r["range_returned"] and
                 r["interest_returned"] and r["interested"] is EXPECTED[i], "real returned branch")
        else:
            need(r["status"] != 0 and not r["range_returned"] and not r["interest_returned"] and
                 r["pc"] == 51 and r["source_exception"] and not r["native_policy"] and
                 r["exception"] == "java/lang/NullPointerException" and
                 r["child_class"] == ("gi" if i == 7 else "ix") and
                 r["child_pc"] == (51 if i == 7 else 24), "precise genuine NULL prefix")
        need(r["player"] != "0" * 16, "actual selected Player identity")
        if i == 0:
            need(not r["stack_nonnull"] and r["stack"] == "0" * 16 and
                 r["tame_calls"] == r["id_reads"] == r["array_reads"] == 0,
                 "NULL Stack must skip future Watcher/Item work")
        if i in [3, 4]:
            need(r["array_reads"] == 2 and r["arrays"][0] == r["arrays"][1] != "0" * 16 and
                 r["items"][0] == r["items"][1] != "0" * 16 and
                 r["food_type"] and r["food_returned"], "actual repeated Food array/element aliases")
    need({r["name"] for r in controls} == {
        "unused_services_and_zero_work", "actual_player_receiver_rejected",
        "reached_registry_failure_prefix"} and all(r.get("passed") is True for r in controls),
        "native controls are executed and distinct")
    return len(profiles), len(controls)


def comparator_controls(rows):
    controls = []
    for name in ["interest", "alias", "parent", "null_site", "missing"]:
        wrong = copy.deepcopy(rows)
        profiles = [r for r in wrong if r.get("record") == "profile"]
        if name == "interest":
            profiles[1]["interested"] = False
        elif name == "alias":
            profiles[3]["items"][1] = "0" * 16
        elif name == "parent":
            profiles[0]["parent_return_credit"] = True
        elif name == "null_site":
            profiles[8]["child_pc"] = 51
        else:
            wrong.remove(profiles[8])
        try:
            validate(wrong)
        except AssertionError:
            controls.append(name)
        else:
            raise AssertionError("validator accepted altered " + name)
    return controls


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--original", type=Path)
    parser.add_argument("--build-receipt", type=Path)
    args = parser.parse_args()
    need(bool(args.original) == bool(args.build_receipt), "Original requires explicit packet and actual target build/output companion")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="wolf-interest-", dir=args.work_dir.resolve()))
    report = dict(status="FAIL", scope="AUTHORED_NATIVE_GI_TAIL_ONLY_UNPAIRED",
                  original_comparison=False, superclass_or_full_method_credit=False,
                  compiled_input_binding="external build-system receipt, not established by this wrapper")
    failure = None
    inputs = []
    source_pins, build_pins, support, original_cases = {}, {}, None, None
    try:
        if args.original:
            reference = ROOT / "tests/reference/entity/wolf_interest"
            spec = importlib.util.spec_from_file_location("wolf_original_support", reference / "support.py")
            support = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(support)
            original_cases, source_pins, audit = support.audit_original(args.original.resolve(), reference, ROOT)
            build_pins, binding, actual_build, companion = support.build_inputs(args.build_receipt.resolve(), args.executable.resolve(), ROOT)
            provenance, public_paths = support.public_inputs(reference)
            report.update(scope="FINITE_ORIGINAL_GI51_126_LEAF_PAIR", original_comparison=True,
                          Original_audit=audit, compiled_input_binding=binding,
                          chronology=provenance["chronology"], public_reference_paths=[str(p) for p in public_paths])
            report["manifest_negative_controls"] = support.manifest_negatives(args.original.resolve(), provenance)
            report["build_negative_controls"] = support.build_negatives(actual_build, companion, args.executable.resolve(), ROOT)
            report["expected_pin_negative_controls"] = [
                support.drift_negative(source_pins, "attempt-b/debugger/checkpoints.ndjson", "recorded_Original_artifact_drift"),
                support.drift_negative(build_pins, "tests/entity/test_wolf_interest.c", "recorded_compiled_fixture_drift")]
            inputs += list(source_pins) + list(build_pins)
        spec = importlib.util.spec_from_file_location("wolf_resources", ROOT / "tests/tools/entity/test_mob_construction.py")
        resource_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(resource_module)
        resources, worlds = work / "resources", work / "worlds"
        resources.mkdir()
        worlds.mkdir()
        for i in range(9):
            (worlds / f"case{i}").mkdir()
        inputs += [args.executable.resolve(), Path(__file__).resolve(),
                  ROOT / "tests/entity/test_wolf_interest.c", ROOT / "cmake/mob_tick.cmake",
                  ROOT / "tests/tools/entity/test_mob_construction.py",
                  ROOT / "tests/world/runtime/test_world_random_tick.c"]
        for folder in ["include", "src"]:
            inputs += sorted(p for p in (ROOT / folder).rglob("*") if p.is_file() and
                             p.suffix in [".c", ".h", ".inc"])
        for name, content in resource_module.RESOURCES.items():
            p = resources / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(content)
            inputs.append(p)
        report["input_before"] = inventory(inputs)
        command = [str(args.executable.resolve()), str(resources), str(worlds), str(work)]
        report["command"] = command
        # Recheck exact historical pins immediately before dispatch. A fresh
        # inventory cannot replace the recorded capture/build expectations.
        if support:
            report["pre_execution_recorded_Original"] = support.check_pins(source_pins, "Original pin drift before Native")
            report["pre_execution_recorded_build"] = support.check_pins(build_pins, "compiled pin drift before Native")
        report["native_execution_started"] = True
        start = time.monotonic()
        with (work / "native.stdout").open("wb") as so, (work / "native.stderr").open("wb") as se:
            process = subprocess.run(command, stdout=so, stderr=se, timeout=240)
        report["native_exit"] = process.returncode
        report["seconds"] = time.monotonic() - start
        stderr = (work / "native.stderr").read_bytes()
        need(process.returncode == 0,
             f"native exit {process.returncode}; stderr preview {stderr[:2048]!r}; full {work / 'native.stderr'}")
        need(not stderr, "successful Native run emitted stderr (full artifact retained)")
        stdout = (work / "native.stdout").read_text()
        match = re.fullmatch(r"(?:151 recipes\n16 achievements\n){9}Wolf interest: (\d+) checks;9 real tail profiles;3 native controls\n", stdout)
        need(match and int(match.group(1)) >= 300, "exact declared setup output and C check footer")
        rows = [json.loads(line) for line in (work / "native.ndjson").read_text().splitlines()]
        report["profiles"], report["native_controls"] = validate(rows)
        report["comparator_negative_controls"] = comparator_controls(rows)
        if support:
            report["Original_leaf_comparison"] = support.compare(rows, original_cases)
            report["Original_comparator_negative_controls"] = support.comparator_negatives(rows, original_cases)
        report["c_checks"] = int(match.group(1))
        report["input_after"] = inventory(inputs)
        report["inputs_unchanged"] = report["input_before"] == report["input_after"]
        need(report["inputs_unchanged"], "current inputs changed during execution")
        report["status"] = "PASS"
    except Exception as error:
        failure = str(error)
        report["failure"] = failure
    finally:
        if support:
            try:
                report["final_recorded_Original"] = support.check_pins(source_pins, "final Original expected pin drift")
                report["final_recorded_build"] = support.check_pins(build_pins, "final compiled expected pin drift")
            except Exception as error:
                failure = (failure + "; " if failure else "") + str(error)
                report["failure"], report["status"] = failure, "FAIL"
        if "input_before" in report:
            try:
                report["input_after"] = inventory(inputs)
                report["inputs_unchanged"] = report["input_before"] == report["input_after"]
                if not report["inputs_unchanged"]:
                    failure = (failure + "; " if failure else "") + "final current-input audit drift"
                    report["failure"], report["status"] = failure, "FAIL"
            except Exception as error:
                failure = (failure + "; " if failure else "") + "final input audit: " + str(error)
                report["failure"], report["status"] = failure, "FAIL"
        report["artifacts"] = {p.name: sha(p) for p in sorted(work.iterdir()) if p.is_file()}
        (work / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    print(work / "run.json")
    if failure:
        raise SystemExit(failure)


if __name__ == "__main__":
    main()
