"""Authored real post-draw contracts; Original packets remain separate inputs to comparison only."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = next(p for p in Path(__file__).resolve().parents if (p / "CMakeLists.txt").is_file())
TARGET = "mc_test_burning_billboard_draw"
REFERENCE = ROOT / "tests/reference/client/burning_billboard"
LAYERS = [2, 2, 2, 5, 0, 3, 4]
PROFILES = [("cn", "3fb33333", "3f666666"), ("uw", "3f19999a", "3f19999a"),
            ("xt", "3f733333", "3f733333"), ("gb", "3f19999a", "3fe66666"),
            ("cn", "3fb33333", "00000000"), ("gb", "3f800000", "3fb33333"),
            ("gb", "3f800000", "40066666")]
CONTROL = {
    "work_zero": (197, False, False, 0, 0, 0, 0, True),
    "work_vertex": (358, False, False, 0, 1, 1, 8, True),
    "null_bounds": (136, False, True, 0, 0, 0, 0, False),
    "preexisting_batch": (187, False, True, 0, 0, 1, 8, True),
    "fp_after_bind": (125, False, False, 0, 0, 0, 0, False),
    "draw_buffer_null": (450, False, True, 2, 8, 12, 96, False),
    "nan_height": (-1, True, False, 0, 0, 0, 0, False),
    "infinite_height_limit": (332, False, False, 1, 4, 6, 48, True),
}


def need(value, message):
    if not value:
        raise AssertionError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atlas_bytes():
    return bytes(channel for y in range(256) for x in range(256)
                 for channel in ((x % 16) * 17, (y % 16) * 17,
                                 (x // 16 + y // 16 * 16) & 255,
                                 255 if (x + y) % 4 else 0))


def geometry_path():
    # The private candidate is inspectable before root integration. Native
    # execution still uses only the selected compiled executable and assets.
    private = Path(__file__).resolve().parents[2] / "reference/client/burning_billboard/authored-geometry.json"
    return private if private.is_file() else REFERENCE / "authored-geometry.json"


def finite_geometry():
    data = json.loads(geometry_path().read_text())
    need(data["status"] == "AUTHORED_ARITHMETIC_CONTROLS_PASS" and
         data["expected_layer_counts"] == LAYERS and len(data["profiles"]) == 7,
         "explicit authored binary32 table, never an Original observation")
    return data["profiles"]


def common(rows):
    need(len(rows) >= 7 and [r.get("event") for r in rows[:6]] == ["texture"] * 6 and
         [r.get("index") for r in rows[:6]] == list(range(6)), "complete six-texture setup")
    device = rows[6]
    need(device.get("event") == "device" and
         all(type(device.get(k)) is str and device[k] for k in ("vendor", "renderer", "version")) and
         device["framebuffer"]["alpha"] == 0 and device["framebuffer"]["depth"] > 0,
         "actual device and SDK-default framebuffer observations")


def authored_rows(rows):
    common(rows)
    expected = finite_geometry()
    cursor = 7
    for case, count in enumerate(LAYERS):
        need(rows[cursor].get("event") == "input" and rows[cursor].get("case") == case,
             "ordered actual caller inputs")
        caller = rows[cursor]
        need((caller["actor"], caller["width"], caller["height"]) == PROFILES[case] and
             caller["bv"] == 20 and caller["view_yaw"] == "42870000" and
             caller["y"] == caller["min_y"], "declared timer, live TH yaw and bounds offset")
        need(rows[cursor + 1].get("event") == "state" and rows[cursor + 1].get("case") == case and
             rows[cursor + 1].get("phase") == "before", "actual before-state")
        before = rows[cursor + 1]
        cursor += 2
        batches = []
        while cursor < len(rows) and rows[cursor].get("event") == "batch":
            b = rows[cursor]
            need(b.get("case") == case and b.get("ordinal") == len(batches), "ordered actual NW batches")
            need(type(b["words"]) is list and len(b["words"]) == b["nw"]["o"] and
                 all(type(w) is str and re.fullmatch(r"[0-9a-f]{8}", w) for w in b["words"]),
                 "complete synchronous raw NW word copy")
            batches.append(b)
            cursor += 1
        terrain, fire, after = rows[cursor:cursor + 3]
        need([r.get("event") for r in (terrain, fire, after)] == ["terrain", "fire", "state"] and
             all(r.get("case") == case for r in (terrain, fire, after)) and
             after.get("phase") == "after", "complete actual post-body observation order")
        need(terrain["keys"] == ["%clamp%/misc/shadow.png", "/terrain.png"] and
             terrain["calls"] == 2 and (terrain["wrap_s"], terrain["wrap_t"], terrain["min"], terrain["mag"]) ==
             (10497, 10497, 9728, 9728), "actual default shadow then terrain lookup/bind")
        if case == 0:
            need(terrain["transfer_hex"] == atlas_bytes().hex(), "actual decoded full authored atlas transfer")
        else:
            need("transfer_hex" not in terrain, "only the reached first atlas upload is claimed")
        need((fire["layers"], fire["vertices"], fire["loop_tests"], fire["texture"]) ==
             (count, 4 * count, count + 1, 31), "actual finite layer/vertex/draw receipts")
        for key in ("scale", "initial_height", "remaining", "z_offset"):
            need(fire[key] == expected[case][key], "finite authored binary32 " + key)
        need(fire["initial_y_offset"] == "00000000" and fire["view_yaw"] == "42870000",
             "actual bounds operand and fresh manager yaw")
        fire_batches = [b for b in batches if b["fire"]]
        need(len(fire_batches) == bool(count), "zero height reaches NWdraw without fabricating GL vertices")
        if count:
            b = fire_batches[0]
            need((b["mode"], b["first"], b["count"]) == (4, 0, count * 6) and
                 (b["nw"]["h"], b["nw"]["o"], b["nw"]["p"], b["nw"]["r"]) ==
                 (count * 6, count * 48, count * 4, 7), "actual NW quad expansion and real GL triangles")
            need(not b["nw"]["w"] and not b["nw"]["l"] and b["nw"]["m"] and
                 not b["nw"]["n"] and not b["nw"]["q"] and not b["lighting"] and
                 b["texture_enabled"] and b["alpha"] and b["matrix_depth"] == 2 and
                 b["texture"] == terrain["name"] and b["color"] == ["3f800000"] * 4 and
                 b["nw"]["translation"] == ["0000000000000000"] * 3,
                 "actual fire batch attributes, bound atlas and matrix prefix")
            active = [b["words"][i:i + 5] for i in range(0, len(b["words"]), 8)]
            golden = [layer["vertices_f32"][i] for layer in expected[case]["layers"]
                      for i in (0, 1, 2, 0, 2, 3)]
            need(active == golden, "actual packed positions/UV/order and both atlas-row/U cycles")
        need(fire["gl"]["modelview_depth"] == fire["gl"]["projection_depth"] == 1 and
             next(c["enabled"] for c in fire["gl"]["caps"] if c["cap"] == 2896),
             "source pop and unconditional lighting enable returned")
        need(before["entropy"] == after["entropy"],
             "actual source/random streams do not advance during post-body")
        cursor += 3
    need(cursor == len(rows), "no missing/extra native transport records")
    return {"profiles": 7, "source_vertices": sum(LAYERS) * 4,
            "scope": "Authored finite contract; Original and pixel parity are separate"}


def control_rows(rows, name):
    common(rows)
    need(len(rows) == 8 and rows[-1].get("event") == "control" and rows[-1].get("name") == name,
         "one complete fresh-process native control")
    row = rows[-1]
    pc, returned, source, layers, vertices, h, o, writing = CONTROL[name]
    need((row["pc"], row["returned"], row["source_exception"], row["layers"], row["vertices"],
          row["nw_h"], row["nw_o"], row["nw_w"]) ==
         (pc, returned, source, layers, vertices, h, o, writing), "exact reached failure prefix: " + name)
    need(row["native_policy"] == (not returned and not source) and
         row["matrix_depth"] == (1 if returned else 2) and row["lighting"] == returned and
         row["checked_retired"] is True and row["prefix_preserved"] is True and
         len(row["raw_words"]) == o and all(re.fullmatch(r"[0-9a-f]{8}", w) for w in row["raw_words"]),
         "classification, synchronous prefix bytes and checked ownership retirement")
    return name


def negatives(rows):
    rejected = []
    variants = []
    for label, event, key, value in (
        ("missing_layers", "fire", "layers", 1),
        ("wrong_scale", "fire", "scale", "00000000"),
        ("wrong_depth_offset", "fire", "z_offset", "00000000"),
        ("camera_yaw", "input", "view_yaw", "43340000"),
        ("atlas_upload", "terrain", "transfer_hex", "00"),
    ):
        bad = copy.deepcopy(rows)
        next(r for r in bad if r.get("event") == event)[key] = value
        variants.append((label, bad))
    for label, offset in (("first_U_direction", 3), ("atlas_row", 4), ("fourth_vertex_order", 40)):
        bad = copy.deepcopy(rows)
        batch = next(r for r in bad if r.get("event") == "batch" and r["fire"])
        batch["words"][offset] = "00000000"
        variants.append((label, bad))
    bad = copy.deepcopy(rows)
    bad.remove(next(r for r in bad if r.get("event") == "batch" and r["fire"]))
    variants.append(("missing_actual_draw", bad))
    bad = copy.deepcopy(rows)
    final = next(r for r in bad if r.get("event") == "fire")["gl"]
    next(c for c in final["caps"] if c["cap"] == 2896)["enabled"] = False
    variants.append(("fake_final_lighting", bad))
    for label, bad in variants:
        try:
            authored_rows(bad)
        except (AssertionError, KeyError, ValueError, IndexError, StopIteration):
            rejected.append(label)
        else:
            raise AssertionError("authored transport accepted copied corruption: " + label)
    return rejected


def local_path(name):
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", name):
        return Path(name[5].upper() + ":/" + name[7:])
    if os.name != "nt" and re.match(r"^[A-Za-z]:[\\/]", name):
        return Path("/mnt") / name[0].lower() / name[3:].replace("\\", "/")
    return Path(name)


def admit_build(path, executable):
    """Use the existing actual-build validator with this target's required inputs."""
    build = module("burning_billboard_actual_build", ROOT / "tests/reference/client/living_overlay/build.py")
    build.TARGET = TARGET
    build.REQUIRED = {
        "src/client/render/living_draw/fire.c", "src/client/render/living_draw/shadow.c",
        "src/client/render/living_draw/common.c", "src/client/render/living_draw/internal.h",
        "src/client/render/living_draw_internal.h", "src/client/render_engine/nw.c",
        "src/client/display/display.c", "src/client/display/source_surface.inc",
        "src/client/display/fog_surface.inc", "src/client/display/camera_surface.inc",
        "tests/client/render/test_burning_billboard_draw.c", "tests/client/render/test_creature_draw.c",
        "tests/client/startup/test_application_startup.c", "cmake/client_mob_draw.cmake",
    }
    admitted = build.admit(path, executable)
    rejected = []
    original = json.loads(path.read_text())
    for label in ("failed_build", "missing_output", "missing_fire_body", "wrong_target"):
        bad = copy.deepcopy(original)
        if label == "failed_build":
            bad["phases"][0]["exit"] = 1
        elif label == "missing_output":
            bad["output_sha256"] = {}
        elif label == "missing_fire_body":
            for key in ("input_before_sha256", "input_after_sha256"):
                bad[key] = {name: pin for name, pin in bad[key].items()
                            if local_path(name).resolve() != (ROOT / "src/client/render/living_draw/fire.c").resolve()}
        else:
            for phase in bad["phases"]:
                phase["command"] = [word.replace(TARGET, TARGET + "_absent") for word in phase["command"]]
        try:
            build.admit(path, executable, bad)
        except AssertionError:
            rejected.append(label)
        else:
            raise AssertionError("build/output validator accepted corruption: " + label)
    return build, admitted, rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--build-receipt", type=Path)
    parser.add_argument("--original", type=Path)
    parser.add_argument("--device-original", type=Path)
    parser.add_argument("--pixel-original", type=Path,
                        help="Explicit finite matching-device Windows ordinary image packet")
    args = parser.parse_args()
    if args.original and not args.build_receipt:
        parser.error("Original comparison requires actual successful current target build/output binding")
    if args.device_original and not args.original:
        parser.error("matching-device observations require the primary Original packet")
    if args.pixel_original and not args.build_receipt:
        parser.error("image comparison requires actual successful current target build/output binding")
    authored = module("burning_billboard_authored", ROOT / "tests/tools/client/test_creature_draw.py")
    png = module("burning_billboard_png", ROOT / "tests/tools/client/test_startup.py").authored_png
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="burning-billboard-", dir=args.work_dir.resolve()))
    assets = out / "assets"
    authored.authored_assets(assets)
    (assets / "terrain.png").write_bytes(png(256, 256,
        lambda x, y: ((x % 16) * 17, (y % 16) * 17, (x // 16 + y // 16 * 16) & 255,
                      255 if (x + y) % 4 else 0)))
    for name in ("ordinary", *CONTROL):
        authored.authored_world(out / name / "world")
    executable = args.executable.resolve(strict=True)
    paths = {executable, Path(__file__).resolve(), geometry_path()}
    for directory in (ROOT / "src", ROOT / "include", ROOT / "tests/client/startup/fixtures",
                      ROOT / "cmake", ROOT / "tests/reference/client/burning_billboard", assets):
        if directory.is_dir():
            paths.update(p for p in directory.rglob("*") if p.is_file())
    paths.update((ROOT / "tests/client/render").glob("*.c"))
    paths.update((ROOT / "tests/tools/client").glob("*.py"))
    paths.update((ROOT / "tests/reference/client/living_overlay").glob("*.py"))
    paths.update(p for name in ("ordinary", *CONTROL) for p in (out / name / "world").rglob("*") if p.is_file())
    before = {str(p.resolve()): digest(p) for p in sorted(paths)}
    report = {"status": "FAIL", "scope": "Authored Native post-body contracts; no Original comparison",
              "input_before_sha256": before, "runs": []}
    build = compiled = comparison = admitted = None
    pixels = pixel_admitted = None
    try:
        if args.build_receipt:
            build, compiled, rejected = admit_build(args.build_receipt.resolve(strict=True), executable)
            report["build_negative_controls"] = rejected
            report["actual_build_output_binding"] = {
                key: str(compiled[key]) for key in
                ("receipt", "receipt_sha256", "executable", "executable_sha256",
                 "source_input_count", "output_scope")}
        if args.original:
            comparison = module("burning_billboard_comparison", REFERENCE / "comparison.py")
            admitted = comparison.admit_original(args.original, args.device_original)
            comparison.recheck(admitted, compiled)
        if args.pixel_original:
            pixels = module("burning_billboard_pixels", REFERENCE / "pixel_comparison.py")
            pixel_admitted = pixels.admit_windows(args.pixel_original)
            report["pixel_original_admitted_before_execution"] = True
        if compiled:
            build.recheck(compiled)
        environment = os.environ.copy()
        environment["ALSOFT_DRIVERS"] = "null"
        for name in ("ordinary", *CONTROL):
            run = out / name
            env = environment.copy()
            for variable in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
                directory = run / variable.lower()
                directory.mkdir()
                env[variable] = str(directory)
            command = [str(executable), str(ROOT / "tests/client/startup/fixtures"), str(assets),
                       str(run / "world"), str(run)]
            if name != "ordinary":
                command.append(name)
            result = subprocess.run(command, cwd=run, env=env, capture_output=True, timeout=300)
            (run / "native.stdout").write_bytes(result.stdout)
            (run / "native.stderr").write_bytes(result.stderr)
            report["runs"].append({"name": name, "command": command, "exit": result.returncode})
            need(result.returncode == 0 and not result.stderr,
                 f"actual Native {name} failed {result.returncode}: {result.stderr!r}")
            pattern = (rb"Burning billboards: ([1-9][0-9]*) checks;7 genuine post-draw profiles;checked terminal cleanup"
                       if name == "ordinary" else
                       rb"Burning billboard control " + name.encode("ascii") + rb": ([1-9][0-9]*) checks;checked terminal cleanup")
            lines = result.stdout.decode("ascii").splitlines()
            need(lines[:2] == ["151 recipes", "16 achievements"] and len(lines) == 3,
                 "exact genuine setup output; no undeclared source/native output")
            match = re.fullmatch(pattern, lines[-1].encode("ascii"))
            need(match is not None, "actual checked terminal completion")
            rows = [json.loads(line) for line in (run / "billboard.ndjson").read_text().splitlines()]
            report["runs"][-1]["checks"] = int(match[1])
            if name == "ordinary":
                report["authored"] = authored_rows(rows)
                report["transport_negative_controls"] = negatives(rows)
                pixel_sizes = {p.name: p.stat().st_size for p in run.glob("*.rgba")}
                need(pixel_sizes == {prefix + str(i) + ".rgba": 256 * 256 * 4
                                     for i in range(7) for prefix in ("baseline", "draw")},
                     "complete real before/after framebuffer captures, no pixel parity claim")
                if admitted:
                    report["comparison"] = comparison.compare_native(run / "billboard.ndjson", admitted, compiled)
                    report["original_comparator_negative_controls"] = comparison.negatives(rows, admitted)
                    report["scope"] = "Actual build-bound Native execution and finite Original comparison; pixel parity excluded"
                if pixel_admitted:
                    report["pixel_comparison"] = pixels.compare_native(run, pixel_admitted)
                    report["pixel_negative_controls"] = pixels.negatives(run, pixel_admitted)
                    report["scope"] = "Actual build-bound Native execution and finite Windows prepared RGBA comparison; no primary Mesa live-state comparison"
            else:
                report["runs"][-1]["control"] = control_rows(rows, name)
        report["status"] = "PASS"
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        try:
            if admitted:
                comparison.recheck(admitted, compiled)
            if compiled:
                build.recheck(compiled)
            if pixel_admitted:
                pixels.recheck(pixel_admitted)
        except Exception as error:
            report.update(status="FAIL", final_audit_error=str(error))
        after = {str(p.resolve()): digest(p) for p in sorted(paths)}
        report.update(input_after_sha256=after, inputs_unchanged=before == after)
        if before != after:
            report["status"] = "FAIL"
        report["artifact_sha256"] = {str(p.relative_to(out)): digest(p) for p in out.rglob("*")
                                     if p.is_file() and p.suffix in (".ndjson", ".rgba", ".stdout", ".stderr")}
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    need(report["status"] == "PASS", "final input/build/original audit")
    print(f"Burning billboards: seven Native profiles; eight fresh controls; {out / 'run.json'}")


if __name__ == "__main__":
    main()
