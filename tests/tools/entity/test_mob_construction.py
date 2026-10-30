"""Native Mob constructor contracts; optional separately labelled original scalar replay."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]
KINDS = ("cn", "uz", "fr", "gb", "uw")
RESOURCES = {"lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
             "lang/stats_US.lang": b"# Authored Mob constructor statistics\n", "achievement/map.txt": b""}
MATH_FIELDS = {"ls.aj", "ls.G", "ls.F", "sn.aS"}
SLIME_FIELDS = {"ls.Y", "sn.bg", "sn.bh", "uw.c"}
# Pinned constructor constants, independent of native output and constructor entropy.
LEAF_LITERALS = {
    "cn": {"sn.bg": {"F": "3fb33333"}, "sn.bh": {"F": "3f666666"},
           "ls.aB": {"F": "3f4ccccd"}, "ls.Y": 20, "gz.c": 2},
    "uz": {"sn.bg": {"F": "3f19999a"}, "sn.bh": {"F": "3fe66666"},
           "ls.aB": {"F": "3f000000"}, "ls.Y": 20, "gz.c": 5},
    "fr": {"sn.bg": {"F": "3f19999a"}, "sn.bh": {"F": "3fe66666"},
           "ls.aB": {"F": "3f333333"}, "ls.Y": 20, "gz.c": 2},
    "gb": {"sn.bg": {"F": "3f19999a"}, "sn.bh": {"F": "3fe66666"},
           "ls.aB": {"F": "3f333333"}, "ls.Y": 20, "gz.c": 2, "gb.a": 0, "gb.b": 0},
    "uw": {"ls.aB": {"F": "3f333333"}, "uw.a": {"F": "00000000"},
           "uw.b": {"F": "00000000"}},
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fbits(value):
    return {"F": struct.pack(">f", value).hex()}


def canonical(value):
    if isinstance(value, dict):
        if "class" in value:
            if "string" in value:
                text = value["string"].encode("utf-16-be", "surrogatepass")
                return {"utf16": list(struct.unpack(">" + "H" * (len(text) // 2), text))}
            return {"nonnull": True}
        return {key: canonical(cell) for key, cell in value.items()}
    if isinstance(value, list):
        return [canonical(cell) for cell in value]
    return value


def expected_bounds(width, height):
    def d(value):
        return {"D": struct.pack(">d", value).hex()}
    return {"eq.a": d(-width / 2), "eq.b": d(0), "eq.c": d(-width / 2),
            "eq.d": d(width / 2), "eq.e": d(height), "eq.f": d(width / 2)}


def floating(value):
    return struct.unpack(">f", bytes.fromhex(value["F"]))[0]


def native_invariants(rows):
    if len(rows) != 15:
        raise AssertionError("15 actual Mob constructor rows required")
    for index, row in enumerate(rows):
        kind = KINDS[index % 5]
        if (row["case"], row["kind"], row["pass"], row["remote"], row["null_world"], row["same_world"]) != (
                index, kind, index // 5, index // 5 == 1, index // 5 == 2, True):
            raise AssertionError("Mob input/identity order differs")
        if row["entity_counter_after"] != row["entity_counter_before"] + 1:
            raise AssertionError("actual Entity counter increment differs")
        fields = row["fields"]
        leaf = dict(LEAF_LITERALS[kind])
        if kind != "uw":
            leaf.update({"ii.a": None, "ii.d": None, "ii.e": False})
        for key, expected in leaf.items():
            if fields[key] != expected:
                raise AssertionError(f"native {kind} pinned leaf field {key} differs")
        for key, expected in {"sn.aE": {"D": "3ff0000000000000"}, "sn.aF": True,
                              "sn.bd": True, "sn.c": True, "sn.bu": 1, "sn.bw": 300,
                              "sn.bz": 300, "sn.bp": fbits(.5), "ls.E": 20, "ls.ai": -1,
                              "ls.N": True, "ls.P": True, "ls.S": fbits(1)}.items():
            if fields[key] != expected:
                raise AssertionError(f"native {kind} source literal {key} differs")
        if fields["sn.aI"] != (None if index // 5 == 2 else {"nonnull": True}):
            raise AssertionError("actual nullable World identity differs")
        texture = {"cn": "spider", "uz": "zombie", "fr": "skeleton", "gb": "creeper", "uw": "slime"}[kind]
        if fields["ls.O"] != {"utf16": list(map(ord, f"/mob/{texture}.png"))}:
            raise AssertionError("actual texture literal differs")
        records = row["watcher"]["records"]
        if any(record["type"] != 0 or not record["dirty"] for record in records):
            raise AssertionError("actual watcher Byte construction record differs")
        if records[0] != {"index": 0, "type": 0, "value": 0, "dirty": True}:
            raise AssertionError("actual SN early watcher0 differs")
        if kind == "uw":
            size = next(record["value"] for record in records if record["index"] == 16)
            if size not in (1, 2, 4) or fields["ls.Y"] != size * size or not 10 <= fields["uw.c"] <= 29:
                raise AssertionError("actual Slime size/health/jump fields differ")
            width = floating(fbits(floating(fbits(.6)) * size))
            if fields["sn.bg"] != fbits(width) or fields["sn.bh"] != fbits(width):
                raise AssertionError("actual Slime size fields differ")
            if row["bounds"] != expected_bounds(width, width) or row["watcher"]["dirty"] != (size != 1):
                raise AssertionError("actual Slime bounds/watcher update differs")
        else:
            if row["bounds"] != expected_bounds(floating(fbits(.6)), floating(fbits(1.8))):
                raise AssertionError("actual inherited bounds were repaired after a size-only store")
            if kind == "gb":
                if [record["value"] for record in records] != [0, -1, 0] or row["watcher"]["dirty"]:
                    raise AssertionError("actual early Creeper watcher records differ")
            elif len(records) != 1 or row["watcher"]["dirty"]:
                raise AssertionError("actual default watcher shape differs")
        if row["private_random"]["cached"] or row["private_random"]["gaussian"] != "0000000000000000":
            raise AssertionError("Mob own Random Gaussian state changed")
        if kind == "fr" and (row["skeleton_stack"] != {"iz.a": 1, "iz.b": 0, "iz.c": 261, "iz.d": 0}
                             or not row["skeleton_stack_alias"] or not row["skeleton_stack_same"]):
            raise AssertionError("actual shared Skeleton Bow Stack differs")


def compare_original(rows, original):
    debugger = original["debugger_rows"]
    normal = original["normal_rows"]
    entropy = original["entropy_profiles"]
    if len(debugger) != 15 or len(normal) != 15 or len(entropy) != 15:
        raise AssertionError("15 paired original constructor/entropy profiles required")
    direct = replayed = 0
    for native, observed, ordinary, profile in zip(rows, debugger, normal, entropy):
        mask, multiplier = (1 << 48) - 1, 0x5DEECE66D
        if ((int(profile["u"], 16) ^ int(profile["nano"], 16) ^ multiplier) & mask) != int(profile["private_seed_before"], 16):
            raise AssertionError("observed constructor U/nano pair does not initialize its observed private seed")
        math_state = int(observed["math_seed_before"], 16)
        math_draws = []
        for _ in range(4):
            math_state = (math_state * multiplier + 11) & mask
            high = math_state >> 22
            math_state = (math_state * multiplier + 11) & mask
            low = math_state >> 21
            math_draws.append(struct.pack(">d", ((high << 27) + low) * (2.0 ** -53)).hex())
        if (math_draws != profile["math_draws"] or math_draws != observed["draws"]
                or math_state != int(observed["math_seed_after"], 16)):
            raise AssertionError("observed original constructor does not consume exactly four shared Math doubles")
        kind = native["kind"]
        excluded = MATH_FIELDS | (SLIME_FIELDS if kind == "uw" else set())
        for source in (observed, ordinary):
            fields = canonical(source["fields"])
            if set(native["fields"]) != set(fields):
                raise AssertionError(f"complete source field shape differs at case {native['case']}")
            for key in fields.keys() - excluded:
                if native["fields"][key] != fields[key]:
                    raise AssertionError(f"actual Mob literal/independent field differs case {native['case']}: {key}")
                direct += 1
            if kind != "uw" and (native["bounds"] != source["bounds"] or native["watcher"] != source["watcher"]):
                raise AssertionError("actual original independent bounds/watchers differ")
        replay = native["formula_replay"]
        for key in MATH_FIELDS:
            if replay[key] != observed["fields"][key]:
                raise AssertionError(f"labelled original Math scalar replay differs: {key}")
            replayed += 1
        if kind == "uw":
            for key in SLIME_FIELDS:
                if replay[key] != observed["fields"][key]:
                    raise AssertionError(f"labelled original Slime own-RNG replay differs: {key}")
            if replay["bounds"] != observed["bounds"] or replay["private_seed_after"] != profile["private_seed_after"]:
                raise AssertionError("labelled original Slime final Random/bounds replay differs")
            size = replay["size"]
            record = next(record for record in observed["watcher"]["records"] if record["index"] == 16)
            if record["value"] != size or observed["watcher"]["dirty"] != (size != 1):
                raise AssertionError("original Slime watcher replay differs")
            replayed += 7
        elif native["watcher"] != observed["watcher"]:
            raise AssertionError("actual independent watcher records differ")
        if observed["private_random"]["seed48"] != profile["private_seed_after"]:
            raise AssertionError("original private Random method-exit evidence differs")
    return {"direct_independent_field_comparisons": direct, "separately_labelled_scalar_checks": replayed,
            "source_checkpoint_count": len(original["checkpoints"]),
            "scope": "Actual native constructor entropy is not equated to an independent JVM. Read-only original Math returns and pre-Slime seed drive separate scalar replay only. Original U/nano evidence remains in the observed file and is not rebound after World setup."}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="mob-construction-", dir=args.work_dir.resolve()))
    resources = out / "resources"
    for name, data in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    world = out / "world"
    world.mkdir()
    original = json.loads(args.observations.read_text()) if args.observations else None
    if original:
        inputs = [(profile["math_draws"], profile["private_seed_before"]) for profile in original["entropy_profiles"]]
    else:
        inputs = [(["0000000000000000", "3fd0000000000000", "3fe0000000000000", "3fe8000000000000"], "000123456789")] * 15
    tape = out / "scalar-replay-inputs.txt"
    tape.write_text("".join(" ".join([*draws, seed]) + "\n" for draws, seed in inputs), encoding="ascii")
    executable = args.executable.resolve(strict=True)
    paths = [executable, Path(__file__), tape, *[resources / name for name in RESOURCES]]
    if args.observations:
        paths.append(args.observations.resolve(strict=True))
    before = {str(path.resolve()): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before, "original_comparison": None}
    try:
        actual = out / "native.ndjson"
        command = [str(executable), str(resources), str(world), str(tape), str(actual)]
        result = subprocess.run(command, cwd=out, stdin=subprocess.DEVNULL, capture_output=True, timeout=110)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        report["command"] = command
        lines = result.stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"Mob construction: ([1-9][0-9]*) checks; 15 actual constructors and owned failure prefixes", lines[-1]) if lines else None
        if result.returncode or result.stderr or footer is None or any(line not in ("151 recipes", "16 achievements") for line in lines[:-1]):
            raise AssertionError(f"Mob native failure ({result.returncode}); {result.stderr.decode(errors='replace')}; {out}")
        rows = [json.loads(line) for line in actual.read_text().splitlines()]
        native_invariants(rows)
        if original:
            report["original_comparison"] = compare_original(rows, original)
        report.update(status="PASS", checks=int(footer.group(1)), constructors=15,
                      stdout_banners=lines[:-1], native_result=str(actual),
                      scope="genuine shared-profile GAME World/Registry/Entity owners; five hostile constructors under local/remote/null World; actual four-Math-draw/native private-constructor progression; declared fresh null-World Slime clock profiles cover all three sizes and exact own nextInt order; all-five early allocation and Math failures, early virtual watcher failures, strict hold retirement, Skeleton static Stack/class initialization and failure ownership; optional original comparison uses separately labelled scalar replay, not injected constructor entropy; no Mob AI/ticking/rendering claim")
        print(f"Mob constructors: {footer.group(1)} checks; {'original scalar comparison' if original else 'authored native contracts'}; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in paths}
        unchanged = before == report["input_after_sha256"]
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n")
        if not unchanged:
            raise AssertionError("Mob inputs changed")


if __name__ == "__main__":
    main()
