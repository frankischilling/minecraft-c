"""Compare all original Beta TileEntityRenderer rows with four fresh CPU owners.

The sole semantic projection replaces the original null-Class Java stack-line
token with its separately pinned source PC26. Raw finite.json and its complete
digest remain bound. No geometry, field, lookup or Map value is normalized.
This does not compare Java identity hashes, physical bucket order or stack
line numbers with native diagnostics. No JAR, JVM, asset or device is needed.
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


# Reuse the authored capture's declared row/profile metadata. Its rows()
# requires raw JVM hash/bucket diagnostics, which finite.json intentionally
# excludes; native validation therefore uses the exact complete finite row
# sequence and shapes below, without manufacturing Java diagnostics.
original = load("tile_renderer_original_schema", HERE / "capture_reference.py")
support = original.support
ensure, digest, capture, require_success = support.ensure, support.digest, support.capture, support.require_success


def split_profiles(rows):
    ensure(isinstance(rows, list) and all(isinstance(row, str) and row and row == row.strip() for row in rows),
           "complete string row inventory")
    result = {}
    current = None
    for row in rows:
        cells = row.split()
        ensure(cells[0] in original.TAGS, "unknown finite record kind")
        if cells[0] == "TILE_CASE":
            ensure(len(cells) == 2 and cells[1] not in result, "one complete header per fresh profile")
            current = cells[1]
            result[current] = []
        ensure(current is not None, "row before fresh-profile identity")
        result[current].append(row)
    ensure(tuple(result) == original.PROFILES, "all four profiles in declared order")
    return result


def inventory(rows, profile):
    ensure(rows[0] == "TILE_CASE " + profile, "profile row identity")
    ensure([row.split()[1] for row in rows if row.startswith("TILE_LITERAL ")] == list(original.TILES),
           "complete passive Class literal inventory")
    for tag, count in (("TILE_RENDERER", 3), ("TILE_CTOR", 1), ("TILE_DEFAULTS", 1)):
        ensure(sum(row.startswith(tag + " ") for row in rows) == count, "constructor inventory: " + tag)
    for tag, count in {"TILE_SIGN": 1, "TILE_PART": 2, "TILE_VERTEX": 16, "TILE_QUAD": 12, "TILE_GRAPH": 1}.items():
        ensure(sum(row.startswith(tag + " ") for row in rows) == (count if profile == "constructor" else 0),
               "complete geometry inventory: " + tag)
    lookups = [row.split() for row in rows if row.startswith("TILE_LOOKUP ")]
    if profile in ("forward", "reverse"):
        order = original.TILES if profile == "forward" else tuple(reversed(original.TILES))
        ensure([(row[1], row[2]) for row in lookups] == [(str(i), key) for i, key in enumerate(order)],
               "complete original lookup/repetition order")
    else:
        ensure(not lookups, "no substituted lookup profiles")
    stages = ["initial"]
    if profile in ("forward", "reverse"):
        stages.extend(f"lookup{i:02d}" for i in range(9))
    if profile == "nulls":
        stages.append("after_nulls")
    maps = [row.split() for row in rows if row.startswith("TILE_MAP ")]
    ensure([row[1] for row in maps] == stages, "all reached Map snapshots")
    for record in maps:
        ensure(len(record) == 8 and all(value.isdigit() for value in record[2:]), "Map snapshot schema")
        entries = [row for row in rows if row.startswith("TILE_ENTRY " + record[1] + " ")]
        ensure(len(entries) == int(record[2]) and entries == sorted(entries), "all logical Map entries")
    for tag in ("TILE_NULL_TILE", "TILE_NULL_CLASS"):
        ensure(sum(row.startswith(tag + " ") for row in rows) == int(profile == "nulls"), "complete null-path inventory")
    for row in rows:
        cells = row.split()
        if cells[0] == "TILE_PART":
            ensure(len(cells) == 15 and all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[-6:]), "part raw float words")
        elif cells[0] == "TILE_VERTEX":
            ensure(len(cells) == 8 and all(re.fullmatch(r"[0-9a-f]{16}", value) for value in cells[3:6]) and
                   all(re.fullmatch(r"[0-9a-f]{8}", value) for value in cells[6:]), "position/UV raw words")
        elif cells[0] == "TILE_QUAD":
            ensure(len(cells) == 21 and all(re.fullmatch(r"[0-9a-f]{8}", cells[i])
                   for base in range(5, 21, 4) for i in (base + 2, base + 3)), "quad UV/alias raw words")


def project_original(rows, site):
    projected, exceptions = [], []
    ensure(site["class"] == "ll" and site["method"] == "a" and
           site["descriptor"] == "(Ljava/lang/Class;)Lje;" and site["pc"] == 26,
           "independent original null-Class bytecode site")
    for row in rows:
        if row.startswith("TILE_NULL_CLASS "):
            cells = row.split()
            ensure(len(cells) == 8 and cells[1:4] == ["java.lang.NullPointerException", "ll", "a"] and
                   re.fullmatch(r"-?[0-9]+", cells[4]), "complete raw original exception/stack-line row")
            exceptions.append({"raw_original_row": row, "original_java_stack_line": int(cells[4]),
                               "source_pc": site["pc"], "not_compared": "Java stack line versus native PC"})
            cells[4] = "pc" + str(site["pc"])
            row = " ".join(cells)
        projected.append(row)
    ensure(len(exceptions) == 1, "exactly one declared semantic projection in all 254 rows")
    return projected, exceptions


def bindings(executable):
    files = [executable, HERE / "run_native.py", HERE / "finite.json", HERE / "provenance.json",
             HERE / "source-pins.json", HERE / "capture_reference.py", HERE / "TileEntityRendererObservation.java",
             HERE / "README.md", HERE.parent / "controller/capture.py", HERE.parent / "notice/classfile.py",
             REPO / "tests/client/render/test_tile_entity_renderer.c",
             REPO / "include/mc/client/render/tile_entity_renderer/tile_entity_renderer.h",
             REPO / "include/mc/client/render/model.h", REPO / "src/client/render/model.c",
             REPO / "src/client/render/model_graph.c", REPO / "src/client/render/model_graph_internal.h",
             REPO / "src/client/render/interpolation_internal.h",
             REPO / "include/mc/world/runtime/tile_entities/tile_entities.h",
             REPO / "src/world/runtime/tile_entities/tile_entities.c",
             REPO / "src/world/runtime/construction.c",
             REPO / "src/block/registry/registry.c"]
    files.extend(sorted(path for path in (REPO / "src/client/render/tile_entity_renderer").rglob("*")
                        if path.is_file() and path.suffix in (".c", ".h")))
    return {str(path.resolve(strict=True)): digest(path) for path in files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="tile-entity-renderer-", dir=args.work_dir.resolve()))
    before = bindings(executable)
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "scope": "Four fresh genuine canonical CPU TileEntityRenderer owners; complete constructor/geometry, "
                       "passive Class, nullable Map/cache and source null-prefix comparison. No World, assets, device or draw. "
                       "No JVM physical Map-order equivalence or Java stack-line comparison."}
    try:
        provenance = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
        pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
        ensure(digest(HERE / "finite.json") == provenance["fixture_sha256"] and provenance["rows"] == 254,
               "complete unchanged 254-row genuine original fixture digest")
        ensure(provenance["client_jar_sha256"] == pins["client_jar_sha256"] == original.JAR,
               "same untouched original JAR source authority, no local JAR required")
        for name in ("TileEntityRendererObservation.java", "capture_reference.py", "source-pins.json"):
            ensure(digest(HERE / name) == provenance["capture_input_sha256"][name], "original captured source bytes: " + name)
        lookup = [method for method in pins["classes"]["ll"]["methods"] if
                  method["name"] == "a" and method["descriptor"] == "(Ljava/lang/Class;)Lje;"]
        ensure(len(lookup) == 1 and lookup[0]["code_sha256"] ==
               "ce00643d19bb74abf5d5c2424fabd0212895a2fecb3231865c65a338c6052b36",
               "unchanged lookup Code authority for the PC26 projection")
        raw = json.loads((HERE / "finite.json").read_text(encoding="utf-8"))["rows"]
        raw_profiles = split_profiles(raw)
        projected, exceptions = project_original(raw, provenance["null_class_pc"])
        expected = split_profiles(projected)
        ensure(len(raw) == len(projected) == 254, "complete projection preserves record inventory")
        report.update(original_fixture_sha256=digest(HERE / "finite.json"), semantic_projection=exceptions,
                      original_profiles=provenance["profiles"])
        for profile in original.PROFILES:
            inventory(raw_profiles[profile], profile)
            inventory(expected[profile], profile)
            receipt = capture([str(executable), profile], out, profile, 60, os.environ.copy(), out.resolve())
            item = {"name": profile, "capture": receipt}
            report["profiles"].append(item)
            require_success(receipt, empty_stderr=True)
            for stream in ("stdout", "stderr"):
                path = out / receipt[stream + "_file"]
                ensure(path.stat().st_size == receipt[stream + "_bytes"] and
                       digest(path) == receipt[stream + "_sha256"], "complete unchanged native stream receipt")
            text = (out / receipt["stdout_file"]).read_text(encoding="utf-8")
            lines = text.splitlines()
            footer = re.fullmatch(r"Source TileEntityRenderer: " + profile +
                                  r"; rows=([1-9][0-9]*); checks=([1-9][0-9]*); cleanup=1", lines[-1] if lines else "")
            ensure(footer is not None, "actual normal terminal completion after checked cleanup")
            actual = [line for line in lines if line.split(" ", 1)[0] in original.TAGS]
            errors = [line for line in lines if line.startswith("TILE_NATIVE_ERROR ")]
            ensure(len(errors) == int(profile == "nulls") and
                   len(lines) == len(actual) + len(errors) + 1, "complete native stdout inventory, no ignored output")
            if errors:
                cells = errors[0].split()
                ensure(len(cells) == 9 and cells[1].isdigit() and int(cells[1]) > 0 and
                       cells[2:] == ["NULL_REFERENCE", "0", "ll", "a", "(Ljava/lang/Class;)Lje;", "26", "26"],
                       "actual native source-null error category/policy/class/method/descriptor/PC channel")
            inventory(actual, profile)
            item.update(rows=len(actual), checks=int(footer[2]), native_error_rows=errors,
                        mismatches=[{"index": index,
                                     "native": actual[index] if index < len(actual) else None,
                                     "original_projection": expected[profile][index] if index < len(expected[profile]) else None}
                                    for index in range(max(len(actual), len(expected[profile])))
                                    if index >= len(actual) or index >= len(expected[profile]) or actual[index] != expected[profile][index]])
            ensure(int(footer[1]) == len(actual) == len(expected[profile]) and actual == expected[profile],
                   "native/original TileEntityRenderer rows differ; see run.json")
            item["status"] = "PASS"
        ensure(sum(item["rows"] for item in report["profiles"]) == 254, "all four complete original profiles")
        report.update(status="PASS", rows=254)
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
