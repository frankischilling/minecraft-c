"""Finite hidden Windows monitor2 Dispenser cross-owner custody regression.

Each requested node order runs in a fresh genuine bootstrap process. Two real
Players/World/AZ owners and SDK input exercise an entered old GQ receiver; no
ordinary current-App.r input selection, rendered frame or physical input claim.
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
ROOT = Path(__file__).resolve().parents[3]
ORDERS = ("owner-older", "owner-newest")


def admit(stdout, stderr, code, order):
    if (type(stdout) is not bytes or type(stderr) is not bytes or type(code) is not int or
            order not in ORDERS or code != 0 or stderr):
        raise ValueError("Incomplete or failed cross-owner transport")
    separator = b"\r\n" if b"\r" in stdout else b"\n"
    if not stdout.endswith(separator):
        raise ValueError("Incomplete cross-owner stdout")
    rows = stdout.split(separator)[:-1]
    if (len(rows) != 3 or any(b"\r" in row or b"\n" in row for row in rows) or
            rows[:2] != [b"16 achievements", b"151 recipes"]):
        raise ValueError("Unexpected cross-owner bootstrap rows or separators")
    pattern = (rb"Dispenser cross-owner SDK passed: ([1-9][0-9]*) checks; order=" +
               order.encode("ascii") + rb" monitor=2 windows-closed=1")
    match = re.fullmatch(pattern, rows[2])
    if match is None or int(match[1]) > 0xffffffff:
        raise ValueError("Missing exact cross-owner custody footer")
    return int(match[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--order", choices=ORDERS + ("both",), default="both")
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This finite SDK fixture requires Windows monitor2")
    spec = importlib.util.spec_from_file_location(
        "dispenser_cross_capture", ROOT / "tests/reference/client/controller/capture.py")
    if spec is None or spec.loader is None:
        raise RuntimeError("Bounded native capture support unavailable")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    custody_path = ROOT / "tests/tools/client/test_dispenser_gameplay.py"
    custody_spec = importlib.util.spec_from_file_location("dispenser_cross_owned_process", custody_path)
    if custody_spec is None or custody_spec.loader is None:
        raise RuntimeError("Reviewed actual-process custody support unavailable")
    custody = importlib.util.module_from_spec(custody_spec)
    custody_spec.loader.exec_module(custody)
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="cross-owner-", dir=args.work_dir.resolve()))
    sources = [Path(__file__).resolve(), executable,
               ROOT / "tests/client/gui/test_source_dispenser_cross_owner.c",
               ROOT / "src/client/gui/source_screen_transitions.c",
               ROOT / "src/client/gui/source_inventory_screen.c",
               ROOT / "src/client/gui/source_dispenser_screen.c",
               ROOT / "src/client/gui/source_screen_events.c",
               ROOT / "src/client/gui/source_screen_transition_receipts.c",
               ROOT / "src/entity/player/container_interaction.c",
               ROOT / "src/inventory/dispenser.c", ROOT / "src/inventory/dispenser_tile.c",
               ROOT / "tests/reference/client/controller/capture.py", custody_path]

    def pins():
        return {str(path): capture.digest(path) for path in sources}

    def inventory(folder):
        return {path.relative_to(folder).as_posix(): capture.digest(path)
                for path in sorted(folder.rglob("*")) if path.is_file()}

    report = {"status": "FAIL", "scope": __doc__, "host": sys.platform,
              "monitor2_required": True, "inputs_before": pins(),
              "assets_before": inventory(assets), "profiles": {},
              "limits": "Selected source/product brackets and full asset-file inventory. Reviewed owned-handle helper bounds each child at600 seconds plus checked reap; no loaded-library or descendant-process census."}
    try:
        for order in ORDERS if args.order == "both" else (args.order,):
            directory = out / order
            directory.mkdir()
            profile = directory / "profile"
            profile.mkdir()
            environment = os.environ.copy()
            environment.update(APPDATA=str(profile), HOME=str(profile), USERPROFILE=str(profile),
                               XDG_CONFIG_HOME=str(profile / "config"),
                               XDG_CACHE_HOME=str(profile / "cache"))
            item = {"status": "FAIL", "order": order}
            report["profiles"][order] = item
            try:
                item["execution"] = {}
                previous_directory = Path.cwd()
                try:
                    os.chdir(directory)
                    receipt = custody.capture_owned(capture, [str(executable), str(assets), order],
                                                    directory, environment, item["execution"])
                finally:
                    os.chdir(previous_directory)
                item["capture"] = receipt
                custody.natural_custody(item["execution"])
                capture.require_success(receipt, empty_stderr=True)
                streams = {}
                for name in ("stdout", "stderr"):
                    path = directory / receipt[name + "_file"]
                    raw = path.read_bytes()
                    if (len(raw) != receipt[name + "_bytes"] or
                            capture.digest(path) != receipt[name + "_sha256"]):
                        raise ValueError("Cross-owner captured stream changed")
                    streams[name] = raw
                item["checks"] = admit(streams["stdout"], streams["stderr"], receipt["exit"], order)
                item["status"] = "PASS"
            except Exception as error:
                item["failure"] = {"type": type(error).__name__, "message": str(error)}
            finally:
                item["owned_profile_files"] = inventory(profile)
        report["inputs_after"], report["assets_after"] = pins(), inventory(assets)
        if report["inputs_after"] != report["inputs_before"] or report["assets_after"] != report["assets_before"]:
            raise ValueError("Cross-owner source/product/assets changed")
        if not report["profiles"] or any(item["status"] != "PASS" for item in report["profiles"].values()):
            raise ValueError("Cross-owner native regression failed; retained " + str(out))
        report["status"] = "PASS"
    finally:
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "report": str(out / "report.json")}))


if __name__ == "__main__":
    main()
