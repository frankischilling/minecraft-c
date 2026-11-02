"""Actual SDK Player attack with declared saved Inventory and normal picking."""
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
PROFILES = {
    "unarmed": {"id": 0, "count": 0, "damage": 0, "power": 1, "breaks": False},
    "held-dirt": {"id": 3, "count": 7, "damage": 0, "power": 1, "breaks": False},
    "iron-sword-break": {"id": 267, "count": 1, "damage": 250, "power": 8, "breaks": True},
}


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


def saved_metadata(mining, selected, world, name, profile):
    wire = mining.mining_metadata(selected, world, name)
    if profile["id"] == 0:
        return wire
    empty = world.tag(9, "Inventory", b"\x0a\x00\x00\x00\x00")
    item = (world.tag(1, "Slot", b"\x00") +
            world.tag(2, "id", struct.pack(">h", profile["id"])) +
            world.tag(1, "Count", struct.pack(">b", profile["count"])) +
            world.tag(2, "Damage", struct.pack(">h", profile["damage"])) + b"\x00")
    require(wire.count(empty) == 1, "One authored pre-bootstrap Player Inventory field")
    return wire.replace(empty, world.tag(9, "Inventory", b"\x0a" + struct.pack(">i", 1) + item))


def inventory_fields(player, profile, after):
    expected = []
    if profile["id"] != 0 and not (after and profile["breaks"]):
        expected.append({"Slot": [1, 0], "id": [2, profile["id"]],
                         "Count": [1, profile["count"]], "Damage": [2, profile["damage"]]})
    value = player.get("Inventory")
    require(value is not None and value[0] == 9 and value[1][1] == expected and
            (not value[1][1] or value[1][0] == 10),
            "Exact independently decoded declared Inventory before/after source attack")
    return value


def held_receipts(stdout, name, profile, wool):
    rows = [line for line in stdout.splitlines() if line.startswith("ATTACK_HAND ")]
    require(len(rows) == 2, "Three actual returned pre/post hand frames")
    for stage, line in zip(("before", "after"), rows):
        row = re.fullmatch(r"ATTACK_HAND stage=" + stage + r" profile=" + re.escape(name) +
            r" frames=3 empty=([01]) item=([0-9]+) count=([0-9]+) damage=([0-9]+) "
            r"block=([01]) flat=([01]) icon=([0-9]+) geometry=([01]) "
            r"faces=([0-9]+) vertices=([0-9]+) source=1377 wool-slots=([01])", line)
        require(row is not None, "Exact actual hand source and geometry receipt")
        hand_wool = int(row[11])
        require(hand_wool <= wool["inventory_count"] and (stage == "after" or hand_wool == 0),
                "Hand Inventory contains only the observed source Wool pickup")
        initially_empty = profile["id"] == 0 or (stage == "after" and profile["breaks"])
        empty = initially_empty and not hand_wool
        held_id = 0 if empty else (35 if initially_empty else profile["id"])
        held_count = 0 if empty else (1 if held_id == 35 else profile["count"])
        held_damage = 0 if held_id != 267 else profile["damage"]
        require(tuple(map(int, row.group(1, 2, 3, 4))) ==
                (int(empty), held_id, held_count, held_damage),
                "Actual selected Stack identity/fields or cleared hand route")
        require(int(row[5]) == int(held_id in (3, 35)),
                "Real Dirt/Wool inventory geometry versus Item/empty-arm branch")
        flat = held_id == 267
        require(tuple(map(int, row.group(6, 7, 8))) ==
                (int(flat), 66 if flat else 0, int(flat)),
                "Actual delegated Sword icon and returned RA flat-Item geometry")
        if held_id in (3, 35):
            require((int(row[9]), int(row[10])) == (6, 24), "Actual held Block cube geometry")
        if flat:
            require((int(row[9]), int(row[10])) == (66, 264), "Actual Sword extrusion source geometry")
    items = [line for line in stdout.splitlines() if line.startswith("ATTACK_ITEM ")]
    if profile["id"] == 0:
        require(not items, "Unarmed source path has no held Item hit/use statistic invocation")
        return {"hands": rows, "item": None}
    require(len(items) == 1, "One actual Item used/broken/selection receipt")
    expected = int(profile["breaks"])
    row = re.fullmatch(r"ATTACK_ITEM profile=" + re.escape(name) + r" id=" + str(profile["id"]) +
        r" used=" + str(expected) + r" broken=" + str(expected) +
        r" broken-entry=([01]) selected-clear=" + str(expected) +
        r" final-count=" + str(0 if expected else profile["count"]) +
        r" final-damage=0 clear-receipt=([01])", items[0])
    require(row is not None and (not profile["breaks"] or row[1] == "1"),
            "Actual true/false Item-hit statistic distinction and source slot clear")
    return {"hands": rows, "item": items[0]}


