"""Java-free guards for copied-world acceptance tooling.

All worlds and process results constructed here belong to TemporaryDirectory.
They test the acceptance oracle, not original compatibility or C execution.
"""
from __future__ import annotations

import argparse
import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts/reference/storage"))
try:
    import compare_world_storage as world
finally:
    sys.path.pop(0)

level = world.level


def chunk_tree(x=0, z=0, opaque=False):
    """Constructed complete schema, with air at the demonstrated index 64."""
    blocks = bytearray(b"\x01" * 32768)
    blocks[64], blocks[65] = 0, 97
    fields = [level.integer("xPos", x), level.integer("zPos", z),
              level.long("LastUpdate", 17), level.tag(1, "TerrainPopulated", 0)]
    for name, raw in (("Blocks", bytes(blocks)), ("Data", b"\x12" * 16384),
                      ("SkyLight", b"\x34" * 16384), ("BlockLight", b"\x56" * 16384),
                      ("HeightMap", bytes(range(256)))):
        fields.append(level.tag(7, name, raw.hex()))
    for name in ("Entities", "TileEntities"):
        items = [level.compound("", [level.string("id", "ConstructedOpaque"),
                                    level.string("unknown", "NUL\0\ud800")])] if opaque else []
        fields.append(level.tag(9, name, {"element_type": 10, "items": items}))
    return level.compound("", [level.compound("Level", fields)])


def metadata_tree():
    return level.compound("", [level.compound("Data", [
        level.long("RandomSeed", -23), level.integer("SpawnX", -17),
        level.integer("SpawnY", 64), level.integer("SpawnZ", 32),
        level.long("Time", 9876), level.long("LastPlayed", 4567),
        level.long("SizeOnDisk", 8192), level.string("LevelName", "constructed\0\ud800"),
        level.integer("version", 19131), level.integer("rainTime", 23),
        level.tag(1, "raining", 1), level.integer("thunderTime", 24),
        level.tag(1, "thundering", 0)])])


def normalized_map():
    out = bytearray(range(256))
    out[97] = 0
    return bytes(out)


def saved_tree(source, dimension=0):
    out = copy.deepcopy(source)
    current = level.child(out, "Level")
    blocks = level.child(current, "Blocks")
    blocks["value"] = bytes(normalized_map()[byte] for byte in bytes.fromhex(blocks["value"])).hex()
    level.child(current, "LastUpdate")["value"] = 1234 if dimension == 0 else 5678
    return out


def changed_byte(tree, name, index=0, value=255):
    array = level.child(level.child(tree, "Level"), name)
    raw = bytearray.fromhex(array["value"])
    raw[index] = value
    array["value"] = raw.hex()


def gzip_nbt(tree):
    stream = zlib.compressobj(wbits=31)
    return stream.compress(level.encode(tree)) + stream.flush()


def region_image(tree, x=0, z=0, timestamp=0x89abcdef):
    """One constructed record, without relying on the driver's region writer."""
    compressed = zlib.compress(level.encode(tree))
    payload = struct.pack(">I", len(compressed) + 1) + b"\x02" + compressed
    sectors = (len(payload) + 4095) // 4096
    image = bytearray((2 + sectors) * 4096)
    index = (x & 31) + 32 * (z & 31)
    struct.pack_into(">I", image, index * 4, (2 << 8) | sectors)
    struct.pack_into(">I", image, 4096 + index * 4, timestamp)
    image[8192:8192 + len(payload)] = payload
    return bytes(image)


def install_records(parent, records, timestamp=0x89abcdef):
    """Construct complete files by appending independent records to fresh images."""
    groups = {}
    for (dimension, x, z), tree in records.items():
        name = ("DIM-1/" if dimension == -1 else "") + f"region/r.{x >> 5}.{z >> 5}.mcr"
        groups.setdefault(name, []).append((x, z, tree))
    for name, entries in groups.items():
        image = bytearray(8192)
        for x, z, tree in entries:
            compressed = zlib.compress(level.encode(tree))
            payload = struct.pack(">I", len(compressed) + 1) + b"\x02" + compressed
            sectors = (len(payload) + 4095) // 4096
            start = len(image) // 4096
            index = (x & 31) + 32 * (z & 31)
            struct.pack_into(">I", image, index * 4, (start << 8) | sectors)
            struct.pack_into(">I", image, 4096 + index * 4, timestamp)
            image.extend(payload + bytes(sectors * 4096 - len(payload)))
        path = parent / "world" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(image)


def info_view():
    return {"seed": "ffffffffffffffe9", "spawn": [-17, 64, 32],
            "time": "0000000000002694", "last_played": "00000000000011d7",
            "size_on_disk": "0000000000002000", "level_name": level.u16("constructed\0\ud800"),
            "version": 19131, "rain_time": 23, "raining": True,
            "thunder_time": 24, "thundering": False,
            "has_player": False, "player_dimension": 0}


def original_info_view():
    return {"seed": "ffffffffffffffe9", "spawn_x": -17, "spawn_y": 64, "spawn_z": 32,
            "time": "0000000000002694", "last_played": "00000000000011d7",
            "size_on_disk": "0000000000002000", "player": False, "player_dimension": 0,
            "level_name": "constructed\0\ud800", "version": 19131,
            "raining": True, "rain_time": 23, "thundering": False, "thunder_time": 24}


def populated_world(parent):
    directory = parent / "world"
    (directory / "region").mkdir(parents=True)
    (directory / "level.dat").write_bytes(gzip_nbt(metadata_tree()))
    (directory / "level.dat_old").write_bytes(gzip_nbt(metadata_tree()))
    (directory / "session.lock").write_bytes(bytes.fromhex("0123456789abcdef"))
    (directory / "region/r.0.0.mcr").write_bytes(region_image(saved_tree(chunk_tree())))
    (directory / "unrelated.bin").write_bytes(b"constructed unrelated bytes\0\xff")
    return {p.relative_to(directory).as_posix(): p.read_bytes() for p in directory.rglob("*") if p.is_file()}


def stage_snapshot(source, dimension=0, x=0, z=0, stage="direct_load", identity=7):
    current = level.child(saved_tree(source, dimension), "Level")
    data = {}
    for role, key in (("blocks", "Blocks"), ("metadata", "Data"), ("skylight", "SkyLight"),
                      ("block_light", "BlockLight"), ("heightmap", "HeightMap")):
        raw = bytes.fromhex(level.child(current, key)["value"])
        data[role] = world.fp(raw) | {"hex": raw.hex()}
    # These constructed cases use acquired ordinary (0,0) or unloaded extremes.
    active = stage == "acquire" or (stage == "direct_write" and (x, z) == (0, 0))
    return {"id": identity, "dimension": dimension, "coordinates": [x, z], "world_matches": True,
            "data": data, "minimum": 0, "flags": {"c": active, "n": False, "o": False, "p": False, "q": False},
            "last_save": f"{(1234 if dimension == 0 else 5678) if active else 0:016x}",
            "tiles": 0, "entities": [0] * 8}


def process_record(command, label, stdout=b"", stderr=b"", returncode=0):
    return {"command": list(map(str, command)), "returncode": returncode, "timed_out": False,
            "stop_reason": None, "seconds": 0.01,
            "stdout_file": label + ".stdout", "stderr_file": label + ".stderr",
            "stdout": world.fp(stdout), "stderr": world.fp(stderr)}


