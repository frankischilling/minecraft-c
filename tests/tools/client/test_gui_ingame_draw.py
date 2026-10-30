"""Pair unchanged-original UQ state/batches with genuine native startup owners."""
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


def authored_assets(root):
    cloud = Path(__file__).with_name("test_render_global_clouds.py")
    module("hud_startup_assets", cloud).authored_assets(root)
    png = module("hud_authored_png", Path(__file__).with_name("test_startup.py")).authored_png

    def gui(x, y):
        if 0 <= x < 182 and 0 <= y < 22:
            return (32, 180, 64, 255)
        if 0 <= x < 24 and 22 <= y < 44:
            return (64, 96, 224, 255)
        return (0, 0, 0, 0)

    def icons(x, y):
        if x < 16 and y < 16:
            return (255, 255, 255, 255) if x in (7, 8) or y in (7, 8) else (0, 0, 0, 0)
        heart = ("000000000", "011011000", "111111100", "111111100", "011111000",
                 "001110000", "000100000", "000000000", "000000000")
        if y < 9:
            for start in (16, 25, 52, 61, 70, 79):
                local_x = x - start
                if 0 <= local_x < 9 and heart[y][local_x] == "1":
                    if start in (16, 25):
                        return (96, 96, 112, 255)
                    if start in (61, 79) and local_x > 3:
                        return (0, 0, 0, 0)
                    return (228, 48, 64, 255) if start in (52, 61) else (248, 112, 144, 255)
        return (0, 0, 0, 0)

    def vignette(x, y):
        distance = max(abs(x - 127.5), abs(y - 127.5)) / 127.5
        shade = int(180 * distance)
        return (shade, shade, shade, 255)

    for name, pixels in (("gui/gui.png", gui), ("gui/icons.png", icons),
                         ("misc/vignette.png", vignette)):
        target = root / "jar" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(png(256, 256, pixels))


def floating(bits):
    if re.fullmatch(r"[0-9a-f]{8}", bits) is None:
        raise AssertionError("HUD matrix float encoding differs")
    return struct.unpack(">f", bytes.fromhex(bits))[0]


def compare_states(actual, expected):
    if len(actual) != 26 or len(expected) != 26:
        raise AssertionError("26 actual and original HUD state rows required")
    maximum = 0.0
    for index, (native, original) in enumerate(zip(actual, expected)):
        a, b = native.split(), original.split()
        if (len(a) != 63 or len(b) != 63 or int(a[1]) != index
                or a[:12] != b[:12] or a[44:] != b[44:]):
            raise AssertionError(f"HUD state/Random/stack/attributes differ at row {index}")
        for component, (left, right) in enumerate(zip(a[12:44], b[12:44])):
            lvalue, rvalue = floating(left), floating(right)
            delta = abs(lvalue - rvalue)
            if not math.isfinite(lvalue) or not math.isfinite(rvalue) or delta > 2e-6:
                raise AssertionError(f"HUD matrix row {index} component {component}: {left} != {right}")
            maximum = max(maximum, delta)
    return maximum


