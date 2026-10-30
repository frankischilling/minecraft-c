"""Exact TH awake/null method comparison with a complete explicit credit ledger.

The original full Statistics/TH/Pig construction remains retained evidence.
Native runs the genuine full Statistics/TH owners and a genuine RAW LocalPlayer;
it earns no Pig/LocalPlayer constructor, Java reflection-allocation or physical
HashMap implementation equivalence. Expected frame/side/constructor outputs are
never supplied to the executable. Every one of the432 original rows is retained.
"""
import argparse
from collections import Counter
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
spec = importlib.util.spec_from_file_location("manager_frame_original_support", HERE / "capture_reference.py")
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
ensure, digest, capture, require_success = original.ensure, original.digest, original.capture, original.require_success
RAW_SHA256 = "09e389fae8521db85b378aa40e0eb1932090c96afa75f6e6f75bdcf92c370c07"
PROFILES = ("cold_th", "prepared_awake", "prepared_null")
FRAME_CODE_SHA256 = "cc312e67247d9c59c80c87ac102f68ed5ad266b397d5e355b797164e74c4c28e"
CONTROLS = [
    "RM_CONTROL numeric_late UNSUPPORTED_NUMERIC 198 179 refs=1 completed_ij=1 preserved_lmn=1 native_policy=1",
    "RM_CONTROL unsupported_fp UNSUPPORTED_FP 150 29 refs=1 preserved_scalars=1 control_untouched=1 caller_restored=1",
]
ABSENT_ACTOR = "RM_NATIVE_ACTOR dc RAW constructor=uncredited N=gs random=absent watcher=absent math=absent"


def partition(rows):
    ensure(isinstance(rows, list) and len(rows) == 432 and all(
        isinstance(row, str) and row and row == row.strip() and " ".join(row.split()) == row for row in rows),
        "complete432 canonical original records")
    groups = {}
    active = None
    for index, row in enumerate(rows):
        if row.startswith("RM_CASE "):
            active = row.split()[1]
            ensure(active in PROFILES and active not in groups, "one fresh original profile")
            groups[active] = []
        ensure(active is not None, "every original row belongs to a fresh profile")
        groups[active].append((index, row))
    ensure(list(groups) == list(PROFILES) and [len(groups[p]) for p in PROFILES] == [3, 258, 171],
           "complete original cold3/awake258/null171 inventory")
    return groups