class WrittenChunkGuards(unittest.TestCase):
    def setUp(self):
        self.source = chunk_tree()
        self.stored = saved_tree(self.source)

    def validate(self, stored=None, **changes):
        arguments = dict(stored_tree=self.stored if stored is None else stored,
                         source_tree=self.source, block_map=normalized_map(),
                         dimension=0, x=0, z=0, world_time=1234,
                         actual_timestamp=0x89abcdef, expected_timestamp=0x89abcdef)
        arguments.update(changes)
        return world.validate_written_chunk(**arguments)

    def test_complete_constructed_saved_chunk_accepts_map_once(self):
        self.validate()
        self.assertEqual(bytes.fromhex(level.child(level.child(self.stored, "Level"), "Blocks")["value"])[64:66], b"\0\0")

    def test_raw_disk_unknown_substituted_for_air_is_not_normalized_twice(self):
        changed = copy.deepcopy(self.stored)
        changed_byte(changed, "Blocks", 64, 97)
        self.assertEqual(normalized_map()[97], 0)
        self.assertEqual(bytes.fromhex(level.child(level.child(self.source, "Level"), "Blocks")["value"])[64], 0)
        with self.assertRaises(world.Failure):
            self.validate(changed)

    def test_each_raw_array_byte_and_exact_shape_is_compared(self):
        for name in ("Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"):
            for mutation in ("byte", "short", "missing", "wrong-type"):
                changed = copy.deepcopy(self.stored)
                current = level.child(changed, "Level")
                field = level.child(current, name)
                if mutation == "byte":
                    changed_byte(changed, name)
                elif mutation == "short":
                    field["value"] = field["value"][:-2]
                elif mutation == "missing":
                    current["value"].remove(field)
                else:
                    field.update(type=8, value=level.u16("wrong type"))
                with self.subTest(name=name, mutation=mutation), self.assertRaises(world.Failure):
                    self.validate(changed)

    def test_coordinates_population_and_explicit_world_time_are_exact(self):
        for name, value in (("xPos", 1), ("zPos", -1), ("TerrainPopulated", 1), ("LastUpdate", 17)):
            changed = copy.deepcopy(self.stored)
            level.child(level.child(changed, "Level"), name)["value"] = value
            with self.subTest(name=name), self.assertRaises(world.Failure):
                self.validate(changed)

    def test_timestamp_is_exact_unsigned_32_bits_not_a_signed_or_numeric_alias(self):
        for value in (0x89abcdee, -1985229329, 1 << 32, True, float(0x89abcdef)):
            with self.subTest(value=value), self.assertRaises(world.Failure):
                self.validate(actual_timestamp=value)


class BlobGuards(unittest.TestCase):
    def test_complete_constructed_blob_retains_all_bytes(self):
        raw = b"constructed\0\x80\xff"
        self.assertEqual(world.parse_blob(world.fp(raw) | {"hex": raw.hex()}), raw)

    def test_fingerprint_counts_hex_and_schema_are_exact(self):
        raw = b"\xab\xcd"
        valid = world.fp(raw) | {"hex": raw.hex()}
        changes = ({"bytes": 2.0}, {"bytes": True}, {"sha256": "0" * 64},
                   {"hex": "ABCD"}, {"hex": "ab cd"}, {"hex": "abc"},
                   {"hex": "gg"}, {"extra": "unattributed"})
        for change in changes:
            with self.subTest(change=change), self.assertRaises(world.Failure):
                world.parse_blob(valid | change)
        for alias in (False, 0.0):
            with self.subTest(alias=alias), self.assertRaises(world.Failure):
                world.parse_blob(world.fp(b"") | {"bytes": alias, "hex": ""})


class ProcessStreamGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mc-world-streams-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.command = ["constructed-java", "-version"]
        (self.root / "java.stdout").write_bytes(b"")
        (self.root / "java.stderr").write_bytes(b"")
        self.process = process_record(self.command, "java")

    def validate(self, value=None):
        return world.check_child_streams(self.root, self.process if value is None else value, self.command, "java")

    def test_complete_constructed_canonical_empty_streams_accept(self):
        self.assertEqual(self.validate(), self.process)

    def test_identical_alias_streams_cannot_replace_missing_canonical_paths(self):
        for field in ("stdout", "stderr"):
            canonical = self.root / ("java." + field)
            canonical.unlink()
            (self.root / ("alias." + field)).write_bytes(b"")
            value = copy.deepcopy(self.process)
            value[field + "_file"] = "alias." + field
            with self.subTest(field=field), self.assertRaisesRegex(world.Failure, "canonical"):
                self.validate(value)
            canonical.write_bytes(b"")
        swapped = copy.deepcopy(self.process)
        swapped["stdout_file"], swapped["stderr_file"] = swapped["stderr_file"], swapped["stdout_file"]
        with self.assertRaisesRegex(world.Failure, "canonical"):
            self.validate(swapped)

    def test_actual_canonical_bytes_and_exact_child_command_are_bound(self):
        (self.root / "java.stdout").write_bytes(b"unreported")
        with self.assertRaisesRegex(world.Failure, "retained stdout bytes"):
            self.validate()
        (self.root / "java.stdout").write_bytes(b"")
        with self.assertRaisesRegex(world.Failure, "child command"):
            self.validate(self.process | {"command": ["other-java", "-version"]})

    def test_numeric_aliases_nonfinite_durations_and_extra_keys_fail(self):
        mutations = [lambda row: row["stdout"].update(bytes=False),
                     lambda row: row["stderr"].update(bytes=0.0),
                     lambda row: row.update(seconds=True), lambda row: row.update(seconds=float("nan")),
                     lambda row: row.update(seconds=float("inf")), lambda row: row.update(seconds=-1),
                     lambda row: row.update(seconds=121), lambda row: row.update(returncode=False),
                     lambda row: row.update(host_path="C:/unattributed")]
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(self.process)
            mutate(value)
            with self.subTest(mutation=index), self.assertRaises(world.Failure):
                self.validate(value)

    def test_measured_return77_timeout_and_stop_reason_are_failures(self):
        for changes in ({"returncode": 77}, {"timed_out": True}, {"stop_reason": "limit"}):
            with self.subTest(changes=changes), self.assertRaises(world.Failure):
                self.validate(self.process | changes)


