#!/usr/bin/env python3
"""Audit original-only dedicated captures and emit observed constructor inputs.

The capture stays untouched. A nine-row tape is written only after independent
provenance, class-file, graph, typed NBT, disk and complete trace checks pass.
No Java process, native executable or native expected state is an audit input.
"""
from __future__ import annotations

import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import stat
import struct
import sys
import traceback
import zipfile

sys.dont_write_bytecode = True
if __package__:
    from . import semantic
else:
    import semantic

ROOT = Path(__file__).resolve().parents[5]


def release_version(path):
    """Bind the reported VM to the selected JDK, without a vendor/build pin."""
    raw = read(path, 65536).decode("utf-8")
    rows = re.findall(r'^JAVA_VERSION="([^"\r\n]+)"\r?$', raw, re.MULTILINE)
    need(len(rows) == 1 and re.fullmatch(r"17(?:\.[0-9]+)*(?:[+_-][A-Za-z0-9.+_-]+)?", rows[0]),
         "supported selected JDK17 release")
    return rows[0]


class RuntimeImage:
    """Read uncompressed modular image resources without launching jimage.

    JDK distributions may change metadata or packaging. A compressed runtime
    image is an unsupported evidence shape until its codec is audited here.
    """

    def __init__(self, path):
        self.path, self.before = Path(path), pin(path)
        self.stream = self.path.open("rb")
        header = self.stream.read(28)
        need(len(header) == 28, "complete selected runtime image header")
        order = "<" if header[:4] == bytes.fromhex("dadafeca") else ">"
        magic, version, flags, resources, table, locations, strings = struct.unpack(order + "7I", header)
        need(magic == 0xcafedada and version == 0x10000 and flags == 0,
             "supported modular runtime image header")
        need(0 < resources <= table <= 262144 and 0 < locations <= 16 * MIB and 0 < strings <= 16 * MIB,
             "bounded selected runtime image index")
        self.base = 28 + table * 8 + locations + strings
        need(self.base <= self.before["bytes"], "complete runtime image index")
        redirect = self.stream.read(table * 4)
        offsets = self.stream.read(table * 4)
        location_bytes, string_bytes = self.stream.read(locations), self.stream.read(strings)
        need(len(redirect) == len(offsets) == table * 4 and len(location_bytes) == locations and len(string_bytes) == strings,
             "complete selected runtime image tables")

        def string(index):
            need(0 <= index < len(string_bytes), "runtime image string offset")
            end = string_bytes.find(b"\0", index)
            need(index <= end <= index + 8192, "complete bounded runtime image string")
            return string_bytes[index:end].decode("utf-8")

        self.entries, used = {}, set()
        for (offset,) in struct.iter_unpack(order + "I", offsets):
            if not offset:
                continue
            need(offset not in used and 0 < offset < locations, "distinct runtime resource location")
            used.add(offset)
            attributes = {}
            while True:
                need(offset < locations, "complete runtime resource attributes")
                tag = location_bytes[offset]
                offset += 1
                if tag == 0:
                    break
                kind, size = tag >> 3, (tag & 7) + 1
                need(1 <= kind <= 7 and kind not in attributes and offset + size <= locations,
                     "known unique runtime resource attribute")
                attributes[kind] = int.from_bytes(location_bytes[offset:offset + size], "big")
                offset += size
            need(3 in attributes and 7 in attributes, "complete runtime resource extent")
            module = string(attributes.get(1, 0))
            parent = string(attributes.get(2, 0))
            basename = string(attributes[3])
            extension = string(attributes.get(4, 0))
            name = "/" + module + "/" + (parent + "/" if parent else "") + basename + ("." + extension if extension else "")
            start, compressed, size = attributes.get(5, 0), attributes.get(6, 0), attributes[7]
            need(name not in self.entries and 0 <= start <= self.before["bytes"] - self.base and
                 0 < (compressed or size) <= self.before["bytes"] - self.base - start,
                 "unique complete runtime resource")
            self.entries[name] = (self.base + start, compressed, size)
        equal(len(used), resources, "complete runtime image resource count")

    def get(self, name):
        suffix = "/" + name.replace(".", "/") + ".class"
        matches = [value for key, value in self.entries.items() if key.endswith(suffix)]
        need(len(matches) == 1, "one selected runtime class resource: " + name)
        start, compressed, size = matches[0]
        need(compressed == 0 and 0 < size <= 2 * MIB,
             "supported uncompressed runtime class evidence: " + name)
        self.stream.seek(start)
        raw = self.stream.read(size)
        equal(len(raw), size, "complete runtime class bytes")
        return raw

    def close(self):
        self.stream.close()
        equal(pin(self.path), self.before, "selected runtime image unchanged")


# Reviewed OpenJDK17 instruction programs, with symbolic constants rather than
# constant-pool indexes. These are semantic support profiles, not vendor, OS or
# distribution hashes. Metadata and pool ordering may vary. Unknown programs
# fail closed and need their own source/bytecode review before tape emission.
JDK_PROGRAMS = {
    ("java.util.Random", "<init>", "()V"): "041e6fd70bb4f65526a37852941d22d6c041357c3aadd24bb9dc5d53c08ca5b6",
    ("java.util.Random", "seedUniquifier", "()J"): "84fb79b56802c9897760180512a8b4610559e39cbe503ad49cc816778d9b0eec",
    ("java.util.Random", "<init>", "(J)V"): "c244ac0f01db0a0a1663b987426396288af650b73528825f934c243989b8a0da",
    ("java.util.Random", "initialScramble", "(J)J"): "65077d6b86e3e52459b572c18e0631f7c7758c5ca87ed29a49e5077229e6587e",
    ("java.util.Random", "setSeed", "(J)V"): "b34391c46113a8e171b54fe7daf70aa3f5073f5c80dcf6f9f0a69508fc160915",
    ("java.util.Random", "next", "(I)I"): "fe832555c855ed12a64ee5d917289724b26b59cd51437b9b84171a3387085e6f",
    ("java.util.Random", "nextBytes", "([B)V"): "f5a23aa853bcc91ef1c10b028f67ae2237ab012d6f7133882213cac5f873d4a0",
    ("java.util.Random", "nextInt", "()I"): "f5df0a2c7932d8a09e6e64804f2fe93da07324b8b8437932040de2ab694e0976",
    ("java.util.Random", "nextInt", "(I)I"): "ede7201b0bab531c7f0cd8db45578d06ef3efa78302374cee0df3603afc5d22f",
    ("java.util.Random", "nextLong", "()J"): "e9308519ac5fc920b3ece9fa5c7bfdc3d4c738b462541d322c733208cefd31e9",
    ("java.util.Random", "nextBoolean", "()Z"): "abbc9716ee2521b7aacd971142ea81e90b143f8e42fed6caa438f5d2acba4dc7",
    ("java.util.Random", "nextFloat", "()F"): "fb411105774e6e2d9099b231bf6c616c4f67714b1333f3bee0089ba0ef3055ce",
    ("java.util.Random", "nextDouble", "()D"): "efc40d0a859e8fbadc21eed953e212906bc0756ab3bf7b641f979f6325b52377",
    ("java.util.Random", "nextGaussian", "()D"): "5ed5508d204db8e16679b525f421e95915e36510d59750eed0afbe9ea72be5ee",
    ("java.util.Random", "writeObject", "(Ljava/io/ObjectOutputStream;)V"): "bd11332332e509950c7d3e714cfd8fba7fc5647907c7659df821bfab443bb819",
    ("java.util.Random", "<clinit>", "()V"): "94c5b5321f52331876096f75d5c92d4356a4fb052a651c247a645fe49212a03d",
    ("java.lang.Math", "random", "()D"): "9d832416313438f8fc3b1e433792b9f52f1888595277ea2563f363fad7419c21",
    ("java.lang.Math$RandomNumberGeneratorHolder", "<clinit>", "()V"): "d2ece15844794d3bd95f1a713fe01ef558f3f1f62c1fb38106247812677e3946",
}

SOURCE_PATHS = (
    "tests/reference/chunk/ChunkObservation.java",
    "tests/reference/cache/CacheCallbacksObservation.java",
    "tests/reference/cache/ChunkLifecycleObservation.java",
    "tests/reference/world/population/NaturalPopulationObservation.java",
    "tests/reference/world/population/dedicated/PopulationDedicatedObservation.java",
    "tests/reference/world/population/dedicated/PopulationDedicatedRandomObserver.java",
)
SOURCE_PINS = {
    "ChunkObservation.java": "213c7ffb98eb8aaf2aeca9e38d65ba528ab5f4f42e0fab4c07b341ffd93b899e",
    "CacheCallbacksObservation.java": "5aac2c1a0bf28e8d2f45d2b504eba66f27faa4990b537210ef066736eab7482f",
    "ChunkLifecycleObservation.java": "5be3fe26d8bd7e9ce47fbe8f1adb9718bbe204b7279a3f7ab023856088a4644e",
    "NaturalPopulationObservation.java": "f6bc4f17eb2132c2c22b961355b593e530f083de0b198b8936a24371a358f1ba",
    "PopulationDedicatedObservation.java": "f54a8dd01c924c3d5949a60fb9894c89cfec8c8f35e81a2f129068062ce80835",
    "PopulationDedicatedRandomObserver.java": "798b7e60665ec6f14cdd8ee5c5cd58459e83f93cce2d6876f5665db449a8bffd",
}
LEGACY_SOURCE_PINS = {
    **SOURCE_PINS,
    "PopulationDedicatedObservation.java": "69008516e301e7c9a96248140df40575bb57ca842b54894d318fbe300ce1482c",
    "PopulationDedicatedRandomObserver.java": "865608dc532533a3b03aaf6e93e1cfeca544f8f28c02942667e236b911a04699",
}
TARGET = "PopulationDedicatedObservation"
EXPERIMENT = TARGET + "$Experiment"
ORIGINAL = TARGET + "$Original"
RANDOM = "java.util.Random"
SYSTEM = "java.lang.System"
OP_DESC = "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;"
SNAPSHOT = "NaturalPopulationObservation$RandomFields"
ACTIONS = ("origin_c",) * 5 + (
    "world_time_1234", "full_world_save_0", "full_world_save_1", "protected_drop_0_0",
    "far_c", "far_c", "far_c", "far_c", "drop_16_0", "cache_maintenance",
    "reload_c_16_0", "repeated_c_16_0")
MIB = 1024 * 1024
MASK = (1 << 48) - 1
MASK64 = (1 << 64) - 1
MULTIPLIER = 0x5deece66d
INITIAL_U = 8682522807148012
U_MULTIPLIER = 1181783497276652981
TAPE_HEADER = b"minecraft-c-random-constructor-inputs-v1\n"
BANNER = re.compile(rb"^Listening for transport dt_socket at address: ([0-9]{1,5})\r?\n", re.MULTILINE)
HEX64 = re.compile(r"[0-9a-f]{16}\Z")
UNSIGNED = re.compile(r"(?:0|[1-9][0-9]*)\Z")
SIGNED = re.compile(r"(?:0|[1-9][0-9]*|-[1-9][0-9]*)\Z")


def need(value, message):
    if not value:
        raise ValueError(message)


def equal(actual, expected, message):
    todo = [(actual, expected)]
    while todo:
        left, right = todo.pop()
        need(type(left) is type(right), message + ": value type")
        if type(left) is dict:
            need(left.keys() == right.keys(), message + ": keys")
            todo.extend((left[key], right[key]) for key in left)
        elif type(left) in (list, tuple):
            need(len(left) == len(right), message + ": length")
            todo.extend(zip(left, right))
        else:
            need(left == right, message)


def keys(value, names, message):
    need(type(value) is dict and set(value) == set(names), message + ": complete keys")


def number(value, low=0, high=(1 << 63) - 1, message="decimal"):
    need(type(value) is str and len(value) <= 24 and SIGNED.fullmatch(value), message + ": canonical decimal")
    result = int(value)
    need(low <= result <= high, message + ": range")
    return result


def bits(value, message="raw long"):
    need(type(value) is str and HEX64.fullmatch(value), message + ": canonical 64 bits")
    return int(value, 16)


def signed64(value):
    return value if value < (1 << 63) else value - (1 << 64)


def signed32(value):
    value &= (1 << 32) - 1
    return value if value < (1 << 31) else value - (1 << 32)


def ordinary(path, directory=False):
    path = Path(path).absolute()
    for item in (path, *path.parents):
        info = item.lstat()
        need(not stat.S_ISLNK(info.st_mode) and not getattr(info, "st_file_attributes", 0) & 0x400,
             "ordinary non-reparse path: " + str(item))
    info = path.stat()
    need(stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode), "ordinary path kind")
    return info


def pin(path):
    before = ordinary(path)
    need(0 <= before.st_size <= 512 * MIB, "selected file byte bound")
    digest, count = hashlib.sha256(), 0
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(MIB), b""):
            count += len(block)
            need(count <= 512 * MIB, "stable file byte bound")
            digest.update(block)
    after = ordinary(path)
    equal((before.st_size, before.st_mtime_ns, before.st_ino),
          (after.st_size, after.st_mtime_ns, after.st_ino), "file stable during complete read")
    equal(count, before.st_size, "complete file read")
    return {"bytes": count, "sha256": digest.hexdigest()}


