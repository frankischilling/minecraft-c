"""Run bounded native-only TH sleeping and lifetime controls on owned files.

These controls have no sleeping original-JAR oracle. The existing original
awake/null fixture is a separate comparison. No JVM, device, replacement
resources, provider callbacks or generated expected method outputs are used.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
CAPTURE = ROOT / "tests/reference/client/controller/capture.py"
WORLD_AUTHOR = ROOT / "tests/tools/client/test_picking.py"
FIXTURE = ROOT / "tests/client/render/test_manager_sleeping.c"
SHARED_FIXTURE = ROOT / "tests/client/render/test_manager.c"
INVENTORY = [
    ("complete28", "entries bw_stores constructor_pc", "ddd"),
    ("math_failed", "status frame_pc math_status math_pc phase last_store zero_words", "ddddddd"),
    ("math_retry", "status frame_pc math_status initial_cause phase last_store", "dddddd"),
    ("null_world", "status frame_pc math_phase math_last_store last_frame_pc", "ddddd"),
    *[(f"bed{i}", "bed_id metadata last_frame_pc yaw pitch x y z", "dddfflll") for i in range(4)],
    ("nonbed", "last_frame_pc yaw pitch x y z", "dfflll"),
    ("world60_failure", "status frame_pc world_status acquisition_status acquisition_cause "
     "activation_pc activation_world_pc load_pc load_builder_pc", "ddddddddd"),
    ("metadata_failure", "world_status acquisition_status acquisition_cause unchanged_output", "dddd"),
    ("lifetime_holds", "retained_engines retained_fonts retained_settings engine_holds "
     "font_holds settings_borrows", "dddddd"),
    ("lifetime_displaced", "retained_engines retained_fonts retained_settings", "ddd"),
    ("settings_active_refusal", "diagnostic_calls manager_status settings_status retained_engines "
     "retained_fonts retained_settings", "dddddd"),
    ("lifetime_released", "engine_holds font_holds settings_borrows", "ddd"),
]
# Each full genuine Statistics-first namespace prints this source sequence.
# The native constructor JUnit's Statistics-first complete profile records
# 151 recipes then16 achievements (the separate Blocks-first profile differs).
# Positions are relative to completed observer rows, never ignored stdout.
BOOTSTRAPS = [
    {"context": "failed_math_namespace", "before_row": 0,
     "lines": ["151 recipes", "16 achievements"]},
    {"context": "ready_math_sleeping_namespace", "before_row": 3,
     "lines": ["151 recipes", "16 achievements"]},
    {"context": "lifetime_namespace", "before_row": 11,
     "lines": ["151 recipes", "16 achievements"]},
]
SOURCE_FILES = [
    Path(__file__).resolve(), CAPTURE, WORLD_AUTHOR, FIXTURE, SHARED_FIXTURE,
    ROOT / "include/mc/client/render/manager/manager.h",
    ROOT / "src/client/render/interpolation_internal.h",
    ROOT / "src/entity/player/tick_math_internal.h",
    ROOT / "src/entity/player/player_internal.h",
    ROOT / "include/mc/entity/player/player.h",
    ROOT / "include/mc/core/math/classes.h",
    ROOT / "include/mc/core/math/math.h",
    ROOT / "src/core/math/classes.c",
    ROOT / "src/core/math/math.c",
    ROOT / "include/mc/world/runtime/blocks.h",
    ROOT / "src/world/runtime/blocks.c",
    ROOT / "src/world/runtime/runtime.c",
    ROOT / "include/mc/client/settings/game_settings.h",
    ROOT / "include/mc/client/settings/construction.h",
    ROOT / "src/client/settings/game_settings.c",
    ROOT / "src/statistics/registry/initializer.c",
    ROOT / "src/statistics/achievements/achievements.c",
    ROOT / "src/item/recipes/managers.c",
    ROOT / "src/platform/text/stdout.c",
]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def files(root):
    """Complete regular-file inventory; do not follow fixture symlinks."""
    answer = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise AssertionError(f"fixture contains a symlink: {path}")
        if path.is_file():
            answer[path.relative_to(root).as_posix()] = {
                "bytes": path.stat().st_size, "sha256": digest(path)}
    return answer


def sources():
    manager_files = [path for path in (ROOT / "src/client/render/manager").rglob("*")
                     if path.is_file() and path.suffix in {".c", ".h"}]
    paths = sorted(set(SOURCE_FILES + manager_files))
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in paths}


def rows(stdout):
    lines = stdout.decode("utf-8", errors="strict").splitlines()
    if len(lines) != len(INVENTORY) + sum(len(item["lines"]) for item in BOOTSTRAPS) + 1:
        raise AssertionError("complete15 native rows, six positioned source diagnostics, "
                             "and one terminal cleanup marker required")
    observed = []
    diagnostics = []
    cursor = 0
    for index, (name, columns, types) in enumerate(INVENTORY):
        for bootstrap in BOOTSTRAPS:
            if bootstrap["before_row"] == index:
                expected = bootstrap["lines"]
                if lines[cursor:cursor + len(expected)] != expected:
                    raise AssertionError(f"source constructor stdout sequence/position: {bootstrap}")
                diagnostics.append({"context": bootstrap["context"], "before_row": index,
                                    "first_stdout_line": cursor + 1,
                                    "lines": lines[cursor:cursor + len(expected)]})
                cursor += len(expected)
        line = lines[cursor]
        cursor += 1
        tokens = line.split(" ")
        if tokens[:2] != ["MANAGER_SLEEP", name] or len(tokens) != len(types) + 2:
            raise AssertionError(f"native row order/width: {line}")
        for value, kind in zip(tokens[2:], types):
            pattern = {"d": r"-?(?:0|[1-9][0-9]*)", "f": r"[0-9a-f]{8}",
                       "l": r"[0-9a-f]{16}"}[kind]
            if re.fullmatch(pattern, value) is None:
                raise AssertionError(f"native row token: {line}")
        observed.append({"control": name, "tokens": tokens[2:],
                         "fields": dict(zip(columns.split(), tokens[2:]))})
    footer = re.fullmatch(
        r"Native TH sleeping controls: 15 rows; ([1-9][0-9]*) checks; cleanup complete", lines[cursor])
    if footer is None or int(footer[1]) < 2 * 65536:
        raise AssertionError("checked native retirement and both complete published-zero scans required")
    return observed, int(footer[1]), diagnostics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resource-root", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    resources = args.resource_root.resolve(strict=True)
    if not executable.is_file() or not resources.is_dir():
        parser.error("a real executable and explicit existing resource directory are required")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="manager-sleeping-", dir=args.work_dir.resolve()))
    report_path = output / "run.json"
    report = {
        "status": "FAIL", "kind": "native-only-th-sleeping-controls",
        "original_sleeping_equality_claim": False,
        "actor": {"actual_type": "dc/LocalPlayer", "phase": "RAW", "dispatch": "gs.N()Z",
                  "declared_input": "actual owned Player.u=true and inherited SN pose fields",
                  "constructor_parity_claim": False, "pig_runtime_class_parity_claim": False},
        "coverage": {
            "statistics_first_complete_28": True,
            "actual_game_world_store_provider_cache_loaded_chunk": True,
            "bed_metadata_0_1_2_3_th101_success": True,
            "nonbed_angle_preservation": True,
            "null_world_failure_th60_after_reached_math_floors": True,
            "math_failed_published_array_and_sticky_retry": True,
            "failed_and_ready_math_use_separate_completed_application_namespaces": True,
            "th60_actual_acquisition_limit_with_complete_borrowed_child": True,
            "standalone_metadata_detailed_failure_single_call": True,
            "isolated_th101_failure_exercised": False,
            "isolated_th101_failure_bound": "same validated chunk; no intervening source effect "
                "can fail its fixed embedded metadata read without an invented mutation hook",
            "raw_engine_font_and_default_settings_holds": True,
            "displaced_occurrences_retained": True,
            "active_settings_refuses_actual_release_after_font_first_cleanup": True,
            "checked_retirement_retry_and_caller_cleanup": True,
            "typed_source_settings_active_preflight_exercised": False,
            "world_bed_placement_or_actor_constructor_parity_claim": False,
        },
        "inventory": [{"control": name, "columns": columns.split()} for name, columns, _ in INVENTORY],
        "bootstrap_inventory": BOOTSTRAPS,
        "executable": str(executable), "resource_root": str(resources), "output": str(output),
        "jvm_processes": 0, "device_contexts": 0,
    }
    try:
        before_sources = sources()
        before_executable = digest(executable)
        before_resources = files(resources)
        if not before_resources:
            raise AssertionError("resource input must contain actual fixture files")
        report.update(source_sha256_before=before_sources,
                      executable_sha256_before=before_executable,
                      resource_inventory_before=before_resources)
        # Only the existing authored NBT/region writer is reused. Its separate
        # resource generators and test main are not invoked or substituted.
        author = load_module("manager_sleeping_authored_world", WORLD_AUTHOR)
        world = output / "world"
        author.authored_world(world)
        world_before = files(world)
        (output / "world-inputs.json").write_text(json.dumps(world_before, indent=2) + "\n")
        report["world_inventory_before"] = world_before
        support = load_module("manager_sleeping_bounded_capture", CAPTURE)
        process = support.capture([str(executable), str(resources), str(world)], output,
                                  "native", timeout=90, working_directory=output)
        report["process"] = process
        report["world_inventory_after"] = files(world)
        report["resource_inventory_after"] = files(resources)
        report["source_sha256_after"] = sources()
        report["executable_sha256_after"] = digest(executable)
        support.require_success(process, empty_stderr=True)
        observed, checks, diagnostics = rows((output / "native.stdout").read_bytes())
        if report["source_sha256_after"] != before_sources:
            raise AssertionError("authored fixture/support or source changed during native execution")
        if report["resource_inventory_after"] != before_resources:
            raise AssertionError("explicit resource fixture changed during native execution")
        if report["executable_sha256_after"] != before_executable:
            raise AssertionError("native executable changed during execution")
        if report["world_inventory_after"].get("session.lock", {}).get("bytes") != 8:
            raise AssertionError("genuine Store session-lock side effect missing")
        missing = sorted(set(world_before) - set(report["world_inventory_after"]))
        if missing:
            raise AssertionError(f"initial owned World artifacts were removed: {missing}")
        changed = sorted(name for name in world_before
                         if world_before[name] != report["world_inventory_after"][name])
        if set(changed) - {"region/r.0.0.mcr"}:
            raise AssertionError(f"unexpected initial World file mutation: {changed}")
        report.update(status="PASS", rows=observed, row_count=len(observed), checks=checks,
                      complete_inventory=True, cleanup_complete=True,
                      bootstrap_diagnostics=diagnostics, owned_artifacts_recorded=True,
                      initial_world_files_retained=True, changed_initial_world_files=changed,
                      world_byte_equality_claim=False)
    except BaseException as error:
        report["failure"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2) + "\n")
        print(f"Native TH sleeping controls {report['status']}: {report_path}")


if __name__ == "__main__":
    main()
