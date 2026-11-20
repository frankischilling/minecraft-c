"""Exercise real cloud draws with authored textures and unchanged-original batch observations."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "tests/reference/client/clouds"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}


def authored_assets(root):
    path = Path(__file__).with_name("test_startup.py")
    spec = importlib.util.spec_from_file_location("cloud_authored_startup", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.resources(root)
    white = lambda x, y: (255, 255, 255, 255)
    for name, size in (("pack.png", 16), ("terrain.png", 256), ("gui/items.png", 256),
                       ("misc/dial.png", 16), ("misc/grasscolor.png", 256),
                       ("misc/foliagecolor.png", 256), ("misc/watercolor.png", 256),
                       ("environment/clouds.png", 32)):
        target = root / "jar" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(module.authored_png(size, size, white))
    for edge in (16, 32):
        target = root / "sdk" / f"lwjgl-icon-{edge}.rgba"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(bytes((255, 255, 255, 255)) * (edge * edge))


def geometry(text):
    rows = []
    for line in text.splitlines():
        if not line.startswith("CLOUD_WORDS "):
            continue
        cells = line.split()
        if len(cells) not in (4, 5):
            raise AssertionError("cloud word row framing differs")
        index, width, count = map(int, cells[1:4])
        words = cells[4] if len(cells) == 5 else ""
        if index != len(rows) or width not in (6, 7) or len(words) != width * count * 8:
            raise AssertionError("cloud selected source batch shape differs")
        if re.fullmatch("[0-9a-f]*", words) is None:
            raise AssertionError("cloud selected word encoding differs")
        rows.append(f"CLOUD_GEOMETRY {index} {width} {count} " +
                    hashlib.sha256(bytes.fromhex(words)).hexdigest())
    if len(rows) != 72:
        raise AssertionError("72 final cloud batch rows required")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, default=ROOT / "tests/client/startup/fixtures")
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    resources = args.resources.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="clouds-device-", dir=args.work_dir.resolve()))
    assets = out / "assets"
    if args.assets is None:
        authored_assets(assets)
    else:
        shutil.copytree(args.assets.resolve(strict=True), assets)
        # The observed texture is deliberately authored, on a disposable copy.
        spec = importlib.util.spec_from_file_location("cloud_authored_png", Path(__file__).with_name("test_startup.py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        target = assets / "jar/environment/clouds.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(module.authored_png(32, 32, lambda x, y: (255, 255, 255, 255)))
    resource_before = inventory(resources)
    assets_before = inventory(assets)
    inputs = [executable, Path(__file__), Path(__file__).with_name("test_startup.py")]
    reference_path = REFERENCE / "draw.json"
    if reference_path.exists():
        inputs.append(reference_path)
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before,
              "resource_before_sha256": resource_before, "assets_before_sha256": assets_before,
              "audio_backend": "OpenAL Soft null playback device; capture is unavailable"}
    try:
        environment = os.environ.copy()
        # Graphics controls still initialize a real OpenAL device. The null
        # backend makes that prerequisite independent of desktop audio sessions.
        environment["ALSOFT_DRIVERS"] = "null"
        for name in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
            path = out / name.lower()
            path.mkdir()
            environment[name] = str(path)
        result = subprocess.run([str(executable), str(resources), str(assets)], cwd=out,
                                env=environment, capture_output=True, text=True, timeout=230)
        (out / "native.stdout.txt").write_text(result.stdout, newline="\n")
        (out / "native.stderr.txt").write_text(result.stderr, newline="\n")
        # Selecting only the real null playback driver deliberately leaves no
        # capture backend. Preserve and admit that exact library diagnostic;
        # all other stderr, nonzero exits and missing assertions remain failures.
        expected_diagnostic = "[ALSOFT] (WW) No capture backend available!\n"
        report["expected_diagnostics"] = result.stderr
        if result.returncode != 0 or result.stderr not in ("", expected_diagnostic):
            raise AssertionError(f"cloud native control failed ({result.returncode}): {result.stderr}; {out}")
        if re.fullmatch(r"Source clouds device: [1-9][0-9]* checks; 72 states and final source batches",
                        result.stdout.splitlines()[-1]) is None:
            raise AssertionError("cloud native assertion footer missing")
        states = [line for line in result.stdout.splitlines() if line.startswith("CLOUD_STATE ")]
        batches = geometry(result.stdout)
        if len(states) != 72 or any(len(row.split()) != 41 for row in states):
            raise AssertionError("cloud post-call state row count/shape differs")
        report.update(state_rows=states, geometry_rows=batches)
        if not reference_path.exists():
            raise AssertionError("original cloud observations are not available")
        expected = json.loads(reference_path.read_text())
        if states != expected["state_rows"] or batches != expected["geometry_rows"]:
            (out / "actual.json").write_text(json.dumps({"state_rows": states, "geometry_rows": batches}, indent=2) + "\n", newline="\n")
            raise AssertionError(f"cloud original/native observations differ; {out / 'actual.json'}")
        report.update(status="PASS", original_state_rows=72, original_final_batches=72,
                      scope="prepared World/camera operands on genuine startup/N owners with real OpenAL Soft null-device initialization; full flat/fancy calls, explicit original-ordered CURRENT_COLOR query before matrix/state inspection and final NW batch words; no audio, complete-frame or whole-method pixel equality")
        print(result.stdout.splitlines()[-1])
        if result.stderr:
            print(result.stderr, end="")
        print(f"Cloud observations: 72 state and 72 final batch comparisons; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        report["resource_after_sha256"] = inventory(resources)
        report["assets_after_sha256"] = inventory(assets)
        if before != report["input_after_sha256"] or resource_before != report["resource_after_sha256"] or assets_before != report["assets_after_sha256"]:
            report["status"] = "FAIL"
            raise AssertionError("cloud native inputs or resources changed")
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", newline="\n")


if __name__ == "__main__":
    main()
