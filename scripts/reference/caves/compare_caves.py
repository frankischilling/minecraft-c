#!/usr/bin/env python3
"""Compare complete natural cave states with isolated original public entries.

Every world is a newly owned root. Inventory monitoring is not a hard filesystem
quota. Raw C table bytes are measured, hashed and checked before state comparison.
"""

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import stat
import subprocess
import sys
import threading
import time
import uuid


REQUEST_SHA = "270f1f0ff0ce836d9a28102407c55dd429222fb4cdf3978ee43a75ba2ee5efae"
TABLE_SHA = "87a67a39fff59636acb04edcda4ef622831037291f11fb61586c57596771dac2"
JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
from fixtures import CASES, fixtures

CHANNELS = dict(zip("klmnoabc", (16, 16, 8, 4, 4, 10, 16, 8)))
SCRATCH = dict(zip("defghrst", (425, 425, 425, 25, 25, 256, 256, 256)))
IMMUTABLE = ("generator_continuation", "noise82", "q425", "scratch", "climate",
             "materials", "math_table_sha256_f32be")
CONTROL = ("CONTROL_CAVES_STATE j_exact noise82_exact q425_exact scratch_exact "
           "climate_exact materials_exact table_exact suffix_exact")
CLASS_NAMES = {"CavesObservation.class", "CavesObservation$Request.class",
               "CavesObservation$Context.class", "CavesObservation$Snapshot.class",
               "CavesObservation$Model.class"}
