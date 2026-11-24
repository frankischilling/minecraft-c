"""Audit the finite unchanged-original WB placement replay; no game execution."""

import hashlib
import json
from pathlib import Path
import re

PINS = {'.gitattributes': 'd4d2151e4895b36b23f26bb645ff295872e2c8998e493de5046c1381fc93a117', 'capture-provenance.json': '5125dd22685bf123c419dfbd7c42e568adcfb1c087051625b486bd6103f4476e', 'capture_reference.py': 'c7e5e584ece292fc2d90911235801a2dc4e8c066dc7234baf4b09b4607417b07', 'declared-inputs.json': '7875a9bb5930dc66c8ec8ac3890e4760bb7f6d2289ecbdcf774b735f6d1f0763', 'ordinary-0.stdout': 'c616dfe23456813a11b3bf311d9af7c9b39a518e1b6830508647f10543970164', 'ordinary-1.stdout': 'c616dfe23456813a11b3bf311d9af7c9b39a518e1b6830508647f10543970164', 'original.stdout': '783a1a4de33b4bc75fb83bfe78a742b787a0ce56bfa49882e915d6114d93b5da', 'PlantPlacementObservation.java': 'cfdec415341284916872d08cdcc6c4a2c0962bef21b6ca4bfa7928d45da08dcf', 'profiles.tsv': '046d8ea3fde9a644c1ef2ba01f71c33ca0f3f71fc8345da8b84004011fffba0b', 'README.md': 'edfc907f9431a8a82276c082febe502336572ac638610ca2fd337b01a653547c', 'source-pins.json': '9f6e3ddcf6a6a51b8e406a20faed5045b05d5736745a3fcfe6d0dd885c5d1f2a'}
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
IDS = [6, 31, 32, 37, 38, 39, 40, 59]
CLASSES = ["CLASS\t6\the\tuu\twb\twb", "CLASS\t31\tru\tuu\twb\twb",
           "CLASS\t32\tjb\tuu\twb\tjb", "CLASS\t37\twb\tuu\twb\twb",
           "CLASS\t38\twb\tuu\twb\twb", "CLASS\t39\ttl\tuu\twb\ttl",
           "CLASS\t40\ttl\tuu\twb\ttl", "CLASS\t59\tni\tuu\twb\tni"]
GUARD = ("PLANT_GUARD\tnetwork=0\tnative=0\texecs=0\tdenied-writes=0\tworld-writes=3"
         "\tworld-fields-unchanged=1\tchunk-fields-unchanged=1\trng-unchanged=1"
         "\tregistry-unchanged=1\tcanonical-chunks=4\tremote=0")