def original_inventory(groups, cases, pins):
    ensure(len(cases) == 24 and [case["id"] for case in cases] == list(range(24)), "all24 authored operands")
    ensure([row for _, row in groups["cold_th"]] == [
        "RM_CASE cold_th",
        "RM_COLD_THROW 0 java.lang.ExceptionInInitializerError jl <clinit> 65",
        "RM_COLD_THROW 1 java.lang.NullPointerException iz <init> 38"], "whole actual cold initializer cause chain")
    roster = sorted((r["key"], r["renderer"]) for r in pins["th_constructor_roster"])
    ensure(len(roster) == 28 and len(set(key for key, _ in roster)) == 28, "complete pinned constructor roster")
    for profile in PROFILES[1:]:
        rows = [row for _, row in groups[profile]]
        counter = Counter(row.split()[0] for row in rows)
        count = 24 if profile == "prepared_awake" else 1
        expected = {"RM_CASE": 1, "RM_PREP": 1, "RM_TH_NORMAL": 1, "RM_ROSTER": 28, "RM_REF": 128,
                    "RM_CTOR": 1, "RM_ORIGINS": 1, "RM_PIG": 1,
                    "RM_INPUT": count, "RM_N": count, "RM_FRAME": count, "RM_SIDE": count}
        if profile == "prepared_null":
            expected.update({tag: 1 for tag in ("RM_NULL_BEFORE", "RM_NULL_THROW", "RM_NULL_AFTER", "RM_NULL_STABLE", "RM_NULL_SIDE")})
        ensure(counter == Counter(expected), "complete upfront original row-kind inventory: " + profile)
        actual_roster = [row.split() for row in rows if row.startswith("RM_ROSTER ")]
        ensure([(r[1], r[2]) for r in actual_roster] == roster and all(
            len(r) == 8 and r[3:6] == ["1", "fh", "cv"] and all(v.isdigit() for v in r[6:]) for r in actual_roster),
            "all28 original renderer/model/CV/backpointer rows")
        refs = [row.split() for row in rows if row.startswith("RM_REF ")]
        ensure(all(len(r) == 5 and r[4].isdigit() for r in refs) and
               set(r[1] for r in refs) == set(key for key, _ in roster), "all128 declared reference aliases")
        for tag in ("RM_INPUT", "RM_N", "RM_FRAME", "RM_SIDE"):
            ensure([int(row.split()[1]) for row in rows if row.startswith(tag + " ")] == list(range(count)),
                   "all mandatory case IDs: " + profile + "/" + tag)
        for row in rows:
            cells = row.split()
            tag = cells[0]
            widths = {"RM_CASE": 2, "RM_PREP": 15, "RM_TH_NORMAL": 2, "RM_ROSTER": 8, "RM_REF": 5,
                      "RM_CTOR": 9, "RM_ORIGINS": 4, "RM_PIG": 5, "RM_INPUT": 14, "RM_N": 4,
                      "RM_FRAME": 12, "RM_SIDE": 11, "RM_NULL_BEFORE": 6, "RM_NULL_THROW": 5,
                      "RM_NULL_AFTER": 11, "RM_NULL_STABLE": 3, "RM_NULL_SIDE": 10}
            ensure(len(cells) == widths[tag], "whole original record width: " + tag)
            if tag == "RM_INPUT":
                ensure(cells[2] == cases[int(cells[1])]["name"] and
                       all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[3:8]) and
                       all(re.fullmatch(r"[0-9a-f]{16}", value) for value in cells[8:]), "actual source-stored input words")
            elif tag == "RM_N":
                ensure(cells[2:] == ["ls", "0"], "actual original Pig inherited Living.N")
            elif tag == "RM_FRAME":
                ensure(cells[2:7] == ["1"] * 5 and all(re.fullmatch(r"[0-9a-f]{8}", v) for v in cells[7:9]) and
                       all(re.fullmatch(r"[0-9a-f]{16}", v) for v in cells[9:]), "complete original return/reference/numeric words")
            elif tag in ("RM_SIDE", "RM_NULL_SIDE"):
                ensure(cells[2 if tag == "RM_SIDE" else 1:] == ["1"] * 9, "all nine original preservation observations")
        ensure("RM_TH_NORMAL 1" in rows, "actual prepared publication")
        if profile == "prepared_null":
            ensure("RM_NULL_THROW java.lang.NullPointerException th a 101" in rows, "actual original null-Living failure frame")
    awake_inputs = [row for _, row in groups["prepared_awake"] if row.startswith("RM_INPUT ")]
    null_inputs = [row for _, row in groups["prepared_null"] if row.startswith("RM_INPUT ")]
    ensure(null_inputs == awake_inputs[:1], "actual null profile begins with the same actual awake0")
    return [row.split(" ", 1)[1] for row in awake_inputs]


def bind_files(executable, input_path):
    # Bind all native source/header inputs, not just the newly authored wrapper.
    paths = [executable, input_path, HERE / "run_native.py", HERE / "finite.json", HERE / "provenance.json",
             HERE / "source-pins.json", HERE / "frame-inputs.json", HERE / "README.md", HERE / "capture_reference.py",
             HERE / "RenderManagerFrameObservation.java", HERE.parent / "tile_entity_renderer/capture_reference.py",
             HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py",
             REPO / "tests/client/render/test_manager_frame.c", REPO / "tests/client/render/test_manager.c",
             REPO / "CMakeLists.txt"]
    for root in (REPO / "src", REPO / "include"):
        paths.extend(path for path in root.rglob("*") if path.is_file() and path.suffix in (".c", ".h", ".inc"))
    return {str(path.resolve(strict=True)): digest(path) for path in sorted(set(paths))}