class ExecutionGuards(unittest.TestCase):
    def guard(self, scenario):
        with tempfile.TemporaryDirectory(prefix="mc-world-lifecycle-") as directory:
            root = Path(directory)
            jars = {side: root / (side + ".jar") for side in world.PIN}
            jar_raw = {side: ("constructed-" + side + "-archive").encode("ascii") for side in world.PIN}
            for side, path in jars.items():
                path.write_bytes(jar_raw[side])
            tools = {name: root / "jdk/bin" / name for name in ("java", "javac")}
            for path in tools.values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"constructed tool")
            for name, raw in (("release", b'JAVA_VERSION="17.0.1"\n'),
                              ("lib/modules", b"constructed modules"), ("lib/ct.sym", b"constructed symbols")):
                path = root / "jdk" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(raw)
            executable = root / "probe"
            executable.write_bytes(b"constructed native probe; never executed")
            real_fp = world.fp
            calls = []

            def fingerprint(raw):
                value = real_fp(raw)
                for side, expected in jar_raw.items():
                    if raw == expected:
                        value["sha256"] = world.PIN[side]
                return value

            def tool_path(selected, name):
                if scenario in ("missing_tool", "jar_changed_before_missing_tool"):
                    if scenario == "jar_changed_before_missing_tool":
                        jars["client"].write_bytes(b"changed previously verified archive")
                    raise world.Unavailable("constructed missing JDK")
                return tools[name]

            def process(command, where, label, timeout=90, stdin=None, stdout_limit=32768):
                calls.append(label)
                where.mkdir(parents=True, exist_ok=True)
                stdout, stderr, code = b"", b"", 0
                if label in ("java", "javac"):
                    stderr = (b'openjdk version "17.0.1"\n' if label == "java" else b"javac 17.0.1\n")
                    if scenario == "version77":
                        code = 77
                elif label == "compile":
                    classes = where / "classes"
                    classes.mkdir(exist_ok=True)
                    for name in ("WorldStorageObservation.class", "WorldStorageObservation$Context.class", "WorldOpaqueObservation.class"):
                        (classes / name).write_bytes(bytes.fromhex("cafebabe0000003d") + b"constructed")
                    if scenario == "compile77":
                        code = 77
                else:
                    self.assertEqual(label, "client-prepare-observed")
                    self.assertEqual(list(map(str, command))[1:5], ["-Xmx512m", "-cp", str(where / "classes"), "WorldStorageObservation"])
                    code, stderr = 77, b"constructed measured child unavailable\n"
                    if scenario == "helper_changed":
                        with (where / "classes/WorldStorageObservation.class").open("ab") as stream:
                            stream.write(b"changed")
                    elif scenario == "helper_truncated":
                        (where / "classes/WorldStorageObservation.class").write_bytes(bytes.fromhex("cafebabe"))
                (where / (label + ".stdout")).write_bytes(stdout)
                (where / (label + ".stderr")).write_bytes(stderr)
                if stdin is not None:
                    (where / (label + ".stdin")).write_bytes(stdin)
                value = process_record(command, label, stdout, stderr, code)
                if label == "compile" and scenario == "compiler_false_fingerprint":
                    value["stdout"]["bytes"] = False
                return value

            argv = ["--client", str(jars["client"]), "--server", str(jars["server"]),
                    "--executable", str(executable), "--java", str(tools["java"]), "--javac", str(tools["javac"]),
                    "--work-dir", str(root / "captures")]
            if scenario == "wrong_jar":
                jars["client"].write_bytes(b"wrong archive")
            if scenario == "invalid_source_label":
                argv += ["--tested-commit", "short"]
            environment = {key: value for key, value in os.environ.items() if key not in world.JAVA_ENVIRONMENT}
            if scenario == "environment":
                environment["JAVA_TOOL_OPTIONS"] = "-Dconstructed=true"
            with patch.dict(os.environ, environment, clear=True), \
                 patch.object(world, "fp", side_effect=fingerprint), \
                 patch.object(world.r, "tool_path", side_effect=tool_path), \
                 patch.object(world.r, "process", side_effect=process), \
                 contextlib.redirect_stdout(io.StringIO()):
                code = world.main(argv)
            reports = list((root / "captures").glob("capture-*/manifest.json"))
            self.assertEqual(len(reports), 1)
            report = level.strict_json(reports[0].read_bytes())
            retained = {p.relative_to(reports[0].parent).as_posix(): p.read_bytes()
                        for p in reports[0].parent.rglob("*.stderr")}
            return code, report, calls, retained

    def test_only_missing_preflight_prerequisite_returns77(self):
        code, report, calls, _ = self.guard("missing_tool")
        self.assertEqual((code, report["status"], report["measurement_started"], calls), (77, "unavailable", False, []))

    def test_actual_version_compile_and_original77_are_failures_with_raw_streams(self):
        for scenario, last in (("version77", "java"), ("compile77", "compile"), ("child77", "client-prepare-observed")):
            code, report, calls, retained = self.guard(scenario)
            with self.subTest(scenario=scenario):
                self.assertEqual((code, report["status"], calls[-1]), (1, "fail", last))
                self.assertTrue(any("complete successful process" in error for error in report["errors"]))
                self.assertTrue(any(name.endswith(last + ".stderr") for name in retained))
                if scenario == "child77":
                    self.assertIs(report["measurement_started"], True)
                    self.assertTrue(any(raw == b"constructed measured child unavailable\n" for raw in retained.values()))

    def test_wrong_archive_environment_and_source_label_fail_before_children(self):
        reasons = {"wrong_jar": "wrong supplied client archive", "environment": "injected Java environment",
                   "invalid_source_label": "explicit full40 tested commit"}
        for scenario in ("wrong_jar", "environment", "invalid_source_label"):
            code, report, calls, _ = self.guard(scenario)
            with self.subTest(scenario=scenario):
                self.assertEqual((code, report["status"], calls), (1, "fail", []))
                self.assertTrue(any(reasons[scenario] in error for error in report["errors"]))

    def test_earlier_verified_archive_mutation_overrides_missing_jdk(self):
        code, report, calls, _ = self.guard("jar_changed_before_missing_tool")
        self.assertEqual((code, report["status"], calls), (1, "fail", []))
        self.assertTrue(any("preserved" in error for error in report["errors"]))
        archives = [name for name in report["inputs_before"] if Path(name).name == "client.jar"]
        self.assertEqual(len(archives), 1)
        self.assertEqual(report["inputs_before"][archives[0]]["sha256"], world.PIN["client"])
        self.assertNotEqual(report["inputs_before"][archives[0]], report["inputs_after"][archives[0]])

    def test_changed_and_truncated_helper_retain_failure_manifests(self):
        for scenario in ("helper_changed", "helper_truncated"):
            code, report, calls, _ = self.guard(scenario)
            with self.subTest(scenario=scenario):
                self.assertEqual((code, report["status"], calls[-1]), (1, "fail", "client-prepare-observed"))
                self.assertTrue(any("helper" in error or "Java17" in error for error in report["errors"]))

    def test_numeric_compiler_stream_fingerprint_fails_before_class_inventory(self):
        code, report, calls, _ = self.guard("compiler_false_fingerprint")
        self.assertEqual((code, report["status"], calls[-1]), (1, "fail", "compile"))
        self.assertNotIn("classes_before", report)
        self.assertTrue(any("fingerprint bytes" in error for error in report["errors"]))

    def test_unavailable_raised_after_measurement_start_is_retained_failure(self):
        def measured(args, directory, report):
            report["measurement_started"] = True
            raise world.Unavailable("constructed unavailability after measurement")
        with tempfile.TemporaryDirectory(prefix="mc-world-measured-") as directory:
            root = Path(directory)
            with patch.object(world, "execute", side_effect=measured), contextlib.redirect_stdout(io.StringIO()):
                code = world.main(["--work-dir", str(root)])
            manifests = list(root.glob("capture-*/manifest.json"))
            self.assertEqual(len(manifests), 1)
            report = level.strict_json(manifests[0].read_bytes())
            self.assertEqual((code, report["status"], report["measurement_started"]), (1, "fail", True))
            self.assertEqual(report["errors"], ["constructed unavailability after measurement"])
            self.assertNotIn("unavailable_reason", report)