MAX_STDOUT = 64 * 1024 * 1024
MAX_STDERR = 4 * 1024 * 1024
MAX_LINE = 4 * 1024 * 1024
MAX_ENTRIES = 8192
MAX_WORLD_BYTES = 128 * 1024 * 1024
MAX_MANIFEST = 4 * 1024 * 1024
CHILD_SECONDS = 60
HEX16 = re.compile(r"[0-9a-f]{16}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
HEX = re.compile(r"[0-9a-f]*\Z")


class Failure(Exception):
    pass


class Unavailable(Exception):
    pass


def require(condition, message):
    if not condition:
        raise Failure(message)


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_record(path, byte_limit=None):
    path = Path(path).resolve(strict=True)
    require(path.is_file(), f"not a regular file: {path}")
    before = path.stat()
    if byte_limit is None:
        digest = sha256(path)
    else:
        require(before.st_size <= byte_limit, f"file exceeds hashing byte limit: {path}")
        digest_state = hashlib.sha256()
        remaining = byte_limit
        with path.open("rb") as stream:
            while remaining:
                block = stream.read(min(1024 * 1024, remaining))
                if not block:
                    break
                digest_state.update(block)
                remaining -= len(block)
            require(not stream.read(1), f"file grew beyond hashing byte limit: {path}")
        digest = digest_state.hexdigest()
    after = path.stat()
    require((before.st_size, before.st_mtime_ns, before.st_ino, before.st_dev) ==
            (after.st_size, after.st_mtime_ns, after.st_ino, after.st_dev),
            f"file changed while hashing: {path}")
    return {"path": str(path), "bytes": after.st_size, "sha256": digest}


def json_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def strict_json(data):
    try:
        return json.loads(data, object_pairs_hook=json_pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(
                              Failure("non-finite JSON number: " + value)))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise Failure(f"invalid JSON: {error}") from error


def write_json(path, value):
    # The enclosing artifact root is newly owned. Atomic publication never
    # replaces an input or a report from another capture.
    data = (json.dumps(value, indent=2, ensure_ascii=True, allow_nan=False) + "\n").encode()
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    with temporary.open("xb") as stream:
        stream.write(data)
    os.replace(temporary, path)


def object_keys(value, names, context):
    require(type(value) is dict and set(value) == set(names), context + ": object keys")


def integer(value, low, high, context):
    require(type(value) is int and low <= value <= high, context + ": integer range")


def bit_list(value, length, context):
    require(type(value) is list and len(value) == length, context + ": cardinality")
    require(all(type(item) is str and HEX16.fullmatch(item) for item in value),
            context + ": raw binary64/long representation")


def double_descriptor(value, minimum, maximum, context):
    object_keys(value, ("length", "values"), context)
    integer(value["length"], minimum, maximum, context)
    bit_list(value["values"], value["length"], context)


def block_descriptor(value, context):
    object_keys(value, ("length", "values"), context)
    require(type(value["length"]) is int and value["length"] == 32771,
            context + ": byte length")
    text = value["values"]
    require(type(text) is str and len(text) == 65542 and HEX.fullmatch(text),
            context + ": raw bytes")
    require(text[-6:] == "8a8b8c", context + ": owned suffix changed")
    return bytes.fromhex(text)


def validate_noise(value, context):
    object_keys(value, CHANNELS, context)
    for channel, count in CHANNELS.items():
        layers = value[channel]
        require(type(layers) is list and len(layers) == count, context + ": layer count")
        for layer in layers:
            object_keys(layer, ("offsets", "permutation"), context)
            bit_list(layer["offsets"], 3, context + ": offsets")
            permutation = layer["permutation"]
            require(type(permutation) is list and len(permutation) == 512,
                    context + ": permutation length")
            require(all(type(item) is int and 0 <= item <= 255 for item in permutation),
                    context + ": permutation entries")
            require(sorted(permutation[:256]) == list(range(256)) and
                    permutation[:256] == permutation[256:], context + ": permutation halves")


def validate_snapshot(value, stage):
    context = "snapshot " + stage
    object_keys(value, ("generator_continuation", "cave_continuation", "noise82", "q425",
                        "scratch", "climate", "materials", "math_table_sha256_f32be", "blocks"), context)
    bit_list(value["generator_continuation"], 4, context)
    if stage in ("constructor", "prefix"):
        require(value["cave_continuation"] is None, context + ": unobserved cave state")
    else:
        bit_list(value["cave_continuation"], 4, context)
    validate_noise(value["noise82"], context)
    require(value["math_table_sha256_f32be"] == TABLE_SHA, context + ": accepted math digest")
    object_keys(value["scratch"], SCRATCH, context + ": scratch")
    for name, size in SCRATCH.items():
        array = value["scratch"][name]
        if stage == "constructor" and name in "defgh":
            require(array is None, context + ": constructor scratch allocation")
        else:
            double_descriptor(array, size, size, context + ": scratch " + name)
            if stage == "constructor":
                require(all(bits == "0000000000000000" for bits in array["values"]),
                        context + ": constructor surface scratch not zero")
    if stage == "constructor":
        require(all(value[name] is None for name in ("q425", "climate", "materials", "blocks")),
                context + ": unobserved data must be null")
        return None
    double_descriptor(value["q425"], 425, 425, context + ": density")
    climate = value["climate"]
    object_keys(climate, ("temperature", "rainfall", "auxiliary", "biomes"), context + ": climate")
    for name in ("temperature", "rainfall", "auxiliary"):
        double_descriptor(climate[name], 256, 4096, context + ": climate " + name)
    require(type(climate["biomes"]) is str and len(climate["biomes"]) == 256 and
            all("a" <= item <= "m" for item in climate["biomes"]), context + ": biome identities")
    material = value["materials"]
    require(type(material) is str and len(material) == 1024 and HEX.fullmatch(material),
            context + ": material bytes")
    return block_descriptor(value["blocks"], context)


def validate_case(value, index, request):
    object_keys(value, ("case", "seed", "chunk", "chunk_seed", "constructor", "prefix", "passes",
                        "restored_output_equal_first", "reused_input_differs_from_prefix",
                        "changed_output_equal_its_input"), "case")
    seed, x, z = request
    require(type(value["case"]) is int and value["case"] == index, "case index")
    require(type(value["seed"]) is int and value["seed"] == seed, "case seed")
    require(type(value["chunk"]) is list and len(value["chunk"]) == 2 and
            all(type(item) is int for item in value["chunk"]) and value["chunk"] == [x, z],
            "case target")
    chunk_seed = (x * 341873128712 + z * 132897987541) & ((1 << 64) - 1)
    require(value["chunk_seed"] == f"{chunk_seed:016x}", "chunk seed wrapping")
    validate_snapshot(value["constructor"], "constructor")
    prefix = value["prefix"]
    prefix_bytes = validate_snapshot(prefix, "prefix")
    require(value["constructor"]["noise82"] == prefix["noise82"], "prefix changed constructor noise")
    passes = value["passes"]
    require(type(passes) is list and len(passes) == 3, "three cave passes required")
    outputs = []
    for number, label in enumerate(("first", "restored", "changed")):
        item = passes[number]
        object_keys(item, ("label", "input_blocks", "after", "changed_prefix_bytes", "suffix_unchanged"),
                    "cave pass")
        require(item["label"] == label, "cave pass label/order")
        before = block_descriptor(item["input_blocks"], label + " input")
        expected_input = prefix_bytes if number < 2 else outputs[1]
        require(before == expected_input, label + ": input relationship")
        after = validate_snapshot(item["after"], label)
        for field in IMMUTABLE:
            require(item["after"][field] == prefix[field], label + ": changed " + field)
        changes = sum(a != b for a, b in zip(before[:32768], after[:32768]))
        integer(item["changed_prefix_bytes"], 0, 32768, label + ": changed byte count")
        require(item["changed_prefix_bytes"] == changes, label + ": incorrect observed byte count")
        require(type(item["suffix_unchanged"]) is bool and item["suffix_unchanged"] and
                before[32768:] == after[32768:], label + ": suffix control")
        outputs.append(after)
    booleans = {"restored_output_equal_first": outputs[0] == outputs[1],
                "reused_input_differs_from_prefix": outputs[1] != prefix_bytes,
                "changed_output_equal_its_input": outputs[2] == outputs[1]}
    for name, observed in booleans.items():
        require(type(value[name]) is bool and value[name] == observed, name + ": observation differs from bytes")
    # Replaying the identical public entry must reseed the retained cave Random.
    # No expectation is imposed on changed-input idempotence or carved counts.
    require(outputs[0] == outputs[1], "restored public entry produced different bytes")
    require(passes[0]["after"]["cave_continuation"] == passes[1]["after"]["cave_continuation"],
            "restored public entry produced different cave continuation")
    return {"case": index, "changed_prefix_bytes": [item["changed_prefix_bytes"] for item in passes],
            "observations": booleans, "restored_continuation_equal": True}


def first_difference(left, right, path="$"):
    if type(left) is not type(right):
        return path + ": type differs"
    if type(left) is dict:
        if set(left) != set(right):
            return path + ": keys differ"
        for key in left:
            difference = first_difference(left[key], right[key], path + "." + key)
            if difference:
                return difference
    elif type(left) is list:
        if len(left) != len(right):
            return path + ": length differs"
        for number, (a, b) in enumerate(zip(left, right)):
            difference = first_difference(a, b, f"{path}[{number}]")
            if difference:
                return difference
    elif left != right:
        return path + ": value differs"
    return None


def is_link(info):
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400)


