"""Run authored AZ producer lifetime or genuine GQ child-projection controls."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PROFILES = {
    # Both fixtures enter Block first (the screen fixture through Water vs1).
    # Its Item/statistics cycle finishes achievements before Crafting prints.
    "producer": ([b"16 achievements", b"151 recipes"],
                 b"Dispenser dirty producer ownership passed: "),
    "screen": ([b"16 achievements", b"151 recipes"],
               b"Dispenser screen diagnostic ownership passed: "),
}


def admit(stdout, stderr, code, profile):
    if (type(stdout) is not bytes or type(stderr) is not bytes or
            type(code) is not int or profile not in PROFILES):
        raise ValueError("Incomplete native ownership transport")
    if code != 0 or stderr:
        raise ValueError("Failed native ownership process or nonempty stderr")
    separator = b"\r\n" if b"\r" in stdout else b"\n"
    if not stdout.endswith(separator):
        raise ValueError("Incomplete native ownership line")
    lines = stdout.split(separator)[:-1]
    if not lines or any(b"\r" in row or b"\n" in row for row in lines):
        raise ValueError("Bare or mixed native ownership separators")
    prefix, footer = PROFILES[profile]
    if lines[:-1] != prefix:
        raise ValueError("Unexpected native ownership bootstrap rows")
    match = re.fullmatch(re.escape(footer) + rb"([1-9][0-9]*) checks", lines[-1])
    if match is None or int(match[1]) > 0xffffffff:
        raise ValueError("Missing complete native ownership footer")
    return int(match[1])


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Ownership fixture support unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--profile", choices=tuple(PROFILES), required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    capture = load("dispenser_dirty_capture", ROOT / "tests/reference/client/controller/capture.py")
    support = load("dispenser_dirty_resources", ROOT / "tests/reference/client/player_statistics/support.py")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=args.profile + "-", dir=args.work_dir.resolve()))
    game, resources = out / "worlds", out / "resources"
    game.mkdir()
    support.resources_at(resources)
    executable = args.executable.resolve(strict=True)
    sources = [Path(__file__).resolve(), executable,
               ROOT / "tests/world/runtime/test_entity_publication.c",
               ROOT / "tests/world/runtime/dispenser/test_dirty.c",
               ROOT / "tests/client/gui/test_source_dispenser_dirty.c",
               ROOT / "tests/client/statistics/test_player_dispatch.c",
               ROOT / "src/inventory/dispenser.c", ROOT / "src/inventory/dispenser_tile.c",
               ROOT / "include/mc/inventory/errors.h", ROOT / "include/mc/inventory/dispenser.h",
               ROOT / "src/client/gui/source_inventory_screen.c",
               ROOT / "src/client/gui/source_screen_transition_receipts.c",
               ROOT / "tests/reference/client/controller/capture.py",
               ROOT / "tests/reference/client/player_statistics/support.py"]

    def pins():
        paths = sources + sorted(p for p in resources.rglob("*") if p.is_file())
        return {str(path): capture.digest(path) for path in paths}

    report = {"status": "FAIL", "scope": __doc__, "profile": args.profile,
              "inputs_before": pins()}
    try:
        command = ([str(executable), "world-dispenser-dirty", str(game).encode("utf-8").hex()]
                   if args.profile == "producer" else [str(executable), str(resources), str(game)])
        receipt = capture.capture(command, out, "native", 120)
        report["capture"] = receipt
        capture.require_success(receipt, empty_stderr=True)
        stdout = (out / receipt["stdout_file"]).read_bytes()
        stderr = (out / receipt["stderr_file"]).read_bytes()
        for name, raw in (("stdout", stdout), ("stderr", stderr)):
            if (len(raw) != receipt[name + "_bytes"] or
                    capture.digest(out / receipt[name + "_file"]) != receipt[name + "_sha256"]):
                raise ValueError("Captured ownership stream changed: " + name)
        report["checks"] = admit(stdout, stderr, receipt["exit"], args.profile)
        report["inputs_after"] = pins()
        if report["inputs_before"] != report["inputs_after"]:
            raise ValueError("Ownership fixture source/product/resource inputs changed")
        report["status"] = "PASS"
    finally:
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "checks": report["checks"],
                      "report": str(out / "report.json")}))


if __name__ == "__main__":
    main()
