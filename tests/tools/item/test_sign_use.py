"""Run authored VC prefixes; the real DC editor child is deliberately absent."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Sign control support unavailable")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    capture = module("sign_use_capture", ROOT / "tests/reference/client/controller/capture.py")
    support = module("sign_use_resources", ROOT / "tests/reference/client/player_statistics/support.py")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="sign-prefix-", dir=args.work_dir.resolve()))
    resources, game = out / "resources", out / "game"
    support.resources_at(resources)
    game.mkdir()
    executable = args.executable.resolve(strict=True)
    paths = [Path(__file__).resolve(), executable,
             ROOT / "tests/item/use/sign_controls.c",
             ROOT / "tests/client/statistics/test_player_dispatch.c",
             ROOT / "src/item/use/use.c", ROOT / "src/item/use/sign.inc",
             ROOT / "src/item/use/sign_internal.h", ROOT / "include/mc/item/use/use.h",
             ROOT / "src/world/runtime/registry.c",
             ROOT / "src/world/runtime/tile_entities/tile_entities.c",
             ROOT / "src/world/runtime/tile_entities/source_detailed_internal.h"]
    def pins():
        return {str(path): capture.digest(path) for path in paths}
    def assets():
        return {path.relative_to(resources).as_posix(): capture.digest(path)
                for path in sorted(resources.rglob("*")) if path.is_file()}
    report = {"status": "FAIL", "scope": __doc__, "inputs_before": pins(),
              "resources_before": assets()}
    try:
        receipt = capture.capture([str(executable), str(resources), str(game)], out, "native", 180)
        report["capture"] = receipt
        capture.require_success(receipt, empty_stderr=True)
        stdout = (out / receipt["stdout_file"]).read_bytes().decode("ascii", errors="strict")
        lines = stdout.splitlines()
        if len(lines) != 3 or lines[:2] != ["16 achievements", "151 recipes"]:
            raise ValueError("unexpected Sign control output")
        match = re.fullmatch(r"PASS: 32 authored Sign placement prefixes, ([1-9][0-9]*) actual-owner checks", lines[-1])
        if match is None:
            raise ValueError("missing complete Sign control footer")
        report.update(checks=int(match[1]), profiles=32)
        report["inputs_after"], report["resources_after"] = pins(), assets()
        if report["inputs_before"] != report["inputs_after"] or report["resources_before"] != report["resources_after"]:
            raise ValueError("Sign control input changed")
        report["status"] = "PASS"
    finally:
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "report": str(out / "report.json"),
                      "profiles": report["profiles"], "checks": report["checks"]}))


if __name__ == "__main__":
    main()
