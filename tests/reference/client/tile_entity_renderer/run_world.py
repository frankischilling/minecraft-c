"""Compare all 251 original LL World/frame-null rows in two fresh CPU contexts.

Only each profile's original Java stack-line75 token is projected to the
independently pinned source PC32. Every source field, alias, CV field/default
and row position remains exact. No original JVM, JAR, asset or device is needed.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


original = load("tile_world_original_schema", HERE / "capture_world.py")
ensure, digest, capture, require_success = original.ensure, original.digest, original.capture, original.require_success
PROFILES = ("callbacks", "frame_null_cold")
COUNTS = {"callbacks": 232, "frame_null_cold": 19}
RAW_SHA256 = "8f6fa73c9801bb2ca86a4b805271c8bab68e10df967696e2c3e721db399382d8"
FRAME_DESCRIPTOR = "(Lfd;Lji;Lsj;Lls;F)V"
METHODS = (
    ("ll", "a", "(Lfd;)V", 52, "9b24a75f0955629a711cf5f4874abae68345905a8582ecf95942f4f56975c129"),
    ("ll", "a", FRAME_DESCRIPTOR, 153, "0da982029051e891f91dc2cfedbc28c37fa529efa69294fab4c728d0c01fdea8"),
    ("je", "a", "(Lfd;)V", 1, "149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d"),
    ("hy", "a", "(Lfd;)V", 13, "e0e43dd40c99060e3473cfa7b618cd6f6c68ea50ac1a4a8791bf74a6a8bfa9e4"),
    ("cv", "<init>", "(Lxp;)V", 65, "d7614d86213803db27b91c7a9f24f11b088f35c5b98a9e5a6b094a1d0d1d2e55"),
    ("cv", "<clinit>", "()V", 5, "702774ff7a2be8a5242942017d7ad6f6801414f105de8bc3ca7b6ae46a76db61"),
)


def split_profiles(rows):
    ensure(isinstance(rows, list) and all(isinstance(row, str) and row and row == row.strip()
                                       and " ".join(row.split()) == row for row in rows),
           "complete exact string row inventory")
    result, current = {}, None
    for row in rows:
        cells = row.split()
        ensure(cells[0] in original.TAGS, "unknown World finite row kind")
        if cells[0] == "TILE_WORLD_CASE":
            ensure(len(cells) == 2 and cells[1] not in result, "one fresh profile header")
            current = cells[1]
            result[current] = []
        ensure(current is not None, "World row before profile header")
        result[current].append(row)
    ensure(tuple(result) == PROFILES, "both complete original profiles in declared order")
    return result


def inventory(rows, profile, projected):
    callbacks = 3 if profile == "callbacks" else 0
    ensure(len(rows) == COUNTS[profile] and rows[0] == "TILE_WORLD_CASE " + profile and
           rows[-1] == f"TILE_WORLD_DONE {profile} {callbacks} 1", "complete bounded profile")
    stages = ["initial"] + [f"world{i}" for i in range(callbacks)] + ["before_frame", "after_frame"]
    ll = [row.split() for row in rows if row.startswith("TILE_WORLD_LL ")]
    ensure([row[1] for row in ll] == stages and all(len(row) == 17 for row in ll),
           "every LL field snapshot, with no omissions/duplicates")
    for row in ll:
        ensure(all(value == "null" for value in row[2:6]) and
               all(re.fullmatch(r"[0-9a-f]{8}", value) for value in row[6:8]) and
               all(re.fullmatch(r"[0-9a-f]{16}", value) for value in row[8:14]) and
               row[14].isdigit() and all(value in ("0", "1") for value in row[15:]),
               "exact LL nullable references, raw floats/doubles and Map/identity schema")
    renderers = [row.split() for row in rows if row.startswith("TILE_WORLD_JE ")]
    ensure([(row[1], row[2]) for row in renderers] ==
           [(stage, key) for stage in stages for key in ("yk", "cy", "uk")] and
           all(len(row) == 6 and row[4] in ("0", "1") and row[5] in ("0", "1") for row in renderers),
           "all actual JE identities/backpointers")
    children = [row.split() for row in rows if row.startswith("TILE_WORLD_CHILDREN ")]
    ensure([row[1] for row in children] == stages and
           all(len(row) == 6 and row[2] in ("0", "1") and row[3] in ("0", "1") and
               row[4].isdigit() and row[5] in ("null", "cv") for row in children),
           "all RF/AG Map/HY source child observations")
    fields = [row.split() for row in rows if row.startswith("TILE_WORLD_FIELD ")]
    ensure(len(original.FIELDS) == len(set(original.FIELDS)) == 65 and
           [(row[1], row[2]) for row in fields] ==
           [(str(step), name) for step in range(callbacks) for name in original.FIELDS],
           "all 65 declared CV fields, in order, for all three real CVs")
    integers = set("dghijklO")
    booleans = {"e", "f", "a", "b", "m"} | {"a" + char for char in "bcdefghijklm"}
    for row in fields:
        ensure(len(row) == 6 and row[3] == ("S" if row[2] == "a" else "I"),
               "actual CV declaring instance/static schema")
        descriptor = "Lxp;" if row[2] == "c" else "I" if row[2] in integers else "Z" if row[2] in booleans else "F"
        ensure(row[4] == descriptor and
               ((descriptor == "Lxp;" and row[5] == "null") or
                (descriptor == "I" and re.fullmatch(r"-?[0-9]+", row[5])) or
                (descriptor == "Z" and row[5] in ("0", "1")) or
                (descriptor == "F" and re.fullmatch(r"[0-9a-f]{8}", row[5]))),
               "typed actual CV source field encoding")
    identities = [row.split() for row in rows if row.startswith("TILE_WORLD_CV_ID ")]
    ensure([row[1] for row in identities] == [str(i) for i in range(callbacks)] and
           all(len(row) == 6 and row[2] == "cv" and all(v in ("0", "1") for v in row[3:])
               for row in identities), "all previous/all-retained CV identity comparisons")
    failure = [row.split() for row in rows if row.startswith("TILE_WORLD_FRAME_NULL ")]
    ensure(len(failure) == 1 and failure[0] ==
           ["TILE_WORLD_FRAME_NULL", "java.lang.NullPointerException", "ll", "a", "pc32" if projected else "75"],
           "one actual source null-Living failure, with explicit stack-line/PC distinction")
    frame_ids = [row.split() for row in rows if row.startswith("TILE_WORLD_FRAME_ID ")]
    ensure(len(frame_ids) == 1 and len(frame_ids[0]) == 4 and
           all(value in ("0", "1") for value in frame_ids[0][1:]), "same-World frame skip identity")
    snapshot = ["TILE_WORLD_LL", *(["TILE_WORLD_JE"] * 3), "TILE_WORLD_CHILDREN"]
    tags = ["TILE_WORLD_CASE", *snapshot]
    for _ in range(callbacks):
        tags.extend(["TILE_WORLD_CV_ID", *(["TILE_WORLD_FIELD"] * 65), *snapshot])
    tags.extend([*snapshot, "TILE_WORLD_FRAME_NULL", "TILE_WORLD_FRAME_ID", *snapshot, "TILE_WORLD_DONE"])
    ensure([row.split()[0] for row in rows] == tags, "complete exact execution-row order")


def project_original(rows, profile, site):
    ensure(site["class"] == "ll" and site["method"] == "a" and
           site["descriptor"] == FRAME_DESCRIPTOR and site["pc"] == 32,
           "independent unchanged frame Code site")
    projected, changes = [], []
    for row in rows:
        if row.startswith("TILE_WORLD_FRAME_NULL "):
            cells = row.split()
            ensure(cells == ["TILE_WORLD_FRAME_NULL", "java.lang.NullPointerException", "ll", "a", "75"],
                   "actual captured Java stack line75 remains bound")
            changes.append({"profile": profile, "raw_original_row": row,
                            "original_java_stack_line": 75, "source_pc": 32,
                            "not_compared": "Java stack line versus native source PC"})
            cells[4] = "pc32"
            row = " ".join(cells)
        projected.append(row)
    ensure(len(changes) == 1 and len(projected) == len(rows), "one declared projection per profile")
    return projected, changes


def bindings(executable):
    files = [executable, HERE / "run_world.py", HERE / "world-finite.json", HERE / "world-provenance.json",
             HERE / "world-source-pins.json", HERE / "capture_world.py", HERE / "TileEntityRendererWorldObservation.java",
             HERE / "WORLD_README.md", HERE / "capture_reference.py", HERE.parent / "controller/capture.py",
             HERE.parent / "notice/classfile.py", REPO / "tests/client/render/test_tile_entity_renderer_world.c",
             REPO / "tests/client/render/test_tile_entity_renderer.c",
             REPO / "include/mc/client/render/tile_entity_renderer/tile_entity_renderer.h",
             REPO / "include/mc/client/render/model.h", REPO / "src/client/render/model.c",
             REPO / "src/client/render/model_graph.c", REPO / "src/client/render/model_graph_internal.h",
             REPO / "src/client/render/model_graph_trace.inc",
             REPO / "include/mc/world/runtime/tile_entities/tile_entities.h",
             REPO / "src/world/runtime/tile_entities/tile_entities.c",
             REPO / "src/world/runtime/construction.c", REPO / "src/world/runtime/runtime.c",
             REPO / "src/world/runtime/runtime_internal.h", REPO / "src/block/registry/registry.c"]
    files.extend(sorted(path for path in (REPO / "src/client/render/tile_entity_renderer").rglob("*")
                        if path.is_file() and path.suffix in (".c", ".h")))
    return {str(path.resolve(strict=True)): digest(path) for path in files}


def checked_capture(executable, argument, out, stem, item):
    command = [str(executable), argument]
    receipt = capture(command, out, stem, 60, os.environ.copy(), out.resolve())
    item["capture"] = receipt
    require_success(receipt, empty_stderr=True)
    ensure(type(receipt["exit"]) is int and receipt["exit"] == 0 and receipt["timed_out"] is False and
           receipt["output_overflow"] is False and receipt["pipe_capture_complete"] is True and
           receipt["reader_errors"] == {} and receipt["command"] == command,
           "actual successful complete bounded native process")
    for stream in ("stdout", "stderr"):
        filename = receipt[stream + "_file"]
        ensure(isinstance(filename, str) and Path(filename).name == filename,
               "capture stream must be a local filename")
        path = out / filename
        ensure(type(receipt[stream + "_bytes"]) is int and receipt[stream + "_bytes"] >= 0 and
               path.is_file() and path.stat().st_size == receipt[stream + "_bytes"] and
               digest(path) == receipt[stream + "_sha256"], "full unchanged stream receipt")
    lines = (out / receipt["stdout_file"]).read_text(encoding="utf-8").splitlines()
    ensure(all(line and line == line.strip() and " ".join(line.split()) == line for line in lines),
           "no blank, partial or ignored whitespace output")
    return receipt, lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--controls", action="store_true", help="also capture a separate native lifecycle/limit process")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="tile-entity-renderer-world-", dir=args.work_dir.resolve()))
    before = bindings(executable)
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "Two fresh genuine CPU LL contexts: three unchanged-null-World callbacks with all actual CV fields/"
                       "retained identities, and complete all-null-reference frame prefix/source NPE32. No nonnull World, "
                       "Engine/Font/Living, interpolation, assets, graphics or draws; no JVM Map-order claim."}
    try:
        provenance = json.loads((HERE / "world-provenance.json").read_text(encoding="utf-8"))
        pins = json.loads((HERE / "world-source-pins.json").read_text(encoding="utf-8"))
        ensure(digest(HERE / "world-finite.json") == provenance["fixture_sha256"] == RAW_SHA256 and
               provenance["rows"] == 251, "complete unchanged actual 251-row original fixture")
        ensure(provenance["client_jar_sha256"] == pins["client_jar_sha256"] == original.base.JAR,
               "same untouched original JAR authority; no local JAR required")
        for name in ("TileEntityRendererWorldObservation.java", "capture_world.py", "world-source-pins.json"):
            ensure(digest(HERE / name) == provenance["capture_input_sha256"][name],
                   "exact original captured helper/Code source: " + name)
        ensure(provenance["declared_scalar_inputs"] == {
            "h_i_float_bits": original.FLOAT_INPUTS, "j_k_l_double_bits": original.DOUBLE_INPUTS,
            "partial_float_bits": "3f000000", "reference_operands": "all null"},
            "only the five declared LL scalar inputs and original frame operands")
        authority = []
        for source, name, descriptor, length, code in METHODS:
            selected = [method for method in pins["classes"][source]["methods"]
                        if method["name"] == name and method["descriptor"] == descriptor]
            ensure(len(selected) == 1 and selected[0]["code_bytes"] == length and
                   selected[0]["code_sha256"] == code, "unchanged actual method Code: " + source + descriptor)
            authority.append({"class": source, **selected[0]})
        ensure([(item["name"], item["rows"], item["status"]) for item in provenance["profiles"]] ==
               [(profile, COUNTS[profile], "PASS") for profile in PROFILES], "both complete original contexts")
        raw = json.loads((HERE / "world-finite.json").read_text(encoding="utf-8"))["rows"]
        raw_profiles = split_profiles(raw)
        expected, changes = {}, []
        for profile in PROFILES:
            inventory(raw_profiles[profile], profile, False)
            expected[profile], projection = project_original(raw_profiles[profile], profile,
                                                              provenance["frame_failure_source_site"])
            inventory(expected[profile], profile, True)
            changes.extend(projection)
        ensure(len(raw) == 251 and len(changes) == 2, "all raw records, exactly two declared stack-line projections")
        report.update(original_fixture_sha256=RAW_SHA256, original_profiles=provenance["profiles"],
                      declared_scalar_inputs=provenance["declared_scalar_inputs"],
                      authority_methods=authority, semantic_projection=changes)
        for profile in PROFILES:
            item = {"name": profile}
            report["profiles"].append(item)
            receipt, lines = checked_capture(executable, profile, out, profile, item)
            footer = re.fullmatch(r"Source TileEntityRenderer World: " + profile +
                                  r"; rows=([1-9][0-9]*); checks=([1-9][0-9]*); cleanup=1",
                                  lines[-1] if lines else "")
            ensure(footer is not None, "normal terminal completion after checked owner/handle cleanup")
            actual = [line for line in lines if line.split(" ", 1)[0] in original.TAGS]
            errors = [line for line in lines if line.startswith("TILE_WORLD_NATIVE_ERROR ")]
            ensure(len(errors) == 1 and len(lines) == len(actual) + 2,
                   "complete native stdout: exact logical rows, one typed error, one footer")
            cells = errors[0].split()
            ensure(len(cells) == 10 and cells[1].isdigit() and int(cells[1]) > 0 and
                   cells[2:] == ["NULL_REFERENCE", "0", "ll", "a", FRAME_DESCRIPTOR, "32", "32", "32"],
                   "actual native error role/policy/class/method/descriptor/PC/LL/frame context")
            inventory(actual, profile, True)
            mismatches = [{"index": index,
                           "native": actual[index] if index < len(actual) else None,
                           "original_projection": expected[profile][index] if index < len(expected[profile]) else None}
                          for index in range(max(len(actual), len(expected[profile])))
                          if index >= len(actual) or index >= len(expected[profile]) or actual[index] != expected[profile][index]]
            item.update(rows=len(actual), checks=int(footer[2]), native_error_rows=errors,
                        mismatch_count=len(mismatches), mismatches=mismatches[:20])
            ensure(int(footer[1]) == len(actual) == COUNTS[profile] and not mismatches,
                   "native/original World rows differ; see bounded mismatch details in run.json")
            item["status"] = "PASS"
        ensure(sum(item["rows"] for item in report["profiles"]) == 251, "all 251 original rows compared")
        if args.controls:
            report["controls"] = {}
            receipt, lines = checked_capture(executable, "--controls", out, "controls", report["controls"])
            footer = re.fullmatch(r"Source TileEntityRenderer World controls: checks=([1-9][0-9]*); cleanup=1",
                                  lines[0] if len(lines) == 1 else "")
            ensure(footer is not None, "complete separate native lifecycle/limit control completion")
            report["controls"].update(status="PASS", checks=int(footer[1]))
        report.update(status="PASS", rows=251)
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    finally:
        after = bindings(executable)
        report["input_after_sha256"] = after
        if after != before:
            report.update(status="FAIL", error="source/reference/executable bytes changed during native capture")
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "rows": report.get("rows", 0), "report": str(out / "run.json")}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