def geometry(stdout):
    rows = []
    for line in stdout.splitlines():
        if not line.startswith("HUD_WORDS "):
            continue
        cells = line.split()
        if (len(cells) != 5 or int(cells[1]) != len(rows) or cells[2:4] != ["5", "6"]
                or re.fullmatch(r"[0-9a-f]{240}", cells[4]) is None):
            raise AssertionError("actual HUD final six-vertex batch framing differs")
        rows.append(f"HUD_GEOMETRY {cells[1]} 5 6 " +
                    hashlib.sha256(bytes.fromhex(cells[4])).hexdigest())
    if len(rows) != 26:
        raise AssertionError("26 actual HUD final NW payloads required")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="gui-ingame-draw-", dir=args.work_dir.resolve()))
    assets = out / "assets"
    authored_assets(assets)
    world_helper = Path(__file__).with_name("test_picking.py")
    world = out / "world"
    module("hud_authored_world", world_helper).authored_world(world)
    executable = args.executable.resolve(strict=True)
    resources = ROOT / "tests/client/startup/fixtures"
    reference = ROOT / "tests/reference/client/gui_ingame_draw/draw.json"
    expected = json.loads(reference.read_text())
    inputs = [executable, Path(__file__), reference, world_helper,
              Path(__file__).with_name("test_startup.py"),
              Path(__file__).with_name("test_render_global_clouds.py")]
    before = {str(p.resolve()): digest(p) for p in inputs}
    assets_before, resources_before = inventory(assets), inventory(resources)
    report = {"status": "FAIL", "input_before_sha256": before,
              "assets_before_sha256": assets_before, "resources_before_sha256": resources_before,
              "audio_backend": "OpenAL Soft null playback device; capture is unavailable"}
    try:
        environment = os.environ.copy()
        environment["ALSOFT_DRIVERS"] = "null"
        for name in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
            path = out / name.lower()
            path.mkdir()
            environment[name] = str(path)
        screenshot = out / "hud.png"
        command = [str(executable), str(resources), str(assets), str(world), str(screenshot)]
        result = subprocess.run(command, cwd=out, env=environment, capture_output=True,
                                text=True, timeout=160)
        (out / "native.stdout.txt").write_text(result.stdout, newline="\n")
        (out / "native.stderr.txt").write_text(result.stderr, newline="\n")
        report["command"] = command
        report["expected_diagnostics"] = result.stderr
        expected_diagnostic = "[ALSOFT] (WW) No capture backend available!\n"
        if result.returncode != 0 or result.stderr not in ("", expected_diagnostic):
            raise AssertionError(f"native HUD failed ({result.returncode}): {result.stderr}; {out}")
        if not result.stdout.splitlines() or re.fullmatch(
                r"GuiIngame draw: [1-9][0-9]* checks; 26 original profiles and visible default HUD",
                result.stdout.splitlines()[-1]) is None:
            raise AssertionError("native HUD assertion footer missing")
        rows = [line for line in result.stdout.splitlines() if line.startswith("HUD_STATE ")]
        report["state_rows"] = rows
        maximum = compare_states(rows, expected["state_rows"])
        actual_geometry = geometry(result.stdout)
        report["geometry_rows"] = actual_geometry
        if actual_geometry != expected["geometry_rows"]:
            raise AssertionError("actual HUD final NW batch hashes differ")
        input_rows = [line for line in result.stdout.splitlines() if line.startswith("HUD_INPUTS ")]
        report["input_rows"] = input_rows
        if input_rows != expected["input_rows"]:
            raise AssertionError("HUD fancy/actual brightness/World subtraction inputs differ")
        light_rows = [line for line in result.stdout.splitlines() if line.startswith("HUD_LIGHT ")]
        report["light_rows"] = light_rows
        if light_rows != expected["light_rows"]:
            raise AssertionError("HUD in-height bounds/World light observations differ")
        controls = [line for line in result.stdout.splitlines() if line.startswith("HUD_CONTROL ")]
        if controls != ["HUD_CONTROL blend_numeric_pairs", "HUD_CONTROL projection_null_settings_before_gl", "HUD_CONTROL null_app",
                        "HUD_CONTROL foreign_lookup_preserved", "HUD_CONTROL debug_text_dependency_after_hud",
                        "HUD_CONTROL item_dependency_preserves_lighting"]:
            raise AssertionError("HUD failure/ownership controls missing")
        blends = [line.split() for line in result.stdout.splitlines() if line.startswith("HUD_BLEND ")]
        if len(blends) != 2:
            raise AssertionError("two nonblack blend observations required")
        blend_pixels = []
        for index, (cells, expected_rgb) in enumerate(zip(blends, ((38, 51, 38), (204, 153, 102)))):
            if len(cells) != 5 or int(cells[1]) != index:
                raise AssertionError("HUD blend observation framing differs")
            rgb = tuple(map(int, cells[2:]))
            if any(abs(actual - wanted) > 1 for actual, wanted in zip(rgb, expected_rgb)):
                raise AssertionError("HUD blend RGB8 result differs beyond one unit")
            blend_pixels.append({"factors": [0 if index == 0 else 775, 769],
                                 "background_source_rgb": [0.2, 0.4, 0.6],
                                 "foreground_source_rgb": [0.25, 0.5, 0.75] if index == 0 else [1.0, 1.0, 1.0],
                                 "rgb": rgb, "expected": expected_rgb, "absolute_tolerance": 1})
        pixels = [line.split() for line in result.stdout.splitlines() if line.startswith("HUD_NATIVE_PIXELS ")]
        if len(pixels) != 1 or len(pixels[0]) != 6 or not screenshot.is_file():
            raise AssertionError("native HUD pixel receipt or screenshot missing")
        green, red, white, width, height = map(int, pixels[0][1:])
        if green < 1000 or red < 200 or white < 8 or (width, height) != (320, 240):
            raise AssertionError("authored hotbar, hearts and crosshair are not visibly drawn")
        report.update(status="PASS", original_rows=26, matrix_absolute_tolerance=2e-6,
                      matrix_maximum_absolute_error=maximum, controls=controls,
                      native_blend_pixels=blend_pixels,
                      native_pixels={"hotbar_green": green, "hearts_red": red,
                                     "crosshair_white": white, "width": width, "height": height},
                      screenshot={"path": str(screenshot), "sha256": digest(screenshot)},
                      scope="real App/Display320x240 startup, GAME World/LocalPlayer and OS controller; prepared finite source fields; 26 original UQ returns with actual Random seed, matrices/state and final heart NW payloads; GUI integer dimensions differ from exact QQ doubles used for projection; same CURRENT_COLOR query before passive matrices/state; native authored hotbar/crosshair/hearts pixels and screenshot; two actual NW solid quads validate blend factors0/769 and775/769 on nonblack clear RGB within one raw RGB8 unit, with no readback normalization; 12 actual above-height SN brightness fallback vignette profiles and two successful in-height World light vignette profiles; pending ownership/foreign lookup and reached failed source effects preserved; debug text and nonempty item draws remain explicit dependencies; null OpenAL playback has no capture device; no full frame, audio or original pixel equality")
        print(result.stdout.splitlines()[-1])
        if result.stderr:
            print(result.stderr, end="")
        print(f"26 original HUD states and final batch hashes; maximum matrix error {maximum}; {out / 'run.json'}")
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
            raise AssertionError("HUD control inputs or assets changed")


if __name__ == "__main__":
    main()
