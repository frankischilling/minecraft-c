"""Execute authored LivingEntity overlays through genuine renderer/model owners."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--original", type=Path,
                        help="Explicit frozen original seven-profile packet")
    parser.add_argument("--device-original", type=Path,
                        help="Separate matching-device ordinary current-normal observations")
    parser.add_argument("--build-receipt", type=Path,
                        help="Actual successful current target build and executable output record")
    args = parser.parse_args()
    if args.original and not args.build_receipt:
        parser.error("original comparison requires an actual build/output receipt")
    if args.device_original and not args.original:
        parser.error("matching-device observations require the primary original packet")
    helper = Path(__file__).with_name("test_creature_draw.py")
    spec = importlib.util.spec_from_file_location("living_overlay_authored", helper)
    authored = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(authored)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="living-overlay-", dir=args.work_dir.resolve()))
    assets, world = out / "assets", out / "world"
    authored.authored_assets(assets)
    authored.authored_world(world)
    executable = args.executable.resolve(strict=True)
    paths = {executable, Path(__file__).resolve(), helper}
    for directory in (ROOT / "src", ROOT / "include", ROOT / "tests/client/startup/fixtures",
                      assets, world):
        paths.update(p for p in directory.rglob("*") if p.is_file())
    paths.update((ROOT / "tests/client/render").glob("*.c"))
    packet = ROOT / "tests/reference/client/living_overlay"
    paths.update(p for p in packet.iterdir() if p.is_file())
    before = {str(p): digest(p) for p in paths}
    report = {"status": "FAIL", "scope": "Authored native renderer integration; no original comparison",
              "input_before_sha256": before}
    comparison, admitted, compiled = None, None, None
    try:
        if args.original or args.build_receipt:
            spec = importlib.util.spec_from_file_location("living_overlay_comparison", packet / "comparison.py")
            comparison = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(comparison)
        if args.original:
            admitted = comparison.admit_original(args.original, args.device_original)
            report["original_admitted"] = True
        if args.build_receipt:
            compiled = comparison.admit_build(args.build_receipt, executable)
            report["build_negative_controls"] = comparison.build.negatives(args.build_receipt, executable)
        if admitted:
            comparison.recheck(admitted, compiled)
        elif compiled:
            comparison.build.recheck(compiled)
        environment = os.environ.copy()
        environment["ALSOFT_DRIVERS"] = "null"
        for name in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
            directory = out / name.lower()
            directory.mkdir()
            environment[name] = str(directory)
        command = [str(executable), str(ROOT / "tests/client/startup/fixtures"), str(assets),
                   str(world), str(out)]
        result = subprocess.run(command, cwd=out, env=environment, capture_output=True, timeout=300)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        report.update(command=command, exit=result.returncode)
        if result.returncode or result.stderr:
            raise AssertionError(f"native failure {result.returncode}: {result.stderr.decode(errors='replace')}")
        match = re.search(rb"Living overlays: ([1-9][0-9]*) checks;7 genuine profiles;3 native controls\r?\n$", result.stdout)
        if match is None or result.stdout.decode("ascii").splitlines()[:-1] != ["151 recipes", "16 achievements"]:
            raise AssertionError("exact native setup and completion; no undeclared output")
        rows = [json.loads(line) for line in (out / "overlay.ndjson").read_text().splitlines()]
        textures, calls, controls = rows[:6], rows[6:-3], rows[-3:]
        if [r.get("event") for r in textures] != ["texture"] * 6 or [r.get("index") for r in textures] != list(range(6)):
            raise AssertionError("complete authored texture setup")
        if len(calls) != 35 or [r.get("case", r.get("index")) for r in calls] != [i for i in range(7) for _ in range(5)]:
            raise AssertionError("complete seven-profile transport")
        if [r.get("event") for r in calls] != ["state", "overlay", "overlay", "draw", "state"] * 7:
            raise AssertionError("actual per-profile observation order")
        if [r.get("name") for r in controls] != ["zero_alpha_white_rgb", "visible_hurt_pixels", "floating_prefix"]:
            raise AssertionError("all native controls returned")
        if admitted:
            report["comparison"] = comparison.compare_native(out / "overlay.ndjson", admitted, compiled)
            report["comparator_negative_controls"] = comparison.negatives(rows, admitted)
            report["scope"] = "Executed native target with actual build/output binding and selected original model/state comparison; pixel parity excluded"
        report.update(status="PASS", checks=int(match[1]), profiles=7, records=len(rows))
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        try:
            if admitted:
                comparison.recheck(admitted, compiled)
            elif compiled:
                comparison.build.recheck(compiled)
        except Exception as error:
            report.update(status="FAIL", final_audit_error=str(error))
        after = {str(p): digest(p) for p in paths}
        report.update(input_after_sha256=after, inputs_unchanged=before == after)
        if before != after:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    if report["status"] != "PASS":
        raise AssertionError("native inputs changed")
    print(f"Living overlays: {report['checks']} native checks; {out / 'run.json'}")


if __name__ == "__main__":
    main()