def wool_receipt(stdout, profile):
    rows = [line for line in stdout.splitlines() if line.startswith("ATTACK_WOOL ")]
    require(len(rows) == 1, "One retained actual white-Wool publication/pickup receipt")
    row = re.fullmatch(r"ATTACK_WOOL published=1 id=35 count=1 damage=0 pop-sounds=([01]) "
                       r"inventory-slot=(-1|0|1) inventory-count=([01]) source-count=([01]) "
                       r"removed=([01])", rows[0])
    require(row is not None, "Exact bounded source Wool receipt")
    pops, slot, count, source_count, removed = map(int, row.groups())
    require(count == pops and source_count == 1 - count and
            slot == ((1 if profile["id"] == 3 else 0) if count else -1) and removed <= pops,
            "Single actual pop/insertion conserves the published white Wool")
    return {"receipt": rows[0], "pop_sounds": pops, "inventory_slot": slot,
            "inventory_count": count, "source_count": source_count, "removed": removed}


def inventory_save_rows(stdout, events):
    lines = [line for line in stdout.splitlines() if line.startswith("ATTACK_INVENTORY_SAVE")]
    require(0 < events <= 32 and len(lines) == events, "One raw Inventory receipt per actual save")
    result = []
    for event, line in enumerate(lines):
        row = re.fullmatch(r"ATTACK_INVENTORY_SAVE event=" + str(event) + r" slots=(.{1,1800})", line)
        require(row is not None, "Exact bounded ordered Inventory save receipt")
        slots = []
        if row[1] != "-":
            for raw in row[1].split(","):
                token = re.fullmatch(r"([0-9]+):(-?[0-9]+):(-?[0-9]+):(-?[0-9]+)", raw)
                require(token is not None and len(slots) < 40, "Bounded raw occupied Inventory slots")
                values = list(map(int, token.groups()))
                require(all(str(value) == text for value, text in zip(values, token.groups())) and
                        all(-(1 << 31) <= value < (1 << 31) for value in values),
                        "Canonical actual int32 Inventory fields")
                slot = values[0]
                require((0 <= slot < 36 or 100 <= slot < 104) and
                        (not slots or slots[-1][0] < slot),
                        "All occupied main36/armor4 slots in exact source order")
                slots.append(values)
        result.append(slots)
    return result


def inventory_save_policy(slots, profile, wool, after):
    before = [] if profile["id"] == 0 else [[0, profile["id"], profile["count"], profile["damage"]]]
    base = [] if profile["breaks"] else before
    allowed = [base]
    if profile["breaks"] and not after:
        allowed.append(before)
    if wool is not None and wool["inventory_count"] == 1:
        allowed.append(base + [[1 if profile["id"] == 3 else 0, 35, 1, 0]])
    require(slots in allowed, "Raw save Inventory contains only declared held input and observed Wool")


def inventory_save_value(slots):
    def narrow(value, bits):
        return ((value + (1 << (bits - 1))) % (1 << bits)) - (1 << (bits - 1))
    # Independent source i2b/i2s conversions; no native NBT serializer is used.
    items = [{"Slot": [1, narrow(slot, 8)], "id": [2, narrow(item, 16)],
              "Count": [1, narrow(count, 8)], "Damage": [2, narrow(damage, 16)]}
             for slot, item, count, damage in slots]
    return [9, [10 if items else 1, items]]


