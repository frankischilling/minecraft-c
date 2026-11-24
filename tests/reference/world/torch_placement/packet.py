"""Audit finite original Torch lifecycle effects and each actual entropy stream."""

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
PINS = {'.gitattributes': '5700cde87e33ab7ae32e9f6a62d7466b68acb6162ff39a021f349a82c659b624', 'capture-provenance.json': 'cb2b55e3d530bc0b8d96b8b533364fbf6e5bd8c1e2465e3b03bbba531448d0ce', 'capture_reference.py': '208cc6f8d03b1823e0337665f171450b47e95b1f8fee51cc11daff972410807c', 'declared-inputs.json': 'c8df7a000cefac9a2c8d864634b0a903081d27384ce88fdf07f1573b4a776f9c', 'entropy.py': '2177e3012c7fe4a6f876300373ab3125421c19af24dab15947195b534d710d9c', 'ordinary-0.entropy.jsonl': '99d1b87440de38ed9ad4fa25e307ba5ac568ebf29625dab12265e41c49abc7fc', 'ordinary-0.stdout': '4a7f8dfaf45a37af1fb87782b0d18edc7b819b005320a4b1a91f0c7a1fa542f8', 'ordinary-1.entropy.jsonl': 'f42d7eb307ee52395b91f259151591e956f6eec9e5ecbde6540cb22d8b375641', 'ordinary-1.stdout': 'd2bb87b9c7010892600af0504ad4b9f5bbe8ff15c9ff8e94765747463aca4b6c', 'original.stdout': '05e5b02107c34d304ab933c8011b0f04c0befa84e694a4b686e780369a555361', 'profiles.tsv': '253a0b67fd1b90cfbf6bd56b96f47404adb116d5bd06d54f3362d321324a5ddb', 'protocol.json': '4da588560ebac9c6b6eeed7b29e341ccfaf1181eab616fb625f4e00658fe5df1', 'README.md': 'd2dc94bce07cbc37fc141ab763e55c7f60a4bf0244ae6b050a3691cddc9aa1d3', 'source-pins.json': '36ff7b20d8a01ef3b2c916c8c3cedc5c105100468a18db290ef9a8f71d4c0e92', 'TorchPlacementObservation.java': '57ca0b5623fa2f220bdd30329724dbdd214df82c3f82bf253d2324c9ed7cfc99'}
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
KEYS = {"index", "name", "values", "result", "exception", "center_before", "center_after",
        "changes", "queues_before", "queues_after", "events", "drops", "world_rng_unchanged"}
GUARD = ("TORCH_GUARD\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes=3"
         "\tcanonical-chunks=4\tgame-code-replacements=0\tremote=0")
METHODS = [("h", "(Lfd;III)Z", 34), ("a", "(Lfd;III)Z", 78),
           ("e", "(Lfd;IIII)V", 132), ("c", "(Lfd;III)V", 134),
           ("i", "(Lfd;III)Z", 43), ("b", "(Lfd;IIII)V", 169),
           ("a", "(Lfd;IIILjava/util/Random;)V", 32)]


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
    require(len(profiles) == 193 and len({row["name"] for row in profiles}) == 193, "complete unique profile inventory")
    require(collections.Counter(row["values"][0] for row in profiles) ==
            dict(enumerate([22, 17, 22, 22, 22, 16, 16, 38, 18])), "all nine declared entries")
    for profile in profiles:
        values = profile["values"]
        require(0 <= values[7] <= 15 and values[10] in (0, 1) and values[11] in (0, 1), "declared support/null flags")
        require(values[8] in (0, 1, 85, 20, 18, 60, 44, 78) and values[9] in (0, 1, 50, 85, 20, 18, 60, 44, 78),
                "finite support/target Blocks")
        if values[0] >= 4:
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
    require(type(row) is dict and set(row) == KEYS, "full TORCH schema")
    require(integer(row["index"]) == index and row["name"] == profile["name"] and row["values"] == profile["values"],
            "exact ordered profile identity/inputs")
    require(type(row["values"]) is list and all(type(value) is int for value in row["values"]), "actual signed input types")
    values = row["values"]
    if values[10]:
        require(row["result"] == "UNRETURNED" and row["exception"] == "java.lang.NullPointerException", "actual NULL-World prefix")
    else:
        allowed = ("true", "false") if values[0] in (0, 1, 2, 3, 6) else ("VOID",)
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
        else:
            require(False, "unexpected callback in this captured cohort")
    for ordinal, drop in enumerate(row["drops"]):
        require(type(drop) is list and drop == [ordinal, "hl", 50, 1, 0, 10, 0, 5], "actual dropped Item logical fields")
        require(all(type(value) is int for value in drop[:1] + drop[2:]), "drop scalar types")
    require([event for event in row["events"] if event[0] == "add"] ==
            [["add", ordinal, "hl"] for ordinal in range(len(row["drops"]))], "actual publication/drop ordering")
    require(type(row["world_rng_unchanged"]) is bool, "actual World RNG comparison flag")