def read(path, limit=64 * MIB):
    need(ordinary(path).st_size <= limit, "bounded retained input: " + str(path))
    with Path(path).open("rb") as stream:
        raw = stream.read(limit + 1)
    need(len(raw) <= limit, "bounded complete retained read")
    equal({"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}, pin(path), "stable retained bytes")
    return raw


def inventory(root):
    ordinary(root, True)
    result, total = {".": {"kind": "directory"}}, 0
    for path in sorted(root.rglob("*")):
        need(len(result) < 4096, "inventory entry bound")
        if path.is_dir():
            ordinary(path, True)
            result[path.relative_to(root).as_posix()] = {"kind": "directory"}
        else:
            value = pin(path)
            total += value["bytes"]
            need(total <= 256 * MIB, "inventory byte bound")
            result[path.relative_to(root).as_posix()] = {"kind": "file", **value}
    return result


def git_status(raw):
    need(not raw or raw.endswith(b"\0"), "complete Git status")
    entries, index, tracked = raw.split(b"\0")[:-1] if raw else [], 0, False
    while index < len(entries):
        row = entries[index]
        need(len(row) >= 4 and row[2:3] == b" ", "Git status record")
        tracked |= row[:2] != b"??"
        index += 1
        if b"R" in row[:2] or b"C" in row[:2]:
            need(index < len(entries) and entries[index], "Git rename/copy second path")
            index += 1
    return tracked


def source_context(report, capture, paths, commands):
    before = report["source_before"]
    equal(report["source_after"], before, "actual source context unchanged through capture")
    exported = before.get("source_export")
    need(type(exported) is bool, "typed exported source context")
    names = {"source_head", "source_dirty", "source_export", "source_context_body_sha256"}
    names.add("source_export_manifest" if exported else "source_tree")
    keys(before, names, "complete source context")
    need(type(before["source_head"]) is str and re.fullmatch(r"[0-9a-f]{40}", before["source_head"]) and
         type(before["source_dirty"]) is bool, "immutable source object and actual dirty state")
    for name, value in before.items():
        equal(report[name], value, "outer report source context: " + name)
    selected = paths["source_context_support"]
    tree = ast.parse(read(selected, 2 * MIB), filename=str(selected))
    bodies = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "source_context"]
    need(len(bodies) == 1, "one selected source attribution implementation")
    segment = ast.get_source_segment(read(selected, 2 * MIB).decode("utf-8"), bodies[0]).encode("utf-8")
    equal(hashlib.sha256(segment).hexdigest(), before["source_context_body_sha256"],
          "recorded source attribution body matches selected public implementation")
    root = Path(report["process_policy"]["cwd"])
    if exported:
        need("git" not in paths and "source_export_manifest" in paths, "export source attribution input closure")
        manifest = semantic.strict_json(read(paths["source_export_manifest"]))
        keys(manifest, ("schema", "source_head", "source_dirty", "files"), "complete exported source manifest")
        equal(manifest["schema"], 1, "supported exported source manifest")
        equal((manifest["source_head"], manifest["source_dirty"]),
              (before["source_head"], before["source_dirty"]), "actual exported source attribution")
        equal(pin(paths["source_export_manifest"]), before["source_export_manifest"], "complete selected source manifest")
        need(type(manifest["files"]) is dict and 1 <= len(manifest["files"]) <= 10000,
             "complete bounded exported source file set")
        for name, expected in manifest["files"].items():
            relative_path(name)
            equal(pin(root / name), expected, "complete exported source file: " + name)
        for role in ("runner", "process_support", "source_context_support", *SOURCE_PINS):
            relative = paths[role].relative_to(root).as_posix()
            need(relative in manifest["files"], "selected source belongs to attributed export: " + role)
            equal(pin(paths[role]), manifest["files"][relative], "selected exported source binding")
    else:
        need("git" in paths and type(before["source_tree"]) is str and
             re.fullmatch(r"[0-9a-f]{40}", before["source_tree"]), "actual checkout tree object")
        git = paths["git"]
        for suffix in ("before", "after"):
            directory = "source-" + suffix
            prefix = capture / directory
            equal(read(prefix / "git-head.stdout"), (before["source_head"] + "\n").encode("ascii"),
                  "complete source context HEAD receipt")
            equal(read(prefix / "git-tree.stdout"), (before["source_tree"] + "\n").encode("ascii"),
                  "complete source context tree receipt")
            status = read(prefix / "git-status.stdout")
            equal(bool(status.strip()), before["source_dirty"], "complete source status dirty attribution")
            for role, args in (("head", ["rev-parse", "HEAD"]),
                               ("status", ["status", "--porcelain", "--untracked-files=all"]),
                               ("tree", ["rev-parse", before["source_head"] + "^{tree}"])):
                commands[directory + "-git-" + role] = ([git, "-C", root, *args], 60,
                                                        directory + "/git-" + role)
    tested = report["tested_commit"]
    need(tested is None or type(tested) is str and re.fullmatch(r"[0-9a-f]{40}", tested), "complete tested source object")
    need(tested is None or tested == before["source_head"] and before["source_dirty"] is False,
         "tested commit requires its actual matching clean source")
    return before


def relative_path(value):
    need(type(value) is str and len(value) <= 1024 and "\\" not in value and ":" not in value and
         all(part not in ("", ".", "..") for part in value.split("/")), "ordinary retained relative path")
    return value


def validate_processes(report, capture, paths, commands, order):
    processes = report["processes"]
    need(type(processes) is list and len(processes) == len(order), "all actual owned child receipts")
    equal([process.get("name") for process in processes], order, "complete expected child order")
    common = """name command cwd stdin pid exit_code reaped kill_requested timed_out output_limited natural_exit
        stdout_path stderr_path stdout_limit stderr_limit started_unix_ns seconds ended_unix_ns stdout stderr""".split()
    previous_end, pair_end = 0, 0
    for process in processes:
        name = process["name"]
        command, deadline, stream_prefix = commands[name]
        deadline_key = "pair_deadline_seconds" if name == "target" else "deadline_seconds"
        keys(process, (*common, deadline_key), "complete actual child receipt: " + name)
        equal(process["command"], [str(part) for part in command], "exact actual child command: " + name)
        equal((process["cwd"], process["stdin"]), (report["process_policy"]["cwd"], "DEVNULL"), "child cwd/stdin")
        for key in ("reaped", "natural_exit"):
            need(process[key] is True, "natural reaped child: " + name)
        for key in ("kill_requested", "timed_out", "output_limited"):
            need(process[key] is False, "successful bounded child: " + name)
        need(type(process["pid"]) is int and process["pid"] > 0 and
             type(process["exit_code"]) is int and process["exit_code"] == 0, "successful actual child identity")
        equal(process[deadline_key], deadline, "actual child deadline")
        need(type(process["seconds"]) in (int, float) and 0 <= process["seconds"] <= deadline,
             "actual bounded child duration")
        start, end = process["started_unix_ns"], process["ended_unix_ns"]
        need(type(start) is int and type(end) is int and 0 < start <= end <= (1 << 63) - 1,
             "actual child clock endpoints")
        if name == "observer":
            need(start >= previous_start and end <= pair_end, "observer lifetime inside owned target receipt interval")
            previous_end = pair_end
        else:
            need(previous_end <= start, "ordered child lifetimes outside paired debugger processes")
            previous_end = end
            if name == "target":
                previous_start, pair_end = start, end
        for stream in ("stdout", "stderr"):
            relative = stream_prefix + "." + stream
            equal(process[stream + "_path"], relative, "exact complete retained stream path")
            relative_path(relative)
            bound = (MIB if stream == "stdout" else 4 * MIB) if name == "target" else \
                (16 * MIB if stream == "stdout" else 4 * MIB) if name == "observer" else 4 * MIB
            equal(process[stream + "_limit"], bound, "per-stream actual byte threshold")
            raw = read(capture / relative, bound)
            equal(pin(capture / relative), process[stream], "complete stream bytes and digest")
            if stream == "stderr" and name not in ("target", "java-version"):
                equal(raw, b"", "complete expected empty stderr")
    for name in ("compile-target", "compile-observer"):
        equal(read(capture / (name + ".stdout")), b"", "complete compiler stdout")
    version = release_version(paths["release"])
    equal(read(capture / "javac-version.stdout").replace(b"\r\n", b"\n"),
          ("javac " + version + "\n").encode("ascii"), "selected compiler version receipt")
    equal(read(capture / "java-version.stdout"), b"", "complete runtime version stdout")
    need(re.search(rb'(?:openjdk|java) version "' + re.escape(version.encode("ascii")) + rb'" ',
                   read(capture / "java-version.stderr")), "selected runtime version receipt")
    if "git" in paths:
        need(re.fullmatch(rb"git version [^\r\n]+\r?\n", read(capture / "git-version.stdout")), "complete actual Git version")


def provenance(capture, *, legacy=False):
    report = semantic.strict_json(read(capture / "report.json", 4 * MIB))
    common = """schema scope profile source_head source_dirty tested_commit native_comparison qualification qualified
        qualification_complete semantic_audit_complete mechanical_capture_only jdi_observed_unqualified passed inputs_unchanged
        processes copied_sources process_policy provenance_limit interpretation_limit inputs_before inputs_after python_version
        python_runtime runner_process source_before source_after retained_sources_before_compile retained_support_before_compile
        classes_before_pair classes_after_pair debugger_pair observer_trace original_output final_disk_inventory
        final_disk_inventory_after_reaping retained_files_before_report retention_self_exclusion seconds""".split()
    legacy_extra = """source_tree tracked_checkout_dirty custom_observer_uncommitted source_dirty_reason""".split()
    public_extra = ["copied_support", "payload_source_context", "source_export", "source_context_body_sha256",
                    "source_export_manifest" if report.get("source_export") else "source_tree"]
    keys(report, (*common, *(legacy_extra if legacy else public_extra)), "complete successful mechanical capture report")
    equal((report["schema"], report["scope"], report["profile"]),
          (1 if legacy else 2, "original-only instrumented dedicated constructor/Random research" if legacy else
           "original-only instrumented dedicated capture", "dedicated-natural-unload"), "explicit mechanical capture schema")
    for name in ("mechanical_capture_only", "jdi_observed_unqualified", "passed", "inputs_unchanged"):
        need(report[name] is True, "honest successful mechanical flag: " + name)
    for name in ("native_comparison", "qualification", "qualified", "qualification_complete", "semantic_audit_complete"):
        need(report[name] is False, "honest unqualified mechanical flag: " + name)
    if legacy:
        need(report["source_dirty"] is True and report["custom_observer_uncommitted"] is True and
             report["tested_commit"] is None, "legacy research remains dirty and unattributed to a tested commit")
    before = inventory(capture)
    declared = report["retained_files_before_report"]
    need(type(declared) is dict and "report.json" not in declared, "capture report sole inventory self-exclusion")
    equal(set(before), set(declared) | {"report.json"}, "complete retained inventory closure")
    for name, expected in declared.items():
        equal(before[name], expected, "complete retained capture entry: " + name)
    equal(report["inputs_before"], report["inputs_after"], "all selected input pins unchanged")
    paths = {}
    for role, selected in report["inputs_before"].items():
        keys(selected, ("path", "bytes", "sha256"), "complete selected input role")
        need(type(role) is str and type(selected["path"]) is str, "selected input role/path strings")
        path = Path(selected["path"])
        need(path.is_absolute() and not path.is_relative_to(capture), "absolute selected input outside capture")
        equal(pin(path), {key: selected[key] for key in ("bytes", "sha256")}, "actual selected input still pinned: " + role)
        paths[role] = path
    required = {"runner", "python", "server_jar", "java", "javac", "modules", "jvm", "java_native", "release",
                "source_archive", "jdwp", "dt_socket", *SOURCE_PINS,
                *["jdk/jmods/" + module + ".jmod" for module in ("java.base", "jdk.compiler", "jdk.jdi")]}
    required.update(("git", "process_helper", "pair_protocol", "target_notes", "random_notes") if legacy else
                    ("process_support", "source_context_support"))
    need(required <= paths.keys(), "complete selected source/JDK/tool role closure")
    windows_jdk = paths["java"].name.endswith(".exe")
    jdk_root = paths["java"].parent.parent
    support = ("bin/jli.dll", "bin/zip.dll", "bin/verify.dll", "bin/net.dll", "bin/nio.dll",
               "bin/attach.dll", "bin/jimage.dll") if windows_jdk else (
                   "lib/libjli.so", "lib/libzip.so", "lib/libverify.so", "lib/libnet.so", "lib/libnio.so",
                   "lib/libattach.so", "lib/libjimage.so")
    required.update("jdk/" + name for name in (*support, "lib/ct.sym", "lib/jrt-fs.jar"))
    if windows_jdk:
        required.update("jdk/" + name for name in (
            "bin/msvcp140.dll", "bin/vcruntime140.dll", "bin/vcruntime140_1.dll", "bin/ucrtbase.dll")
                        if (jdk_root / name).is_file())
    python_libraries = {"python/" + path.name: path.resolve(strict=True)
                        for pattern in ("python*.dll", "vcruntime*.dll")
                        for path in paths["python"].parent.glob(pattern)}
    required.update(python_libraries)
    need(required <= paths.keys(), "complete selected platform JDK/Python support closure")
    for role, path in python_libraries.items():
        equal(paths[role], path, "selected Python support library path: " + role)
    allowed = required
    if not legacy:
        allowed |= {"git", "source_export_manifest", "actual_entry_source"}
    need(paths.keys() <= allowed, "no undeclared selected input role")
    equal(pin(paths["server_jar"])["sha256"], semantic.PIN, "actual supplied original server archive")
    for name, digest in (LEGACY_SOURCE_PINS if legacy else SOURCE_PINS).items():
        equal(pin(paths[name])["sha256"], digest, "reviewed selected original observation source: " + name)
    root = Path(report["process_policy"]["cwd"])
    need(root.is_absolute(), "absolute actual process source root")
    java, javac = paths["java"], paths["javac"]
    need(java.parent == javac.parent and java.name in ("java", "java.exe") and
         javac.name == ("javac.exe" if java.name.endswith(".exe") else "javac"), "one selected full JDK17")
    jdk = java.parent.parent
    selected_jdk = {"release": jdk / "release", "modules": jdk / "lib/modules", "source_archive": jdk / "lib/src.zip"}
    native = {"jvm": "bin/server/jvm.dll", "java_native": "bin/java.dll", "jdwp": "bin/jdwp.dll",
              "dt_socket": "bin/dt_socket.dll"} if java.name.endswith(".exe") else {
                  "jvm": "lib/server/libjvm.so", "java_native": "lib/libjava.so", "jdwp": "lib/libjdwp.so",
                  "dt_socket": "lib/libdt_socket.so"}
    selected_jdk.update({key: jdk / name for key, name in native.items()})
    selected_jdk.update({role: jdk / role[4:] for role in paths if role.startswith("jdk/")})
    for role, expected in selected_jdk.items():
        equal(paths[role], expected.resolve(strict=True), "selected JDK file belongs to actual runtime root: " + role)
    if not legacy:
        expected_sources = {Path(name).name: root / name for name in SOURCE_PATHS}
        expected_support = {"runner": root / "scripts/reference/world/population/dedicated/capture.py",
                            "process_support": root / "scripts/reference/world/population/dedicated/process.py",
                            "source_context_support": root / "scripts/reference/world/population/compare_natural.py"}
        for role, expected in {**expected_sources, **expected_support}.items():
            equal(paths[role], expected.resolve(strict=True), "selected public source path: " + role)
            equal(pin(paths[role]), pin(ROOT / expected.relative_to(root)), "selected reviewed public source bytes: " + role)
    need(type(report["seconds"]) in (int, float) and 0 < report["seconds"] < 1800, "bounded overall capture duration")
    need(type(report["python_version"]) is str and 0 < len(report["python_version"]) <= 2048, "actual Python version receipt")
    keys(report["python_runtime"], ("executable", "prefix", "base_prefix"), "complete selected Python runtime")
    equal(Path(report["python_runtime"]["executable"]).resolve(strict=True), paths["python"], "actual selected Python executable")
    invocation = report["runner_process"]
    keys(invocation, ("pid", "parent_pid", "argv", "cwd", *([] if legacy else ["entry_source_role"])),
         "actual capture runner invocation")
    need(all(type(invocation[name]) is int and invocation[name] > 0 for name in ("pid", "parent_pid")) and
         type(invocation["argv"]) is list and invocation["argv"] and
         all(type(part) is str and 0 < len(part) <= 32768 for part in invocation["argv"]), "complete actual runner identity/argv")
    need(Path(invocation["cwd"]).is_absolute(), "actual absolute runner working directory")
    if "actual_entry_source" in paths:
        equal((Path(invocation["cwd"]) / invocation["argv"][0]).resolve(strict=True), paths["actual_entry_source"],
              "actual invocation entry source binding")
    if not legacy:
        equal(invocation["entry_source_role"], "actual_entry_source" if "actual_entry_source" in paths else None,
              "actual capture invocation source role attribution")
    commands = {}
    if legacy:
        legacy_source_context(report, capture, paths, commands)
        initial = ["git-head-before", "git-tree-before", "git-status-before", "git-version", "java-version", "javac-version"]
        final = ["git-head-after", "git-tree-after", "git-status-after"]
    else:
        context = source_context(report, capture, paths, commands)
        initial = [] if context["source_export"] else ["source-before-git-head", "source-before-git-status", "source-before-git-tree"]
        initial += ["java-version", "javac-version"] + (["git-version"] if "git" in paths else [])
        final = [] if context["source_export"] else ["source-after-git-head", "source-after-git-status", "source-after-git-tree"]
    for name, command in (("java-version", [java, "-version"]), ("javac-version", [javac, "-version"])):
        commands[name] = (command, 60, name)
    if "git" in paths:
        commands["git-version"] = ([paths["git"], "--version"], 60, "git-version")
    source_dir, classes = capture / "sources", capture / "classes"
    source_files = [paths[Path(name).name] for name in SOURCE_PATHS]
    journals = verify_sources_and_support(report, capture, paths, root, source_files, commands, legacy)
    commands.update({
        "compile-target": ([javac, "-J-Xmx256m", "--release", "17", "-encoding", "UTF-8", "-classpath", classes,
                            "-sourcepath", source_dir, "-d", classes, *[source_dir / value.name for value in source_files[:-1]]], 120, "compile-target"),
        "compile-observer": ([javac, "-J-Xmx256m", "--add-modules", "jdk.jdi", "--release", "17", "-g", "-encoding", "UTF-8",
                              "-classpath", classes, "-sourcepath", source_dir, "-d", classes, source_dir / source_files[-1].name], 120, "compile-observer"),
        "target": ([java, "-Xint", "-Xmx512m", "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                    "--add-opens", "java.base/java.util=ALL-UNNAMED", "-cp", classes, TARGET, paths["server_jar"], "server",
                    "0000000000000017", capture / "original-parent", capture / "original.json"], 210, "target"),
        "observer": ([java, "--add-modules", "jdk.jdi", "-Xmx256m", "-cp", classes, "PopulationDedicatedRandomObserver",
                      str(report["debugger_pair"]["port"])], 180, "observer"),
    })
    validate_processes(report, capture, paths, commands, initial + journals + ["compile-target", "compile-observer", "target", "observer"] + final)
    verify_policy_pair_classes(report, capture, root)
    return report, paths, before


def legacy_source_context(report, capture, paths, commands):
    """Validate historical mechanical receipts without borrowing qualification."""
    root, git = Path(report["process_policy"]["cwd"]), paths["git"]
    for name in ("source_head", "source_tree"):
        need(type(report[name]) is str and re.fullmatch(r"[0-9a-f]{40}", report[name]), "legacy immutable source object")
    for suffix in ("before", "after"):
        for role, args in (("head", ["rev-parse", "HEAD"]),
                           ("tree", ["rev-parse", report["source_head"] + "^{tree}"]),
                           ("status", ["status", "--porcelain=v1", "-z", "--untracked-files=all"])):
            name = "git-" + role + "-" + suffix
            commands[name] = ([git, "-C", root, *args], 60, name)
        equal(read(capture / ("git-head-" + suffix + ".stdout")), (report["source_head"] + "\n").encode("ascii"),
              "complete legacy HEAD receipt")
        equal(read(capture / ("git-tree-" + suffix + ".stdout")), (report["source_tree"] + "\n").encode("ascii"),
              "complete legacy tree receipt")
        status = capture / ("git-status-" + suffix + ".stdout")
        equal(report["source_" + suffix], {"head": report["source_head"], "tree": report["source_tree"],
            "tracked_checkout_dirty": git_status(read(status)), "status": pin(status)}, "independent actual legacy source context")
    equal(report["tracked_checkout_dirty"], report["source_before"]["tracked_checkout_dirty"],
          "honest legacy tracked checkout attribution")


def verify_sources_and_support(report, capture, paths, root, source_files, commands, legacy):
    source_dir, support_dir = capture / "sources", capture / "support"
    equal(set(inventory(source_dir)), {".", *SOURCE_PINS}, "only six compiled source copies")
    keys(report["copied_sources"], SOURCE_PINS, "complete compiled source copy map")
    journals = []
    for index, source in enumerate(source_files):
        target = source_dir / source.name
        equal(pin(target), pin(source), "exact selected compiled Java source copy")
        tracked = index < 4 if legacy else bool(report["tested_commit"] and "git" in paths)
        journal = "git-blob-" + source.stem if tracked else None
        equal(report["copied_sources"][source.name], {"path": "sources/" + source.name, **pin(target),
            "tracked_exact_blob": tracked, "git_blob_process": journal}, "actual compiled source attribution")
        if journal:
            commands[journal] = ([paths["git"], "-C", root, "show", report["source_head"] + ":" +
                                  source.relative_to(root).as_posix()], 60, journal)
            equal(read(capture / (journal + ".stdout")), read(target), "complete actual committed Java source blob")
            journals.append(journal)
    if legacy:
        support_roles = ("runner", "process_helper", "pair_protocol", "target_notes", "random_notes")
        support_pins = {
            "runner": "d23ad7cbbfd8ae565d9f32a144618a6222f325bcb2a8a4e5209498dc184e1518",
            "process_helper": "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c",
            "pair_protocol": "6cfd3b0a597ea7154e46ab7cac0453d109242d486f1b58cb92f88c26c556a8cf",
            "target_notes": "20e151115ab13667515e2484fdff7db7235de04c0e31f587aa2a4c7f44a3f022",
            "random_notes": "bbee7dc9727d471d12b1193021246f8411b4f8056b6b973b0514b01f38119a90",
        }
        for role, digest in support_pins.items():
            equal(pin(paths[role])["sha256"], digest, "reviewed historical mechanical support source")
    else:
        support_roles = ("runner", "process_support", "source_context_support", *[
            role for role in ("actual_entry_source", "source_export_manifest") if role in paths])
        keys(report["copied_support"], support_roles, "complete selected support source copy map")
    support_names = set()
    for role in support_roles:
        name = role + paths[role].suffix
        support_names.add(name)
        target = support_dir / name
        equal(pin(target), pin(paths[role]), "complete exact retained mechanical support source")
        if legacy and role in ("process_helper", "pair_protocol"):
            journal = "git-blob-" + role
            commands[journal] = ([paths["git"], "-C", root, "show", report["source_head"] + ":" +
                                  paths[role].relative_to(root).as_posix()], 60, journal)
            equal(read(capture / (journal + ".stdout")), read(target), "complete committed process support blob")
            journals.append(journal)
        elif not legacy:
            equal(report["copied_support"][role], {"path": "support/" + name, **pin(target)}, "retained support attribution")
    equal(set(inventory(support_dir)), {".", *support_names}, "only exact selected support copies")
    equal(inventory(source_dir), report["retained_sources_before_compile"], "compiled source copies unchanged")
    equal(inventory(support_dir), report["retained_support_before_compile"], "retained process support unchanged")
    return journals


def verify_policy_pair_classes(report, capture, root):
    equal(report["process_policy"], {
        "stdin": "DEVNULL", "cwd": str(root), "pair_outer_seconds": 210, "pair_cleanup_reserve_seconds": 10,
        "observer_seconds": 180, "compile_seconds": 120, "ordinary_child_seconds": 60, "cleanup_seconds_per_child": 5,
        "target_stdout_bytes": MIB, "target_stderr_bytes": 4 * MIB,
        "observer_stdout_bytes": 16 * MIB, "observer_stderr_bytes": 4 * MIB,
        "threshold_policy": "Poll all streams; reject overshoot and retain complete produced streams without truncation.",
        "java_environment_options": {name: "absent" for name in (
            "JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "_JAVA_OPTIONS", "CLASSPATH")},
        "git_environment": {"GIT_OPTIONAL_LOCKS": "0", "GIT_TERMINAL_PROMPT": "0", "GIT_PAGER": "cat"},
    }, "complete actual bounded process policy")
    classes = capture / "classes"
    actual = inventory(classes)
    equal(actual, report["classes_before_pair"], "complete retained compiled class inventory")
    equal(report["classes_before_pair"], report["classes_after_pair"], "compiled classes unchanged through process pair")
    observer = {"PopulationDedicatedRandomObserver" + suffix + ".class" for suffix in (
        "", "$Rejected", "$Trace", "$Held", "$State", "$ConstructorCall", "$Draw", "$Session")}
    equal(set(actual), {".", *semantic.HELPERS, *observer}, "only exact target and observer compiler classes")
    for name in semantic.HELPERS | observer:
        value = semantic.Classes.parse(read(classes / name, 2 * MIB))
        equal((value["name"], value["major"]), (name[:-6], 61), "actual selected release17 helper class")
    pair = report["debugger_pair"]
    keys(pair, ("host", "port", "transport", "suspend", "seconds"), "complete actual debugger pair")
    equal((pair["host"], pair["transport"], pair["suspend"]), ("127.0.0.1", "dt_socket", "y"), "owned suspended loopback target")
    need(type(pair["port"]) is int and 1 <= pair["port"] <= 65535 and
         type(pair["seconds"]) in (int, float) and 0 < pair["seconds"] <= 210, "bounded actual debugger pair")
    stdout, stderr = read(capture / "target.stdout"), read(capture / "target.stderr")
    banners = BANNER.findall(stdout) + BANNER.findall(stderr)
    need(len(banners) == 1 and int(banners[0]) == pair["port"], "one complete actual dynamic listener banner")
    equal(BANNER.sub(b"", stdout), b"", "complete target stdout contains only native debugger listener banner")
    equal(BANNER.sub(b"", stderr).replace(b"\r\n", b"\n"), b"16 achievements\n151 recipes\n", "complete original bootstrap stderr")
    equal(report["final_disk_inventory"], report["final_disk_inventory_after_reaping"], "disk stable after both children reap")
    equal(inventory(capture / "original-parent"), report["final_disk_inventory"], "complete actual final disk tree")
    equal(report["retention_self_exclusion"],
          "This inventory precedes the exclusive final report.json write; that sole report file is excluded.",
          "capture report sole explicit retention self-exclusion")


class Catalog:
    """Read class metadata and callsite constants without javap or a JVM."""
    def __init__(self, old, capture, paths):
        self.old, self.capture = old, capture
        self.original = zipfile.ZipFile(paths["server_jar"])
        self.jdk_version = release_version(paths["release"])
        self.runtime = RuntimeImage(paths["modules"])
        self.trace_protocol = 2
        self.semantic_shapes = {}
        self.jmods = [zipfile.ZipFile(paths["jdk/" + name]) for name in (
            "jmods/java.base.jmod", "jmods/jdk.compiler.jmod", "jmods/jdk.jdi.jmod")]
        for archive in self.jmods:
            entries = archive.infolist()
            need(len(entries) == len({entry.filename for entry in entries}) <= 65536,
                 "unique bounded selected JDK module archive")
        self.cache, self.generated = {}, {}
        self.game = {name.filename[:-6].replace("/", ".") for name in self.original.infolist() if name.filename.endswith(".class")}

    def close(self):
        self.runtime.close()
        self.original.close()
        for archive in self.jmods:
            archive.close()

    def get(self, name):
        if name in self.cache:
            return self.cache[name]
        relative = name.replace(".", "/") + ".class"
        if name in self.game:
            raw = self.original.read(relative)
            origin = "original"
        elif (self.capture / "classes" / relative).is_file():
            raw = read(self.capture / "classes" / relative, 2 * MIB)
            origin = "helper"
        else:
            found = [archive for archive in self.jmods if "classes/" + relative in archive.namelist()]
            need(len(found) == 1, "one pinned class-file origin: " + name)
            entry = found[0].getinfo("classes/" + relative)
            need(0 < entry.file_size <= 2 * MIB, "bounded pinned JDK class")
            raw, origin = found[0].read(entry), "jdk"
            equal(self.runtime.get(name), raw, "selected running image and jmod class bytes: " + name)
        need(len(raw) <= 2 * MIB, "bounded class bytes")
        value = self.old.Classes.parse(raw)
        equal(value["name"], name, "exact class-file declaring type")
        if origin == "jdk":
            equal(value["major"], 61, "selected JDK17 class-file version")
        value["constants"] = self.constants(raw)
        value["origin"] = origin
        for method in value["methods"]:
            if "code" in method:
                method["pcs"] = self.instructions(method["code"])
        self.cache[name] = value
        return value

    def constants(self, raw):
        reader = self.old.Binary(raw)
        reader.take(8)
        size = reader.u2()
        pool, index = [None] * size, 1
        while index < size:
            tag = reader.number(">B")
            if tag == 1:
                value = self.old.mutf(reader.take(reader.u2()))
            elif tag in (3, 4):
                value = reader.take(4)
            elif tag in (5, 6):
                pool[index] = (tag, reader.take(8))
                index += 2
                continue
            elif tag in (7, 8, 16, 19, 20):
                value = reader.u2()
            elif tag in (9, 10, 11, 12, 17, 18):
                value = (reader.u2(), reader.u2())
            elif tag == 15:
                value = (reader.number(">B"), reader.u2())
            else:
                raise ValueError("unsupported class constant")
            pool[index] = (tag, value)
            index += 1
        return pool

    @staticmethod
    def instructions(code):
        # Fixed operand lengths; switch/wide instructions are decoded separately.
        lengths = {opcode: 1 for opcode in range(0xca)}
        for opcode in (0x10, 0x12, *range(0x15, 0x1a), *range(0x36, 0x3b), 0xa9, 0xbc):
            lengths[opcode] = 2
        for opcode in (0x11, 0x13, 0x14, 0x84, *range(0x99, 0xa9), *range(0xb2, 0xb9),
                       0xbb, 0xbd, 0xc0, 0xc1, 0xc6, 0xc7):
            lengths[opcode] = 3
        lengths.update({0xb9: 5, 0xba: 5, 0xc5: 4, 0xc8: 5, 0xc9: 5})
        result, pc = {}, 0
        while pc < len(code):
            opcode = code[pc]
            need(opcode in lengths, "ordinary JVM instruction opcode")
            if opcode in (0xaa, 0xab):
                begin = (pc + 4) & ~3
                need(begin + (12 if opcode == 0xaa else 8) <= len(code), "complete switch header")
                if opcode == 0xaa:
                    low, high = struct.unpack(">ii", code[begin + 4:begin + 12])
                    need(0 <= high - low <= 65535, "bounded table switch")
                    length = begin + 12 + 4 * (high - low + 1) - pc
                else:
                    count = struct.unpack(">i", code[begin + 4:begin + 8])[0]
                    need(0 <= count <= 65535, "bounded lookup switch")
                    length = begin + 8 + count * 8 - pc
            elif opcode == 0xc4:
                need(pc + 1 < len(code) and code[pc + 1] in (
                    *range(0x15, 0x1a), *range(0x36, 0x3b), 0x84, 0xa9), "valid wide instruction")
                length = 6 if code[pc + 1] == 0x84 else 4
            else:
                length = lengths[opcode]
            need(pc + length <= len(code), "complete JVM instruction")
            result[pc] = (opcode, code[pc:pc + length])
            pc += length
        equal(pc, len(code), "complete bytecode instruction boundaries")
        return result

    def method(self, owner, name, descriptor):
        found = [value for value in self.get(owner)["methods"]
                 if (value["name"], value["descriptor"]) == (name, descriptor)]
        need(len(found) == 1, "one exact actual method: " + owner + "." + name + descriptor)
        return found[0]

    def call(self, frame):
        method = self.method(frame["class"], frame["method"], frame["descriptor"])
        opcode, raw = method["pcs"][frame["pc"]]
        need(opcode in (0xb6, 0xb7, 0xb8, 0xb9), "source callsite is actual invocation instruction")
        pool = self.get(frame["class"])["constants"]
        index = int.from_bytes(raw[1:3], "big")
        need(0 < index < len(pool) and pool[index][0] in (10, 11), "actual method reference constant")
        declaring, signature = pool[index][1]
        need(pool[declaring][0] == 7 and pool[signature][0] == 12, "method reference shape")
        owner = pool[pool[declaring][1]][1].replace("/", ".")
        name, descriptor = pool[signature][1]
        return opcode, owner, pool[name][1], pool[descriptor][1]

    def semantic_code(self, owner, name, descriptor):
        """Normalize constant indexes while retaining instructions and PCs.

        Matching a reviewed program permits pool ordering, debug metadata and
        distribution changes. Different instructions remain unsupported rather
        than inheriting Java Random semantics from a version string alone.
        """
        value = self.get(owner)
        method = self.method(owner, name, descriptor)
        pool = value["constants"]

        def constant(index, depth=0):
            need(0 < index < len(pool) and pool[index] is not None and depth <= 8,
                 "complete bounded symbolic instruction constant")
            tag, item = pool[index]
            if tag == 1:
                return [tag, item]
            if tag in (3, 4, 5, 6):
                return [tag, item.hex()]
            if tag in (7, 8, 16, 19, 20):
                return [tag, constant(item, depth + 1)]
            if tag in (9, 10, 11, 12):
                return [tag, *[constant(part, depth + 1) for part in item]]
            if tag == 15:
                return [tag, item[0], constant(item[1], depth + 1)]
            raise ValueError("unsupported symbolic semantic instruction constant")

        program = []
        for pc, (opcode, raw) in method["pcs"].items():
            if opcode in (0x12, 0x13, 0x14, *range(0xb2, 0xba), 0xbb, 0xbd, 0xc0, 0xc1, 0xc5):
                width = 1 if opcode == 0x12 else 2
                operand = [constant(int.from_bytes(raw[1:1 + width], "big")), raw[1 + width:].hex()]
            else:
                operand = raw[1:].hex()
            program.append([pc, opcode, operand])
        encoded = json.dumps(program, ensure_ascii=True, separators=(",", ":")).encode("ascii")
        return {"sha256": hashlib.sha256(encoded).hexdigest(), "instructions": len(program),
                "code_bytes": len(method["code"])}

    def audit_jdk(self, paths):
        for key, digest in JDK_PROGRAMS.items():
            shape = self.semantic_code(*key)
            equal(shape["sha256"], digest, "supported selected JDK instruction semantics: " + ".".join(key))
            self.semantic_shapes[".".join(key)] = shape
        nano = self.method(SYSTEM, "nanoTime", "()J")
        need(nano["modifiers"] & 0x100 and nano["modifiers"] & 8 and "code" not in nano,
             "selected System.nanoTime is the actual native static clock boundary")
        exit_method = self.method(SYSTEM, "exit", "(I)V")
        need(exit_method["modifiers"] & 8 and 0 in exit_method["pcs"], "selected System.exit concrete entry PC0")
        with zipfile.ZipFile(paths["source_archive"]) as archive:
            names = archive.namelist()
            need(len(names) == len(set(names)) <= 65536, "unique bounded selected JDK source archive")
            self.source_bindings = {}
            for name in ("java.base/java/util/Random.java", "java.base/java/lang/Math.java",
                         "java.base/java/lang/System.java", "java.base/jdk/internal/jimage/ImageLocation.java",
                         "java.base/jdk/internal/jimage/BasicImageReader.java"):
                entry = archive.getinfo(name)
                need(not entry.flag_bits & 1 and 0 < entry.file_size <= 2 * MIB, "bounded selected JDK source member")
                raw = archive.read(entry)
                raw.decode("utf-8")
                self.source_bindings[name] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}

    def frame(self, frame):
        generated = re.fullmatch(r"jdk\.internal\.reflect\.Generated(Method|Constructor|SerializationConstructor)Accessor[1-9][0-9]*",
                                 frame["class"])
        if generated:
            need(frame["scope"] == "jdk_nonbootstrap" and frame["native"] is False and 0 <= frame["pc"] <= 65535,
                 "bounded reported generated reflection bridge")
            expected = ("invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;") if generated[1] == "Method" else \
                ("newInstance", "([Ljava/lang/Object;)Ljava/lang/Object;")
            equal((frame["method"], frame["descriptor"]), expected, "known generated reflection bridge signature")
            self.generated[frame["class"]] = {"method": frame["method"], "descriptor": frame["descriptor"],
                                              "coverage": "reported frame only; generated class bytes not retained"}
            return
        value = self.method(frame["class"], frame["method"], frame["descriptor"])
        need(bool(value["modifiers"] & 0x100) == frame["native"], "class-file native method flag")
        if frame["native"]:
            equal(frame["pc"], -1, "native frame PC")
        else:
            need("pcs" in value and frame["pc"] in value["pcs"], "actual complete Java instruction PC")


class Trace:
    def __init__(self, raw, catalog, old):
        need(0 < len(raw) <= 16 * MIB and raw.endswith(b"\n") and b"\r" not in raw, "complete bounded literal LF trace")
        need(all(value in (9, 10) or 32 <= value <= 126 for value in raw), "complete ASCII TSV")
        self.catalog, self.old = catalog, old
        self.rows = []
        for index, line in enumerate(raw[:-1].split(b"\n"), 1):
            need(0 < len(line) <= 16384, "trace line byte bound")
            fields = line.decode("ascii").split("\t")
            need(all(0 < len(field) <= 8192 for field in fields), "trace field bounds")
            self.rows.append({"line": index, "fields": fields})
        self.identities, self.used_ids = {}, set()
        self.meta, self.bindings, self.breakpoints = {}, {}, {}
        self.states, self.stacks, self.stack_ids = {}, {}, {}
        self.math_rows, self.counts, self.events = [], {}, []
        self.used_states, self.used_stacks = set(), set()
        self.constructors, self.draws, self.boundaries = [], [], []
        self.parse()
        self.validate_aux()

    def identity(self, label, kind=None, line=None):
        need(label in self.identities, "existing declared JDI identity: " + label)
        value = self.identities[label]
        need(line is None or value["line"] < line, "identity declared before use")
        need(kind is None or value["class"] == kind, "exact JDI identity class")
        self.used_ids.add(label)
        return value

    def parse(self):
        for row in self.rows:
            fields, line = row["fields"], row["line"]
            kind = fields[0]
            if kind == "identity":
                need(len(fields) == 3 and fields[1] == "o" + str(len(self.identities) + 1) and
                     1 <= len(fields[2]) <= 256 and len(self.identities) < 16384, "complete sequential JDI identity")
                self.identities[fields[1]] = {"class": fields[2], "line": line}
            elif kind == "meta":
                need(len(fields) >= 3 and fields[1] not in self.meta, "unique metadata receipt")
                self.meta[fields[1]] = row
            elif kind == "binding":
                need(len(fields) >= 3 and fields[1] not in self.bindings, "unique passive binding")
                self.bindings[fields[1]] = row
            elif kind == "breakpoint":
                need(len(fields) == 6, "complete breakpoint binding")
                key = tuple(fields[1:])
                need(key not in self.breakpoints, "no duplicate breakpoint binding")
                self.breakpoints[key] = line
            elif kind == "state":
                need(len(fields) == 9, "complete actual Random state receipt")
                key = (fields[1], fields[2])
                need(key not in self.states, "one state per context/role")
                self.identity(fields[3], line=line)
                self.identity(fields[4], "java.util.concurrent.atomic.AtomicLong", line)
                seed, cached = bits(fields[5]), bits(fields[6])
                need(seed <= MASK and fields[7] in ("0", "1"), "complete seed/cache flag bits")
                continuation = fields[8].split(",")
                state = {"seed48": f"{seed:012x}", "have_next_next_gaussian": fields[7] == "1",
                         "next_next_gaussian_bits": fields[6], "next_longs": continuation}
                self.old.random_state(state)
                self.states[key] = {"owner": fields[3], "seed_owner": fields[4], "seed": seed,
                                    "cached": cached, "flag": fields[7] == "1", "state": state, "line": line}
            elif kind == "stack":
                need(len(fields) == 5 and fields[1] == "s" + str(len(self.stack_ids) + 1) and
                     fields[2] not in self.stacks, "complete sequential source stack")
                depth = number(fields[3], 1, 512, "complete source stack depth")
                self.identity(fields[4], "java.lang.Thread", line)
                value = {"id": fields[1], "context": fields[2], "depth": depth, "thread": fields[4],
                         "line": line, "frames": [], "caller": None}
                self.stack_ids[fields[1]] = value
                self.stacks[fields[2]] = value
            elif kind == "frame":
                need(len(fields) == 10 and fields[1] in self.stack_ids, "complete source frame")
                stack = self.stack_ids[fields[1]]
                equal(number(fields[2], 0, 511), len(stack["frames"]), "full source frame ordinal")
                need(fields[3] in ("jdk_bootstrap", "jdk_nonbootstrap", "target", "helper", "game") and
                     fields[9] in ("native", "java"), "source frame domain/native flags")
                if fields[8] != "bootstrap":
                    self.identity(fields[8], line=line)
                frame = {"scope": fields[3], "class": fields[4], "method": fields[5], "descriptor": fields[6],
                         "pc": number(fields[7], -1, 65535, "actual source PC"), "loader": fields[8],
                         "native": fields[9] == "native", "line": line}
                stack["frames"].append(frame)
            elif kind == "caller":
                need(len(fields) == 10 and fields[2] in self.stack_ids, "complete caller receipt")
                stack = self.stack_ids[fields[2]]
                need(fields[1] == stack["context"] and stack["caller"] is None, "unique matching source caller")
                stack["caller"] = {"index": number(fields[3], 0, 511), "fields": fields[4:], "line": line}
            elif kind == "math":
                need(len(fields) == 8, "complete passive Math observation")
                self.math_rows.append(row)
            elif kind == "count":
                expected_length = 9 if len(fields) > 1 and fields[1] == "default_constructors" else 7
                need(len(fields) == expected_length and fields[1] not in self.counts, "complete unique aggregate count")
                self.counts[fields[1]] = row
            elif kind in ("constructor", "boundary", "draw_scope", "draw", "draw_argument", "life", "uniquifier", "result"):
                self.events.append(row)
            else:
                raise ValueError("unknown trace row kind at line " + str(line))

    def validate_aux(self):
        meta = self.meta
        expected = {
            "protocol": ["dedicated-random-observation-" + str(self.catalog.trace_protocol)],
            "scope": ["passive-original-only-instrumented-constructor-inputs-and-World-r-far-draws"],
            "tested_commit": ["null"], "native_comparison": ["false"],
            "can_get_method_return_values": ["1"], "can_get_bytecodes": ["1"], "can_use_instance_filters": ["1"],
            "mirror_policy": ["strong_and_disable_collection_until_VM_death"],
            "clock_policy": ["observe_actual_native_returns_debugger_pauses_change_samples"],
            "bounds": ["25000", "4096", "16384", "512", str(16 * MIB), "180_seconds"],
        }
        if self.catalog.trace_protocol == 2:
            expected.update(source_context=["outer-capture-report"], mechanical_capture_only=["true"])
        equal(set(meta), set(expected) | {"vm_name_utf8hex", "vm_version_utf8hex", "vm_description_utf8hex", "startup_loaded_types"},
              "complete exact trace metadata")
        for key, fields in expected.items():
            equal(meta[key]["fields"], ["meta", key, *fields], "exact observer metadata: " + key)
        for key in ("vm_name_utf8hex", "vm_version_utf8hex", "vm_description_utf8hex"):
            fields = meta[key]["fields"]
            need(len(fields) == 3 and 0 < len(fields[2]) <= 8192 and len(fields[2]) % 2 == 0 and
                 re.fullmatch(r"[0-9a-f]+", fields[2]), "complete VM metadata UTF8 hex")
            bytes.fromhex(fields[2]).decode("utf-8")
        equal(bytes.fromhex(meta["vm_version_utf8hex"]["fields"][2]).decode("utf-8"),
              self.catalog.jdk_version, "actual selected VM version")
        need(len(meta["startup_loaded_types"]["fields"]) == 3, "startup type metadata")
        number(meta["startup_loaded_types"]["fields"][2], 1, 16384, "startup loaded types")
        equal(self.rows[0]["fields"], ["meta", "protocol", "dedicated-random-observation-" + str(self.catalog.trace_protocol)], "literal first protocol row")
        equal(self.rows[-1]["fields"], ["result", "complete"], "literal final completion row")
        equal(set(self.bindings), {"random", "helper_loader", "original", "operation_bytecode_length", "world"},
              "complete passive role bindings")
        binding = self.bindings["random"]["fields"]
        equal(binding, ["binding", "random", RANDOM, "bootstrap", "class-not-actively-initialized-by-observer"],
              "ordinary bootstrap Random class binding")
        helper = self.bindings["helper_loader"]
        need(len(helper["fields"]) == 3, "actual helper loader binding")
        self.helper_loader = helper["fields"][2]
        self.identity(self.helper_loader, "jdk.internal.loader.ClassLoaders$AppClassLoader", helper["line"])
        original = self.bindings["original"]
        need(len(original["fields"]) == 5 and original["fields"][3] == "game_loader", "actual Original/game loader binding")
        self.original, self.game_loader = original["fields"][2], original["fields"][4]
        self.identity(self.original, ORIGINAL, original["line"])
        self.identity(self.game_loader, "java.net.URLClassLoader", original["line"])
        need(self.helper_loader != self.game_loader, "distinct helper/game loaders")
        world = self.bindings["world"]
        need(len(world["fields"]) == 8 and world["fields"][3] == "cache" and world["fields"][5] == "World_r",
             "complete actual dedicated World/cache/Random role binding")
        self.world, self.cache, self.world_random, self.world_seed = (
            world["fields"][2], world["fields"][4], world["fields"][6], world["fields"][7])
        for label, kind in ((self.world, "dp"), (self.cache, "he"), (self.world_random, RANDOM),
                            (self.world_seed, "java.util.concurrent.atomic.AtomicLong")):
            self.identity(label, kind, world["line"])
        expected_breakpoints = {
            ("default-entry", RANDOM, "<init>", "()V", "0"),
            ("uniquifier-entry", RANDOM, "seedUniquifier", "()J", "0"),
            ("delegate-entry", RANDOM, "<init>", "(J)V", "0"),
            ("target-exit", SYSTEM, "exit", "(I)V", "0"),
            ("operation-before", EXPERIMENT, "operation", OP_DESC, "0"),
        }
        operation = self.catalog.method(EXPERIMENT, "operation", OP_DESC)
        need(operation["code"][-1] == 0xb0, "actual frozen operation ends in ARETURN")
        expected_breakpoints.add(("operation-after", EXPERIMENT, "operation", OP_DESC, str(len(operation["code"]) - 1)))
        op_binding = self.bindings["operation_bytecode_length"]["fields"]
        equal(op_binding, ["binding", "operation_bytecode_length", str(len(operation["code"])), "final_opcode", "b0"],
              "independently derived actual operation bytecode binding")
        instance_methods = [method for method in self.catalog.get(RANDOM)["methods"]
                            if not method["modifiers"] & (0x8 | 0x100 | 0x400)]
        need(1 <= len(instance_methods) <= 64, "bounded complete actual Random instance entry points")
        for method in instance_methods:
            need(0 in method["pcs"], "actual instance method entry PC0")
            expected_breakpoints.add(("World-r-draw-entry", RANDOM, method["name"], method["descriptor"], "0"))
        equal(set(self.breakpoints), expected_breakpoints, "every exact declared nonstatic Random entry breakpoint")
        for role, owner, name, descriptor, pc in expected_breakpoints:
            method = self.catalog.method(owner, name, descriptor)
            need(number(pc, 0, 65535) in method["pcs"] and not method["modifiers"] & (0x100 | 0x400),
                 "actual concrete bytecode breakpoint")
        self.validate_stacks()

    def validate_stacks(self):
        main = None
        for stack in self.stacks.values():
            equal(len(stack["frames"]), stack["depth"], "every complete captured source stack frame")
            if main is None:
                main = stack["thread"]
            equal(stack["thread"], main, "same measured main thread for all source stacks")
            need(stack["caller"] is not None, "actual source caller present")
            first = next((index for index, frame in enumerate(stack["frames"])
                          if frame["class"] != RANDOM and not (
                              frame["class"] == SYSTEM and frame["method"] == "nanoTime")), None)
            need(first is not None, "first non-Random source caller")
            caller, frame = stack["caller"], stack["frames"][first]
            equal(caller["index"], first, "actual first source caller ordinal")
            equal(caller["fields"], [frame["scope"], frame["class"], frame["method"], frame["descriptor"],
                                    str(frame["pc"]), frame["loader"]], "caller duplicates complete actual frame")
            equal(caller["line"], frame["line"] + 1, "caller follows exact captured frame")
            end = max(caller["line"], stack["frames"][-1]["line"])
            for row in self.rows[stack["line"]:end]:
                need(row["fields"][0] in ("identity", "frame", "caller"), "uninterrupted complete stack payload")
            for frame in stack["frames"]:
                scope, owner, loader = frame["scope"], frame["class"], frame["loader"]
                if scope == "jdk_bootstrap":
                    equal(loader, "bootstrap", "actual bootstrap loader")
                    need(owner.startswith(("java.", "javax.", "jdk.", "sun.", "com.sun.")), "bootstrap source class domain")
                elif scope in ("helper", "target"):
                    equal(loader, self.helper_loader, "actual pinned helper loader")
                    need(self.bindings["helper_loader"]["line"] < frame["line"], "helper binding precedes frame")
                    need(owner == TARGET if scope == "target" else owner + ".class" in self.old.HELPERS,
                         "exact project helper source domain")
                elif scope == "game":
                    equal(loader, self.game_loader, "actual supplied archive loader")
                    need(owner in self.catalog.game and self.bindings["original"]["line"] < frame["line"],
                         "pinned original source frame")
                else:
                    need(loader != "bootstrap" and owner.startswith(("java.", "javax.", "jdk.", "sun.", "com.sun.")),
                         "explicit JDK nonbootstrap frame domain")
                    self.identity(loader)
                    if re.fullmatch(r"jdk\.internal\.reflect\.Generated(Method|Constructor|SerializationConstructor)Accessor[1-9][0-9]*",
                                    owner):
                        self.identity(loader, "jdk.internal.reflect.DelegatingClassLoader")
                self.catalog.frame(frame)
            stack["end_line"] = end
        self.main_thread = main

    def state(self, context, role, after=0, before=None):
        key = (context, role)
        need(key in self.states and key not in self.used_states, "one complete state used in actual context: " + context)
        value = self.states[key]
        need(value["line"] > after and (before is None or value["line"] < before), "state receipt in actual event interval")
        self.used_states.add(key)
        return value

    def stack(self, context, after, before, method):
        need(context in self.stacks and context not in self.used_stacks, "complete stack for actual event: " + context)
        value = self.stacks[context]
        need(after < value["line"] <= value["end_line"] < before, "complete stack in actual event interval")
        frame = value["frames"][0]
        equal((frame["class"], frame["method"], frame["descriptor"]), method, "actual observed top source method")
        self.used_stacks.add(context)
        return value

    @staticmethod
    def same(left, right, message):
        equal({key: left[key] for key in ("owner", "seed_owner", "seed", "cached", "flag", "state")},
              {key: right[key] for key in ("owner", "seed_owner", "seed", "cached", "flag", "state")}, message)

    def constructor_path(self, call):
        contexts = ("entry", "uniquifier_return", "nano_time_return", "delegate")
        tops = ((RANDOM, "<init>", "()V"), (RANDOM, "seedUniquifier", "()J"),
                (SYSTEM, "nanoTime", "()J"), (RANDOM, "<init>", "(J)V"))
        lower = (call["entry_line"], call["unique_line"], call["clock_line"], call["delegate_line"])
        upper = (call["unique_entry_line"], call["clock_line"], call["delegate_line"], call["delegate_state"]["line"])
        stacks = []
        for suffix, top, low, high in zip(contexts, tops, lower, upper):
            stacks.append(self.stack("constructor_" + str(call["index"]) + "_" + suffix, low, high, top))
        entry = stacks[0]
        caller = entry["frames"][entry["caller"]["index"]]
        comparable = ("scope", "class", "method", "descriptor", "pc", "loader")
        for stack in stacks[1:]:
            actual = stack["frames"][stack["caller"]["index"]]
            equal({key: actual[key] for key in comparable}, {key: caller[key] for key in comparable},
                  "same complete source callsite throughout constructor")
        opcode, owner, method, descriptor = self.catalog.call(caller)
        equal((opcode, owner, method, descriptor), (0xb7, RANDOM, "<init>", "()V"),
              "actual source invokespecial Random default constructor")
        need(all(stack["frames"][-1]["class"] == TARGET and stack["frames"][-1]["method"] == "main"
                 for stack in stacks), "complete stacks end in actual frozen target main")
        if call["kind"] == "subclass_base":
            lineage, owner = [], call["actual_class"]
            while owner is not None and owner != RANDOM:
                need(owner not in lineage and len(lineage) < 64, "actual Random subclass hierarchy")
                lineage.append(owner)
                owner = self.catalog.get(owner)["super"]
            equal(owner, RANDOM, "actual subclass reaches Random superclass")
        elif call["kind"] == "ordinary":
            equal(call["actual_class"], RANDOM, "ordinary constructor has exact Random class")
        else:
            raise ValueError("unknown constructor branch")
        game_frames = [frame for frame in entry["frames"] if frame["scope"] == "game"]
        if caller["scope"] == "game":
            classification = "game-allocation" if call["kind"] == "ordinary" else "game-subclass-base"
            selected = True
            applicability = "actual game allocation callsite" if call["kind"] == "ordinary" else \
                "base constructor inputs only; subclass virtual setSeed semantics retained separately"
        elif (caller["class"], caller["method"], caller["descriptor"]) == (
                "java.lang.Math$RandomNumberGeneratorHolder", "<clinit>", "()V"):
            need(game_frames and any(frame["class"] == "java.lang.Math" and
                 (frame["method"], frame["descriptor"]) == ("random", "()D") for frame in entry["frames"]),
                 "Math holder constructor has complete actual game-to-Math.random source path")
            need(call["kind"] == "ordinary", "represented Math holder uses exact ordinary Random")
            classification, selected = "game-triggered-library-Math", True
            applicability = "native Math lazy holder shares the actual default constructor profile/CAS/sampling boundary"
        elif caller["scope"] in ("helper", "target"):
            classification, selected, applicability = "target-helper-allocation", False, "observer service input"
        elif caller["scope"].startswith("jdk_"):
            classification = "game-triggered-unrepresented-JDK" if game_frames else "JDK-only-allocation"
            selected, applicability = False, "no represented native source constructor callpath"
        else:
            raise ValueError("unsupported actual constructor allocation domain")
        call.update(classification=classification, selected=selected, applicability=applicability,
                    source_caller={key: caller[key] for key in comparable},
                    source_stacks=stacks)

    def draw_result(self, draw, after, returned):
        name, descriptor = draw["method"], draw["descriptor"]
        arguments, children, before = draw["arguments"], draw["children"], draw["before"]
        child_names = [(child["method"], child["descriptor"]) for child in children]
        def next_values(widths):
            equal(child_names, [("next", "(I)I")] * len(widths), "exact nested next call tree")
            values = []
            for child, width in zip(children, widths):
                equal(child["arguments"], ["int:" + str(width)], "exact nested bit request")
                need(child["returned"].startswith("int:"), "nested integer result")
                values.append(number(child["returned"][4:], -(1 << 31), (1 << 31) - 1))
            return values
        def preserve_cache():
            equal((after["cached"], after["flag"]), (before["cached"], before["flag"]), "draw preserves Gaussian cache")
        if (name, descriptor) == ("writeObject", "(Ljava/io/ObjectOutputStream;)V"):
            need(not children and len(arguments) == 1 and arguments[0].startswith("reference:"), "passive serialization has no nested Random call")
            self.identity(arguments[0][10:], "java.io.ObjectOutputStream", draw["argument_lines"][0])
            equal(returned, "void", "actual serializer void return")
            self.same(before, after, "observer serialization preserves complete owner/state")
            draw["classification"] = "observer-service"
        elif (name, descriptor) == ("next", "(I)I"):
            need(len(arguments) == 1 and arguments[0].startswith("int:") and not children, "actual next argument/leaf")
            width = number(arguments[0][4:], 1, 32, "actual next bit width")
            seed = (before["seed"] * MULTIPLIER + 11) & MASK
            equal(after["seed"], seed, "full actual next LCG transition")
            equal(returned, "int:" + str(signed32(seed >> (48 - width))), "actual next return bits")
            preserve_cache()
        elif (name, descriptor) == ("setSeed", "(J)V"):
            need(len(arguments) == 1 and arguments[0].startswith("long:") and not children, "actual setSeed input/leaf")
            seed = (bits(arguments[0][5:]) ^ MULTIPLIER) & MASK
            equal(after["seed"], seed, "actual setSeed scramble")
            equal((after["cached"], after["flag"], returned), (before["cached"], False, "void"), "actual setSeed Gaussian effects")
        elif (name, descriptor) == ("nextFloat", "()F"):
            equal(arguments, [], "nextFloat no arguments")
            value = next_values([24])[0]
            equal(returned, "float:" + struct.pack(">f", value / (1 << 24)).hex(), "exact dyadic nextFloat result bits")
            preserve_cache()
        elif (name, descriptor) == ("nextDouble", "()D"):
            equal(arguments, [], "nextDouble no arguments")
            first, second = next_values([26, 27])
            equal(returned, "double:" + struct.pack(">d", ((first << 27) + second) / (1 << 53)).hex(), "exact dyadic nextDouble result")
            preserve_cache()
        elif (name, descriptor) == ("nextLong", "()J"):
            equal(arguments, [], "nextLong no arguments")
            high, low = next_values([32, 32])
            equal(returned, "long:" + f"{((high << 32) + low) & MASK64:016x}", "signed-low nextLong addition")
            preserve_cache()
        elif (name, descriptor) == ("nextBoolean", "()Z"):
            equal(arguments, [], "nextBoolean no arguments")
            value = next_values([1])[0]
            equal(returned, "boolean:" + str(int(value != 0)), "actual nextBoolean result")
            preserve_cache()
        elif (name, descriptor) == ("nextInt", "()I"):
            equal(arguments, [], "nextInt no arguments")
            value = next_values([32])[0]
            equal(returned, "int:" + str(value), "actual unbounded nextInt")
            preserve_cache()
        elif (name, descriptor) == ("nextInt", "(I)I"):
            need(len(arguments) == 1 and arguments[0].startswith("int:"), "bounded nextInt argument")
            bound = number(arguments[0][4:], 1, (1 << 31) - 1, "nextInt positive bound")
            need(children, "bounded nextInt has actual child draws")
            values = next_values([31] * len(children))
            if bound & (bound - 1) == 0:
                need(len(values) == 1, "power-of-two nextInt consumes one draw")
                expected = (bound * values[0]) >> 31
            else:
                for value in values[:-1]:
                    need(signed32(value - value % bound + bound - 1) < 0, "every actual nextInt rejected candidate")
                value = values[-1]
                need(signed32(value - value % bound + bound - 1) >= 0, "actual accepted nextInt candidate")
                expected = value % bound
            equal(returned, "int:" + str(expected), "actual bounded nextInt return")
            preserve_cache()
        elif (name, descriptor) == ("nextGaussian", "()D"):
            need(not arguments and before["flag"] and not children, "uncached nextGaussian needs an additional exact StrictMath audit")
            equal(returned, "double:" + f"{before['cached']:016x}", "actual cached Gaussian return bits")
            equal((after["seed"], after["cached"], after["flag"]),
                  (before["seed"], before["cached"], False), "actual cached Gaussian state")
        else:
            raise ValueError("unsupported reached actual Random method")
        if name not in ("next", "setSeed"):
            expected_seed = children[-1]["after"]["seed"] if children else before["seed"]
            equal(after["seed"], expected_seed, "no unobserved LCG step in enclosing method")
        equal((after["owner"], after["seed_owner"]), (before["owner"], before["seed_owner"]), "actual draw owner stability")
        draw.setdefault("classification", "gameplay-LCG-step" if name == "next" else "gameplay-method")

    def audit_events(self):
        current_u, unique_owner, active, operation, next_operation = None, None, None, None, 0
        last_boundary, far, last_draw = None, False, None
        draw_stack, owner_calls, seed_calls = [], {}, {}
        life, result = {}, 0
        index = 0
        while index < len(self.events):
            row = self.events[index]
            fields, line = row["fields"], row["line"]
            next_line = self.events[index + 1]["line"] if index + 1 < len(self.events) else len(self.rows) + 1
            kind = fields[0]
            if kind == "life":
                need(len(fields) >= 2 and fields[1] not in life, "one actual lifecycle event")
                event = fields[1]
                if event == "VM_start":
                    need(len(fields) == 3 and not life and active is None, "actual VM startup")
                    self.identity(fields[2], "java.lang.Thread", line)
                    equal(fields[2], self.main_thread, "stack thread is actual VM main thread")
                elif event == "target_System_exit":
                    need(len(fields) == 4 and fields[2] == "0" and next_operation == 17 and
                         operation is None and active is None and not draw_stack and not far, "complete successful actual target exit")
                    equal(bits(fields[3]), current_u, "target exit actual full uniquifier")
                    final = self.state("target_exit", "World_r", before=line)
                    self.same(final, last_boundary, "World.r unchanged after final operation")
                    need(final["line"] > self.boundaries[-1]["line"], "final Random state after final operation")
                elif event == "VM_death":
                    equal(fields, ["life", "VM_death"], "actual VM death receipt")
                    need("target_System_exit" in life, "death after actual target System.exit")
                elif event == "VM_disconnect":
                    equal(fields, ["life", "VM_disconnect"], "actual VM disconnect receipt")
                    need("VM_death" in life, "disconnect after death")
                else:
                    raise ValueError("unknown lifecycle receipt")
                life[event] = row
            elif kind == "uniquifier":
                need(len(fields) == 4 and fields[1] == "fresh_initial" and current_u is None and
                     not self.constructors and "VM_start" in life, "one fresh observed bootstrap uniquifier")
                unique_owner = fields[2]
                self.identity(unique_owner, "java.util.concurrent.atomic.AtomicLong", line)
                current_u = bits(fields[3])
                equal(current_u, INITIAL_U, "fresh full 64-bit default constructor admission")
            elif kind == "constructor":
                need(len(fields) >= 3 and "VM_start" in life and "target_System_exit" not in life, "constructor inside live target")
                serial = number(fields[1], 1, 4096, "constructor serial")
                phase = fields[2]
                if phase == "entry":
                    need(len(fields) == 10 and active is None and not draw_stack and current_u is not None,
                         "nonoverlapping actual default constructor entry")
                    equal(serial, len(self.constructors) + 1, "all constructor serials contiguous")
                    self.identity(fields[3], fields[9], line)
                    equal(fields[4], unique_owner, "same shared actual uniquifier owner")
                    equal(bits(fields[5]), current_u, "full current U before each constructor")
                    stage = ACTIONS[operation] if operation is not None else \
                        "target_bootstrap" if next_operation == 0 else "target_completion" if next_operation == 17 else "between_operations"
                    equal((fields[6], number(fields[7], -1, 16)), (stage, -1 if operation is None else operation),
                          "actual constructor operation scope")
                    need(fields[8] in ("ordinary", "subclass_base") and fields[3] not in owner_calls, "fresh constructor owner/branch")
                    active = {"index": serial, "owner": fields[3], "kind": fields[8], "actual_class": fields[9],
                              "initial_u": current_u, "entry_line": line, "stage": stage, "operation": operation, "phase": 1}
                else:
                    need(active is not None and serial == active["index"], "actual constructor phase owner/order")
                    if phase == "uniquifier_entry":
                        need(len(fields) == 4 and active["phase"] == 1, "actual seedUniquifier entry")
                        equal(bits(fields[3]), current_u, "full uniquifier remains before actual CAS")
                        active.update(phase=2, unique_entry_line=line)
                    elif phase == "uniquifier_return":
                        need(len(fields) == 5 and active["phase"] == 2 and fields[3] == unique_owner, "actual uniquifier return phase")
                        committed = bits(fields[4])
                        equal(committed, (current_u * U_MULTIPLIER) & MASK64, "every observed wrapping 64-bit U step")
                        current_u = committed
                        active.update(phase=3, committed_u=committed, unique_line=line)
                    elif phase == "nano_time_return":
                        need(len(fields) == 4 and active["phase"] == 3, "actual native clock return order")
                        sample = bits(fields[3])
                        active.update(phase=4, nano_time_bits=sample, nano_time_signed=signed64(sample), clock_line=line)
                    elif phase == "delegate_entry":
                        need(len(fields) == 4 and active["phase"] == 4, "actual seeded delegate entry order")
                        argument = bits(fields[3])
                        equal(argument, active["committed_u"] ^ active["nano_time_bits"], "actual full64 XOR delegate argument")
                        active.update(phase=5, delegate_argument=argument, delegate_line=line)
                    elif phase == "return":
                        need(len(fields) == 5 and active["phase"] == 5 and fields[3] == active["owner"], "actual completed default return")
                        equal(bits(fields[4]), current_u, "no omitted constructor U change")
                        context = "constructor_" + str(serial)
                        role = active["kind"]
                        delegated = self.state(context + "_delegate_return", role, after=active["delegate_line"], before=line)
                        completed = self.state(context + "_default_return", role, after=delegated["line"], before=line)
                        self.same(completed, delegated, "complete default/delegate return state and owner binding")
                        equal(completed["owner"], active["owner"], "actual constructor result owner")
                        need(completed["seed_owner"] != unique_owner and completed["seed_owner"] not in seed_calls,
                             "distinct actual AtomicLong seed owners")
                        active["delegate_state"], active["completed_state"] = delegated, completed
                        if role == "ordinary":
                            equal((completed["seed"], completed["cached"], completed["flag"]),
                                  ((active["delegate_argument"] ^ MULTIPLIER) & MASK, 0, False),
                                  "fresh ordinary constructor complete seed/Gaussian state")
                        else:
                            base_key = (context + "_base_setSeed_return", "subclass_base")
                            if base_key in self.states:
                                base = self.state(*base_key, after=active["delegate_line"], before=delegated["line"])
                                equal((base["seed"], base["flag"]), ((active["delegate_argument"] ^ MULTIPLIER) & MASK, False),
                                      "actual reached subclass base setSeed effect")
                                equal((base["owner"], base["seed_owner"]), (completed["owner"], completed["seed_owner"]),
                                      "subclass base seed owner binding")
                                active["base_set_seed_state"] = base
                        active.update(return_line=line)
                        self.constructor_path(active)
                        owner_calls[active["owner"]] = active
                        seed_calls[completed["seed_owner"]] = active
                        self.constructors.append(active)
                        active = None
                    else:
                        raise ValueError("unknown constructor phase")
            elif kind == "boundary":
                need(len(fields) == 7 and active is None and not draw_stack, "complete idle operation boundary")
                serial = number(fields[1], 0, 16, "operation index")
                equal((fields[2], fields[4], fields[5]), (ACTIONS[serial], self.world, self.cache), "actual operation source roles/schedule")
                equal(bits(fields[6]), current_u, "actual full U at operation boundary")
                when = fields[3]
                state = self.state("operation_" + str(serial) + "_" + when, "World_r", after=line, before=next_line)
                equal(state["line"], line + 1, "actual complete World.r follows boundary")
                equal((state["owner"], state["seed_owner"]), (self.world_random, self.world_seed), "actual World.r/AtomicLong roles")
                need(self.world_random in owner_calls, "World.r default construction precedes measured operations")
                if when == "before":
                    need(operation is None and serial == next_operation and not far, "ordered operation entry")
                    if last_boundary is not None:
                        self.same(last_boundary, state, "no World.r change between observed operations")
                    operation = serial
                elif when == "after":
                    need(operation == serial == next_operation and not far, "ordered operation exit after draw scope close")
                    previous = self.boundaries[-1]["state"]
                    if serial == 12:
                        self.same(last_draw, state, "complete traced far scope reaches actual operation end")
                    else:
                        self.same(previous, state, "World.r unchanged outside actual traced far operation")
                    last_boundary, operation, next_operation = state, None, next_operation + 1
                else:
                    raise ValueError("unknown operation boundary")
                self.boundaries.append({"index": serial, "when": when, "line": line, "state": state})
            elif kind == "draw_scope":
                need(active is None and operation == 12 and len(fields) >= 2, "actual far operation scope")
                if fields[1] == "begin":
                    equal(fields, ["draw_scope", "begin", "12", self.world_random, self.world_seed, "actual-instance-filter"],
                          "genuine World.r instance-filtered scope")
                    need(not far and not self.draws, "unique actual far trace start")
                    far = True
                    last_draw = self.boundaries[-1]["state"]
                elif fields[1] == "end":
                    need(len(fields) == 5 and fields[2] == "12" and far and not draw_stack, "actual far scope closes with no unfinished call")
                    equal((number(fields[3]), number(fields[4])), (len(self.draws), len(self.draws)), "complete scope entries/returns")
                    far = False
                else:
                    raise ValueError("unknown draw scope")
            elif kind == "draw":
                need(len(fields) >= 3 and far and operation == 12 and active is None, "World.r event inside actual far scope")
                serial = number(fields[1], 1, 25000, "actual draw serial")
                if fields[2] == "entry":
                    need(len(fields) == 8 and len(draw_stack) < 32, "complete actual Random method entry")
                    equal(serial, len(self.draws) + 1, "all actual method entry serials")
                    equal((number(fields[3], 0, 31), fields[4], fields[7]),
                          (len(draw_stack), str(draw_stack[-1]["index"]) if draw_stack else "-", self.world_random),
                          "actual nested call depth/parent/owner")
                    need(not draw_stack or draw_stack[-1]["method"] != "writeObject",
                         "reject every nested Random entry while passive writeObject is active")
                    arguments, argument_lines = [], []
                    while index + 1 < len(self.events) and self.events[index + 1]["fields"][0] == "draw_argument":
                        index += 1
                        argument = self.events[index]["fields"]
                        need(len(argument) == 4 and number(argument[1], 1, 25000) == serial and
                             number(argument[2], 0, 31) == len(arguments), "complete ordered actual call arguments")
                        arguments.append(argument[3])
                        argument_lines.append(self.events[index]["line"])
                    next_line = self.events[index + 1]["line"] if index + 1 < len(self.events) else len(self.rows) + 1
                    context = "draw_" + str(serial) + "_entry"
                    state = self.state(context, "World_r", after=line, before=next_line)
                    equal((state["owner"], state["seed_owner"]), (self.world_random, self.world_seed), "actual instance-filtered entry state")
                    self.same(state, last_draw, "no World.r state gap before actual method entry")
                    stack = self.stack(context, state["line"], next_line, (RANDOM, fields[5], fields[6]))
                    equal(stack["frames"][0]["pc"], 0, "actual per-method entry breakpoint PC0")
                    method = self.catalog.method(RANDOM, fields[5], fields[6])
                    need(not method["modifiers"] & (0x8 | 0x100 | 0x400), "actual concrete nonstatic Random method")
                    if draw_stack:
                        parent = draw_stack[-1]
                        frame = stack["frames"][1]
                        equal((frame["class"], frame["method"], frame["descriptor"]),
                              (RANDOM, parent["method"], parent["descriptor"]), "immediate actual nested Random caller")
                    if fields[5] == "writeObject":
                        snapshots = [frame for frame in stack["frames"] if frame["scope"] == "helper" and
                                     frame["class"] == SNAPSHOT and frame["method"] == "snapshot" and
                                     frame["descriptor"] == "(Ljava/util/Random;)Ljava/util/Map;"]
                        need(not draw_stack and fields[6] == "(Ljava/io/ObjectOutputStream;)V" and len(snapshots) == 1,
                             "serialization through exact pinned passive snapshot source path")
                        equal(self.catalog.call(snapshots[0]),
                              (0xb6, "java.io.ObjectOutputStream", "writeObject", "(Ljava/lang/Object;)V"),
                              "actual copied-state helper serialization callsite")
                    else:
                        need(stack["frames"][stack["caller"]["index"]]["scope"] == "game",
                             "gameplay World.r draw has actual supplied-original source caller")
                    draw = {"index": serial, "line": line, "depth": len(draw_stack), "method": fields[5],
                            "descriptor": fields[6], "arguments": arguments, "argument_lines": argument_lines,
                            "children": [], "before": state,
                            "source_stack": stack}
                    if draw_stack:
                        draw_stack[-1]["children"].append(draw)
                    self.draws.append(draw)
                    draw_stack.append(draw)
                elif fields[2] == "return":
                    need(len(fields) == 7 and draw_stack and draw_stack[-1]["index"] == serial, "actual nested method return order")
                    draw = draw_stack.pop()
                    equal((number(fields[3], 0, 31), fields[4], fields[5]),
                          (draw["depth"], draw["method"], draw["descriptor"]), "exact returning method")
                    after = self.state("draw_" + str(serial) + "_return", "World_r", after=line, before=next_line)
                    self.draw_result(draw, after, fields[6])
                    draw.update(after=after, returned=fields[6], return_line=line)
                    last_draw = after
                else:
                    raise ValueError("unknown actual Random event")
            elif kind == "draw_argument":
                raise ValueError("orphan actual Random argument")
            elif kind == "result":
                equal(fields, ["result", "complete"], "successful final observer result")
                need("VM_disconnect" in life and active is None and operation is None and not draw_stack and not far,
                     "complete target/event trajectory before result")
                result += 1
            else:
                raise ValueError("unknown semantic event")
            index += 1
        need(result == 1 and set(life) == {"VM_start", "target_System_exit", "VM_death", "VM_disconnect"} and
             len(self.boundaries) == 34 and next_operation == 17 and self.constructors, "complete original event corpus")
        self.life, self.owner_calls = life, owner_calls
        self.audit_math()
        self.audit_counts()
        equal(self.used_states, set(self.states), "every actual state receipt independently checked")
        equal(self.used_stacks, set(self.stacks), "every complete captured stack independently attributed")
        unbound = set(self.identities) - self.used_ids
        # worldBinding deliberately holds Experiment without emitting its label
        # in a separate binding row. Preserve that producer-schema limitation.
        need(len(unbound) <= 1 and all(self.identities[label]["class"] == EXPERIMENT for label in unbound),
             "only the source-documented held Experiment mirror lacks a role receipt")
        self.unbound_helper_mirrors = sorted(unbound)

    def audit_math(self):
        markers = ["VM_start", *["operation_" + str(index) + "_" + when
                                for index in range(17) for when in ("before", "after")], "target_exit"]
        equal([row["fields"][1] for row in self.math_rows], markers, "all thirty-six ordered passive Math boundaries")
        prior_loaded, prior_prepared, prior_initialized, owner, seed = False, False, False, None, None
        observations = []
        boundary_lines = {"operation_" + str(row["index"]) + "_" + row["when"]: row["line"]
                          for row in self.boundaries}
        for row in self.math_rows:
            fields, line = row["fields"], row["line"]
            marker, status = fields[1], fields[2]
            if marker == "VM_start":
                need(self.life["VM_start"]["line"] < line < self.constructors[0]["entry_line"], "passive Math startup before actual constructor")
            elif marker == "target_exit":
                need(self.states[("target_exit", "World_r")]["line"] < line < self.life["target_System_exit"]["line"],
                     "passive Math state inside actual target exit")
            else:
                equal(line, boundary_lines[marker] + 2, "passive Math follows actual World.r boundary state")
            if status == "absent":
                equal(fields[3:], ["0", "0", "0", "-", "-"], "actual absent Math holder receipt")
                need(not prior_loaded and owner is None, "Math holder cannot disappear")
                value = {"marker": marker, "status": "absent", "line": line}
            elif status == "loaded":
                need(fields[3] in ("0", "1") and fields[4] in ("0", "1") and fields[5] == "0",
                     "actual prepared/initialized/nonfailed holder flags")
                prepared, initialized = fields[3] == "1", fields[4] == "1"
                need(not prior_prepared or prepared, "Math preparation does not regress")
                need(not prior_initialized or initialized, "Math initialization does not regress")
                prior_loaded, prior_prepared, prior_initialized = True, prepared, initialized
                value = {"marker": marker, "status": "loaded", "prepared": prepared,
                         "initialized": initialized, "line": line}
                if initialized:
                    need(prepared, "initialized holder is prepared")
                    self.identity(fields[6], RANDOM, line)
                    self.identity(fields[7], "java.util.concurrent.atomic.AtomicLong", line)
                    if owner is None:
                        owner, seed = fields[6], fields[7]
                    equal((fields[6], fields[7]), (owner, seed), "same actual Math Random/AtomicLong owners")
                    need(owner in self.owner_calls, "real Math constructor was fully observed")
                    call = self.owner_calls[owner]
                    need(call["classification"] == "game-triggered-library-Math" and call["kind"] == "ordinary" and
                         call["return_line"] < line and call["completed_state"]["seed_owner"] == seed,
                         "actual Math holder publication binds its complete observed constructor")
                    state = self.state(marker, "Math", after=line, before=line + 2)
                    equal((state["owner"], state["seed_owner"]), (owner, seed), "actual passive Math state roles")
                    value["state"] = state
                else:
                    equal(fields[6:], ["-", "-"], "uninitialized Math holder is not actively read")
                    need(owner is None, "Math owner not read before initialization")
            else:
                raise ValueError("unknown Math holder state")
            observations.append(value)
        self.math = {"owner": owner, "seed_owner": seed, "observations": observations,
                     "draw_trace": False,
                     "scope": "actual initialization and complete state at retained boundaries; Math draw calls are not traced"}

    def audit_counts(self):
        expected_keys = {"events", "default_constructors", "ordinary_defaults", "World_r_method_entries", "operations"}
        equal(set(self.counts), expected_keys, "all complete aggregate count rows")
        expected_labels = {
            "events": ("events", "breakpoints", "class_prepares"),
            "default_constructors": ("default_constructors", "uniquifier_returns", "nano_time_returns", "delegate_returns"),
            "ordinary_defaults": ("ordinary_defaults", "subclass_base_defaults", "validated_subclass_base_setSeed_returns"),
            "World_r_method_entries": ("World_r_method_entries", "World_r_method_returns", "validated_constructor_scramble_returns"),
            "operations": ("operations", "far_scopes", "held_mirrors")}
        counts = {}
        for key, row in self.counts.items():
            fields = row["fields"]
            equal(tuple(fields[1::2]), expected_labels[key], "exact complete counter labels")
            need(self.life["VM_disconnect"]["line"] < row["line"] < self.rows[-1]["line"], "count rows after completed VM disconnect")
            for name, value in zip(fields[1::2], fields[2::2]):
                need(name not in counts, "unique aggregate counter name")
                counts[name] = number(value, 0, 25000, "bounded aggregate count")
        defaults = len(self.constructors)
        ordinary = sum(call["kind"] == "ordinary" for call in self.constructors)
        subclass = defaults - ordinary
        base = sum("base_set_seed_state" in call for call in self.constructors)
        for name in ("default_constructors", "uniquifier_returns", "nano_time_returns", "delegate_returns"):
            equal(counts[name], defaults, "actual complete constructor count")
        equal((counts["ordinary_defaults"], counts["subclass_base_defaults"],
               counts["validated_subclass_base_setSeed_returns"]), (ordinary, subclass, base), "actual constructor branch counts")
        equal(counts["validated_constructor_scramble_returns"], ordinary + base,
              "scramble aggregate consistent with actual ordinary/subclass branch states")
        equal((counts["World_r_method_entries"], counts["World_r_method_returns"]),
              (len(self.draws), len(self.draws)), "every actual instance method entry/return counted")
        equal((counts["operations"], counts["far_scopes"], counts["held_mirrors"]),
              (17, 1, len(self.identities)), "actual operations/scope/strong mirrors")
        expected_breakpoints = 3 * defaults + 34 + 1 + len(self.draws)
        equal(counts["breakpoints"], expected_breakpoints, "all actual constructor/operation/exit/instance breakpoint events")
        need(0 <= counts["class_prepares"] <= 4, "at most the four exact class prepare requests")
        expected_events = 3 + counts["class_prepares"] + expected_breakpoints + \
            4 * defaults + counts["validated_constructor_scramble_returns"] + base + len(self.draws)
        equal(counts["events"], expected_events, "all lifecycle/prepare/breakpoint/method-exit events accounted")
        need(1 <= counts["events"] <= 25000, "complete event budget")
        self.aggregate_counts = counts

    def input_tape(self):
        selected, excluded, current = [], [], INITIAL_U
        for call in self.constructors:
            attribution = {
                "constructor_index": call["index"], "owner": call["owner"], "kind": call["kind"],
                "actual_class": call["actual_class"], "classification": call["classification"],
                "applicability": call["applicability"], "stage": call["stage"], "operation": call["operation"],
                "committed_uniquifier_hex": f"{call['committed_u']:016x}",
                "nano_time_hex": f"{call['nano_time_bits']:016x}", "nano_time_signed": call["nano_time_signed"],
                "observed_delegate_argument_hex": f"{call['delegate_argument']:016x}",
                "source_caller": call["source_caller"], "source_stacks": call["source_stacks"],
                "trace_lines": {key: call[key] for key in (
                    "entry_line", "unique_entry_line", "unique_line", "clock_line", "delegate_line", "return_line")},
            }
            if call["selected"]:
                need(call["initial_u"] == current and call["committed_u"] == (current * U_MULTIPLIER) & MASK64,
                     "selected constructor tape has a full observed U progression gap; excluded calls cannot be compressed")
                current = call["committed_u"]
                selected.append(attribution)
            else:
                excluded.append(attribution)
        equal(len(selected), 9, "all nine source-profile constructor inputs")
        need(not excluded, "finite profile has no unrepresented constructor gaps")
        rows = [f"{row['committed_uniquifier_hex']}\t{row['nano_time_signed']}\n".encode("ascii") for row in selected]
        tape = TAPE_HEADER + b"".join(rows)
        need(not tape.startswith(b"\xef\xbb\xbf") and len(tape) <= len(TAPE_HEADER) + 65536 * 39,
             "bounded exact native input tape")
        return tape, {"selected": selected, "excluded": excluded,
                      "shared_initial_uniquifier_hex": f"{INITIAL_U:016x}",
                      "selected_rows": len(selected), "complete_observed_constructor_rows": len(self.constructors),
                      "contiguous_shared_progression": True, "native_expected_state": False}


def bind_graph(document, findings, trace, old, jar):
    classes = old.Classes(jar)
    audit = old.Observation(document, classes)
    audit.load_blobs()
    objects = [audit.graph(state["graph"], index > 0) for index, state in enumerate(document["states"])]
    world_ref = document["states"][0]["dedicated"]["world"]
    world_id = world_ref["ref"]
    cache_ref = old.Observation.field(objects[0], world_ref, "dj", "v")
    random_ref = old.Observation.field(objects[0], world_ref, "dj", "r")
    world_random = random_ref["ref"]
    equal(world_random, findings["world_random_owner"], "same fully audited target graph Random role")
    role_map = {trace.world: world_id, trace.cache: cache_ref["ref"], trace.world_random: world_random,
                trace.world_seed: objects[0][world_random]["seed_owner"]["ref"]}
    rows, covered = [], set()
    for boundary in trace.boundaries:
        operation = document["operations"][boundary["index"]]
        state_index = operation[boundary["when"]]["state_reference"]
        state = boundary["state"]
        actual = objects[state_index]
        equal(old.Observation.field(actual, world_ref, "dj", "r"), random_ref, "same actual graph World.r owner throughout")
        equal(actual[world_random]["state"], state["state"], "all raw World.r fields and eight copied continuations cross-bind")
        equal(actual[world_random]["seed_owner"]["ref"], role_map[trace.world_seed], "same graph AtomicLong owner throughout")
        equal(actual[role_map[trace.world_seed]]["raw_long"], f"{state['seed']:016x}", "graph AtomicLong equals actual traced seed48")
        equal(old.Observation.field(actual, world_ref, "dp", "C"), cache_ref, "actual graph derived C cache")
        equal(old.Observation.field(actual, world_ref, "dj", "v"), cache_ref, "actual graph base v cache")
        equal(old.Observation.field(actual, cache_ref, "he", "h"), world_ref, "actual graph cache World role")
        rows.append({"operation": boundary["index"], "when": boundary["when"], "state_reference": state_index,
                     "jdi_state_line": state["line"]})
        covered.add(state_index)
    initial_index, final_index = document["initial"]["state_reference"], document["final"]["state_reference"]
    equal(objects[initial_index][world_random]["state"], trace.boundaries[0]["state"]["state"], "complete initial graph/JDI state binding")
    equal(objects[final_index][world_random]["state"], trace.states[("target_exit", "World_r")]["state"],
          "complete final graph/target exit JDI state binding")
    covered.update((initial_index, final_index))
    equal(covered, set(range(len(objects))), "every interned graph state cross-bound through actual operation references")
    owners = sorted({identity for graph in objects for identity, value in graph.items()
                     if value["object"]["class"] == RANDOM})
    all_randoms = []
    for owner in owners:
        occurrences = []
        for index, graph in enumerate(objects):
            if owner in graph:
                value = graph[owner]
                old.random_state(value["state"])
                occurrences.append({"state_reference": index, "seed_owner": value["seed_owner"]["ref"],
                                    "state": value["state"]})
        all_randoms.append({"graph_owner": owner, "jdi_role_binding": "World.r" if owner == world_random else None,
                            "coverage": "complete graph fields/AtomicLong/copy continuation; no independent JDI constructor identity bridge"
                                if owner != world_random else "shared frozen Original.world -> dj.r source-role chain and complete boundary states",
                            "occurrences": occurrences})
    classes.archive.close()
    return {"role_mapping": role_map, "identity_basis": "same actual source field chain plus exact time-indexed complete states; numeric namespaces are independent",
            "boundary_bindings": rows, "graph_random_owners": all_randoms,
            "generator_cave_jdi_identity_bridge": False}


def audit_capture(capture_dir, *, legacy=False):
    """Return a report and observed input bytes after all independent checks.

    This API only reads ordinary files. It neither starts children nor writes
    outputs. Historical captures require explicit legacy=True and retain their
    dirty source attribution; successful auditing never upgrades that evidence.
    """
    need(type(legacy) is bool, "explicit legacy audit selection")
    capture = Path(capture_dir).absolute()
    ordinary(capture, True)
    auditor_before = {name: pin(Path(__file__).with_name(name)) for name in ("audit.py", "semantic.py")}
    report, paths, before_inventory = provenance(capture, legacy=legacy)
    raw = read(capture / "original.json")
    document = semantic.strict_json(raw)
    equal(semantic.encoded(document), raw, "complete original JSON canonical producer encoding")
    equal(report["original_output"], {**pin(capture / "original.json"), "path": "original.json",
          "independent_graph_semantic_audit": False}, "honest mechanical-only graph output receipt")
    nbt_path = ROOT / "scripts/reference/storage/compare_level.py"
    nbt_before = pin(nbt_path)
    nbt = semantic.load_level()
    catalog = Catalog(semantic, capture, paths)
    catalog.trace_protocol = 1 if legacy else 2
    try:
        catalog.audit_jdk(paths)
        findings, closed = semantic.audit_original(document, paths["server_jar"], capture / "original-parent", nbt,
                                                    jdk_version=catalog.jdk_version, legacy=legacy)
        for name, value in closed.items():
            equal(read(capture / "original-parent" / name, 4 * MIB), value,
                  "actual complete final disk bytes after target/observer reap")
        actual_disk = inventory(capture / "original-parent")
        equal({name for name, value in actual_disk.items() if value["kind"] == "file"}, set(closed),
              "every final file matches independently decoded graph disk blobs")
        equal(sorted(name for name, value in actual_disk.items() if value["kind"] == "directory"),
              [value["path"] for value in document["disk_after_close"]["entries"] if value["kind"] == "directory"],
              "all final empty and nonempty directories independently reread")
        trace = Trace(read(capture / "observer.stdout", 16 * MIB), catalog, semantic)
        trace.audit_events()
        binding = bind_graph(document, findings, trace, semantic, paths["server_jar"])
        tape, attribution = trace.input_tape()
        equal(report["observer_trace"], {
            "path": "observer.stdout", "rows": len(trace.rows),
            "reported_counts": {key: {row["fields"][index]: number(row["fields"][index + 1])
                                     for index in range(1, len(row["fields"]), 2)} for key, row in trace.counts.items()},
            **pin(capture / "observer.stdout"), "independent_event_semantic_audit": False},
            "mechanical complete trace envelope agrees with independently audited events")
        summary = {
            "schema": 2, "scope": "independent original dedicated constructor, trace, graph and disk audit",
            "passed": True, "semantic_audit_complete": True, "native_comparison": False,
            "qualification": False, "qualification_complete": False, "legacy_capture": legacy,
            "tested_commit": report["tested_commit"], "source_dirty": report["source_dirty"],
            "source_head": report["source_head"], "source_tree": report.get("source_tree"),
            "source_context": report["source_before"],
            "clean_source_attribution": not legacy and report["source_dirty"] is False and report["tested_commit"] is not None,
            "capture": str(capture), "capture_report": pin(capture / "report.json"),
            "original_output": pin(capture / "original.json"), "observer_trace": pin(capture / "observer.stdout"),
            "audit_inputs": {**auditor_before, "pure_nbt_parser": nbt_before},
            "recorded_reaped_children": len(report["processes"]), "retained_entries": len(before_inventory),
            "aggregate_trace_counts": trace.aggregate_counts, "constructor_inputs": attribution,
            "graph_and_disk": findings, "graph_trace_binding": binding, "Math": trace.math,
            "actual_World_r_calls": [{
                "index": draw["index"], "method": draw["method"], "descriptor": draw["descriptor"],
                "depth": draw["depth"], "classification": draw["classification"], "arguments": draw["arguments"],
                "returned": draw["returned"], "entry_line": draw["line"], "return_line": draw["return_line"],
                "before": draw["before"]["state"], "after": draw["after"]["state"], "source_stack": draw["source_stack"]}
                for draw in trace.draws],
            "source_class_metadata": {name: {key: value[key] for key in ("fingerprint", "origin", "major", "super")}
                                      for name, value in catalog.cache.items()},
            "selected_jdk": {"version": catalog.jdk_version, "runtime_image_class_binding": "all used JDK class bytes equal selected jmods",
                             "instruction_semantics": catalog.semantic_shapes, "source_members": catalog.source_bindings},
            "reported_generated_reflection_bridges": catalog.generated,
            "unbound_held_Experiment_mirrors": trace.unbound_helper_mirrors,
            "limits": [
                "Original-only instrumented seed23 evidence. No C executable, C output or native expected state is an audit input.",
                "Debugger stops and mirror retention change scheduling, clocks and lifetime; this does not establish uninstrumented execution or OS-clock equivalence.",
                "Constructor allocation callsites are checked against selected runtime, helper and original bytecode. Generated reflection bridges retain complete reported frames/signatures/loaders; their bytes were not captured.",
                "JDI and graph numeric namespaces have no direct identity bridge. World.r binds through the same actual source field chain and complete boundary states. Generator/cave and other graph Random owners lack an independent JDI constructor-role bridge.",
                "Math initialization and complete boundary states are observed; subsequent Math draw methods are not traced.",
                "initialScramble return checks and System.exit direct-caller checks are producer assertions without individual raw value/stack rows; constructor relations and every available source stack are independently checked.",
                "Only reached fields/arrays/collections, selected statics and retained disk tree are represented. Arbitrary statics, JDK collection capacity, whole heap and transitive OS/Python dependencies remain outside this schema.",
                "Graph helper checks the inert daemon and region close but records no post-close graph. Final disk files and directories are independently reread after both owned processes reap.",
                "Nonempty Entity/Tile serialization and uncached nextGaussian need additional concrete codec/StrictMath support and are rejected rather than normalized.",
                "No running server loop, general failure policy, Player support, general restart or full-game/native qualification follows from this finite profile.",
            ],
        }
    finally:
        catalog.close()
    for name, value in report["inputs_before"].items():
        equal(pin(Path(value["path"])), {key: value[key] for key in ("bytes", "sha256")}, "selected input unchanged through full audit: " + name)
    equal(inventory(capture), before_inventory, "complete retained capture unchanged by independent audit")
    equal(pin(nbt_path), nbt_before, "pure NBT support unchanged through audit")
    for name, expected in auditor_before.items():
        equal(pin(Path(__file__).with_name(name)), expected, "public semantic audit source unchanged")
    return summary, tape


def output_path(value, capture, suffix):
    path = Path(value).absolute()
    need(path.suffix == suffix and not path.is_relative_to(capture), "separate audit output outside capture with expected suffix")
    ordinary(path.parent, True)
    need(not path.exists() and not path.is_symlink(), "new exclusive audit output preserves prior evidence")
    return path


def write_outputs(summary, tape, report_path, tape_path, capture_dir):
    """Exclusively retain passing audited inputs and their complete report."""
    need(summary.get("passed") is True and summary.get("semantic_audit_complete") is True,
         "only complete successful independent audit can emit constructor inputs")
    need(type(tape) is bytes and tape.startswith(TAPE_HEADER), "complete exact audited constructor input tape")
    capture = Path(capture_dir).absolute()
    output = output_path(report_path, capture, ".json")
    inputs = output_path(tape_path, capture, ".txt")
    need(output != inputs, "distinct JSON audit report and constructor tape destinations")
    result = dict(summary)
    result["failure"] = None
    result["constructor_input_tape"] = {
        "path": str(inputs), "rows": result["constructor_inputs"]["selected_rows"],
        "bytes": len(tape), "sha256": hashlib.sha256(tape).hexdigest(),
        "format": "minecraft-c-random-constructor-inputs-v1; full committed U hex TAB canonical signed int64 nanoTime LF",
        "native_expected_state": False,
    }
    with inputs.open("xb") as stream:
        stream.write(tape)
    equal(read(inputs, 4 * MIB), tape, "exact retained audited constructor input tape")
    result["constructor_input_tape_written"] = True
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, indent=2, ensure_ascii=True, allow_nan=False)
        stream.write("\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--constructor-inputs", type=Path, required=True)
    parser.add_argument("--legacy", action="store_true", help="explicitly audit historical dirty schema1 research without qualification")
    args = parser.parse_args()
    capture = args.capture.absolute()
    output = output_path(args.output, capture, ".json")
    output_path(args.constructor_inputs, capture, ".txt")
    try:
        summary, tape = audit_capture(capture, legacy=args.legacy)
        write_outputs(summary, tape, output, args.constructor_inputs, capture)
        print("PASS: independent original dedicated JDI inputs, complete available trace, graph, typed NBT and final disk")
        print("Constructor input tape:", args.constructor_inputs.absolute())
        print("Audit report:", output)
        return 0
    except Exception as error:
        failure = {"schema": 2, "passed": False, "semantic_audit_complete": False, "native_comparison": False,
                   "qualification": False, "qualification_complete": False, "tested_commit": None,
                   "capture": str(capture), "legacy_capture": args.legacy, "constructor_input_tape_written": False,
                   "failure": f"{type(error).__name__}: {error}", "traceback": traceback.format_exc()}
        if not output.exists():
            with output.open("x", encoding="utf-8", newline="\n") as stream:
                json.dump(failure, stream, indent=2, ensure_ascii=True, allow_nan=False)
                stream.write("\n")
        print("FAIL:", failure["failure"], file=sys.stderr)
        print("Audit report:", output, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