def no_link_components(path):
    """Check lexical components before resolve can hide a redirect."""
    path = Path(os.path.abspath(path))
    for component in reversed((path, *path.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        require(not is_link(info), f"link/reparse component rejected: {component}")
        if component != path:
            require(stat.S_ISDIR(info.st_mode), f"non-directory parent: {component}")
    return path


def world_inventory(root, hashes=False, max_entries=MAX_ENTRIES, max_bytes=MAX_WORLD_BYTES):
    """Bound traversal/storage; polling cannot impose a kernel disk quota."""
    root = Path(root)
    result = {"root": str(root), "exists": False, "entries": [], "files": 0,
              "directories": 0, "bytes": 0, "complete": False, "error": None}
    try:
        no_link_components(root)
        if not root.exists():
            result["complete"] = True
            return result
        require(root.is_dir(), "world root is not a directory")
        result["exists"] = True
        pending = [root]
        while pending:
            directory = pending.pop()
            with os.scandir(directory) as entries:
                for entry in entries:
                    require(len(result["entries"]) < max_entries, "world inventory entry limit")
                    # DirEntry.stat reports zero identity/link fields on Windows.
                    # os.stat supplies the actual hard-link count without following links.
                    info = os.stat(entry.path, follow_symlinks=False)
                    require(not is_link(info), "world link/reparse entry: " + entry.path)
                    relative = Path(entry.path).relative_to(root)
                    require(len(relative.parts) <= 64 and len(str(relative)) <= 4096,
                            "world inventory path/depth limit")
                    record = {"path": relative.as_posix()}
                    if stat.S_ISDIR(info.st_mode):
                        record["kind"] = "directory"
                        result["directories"] += 1
                        pending.append(Path(entry.path))
                    else:
                        require(stat.S_ISREG(info.st_mode), "non-regular world entry: " + entry.path)
                        require(info.st_nlink == 1, "hard-linked world file: " + entry.path)
                        require(info.st_size <= max_bytes - result["bytes"], "world inventory byte limit")
                        record.update(kind="file", bytes=info.st_size)
                        result["files"] += 1
                        result["bytes"] += info.st_size
                        if hashes:
                            current = file_record(Path(entry.path), byte_limit=info.st_size)
                            require(current["bytes"] == info.st_size, "world file changed during inventory")
                            record["sha256"] = current["sha256"]
                    result["entries"].append(record)
        result["entries"].sort(key=lambda entry: entry["path"])
        result["complete"] = True
    except (OSError, Failure) as error:
        result["error"] = str(error)
    return result


@dataclass
class ProcessResult:
    command: list
    returncode: int | None
    seconds: float
    timed_out: bool
    stop_reason: str | None
    stdout_observed: int
    stderr_observed: int
    stdout_retained: int
    stderr_retained: int
    inventory_polls: int


def run_live(command, stdout_path, stderr_path, cwd, *, seconds=CHILD_SECONDS,
             stdout_limit=MAX_STDOUT, stderr_limit=MAX_STDERR, world_root=None, stdin_path=None):
    started = time.monotonic()
    counts = {"stdout": [0, 0], "stderr": [0, 0]}
    stop = []
    lock = threading.Lock()
    timed_out = False
    polls = 0
    with Path(stdout_path).open("xb") as out, Path(stderr_path).open("xb") as err, \
            (Path(stdin_path).open("rb") if stdin_path is not None else open(os.devnull, "rb")) as child_input:
        process = subprocess.Popen(command, cwd=cwd, stdin=child_input,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        def kill(reason):
            with lock:
                if not stop:
                    stop.append(reason)
            try:
                process.kill()
            except OSError:
                pass

        def drain(name, pipe, sink, limit):
            try:
                while True:
                    block = pipe.read1(65536)
                    if not block:
                        break
                    counts[name][0] += len(block)
                    retained = min(len(block), limit - counts[name][1])
                    if retained:
                        sink.write(block[:retained])
                        counts[name][1] += retained
                    if counts[name][0] > limit:
                        kill(name + " live byte limit exceeded")
            except (OSError, ValueError) as error:
                kill(name + " capture failed: " + str(error))
            finally:
                pipe.close()

        threads = []
        try:
            for name, pipe, sink, limit in (("stdout", process.stdout, out, stdout_limit),
                                             ("stderr", process.stderr, err, stderr_limit)):
                thread = threading.Thread(target=drain, args=(name, pipe, sink, limit), daemon=True)
                thread.start()
                threads.append(thread)
            next_poll = started
            while process.poll() is None:
                now = time.monotonic()
                if now - started >= seconds:
                    timed_out = True
                    kill("child timeout")
                if world_root is not None and now >= next_poll:
                    inventory = world_inventory(world_root)
                    polls += 1
                    if inventory["error"]:
                        kill("world inventory: " + inventory["error"])
                    next_poll = now + 0.5
                try:
                    process.wait(timeout=0.1)
                except subprocess.TimeoutExpired:
                    continue
        finally:
            # Also runs if thread startup, polling or inventory supervision
            # raises. Reap the direct child before touching its pipe handles.
            if process.returncode is None:
                kill("capture supervision ended before child exit")
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                kill("direct child could not be reaped within cleanup bound")
            deadline = time.monotonic() + 2
            for thread in threads:
                thread.join(timeout=max(0, deadline - time.monotonic()))
            alive = any(thread.is_alive() for thread in threads)
            if alive:
                kill("pipe drain did not finish after child exit")
            for number, pipe in enumerate((process.stdout, process.stderr)):
                if pipe.closed:
                    continue
                if number < len(threads) and threads[number].is_alive():
                    # A descendant retaining a pipe is outside this direct-child
                    # contract. Close its OS handle without waiting on the
                    # buffered reader lock; the capture remains a failure.
                    try:
                        os.close(pipe.fileno())
                    except OSError:
                        pass
                else:
                    pipe.close()
            out.flush()
            err.flush()
    return ProcessResult(list(map(str, command)), process.returncode, time.monotonic() - started,
                         timed_out, stop[0] if stop else None, counts["stdout"][0],
                         counts["stderr"][0], counts["stdout"][1], counts["stderr"][1], polls)


def lifecycle(stderr_path, requests, world_root):
    text = Path(stderr_path).read_bytes().decode("utf-8", errors="strict")
    phase, index, controls, total = "class", 0, 0, 0
    unavailable = []
    for line in text.splitlines():
        if line.startswith("UNAVAILABLE:"):
            unavailable.append({"line": line, "phase": phase, "next_case": index + 1})
            continue
        if not line.startswith(("SETUP_", "MEASURE_", "CONTROL_CAVES")):
            continue  # Actual constructor/setup diagnostics and stack traces.
        require(index < len(requests), "lifecycle marker beyond request count")
        seed = requests[index][0]
        if line.startswith("SETUP_BEGIN"):
            require(phase in ("class", "between"), "SETUP_BEGIN out of order")
            expected = f"SETUP_BEGIN seed={seed} root={world_root / ('case-' + format(index + 1, '03d'))}"
            require(line == expected, "SETUP_BEGIN seed/root differs")
            phase = "setup"
        elif line.startswith("SETUP_DONE"):
            require(phase == "setup" and line == f"SETUP_DONE seed={seed}", "SETUP_DONE differs/order")
            phase = "ready"
        elif line.startswith("MEASURE_BEGIN"):
            require(phase == "ready" and line == f"MEASURE_BEGIN case={index + 1} seed={seed}",
                    "MEASURE_BEGIN differs/order")
            phase, controls = "measure", 0
        elif line.startswith("CONTROL_CAVES"):
            require(phase == "measure" and line == CONTROL and controls < 3, "cave control marker differs/order")
            controls += 1
            total += 1
        elif line.startswith("MEASURE_DONE"):
            require(phase == "measure" and controls == 3 and
                    line == f"MEASURE_DONE case={index + 1} seed={seed}", "MEASURE_DONE differs/order")
            phase, index = "between", index + 1
        else:
            raise Failure("unknown reserved lifecycle marker: " + line)
    return {"phase": phase, "completed": index, "active_controls": controls,
            "control_count": total, "unavailable": unavailable}


def permitted_unavailable(process, markers, records, requests):
    if process.stop_reason and not (process.timed_out and process.stop_reason == "child timeout"):
        return False
    if markers["completed"] != records:
        return False
    if process.timed_out:
        return markers["phase"] == "setup" and records < len(requests) and not markers["unavailable"]
    if process.returncode != 77 or len(markers["unavailable"]) != 1:
        return False
    event = markers["unavailable"][0]
    if markers["phase"] == "class" and records == 0 and event["phase"] == "class":
        return event["line"] in ("UNAVAILABLE: original class setup failed",
                                 "UNAVAILABLE: original JAR SHA-256 differs")
    return (markers["phase"] == "setup" and records < len(requests) and
            event["phase"] == "setup" and event["next_case"] == records + 1 and
            event["line"] == "UNAVAILABLE: genuine original world/generator setup failed for seed=" +
            str(requests[records][0]))


def read_records(path, requests, side):
    records = 0
    with Path(path).open("rb") as stream:
        while True:
            line = stream.readline(MAX_LINE + 1)
            if not line:
                break
            require(len(line) <= MAX_LINE and line.endswith(b"\n"), side + ": oversized/incomplete record")
            require(records < len(requests) and line.startswith(b"OK\t"), side + ": unexpected output record")
            payload = line[3:-1]
            if payload.endswith(b"\r"):
                payload = payload[:-1]
            value = strict_json(payload.decode("utf-8", errors="strict"))
            observation = validate_case(value, records + 1, requests[records])
            yield value, observation
            records += 1


def parse_side(path, requests, side, artifact_root, paired_paths):
    records, controls, paths = 0, [], []
    for value, observation in read_records(path, requests, side):
        output = artifact_root / f"{side}-case-{records + 1:03d}.json"
        with output.open("xb") as destination:
            destination.write(json.dumps(value, separators=(",", ":"), allow_nan=False).encode() + b"\n")
        paths.append(output)
        controls.append(observation)
        # Compare each available completed server case immediately. A later
        # setup-unavailable can never mask an earlier paired difference.
        if paired_paths is not None:
            require(records < len(paired_paths), "server completed beyond client prefix")
            reference = strict_json(paired_paths[records].read_bytes())
            difference = first_difference(reference, value)
            require(difference is None, f"client/server case {records + 1}: {difference}")
        records += 1
    return paths, controls


def find_tool(override, name):
    executable = shutil.which(override or name)
    if executable:
        return Path(executable).resolve()
    if not override and os.environ.get("JAVA_HOME"):
        path = Path(os.environ["JAVA_HOME"]) / "bin" / (name + (".exe" if os.name == "nt" else ""))
        if path.is_file():
            return path.resolve()
    raise Unavailable(name + " executable not found")


def samefile_or_ancestor(candidate, inputs):
    candidate = Path(candidate).resolve()
    for path in inputs:
        path = Path(path).resolve()
        require(candidate != path and candidate not in path.parents,
                f"owned output would contain an input: {path}")
        if candidate.exists() and path.exists():
            require(not os.path.samefile(candidate, path), "owned output aliases an input")


def input_fingerprint(inputs):
    return {key: {"bytes": value["bytes"], "sha256": value["sha256"]}
            for key, value in sorted(inputs.items())}


def runtime_tools(args):
    tools = {name: find_tool(getattr(args, name), name) for name in ("java", "javac", "javap")}
    homes = {path.parent.parent.resolve() for path in tools.values()}
    require(len(homes) == 1, "java, javac and javap must belong to one JDK")
    home = next(iter(homes))
    for name, path in (("jdk_release", home / "release"), ("jdk_modules", home / "lib/modules"),
                       ("jdk_ct_sym", home / "lib/ct.sym")):
        if not path.is_file():
            raise Unavailable("missing attributed JDK runtime file: " + str(path))
        tools[name] = path.resolve()
    return tools


def java_major(text, name):
    pattern = (r'(?m)^\s*(?:openjdk|java) version "(\d+)(?:\.(\d+))?' if name == "java"
               else r"(?m)^\s*(?:javac\s+)?(\d+)(?:\.(\d+))?")
    match = re.search(pattern, text)
    require(match is not None, "cannot identify " + name + " version")
    return int(match.group(2) if match.group(1) == "1" else match.group(1))


def tool_versions(tools, artifact_root, report):
    report["versions"] = {}
    for name in ("java", "javac", "javap"):
        command = [str(tools[name]), "-version"]
        out, err = (artifact_root / (name + "-version-" + ending + ".txt") for ending in ("stdout", "stderr"))
        result = run_live(command, out, err, artifact_root, seconds=15,
                          stdout_limit=65536, stderr_limit=65536)
        report["versions"][name] = asdict(result)
        require(not result.stop_reason and result.returncode == 0, name + " version process failed")
        version = (out.read_bytes() + b"\n" + err.read_bytes()).decode("utf-8", errors="strict")
        report["versions"][name]["output"] = version
        major = java_major(version, name)
        report["versions"][name]["major"] = major
        if major < 11:
            raise Unavailable(name + " requires version 11 or newer")


def classes_inventory(directory):
    require({item.name for item in directory.iterdir()} == CLASS_NAMES, "unexpected/missing helper class files")
    result = {}
    for name in sorted(CLASS_NAMES):
        path = directory / name
        require(not is_link(path.lstat()) and path.is_file() and path.stat().st_nlink == 1,
                "class file is not fresh regular storage")
        with path.open("rb") as stream:
            header = stream.read(8)
        require(len(header) == 8 and header[:4] == b"\xca\xfe\xba\xbe" and header[6:8] == b"\x00\x37",
                "helper class must have Java 11 major 55")
        result[name] = file_record(path, byte_limit=4 * 1024 * 1024)
    return result


def class_fingerprint(classes):
    return {name: {"bytes": item["bytes"], "sha256": item["sha256"]}
            for name, item in sorted(classes.items())}


def after_inputs(inputs):
    after, errors = {}, []
    for name, before in inputs.items():
        try:
            current = file_record(Path(name))
            after[name] = current
            if current != before:
                errors.append("attributed input changed: " + name)
        except (OSError, Failure) as error:
            errors.append("cannot verify attributed input: " + name + ": " + str(error))
    return after, errors



def source_paths(root):
    names = (
        'CMakeLists.txt', 'scripts/reference/caves/compare_caves.py',
        'scripts/reference/caves/fixtures.py', 'tests/reference/caves/CavesObservation.java',
        'src/world/terrain/caves.c', 'include/mc/world/terrain/caves.h',
        'src/world/terrain/caves_internal.h', 'tests/world/terrain/caves_probe.c',
        'tests/world/terrain/test_caves.c', 'tests/world/terrain/caves_fixtures.h',
        'tests/tools/caves/test_reference.py', 'docs/reference/caves.md',
        'src/world/terrain/surface.c', 'include/mc/world/terrain/surface.h',
        'src/world/terrain/terrain.c', 'include/mc/world/terrain/terrain.h',
        'src/world/biome/biome.c', 'include/mc/world/biome/biome.h',
        'src/world/noise/noise.c', 'include/mc/world/noise/noise.h',
        'src/world/noise/simplex.c', 'include/mc/world/noise/simplex.h',
        'src/core/java/java.c', 'include/mc/core/java/java.h',
        'src/core/math/math.c', 'include/mc/core/math/math.h',
    )
    return [root / name for name in names]


def request_data():
    cases = fixtures()
    require(len(cases) == 10, 'natural fixture cardinality')
    data = ''.join(case.request + '\n' for case in cases).encode('ascii')
    require(len(data) <= 4096 and hashlib.sha256(data).hexdigest() == REQUEST_SHA,
            'reviewed natural whitelist hash differs')
    require(data == ''.join(f'case\t{seed}\t{x}\t{z}\n' for seed, x, z in CASES).encode(),
            'natural request descriptors differ')
    return cases, data


def adapt_c_case(value, digest):
    object_keys(value, ('case', 'seed', 'chunk', 'chunk_seed', 'constructor', 'prefix', 'passes',
                       'restored_output_equal_first', 'reused_input_differs_from_prefix',
                       'changed_output_equal_its_input'), 'C case')
    require(type(value['passes']) is list and len(value['passes']) == 3, 'C pass cardinality')
    snapshots = [value['constructor'], value['prefix']]
    for item in value['passes']:
        require(type(item) is dict and 'after' in item, 'C pass has no after snapshot')
        snapshots.append(item['after'])
    names = {'generator_continuation', 'cave_continuation', 'noise82', 'q425', 'scratch',
             'climate', 'materials', 'table_unchanged', 'blocks'}
    for snapshot in snapshots:
        object_keys(snapshot, names, 'C snapshot')
        require(snapshot.pop('table_unchanged') is True, 'C measured table preservation marker is not true')
        snapshot['math_table_sha256_f32be'] = digest
    return value


def read_c_records(path, artifact_root):
    with Path(path).open('rb') as stream:
        header = stream.readline(MAX_LINE + 1)
        require(header.startswith(b'TABLE\t') and header.endswith(b'\n'), 'missing measured C TABLE header')
        payload = header[6:-1]
        if payload.endswith(b'\r'):
            payload = payload[:-1]
        require(len(payload) == 65536 * 8 and re.fullmatch(b'[0-9a-f]+', payload), 'C TABLE raw bits/cardinality')
        raw = bytes.fromhex(payload.decode('ascii'))
        digest = hashlib.sha256(raw).hexdigest()
        require(len(raw) == 65536 * 4 and digest == TABLE_SHA, 'measured C table differs from original accepted table')
        with (artifact_root / 'c-table.f32be').open('xb') as output:
            output.write(raw)
        index = 0
        while True:
            line = stream.readline(MAX_LINE + 1)
            if not line:
                break
            require(len(line) <= MAX_LINE and line.endswith(b'\n') and line.startswith(b'OK\t'),
                    'C output record exceeds bound or is incomplete')
            require(index < len(CASES), 'C output contains extra record')
            value = strict_json(line[3:].decode('utf-8', errors='strict'))
            value = adapt_c_case(value, digest)
            observation = validate_case(value, index + 1, CASES[index])
            yield value, observation
            index += 1
        require(index == len(CASES), 'C output record cardinality differs')


def case_counts(value):
    result = {'binary64': 0, 'permutation_entries': 0, 'generator_longs': 0, 'cave_longs': 0,
              'block_bytes': 0, 'material_bytes': 0, 'biome_identities': 0, 'snapshots': 5}
    snapshots = [value['constructor'], value['prefix']] + [item['after'] for item in value['passes']]
    for snapshot in snapshots:
        result['generator_longs'] += len(snapshot['generator_continuation'])
        result['cave_longs'] += len(snapshot['cave_continuation'] or [])
        for layers in snapshot['noise82'].values():
            for layer in layers:
                result['binary64'] += len(layer['offsets'])
                result['permutation_entries'] += len(layer['permutation'])
        for array in [snapshot['q425'], *snapshot['scratch'].values()]:
            if array is not None:
                result['binary64'] += array['length']
        if snapshot['climate'] is not None:
            for name in ('temperature', 'rainfall', 'auxiliary'):
                result['binary64'] += snapshot['climate'][name]['length']
            result['biome_identities'] += len(snapshot['climate']['biomes'])
            result['material_bytes'] += len(snapshot['materials']) // 2
        if snapshot['blocks'] is not None:
            result['block_bytes'] += snapshot['blocks']['length']
    result['block_bytes'] += sum(item['input_blocks']['length'] for item in value['passes'])
    return result


def save_records(records, side, artifact_root, compare_paths=()):
    paths, observations, counts = [], [], {}
    for index, (value, observation) in enumerate(records):
        path = artifact_root / f'{side}-case-{index + 1:03d}.json'
        with path.open('xb') as output:
            output.write(json.dumps(value, separators=(',', ':'), allow_nan=False).encode() + b'\n')
        paths.append(path)
        observations.append(observation)
        for name, count in case_counts(value).items():
            counts[name] = counts.get(name, 0) + count
        for label, references in compare_paths:
            require(index < len(references), label + ': original completed beyond reference prefix')
            require(references[index].stat().st_size <= MAX_LINE, 'retained reference case exceeds bound')
            expected = strict_json(references[index].read_bytes())
            difference = first_difference(value, expected)
            require(difference is None, f'{label} case {index + 1}: {difference}')
    return paths, observations, counts


def c_controls(path):
    require(path.stat().st_size <= MAX_STDERR, 'C stderr exceeds bound')
    lines = path.read_text(encoding='utf-8').splitlines()
    expected = []
    for index in range(1, len(CASES) + 1):
        expected.extend([CONTROL] * 3)
        expected.append(f'CASE_DONE case={index}')
    require(lines == expected, 'C measured preservation controls/cardinality differ')
    return {'control_count': len(CASES) * 3, 'completed': len(CASES)}


def checked_inputs(inputs, classes=None, request_before=None):
    _, errors = after_inputs(inputs)
    require(not errors, '; '.join(errors))
    if classes is not None:
        directory = Path(next(iter(classes.values()))['path']).parent
        require(classes_inventory(directory) == classes, 'fresh compiled class bytes changed')
    if request_before is not None:
        require(file_record(Path(request_before['path']), byte_limit=4096) == request_before,
                'generated request artifact changed')


def work_guard(root, work, world_base, protected):
    no_link_components(work)
    no_link_components(world_base)
    require(world_base.resolve().is_relative_to(root), 'world setup base leaves workspace')
    samefile_or_ancestor(work, protected)
    samefile_or_ancestor(world_base, protected)
    a, b = work.resolve(), world_base.resolve()
    require(a != b and a not in b.parents and b not in a.parents, 'report/world directories overlap')
    manifest = work / 'manifest.json'
    if os.path.lexists(manifest):
        no_link_components(manifest)
        require(manifest.is_file() and manifest.stat().st_nlink == 1, 'manifest is not independent regular storage')
        require(all(not os.path.samefile(manifest, path) for path in protected if path.exists()),
                'manifest aliases a supplied input')


def main(argv=None):
    helper = Path(__file__).resolve()
    root = helper.parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--client', type=Path, default=root / 'b1.7.3.jar')
    parser.add_argument('--server', type=Path, default=root / 'b1.7.3_server.jar')
    for name in ('java', 'javac', 'javap'):
        parser.add_argument('--' + name)
    parser.add_argument('--work-dir', type=Path, default=root / '.local/reference/caves')
    parser.add_argument('--tested-commit')
    args = parser.parse_args(argv)
    if args.tested_commit and not re.fullmatch('[0-9a-fA-F]{40}', args.tested_commit):
        parser.error('--tested-commit requires 40 hexadecimal characters')
    work = Path(os.path.abspath(args.work_dir))
    world_base = root / '.local/reference/caves-setup'
    executable = args.executable.resolve()
    jars = {'client': args.client.resolve(), 'server': args.server.resolve()}
    sources = source_paths(root)
    protected = {path.resolve() for path in [executable, *jars.values(), *sources, Path(sys.executable)]}
    for name in ('java', 'javac', 'javap'):
        value = getattr(args, name)
        if value and ('/' in value or '\\' in value):
            protected.add(Path(value).resolve())
    try:
        require(helper == (root / 'scripts/reference/caves/compare_caves.py').resolve(),
                'executing helper is not the attributed canonical source path')
        work_guard(root, work, world_base, protected)
    except (OSError, Failure) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    work.mkdir(parents=True, exist_ok=True)
    token = uuid.uuid4().hex
    artifact_root = work / ('run-' + token)
    artifact_root.mkdir()
    manifest = work / 'manifest.json'
    report = {'schema': 1, 'protocol': 1, 'status': 'running', 'run_id': token, 'mode': 'natural',
              'started_utc': utc_now(), 'workspace': str(root), 'artifact_root': str(artifact_root),
              'tested_commit': args.tested_commit.lower() if args.tested_commit else None,
              'commit_record': 'caller-supplied label; source/executable identity recorded separately',
              'scope': 'Natural original public cave scheduler after actual climate/base/surface; no finishing or save compatibility claim',
              'comparison_rules': ['Every complete C/client/server case compared by type, cardinality and every raw state value',
                                   'C TABLE carries actual 65536 f32be values; its computed digest must equal accepted original digest',
                                   'Only C table_unchanged:true maps to the computed digest field; no numerical normalization',
                                   'Restored input must reproduce blocks and post-call cave continuation; changed-input idempotence is an observation'],
              'limits': {'requests': 10, 'request_bytes': 4096, 'child_seconds': CHILD_SECONDS,
                         'stdout_live_bytes': MAX_STDOUT, 'stderr_live_bytes': MAX_STDERR, 'record_bytes': MAX_LINE,
                         'java_heap_mib': 512, 'world_inventory_entries': MAX_ENTRIES, 'world_inventory_bytes': MAX_WORLD_BYTES,
                         'world_inventory_depth': 64, 'world_inventory_path_chars': 4096,
                         'inventory_policy': 'Polling and post-exit validation, not a hard filesystem quota',
                         'process_policy': 'Direct-child timeout/kill and bounded pipe cleanup; no operating-system memory or process-tree quota'},
              'math_table_sha256': TABLE_SHA, 'request_sha256': REQUEST_SHA, 'sides': {},
              'errors': [], 'matched_records': {'client-to-C': 0, 'server-to-C': 0, 'client-to-server': 0},
              'python': sys.version, 'host': platform.platform(), 'completed': False}
    inputs, classes, request_before = {}, None, None
    unavailable, completed = None, False
    world_roots = {side: world_base / (token + '-' + side) for side in ('client', 'server')}
    report['owned_world_roots'] = {side: str(path) for side, path in world_roots.items()}

    def save():
        write_json(artifact_root / 'manifest.json', report)
        write_json(manifest, report)

    save()
    try:
        for path in [*sources, executable, *jars.values(), Path(sys.executable)]:
            if not path.is_file():
                raise Unavailable('required input file is missing: ' + str(path))
            current = file_record(path)
            inputs[current['path']] = current
            report['inputs_before'] = dict(inputs)
        for side, path in jars.items():
            require(inputs[str(path)]['sha256'] == JAR_HASHES[side], side + ': original JAR SHA-256 differs')
        tools = runtime_tools(args)
        for path in tools.values():
            current = file_record(path)
            inputs[current['path']] = current
        report['inputs_before'] = dict(inputs)
        report['input_fingerprint'] = input_fingerprint(inputs)
        # Tool resolution can reveal a path not available to the initial guard.
        work_guard(root, work, world_base, {Path(path) for path in inputs})
        cases, data = request_data()
        request_file = artifact_root / 'requests.tsv'
        with request_file.open('xb') as stream:
            stream.write(data)
        request_before = file_record(request_file, byte_limit=4096)
        report['request_artifact_before'] = request_before
        write_json(artifact_root / 'fixtures.json', [{'label': case.label, 'request': case.request,
                                                     'coverage': case.coverage} for case in cases])
        save()
        checked_inputs(inputs, request_before=request_before)
        tool_versions(tools, artifact_root, report)
        save()
        class_dir = artifact_root / 'classes'
        class_dir.mkdir()
        empty_cp, empty_sp = artifact_root / 'empty-classpath', artifact_root / 'empty-sourcepath'
        empty_cp.mkdir()
        empty_sp.mkdir()
        source = root / 'tests/reference/caves/CavesObservation.java'
        command = [str(tools['javac']), '-J-Xmx512m', '--release', '11', '-Xlint:all', '-Werror',
                   '-proc:none', '-implicit:none', '-classpath', str(empty_cp), '-sourcepath', str(empty_sp),
                   '-encoding', 'UTF-8', '-d', str(class_dir), str(source)]
        process = run_live(command, artifact_root / 'compile-stdout.txt', artifact_root / 'compile-stderr.txt',
                           artifact_root, seconds=30, stdout_limit=MAX_STDERR, stderr_limit=MAX_STDERR)
        report['compile'] = asdict(process)
        require(not process.stop_reason and process.returncode == 0, 'helper compilation failed')
        classes = classes_inventory(class_dir)
        report['classes_before'] = classes
        save()
        c_out, c_err = artifact_root / 'c-stdout.tsv', artifact_root / 'c-stderr.txt'
        checked_inputs(inputs, classes, request_before)
        process = run_live([str(executable)], c_out, c_err, artifact_root, stdin_path=request_file)
        report['sides']['C'] = {'process': asdict(process), 'status': 'running', 'records': 0}
        save()
        c_paths, observations, counts = save_records(read_c_records(c_out, artifact_root), 'C', artifact_root)
        report['sides']['C'].update(records=len(c_paths), observations=observations, counts=counts,
                                    cases=[file_record(path) for path in c_paths], controls=c_controls(c_err))
        require(not process.stop_reason and process.returncode == 0, 'C probe failed')
        report['sides']['C']['status'] = 'pass'
        report['c_table'] = file_record(artifact_root / 'c-table.f32be')
        save()
        no_link_components(world_base)
        world_base.mkdir(parents=True, exist_ok=True)
        client_paths = None
        for side in ('client', 'server'):
            checked_inputs(inputs, classes, request_before)
            owned = world_roots[side]
            no_link_components(owned)
            require(not os.path.lexists(owned) and owned.parent.resolve() == world_base.resolve(),
                    'original world root is not a new immediate child')
            command = [str(tools['java']), '-Xms32m', '-Xmx512m', '-Djava.awt.headless=true', '-cp', str(class_dir),
                       'CavesObservation', side, str(jars[side]), str(request_file), str(owned), str(root)]
            record = {'status': 'running', 'world_root': str(owned), 'command': command, 'records': 0}
            report['sides'][side] = record
            save()
            stdout, stderr = artifact_root / (side + '-stdout.tsv'), artifact_root / (side + '-stderr.txt')
            process = run_live(command, stdout, stderr, artifact_root, world_root=owned)
            record['process'] = asdict(process)
            record['inventory'] = world_inventory(owned, hashes=True)
            save()
            comparisons = [(side + '-to-C', c_paths)]
            if side == 'server':
                comparisons.append(('client-to-server', client_paths))
            paths, observations, counts = save_records(read_records(stdout, CASES, side), side, artifact_root, comparisons)
            record.update(records=len(paths), observations=observations, counts=counts,
                          cases=[file_record(path) for path in paths])
            report['matched_records'][side + '-to-C'] = len(paths)
            if side == 'server':
                report['matched_records']['client-to-server'] = len(paths)
            markers = lifecycle(stderr, CASES, owned)
            record['lifecycle'] = markers
            save()
            require(record['inventory']['complete'] and not record['inventory']['error'], side + ': final world inventory failed')
            if permitted_unavailable(process, markers, len(paths), CASES):
                record['status'] = 'unavailable'
                raise Unavailable(side + ': genuine original class/world setup unavailable')
            require(not process.stop_reason and process.returncode == 0, side + ': measured child failed')
            require(len(paths) == len(CASES) and markers['completed'] == len(CASES) and markers['phase'] == 'between' and
                    markers['control_count'] == 3 * len(CASES) and not markers['unavailable'], side + ': lifecycle/cardinality differs')
            record['status'] = 'pass'
            save()
            if side == 'client':
                client_paths = paths
        require(all(count == len(CASES) for count in report['matched_records'].values()), 'independent comparison cardinality')
        checked_inputs(inputs, classes, request_before)
        completed = True
    except Unavailable as error:
        unavailable = str(error)
    except KeyboardInterrupt:
        report['errors'].append('comparison interrupted')
    except Exception as error:
        report['errors'].append(type(error).__name__ + ': ' + str(error))
    finally:
        current, errors = after_inputs(inputs)
        report['inputs_after'] = current
        report['inputs_preserved'] = not errors
        report['errors'].extend(errors)
        if classes is not None:
            try:
                current = classes_inventory(artifact_root / 'classes')
                report['classes_after'] = current
                report['classes_preserved'] = current == classes
                if current != classes:
                    report['errors'].append('compiled class bytes changed')
            except (Failure, OSError) as error:
                report['classes_preserved'] = False
                report['errors'].append(str(error))
        if request_before is not None:
            try:
                current = file_record(Path(request_before['path']), byte_limit=4096)
                report['request_artifact_after'] = current
                require(current == request_before, 'generated request artifact changed')
            except (Failure, OSError) as error:
                report['errors'].append(str(error))
        for side, record in report['sides'].items():
            if record['status'] == 'running':
                record['status'] = 'fail'
            if side != 'C' and 'inventory' not in record:
                record['inventory'] = world_inventory(world_roots[side], hashes=True)
        report['completed'] = completed
        report['unavailable_reason'] = unavailable
        if not completed and not unavailable and not report['errors']:
            report['errors'].append('comparison did not complete all paired observations')
        report['status'] = 'fail' if report['errors'] else ('unavailable' if unavailable else 'pass')
        report['finished_utc'] = utc_now()
        artifacts = []
        for path in artifact_root.iterdir():
            if path.is_file() and path.name != 'manifest.json':
                try:
                    maximum = MAX_STDOUT if path.name.endswith('-stdout.tsv') else MAX_LINE
                    artifacts.append(file_record(path, byte_limit=maximum))
                except (Failure, OSError) as error:
                    report['errors'].append(str(error))
                    report['status'] = 'fail'
        report['artifacts'] = sorted(artifacts, key=lambda value: value['path'])
        save()
    print(json.dumps({'status': report['status'], 'manifest': str(manifest),
                      'manifest_sha256': sha256(manifest), 'matched_records': report['matched_records'],
                      'errors': report['errors'], 'unavailable_reason': unavailable}))
    return 1 if report['status'] == 'fail' else (77 if report['status'] == 'unavailable' else 0)


if __name__ == '__main__':
    sys.exit(main())
