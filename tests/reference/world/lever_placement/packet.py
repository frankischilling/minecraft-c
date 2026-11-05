"""Audit finite original Lever lifecycle effects and each actual entropy stream."""

import collections
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("torch_own_entropy", HERE / "entropy.py")
OWN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OWN)
audit_entropy = OWN.audit_entropy
audit_cohort = OWN.audit_cohort
PINS = {'.gitattributes': 'a9b7a029a1b61ae51b4eca7a273cac2c352c899c08e836863859bf9483453c74', 'capture-provenance.json': '309991362cfaa2e6ed8d4b5d68e2a718a42d886a27c305e93d907c5cb3d8c384', 'capture_reference.py': '7ab120d4f7e2bdfdcef9e57fbbcccb91fd13ff29b6c0d7ab82525f15bcfb7c4d', 'declared-inputs.json': '62fdea9f65ac64171e5d735e46cd5f0c60b755e02c363d4b1aaeefd9399b796c', 'entropy.py': 'e84aaa944000305a26c644c41b95f27afa5a7551961fd71070affe20c6a417aa', 'LeverPlacementObservation.java': 'aa506077effd089b4135de118abc3f015271b5d6fe5a05e10e2abb8c2c216429', 'ordinary-0.entropy.jsonl': 'd69b071db4867087d04f2d9c861671ccc75c69213dd0db98039ead4ffa899595', 'ordinary-0.stdout': '33d649407a49ad23fd907d8fbf7a7a3c8aa08c6e8b5ecb3f5c73e23ac230f7df', 'ordinary-1.entropy.jsonl': '4e95a6f182cf7c7d1c6ca580f4334e7dbafd10103bb5d860ca22b3be318d3fd9', 'ordinary-1.stdout': '16dac2d8afb6f7bd2fa20840be01680a7ad6dc7de5c30ebf3494e4641f29056b', 'original.stdout': '362aa05e084b8e9300f6b722379afc1bf9a035998060f37b37fc98f0eb33d261', 'profiles.tsv': 'f66a8ce10f566c37e70c68122d772d50179a8d435e834aa5b0085bafec1104d0', 'protocol.json': 'c9991434cba21f37dc6b2d491f460e5b7d56564655b6d3e2041a93ae018bc9c7', 'README.md': '7c659204e6accc03ee6f1fb993cfc5cbb65face2af2e38bb3f58ddccc29e976e', 'source-pins.json': 'b0545dc47f36a1d904c8400c9954de56d60e2066e19bb574c0a6f3667be70f1a'}
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
KEYS = {"index", "name", "values", "result", "exception", "center_before", "center_after",
        "changes", "queues_before", "queues_after", "events", "drops", "world_rng_unchanged"}
GUARD = ("LEVER_GUARD\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes=3"
         "\tcanonical-chunks=4\tgame-code-replacements=0\tremote=0")
METHODS = [('<init>', '(II)V', 10), ('e', '(Lfd;III)Leq;', 2), ('c', '()Z', 2), ('d', '()Z', 2), ('b', '()I', 3), ('a', '(Lfd;IIII)Z', 107), ('a', '(Lfd;III)Z', 77), ('e', '(Lfd;IIII)V', 194), ('b', '(Lfd;IIII)V', 194), ('h', '(Lfd;III)Z', 43), ('a', '(Lxp;III)V', 183), ('b', '(Lfd;IIILgs;)V', 13), ('a', '(Lfd;IIILgs;)Z', 220), ('b', '(Lfd;III)V', 153), ('c', '(Lxp;IIII)Z', 22), ('d', '(Lfd;IIII)Z', 114), ('f', '()Z', 2)]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def integer(value):
    require(type(value) is int and -2147483648 <= value <= 2147483647, "signed JSON int")
    return value


def integer_text(value):
    require(re.fullmatch(r"0|-?[1-9][0-9]*", value), "canonical signed input")
    return integer(int(value))