class SourceBindingGuards(unittest.TestCase):
    def capture(self, mutation=None):
        """Supply a constructed Git response; never invoke Git or alter sources."""
        with tempfile.TemporaryDirectory(prefix="mc-world-source-") as directory:
            root = Path(directory)
            paths = world.source_paths()
            report = {}

            def process(command, where, label, timeout=90, stdin=None, stdout_limit=32768):
                if label == "git-kind":
                    self.assertEqual(command, ["git", "cat-file", "-t", "a" * 40])
                    stdout = b"tree\n" if mutation == "not-commit" else b"commit\n"
                    (where / "git-kind.stdout").write_bytes(stdout)
                    (where / "git-kind.stderr").write_bytes(b"")
                    return process_record(command, label, stdout)
                self.assertEqual(command, ["git", "ls-tree", "-r", "-z", "a" * 40])
                self.assertEqual(label, "git-source")
                entries = []
                for index, name in enumerate(paths):
                    raw = (ROOT / name).read_bytes()
                    digest = hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()
                    mode, kind = b"100644", b"blob"
                    if index == 0:
                        if mutation == "missing":
                            continue
                        if mutation == "digest":
                            digest = "0" * 40
                        if mutation == "symlink-mode":
                            mode = b"120000"
                    entries.append(mode + b" " + kind + b" " + digest.encode("ascii") + b"\t" + name.encode("utf8") + b"\0")
                stdout = b"".join(entries)
                (where / "git-source.stdout").write_bytes(stdout)
                (where / "git-source.stderr").write_bytes(b"")
                return process_record(command, label, stdout)

            environment = {key: value for key, value in os.environ.items() if key not in world.JAVA_ENVIRONMENT}
            with patch.dict(os.environ, environment, clear=True), patch.object(world.r, "process", side_effect=process):
                sources = world.prepare_sources(argparse.Namespace(tested_commit="a" * 40), root, report)
            self.assertEqual(set(sources), set(paths))
            self.assertGreater(len(sources), 20)
            for name, fingerprint in sources.items():
                self.assertEqual((root / "sources" / name).read_bytes(), (ROOT / name).read_bytes())
                self.assertEqual(fingerprint, world.fp((ROOT / name).read_bytes()))
            return report

    def test_complete_source_closure_copied_and_bound_to_constructed_tree(self):
        report = self.capture()
        self.assertIs(report["git_source_binding"]["all_blob_bytes_match"], True)
        self.assertEqual(report["git_source_binding"]["files"], len(report["source_fingerprints"]))

    def test_omitted_altered_or_symlink_source_cannot_claim_exact_tree(self):
        for mutation in ("missing", "digest", "symlink-mode"):
            with self.subTest(mutation=mutation), self.assertRaisesRegex(world.Failure, "bound to tested Git tree"):
                self.capture(mutation)

    def test_source_label_must_identify_an_actual_commit_not_an_identical_tree(self):
        with self.assertRaisesRegex(world.Failure, "actual Git commit"):
            self.capture("not-commit")

    def test_driver_alias_is_refused_before_source_copy_or_child(self):
        with tempfile.TemporaryDirectory(prefix="mc-world-alias-") as directory:
            root = Path(directory)
            with patch.object(world, "__file__", str(root / "renamed.py")), \
                 patch.object(world.r, "process") as process, \
                 self.assertRaisesRegex(world.Failure, "canonical executing driver"):
                world.prepare_sources(argparse.Namespace(tested_commit=None), root, {})
            process.assert_not_called()
            self.assertFalse((root / "sources").exists())

    def test_loaded_python_bytes_cannot_be_aliased_or_reattributed_to_current_sources(self):
        loaded = copy.deepcopy(world.LOADED_PYTHON)
        self.assertTrue(loaded)
        name = next(iter(loaded))
        for mutation in ("float-count", "changed-digest", "aliased-path"):
            changed = copy.deepcopy(loaded)
            if mutation == "float-count":
                changed[name]["bytes"] = float(changed[name]["bytes"])
            elif mutation == "changed-digest":
                changed[name]["sha256"] = "0" * 64
            else:
                changed[name + ".alias"] = changed.pop(name)
            with tempfile.TemporaryDirectory(prefix="mc-world-loaded-source-") as directory:
                root = Path(directory)
                environment = {key: value for key, value in os.environ.items() if key not in world.JAVA_ENVIRONMENT}
                with patch.dict(os.environ, environment, clear=True), \
                     patch.object(world, "LOADED_PYTHON", changed), \
                     patch.object(world.r, "process") as process, \
                     self.subTest(mutation=mutation), self.assertRaisesRegex(world.Failure, "actual compiled Python bytes"):
                    world.prepare_sources(argparse.Namespace(tested_commit=None), root, {})
                process.assert_not_called()
                self.assertFalse((root / "sources").exists())


class WorkDirectoryGuards(unittest.TestCase):
    def test_safe_new_directory_is_validated_without_being_created(self):
        with tempfile.TemporaryDirectory(prefix="mc-world-output-") as directory:
            path = Path(directory) / "new/nested"
            self.assertEqual(world.validate_work_dir(path), path)
            self.assertFalse(path.exists())

    def test_public_source_layout_and_protected_ancestor_or_alias_are_refused(self):
        for path in (ROOT, ROOT / "src", ROOT / "docs/wiki"):
            with self.subTest(path=path), self.assertRaisesRegex(world.Failure, "source/input layout"):
                world.validate_work_dir(path)
        with tempfile.TemporaryDirectory(prefix="mc-world-protected-") as directory:
            root = Path(directory)
            protected = root / "input.bin"
            protected.write_bytes(b"constructed protected input")
            with self.assertRaisesRegex(world.Failure, "attributed input"):
                world.validate_work_dir(root, [protected])
            with self.assertRaises(world.Failure):
                world.validate_work_dir(root / "child/../", [root])
            self.assertEqual(protected.read_bytes(), b"constructed protected input")

    def test_lexical_symlink_and_reparse_components_are_refused_before_resolution(self):
        with tempfile.TemporaryDirectory(prefix="mc-world-reparse-") as directory:
            root = Path(directory)
            component = root / "component"
            component.mkdir()
            original_lstat = Path.lstat
            for mode, attributes in ((stat.S_IFLNK | 0o777, 0), (stat.S_IFDIR | 0o700, 0x400)):
                def lstat(path, *args, **kwargs):
                    if path == component:
                        return SimpleNamespace(st_mode=mode, st_file_attributes=attributes)
                    return original_lstat(path, *args, **kwargs)
                with self.subTest(mode=mode, attributes=attributes), \
                     patch.object(Path, "lstat", lstat), self.assertRaisesRegex(world.Failure, "link/reparse"):
                    world.validate_work_dir(component / "new")
            self.assertFalse((component / "new").exists())

    def test_unsafe_main_output_returns_failure_record_without_writing_any_capture(self):
        with tempfile.TemporaryDirectory(prefix="mc-world-output-refusal-") as directory:
            root = Path(directory)
            protected = root / "input.jar"
            protected.write_bytes(b"constructed protected original")
            errors = io.StringIO()
            with patch.object(world, "execute") as execute, \
                 contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
                code = world.main(["--client", str(protected), "--work-dir", str(root)])
            report = level.strict_json(errors.getvalue().encode("utf8"))
            self.assertEqual((code, report["status"]), (1, "fail"))
            self.assertTrue(any("attributed input" in error for error in report["errors"]))
            execute.assert_not_called()
            self.assertEqual(list(root.iterdir()), [protected])
            self.assertEqual(protected.read_bytes(), b"constructed protected original")


class MetadataExpectationGuards(unittest.TestCase):
    def test_live_info_changes_only_explicit_version_and_physical_growth(self):
        source = info_view()
        actual = copy.deepcopy(source)
        actual.update(version=19132, size_on_disk="0000000000003000")
        world.validate_live_info(actual, source, 4096)
        self.assertEqual(source, info_view())

    def test_every_known_metadata_field_and_exact_type_is_compared(self):
        source = info_view()
        expected = copy.deepcopy(source)
        expected.update(version=19132, size_on_disk="0000000000003000")
        for key in expected:
            changed = copy.deepcopy(expected)
            value = changed[key]
            if type(value) is bool:
                changed[key] = not value
            elif type(value) is int:
                changed[key] += 1
            elif type(value) is list:
                changed[key][1] += 1
            else:
                changed[key] = "0000000000000000" if key != "level_name" else level.u16("changed")
            with self.subTest(key=key), self.assertRaises(world.Failure):
                world.validate_live_info(changed, source, 4096)
        for key, value in (("version", 19132.0), ("raining", 1), ("spawn", [-17, 64.0, 32])):
            with self.subTest(key=key, alias=True), self.assertRaises(world.Failure):
                world.validate_live_info(expected | {key: value}, source, 4096)

    def test_create_uses_zero_initial_size_without_replacing_loaded_clock(self):
        source = info_view()
        actual = copy.deepcopy(source)
        actual.update(version=19132, size_on_disk="0000000000001000")
        world.validate_live_info(actual, source, 4096, create=True)
        actual["last_played"] = f"{world.CLOCK:016x}"
        with self.assertRaises(world.Failure):
            world.validate_live_info(actual, source, 4096, create=True)


