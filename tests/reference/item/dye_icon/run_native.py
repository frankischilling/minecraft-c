"""Compare actual native Dye icon leaves with the retained original scalar packet."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
FIELDS = {"index", "name", "bh", "damage", "direct_icon", "stack_icon", "color", "full3d", "rotate"}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_packet(directory):
    spec = importlib.util.spec_from_file_location("dye_icon_original_packet", directory / "packet.py")
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    return packet, packet.read_packet(directory)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate native observation field")
        result[key] = value
    return result


def compare_rows(expected, stdout):
    lines = [line for line in stdout.splitlines() if line.startswith("DYE_ICON_NATIVE")]
    require(len(expected) == 33 and len(lines) == 35, "complete ordered Dye icon inventory required")
    require(lines[0] == 'DYE_ICON_NATIVE_INIT\t{"id":351,"initial_bh":78}',
            "native actual registered Dye identity differs")
    require(lines[-1] == "DYE_ICON_NATIVE_COMPLETE rows=33 restored_bh=78",
            "native complete restored Dye cohort required")
    rows = []
    for ordinal, (line, original) in enumerate(zip(lines[1:-1], expected)):
        require(line.startswith("DYE_ICON_NATIVE\t"), "typed native Dye icon row required")
        row = json.loads(line.split("\t", 1)[1], object_pairs_hook=unique_object)
        require(set(row) == FIELDS, "complete native Dye icon row schema required")
        require(all(type(row[key]) is int for key in FIELDS - {"name", "full3d", "rotate"}) and
                type(row["name"]) is str and type(row["full3d"]) is bool and
                type(row["rotate"]) is bool, "exact native signed/Boolean scalar types required")
        require(original["direct_icon"] == original["item_stack_icon"] == original["stack_icon"],
                "supporting original inherited Item wrapper differs")
        projected = {key: value for key, value in original.items() if key != "item_stack_icon"}
        require(row == projected, f"native/original Dye icon row {ordinal} differs")
        rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--packet", type=Path, default=HERE)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    directory = args.packet.resolve(strict=True)
    packet, expected = load_packet(directory)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="dye-icon-", dir=args.work_dir.resolve()))
    inputs = [executable, Path(__file__).resolve(), directory / "packet.py",
              *[directory / name for name in packet.PINS],
              ROOT / "src/item/render/leaves.c", ROOT / "src/item/render/internal.h",
              ROOT / "src/item/registry/runtime.c", ROOT / "src/item/stack/stack.c",
              ROOT / "tests/client/render/test_inventory_block.c"]
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "inputs_before": before,
              "scope": "33 finite Dye icon direct/actual IZ wrapper results and inherited GM leaves; no original pixels or GL",
              "supporting_original_item_stack_rows": len(expected),
              "native_entry_paths": ["KM.a(I)I entered icon", "IZ.b()I including inherited GM.b(IZ) child"]}
    try:
        child = subprocess.run([str(executable)], capture_output=True, timeout=120)
        (output / "native.stdout").write_bytes(child.stdout)
        (output / "native.stderr").write_bytes(child.stderr)
        report["exit"] = child.returncode
        require(child.returncode == 0 and child.stderr == b"", "native inventory controls failed")
        native_text = child.stdout.decode("utf-8")
        all_lines = native_text.splitlines()
        require(all_lines and re.fullmatch(
            r"Inventory Block controls: [1-9][0-9]* checks; real Dirt/BlockItem/NW failure prefixes",
            all_lines[-1]), "native inventory controls did not reach their final completion marker")
        rows = compare_rows(expected, native_text)
        report.update(status="PASS", rows=len(rows))
    finally:
        report["inputs_after"] = {str(path): digest(path) for path in inputs}
        if report["inputs_after"] != before:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        require(report["inputs_after"] == before, "Dye icon comparison inputs changed")
    print("PASS: 33 exact original Dye icon scalar rows; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
