"""Qualify real Display/JI legacy clamp sampling with authored finite inputs.

Optional legacy observations are a separately labelled real SDK edge probe,
not an original game/render parity claim. Bare CI requires no original JAR.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
COORDINATES = (-2, -.25, 0, .125, .25, .5, .75, .875, 1, 1.25, 3)
TEXELS = (((240, 32, 16, 255), (16, 224, 48, 192)),
          ((32, 48, 240, 128), (208, 176, 64, 64)))
REPEAT, CLAMP, NEAREST, LINEAR = 10497, 10496, 9728, 9729


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(root.rglob("*")) if p.is_file()}


def png():
    def chunk(kind, raw):
        return (struct.pack(">I", len(raw)) + kind + raw
                + struct.pack(">I", zlib.crc32(kind + raw)))
    raw = b"".join(b"\0" + bytes(channel for pixel in row for channel in pixel) for row in TEXELS)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def axis(value, wrap, linear, wrong):
    if wrap == REPEAT:
        value -= math.floor(value)
    elif wrong != "border":
        value = min(1, max(0, value))
    if not linear:
        return [(min(1, max(0, math.floor(value * 2))), 1)]
    low = math.floor(value * 2 - .5)
    fraction = value * 2 - .5 - low
    return [(low, 1 - fraction), (low + 1, fraction)]


def expected(s, t, wrap_s, wrap_t, linear, border, wrong=None, gradient=0):
    # Independent GL 1.1/2.1 texel-selection/interpolation equations. This is
    # deliberately not the production core sampler lowering or its GLSL.
    if wrong == "vertex" and gradient:
        if wrap_s == CLAMP:
            s = (min(1, max(0, s - 4)) + min(1, max(0, s + 4))) * .5
        if wrap_t == CLAMP:
            t = (min(1, max(0, t - 4)) + min(1, max(0, t + 4))) * .5
    result = [0.0] * 4
    for x, wx in axis(s, wrap_s, linear, wrong):
        for y, wy in axis(t, wrap_t, linear, wrong):
            xx = x % 2 if wrap_s == REPEAT else min(1, max(0, x)) if wrong == "edge" else x
            yy = y % 2 if wrap_t == REPEAT else min(1, max(0, y)) if wrong == "edge" else y
            value = TEXELS[yy][xx] if 0 <= xx < 2 and 0 <= yy < 2 else border
            for i in range(4):
                result[i] += wx * wy * value[i]
    return result


def check_rows(rows, wrong=None):
    if len(rows) != 3872:
        raise AssertionError("all 3872 independent S/T/filter/border/gradient samples required")
    maximum = 0.0
    for index, cells in enumerate(rows):
        if len(cells) != 10 or cells[0] != "CLAMP_PIXEL" or int(cells[1]) != index:
            raise AssertionError("complete ordered clamp sample record required")
        filter_, border, s, t, gradient, x, y = map(int, cells[2:9])
        if (filter_ not in (NEAREST, LINEAR) or border not in (0, 1)
                or s not in (REPEAT, CLAMP) or t not in (REPEAT, CLAMP)
                or gradient not in (0, 1) or not 0 <= x < 11 or not 0 <= y < 11
                or re.fullmatch(r"[0-9a-f]{8}", cells[9]) is None):
            raise AssertionError("finite clamp sample operands/encoding differ")
        expected_index = (((((filter_ == LINEAR) * 2 + border) * 2 + (s == CLAMP)) * 2
                           + (t == CLAMP)) * 2 + gradient) * 121 + y * 11 + x
        if expected_index != index:
            raise AssertionError("sample operands differ from authored finite matrix")
        actual = bytes.fromhex(cells[9])
        predicted = expected(COORDINATES[x], COORDINATES[y], s, t, filter_ == LINEAR,
                             (0, 0, 0, 0) if border == 0 else (63.75, 95.625, 127.5, 159.375),
                             wrong, gradient)
        error = max(abs(a - b) for a, b in zip(actual, predicted))
        if error > 1 or (filter_ == NEAREST and error != 0):
            raise AssertionError(f"clamp formula sample {index}: {list(actual)} != {predicted}")
        maximum = max(maximum, error)
    return maximum


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--legacy-observations", type=Path,
                        help="Explicit private real LWJGL2 edge-probe legacy.txt, not game parity")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="legacy-clamp-", dir=args.work_dir.resolve()))
    assets, game = out / "resources", out / "game"
    for name in ("pack.png", "gui/background.png", "misc/shadow.png"):
        path = assets / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(png())
    (assets / "font.txt").write_text("Authored non-image resource\n", encoding="utf-8", newline="\n")
    (game / "texturepacks").mkdir(parents=True)
    (game / "options.txt").write_text("skin:Default\n", encoding="utf-8", newline="\n")
    inputs = [executable, Path(__file__), ROOT / "tests/client/display/test_legacy_clamp.c",
              ROOT / "tests/client/render_engine/test_texture_lookup.c"]
    for name in ("include", "src"):
        inputs.extend(p for p in (ROOT / name).rglob("*") if p.is_file())
    if args.legacy_observations is not None:
        inputs.append(args.legacy_observations.resolve(strict=True))
    before = {str(p.resolve()): digest(p) for p in sorted(set(inputs))}
    assets_before = inventory(assets)
    report = {"status": "FAIL", "scope": "Production Display/JI/NW/GE authored edge/filter and ownership controls; original game parity is not claimed.",
              "input_before_sha256": before, "assets_before_sha256": assets_before}
    try:
        command = [str(executable), str(assets), str(game)]
        if os.name != "nt" and not os.environ.get("DISPLAY"):
            launcher = shutil.which("xvfb-run")
            if launcher is None:
                raise AssertionError("a real X11 Display or xvfb-run is required")
            command = [launcher, "-a", "-s", "-screen 0 640x480x24 -noreset", *command]
        report["command"] = command
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, timeout=240)
        (out / "native.stdout.txt").write_text(result.stdout, encoding="utf-8", newline="\n")
        (out / "native.stderr.txt").write_text(result.stderr, encoding="utf-8", newline="\n")
        report["exit_code"] = result.returncode
        if result.returncode != 0 or result.stderr:
            raise AssertionError(f"native clamp fixture failed: {result.returncode}: {result.stderr}")
        lines = result.stdout.splitlines()
        footer = re.fullmatch(r"Legacy clamp production: ([1-9][0-9]*) checks; 3872 GUI/NW pairs; 5 GE replays; 4 controls", lines[-1])
        if footer is None:
            raise AssertionError("mandatory native assertion footer missing")
        rows = [line.split() for line in lines if line.startswith("CLAMP_PIXEL ")]
        maximum = check_rows(rows)
        negatives = []
        for wrong in ("edge", "border", "vertex"):
            try:
                check_rows(rows, wrong)
            except AssertionError:
                negatives.append(wrong)
            else:
                raise AssertionError(f"incorrect {wrong} sampler passed negative comparison control")
        lists = [line.split() for line in lines if line.startswith("CLAMP_LIST ")]
        if len(lists) != 5 or any(len(row) != 3 or int(row[1]) != i for i, row in enumerate(lists)):
            raise AssertionError("five actual GE replay observations required")
        for i, (filter_, s, t) in enumerate(((NEAREST, REPEAT, REPEAT), (NEAREST, CLAMP, REPEAT),
                                           (LINEAR, CLAMP, CLAMP), (NEAREST, CLAMP, CLAMP),
                                           (LINEAR, REPEAT, REPEAT))):
            prediction = expected(1.25, -.25, s, t, filter_ == LINEAR,
                                  (63.75, 95.625, 127.5, 159.375))
            if max(abs(a - b) for a, b in zip(bytes.fromhex(lists[i][2]), prediction)) > 1:
                raise AssertionError(f"dynamic GE sample differs at replay {i}")
        states = [line for line in lines if line.startswith("CLAMP_STATE ")]
        controls = [line for line in lines if line.startswith("CLAMP_CONTROL ")]
        if len(states) != 16 or len(controls) != 4 or len(set(controls)) != 4:
            raise AssertionError("all source/physical state groups and native controls required")
        report.update(native_checks=int(footer[1]), samples=3872, draw_routes_per_sample=2,
                      formula_channel_checks=15488, maximum_linear_byte_error=maximum,
                      negative_controls=negatives, source_physical_state_rows=states,
                      ge_replay_rows=[" ".join(row) for row in lists], native_controls=controls,
                      device=next(line for line in lines if line.startswith("CLAMP_DEVICE ")))
        if args.legacy_observations is not None:
            legacy = [line.split() for line in args.legacy_observations.read_text().splitlines()
                      if line.startswith("PIXEL ")]
            expected_rows = [["CLAMP_PIXEL", *row[1:]] for row in legacy]
            check_rows(expected_rows)
            if rows != expected_rows:
                raise AssertionError("production samples differ from explicit real SDK observations")
            report["legacy_sdk_comparison"] = {"samples": 3872, "exact_rgba_matches": 3872,
                                                "scope": "Real LWJGL2 edge probe only; no original JI/game parity"}
        report["status"] = "PASS"
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in sorted(set(inputs))}
        report["assets_after_sha256"] = inventory(assets)
        report["inputs_unchanged"] = (report["input_after_sha256"] == before
                                      and report["assets_after_sha256"] == assets_before)
        if not report["inputs_unchanged"]:
            report["status"] = "FAIL"
            report["error"] = "source/executable/helper/authored asset input changed during run"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    if report["status"] != "PASS":
        raise AssertionError(report["error"])
    print(json.dumps({"status": report["status"], "native_checks": report["native_checks"],
                      "samples": report["samples"], "negative_controls": report["negative_controls"],
                      "receipt": str(out / "run.json")}))


if __name__ == "__main__":
    main()