def parse_profiles(data):
    lines = data.decode("ascii").splitlines()
    require(data == ("\n".join(lines) + "\n").encode("ascii"), "canonical LF profiles")
    profiles = []
    for line in lines:
        row = line.split("\t")
        require(len(row) == 13 and re.fullmatch(r"[a-z0-9_-]+", row[0]), "profile schema")
        profiles.append({"name": row[0], "values": [integer_text(value) for value in row[1:]]})
    require(len(profiles) == 244 and len({row["name"] for row in profiles}) == 244, "complete unique profile inventory")
    require(collections.Counter(row["values"][0] for row in profiles) ==
            {0: 13, 1: 13, 2: 20, 3: 17, 4: 33, 5: 33, 6: 33, 7: 49, 8: 33}, "all nine declared entries")
    for profile in profiles:
        values = profile["values"]
        require(0 <= values[7] <= 15 and values[10] in (0, 1) and values[11] in (0, 1), "declared support/null/remote flags")
        require(values[8] in (0, 1, 85, 20, 18, 60, 44) and values[9] in (0, 69), "finite support/target Blocks")
        if values[0] not in (0, 2, 3):
            require(values[1:4] == [8, 64, 8], "only read-only signed-coordinate profiles")
    return profiles


def authored_arrays(values):
    """Reconstruct declared inputs only, without evaluating any game behavior."""
    arrays = [bytearray([15 if kind == 2 else 0]) * size for _ in range(4)
              for kind, size in enumerate((32768, 16384, 16384, 16384, 256))]

    def put(x, y, z, block, metadata=0):
        if not (0 <= x <= 31 and 0 <= z <= 31 and 0 <= y <= 127):
            return
        chunk = (x >> 4) * 2 + (z >> 4)
        index = ((x & 15) << 11) | ((z & 15) << 7) | y
        arrays[chunk * 5][index] = block
        shift = (index & 1) * 4
        meta = arrays[chunk * 5 + 1]
        meta[index >> 1] = (meta[index >> 1] & ~(15 << shift)) | ((metadata & 15) << shift)

    def wrapped(value):
        return (value + (1 << 31)) % (1 << 32) - (1 << 31)

    x, y, z, mask = values[1], values[2], values[3], values[7]
    for flag, dx, dz in ((1, -1, 0), (2, 1, 0), (4, 0, -1), (8, 0, 1)):
        if mask & flag:
            put(wrapped(x + dx), y, wrapped(z + dz), 1)
    put(x, wrapped(y - 1), z, values[8])
    put(x, y, z, values[9], values[4])
    return arrays


def center(arrays, values):
    x, y, z = values[1:4]
    if not (0 <= x <= 31 and 0 <= z <= 31 and 0 <= y <= 127):
        return [0, 0]
    chunk = (x >> 4) * 2 + (z >> 4)
    index = ((x & 15) << 11) | ((z & 15) << 7) | y
    return [arrays[chunk * 5][index], (arrays[chunk * 5 + 1][index >> 1] >> ((index & 1) * 4)) & 15]


def validate_row(row, profile, index):
    require(type(row) is dict and set(row) == KEYS, "full LEVER schema")
    require(integer(row["index"]) == index and row["name"] == profile["name"] and row["values"] == profile["values"],
            "exact ordered profile identity/inputs")
    require(type(row["values"]) is list and all(type(value) is int for value in row["values"]), "actual signed input types")
    values = row["values"]
    if values[10]:
        require(row["result"] == "UNRETURNED" and row["exception"] == "java.lang.NullPointerException", "actual NULL-World prefix")
    else:
        allowed = ("true", "false") if values[0] in (0, 1, 2, 3, 5) else ("VOID",)
        require(row["result"] in allowed and row["exception"] == "NONE", "literal original outcome shape")
    arrays = authored_arrays(values)
    require(row["center_before"] == center(arrays, values), "actual declared center input")
    require(type(row["changes"]) is list, "five-array changes")
    previous = (-1, -1, -1)
    for change in row["changes"]:
        require(type(change) is list and len(change) == 5 and all(type(value) is int for value in change), "sparse change schema")
        chunk, kind, index_in_array, before, after = change
        require(0 <= chunk < 4 and 0 <= kind < 5, "actual Chunk/array index")
        require(previous < (chunk, kind, index_in_array), "unique ordered array changes")
        previous = (chunk, kind, index_in_array)
        actual = arrays[chunk * 5 + kind]
        require(0 <= index_in_array < len(actual) and 0 <= before <= 255 and 0 <= after <= 255 and before != after,
                "actual changed byte extent")
        require(actual[index_in_array] == before, "observed before byte agrees with authored input")
        actual[index_in_array] = after
    require(row["center_after"] == center(arrays, values), "center receipt agrees with complete observed array changes")
    for key in ("center_before", "center_after"):
        require(type(row[key]) is list and len(row[key]) == 2 and all(type(value) is int for value in row[key]), "center scalar types")
    for key in ("queues_before", "queues_after"):
        require(type(row[key]) is list, "actual lighting queue")
        for region in row[key]:
            require(type(region) is list and len(region) == 7 and all(type(value) is int for value in region), "light region schema")
            require(region[0] in (0, 1), "actual light kind")
            for value in region[1:]:
                integer(value)
            require(all(region[1 + axis] <= region[4 + axis] for axis in range(3)), "source light-region bounds")
    require(type(row["events"]) is list and type(row["drops"]) is list, "actual callback/drop lists")
    for event in row["events"]:
        require(type(event) is list and event, "event schema")
        if event[0] == "cell":
            require(len(event) == 6 and all(type(value) is int for value in event[1:]), "cell callback")
            require(0 <= event[4] <= 255 and 0 <= event[5] <= 15, "actual intermediate cell")
        elif event[0] == "add":
            require(len(event) == 3 and type(event[1]) is int and event[2] == "hl", "actual Item add callback")
        elif event[0] == "range":
            require(len(event) == 7 and all(type(value) is int for value in event[1:]), "range callback")
        elif event[0] == "sound":
            require(len(event) == 7 and event[1] == "random.click", "click sound callback")
            require(all(type(value) is str and re.fullmatch("[0-9a-f]{16}", value) for value in event[2:5]), "sound double bits")
            require(all(type(value) is str and re.fullmatch("[0-9a-f]{8}", value) for value in event[5:7]), "sound float bits")
        else:
            require(False, "unexpected callback in this captured cohort")
    for ordinal, drop in enumerate(row["drops"]):
        require(type(drop) is list and drop == [ordinal, "hl", 69, 1, 0, 10, 0, 5], "actual dropped Item logical fields")
        require(all(type(value) is int for value in drop[:1] + drop[2:]), "drop scalar types")
    require([event for event in row["events"] if event[0] == "add"] ==
            [["add", ordinal, "hl"] for ordinal in range(len(row["drops"]))], "actual publication/drop ordering")
    require(type(row["world_rng_unchanged"]) is bool, "actual World RNG comparison flag")