class OriginalInfoAndClockGuards(unittest.TestCase):
    def test_complete_original_info_projection_preserves_exact_source_values(self):
        self.assertEqual(world.original_info(metadata_tree()), original_info_view())

    def test_every_original_info_field_is_bound_except_explicit_time_growth_version(self):
        source = original_info_view()
        actual = source | {"time": "000000000000162e", "size_on_disk": "0000000000003000", "version": 19132}
        world.validate_original_info(actual, source, world_time=5678, physical_growth=4096, version=19132)
        for key, value in actual.items():
            changed = copy.deepcopy(actual)
            if type(value) is bool:
                changed[key] = not value
            elif type(value) is int:
                changed[key] += 1
            else:
                changed[key] = "changed" if key == "level_name" else "0000000000000001"
            with self.subTest(key=key), self.assertRaisesRegex(world.Failure, "every original Info field"):
                world.validate_original_info(changed, source, world_time=5678, physical_growth=4096, version=19132)

    def test_original_info_transition_arguments_reject_numeric_aliases_and_bounds(self):
        source = original_info_view()
        for changes in ({"world_time": True}, {"world_time": 5678.0}, {"world_time": 1 << 63},
                        {"physical_growth": False}, {"physical_growth": 0.0}, {"physical_growth": -1},
                        {"physical_growth": 64 * 1024 * 1024 + 1}, {"version": True}, {"version": 19131.0}):
            with self.subTest(changes=changes), self.assertRaises(world.Failure):
                world.validate_original_info(source, source, **changes)

    def test_exact_bounded_intervals_allow_only_two_ordered_epoch_millisecond_ints(self):
        start = 1660000000000
        for value in ([start, start], [start, start + 120000]):
            self.assertEqual(world.validate_interval(value), value)
        invalid = ([0, (1 << 63) - 1], [start, start + 120001], [start + 1, start],
                   [-1, 0], [start, 1 << 63], [True, start], [start, float(start)],
                   [], [start], [start, start, start], (start, start), None)
        for value in invalid:
            with self.subTest(value=value), self.assertRaisesRegex(world.Failure, "clock interval"):
                world.validate_interval(value)


class StageSnapshotGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mc-world-stage-")
        self.addCleanup(self.temp.cleanup)
        self.baseline = Path(self.temp.name)
        self.source = chunk_tree()
        extreme = ((1 << 31) - 1, -(1 << 31))
        install_records(self.baseline, {(0, 0, 0): self.source,
                                       (-1, 0, 0): self.source,
                                       (0, *extreme): chunk_tree(*extreme)})

    def validate(self, value, stage="direct_load", dimension=0, x=0, z=0):
        return world.validate_stage_chunk(value, self.baseline, normalized_map(), dimension, x, z, stage, 7)

    def test_all_three_observed_stages_require_complete_bound_snapshots(self):
        for stage in ("direct_load", "acquire", "direct_write"):
            for dimension in (0, -1):
                with self.subTest(stage=stage, dimension=dimension):
                    value = stage_snapshot(self.source, dimension, stage=stage)
                    self.assertEqual(self.validate(value, stage, dimension), 7)
                    with self.assertRaises(world.Failure):
                        self.validate(None, stage, dimension)

    def test_rehashed_initial_or_acquired_or_written_blocks_cannot_hide_behind_final_reload(self):
        for stage in ("direct_load", "acquire", "direct_write"):
            value = stage_snapshot(self.source, stage=stage)
            changed = bytearray.fromhex(value["data"]["blocks"]["hex"])
            changed[64] = 97
            value["data"]["blocks"] = world.fp(bytes(changed)) | {"hex": changed.hex()}
            self.assertEqual(world.parse_blob(value["data"]["blocks"])[64], 97)
            with self.subTest(stage=stage), self.assertRaisesRegex(world.Failure, "payload/transient|stage"):
                self.validate(value, stage)

    def test_every_stage_requires_every_array_and_transient_field(self):
        for stage in ("direct_load", "acquire", "direct_write"):
            original = stage_snapshot(self.source, stage=stage)
            for role in original["data"]:
                changed = copy.deepcopy(original)
                del changed["data"][role]
                with self.subTest(stage=stage, missing=role), self.assertRaises(world.Failure):
                    self.validate(changed, stage)
            for field in ("minimum", "last_save", "flags", "world_matches", "id"):
                changed = copy.deepcopy(original)
                del changed[field]
                with self.subTest(stage=stage, missing=field), self.assertRaises(world.Failure):
                    self.validate(changed, stage)

    def test_wrong_stage_flags_clock_minimum_and_identity_are_rejected(self):
        for stage in ("direct_load", "acquire", "direct_write"):
            original = stage_snapshot(self.source, stage=stage)
            mutations = [lambda row: row.update(minimum=1),
                         lambda row: row.update(id=8),
                         lambda row: row.update(last_save="0000000000000001")]
            mutations += [lambda row, flag=flag: row["flags"].update({flag: not row["flags"][flag]}) for flag in "cnopq"]
            for index, mutate in enumerate(mutations):
                changed = copy.deepcopy(original)
                mutate(changed)
                with self.subTest(stage=stage, mutation=index), self.assertRaises(world.Failure):
                    self.validate(changed, stage)

    def test_extreme_direct_write_retains_inactive_zero_stamp(self):
        x, z = (1 << 31) - 1, -(1 << 31)
        value = stage_snapshot(chunk_tree(x, z), x=x, z=z, stage="direct_write")
        self.assertEqual(self.validate(value, "direct_write", x=x, z=z), 7)
        value["flags"]["c"] = True
        value["last_save"] = "00000000000004d2"
        with self.assertRaises(world.Failure):
            self.validate(value, "direct_write", x=x, z=z)

    def test_snapshot_schema_has_exact_types_bounds_and_no_unattributed_fields(self):
        mutations = [lambda row: row.update(id=7.0), lambda row: row.update(id=4097),
                     lambda row: row.update(dimension=0.0), lambda row: row.update(coordinates=[False, 0]),
                     lambda row: row.update(minimum=False), lambda row: row.update(minimum=129),
                     lambda row: row.update(tiles=False), lambda row: row.update(entities=[False] * 8),
                     lambda row: row.update(entities=[0] * 9), lambda row: row["flags"].update(c=0),
                     lambda row: row["data"]["blocks"].update(bytes=32768.0),
                     lambda row: row.update(host_path="C:/unattributed"),
                     lambda row: row["data"].update(unattributed={})]
        for index, mutate in enumerate(mutations):
            value = stage_snapshot(self.source)
            mutate(value)
            with self.subTest(mutation=index), self.assertRaises(world.Failure):
                world.validate_chunk_snapshot(value)