def resource_files(root):
    ensure(root.is_dir() and not root.is_symlink(), "real passed authored resource root")
    files = {}
    for path in sorted(root.rglob("*")):
        ensure(not path.is_symlink(), "resource fixture has no linked external input")
        if path.is_file():
            files[path.relative_to(root).as_posix()] = {"bytes": path.stat().st_size, "sha256": digest(path)}
    ensure(files, "nonempty actual Statistics resource fixture")
    return files


def check_original_pins(provenance, pins):
    ensure(provenance["client_jar_sha256"] == pins["client_jar_sha256"] == original.base.JAR,
           "unchanged original JAR authority, never passed to native")
    ensure(len(pins["classes"]) == 87, "complete87 selected original class Code inventories")
    methods = [m for m in pins["classes"]["th"]["methods"] if m["name"] == "a" and
               m["descriptor"] == "(Lfd;Lji;Lsj;Lls;Lkv;F)V"]
    ensure(len(methods) == 1 and methods[0]["code_bytes"] == 258 and
           methods[0]["code_sha256"] == FRAME_CODE_SHA256, "unchanged complete TH258 frame Code")
    required = {"tests/reference/client/render_manager_frame/RenderManagerFrameObservation.java",
                "tests/reference/client/render_manager_frame/capture_reference.py",
                "tests/reference/client/render_manager_frame/frame-inputs.json",
                "tests/reference/client/render_manager_frame/source-pins.json",
                "tests/reference/client/tile_entity_renderer/capture_reference.py",
                "tests/reference/client/controller/capture.py", "tests/reference/client/notice/classfile.py"}
    hashes = provenance["project_input_sha256"]
    ensure(required <= set(hashes), "all actually executed source/input/support pins present")
    for relative in sorted(required):
        ensure(digest(REPO / relative) == hashes[relative], "unchanged actual original executed input: " + relative)
    ensure(hashes["b1.7.3.jar"] == original.base.JAR, "historical archive pin")
    # README was updated with the actual capture result after execution. Its
    # current bytes are bound before/after; its historic instruction hash is not
    # an executed-program pin and must not force stale prose into the runner.


def read_capture(receipt, out, profile, expected_diagnostics):
    require_success(receipt, empty_stderr=True)
    for stream in ("stdout", "stderr"):
        name = receipt[stream + "_file"]
        ensure(Path(name).name == name, "local bounded capture stream")
        path = out / name
        ensure(path.stat().st_size == receipt[stream + "_bytes"] and digest(path) == receipt[stream + "_sha256"],
               "complete stable process stream receipt")
    lines = (out / receipt["stdout_file"]).read_text(encoding="utf-8").splitlines()
    ensure(lines and all(line and line == line.strip() and " ".join(line.split()) == line for line in lines),
           "all stdout is canonical parsed records")
    footer = re.fullmatch(r"Source RenderManager frame: " + re.escape(profile) + r"; ([1-9][0-9]*) checks", lines[-1])
    ensure(footer is not None, "normal profile completion after checked owner cleanup")
    rows, diagnostics, row_positions = [], [], []
    for raw_line_index, line in enumerate(lines[:-1]):
        if line.startswith("RM_"):
            row_positions.append(raw_line_index)
            rows.append(line)
        else:
            diagnostics.append({"raw_line_index": raw_line_index, "text": line,
                                "semantic_rows_before": len(rows)})
    # Source println output is real constructor behavior, not observer rows.
    # Keep its complete raw placement and require the actual original content
    # and order. An unknown line is a failure, never suppressed console noise.
    receipt["source_console_diagnostics"] = {
        "expected_original": expected_diagnostics, "observed": diagnostics,
        "index_basis": "zero-based lines in the complete hashed stdout stream"}
    receipt["semantic_row_raw_line_indices"] = row_positions
    receipt["footer_raw_line_index"] = len(lines) - 1
    receipt["raw_stdout_lines"] = len(lines)
    ensure([entry["text"] for entry in diagnostics] == expected_diagnostics,
           "complete source Statistics console content/order matches actual original capture: " + profile)
    ensure(rows[0] == "RM_CASE " + profile, "native fresh-process profile identity")
    if profile == "cold_th":
        ensure(len(rows) == 3 and rows[1] ==
               "RM_COLD_NATIVE NULL_REFERENCE iz <init> pc2 registry_pc3674 active_ep_pc23" and rows[2] ==
               "RM_CONTROL cold_erroneous candidate=1 entries=18 bw_stores=0 last_put=478 repeat=CLASS_ERRONEOUS",
               "typed real cold cause plus independently checked failed-prefix/repeat control")
    else:
        actor = [row for row in rows if row.startswith("RM_NATIVE_ACTOR ")]
        graph = [row for row in rows if row.startswith("RM_NATIVE_GRAPH ")]
        ensure(actor == [ABSENT_ACTOR] and len(graph) == 1 and re.fullmatch(
            r"RM_NATIVE_GRAPH [1-9][0-9]* [1-9][0-9]*", graph[0]), "honest actual RAW actor and native graph observation inventory")
        ensure(int(graph[0].split()[1]) <= 32768 and int(graph[0].split()[2]) <= 16 * 1024 * 1024,
               "bounded native typed object snapshot, never Java allocation count parity")
        count = {"prepared_awake": 259, "prepared_null": 172, "controls": 165}[profile]
        ensure(len(rows) == count, "complete native records including explicit extra diagnostics")
        controls = [row for row in rows if row.startswith("RM_CONTROL ")]
        ensure(controls == (CONTROLS if profile == "controls" else []), "separate authored control inventory")
        allowed = set(original.TAGS) | {"RM_NATIVE_ACTOR", "RM_NATIVE_GRAPH", "RM_CONTROL"}
        ensure(all(row.split()[0] in allowed for row in rows), "no silently ignored native output kind")
    return rows, int(footer[1])