def inventory_save_audit(evidence, stdout, decoder, profile, wool, events):
    rows = inventory_save_rows(stdout, events)
    post_death = re.findall(r"^ATTACK_AUTOSAVE_POST_DEATH event=([0-9]+) world-remove=1 "
                           r"player-score=-?[0-9]+$", stdout, re.M)
    require(len(post_death) == 1 and 0 <= int(post_death[0]) < events,
            "One actual post-death Inventory save scope")
    post_death = int(post_death[0])
    report = []
    for event, slots in enumerate(rows):
        inventory_save_policy(slots, profile, wool, event >= post_death)
        expected = inventory_save_value(slots)
        folder = evidence / "autosaves" / f"save-{event}"
        live = decoder.Nbt(decoder.read(folder / "player.nbt")).root()
        saved = decoder.inflate(decoder.read(folder / "after/alpha/level.dat"),
                                31)["Data"][1]["Player"][1]
        require(live.get("Inventory") == expected and saved.get("Inventory") == expected,
                "Both typed Player Inventory records equal that save's actual raw slots")
        report.append({"event": event, "slots": slots, "inventory": expected})
    final = decoder.inflate(decoder.read(evidence / "after-verified/alpha/level.dat"),
                            31)["Data"][1]["Player"][1]
    require(final.get("Inventory") == report[-1]["inventory"],
            "Final saved Inventory equals the last exact raw autosave input")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--profile", choices=PROFILES, default="unarmed")
    args = parser.parse_args()
    profile = PROFILES[args.profile]
    require(os.name == "nt", "This standalone APPDATA isolation profile requires Windows")
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=args.profile + "-attack-", dir=args.artifacts.resolve()))
    mining = module("attack_saved_inputs", ROOT / "tests/tools/client/test_mining_loop.py")
    selected = module("attack_selection_inputs", ROOT / "tests/tools/client/test_block_selection.py")
    world = module("attack_world_inputs", ROOT / "tests/tools/client/test_world_selection_loop.py")
    autosave = module("attack_autosave_audit", ROOT / "tests/tools/client/attack_autosave.py")
    decoder = module("attack_inventory_nbt", ROOT / "tests/tools/world/test_source_saving.py")
    inputs = [executable, Path(__file__).resolve(), ROOT / "tests/client/gameplay/test_attack.c",
              ROOT / "tests/client/gameplay/test_mining.c",
              ROOT / "tests/client/gameplay/test_attack_autosave.inc",
              ROOT / "tests/tools/client/attack_autosave.py",
              ROOT / "tests/tools/world/test_source_saving.py",
              ROOT / "tests/tools/client/test_mining_loop.py",
              ROOT / "tests/tools/client/test_block_selection.py",
              ROOT / "tests/tools/client/test_world_selection_loop.py",
              ROOT / "cmake/player_attack.cmake",
              ROOT / "tests/client/startup/test_world_selection_loop.c",
              ROOT / "tests/client/startup/test_world_reload.inc"]
    inputs.extend(path for folder in ("src", "include") for path in (ROOT / folder).rglob("*")
                  if path.is_file() and path.suffix in (".c", ".h", ".inc"))
    before_inputs = {str(path): digest(path) for path in inputs}
    before_assets = world.snapshot(assets)
    report = {"status": "FAIL", "scope": __doc__, "profile": args.profile,
              "authored_saved_inventory": profile, "inputs_before": before_inputs,
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
                metadata = (saved_metadata(mining, selected, world, name, profile) if folder == "alpha"
                            else world.metadata(name, played, 19132))
                if folder == "alpha":
                    player = decoder.Nbt(metadata).root()["Data"][1]["Player"][1]
                    report["saved_input_inventory"] = inventory_fields(player, profile, False)
                    report["saved_metadata_sha256"] = hashlib.sha256(metadata).hexdigest()
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
            command = [str(executable), str(assets), str(evidence)]
            if args.profile != "unarmed":
                command.append(args.profile)
            result = world.retained_native_run(command, environment, evidence, saves, before,
                args.profile + "-attack", time.time_ns() // 1000000, 360)
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            require(result.returncode == 0, "Native SDK Player attack executable failed")
            require(result.stdout.splitlines().count(
                "ATTACK_PICK actual-source-ray=1 same-published-sheep=1 hitresult-untouched=1") == 1,
                "Actual source picking receipt")
            operands = [line for line in result.stdout.splitlines() if line.startswith("ATTACK_OPERAND ")]
            require(len(operands) == 1, "One actual death-sound Player operand observation")
            sample = re.fullmatch(r"ATTACK_OPERAND y-velocity-bits=([0-9a-f]{16}) expected-damage=([1-9][0-9]*) "
                r"actual-damage=([1-9][0-9]*) falling-bonus=([01]) sample-count=1 active-call=1", operands[0])
            require(sample is not None, "Exact actual Player velocity and entered-call receipt")
            velocity = struct.unpack(">d", bytes.fromhex(sample[1]))[0]
            damage = profile["power"] + int(velocity < 0.0)
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
            report["wool"] = wool_receipt(result.stdout, profile)
            report["held"] = held_receipts(result.stdout, args.profile, profile, report["wool"])
            require(all((evidence / name).is_file() for name in
                    ("attack-hand-before.png", "attack-hand-after.png")),
                    "Actual empty/held/cleared-hand frame captures")
            report["inventory_saves"] = inventory_save_audit(
                evidence, result.stdout, decoder, profile, report.get("wool"), len(report["autosaves"]))
            report["saved_final_inventory"] = report["inventory_saves"][-1]["inventory"]
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