METHODS = {
    ("uu", "a", "(Lfd;IIII)Z"): "94729f08853ab3f74c7cbb9d09338bfc3c1cc758791397ee30848b9beb040a12",
    ("uu", "a", "(Lfd;III)Z"): "07e4ebd6fba5526091bfc0c6ed03b925d73eeaa21a5dbb9fb4e13bd66545cc2e",
    ("wb", "a", "(Lfd;III)Z"): "4b25ac73468e3f21cb26bb8371eb78a1999ca809918992de6781fad6f5d33047",
    ("wb", "d", "(I)Z"): "30f0544d9c6d19694c826eecb86ac16e88281a8cbe56c1e30fa7ba40d9ce7da2",
    ("jb", "d", "(I)Z"): "290c45d1a53f919948b61f8516ac9411719c92ac79942c2b7e8fb25dff79397e",
    ("ni", "d", "(I)Z"): "2857a91d6823333b499d3abafa96bb63fc58c43a577f6224d5787b1e0bbe99b3",
    ("tl", "d", "(I)Z"): "00d14110d5487bba1b37a00d8a881c09bb111eefa57f61462c1f528fd08a8c28",
    ("fd", "a", "(IIIIZI)Z"): "71988febb19bfb799eb98c7925a18eefac7671467455f6b2aa6f749fbf1940d1",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def integer(text):
    require(re.fullmatch(r"0|-?[1-9][0-9]*", text) is not None, "canonical integer")
    value = int(text)
    require(-2147483648 <= value <= 2147483647, "signed int range")
    return value


def expected_profiles():
    """Declared input data only; never computes a placement result."""
    rows = []
    valid = {item: 12 if item == 32 else 60 if item == 59 else 1 if item in (39, 40) else 2
             for item in IDS}

    def add(name, item, y, face, target, below, null=0):
        rows.append([name, item, 8, y, 8, face, target, below, null])

    def code(value):
        return "n" + str(-value) if value < 0 else str(value)

    for item in IDS:
        for below in (0, 1, 2, 3, 12, 60, 20, 18):
            add(f"below_{item}_{below}", item, 64, 1, 0, below)
    for item in IDS:
        for target in (0, 1, 8, 9, 10, 11, 18, 31, 32, 37, 51, 78):
            add(f"target_{item}_{target}", item, 64, 1, target, valid[item])
    for item in IDS:
        for y in (-2147483648, -1, 0, 1, 127, 128, 129, 2147483647):
            add(f"y_{item}_{code(y)}", item, y, 1, 0, valid[item])
    for item in IDS:
        for face in (-2147483648, -1, 0, 1, 5, 6, 2147483647):
            add(f"face_{item}_{code(face)}", item, 64, face, 0, valid[item])
    for item in IDS:
        add(f"null_world_{item}", item, 64, 1, 0, valid[item], 1)
    return rows


def parse_profiles(data):
    lines = data.decode("ascii").splitlines()
    require(data == ("\n".join(lines) + "\n").encode("ascii"), "canonical LF profiles")
    rows = []
    for line in lines:
        fields = line.split("\t")
        require(len(fields) == 9 and re.fullmatch(r"[a-z0-9_]+", fields[0]), "profile schema")
        rows.append([fields[0], *map(integer, fields[1:])])
    require(rows == expected_profiles(), "complete frozen profile matrix/order")
    return rows


def parse_lines(lines, profiles):
    """Validate the complete literal row contract, retaining independent channels."""
    require(len(lines) == 873 and lines[:8] == CLASSES, "eight genuine registered receiver declarations")
    position, exceptions, changed = 8, 0, 0
    for index, profile in enumerate(profiles):
        name, *values = profile
        for channel in range(3):
            row = lines[position].split("\t")
            require(len(row) == 14 and row[0] == "PLANT", "PLANT schema")
            require(integer(row[1]) == index and row[2] == name and integer(row[3]) == channel,
                    "profile-major/channel order")
            require([integer(value) for value in row[4:11]] == values[:7], "exact declared operands")
            if values[7] == 0:
                require(row[11] in ("true", "false") and row[12] == "NONE", "actual normal outcome shape")
            else:
                require(row[11:13] == ["UNRETURNED", "java.lang.NullPointerException"], "retained NULL-World exception")
                exceptions += 1
            count = integer(row[13])
            require(0 <= count <= 328704, "actual five-array diff extent")
            changed += count
            position += 1
    require(lines[-1] == "COMPLETE\t8\t288\t864", "completion")
    require(exceptions == 24 and changed == 0, "observed exceptions and unchanged array bytes")
    return {"registered_receivers": 8, "runtime_classes": 6, "profiles": len(profiles), "calls": 864,
            "source_exceptions": exceptions, "changed_array_bytes": changed}


def audit(directory=None):
    base = Path(directory) if directory is not None else Path(__file__).resolve().parent
    require(bool(PINS), "frozen artifact inventory")
    for name, expected in PINS.items():
        require(digest((base / name).read_bytes()) == expected, "artifact pin: " + name)
    profiles = parse_profiles((base / "profiles.tsv").read_bytes())
    declared = json.loads((base / "declared-inputs.json").read_text(encoding="utf-8"))
    require(declared["jar_sha256"] == JAR and declared["registered_receivers"] == IDS
            and declared["runtime_classes"] == 6 and declared["profiles"] == 288, "declared finite inventory")
    require(declared["rows"] == [{"name": row[0], "values": row[1:]} for row in profiles], "full declared input rows")
    raw = [(base / f"ordinary-{index}.stdout").read_bytes() for index in range(2)]
    require(raw[0] == raw[1], "two fresh ordinary VM outputs identical")
    raw_lines = raw[0].decode("ascii").splitlines()
    require(raw[0] == ("\r\n".join(raw_lines) + "\r\n").encode("ascii"), "exact captured CRLF bytes")
    require(raw_lines[:2] == ["16 achievements", "151 recipes"] and raw_lines[-1] == GUARD,
            "genuine bootstrap and passive effect guard")
    selected = [line for line in raw_lines if line.startswith(("CLASS\t", "PLANT\t", "COMPLETE\t"))]
    require(len(raw_lines) == len(selected) + 3, "no unaccounted raw rows")
    projection = (base / "original.stdout").read_bytes()
    require(projection == ("\n".join(selected) + "\n").encode("ascii"), "declared LF projection")
    summary = parse_lines(projection.decode("ascii").splitlines(), profiles)
    source = json.loads((base / "source-pins.json").read_text(encoding="utf-8"))
    require(source["jar_sha256"] == JAR and len(source["classes"]) == 19, "source class inventory")
    for name, info in source["classes"].items():
        require(info["major"] == 49 and re.fullmatch("[0-9a-f]{64}", info["class_sha256"]), "original class identity")
        for method in info["methods"]:
            if "code_sha256" in method:
                require(method["code_bytes"] > 0 and re.fullmatch("[0-9a-f]{64}", method["code_sha256"]), "original Code pin")
    for (owner, name, descriptor), expected in METHODS.items():
        found = [method for method in source["classes"][owner]["methods"]
                 if method["name"] == name and method["descriptor"] == descriptor]
        require(len(found) == 1 and found[0]["code_sha256"] == expected, "exact selected method identity")
    require(source["classes"]["wb"]["super_class"] == "uu" and all(
        source["classes"][name]["super_class"] == "wb" for name in ("he", "ru", "jb", "tl", "ni")), "actual plant hierarchy")
    receipt = json.loads((base / "capture-provenance.json").read_text(encoding="utf-8"))
    require(receipt["status"] == "PASS" and receipt["ordinary_fresh_vms"] == 2
            and receipt["original_game_class_replacements"] == 0, "original VM receipt")
    require(receipt["inputs_before"] == receipt["inputs_after"] and receipt["inputs_unchanged"], "input continuity")
    require(receipt["helpers_before"] == receipt["helpers_after"] and receipt["helpers_unchanged"], "helper continuity")
    require(receipt["world_inputs_except_source_session_lock_unchanged"], "genuine saved-input continuity")
    worlds = receipt["world_inputs_before"]
    require(len(worlds) == 2 and worlds[0] == worlds[1], "identical pre-frozen World inputs")
    require(len(receipt["world_files_after"]) == 2, "both actual post-World inventories")
    for before, after in zip(worlds, receipt["world_files_after"]):
        require({key: value for key, value in before.items() if key != "session.lock"} ==
                {key: value for key, value in after.items() if key != "session.lock"}, "source session-lock-only file changes")
    require(receipt["raw_outputs_identical"] and receipt["original_stdout_sha256"] == digest(projection)
            and receipt["supporting_effect_guard"] == [GUARD], "observation/effect receipt")
    require(receipt["compile"]["returncode"] == 0 and receipt["compile"]["stderr_raw_private_sha256"] == digest(b""),
            "warning-free authored helper compile")
    require(len(receipt["runs"]) == 2 and all(row["returncode"] == 0 and row["stdout_sha256"] == digest(raw[index])
                                             for index, row in enumerate(receipt["runs"])), "actual successful repeats")
    require(receipt["ordinary_vm_flags"] == ["-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
                                             "-Djava.security.manager=allow", "--add-opens",
                                             "java.base/java.util=ALL-UNNAMED"], "ordinary VM flags/passive Random access")
    for name in ("PlantPlacementObservation.java", "profiles.tsv", "declared-inputs.json", "capture_reference.py", "source-pins.json"):
        require(receipt["inputs_before"]["authored/" + name] == digest((base / name).read_bytes()), "frozen authored input: " + name)
    require(receipt["inputs_before"]["inputs/placement-static-pins.json"] == source["placement_static_packet_sha256"],
            "shared public static authority pin")
    return {**summary, "original_stdout_sha256": digest(projection)}


def reference_lines(directory=None):
    audit(directory)
    base = Path(directory) if directory is not None else Path(__file__).resolve().parent
    return (base / "original.stdout").read_text(encoding="ascii").splitlines()


if __name__ == "__main__":
    print("Plant placement original packet PASS:", audit())
