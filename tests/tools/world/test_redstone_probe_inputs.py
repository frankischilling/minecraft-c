"""Native constructor-input controls on authored, disposable redstone Worlds.

Tape rows are declared constructor inputs, not original observations, roles,
seeds or expected World state. Required length comes from actual consumption.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import traceback
import zlib


ROOT = Path(__file__).resolve().parents[3]
PLAN = ROOT / "tests/reference/world/redstone/stairs.plan"
HEADER = b"minecraft-c-random-constructor-inputs-v1\n"
MAX_INPUTS = 65536
INPUT_BYTE_LIMIT = len(HEADER) + 1 + 39 * MAX_INPUTS
MAX_OUTPUT = 64 * 1024 * 1024
MAX_LINE = 8 * 1024 * 1024
MAX_CONSOLE = 65536
PROFILE_OK, PROFILE_INVALID, PROFILE_RANGE = 0, 1, 5
INPUT_NONE, INPUT_EXHAUSTED, INPUT_EXTRA = 0, 1, 3


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def tag(kind, name, payload):
    text = name.encode("ascii")
    return bytes([kind]) + struct.pack(">H", len(text)) + text + payload


def compound(name, members):
    return tag(10, name, b"".join(members) + b"\0")


def byte_array(name, data):
    return tag(7, name, struct.pack(">i", len(data)) + data)


def prepare_world(arena):
    """Author public NBT/McRegion input, without copying a reference World."""
    world = arena / "world"
    region = world / "region"
    region.mkdir(parents=True)
    name = b"redstone-inputs-native"
    data = [tag(4, "RandomSeed", struct.pack(">q", 23)),
            *(tag(3, name, struct.pack(">i", value)) for name, value in
              (("SpawnX", 0), ("SpawnY", 64), ("SpawnZ", 0), ("version", 19132),
               ("rainTime", 0), ("thunderTime", 0))),
            *(tag(4, name, struct.pack(">q", 0)) for name in ("Time", "LastPlayed", "SizeOnDisk")),
            tag(8, "LevelName", struct.pack(">H", len(name)) + name),
            tag(1, "raining", b"\0"), tag(1, "thundering", b"\0")]
    (world / "level.dat").write_bytes(gzip.compress(compound("", [compound("Data", data)]),
                                                    mtime=0))
    chunk = compound("", [compound("Level", [
        tag(3, "xPos", struct.pack(">i", 0)), tag(3, "zPos", struct.pack(">i", 0)),
        tag(4, "LastUpdate", struct.pack(">q", 0)),
        byte_array("Blocks", bytes(32768)), byte_array("Data", bytes(16384)),
        byte_array("SkyLight", b"\xff" * 16384), byte_array("BlockLight", bytes(16384)),
        byte_array("HeightMap", bytes(256)), tag(1, "TerrainPopulated", b"\1"),
        tag(9, "Entities", b"\x0a" + struct.pack(">i", 0)),
        tag(9, "TileEntities", b"\x0a" + struct.pack(">i", 0))])])
    payload = b"\2" + zlib.compress(chunk)
    record = struct.pack(">I", len(payload)) + payload
    sectors = (len(record) + 4095) // 4096
    require(0 < sectors <= 255, "bounded authored region record")
    image = bytearray(8192 + sectors * 4096)
    struct.pack_into(">I", image, 0, (2 << 8) | sectors)
    image[8192:8192 + len(record)] = record
    (region / "r.0.0.mcr").write_bytes(image)
    (world / "session.lock").write_bytes(struct.pack(">q", 137))
    return world


def files(world):
    result = {}
    for path in world.rglob("*"):
        require(not path.is_symlink() and not getattr(path.lstat(), "st_file_attributes", 0) & 0x400,
                "ordinary owned World member")
        if path.is_file():
            require(len(result) < 16 and path.stat().st_size <= 1024 * 1024,
                    "bounded complete World inventory")
            result[path.relative_to(world).as_posix()] = path.read_bytes()
    require(sum(map(len, result.values())) <= 1024 * 1024, "bounded authored World bytes")
    return result


def rows(count):
    value = 8682522807148012
    result = []
    for index in range(count):
        value = value * 1181783497276652981 & ((1 << 64) - 1)
        sample = -(1 << 63) if index % 2 == 0 else (1 << 63) - 1
        result.append(f"{value:016x}\t{sample}\n".encode("ascii"))
    return result


def tape(count):
    return HEADER + b"".join(rows(count))


def invalid_tapes():
    first = rows(1)[0]
    word = first.split(b"\t")[0]
    def sample(value):
        return HEADER + word + b"\t" + value + b"\n"
    return (
        ("bad-header", b"invalid constructor inputs\n" + first),
        ("uppercase-hex", HEADER + b"A" + first[1:]),
        ("nonhex", HEADER + b"g" + first[1:]),
        ("short-hex", HEADER + first[1:]),
        ("long-hex", HEADER + b"0" + first),
        ("separator-space", HEADER + first.replace(b"\t", b" ")),
        ("extra-tab", HEADER + first.replace(b"\t", b"\t\t")),
        ("crlf", HEADER + first.replace(b"\n", b"\r\n")),
        ("leading-zero", sample(b"01")), ("negative-zero", sample(b"-0")),
        ("plus", sample(b"+1")), ("positive-overflow", sample(b"9223372036854775808")),
        ("negative-overflow", sample(b"-9223372036854775809")),
        ("embedded-nul", sample(b"0\0")), ("missing-lf", HEADER + first[:-1]),
        ("blank-row", HEADER + b"\n"), ("excessive-rows", tape(MAX_INPUTS + 1)),
        ("excessive-bytes", HEADER + b"x" * (INPUT_BYTE_LIMIT + 1 - len(HEADER))),
    )


def strict_json(raw):
    def members(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "unique JSON member")
            result[key] = value
        return result
    def invalid(value):
        raise AssertionError("nonfinite JSON: " + value)
    return json.loads(raw, object_pairs_hook=members, parse_constant=invalid)


def records(path):
    require(path.is_file() and path.stat().st_size <= MAX_OUTPUT, "bounded complete native output")
    result = []
    with path.open("rb") as stream:
        while True:
            raw = stream.readline(MAX_LINE + 1)
            if not raw:
                break
            require(len(raw) <= MAX_LINE and raw.endswith(b"\n") and len(result) < 292,
                    "complete bounded native JSONL records")
            result.append(strict_json(raw))
    require(result and result[-1]["type"] == "terminal", "actual terminal retained")
    require(all(row["type"] != "terminal" for row in result[:-1]), "one final terminal")
    return result


class Harness:
    def __init__(self, executable, work, plan):
        self.executable, self.work, self.plan = executable, work, plan
        self.count, self.deadline = 0, time.monotonic() + 180
        self.commands = plan.decode("ascii").splitlines()[1:]
        require(len(self.commands) == 15, "public fifteen-operation stairs input")

    def run(self, side, label, inputs=None, existing=False):
        try:
            return self._run(side, label, inputs, existing)
        except Exception as error:
            raise AssertionError(f"{side}/{label}: {error}") from error

    def _run(self, side, label, inputs, existing):
        remaining = self.deadline - time.monotonic()
        require(remaining > 0, "bounded complete harness deadline")
        arena = self.work / (side + "-" + label)
        arena.mkdir()
        world = prepare_world(arena)
        before = files(world)
        plan = arena / "stairs.plan"
        plan.write_bytes(self.plan)
        output = arena / "output.jsonl"
        protected = b"existing output must remain unchanged\n"
        if existing:
            output.write_bytes(protected)
        command = [str(self.executable), side, str(arena).encode("utf-8").hex(), str(plan), str(output)]
        if inputs is not None:
            path = arena / "constructor-inputs.txt"
            path.write_bytes(inputs)
            command += ["--constructor-inputs", str(path)]
        stdout, stderr = arena / "stdout", arena / "stderr"
        remaining = self.deadline - time.monotonic()
        require(remaining > 0, "bounded deadline before launching native child")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            child = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=out, stderr=err,
                                   timeout=min(20, remaining), check=False)
        require(stdout.stat().st_size <= MAX_CONSOLE and stderr.stat().st_size <= MAX_CONSOLE,
                "bounded complete console streams after direct reap")
        require(plan.read_bytes() == self.plan and
                (inputs is None or path.read_bytes() == inputs), "actual plan/tape preserved")
        after = files(world)
        require(set(before) == set(after) and all(before[name] == after[name]
                for name in before if name != "session.lock"), "complete level/region bytes unchanged")
        errors = stderr.read_bytes().replace(b"\r\n", b"\n")
        if existing:
            require(child.returncode == 1 and output.read_bytes() == protected and before == after,
                    "fresh-output guard preserves existing bytes before any World access")
            require(errors == b"native redstone probe failed: output or admission\n",
                    "output admission failure only; no sanitizer diagnostic")
            require(time.monotonic() < self.deadline, "bounded output-guard case")
            self.count += 1
            return None
        observed = records(output)
        terminal = observed[-1]
        require(terminal["planned"] == 15 and terminal["cleanup"] is True and
                type(terminal["completed"]) is int and 0 <= terminal["completed"] <= 15,
                "actual planned/completed count and successful cleanup")
        failures = [row for row in observed if row["type"] == "failure"]
        if terminal["complete"]:
            require(child.returncode == 0 and terminal["completed"] == 15 and
                    terminal["failure"] is None and not failures and not errors,
                    "actual successful exit and clean terminal")
        else:
            require(child.returncode == 1 and len(failures) == 1 and observed[-2] is failures[0]
                    and failures[0]["stage"] == terminal["failure"], "first failure retained before terminal")
            require(errors == ("native redstone probe failed: " + terminal["failure"] + "\n").encode(),
                    "actual first failure only; no sanitizer diagnostic")
            require(failures[0]["constructor_inputs"] == terminal["constructor_inputs"],
                    "passive input view retained through retirement")
        header = observed[0] if observed[0]["type"] == "header" else None
        states = observed[1:-1] if header else observed[:-1]
        if failures:
            states = states[:-1]
        expected = [("snapshot", "initial", -1)]
        for ordinal in range(15):
            expected += [("snapshot", "before", ordinal), ("result", None, ordinal),
                         ("snapshot", "after", ordinal)]
        actual = [(row["type"], row.get("boundary"), row["ordinal"]) for row in states]
        require(actual == expected[:len(actual)], "complete source-order operation prefix")
        for row in states[1:]:
            require(row["input"] == self.commands[row["ordinal"]], "actual operation input binding")
        if terminal["completed"] == 15:
            require(actual == expected and header is not None and terminal["last_operation_status"] == 0,
                    "all actual initial/before/result/after boundaries reached before completion check")
            require(all(row["native_status"] == 0 for row in states if row["type"] == "result"),
                    "every completed source operation returned native success")
            require(after["session.lock"].hex() == terminal["session_token"], "actual Store session token")
        if header:
            require(header["schema"] == 2 and header["qualified"] is False and header["side"] == side
                    and header["planned"] == 15 and
                    header["constructor_input_mode"] == terminal["constructor_inputs"]["mode"] and
                    header["constructor_input_count"] == terminal["constructor_inputs"]["supplied"],
                    "native header mode/count remains unqualified and joins terminal diagnostics")
        if header is None or failures and failures[0]["stage"] == "constructor input binding":
            require(before == after, "pre-construction rejection leaves the entire World untouched")
        self.count += 1
        require(time.monotonic() < self.deadline, "bounded full native-control run")
        return terminal, failures[0] if failures else None, header


def input_state(observation, bound, count, consumed, failure, checked=False):
    terminal = observation[0]
    state = terminal["constructor_inputs"]
    require(state["mode"] == "conditional-constructor-input-replay" and state["supplied"] == count
            and state["binding_attempted"] is True and state["binding_status"] == PROFILE_OK
            and state["completion_checked"] is checked, "actual conditional mode/binding/completion")
    view = state["profile_view_before_retirement"]
    require(view == {"bound": bound, "count": count, "consumed": consumed, "failure": failure},
            "actual native tape consumption/failure; no role attribution")
    return state


def extra_inputs(observation, count, consumed):
    state = input_state(observation, True, count, consumed, INPUT_EXTRA, checked=True)
    require(observation[0]["complete"] is False and observation[0]["completed"] == 15
            and observation[0]["failure"] == "terminal constructor input completeness"
            and state["completion_status"] == PROFILE_RANGE, "extra rows fail only at final boundary")


def exercise(harness, side):
    default = harness.run(side, "unbound")
    state = default[0]["constructor_inputs"]
    require(default[0]["complete"] is True and state["mode"] == "unpaired-default-clock"
            and state["supplied"] == 0 and state["binding_attempted"] is False
            and state["binding_status"] is None and state["completion_checked"] is False
            and state["completion_status"] is None and state["profile_view_before_retirement"] ==
            {"bound": False, "count": 0, "consumed": 0, "failure": INPUT_NONE},
            "old CLI stays unbound, unpaired and never checks tape completeness")
    measured = harness.run(side, "derive-length", tape(64))
    consumed = measured[0]["constructor_inputs"]["profile_view_before_retirement"]["consumed"]
    require(type(consumed) is int and 2 <= consumed < 64, "observed length exercises both signed extremes")
    extra_inputs(measured, 64, consumed)
    extra_inputs(harness.run(side, "one-extra", tape(consumed + 1)), consumed + 1, consumed)
    exact = harness.run(side, "exact-length", tape(consumed))
    state = input_state(exact, True, consumed, consumed, INPUT_NONE, checked=True)
    require(exact[0]["complete"] is True and state["completion_status"] == PROFILE_OK,
            "observed exact input count consumed by normal constructors")
    for label, count in (("empty", 0), ("short", consumed - 1)):
        observation = harness.run(side, label, tape(count))
        state = input_state(observation, True, count, count, INPUT_EXHAUSTED)
        require(observation[0]["complete"] is False and observation[0]["completed"] < 15
                and observation[0]["failure"] != "terminal constructor input completeness"
                and state["completion_status"] is None, "exhaustion preserves failed prefix without completion latch")
    values = rows(consumed)
    word, sample = values[0].split(b"\t", 1)
    values[0] = f"{int(word, 16) ^ 1:016x}".encode() + b"\t" + sample
    wrong = harness.run(side, "wrong-uniquifier", HEADER + b"".join(values))
    state = wrong[0]["constructor_inputs"]
    require(wrong[0]["failure"] == "constructor input binding" and
            state["mode"] == "conditional-constructor-input-replay" and state["supplied"] == consumed
            and state["binding_attempted"] is True and state["binding_status"] == PROFILE_INVALID
            and state["completion_checked"] is False and state["completion_status"] is None
            and state["profile_view_before_retirement"] ==
            {"bound": False, "count": 0, "consumed": 0, "failure": INPUT_NONE}
            and wrong[1]["construction"] is None and wrong[1]["registry"] is None,
            "full-width witness mismatch rejects before constructors without installing U")
    for label, data in invalid_tapes():
        observation = harness.run(side, label, data)
        state = observation[0]["constructor_inputs"]
        stage = "constructor input artifact read" if label == "excessive-bytes" else "constructor input artifact shape"
        require(observation[0]["failure"] == stage and observation[0]["completed"] == 0
                and observation[2] is None and state["mode"] == "conditional-constructor-input-replay"
                and state["supplied"] == 0 and state["binding_attempted"] is False
                and state["binding_status"] is None and state["completion_checked"] is False
                and state["completion_status"] is None and state["profile_view_before_retirement"] is None
                and observation[1]["construction"] is None and observation[1]["registry"] is None,
                "canonical parser rejects before profile/constructors")
        if label == "excessive-bytes":
            require(observation[1]["storage_status"] == 7 and
                    observation[1]["storage_operation"] == "read.length", "actual bounded file-reader rejection")
    harness.run(side, "existing-output", existing=True)


def fingerprint(path):
    require(path.is_file() and path.stat().st_size <= 128 * 1024 * 1024, "bounded actual test input")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    require(__debug__ and sys.flags.optimize == 0, "active Python validation")
    executable = args.executable.resolve(strict=True)
    selected = (executable, PLAN, Path(__file__).resolve())
    before = {path: fingerprint(path) for path in selected}
    try:
        with tempfile.TemporaryDirectory(prefix="minecraft-c-redstone-inputs-") as directory:
            harness = Harness(executable, Path(directory), PLAN.read_bytes())
            for side in ("client", "server"):
                exercise(harness, side)
            require(harness.count == 52, "all both-side native controls completed")
            print(f"PASS: {harness.count} native redstone constructor-input controls; no original comparison")
    finally:
        require({path: fingerprint(path) for path in selected} == before, "executable/plan/driver unchanged")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        traceback.print_exc()
        raise SystemExit(1)
