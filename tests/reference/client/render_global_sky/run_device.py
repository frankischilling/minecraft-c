"""Replay real source sky programs and compare original post-call color/state rows."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
sys.path.insert(0, str(HERE))
from capture_draw import rows


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    ensure(spec is not None and spec.loader is not None, "authored helper module")
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    ensure(os.name == "nt", "native whole-App fixture uses the isolated Windows APPDATA profile")
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="sky-device-", dir=args.work_dir.resolve()))
    # All source asset reads use this disposable copy; caller's supplied inputs
    # remain read-only. The observed sky calls use genuine cache-hit methods.
    assets = out / "assets"
    shutil.copytree(args.assets.resolve(strict=True), assets)
    startup_path = HERE.parents[2] / "tools/client/test_startup.py"
    authored = module("sky_device_authored_png", startup_path)
    for name in ("sun.png", "moon.png"):
        target = assets / "jar/terrain" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(authored.authored_png(1, 1, lambda x, y: (255,255,255,255)))
    resources = args.resources.resolve(strict=True)
    source_before = inventory(resources)
    assets_before = inventory(assets)
    (out / "appdata").mkdir()
    expected = json.loads((HERE / "draw.json").read_text())["rows"]
    ensure(rows("\n".join(expected)) == expected, "original sky state fixture invalid")
    inputs = [executable, HERE / "run_device.py", HERE / "capture_draw.py",
              HERE / "draw.json", HERE / "SkyDrawObservation.java", HERE / "source-pins.json",
              startup_path, SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status":"FAIL", "input_before_sha256":before,
              "resource_before_sha256":source_before,"assets_before_sha256":assets_before}
    try:
        environment = os.environ.copy()
        environment["APPDATA"] = str(out / "appdata")
        receipt = capture([str(executable), str(resources), str(assets)], out, "native", 230,
                          environment, out.resolve())
        report["native"] = receipt
        require_success(receipt, empty_stderr=True)
        text = (out / receipt["stdout_file"]).read_text()
        ensure(re.fullmatch(r"Source sky device: [1-9][0-9]* checks; 48 original states and actual GPU bottom-list records",
                            text.splitlines()[-1]), "native sky device completion missing")
        actual = rows(text)
        report["actual_rows"] = actual
        ensure(actual == expected, "native/original sky post-call color/state rows differ")
        report.update(status="PASS",original_rows=48,
                      scope="real source constructor lists, full sky body, exact post-call color/state rows; actual last lower-list GPU record checked; no pixel/complete-frame claim")
    finally:
        report["input_after_sha256"] = {str(p.resolve()):digest(p) for p in inputs}
        report["resource_after_sha256"] = inventory(resources)
        report["assets_after_sha256"] = inventory(assets)
        same = before == report["input_after_sha256"] and source_before == report["resource_after_sha256"] and assets_before == report["assets_after_sha256"]
        if not same: report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
        ensure(same,"native sky inputs/resources changed")
    print("Source sky device: 48 exact original post-call states; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
