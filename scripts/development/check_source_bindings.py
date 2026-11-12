"""Check asset-independent current test inputs before configuring a native build."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

PROFILES = (
    ("application_owner", ("native_sources", "reference_files", "controller_files", "shared_helpers")),
    ("biped", ("native_sources", "reference_files", "controller_files", "shared_helpers")),
    ("render_resources", ("source_files",)),
    ("display_sdk", ("source_files",)),
    ("loading_draw", ("native_sources",)),
    ("resource_lookup_common", ("source_files",)),
)


LIVE_BUNDLES = (
    ("tests/tools/entity/damage_native_close_bundle.json",
     "tests/tools/entity/test_damage_native_close.py", ("files",)),
    ("tests/tools/world/kernel_execution_bundle.json",
     "tests/tools/world/test_kernel_execution.py", ("sources", "helpers")),
    ("tests/tools/world/world_query_controls_bundle.json",
     "tests/tools/world/test_world_query_controls.py", ("sources", "helpers")),
    ("tests/tools/world/arrow_natural_flight_bundle.json",
     "tests/tools/world/test_arrow_natural_flight.py", ("sources",)),
    ("tests/tools/client/arrow_world_tick_cleanup_bundle.json",
     "tests/tools/client/test_arrow_world_tick_cleanup.py", ("sources", "helpers")),
)


def current_pin(root, relative, observed, label):
    if type(relative) is not str:
        raise ValueError(f"{label}: expected relative source path")
    path = (root / relative).resolve(strict=True)
    if not path.is_relative_to(root):
        raise ValueError(f"{label}: path outside source root: {relative}")
    if path not in observed:
        data = path.read_bytes()
        observed[path] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    return observed[path]


def check_current_table(root, label, files, observed):
    if type(files) is not dict or not files:
        raise ValueError(f"{label}: expected declared input table")
    for relative, expected in files.items():
        if (type(expected) is not dict or set(expected) != {"bytes", "sha256"} or
                type(expected["bytes"]) is not int or expected["bytes"] < 0 or
                type(expected["sha256"]) is not str or
                re.fullmatch(r"[0-9a-f]{64}", expected["sha256"]) is None):
            raise ValueError(f"{label}: invalid binding for {relative}")
        if current_pin(root, relative, observed, label) != expected:
            raise ValueError(f"{label}: stale source binding: {relative}")
    return len(files)


def check_live_bundles(root, observed=None):
    root = root.resolve(strict=True)
    if observed is None:
        observed = {}
    declared = {bundle for bundle, _, _ in LIVE_BUNDLES}
    actual = {path.relative_to(root).as_posix()
              for path in (root / "tests/tools").rglob("*_bundle.json")}
    if actual != declared:
        raise ValueError("Live bundles: current bundle inventory differs from declared guards")
    count = 0
    for bundle, helper, groups in LIVE_BUNDLES:
        proof = json.loads((root / bundle).read_text(encoding="utf-8"))
        if type(proof) is not dict:
            raise ValueError(f"{bundle}: expected declared bundle")
        for group in groups:
            count += check_current_table(root, f"{bundle}.{group}", proof.get(group), observed)
        literals = re.findall(r"^BUNDLE_SHA = '([0-9a-f]{64})'$",
                              (root / helper).read_text(encoding="utf-8"), re.MULTILINE)
        if (len(literals) != 1 or
                literals[0] != current_pin(root, bundle, observed, bundle)["sha256"]):
            raise ValueError(f"{helper}: stale or invalid BUNDLE_SHA for {bundle}")
    return count


def check(root):
    root = root.resolve(strict=True)
    observed = {}
    count = 0
    for profile, groups in PROFILES:
        metadata = root / "tests/reference/client" / profile / "provenance.json"
        proof = json.loads(metadata.read_text(encoding="utf-8"))
        for group in groups:
            count += check_current_table(root, f"{profile}.{group}", proof.get(group), observed)
    bundle_count = check_live_bundles(root, observed)
    print(f"Live bundles: {len(LIVE_BUNDLES)} bundles, {bundle_count} source/helper entries, "
          f"{len(LIVE_BUNDLES)} bundle SHA constants")
    print(f"Source bindings: {count} entries, {len(observed)} distinct current files")
    # The sealed Workbench receipt describes its captured checkpoint. Later
    # source changes neither invalidate that history nor inherit its native
    # qualification. Its separate --current-sources check requires byte equality.
    packet = root / "tests/reference/client/workbench_open_shutdown/packet.py"
    spec = importlib.util.spec_from_file_location("workbench_source_bindings", packet)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.audit_public()
    print(f"Workbench historical capture: {result['runtime_sources']} runtime, "
          f"{result['harness_sources']} harness files; no current-source qualification")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    try:
        check(args.root)
    except (OSError, ValueError) as error:
        print(f"Source bindings failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
