"""Actual SDK unarmed attack against a published Sheep reached by normal picking."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
sys.dont_write_bytecode = True


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    args = parser.parse_args()
    require(os.name == "nt", "This standalone APPDATA isolation profile requires Windows")
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="unarmed-attack-", dir=args.artifacts.resolve()))
    mining = module("attack_saved_inputs", ROOT / "tests/tools/client/test_mining_loop.py")
    selected = module("attack_selection_inputs", ROOT / "tests/tools/client/test_block_selection.py")
    world = module("attack_world_inputs", ROOT / "tests/tools/client/test_world_selection_loop.py")
    autosave = module("attack_autosave_audit", ROOT / "tests/tools/client/attack_autosave.py")
    inputs = [executable, Path(__file__).resolve(), ROOT / "tests/client/gameplay/test_attack.c",
              ROOT / "tests/client/gameplay/test_mining.c",
              ROOT / "tests/client/gameplay/test_attack_autosave.inc",
              ROOT / "tests/tools/client/attack_autosave.py",
              ROOT / "tests/tools/world/test_source_saving.py",
              ROOT / "cmake/player_attack.cmake",
              ROOT / "tests/client/startup/test_world_selection_loop.c",
              ROOT / "tests/client/startup/test_world_reload.inc"]
    inputs.extend(path for folder in ("src", "include") for path in (ROOT / folder).rglob("*")
                  if path.is_file() and path.suffix in (".c", ".h", ".inc"))
    before_inputs = {str(path): digest(path) for path in inputs}
    before_assets = world.snapshot(assets)
    report = {"status": "FAIL", "scope": __doc__, "inputs_before": before_inputs,
              "assets_before": before_assets,
              "authored_actor": {"class": "Sheep", "health": 1, "score": 7, "color": 0,
                                 "placement": "two blocks along actual Player look before publication"},
              "limitations": "Finite native SDK scenario; no original graphical/session parity claim"}
    try:
        with tempfile.TemporaryDirectory(prefix="attack-appdata-", dir=evidence) as temporary:
            home = Path(temporary)
            game = home / ".minecraft"
            world.repeat_statistics(game, assets)
            (game / "options.txt").write_bytes(b"viewDistance:3\n")
            saves = game / "saves"
            saves.mkdir()
            for folder, name, played in (("older", "Old world", 1303000000000),
                                         ("bravo", "", 1304000000000),
                                         ("alpha", "Attack world", 1304000000000)):
                directory = saves / folder
                directory.mkdir()
                metadata = (mining.mining_metadata(selected, world, name) if folder == "alpha"
                            else world.metadata(name, played, 19132))
                (directory / ("level.dat_old" if folder == "bravo" else "level.dat")).write_bytes(
                    gzip.compress(metadata, mtime=0))
                (directory / "preserve-chunk.dat").write_bytes(b"authored untouched attack world")
            regions = saves / "alpha/region"
            regions.mkdir()
            selected.saved_regions(world, regions, {"id": 3, "metadata": 0})
            before = world.save_title_files(saves, evidence / "before")
            environment = os.environ.copy()
            observations = evidence / "autosaves"
            observations.mkdir()
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS="null", MC_ATTACK_SAVES=str(saves),
                               MC_ATTACK_AUTOSAVES=str(observations))
            result = world.retained_native_run([str(executable), str(assets), str(evidence)],
                environment, evidence, saves, before, "unarmed-attack", time.time_ns() // 1000000,
                360)
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            require(result.returncode == 0, "Native SDK Player attack executable failed")
            require(result.stdout.splitlines().count(
                "ATTACK_PICK actual-source-ray=1 same-published-sheep=1 hitresult-untouched=1") == 1,
                "Actual source picking receipt")
            operands = [line for line in result.stdout.splitlines() if line.startswith("ATTACK_OPERAND ")]
            require(len(operands) == 1, "One actual death-sound Player operand observation")
            sample = re.fullmatch(r"ATTACK_OPERAND y-velocity-bits=([0-9a-f]{16}) expected-damage=([12]) "
                r"actual-damage=([12]) falling-bonus=([01]) sample-count=1 active-call=1", operands[0])
            require(sample is not None, "Exact actual Player velocity and entered-call receipt")
            velocity = struct.unpack(">d", bytes.fromhex(sample[1]))[0]
            damage = 1 + int(velocity < 0.0)
            require(int(sample[2]) == int(sample[3]) == damage and int(sample[4]) == int(velocity < 0.0),
                    "Original negative-Y bonus derived independently from sampled binary64 operand")
            attack = [line for line in result.stdout.splitlines() if line.startswith("ATTACK_INPUT ")]
            require(len(attack) == 1 and re.fullmatch(
                r"ATTACK_INPUT sdk-left=1 attempts=([1-9]|1[0-2]) mouse-progress=1 swing=1 "
                r"health-nonpositive=1 dying=1 score-added=7 mob-kills=1 damage-dealt=" + str(damage) + r" wool-items=1 "
                r"returned-world-hud=3", attack[0]) is not None, "Actual complete SDK attack receipt")
            death = [line for line in result.stdout.splitlines() if line.startswith("ATTACK_DEATH ")]
            require(len(death) == 1 and re.fullmatch(
                r"ATTACK_DEATH age=21 particles=20 distinct-gy=20 actual-world-remove=1 normal-timer=1",
                death[0]) is not None,
                "Complete original twenty-particle death loop and ordinary World removal")
            cleanup = [line for line in result.stdout.splitlines() if line.startswith("ATTACK_FINISH ")]
            require(len(cleanup) == 1 and re.fullmatch(
                r"ATTACK_FINISH cleanup=1 windows-absent=1 checks=[1-9][0-9]*", cleanup[0]) is not None,
                "Checked complete native attack cleanup")
            after = world.save_title_files(saves, evidence / "after-verified")
            report["autosaves"] = autosave.audit(evidence, result.stdout)
            require(all((evidence / name).is_file() for name in ("attack-target.png", "attack-returned.png")),
                    "Actual before/after frame captures")
            report["receipt"] = attack[0]
            report["operand"] = operands[0]
            report["death"] = death[0]
            report["cleanup"] = cleanup[0]
            report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["inputs_after"] = {str(path): digest(path) for path in inputs}
        report["assets_after"] = world.snapshot(assets)
        if report["inputs_after"] != before_inputs or report["assets_after"] != before_assets:
            report["status"] = "FAIL"
            report["continuity_error"] = "Source/product/resources changed during run"
        (evidence / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    require(report["status"] == "PASS", "Player attack execution input continuity")
    print("Actual SDK Player attack PASS: " + str(evidence))


if __name__ == "__main__":
    main()