class StateAndThreadGuards(unittest.TestCase):
    def state(self, parent):
        metadata = original_info_view()
        headers = [{"id": identity, "coordinates": [x, 0],
                    "flags": {"c": True, "n": False, "o": False, "p": False, "q": False},
                    "minimum": 0, "last_save": "00000000000004d2"}
                   for x, identity in ((0, 7), (1, 8))]
        return {"dimension": 0, "world_id": 1, "info_id": 2, "storage_id": 3, "metadata": metadata,
                "map": [[0, 7], [65536, 8]], "insertions": [7, 8], "cached_headers": headers,
                "pending": [], "queue_count": 0, "loader_root": str(parent / "world"), "no_sky": False}

    def test_complete_constructed_world_cache_identity_graph_accepts(self):
        with tempfile.TemporaryDirectory(prefix="mc-world-state-") as directory:
            parent = Path(directory)
            value = self.state(parent)
            self.assertEqual(world.validate_state(value, parent), value)

    def test_owner_identity_metadata_types_graph_order_and_dimension_root_are_exact(self):
        mutations = [lambda row: row.update(info_id=row["world_id"]),
                     lambda row: row.update(storage_id=3.0), lambda row: row.update(no_sky=True),
                     lambda row: row.update(queue_count=False), lambda row: row.update(queue_count=131073),
                     lambda row: row["metadata"].update(spawn_y=64.0), lambda row: row["metadata"].update(raining=1),
                     lambda row: row["metadata"].update(host_path="C:/unattributed"),
                     lambda row: row.update(insertions=[8, 7]), lambda row: row["map"][0].__setitem__(1, 8),
                     lambda row: row.update(pending=[0]), lambda row: row.update(loader_root="outside")]
        with tempfile.TemporaryDirectory(prefix="mc-world-state-") as directory:
            parent = Path(directory)
            for index, mutate in enumerate(mutations):
                value = self.state(parent)
                mutate(value)
                with self.subTest(mutation=index), self.assertRaises(world.Failure):
                    world.validate_state(value, parent)

    def test_thread_original_frames_cannot_be_hidden_by_false_summary_flag(self):
        valid = {"id": 1, "name": "main", "daemon": False, "state": "RUNNABLE",
                 "original_frame": False, "frames": ["java.lang.Thread.getAllStackTraces(Thread.java:1)"]}
        world.validate_threads([valid], {"fd"})
        for frame in ("fd.a(SourceFile:17)", "constructed-loader/fd.a(SourceFile:17)"):
            with self.subTest(frame=frame), self.assertRaisesRegex(world.Failure, "no original class"):
                world.validate_threads([valid | {"frames": [frame]}], {"fd"})
        for changes in ({"id": True}, {"original_frame": True}, {"name": "non-daemon-worker"},
                        {"frames": ["x"] * 129}, {"extra": "unattributed"}):
            with self.subTest(changes=changes), self.assertRaises(world.Failure):
                world.validate_threads([valid | changes], {"fd"})
        with self.assertRaises(world.Failure):
            world.validate_threads([valid, copy.deepcopy(valid)], {"fd"})


class BindingGuards(unittest.TestCase):
    def validate_constructed_header(self, side, mutation=None):
        """Isolate header/binding checks; real disk checks have separate guards."""
        with tempfile.TemporaryDirectory(prefix="mc-world-binding-") as directory:
            parent = Path(directory)
            bindings, fields = world.expected_bindings(side, "prepare")
            owners = {row["class"] for row in (*bindings.values(), *fields.values())}
            owners.update("ConstructedExtra" + str(i) for i in range(max(0, 20 - len(owners))))
            archive = parent / "constructed.jar"
            with zipfile.ZipFile(archive, "w") as output:
                for owner in owners:
                    output.writestr(owner + ".class", b"constructed nonexecuted class")
            origin = archive.resolve().as_uri().replace("file:///", "file:/", 1)
            thread = {"id": 1, "name": "main", "daemon": False, "state": "RUNNABLE",
                      "original_frame": False, "frames": []}
            acquisitions = [{"kind": "prepare_acquire", "dimension": dimension,
                             "coordinates": [x, z], "chunk": None}
                            for dimension in (0, -1)
                            for x, z in ((0, 0), (1, 0), (-1, -1), (31, 31), (32, 32), (-32, -33))]
            record = {"schema": 1, "phase": "prepare", "side": side, "mode": "control",
                      "jar_sha256": world.PIN[side], "class_origins": {owner: origin for owner in owners},
                      "block_map": world.fp(normalized_map()) | {"hex": normalized_map().hex()},
                      "events": acquisitions, "result": {}, "bindings": copy.deepcopy(bindings), "fields": copy.deepcopy(fields),
                      "threads_before": [thread], "threads_after": [copy.deepcopy(thread)],
                      "scope": "constructed header only", "control_scope": "constructed header only"}
            if mutation is not None:
                mutation(record)
            expected = {"constructed_lower_file_check": True}
            with patch.object(world, "JARS", {side: archive}, create=True), \
                 patch.object(world, "validate_record", return_value=expected) as lower:
                result = world.validate_original(record, parent)
            lower.assert_called_once_with(record, parent, None)
            return result

    def test_both_constructed_original_headers_require_exact_complete_binding_tables(self):
        for side in ("client", "server"):
            with self.subTest(side=side):
                self.assertEqual(self.validate_constructed_header(side), {"constructed_lower_file_check": True})

    def test_changed_descriptors_modifiers_origins_and_header_types_are_rejected(self):
        mutations = [lambda row: row.update(schema=1.0), lambda row: row.update(schema=True),
                     lambda row: row.update(host_path="C:/unattributed"),
                     lambda row: next(iter(row["bindings"].values())).update(descriptor="()V"),
                     lambda row: next(iter(row["fields"].values())).update(modifiers=2.0),
                     lambda row: row["bindings"].pop(next(iter(row["bindings"]))),
                     lambda row: row["class_origins"].update({next(iter(row["class_origins"])): "file:/wrong.jar"})]
        for index, mutate in enumerate(mutations):
            with self.subTest(mutation=index), self.assertRaises(world.Failure):
                self.validate_constructed_header("client", mutate)


class PreservationGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mc-world-preservation-")
        self.addCleanup(self.temp.cleanup)
        self.parent = Path(self.temp.name)
        self.before = populated_world(self.parent)

    def test_readback_allows_only_the_replaced_session_token(self):
        (self.parent / "world/session.lock").write_bytes(b"\xff" * 8)
        world.preservation(self.before, self.parent, [])
        self.assertEqual((self.parent / "world/level.dat_old").read_bytes(), self.before["level.dat_old"])

    def test_readback_rejects_current_old_and_unrelated_file_mutations(self):
        for name in ("level.dat", "level.dat_old", "region/r.0.0.mcr", "unrelated.bin"):
            path = self.parent / "world" / name
            path.write_bytes(self.before[name] + b"changed")
            with self.subTest(name=name), self.assertRaises(world.Failure):
                world.preservation(self.before, self.parent, [])
            path.write_bytes(self.before[name])

    def test_readback_rejects_missing_or_unexpected_non_session_files(self):
        old = self.parent / "world/level.dat_old"
        old.unlink()
        with self.assertRaises(world.Failure):
            world.preservation(self.before, self.parent, [])
        old.write_bytes(self.before["level.dat_old"])
        for name in ("level.dat_new", "unattributed.bin"):
            extra = self.parent / "world" / name
            extra.write_bytes(b"constructed unexpected file")
            with self.subTest(name=name), self.assertRaises(world.Failure):
                world.preservation(self.before, self.parent, [])
            extra.unlink()

    def test_write_exclusion_is_only_the_selected_region_ranges(self):
        path = self.parent / "world/region/r.0.0.mcr"
        image = bytearray(self.before["region/r.0.0.mcr"])
        struct.pack_into(">I", image, 4096, 23)
        image[8199] ^= 1
        path.write_bytes(image)
        world.preservation(self.before, self.parent, [(0, 0, 0)])
        image[4096 + 4] ^= 1
        path.write_bytes(image)
        with self.assertRaises(world.Failure):
            world.preservation(self.before, self.parent, [(0, 0, 0)])

    def test_selected_sector_padding_and_dead_tail_are_preserved(self):
        path = self.parent / "world/region/r.0.0.mcr"
        before = self.before["region/r.0.0.mcr"]
        record_length = struct.unpack_from(">I", before, 8192)[0]
        for index in (8192 + 4 + record_length, len(before) - 1):
            raw = bytearray(before)
            raw[index] ^= 1
            path.write_bytes(raw)
            with self.subTest(index=index), self.assertRaises(world.Failure):
                world.preservation(self.before, self.parent, [(0, 0, 0)])

    def test_metadata_only_save_does_not_exclude_any_region_record_or_header(self):
        (self.parent / "world/level.dat").write_bytes(self.before["level.dat"] + b"explicit metadata change")
        world.preservation(self.before, self.parent, [], metadata_write=True)
        path = self.parent / "world/region/r.0.0.mcr"
        raw = bytearray(path.read_bytes())
        raw[4096] ^= 1
        path.write_bytes(raw)
        with self.assertRaises(world.Failure):
            world.preservation(self.before, self.parent, [], metadata_write=True)


class WrittenDiskGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mc-world-written-disk-")
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.source, self.written = root / "source", root / "written"
        for parent in (self.source, self.written):
            (parent / "world").mkdir(parents=True)
        self.sources = {tuple(slot): chunk_tree(slot[1], slot[2]) for slot in world.SLOTS}
        self.records = {slot: saved_tree(tree, slot[0]) for slot, tree in self.sources.items()}
        install_records(self.source, self.sources)
        install_records(self.written, self.records)
        (self.source / "world/level.dat").write_bytes(gzip_nbt(metadata_tree()))
        self.live = info_view() | {"version": 19132}
        self.metadata = metadata_tree()
        data = level.child(self.metadata, "Data")
        level.child(data, "version")["value"] = 19132
        level.child(data, "LastPlayed")["value"] = world.CLOCK
        self.write_metadata()

    def write_metadata(self):
        (self.written / "world/level.dat").write_bytes(gzip_nbt(self.metadata))

    def validate(self):
        return world.validate_written_disk(self.written, self.source, normalized_map(), self.live)

    def test_all_sixteen_constructed_slots_and_complete_disk_metadata_accept(self):
        self.assertEqual(len(self.records), 16)
        self.validate()

    def test_actual_raw_disk_blocks_cannot_hide_unknown_behind_second_mapping(self):
        changed_byte(self.records[(0, 0, 0)], "Blocks", 64, 97)
        install_records(self.written, self.records)
        with self.assertRaisesRegex(world.Failure, "raw|map-once|normalization"):
            self.validate()

    def test_exact_selected_header_timestamp_is_checked_on_disk(self):
        path = self.written / "world/region/r.0.0.mcr"
        raw = bytearray(path.read_bytes())
        struct.pack_into(">I", raw, 4096, 0x89abcdee)
        path.write_bytes(raw)
        with self.assertRaisesRegex(world.Failure, "timestamp|clock"):
            self.validate()

    def test_changed_known_disk_metadata_is_not_a_clock_exclusion(self):
        level.child(level.child(self.metadata, "Data"), "SpawnY")["value"] += 1
        self.write_metadata()
        with self.assertRaisesRegex(world.Failure, "metadata"):
            self.validate()

    def test_missing_selected_slot_is_not_ignored(self):
        path = self.written / "world/DIM-1/region/r.0.0.mcr"
        raw = bytearray(path.read_bytes())
        struct.pack_into(">I", raw, 4, 0)
        path.write_bytes(raw)
        with self.assertRaisesRegex(world.Failure, "slot|extent|record"):
            self.validate()


class ControlFileGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mc-world-controls-")
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.left, self.right = root / "observed", root / "control"
        populated_world(self.left)
        populated_world(self.right)
        self.writes = [{"dimension": 0, "coordinates": [0, 0]}]

    def test_only_explicit_metadata_clock_and_written_timestamp_cells_are_excluded(self):
        (self.right / "world/session.lock").write_bytes(b"\xfe" * 8)
        for name in ("level.dat", "level.dat_old"):
            tree = metadata_tree()
            level.child(level.child(tree, "Data"), "LastPlayed")["value"] += 1
            (self.right / "world" / name).write_bytes(gzip_nbt(tree))
        path = self.right / "world/region/r.0.0.mcr"
        raw = bytearray(path.read_bytes())
        struct.pack_into(">I", raw, 4096, 0x89abcdee)
        path.write_bytes(raw)
        differences = world.compare_files(self.left, self.right, self.writes)
        self.assertEqual({row["file"] for row in differences},
                         {"session.lock", "level.dat", "level.dat_old", "region/r.0.0.mcr"})

    def test_known_metadata_change_survives_clock_normalization(self):
        tree = metadata_tree()
        level.child(level.child(tree, "Data"), "SpawnY")["value"] += 1
        (self.right / "world/level.dat").write_bytes(gzip_nbt(tree))
        with self.assertRaisesRegex(world.Failure, "metadata"):
            world.compare_files(self.left, self.right, self.writes)

    def test_unwritten_timestamp_and_payload_bytes_are_not_excluded(self):
        path = self.right / "world/region/r.0.0.mcr"
        before = path.read_bytes()
        for index in (4100, 8199):
            raw = bytearray(before)
            raw[index] ^= 1
            path.write_bytes(raw)
            with self.subTest(index=index), self.assertRaisesRegex(world.Failure, "outside written timestamp"):
                world.compare_files(self.left, self.right, self.writes)

    def test_control_file_set_and_owned_loader_paths_are_complete(self):
        (self.right / "world/level.dat_old").unlink()
        with self.assertRaisesRegex(world.Failure, "inventory"):
            world.compare_files(self.left, self.right, self.writes)
        with self.assertRaisesRegex(world.Failure, "owned loader"):
            world.control_projection({"loader_root": str(self.right / "outside")}, self.left)


