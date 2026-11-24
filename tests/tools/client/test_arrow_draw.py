"""Authored MC/Arrow draw contracts; no Original or rendered-parity admission."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
SUCCESS = ("ordinary", "shake", "replace_singleton", "live_angles", "guard")
CONTROLS = {
    "work_zero": (264, False, 0, 0, 2, True, 0, 0, True),
    "work_vertex": (284, False, 1, 0, 2, True, 1, 8, True),
    "preexisting_batch": (244, True, 0, 0, 2, True, 1, 8, True),
    "null_singleton": (244, True, 0, 0, 2, True, 0, 0, False),
    "draw_buffer_null": (329, True, 4, 0, 2, True, 6, 48, False),
    "null_engine": (11, True, 0, 0, 1, False, 0, 0, False),
    "missing_math": (194, False, 0, 0, 2, True, 0, 0, False),
    "fp_after_bind": (29, False, 0, 0, 2, False, 0, 0, False),
    "guard_post_null": (18, True, 0, 0, 1, False, 0, 0, False),
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


def fbits(value):
    return struct.pack(">f", value).hex()


def pixels(x, y):
    return x * 7, y * 7, (x ^ y) * 7, 255 if (x + y) % 4 else 64


def active_words(quad):
    """Authored geometry, independent of Native receipts and production helpers."""
    if quad == 0:
        vertices = [(-7, -2, -2, 0, 5/32), (-7, -2, 2, 5/32, 5/32),
                    (-7, 2, 2, 5/32, 10/32), (-7, 2, -2, 0, 10/32)]
    elif quad == 1:
        vertices = [(-7, 2, -2, 0, 5/32), (-7, 2, 2, 5/32, 5/32),
                    (-7, -2, 2, 5/32, 10/32), (-7, -2, -2, 0, 10/32)]
    else:
        vertices = [(-8, -2, 0, 0, 0), (8, -2, 0, .5, 0),
                    (8, 2, 0, .5, 5/32), (-8, 2, 0, 0, 5/32)]
    return [[fbits(v[0] + .125), fbits(v[1] - .25), fbits(v[2] + .5),
             fbits(v[3]), fbits(v[4])] for v in (vertices[i] for i in (0, 1, 2, 0, 2, 3))]


def check_rows(rows, name):
    if name == "cpu":
        need(rows == [{"event": "cpu", "bridge_cast_pc": 2, "null_pc": 1,
                       "storage_native": True, "signed_zero_return_pc": 18,
                       "services_reached": False, "reused_error_current": True,
                       "nan_guard_proceeds": True, "guard_short_circuit": True,
                       "nw_owned_local": True, "nw_borrowed_replacement_retired": True,
                       "nw_failed_candidate_rejected": True}],
             "complete genuine-owner CPU controls")
        return
    need(len(rows) >= 3 and rows[0].get("event") == "device" and
         all(type(rows[0].get(key)) is str and rows[0][key]
             for key in ("vendor", "renderer", "version")) and
         rows[1].get("event") == "input" and rows[-1].get("event") == "result",
         "actual device, caller and completion records")
    caller, result = rows[1], rows[-1]
    need(caller["partial"] == "3ec00000" and caller["yaw_argument"] == "7fc00000" and
         caller["shake"] == (5 if name in ("shake", "missing_math") else 0) and
         caller["angles"] == (["00000000", "428c0000", "00000000", "420c0000"]
             if name in ("guard", "guard_post_null") else
             ["41200000", "428c0000", "c0a00000", "420c0000"]),
         "exact authored source caller inputs, ignored yaw operand")
    need(result["name"] == name and result["checked_retired"] is True and
         result["prefix_preserved"] is True, "checked terminal ownership and source prefix")
    middle = rows[2:-1]
    batches = [r for r in middle if r.get("event") == "batch"]
    textures = [r for r in middle if r.get("event") == "texture"]
    need(middle == batches + textures, "bounded ordered complete transport")
    if name in ("guard", "guard_post_null"):
        need(not middle and result["texture_calls"] == 0 and not result["shake_math"],
             "both zero angles return before every draw service")
    else:
        need(len(textures) == 1 and result["texture_calls"] == 1, "one actual inherited BW texture call")
        texture = textures[0]
        need(texture["key"] == "/item/arrows.png" and
             (texture["caller_pc"], texture["lookup_pc"], texture["bind_pc"]) == (22, 11, 14) and
             texture["returned"] == (name != "null_engine"), "actual MC/BW lookup/bind ordering")
        if texture["returned"]:
            need(texture["transfer_hex"] == bytes(c for y in range(32) for x in range(32)
                                                 for c in pixels(x, y)).hex(),
                 "complete decoded authored texture transfer")
    if name in SUCCESS and name != "guard":
        need((result["returned"], result["pc"], result["source_exception"], result["native_policy"],
              result["vertices"], result["draws"], result["depth"], result["rescale"]) ==
             (True, 550, False, False, 24, 6, 1, False) and len(batches) == 6 and
             result["primary_returned"] and result["post_returned"] and
             result["scale"] == "3d666666" and result["pitch"] ==
             ("40200000" if name == "live_angles" else "41200000") and
             result["yaw"] == ("43048000" if name == "live_angles" else "c2660000") and
             result["shake_math"] == (name == "shake") and
             result["normal"] == ["00000000", "00000000", "3d666666"],
             "complete source transforms, shake branch, six quads and tail")
        for index, batch in enumerate(batches):
            need((batch["ordinal"], batch["mode"], batch["first"], batch["count"],
                  batch["h"], batch["o"], batch["p"], batch["r"], batch["depth"]) ==
                 (index, 4, 0, 6, 6, 48, 4, 7, 2) and
                 batch["rescale"] is True and batch["m"] is True and
                 all(batch[key] is False for key in ("w", "l", "n", "q")) and
                 batch["translation"] == ["3fc0000000000000", "bfd0000000000000", "3fe0000000000000"] and
                 batch["texture"] == textures[0]["name"] and
                 batch["color"] == ["3e000000", "3e800000", "3f000000", "3f400000"],
                 "actual NW quad expansion, flags, offsets, current color and texture")
            words = batch["words"]
            need(len(words) == 48 and all(type(w) is str and re.fullmatch(r"[0-9a-f]{8}", w)
                                         for w in words) and
                 [words[i:i+5] for i in range(0, 48, 8)] == active_words(index) and
                 all(words[i+5:i+8] == ["5a5aa5a5"] * 3 for i in range(0, 48, 8)),
                 "real raw vertex/UV order and untouched color/normal/padding words")
            normal = (["3d666666", "00000000", "00000000"] if index == 0 else
                      ["bd666666", "00000000", "00000000"] if index == 1 else
                      ["00000000", "00000000", "3d666666"])
            need(batch["normal"] == normal and len(batch["modelview"]) == 16 and
                 len(batch["projection"]) == 16 and
                 all(re.fullmatch(r"[0-9a-f]{8}", w)
                     for w in batch["modelview"] + batch["projection"]),
                 "current GL normal and complete selected-matrix captures")
    elif name == "guard":
        need(result["returned"] and result["pc"] == 18 and result["primary_returned"] and
             result["post_returned"] and result["depth"] == 1 and not result["rescale"],
             "TH still performs actual BW postdraw after primary early return")
    else:
        pc, source, vertices, draws, depth, rescale, h, o, writing = CONTROLS[name]
        need((result["pc"], result["source_exception"], result["native_policy"], result["vertices"],
              result["draws"], result["depth"], result["rescale"], result["nw_h"],
              result["nw_o"], result["nw_w"]) ==
             (pc, source, not source, vertices, draws, depth, rescale, h, o, writing) and
             result["returned"] is False and not batches and not result["post_returned"] and
             result["primary_returned"] == (name == "guard_post_null"),
             "exact failure classification, reached prefix and postdraw boundary: " + name)


def checked_capture(result, name, run):
    capture = result.capture_report
    need(type(result.returncode) is int and result.returncode == 0 and
         type(result.stdout) is bytes and type(result.stderr) is bytes and result.stderr == b"" and
         type(capture) is dict and
         all(type(capture.get(key)) is int for key in
             ("exit", "timeout_seconds", "limit", "stdout_bytes", "stderr_bytes")) and
         capture["exit"] == 0 and capture["timeout_seconds"] == 90 and capture["limit"] == 262144 and
         capture["stdout_bytes"] == len(result.stdout) and capture["stderr_bytes"] == len(result.stderr) and
         capture.get("pipe_capture_complete") is True and capture.get("reaped") is True and
         capture.get("reader_errors") == {} and capture.get("timed_out") is False and
         capture.get("output_overflow") is False,
         f"native {name} failed; exit={result.returncode}; capture={capture}; "
         f"retained_output={run}; stderr={result.stderr[:65536]!r}")


def checked_stdout(data, name):
    need(type(data) is bytes and data.endswith(b"\n"), "complete native stdout bytes")
    lines = data.decode("ascii", "strict").splitlines()
    bootstrap = ["16 achievements", "151 recipes"] if name == "cpu" else ["151 recipes", "16 achievements"]
    need(len(lines) == 3 and lines[:2] == bootstrap and
         re.fullmatch(r"Arrow draw " + re.escape(name) +
                      r": [1-9][0-9]* checks;checked terminal cleanup", lines[-1]),
         "complete actual checked native completion and quiet streams")


def self_test():
    sample = [{"event": "device", "vendor": "authored", "renderer": "control", "version": "0"},
              {"event": "input", "angles": ["41200000", "428c0000", "c0a00000", "420c0000"],
               "shake": 0, "partial": "3ec00000", "yaw_argument": "7fc00000"}]
    for index in range(6):
        words = [w for v in active_words(index) for w in v + ["5a5aa5a5"] * 3]
        sample.append({"event": "batch", "ordinal": index, "mode": 4, "first": 0, "count": 6,
                       "h": 6, "o": 48, "p": 4, "r": 7, "depth": 2, "rescale": True,
                       "m": True, "w": False, "l": False, "n": False, "q": False,
                       "translation": ["3fc0000000000000", "bfd0000000000000", "3fe0000000000000"],
                       "texture": 1, "color": ["3e000000", "3e800000", "3f000000", "3f400000"],
                       "normal": (["3d666666", "00000000", "00000000"] if index == 0 else
                                  ["bd666666", "00000000", "00000000"] if index == 1 else
                                  ["00000000", "00000000", "3d666666"]),
                       "words": words, "modelview": ["00000000"] * 16,
                       "projection": ["00000000"] * 16})
    sample.append({"event": "texture", "key": "/item/arrows.png", "caller_pc": 22,
                   "lookup_pc": 11, "bind_pc": 14, "returned": True, "name": 1,
                   "transfer_hex": bytes(c for y in range(32) for x in range(32) for c in pixels(x,y)).hex()})
    sample.append({"event": "result", "name": "ordinary", "checked_retired": True,
                   "prefix_preserved": True, "returned": True, "pc": 550,
                   "source_exception": False, "native_policy": False, "vertices": 24,
                   "draws": 6, "depth": 1, "rescale": False, "primary_returned": True,
                   "post_returned": True, "scale": "3d666666", "pitch": "41200000",
                   "yaw": "c2660000", "shake_math": False, "texture_calls": 1,
                   "normal": ["00000000", "00000000", "3d666666"]})
    check_rows(sample, "ordinary")
    mutations = [("winding", 2, "words", ["00000000"] * 48),
                 ("NW normal", 2, "n", True), ("current normal", 2, "normal", ["00000000"] * 3),
                 ("scale", -1, "scale", "3d800000"), ("wrapped yaw", -1, "yaw", "00000000"),
                 ("extra draw", -1, "draws", 7), ("missing pop", -1, "depth", 2),
                 ("wrong texture", -2, "key", "/terrain.png"),
                 ("changed untouched word", 3, "words", sample[3]["words"][:-1] + ["00000000"]),
                 ("unretired", -1, "checked_retired", False),
                 ("lost rescale prefix", 2, "rescale", False)]
    rejected = []
    for label, index, key, value in mutations:
        bad = copy.deepcopy(sample)
        bad[index][key] = value
        try:
            check_rows(bad, "ordinary")
        except AssertionError:
            rejected.append(label)
        else:
            raise AssertionError("accepted authored transport corruption: " + label)
    truncated = copy.deepcopy(sample)
    del truncated[2]
    try:
        check_rows(truncated, "ordinary")
    except AssertionError:
        rejected.append("missing batch")
    else:
        raise AssertionError("accepted missing actual batch")
    for name, (pc, source, vertices, draws, depth, rescale, h, o, writing) in CONTROLS.items():
        control = copy.deepcopy(sample[:2])
        control[1]["shake"] = 5 if name == "missing_math" else 0
        if name == "guard_post_null":
            control[1]["angles"] = ["00000000", "428c0000", "00000000", "420c0000"]
        else:
            texture = copy.deepcopy(sample[-2])
            texture["returned"] = name != "null_engine"
            control.append(texture)
        result = copy.deepcopy(sample[-1])
        result.update(name=name, pc=pc, source_exception=source, native_policy=not source,
                      vertices=vertices, draws=draws, depth=depth, rescale=rescale,
                      nw_h=h, nw_o=o, nw_w=writing, returned=False, post_returned=False,
                      primary_returned=name == "guard_post_null",
                      texture_calls=0 if name == "guard_post_null" else 1)
        control.append(result)
        check_rows(control, name)
        control[-1]["source_exception"] = not source
        try:
            check_rows(control, name)
        except AssertionError:
            rejected.append(name + " classification")
        else:
            raise AssertionError("accepted wrong failure classification: " + name)
    need(len(rejected) == len(mutations) + 1 + len(CONTROLS), "complete pure rejection inventory")
    transport_rejections = 0
    for name in ("cpu", "ordinary"):
        bootstrap = ["16 achievements", "151 recipes"] if name == "cpu" else ["151 recipes", "16 achievements"]
        lines = [*bootstrap, "Arrow draw " + name + ": 53 checks;checked terminal cleanup"]
        valid = ("\n".join(lines) + "\n").encode("ascii")
        checked_stdout(valid, name)
        for invalid in (valid[:-1], valid + b"EXTRA\n", valid.replace(b"53 checks", b"0 checks"),
                        ("\n".join([lines[1], lines[0], lines[2]]) + "\n").encode("ascii")):
            try:
                checked_stdout(invalid, name)
            except AssertionError:
                transport_rejections += 1
            else:
                raise AssertionError("accepted altered native stdout profile: " + name)
    need(transport_rejections == 8, "both exact native bootstrap profiles")
    capture = {"exit": 0, "timeout_seconds": 90, "limit": 262144,
               "stdout_bytes": 3, "stderr_bytes": 0, "pipe_capture_complete": True,
               "reaped": True, "reader_errors": {}, "timed_out": False, "output_overflow": False}
    checked_capture(SimpleNamespace(returncode=0, stdout=b"ok\n", stderr=b"", capture_report=capture),
                    "cpu", "authored-pure-control")
    capture_rejections = 0
    for key, value in (("stderr_bytes", False), ("stderr_bytes", 0.0), ("stdout_bytes", 3.0),
                       ("timeout_seconds", 90.0), ("limit", 262144.0), ("exit", False),
                       ("reader_errors", []), ("returncode", False)):
        bad = copy.deepcopy(capture)
        if key != "returncode":
            bad[key] = value
        try:
            checked_capture(SimpleNamespace(returncode=value if key == "returncode" else 0,
                                           stdout=b"ok\n", stderr=b"", capture_report=bad),
                            "cpu", "authored-pure-control")
        except AssertionError:
            capture_rejections += 1
        else:
            raise AssertionError("accepted malformed native capture: " + key)
    need(capture_rejections == 8, "strict native integer/Boolean/capture fields")
    print(f"Arrow draw pure controls: {len(rejected) + transport_rejections + capture_rejections} rejections; native processes launched=0")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--cpu-only", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        need(args.executable is None and args.work_dir is None and not args.cpu_only,
             "pure controls have no executable/work directory")
        self_test()
        return
    need(args.executable is not None and args.work_dir is not None, "native executable/work directory")
    process = module("arrow_bounded_process", ROOT / "tests/reference/client/statistics/support.py")
    authored = module("arrow_authored_resources", Path(__file__).with_name("test_creature_draw.py"))
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="arrow-draw-", dir=args.work_dir.resolve()))
    assets = output / "assets"
    authored.authored_assets(assets)
    png = module("arrow_authored_png", Path(__file__).with_name("test_startup.py")).authored_png
    (assets / "item").mkdir()
    (assets / "item/arrows.png").write_bytes(png(32, 32, pixels))
    names = ("cpu",) if args.cpu_only else ("cpu", *SUCCESS, *CONTROLS)
    for name in names:
        run = output / name
        run.mkdir()
        if name != "cpu":
            authored.authored_world(run / "world")
    paths = {Path(__file__).resolve(), executable, ROOT / "CMakeLists.txt",
             ROOT / "tests/reference/client/statistics/support.py"}
    for directory in (ROOT / "src", ROOT / "include", ROOT / "cmake", ROOT / "tests/client",
                      ROOT / "tests/tools", assets):
        paths.update(p for p in directory.rglob("*") if p.is_file())
    for name in names:
        if name != "cpu":
            paths.update(p for p in (output / name / "world").rglob("*") if p.is_file())
    for pattern in ("*.dll", "*.so", "*.so.*"):
        paths.update(executable.parent.glob(pattern))
    before = {str(p): digest(p) for p in sorted(paths)}
    report = {"status": "FAIL", "scope": "Authored Native contracts; no Original or rendered parity",
              "input_before_sha256": before, "runs": []}
    try:
        for name in names:
            run = output / name
            environment = os.environ.copy()
            environment["ALSOFT_DRIVERS"] = "null"
            for variable in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
                path = run / variable.lower()
                path.mkdir()
                environment[variable] = str(path)
            resources = str(ROOT / "tests/client/startup/fixtures")
            if name == "cpu":
                command = [str(executable), "--cpu", resources, str(run)]
            else:
                command = [str(executable), resources, str(assets), str(run / "world"), str(run), name]
            receipt = {"name": name, "command": command, "retained_output": str(run)}
            report["runs"].append(receipt)
            result = process.run_bounded(command, run, "native", timeout=90, limit=262144,
                                         env=environment)
            receipt.update(exit=result.returncode, capture=result.capture_report)
            checked_capture(result, name, run)
            checked_stdout(result.stdout, name)
            rows = [json.loads(line) for line in (run / "arrow.ndjson").read_text().splitlines()]
            check_rows(rows, name)
        report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        if hasattr(error, "capture_report"):
            report["runs"][-1]["capture"] = error.capture_report
        print(f"Arrow draw failed; retained report={output / 'report.json'}; error={error}", file=sys.stderr)
        raise
    finally:
        after = {str(p): digest(p) for p in sorted(paths)}
        report.update(input_after_sha256=after, inputs_unchanged=before == after)
        if after != before:
            report["status"] = "FAIL"
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    need(report["status"] == "PASS", "unchanged native execution inputs")
    print(f"Arrow draw authored contracts: {len(report['runs'])} runs; {output / 'report.json'}")


if __name__ == "__main__":
    main()