def parse_lines(lines, profiles):
    require(len(lines) == 245 and lines[-1] == "COMPLETE\t244", "complete literal projection")
    rows = []
    for index, (line, profile) in enumerate(zip(lines[:-1], profiles)):
        require(line.startswith("LEVER\t"), "LEVER prefix")
        row = json.loads(line.split("\t", 1)[1])
        validate_row(row, profile, index)
        if rows:
            require(rows[-1]["queues_after"] == row["queues_before"], "actual lighting queue joins without helper drain/reset")
        rows.append(row)
    return rows


def audit(directory=None):
    base = Path(directory) if directory is not None else HERE
    require(bool(PINS), "frozen artifact inventory")
    for name, expected in PINS.items():
        require(digest((base / name).read_bytes()) == expected, "artifact pin: " + name)
    profiles = parse_profiles((base / "profiles.tsv").read_bytes())
    declared = json.loads((base / "declared-inputs.json").read_text(encoding="utf-8"))
    require(declared["profiles"] == 244 and declared["rows"] == profiles, "frozen declared inputs")
    protocol = json.loads((base / "protocol.json").read_text(encoding="utf-8"))
    require(protocol["profiles"] == 244, "declared full projection schema")
    projection = (base / "original.stdout").read_bytes()
    rows = parse_lines(projection.decode("ascii").splitlines(), profiles)
    audits, raw_values, row_sets, projections = [], [], [], []
    for index in range(2):
        raw = (base / f"ordinary-{index}.stdout").read_bytes()
        raw_values.append(raw)
        lines = raw.decode("ascii").splitlines()
        require(raw == ("\r\n".join(lines) + "\r\n").encode("ascii"), "exact captured CRLF bytes")
        require(lines[:2] == ["16 achievements", "151 recipes"] and lines[-1] == GUARD, "actual bootstrap/guard")
        selected = [line for line in lines if line.startswith(("LEVER\t", "COMPLETE\t"))]
        projected = ("\n".join(selected) + "\n").encode("ascii")
        projections.append(projected)
        if index == 0:
            require(projection == projected, "whole process-zero literal projection")
        own_lines = [line for line in lines if line.startswith("ENTROPY\t")]
        require((base / f"ordinary-{index}.entropy.jsonl").read_bytes() == ("\n".join(own_lines) + "\n").encode("ascii"),
                "every raw entropy observation retained by declared LF selection")
        require(len(lines) == 492 and lines[2:-2:2] == selected[:-1] and lines[3:-2:2] == own_lines,
                "exact ordered LEVER/ENTROPY pairs; no hidden raw rows")
        current_rows = parse_lines(selected, profiles)
        row_sets.append(current_rows)
        own = [json.loads(line.split("\t", 1)[1]) for line in own_lines]
        audits.append(audit_cohort(current_rows, own))
    floor_cases = 0
    for first, second in zip(*row_sets):
        values = first["values"]
        floor_choice = values[0] == 4 and values[5] == 1 and values[8] == 1 and not values[10]
        if floor_choice:
            floor_cases += 1
        else:
            require(first == second, "every non-floor-choice literal record matches both originals")
    require(floor_cases == 2, "two independently own-stream-audited floor choices, without output normalization")
    source = json.loads((base / "source-pins.json").read_text(encoding="utf-8"))
    require(source["jar_sha256"] == JAR and source["classes"]["xr"]["class_sha256"] ==
            'bcc892e1884051f86d4d53fd3448c74bb0ce286fae025eb379e94230d470ca28', "unchanged original Lever authority")
    for name, descriptor, size in METHODS:
        found = [method for method in source["classes"]["xr"]["methods"] if method["name"] == name and method["descriptor"] == descriptor]
        require(len(found) == 1 and found[0]["code_bytes"] == size, "all exact Lever method identities")
    require(set(source["jdk_classes"]) == {"java/lang/Math", "java/lang/Math$RandomNumberGeneratorHolder", "java/util/Random"},
            "observed JDK entropy-owner/method pins")
    receipt = json.loads((base / "capture-provenance.json").read_text(encoding="utf-8"))
    require(receipt["status"] == "PASS" and receipt["ordinary_fresh_vms"] == 2 and receipt["original_game_class_replacements"] == 0,
            "two unchanged original VM receipt")
    require(receipt["inputs_before"] == receipt["inputs_after"] and receipt["inputs_unchanged"], "input continuity")
    require(receipt["helpers_before"] == receipt["helpers_after"] and receipt["helpers_unchanged"], "helper continuity")
    require(receipt["world_inputs_except_source_session_lock_unchanged"] and receipt["world_inputs_before"][0] == receipt["world_inputs_before"][1],
            "frozen genuine saved World inputs")
    for before, after in zip(receipt["world_inputs_before"], receipt["world_files_after"]):
        require({key: value for key, value in before.items() if key != "session.lock"} ==
                {key: value for key, value in after.items() if key != "session.lock"}, "actual source session-lock-only disk changes")
    require(receipt["logical_outputs_identical"] == (projections[0] == projections[1]) and receipt["original_stdout_sha256"] == digest(projection)
            and receipt["raw_outputs_identical"] == (raw_values[0] == raw_values[1]), "declared equality scope")
    require(receipt["supporting_guards"] == [[GUARD], [GUARD]], "actual external-effect guards")
    require(receipt["own_entropy_audits"] == [{"status": "PASS", **result} for result in audits], "both actual own-entropy audits")
    require(receipt["compile"]["returncode"] == 0 and receipt["compile"]["stderr_raw_private_sha256"] == digest(b""), "authored helper warning-free compile")
    require(len(receipt["runs"]) == 2 and all(run["returncode"] == 0 and run["stdout_sha256"] == digest(raw_values[index])
                                             for index, run in enumerate(receipt["runs"])), "all actual VM outcomes")
    for name in ("LeverPlacementObservation.java", "profiles.tsv", "protocol.json", "declared-inputs.json", "entropy.py", "capture_reference.py", "source-pins.json"):
        require(receipt["inputs_before"]["authored/" + name] == digest((base / name).read_bytes()), "frozen authored input: " + name)
    return {"profiles": 244, "selected_lines": 245, "source_exceptions": 9, "own_entropy_audits": audits,
            "original_stdout_sha256": digest(projection)}


def reference_lines(directory=None, process=0):
    audit(directory)
    require(process in (0, 1), "actual process index")
    base = Path(directory) if directory is not None else HERE
    return [line for line in (base / f"ordinary-{process}.stdout").read_text(encoding="ascii").splitlines()
            if line.startswith(("LEVER\t", "COMPLETE\t"))]


def reference_rows(directory=None, process=0):
    return [json.loads(line.split("\t", 1)[1]) for line in reference_lines(directory, process) if line.startswith("LEVER\t")]


read_packet = reference_rows


if __name__ == "__main__":
    print("Original Lever placement packet PASS:", audit())
