"""Compare unchanged-original hand-pass matrices on genuine native startup owners."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}


def floating(bits):
    if re.fullmatch(r"[0-9a-f]{8}", bits) is None:
        raise AssertionError("hand matrix float encoding differs")
    return struct.unpack(">f", bytes.fromhex(bits))[0]


def compare(actual, expected):
    if len(actual) != 64 or len(expected) != 64:
        raise AssertionError("64 hand outer outcomes required")
    maximum = 0.0
    for index, (native, original) in enumerate(zip(actual, expected)):
        a, b = native.split(), original.split()
        if len(a) != 42 or len(b) != 42 or a[:10] != b[:10] or int(a[1]) != index:
            raise AssertionError(f"hand outer identity/outcome/stack differs at row {index}")
        for component, (left, right) in enumerate(zip(a[10:], b[10:])):
            lvalue, rvalue = floating(left), floating(right)
            delta = abs(lvalue - rvalue)
            if not math.isfinite(lvalue) or not math.isfinite(rvalue) or delta > 2e-6:
                raise AssertionError(f"hand matrix row {index} component {component}: {left} != {right}")
            maximum = max(maximum, delta)
    return maximum


def arm_geometry(stdout):
    rows = []
    for line in stdout.splitlines():
        if not line.startswith("ARM_WORDS "):
            continue
        cells = line.split()
        if (len(cells) != 4 or int(cells[1]) != len(rows) or int(cells[2]) != 6
                or re.fullmatch(r"[0-9a-f]{336}", cells[3]) is None):
            raise AssertionError("six selected arm batch words required")
        rows.append({"face": int(cells[1]), "vertices": 6,
                     "sha256": hashlib.sha256(bytes.fromhex(cells[3])).hexdigest()})
    if len(rows) != 6:
        raise AssertionError("six actual compiled arm faces required")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="first-person-hand-", dir=args.work_dir.resolve()))
    resources = ROOT / "tests/client/startup/fixtures"
    cloud_helper = Path(__file__).with_name("test_render_global_clouds.py")
    assets = out / "assets"
    module("hand_authored_assets", cloud_helper).authored_assets(assets)
    png_helper = Path(__file__).with_name("test_startup.py")
    skin = assets / "jar/mob/char.png"
    skin.parent.mkdir(parents=True, exist_ok=True)
    skin.write_bytes(module("hand_authored_skin", png_helper).authored_png(
        64, 32, lambda x, y: (191, 73, 229, 255)))
    world_helper = Path(__file__).with_name("test_picking.py")
    world_module = module("hand_authored_world", world_helper)
    for phase in ("outer", "arm"):
        world_module.authored_world(out / phase / "world")
    executable = args.executable.resolve(strict=True)
    reference = ROOT / "tests/reference/client/first_person_hand/outer.json"
    arm_reference = reference.with_name("arm.json")
    full_reference = reference.with_name("full.json")
    inputs = [executable, Path(__file__), cloud_helper, world_helper,
              Path(__file__).with_name("test_startup.py"), reference, arm_reference, full_reference]
    before = {str(p.resolve()): digest(p) for p in inputs}
    assets_before, resources_before = inventory(assets), inventory(resources)
    report = {"status": "FAIL", "input_before_sha256": before,
              "assets_before_sha256": assets_before, "resources_before_sha256": resources_before,
              "audio_backend": "OpenAL Soft null playback device; capture is unavailable"}
    try:
        screenshot = out / "arm.png"
        expected_diagnostic = "[ALSOFT] (WW) No capture backend available!\n"
        report["expected_diagnostics"] = {}
        outputs = {}
        for phase in ("outer", "arm"):
            environment = os.environ.copy()
            environment["ALSOFT_DRIVERS"] = "null"
            phase_root = out / phase
            for name in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
                path = phase_root / name.lower()
                path.mkdir()
                environment[name] = str(path)
            command = [str(executable), str(resources), str(assets), str(phase_root / "world"),
                       str(screenshot), phase]
            result = subprocess.run(command, cwd=phase_root, env=environment, capture_output=True,
                                    text=True, timeout=160)
            (phase_root / "native.stdout.txt").write_text(result.stdout, newline="\n")
            (phase_root / "native.stderr.txt").write_text(result.stderr, newline="\n")
            report["expected_diagnostics"][phase] = result.stderr
            if result.returncode != 0 or result.stderr not in ("", expected_diagnostic):
                raise AssertionError(f"hand {phase} failed ({result.returncode}): {result.stderr}; {out}")
            if not result.stdout.splitlines() or re.fullmatch(
                    rf"First-person hand: [1-9][0-9]* checks; {phase} phase",
                    result.stdout.splitlines()[-1]) is None:
                raise AssertionError("native hand assertion footer missing")
            outputs[phase] = result.stdout
            print(result.stdout.splitlines()[-1])
            if result.stderr:
                print(result.stderr, end="")
        rows = [line for line in outputs["outer"].splitlines() if line.startswith("HAND_OUTER ")]
        expected = json.loads(reference.read_text())["rows"]
        maximum = compare(rows, expected)
        full_rows = [line for line in outputs["outer"].splitlines() if line.startswith("HAND_DRAW ")]
        if full_rows != json.loads(full_reference.read_text())["rows"]:
            report["full_rows"] = full_rows
            raise AssertionError("four full RA state/stack/matrix/attribute/cursor observations differ")
        arm_expected = json.loads(arm_reference.read_text())
        part_rows = [line for line in outputs["arm"].splitlines() if line.startswith("ARM_PART ")]
        if part_rows != arm_expected["part_rows"]:
            report["part_rows"] = part_rows
            raise AssertionError("actual DS/FH/PS lazy compile/reuse observations differ")
        pose_rows = [line for line in outputs["arm"].splitlines() if line.startswith("ARM_POSE ")]
        if len(pose_rows) != 16 or pose_rows != arm_expected["pose_rows"]:
            report["pose_rows"] = pose_rows
            raise AssertionError("16 actual DS/FH arm poses differ")
        geometry = arm_geometry(outputs["arm"])
        for actual, expected_row in zip(geometry, arm_expected["face_rows"]):
            _, face, count, original_cursor, sha256 = expected_row.split()
            if actual != {"face": int(face), "vertices": int(count), "sha256": sha256}:
                report["arm_geometry"] = geometry
                raise AssertionError("actual arm selected batch hash differs")
        pixels = [line.split() for line in outputs["outer"].splitlines() if line.startswith("HAND_PIXELS ")]
        controls = [line for line in outputs["outer"].splitlines() if line.startswith("HAND_CONTROL ")]
        if controls != ["HAND_CONTROL foreign_lookup_preserved",
                        "HAND_CONTROL overlay_dependencies_preserve_alpha"]:
            raise AssertionError("hand pending/failure ownership controls missing")
        if len(pixels) != 1 or len(pixels[0]) != 5 or not screenshot.is_file():
            raise AssertionError("actual arm pixel observation or screenshot missing")
        changed, purple, width, height = map(int, pixels[0][1:])
        if changed < 64 or purple < 64 or width <= 0 or height <= 0:
            raise AssertionError("authored skin did not produce visible arm pixels")
        report.update(status="PASS", rows=rows, original_rows=64,
                      full_rows=full_rows, part_rows=part_rows, pose_rows=pose_rows,
                      arm_geometry=geometry, controls=controls,
                      native_pixels={"changed": changed, "purple": purple,
                                     "width": width, "height": height,
                                     "prepared_equipped_c_d": [1.0, 1.0],
                                     "prepared_swing_W_X": [0.0, 0.0],
                                     "prepared_yaw_pitch_current_previous": [0.0, 0.0, 0.0, 0.0]},
                      screenshot={"path": str(screenshot), "sha256": digest(screenshot)},
                      matrix_absolute_tolerance=2e-6, matrix_maximum_absolute_error=maximum,
                      scope="two genuine App/PX startup and GAME World/Player processes with prepared fields and authored skin; 64 exact original outer outcomes/stack/mode and 32 matrix components within explicit tolerance; four exact full RA state rows; separate genuine DS arm compile/reuse, 16 exact flag poses and six selected batch hashes excluding reserved word7; original intermediate per-face pool cursors are reference-only, native final cursor is compared; CURRENT_COLOR query precedes passive matrices/state like the original; native purple arm pixels and screenshot; pending RA blocks mutation and foreign lookup survives retirement; fire/stone/water overlays reach explicit dependency and preserve alpha disable; null OpenAL playback has no capture device; no complete-frame, audio, ready-download-image upload or original hand-pixel equality")
        print(f"64 original hand matrix profiles, maximum error {maximum}; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["assets_after_sha256"] = inventory(assets)
        report["resources_after_sha256"] = inventory(resources)
        same = (before == report["input_after_sha256"] and assets_before == report["assets_after_sha256"]
                and resources_before == report["resources_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", newline="\n")
        if not same:
            raise AssertionError("hand control inputs or resources changed")


if __name__ == "__main__":
    main()