def parse_lines(lines, profiles):
    require(len(lines) == 194 and lines[-1] == "COMPLETE\t193", "complete literal projection")
    rows = []
    for index, (line, profile) in enumerate(zip(lines[:-1], profiles)):
        require(line.startswith("TORCH\t"), "TORCH prefix")
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
    require(declared["jar_sha256"] == JAR and declared["profiles"] == 193 and declared["rows"] == profiles, "frozen declared inputs")
    protocol = json.loads((base / "protocol.json").read_text(encoding="utf-8"))
    require(protocol["profiles"] == 193 and set(protocol["torch_keys"]) == KEYS, "declared full projection schema")
    projection = (base / "original.stdout").read_bytes()
    rows = parse_lines(projection.decode("ascii").splitlines(), profiles)
    audits, raw_values = [], []
    for index in range(2):
        raw = (base / f"ordinary-{index}.stdout").read_bytes()
        raw_values.append(raw)
        lines = raw.decode("ascii").splitlines()
        require(raw == ("\r\n".join(lines) + "\r\n").encode("ascii"), "exact captured CRLF bytes")
        require(lines[:2] == ["16 achievements", "151 recipes"] and lines[-1] == GUARD, "actual bootstrap/guard")
        selected = [line for line in lines if line.startswith(("TORCH\t", "COMPLETE\t"))]
        require(projection == ("\n".join(selected) + "\n").encode("ascii"), "whole declared logical projections identical")
        own_lines = [line for line in lines if line.startswith("ENTROPY\t")]
        require((base / f"ordinary-{index}.entropy.jsonl").read_bytes() == ("\n".join(own_lines) + "\n").encode("ascii"),
                "every raw entropy observation retained by declared LF selection")
        require(len(lines) == 390 and lines[2:-2:2] == selected[:-1] and lines[3:-2:2] == own_lines,
                "exact ordered TORCH/ENTROPY pairs; no hidden raw rows")
        own = [json.loads(line.split("\t", 1)[1]) for line in own_lines]
        audits.append(audit_cohort(rows, own))
    source = json.loads((base / "source-pins.json").read_text(encoding="utf-8"))
    require(source["jar_sha256"] == JAR and source["classes"]["vm"]["class_sha256"] ==
            "de491b271f4a805c89bf0132d8578f1fb22792959d1cd616ea68d9e0ffc1ae5c", "unchanged original Torch authority")
    for name, descriptor, size in METHODS:
        found = [method for method in source["classes"]["vm"]["methods"] if method["name"] == name and method["descriptor"] == descriptor]
        require(len(found) == 1 and found[0]["code_bytes"] == size, "all seven exact Torch method identities")
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
    require(receipt["logical_outputs_identical"] and receipt["original_stdout_sha256"] == digest(projection)
            and receipt["raw_outputs_identical"] == (raw_values[0] == raw_values[1]), "declared equality scope")
    require(receipt["supporting_guards"] == [[GUARD], [GUARD]], "actual external-effect guards")
    require(receipt["own_entropy_audits"] == [{"status": "PASS", **result} for result in audits], "both actual own-entropy audits")
    require(receipt["compile"]["returncode"] == 0 and receipt["compile"]["stderr_raw_private_sha256"] == digest(b""), "authored helper warning-free compile")
    require(len(receipt["runs"]) == 2 and all(run["returncode"] == 0 and run["stdout_sha256"] == digest(raw_values[index])
                                             for index, run in enumerate(receipt["runs"])), "all actual VM outcomes")
    for name in ("TorchPlacementObservation.java", "profiles.tsv", "protocol.json", "declared-inputs.json", "entropy.py", "capture_reference.py", "source-pins.json"):
        require(receipt["inputs_before"]["authored/" + name] == digest((base / name).read_bytes()), "frozen authored input: " + name)
    return {"profiles": 193, "selected_lines": 194, "source_exceptions": 9, "own_entropy_audits": audits,
            "original_stdout_sha256": digest(projection)}


def reference_lines(directory=None):
    audit(directory)
    base = Path(directory) if directory is not None else HERE
    return (base / "original.stdout").read_text(encoding="ascii").splitlines()


def reference_rows(directory=None):
    return [json.loads(line.split("\t", 1)[1]) for line in reference_lines(directory) if line.startswith("TORCH\t")]


read_packet = reference_rows


if __name__ == "__main__":
    print("Original Torch placement packet PASS:", audit())