class OpaqueTransportOracleGuards(unittest.TestCase):
    def run_constructed_transport(self, mutation=None):
        """Mock only C execution; check the actual driver's complete disk oracle."""
        with tempfile.TemporaryDirectory(prefix="mc-world-opaque-") as directory:
            parent = Path(directory)
            populated_world(parent)
            source = chunk_tree(1, 0, opaque=True)
            # Opaque transport permits unchanged arrays; keep this positive
            # constructed case free of registry normalization edits.
            changed_byte(source, "Blocks", 65, 1)
            source["value"].append(level.string("OpaqueRoot", "root\0\ud800"))
            current = level.child(source, "Level")
            current["value"].append(level.string("OpaqueLevel", "level\0\udc00"))
            for name in ("Entities", "TileEntities"):
                items = level.child(current, name)["value"]["items"]
                second = copy.deepcopy(items[0])
                level.child(second, "unknown")["value"] = level.u16("second opaque entry")
                items.append(second)
            region = parent / "world/region/r.0.0.mcr"
            region.write_bytes(region_image(source, 1, 0))

            def batch(target, actual_parent, commands, label):
                self.assertEqual((target, actual_parent, label), ("constructed", parent, "opaque-C"))
                stored = saved_tree(source)
                if mutation is not None:
                    mutation(stored)
                replacement = region_image(stored, 1, 0)
                retained = bytearray(region.read_bytes())
                prefix_end = 8192 + 4 + struct.unpack_from(">I", replacement, 8192)[0]
                retained[4:8] = replacement[4:8]
                retained[4100:4104] = replacement[4100:4104]
                retained[8192:prefix_end] = replacement[8192:prefix_end]
                region.write_bytes(retained)
                metadata = metadata_tree()
                data = level.child(metadata, "Data")
                level.child(data, "version")["value"] = 19132
                level.child(data, "LastPlayed")["value"] = world.CLOCK
                (parent / "world/level.dat").write_bytes(gzip_nbt(metadata))
                return [(command, {"handle": 1, "chunk": world.projected_chunk(source), "growth": 0})
                        for command in commands]

            with patch.object(world, "BLOCK_MAP", normalized_map(), create=True), \
                 patch.object(world, "c_batch", side_effect=batch):
                return world.C_opaque("constructed", parent, source)

    def test_full_typed_opaque_tree_accepts_constructed_transport(self):
        result = self.run_constructed_transport()
        self.assertIs(result["typed_tree_retained"], True)
        self.assertEqual(result["growth"], 0)

    def test_entity_tile_and_unknown_root_level_values_cannot_be_normalized_away(self):
        def opaque_value(tree, name):
            records = level.child(level.child(tree, "Level"), name)["value"]["items"]
            level.child(records[0], "unknown")["value"] = level.u16("changed")

        mutations = {
            "entity": lambda tree: opaque_value(tree, "Entities"),
            "tile": lambda tree: opaque_value(tree, "TileEntities"),
            "root": lambda tree: level.child(tree, "OpaqueRoot").update(value=level.u16("changed")),
            "level": lambda tree: level.child(level.child(tree, "Level"), "OpaqueLevel").update(value=level.u16("changed")),
            "entity-order": lambda tree: level.child(level.child(tree, "Level"), "Entities")["value"]["items"].reverse(),
            "tile-order": lambda tree: level.child(level.child(tree, "Level"), "TileEntities")["value"]["items"].reverse(),
        }
        for name, mutation in mutations.items():
            with self.subTest(name=name), self.assertRaisesRegex(world.Failure, "opaque|typed"):
                self.run_constructed_transport(mutation)


class OpaqueOriginalChunkGuards(unittest.TestCase):
    def setUp(self):
        self.value = stage_snapshot(chunk_tree(1, 0), x=1)
        del self.value["dimension"]
        self.value["flags"]["q"] = True
        self.value["entities"] = [{"slice": 4, "class": "ConstructedEntity", "id": 11}]
        self.value["tiles"] = [{"class": "yk", "id": 12, "key_class": "wf",
                                "position": {"a": 0, "b": 64, "c": 0}}]
        self.expected_data = {role: {"length": blob["bytes"], "values": blob["hex"]}
                              for role, blob in self.value["data"].items()}
        self.expected_data["minimum_height"] = 0

    def validate(self, value=None):
        return world.original_chunk(self.value if value is None else value, self.expected_data,
                                    "ConstructedEntity", "yk")

    def test_complete_constructed_factory_snapshot_accepts(self):
        self.validate()
        self.assertEqual((len(self.value["entities"]), len(self.value["tiles"])), (1, 1))

    def test_snapshot_and_nested_factory_numeric_aliases_are_rejected(self):
        mutations = [lambda row: row.update(coordinates=[True, 0]), lambda row: row.update(world_matches=1),
                     lambda row: row.update(minimum=False), lambda row: row["flags"].update(c=0),
                     lambda row: row["flags"].update(q=1), lambda row: row["entities"][0].update(slice=4.0),
                     lambda row: row["entities"][0].update(id=True), lambda row: row["tiles"][0].update(id=-1),
                     lambda row: row["tiles"][0]["position"].update(b=64.0)]
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(self.value)
            mutate(value)
            with self.subTest(mutation=index), self.assertRaises(world.Failure):
                self.validate(value)

    def test_factory_rows_reject_missing_and_unattributed_fields(self):
        mutations = [lambda row: row["entities"][0].update(host_path="C:/unattributed"),
                     lambda row: row["tiles"][0].update(host_path="C:/unattributed"),
                     lambda row: row["tiles"][0].pop("key_class"),
                     lambda row: row["data"].pop("metadata"),
                     lambda row: row.update(entities=[]), lambda row: row.update(tiles=[])]
        for index, mutate in enumerate(mutations):
            value = copy.deepcopy(self.value)
            mutate(value)
            with self.subTest(mutation=index), self.assertRaises(world.Failure):
                self.validate(value)


class SavedOriginalMetadataGuards(unittest.TestCase):
    def setUp(self):
        self.live = original_info_view()
        self.interval = [world.CLOCK - 7, world.CLOCK + 7]

    def saved(self, clock=None):
        tree = metadata_tree()
        data = level.child(tree, "Data")
        level.child(data, "version")["value"] = 19132
        level.child(data, "LastPlayed")["value"] = world.CLOCK if clock is None else clock
        return tree

    def test_sampled_clock_and_forced_version_use_unmodified_pre_save_live_info(self):
        before = copy.deepcopy(self.live)
        for clock in (self.interval[0], world.CLOCK, self.interval[1]):
            world.validate_saved_original_info(self.saved(clock), self.live, self.interval)
        self.assertEqual(self.live, before)
        self.assertEqual(self.live["last_played"], "00000000000011d7")
        self.assertEqual(self.live["version"], 19131)
        self.assertEqual(self.live["size_on_disk"], "0000000000002000")
        # Original full_save serializes Info before cache writes can grow it.
        after_cache_write = self.live | {"size_on_disk": "0000000000003000"}
        with self.assertRaises(world.Failure):
            world.validate_saved_original_info(self.saved(), after_cache_write, self.interval)

    def test_coherent_known_saved_metadata_changes_are_rejected(self):
        for name in ("RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "SizeOnDisk",
                     "LevelName", "rainTime", "raining", "thunderTime", "thundering"):
            tree = self.saved()
            field = level.child(level.child(tree, "Data"), name)
            field["value"] = level.u16("coherently changed") if name == "LevelName" else field["value"] ^ 1
            with self.subTest(name=name), self.assertRaises(world.Failure):
                world.validate_saved_original_info(tree, self.live, self.interval)
        for name, value in (("LastPlayed", self.interval[0] - 1),
                            ("LastPlayed", self.interval[1] + 1), ("version", 19131)):
            tree = self.saved()
            level.child(level.child(tree, "Data"), name)["value"] = value
            with self.subTest(name=name, value=value), self.assertRaises(world.Failure):
                world.validate_saved_original_info(tree, self.live, self.interval)

    def test_missing_wrongtyped_and_noncanonical_known_saved_fields_are_rejected(self):
        names = ("RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time", "LastPlayed", "SizeOnDisk",
                 "LevelName", "version", "rainTime", "raining", "thunderTime", "thundering")
        for name in names:
            for mutation in ("missing", "wrong-type"):
                tree = self.saved()
                data = level.child(tree, "Data")
                field = level.child(data, name)
                if mutation == "missing":
                    data["value"].remove(field)
                elif name == "LevelName":
                    field.update(type=3, value=17)
                else:
                    field.update(type=8, value=level.u16("wrong type"))
                with self.subTest(name=name, mutation=mutation), self.assertRaises(world.Failure):
                    world.validate_saved_original_info(tree, self.live, self.interval)
        for name in ("raining", "thundering"):
            for value in (2, -128):
                tree = self.saved()
                level.child(level.child(tree, "Data"), name)["value"] = value
                with self.subTest(name=name, byte=value), self.assertRaises(world.Failure):
                    world.validate_saved_original_info(tree, self.live, self.interval)


if __name__ == "__main__":
    unittest.main()