def column_reason(tag, column):
    if tag == "RM_PIG":
        return "Original genuine wh/Pig constructor versus native genuine RAW dc/LocalPlayer; no constructor/Class/ID/Watcher equivalence."
    if tag == "RM_PREP" and column in (9, 10, 11, 12):
        return "Native public statistic view exposes these actual arrays without allocation lengths; original lengths remain original-only."
    if tag == "RM_CTOR" and column == 2:
        return "Actual native bucket map is not a java.util.HashMap runtime object or physical layout."
    if tag == "RM_CTOR" and column in (7, 8):
        return "Original9368 reflected slots/880 arrays count Java architecture; native typed snapshot counts are independently measured, not substituted."
    if tag == "RM_N" and column == 2:
        return "Actual native RAW LocalPlayer dispatch declares gs.N; original genuine Pig inherited ls.N. Branch result is compared independently."
    absent = (3, 4, 8) if tag == "RM_SIDE" else (2, 3, 7) if tag == "RM_NULL_SIDE" else ()
    if column in absent:
        return "Native unconstructed actor Random/Math holder/Watcher absence cannot qualify preservation of the original instantiated child."
    return None


def compare_profile(profile, raw_records, native, ledger, mismatches):
    if profile == "cold_th":
        for index, line in raw_records:
            cells = line.split()
            row = {"raw_index": index, "profile": profile, "original": line, "columns": []}
            if cells[0] == "RM_CASE":
                row["native_row"] = 0
                for column, value in enumerate(cells):
                    actual = native[0].split()[column]
                    row["columns"].append({"column": column, "credit": "EXACT", "native_column": column, "actual": actual})
                    if value != actual:
                        mismatches.append({"raw_index": index, "column": column, "expected": value, "actual": actual})
            else:
                row["native_row"] = 1 if cells[1] == "1" else None
                for column, value in enumerate(cells):
                    if cells[1] == "1" and column in (2, 3, 4):
                        native_column = {2: 1, 3: 2, 4: 3}[column]
                        actual = native[1].split()[native_column]
                        if column == 2:
                            actual = {"NULL_REFERENCE": "java.lang.NullPointerException"}.get(actual, actual)
                        row["columns"].append({"column": column, "credit": "TYPED_STATUS" if column == 2 else "EXACT",
                                               "native_column": native_column, "actual": actual,
                                               "rule": "Actual source NULL_REFERENCE category names Java NPE; no Throwable or stack reconstruction." if column == 2 else "actual retained leaf constructor cause"})
                        if actual != value:
                            mismatches.append({"raw_index": index, "column": column, "expected": value, "actual": actual})
                    else:
                        row["columns"].append({"column": column, "credit": "UNQUALIFIED",
                                               "reason": "Original Throwable wrapper/index/Java stack line is retained without fabricated native Throwable or line-to-PC mapping."})
            ledger.append(row)
        return
    common = [(i, row) for i, row in enumerate(native) if not row.startswith(("RM_NATIVE_ACTOR ", "RM_NATIVE_GRAPH "))]
    expected_common = [(i, row) for i, row in raw_records if not row.startswith("RM_PIG ")]
    ensure(len(common) == len(expected_common), "complete semantic row alignment: " + profile)
    cursor = 0
    for index, line in raw_records:
        cells = line.split()
        tag = cells[0]
        row = {"raw_index": index, "profile": profile, "original": line, "columns": []}
        if tag == "RM_PIG":
            row["native_row"] = None
            row["columns"] = [{"column": column, "credit": "UNQUALIFIED", "reason": column_reason(tag, column)}
                              for column in range(len(cells))]
            ledger.append(row)
            continue
        native_index, native_line = common[cursor]
        cursor += 1
        actual = native_line.split()
        ensure(len(actual) == len(cells) and actual[0] == tag, "whole ordered native row width/tag: " + profile + "/" + tag)
        row["native_row"] = native_index
        for column, value in enumerate(cells):
            reason = column_reason(tag, column)
            if reason is not None:
                expected_uncredited = "gs" if tag == "RM_N" else "native_bucket_map" if tag == "RM_CTOR" and column == 2 else \
                    "absent" if tag in ("RM_SIDE", "RM_NULL_SIDE") else "uncredited"
                ensure(actual[column] == expected_uncredited, "explicit native limit marker: " + tag)
                entry = {"column": column, "credit": "UNQUALIFIED", "native_column": column,
                         "actual": actual[column], "reason": reason}
            else:
                projected = tag == "RM_NULL_THROW" and column == 4
                compared = "pc29" if projected and value == "101" else value
                entry = {"column": column, "credit": "PINNED_PC" if projected else "EXACT",
                         "native_column": column, "actual": actual[column]}
                if projected:
                    entry["rule"] = "Original Java th.a line101 corresponds to independently pinned original frame PC29; raw line101 retained unchanged."
                if actual[column] != compared:
                    mismatches.append({"raw_index": index, "column": column, "expected": compared, "actual": actual[column]})
            row["columns"].append(entry)
        ledger.append(row)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--resource-root", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    resources = args.resource_root.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="render-manager-frame-", dir=args.work_dir.resolve()))
    raw = json.loads((HERE / "finite.json").read_text(encoding="utf-8"))["rows"]
    provenance = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
    pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
    cases = json.loads((HERE / "frame-inputs.json").read_text(encoding="utf-8"))["cases"]
    ensure(digest(HERE / "finite.json") == provenance["fixture_sha256"] == RAW_SHA256 and provenance["rows"] == 432,
           "unchanged full genuine original432-row fixture/provenance")
    groups = partition(raw)
    inputs = original_inventory(groups, cases, pins)
    check_original_pins(provenance, pins)
    original_profiles = provenance["profiles"]
    ensure([profile["name"] for profile in original_profiles] == list(PROFILES),
           "complete original source-console profile authority")
    console_authority = {}
    for profile in original_profiles:
        name = profile["name"]
        diagnostics = profile["source_console_diagnostics"]
        ensure(profile["status"] == "PASS" and profile["rows"] == len(groups[name]) and
               re.fullmatch(r"[0-9a-f]{64}", profile["original_stdout_sha256"]) and
               diagnostics == ([] if name == "cold_th" else ["151 recipes", "16 achievements"]),
               "actual successful original source console inventory and raw stdout pin: " + name)
        console_authority[name] = {"original_stdout_sha256": profile["original_stdout_sha256"],
                                   "source_console_diagnostics": diagnostics}
    input_path = out / "inputs-only.tsv"
    input_path.write_text("\n".join(inputs) + "\n", encoding="ascii", newline="\n")
    before = bind_files(executable, input_path)
    resources_before = resource_files(resources)
    report = {"status": "FAIL", "original_fixture_sha256": RAW_SHA256, "original_rows": 432,
              "original_profiles": {p: len(groups[p]) for p in PROFILES}, "selected_original_classes": len(pins["classes"]),
              "original_source_console_authority": console_authority,
              "input_before_sha256": before, "resource_root": str(resources), "resources_before": resources_before,
              "input_only": {"path": str(input_path), "sha256": digest(input_path), "cases": 24,
                             "fields": "actual source-stored partial and ten common SN pose words only; no expected results, constructor counts or side effects"},
              "limits": ["Java reflection setter behavior is not compared; independently observed stored operands are supplied.",
                         "Original Pig constructor/READY type versus native RAW LocalPlayer and gs.N declaring class is explicit, uncredited.",
                         "Java map architecture and reflected slot/array counts are original-only; native graph bytes/objects measured separately.",
                         "Exact common numeric/reference/branch/pose/renderer-Random/ID/graph/origin method channels; no NaN normalization or tolerance.",
                         "No World/Engine/Font/Settings instance, sleeping/bed, GL/device, draw or whole-startup/whole-frame completion credit.",
                         "Current README is bound as current documentation; original execution pins remain exact and historical README instructions retained in provenance."],
              "native": {}, "column_credit_inventory": [], "authored_controls_original_parity_credit": False}
    try:
        mismatches = []
        semantic_streams = {}
        for profile in (*PROFILES, "controls"):
            command = [str(executable), str(resources), profile, str(input_path.resolve())]
            receipt = capture(command, out, profile, 120, os.environ.copy(), out.resolve())
            report["native"][profile] = receipt
            ensure(receipt["command"] == command, "actual native executable receives only authored resource root/profile and input-only file")
            authority = "prepared_awake" if profile == "controls" else profile
            receipt["source_console_original_profile"] = authority
            expected_diagnostics = console_authority[authority]["source_console_diagnostics"]
            rows, checks = read_capture(receipt, out, profile, expected_diagnostics)
            semantic_streams[profile] = rows
            report["native"][profile].update(rows=len(rows), checks=checks)
            if profile == "controls":
                # The control process must execute the same real full constructor
                # prefix, not replace it with a smaller control-only object.
                prefix = [row for row in rows if not row.startswith(("RM_CASE ", "RM_CONTROL ", "RM_NATIVE_GRAPH "))]
                awake_rows = semantic_streams["prepared_awake"]
                awake_prefix = [row for row in awake_rows if not row.startswith(("RM_CASE ", "RM_INPUT ", "RM_N ", "RM_FRAME ", "RM_SIDE ", "RM_NATIVE_GRAPH "))]
                ensure(prefix == awake_prefix, "independent controls retain the same completed28-renderer logical constructor inventory")
                report["authored_controls"] = CONTROLS
            else:
                compare_profile(profile, groups[profile], rows, report["column_credit_inventory"], mismatches)
        ledger = report["column_credit_inventory"]
        ensure([row["raw_index"] for row in ledger] == list(range(432)) and all(
            [c["column"] for c in row["columns"]] == list(range(len(row["original"].split()))) for row in ledger),
            "all432 raw source rows and every column explicitly accounted, with no dropped row/column")
        report["column_credit_counts"] = dict(Counter(c["credit"] for row in ledger for c in row["columns"]))
        report["mismatch_count"] = len(mismatches)
        report["mismatches"] = mismatches[:30]
        ensure(not mismatches, "common exact native/original channels differ; see complete credit ledger/run.json")
        report["status"] = "PASS"
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    finally:
        after = bind_files(executable, input_path)
        resources_after = resource_files(resources)
        report["input_after_sha256"] = after
        report["resources_after"] = resources_after
        if before != after or resources_before != resources_after:
            report.update(status="FAIL", error="native/reference/source/input/executable or resource fixture bytes changed")
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "original_rows": 432,
                      "mismatch_count": report.get("mismatch_count"), "report": str(out / "run.json")}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
