"""Capture the entered YC Event component and genuine FU/RQ native custody.

The real bootstrap App.r starts at its untouched FU. A separately constructed
YC receiver is entered explicitly; this is not current-App.r input selection.
Windows moves the same hidden window to monitor2 after bootstrap, before YC.
Linux may use an external hidden X11 display; no monitor2 claim is made there.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location(
        "sign_event_capture", ROOT / "tests/reference/client/controller/capture.py")
    if spec is None or spec.loader is None:
        raise RuntimeError("Native capture support unavailable")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="sign-events-", dir=args.work_dir.resolve()))
    profile = out / "profile"
    profile.mkdir()
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    paths = [Path(__file__).resolve(), executable,
             ROOT / "tests/client/gui/test_source_sign_event_ownership.c",
             ROOT / "cmake/client_sign_event_tests.cmake",
             ROOT / "tests/reference/client/controller/capture.py",
             ROOT / "src/client/gui/source_screen_events.c",
             ROOT / "src/client/gui/source_screen_transitions.c",
             ROOT / "src/client/gui/source_sign_screen.c",
             ROOT / "src/client/gui/source_sign_screen_methods.c",
             ROOT / "src/client/gui/display_screen.c",
             ROOT / "src/client/runtime/menu_loop.c"]

    def pins():
        return {str(path): capture.digest(path) for path in paths}

    def inventory(folder):
        return {path.relative_to(folder).as_posix(): capture.digest(path)
                for path in sorted(folder.rglob("*")) if path.is_file()}

    report = {"status": "FAIL", "scope": __doc__, "inputs_before": pins(),
              "assets_before": inventory(assets), "host": sys.platform,
              "monitor2_scope": "hidden-window after bootstrap, before YC" if os.name == "nt"
                               else "not claimed"}
    environment = os.environ.copy()
    environment.update(APPDATA=str(profile), HOME=str(profile), USERPROFILE=str(profile),
                       XDG_CONFIG_HOME=str(profile / "config"),
                       XDG_CACHE_HOME=str(profile / "cache"))
    try:
        receipt = capture.capture([str(executable), str(assets)], out, "native", 180,
                                  environment=environment, working_directory=out)
        report["capture"] = receipt
        capture.require_success(receipt, empty_stderr=True)
        lines = (out / receipt["stdout_file"]).read_bytes().decode("utf-8", errors="strict").splitlines()
        pattern = (r"PASS: Sign Event component ownership, ([1-9][0-9]*) checks; queued-clicks=4 "
                   r"Done=2 sound-returns=4 Stats-Done=2 fallback-FU=2 RQ=1 cancel=1 "
                   r"checked-BUSY=3 windows-closed=1 current-App-r-selection=0")
        match = re.fullmatch(pattern, lines[-1]) if lines else None
        if match is None or sum(line.startswith("PASS:") for line in lines) != 1:
            raise ValueError("Missing complete Sign Event custody footer")
        report.update(checks=int(match[1]), stdout_lines=len(lines), queued_clicks=4,
                      done_calls=2, sound_returns=4, done_stats_returns=2,
                      fallback_menus=2, world_selection=1, cancel=1,
                      checked_busy=3, windows_closed=True, current_app_r_selection=False)
        report["inputs_after"], report["assets_after"] = pins(), inventory(assets)
        if report["inputs_after"] != report["inputs_before"] or report["assets_after"] != report["assets_before"]:
            raise ValueError("Sign Event selected source/product/assets changed")
        report["status"] = "PASS"
    finally:
        report["owned_profile_files"] = inventory(profile)
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "report": str(out / "report.json"),
                      "checks": report["checks"]}))


if __name__ == "__main__":
    main()
