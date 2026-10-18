#!/usr/bin/env python3
"""Public chunk-observer protocol guards. Synthetic records are not parity data."""

import copy
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "scripts/reference/chunk/compare_chunk.py"


def load_helper():
    module = types.ModuleType("_chunk_data_guard_subject")
    module.__file__ = str(SOURCE)
    exec(compile(SOURCE.read_bytes(), str(SOURCE), "exec"), module.__dict__)
    return module


def descriptor(data):
    return dict(length=len(data), values=data.hex())


def local_data(blocks):
    return dict(blocks=descriptor(blocks), metadata=descriptor(bytes(16384)),
                skylight=descriptor(bytes(16384)), block_light=descriptor(bytes(16384)),
                heightmap=descriptor(bytes(256)), minimum_height=0)


def reference(tag, name):
    return dict(ref=tag, **{"class": name})


def synthetic_world():
    flags = dict(c=False, n=False, o=False, p=False, q=False)
    chunk = dict(object=reference("r4", "li"), coordinates=[0, 0], heightmap=descriptor(bytes(256)),
                 minimum_height=0, flags=flags, time="0000000000000000", measured=False,
                 fallback=True, empty=True, array_references=[reference(None, None)] * 8,
                 blocks=dict(length=None, sha256=None), metadata=dict(length=None, sha256=None),
                 skylight=dict(length=None, sha256=None), block_light=dict(length=None, sha256=None))
    world = {name: reference("r0", "fd") for name in
             ("world", "provider", "storage", "cache", "provider_world", "manager", "cache_world", "generator", "loader")}
    world.update(no_sky=False, chunk_static_a=False, fallback="r4", chunks=[chunk], enqueue_depth=0, drain_depth=0,
                 map=dict(object=reference("r5", "java.util.HashMap"), entries=[]),
                 insertion_list=dict(object=reference("r6", "java.util.ArrayList"), entries=[]),
                 pending_set=dict(object=reference("r7", "java.util.HashSet"), entries=[]),
                 queue=dict(object=reference("r8", "java.util.ArrayList"), entries=[]),
                 presence=[dict(coordinates=[x, z], key=index, present=False, contains=False, value=None)
                           for index, (x, z) in enumerate(((0, 0), (-1, 0), (1, 0), (0, -1), (0, 1)))])
    return world


def synthetic_original(helper):
    case = helper.cases(ROOT, smoke=True)[0][0]
    before = dict(data=local_data(case["initial"]), coordinates=[0, 0],
                  flags=dict(c=False, n=False, o=False, p=False, q=False),
                  time="0000000000000000",
                  map={"class": "java.util.HashMap", "size": 0},
                  entity_slices=[{"class": "java.util.ArrayList", "size": 0}] * 8,
                  identities=dict(roles_retained=[True] * 19, roles_pairwise_distinct=True,
                                  input_retained=True, world_retained=True))
    result = dict(schema=2, side="client", seed="0000000000000000", coordinates=[0, 0],
                  bindings=dict(world="fd", chunk="lm", finish="c", nibble="wi", provider="rh", cache="ok"),
                  input_sha256=hashlib.sha256(case["initial"]).hexdigest(),
                  repeat_input_sha256=hashlib.sha256(case["repeat"]).hexdigest(),
                  setup_world=synthetic_world(),
                  controls=dict(initial_blocks_unchanged=True, repeat_blocks_unchanged=True, opacity_unchanged=True,
                                chunk_static_a_unchanged=True))
    for stage in helper.STAGES:
        local = copy.deepcopy(before)
        if stage != "before":
            local["flags"]["o"] = True
        if stage in ("repeat_before", "repeat_after"):
            local["data"]["blocks"] = descriptor(case["repeat"])
        result[stage] = dict(local=local, world=synthetic_world(),
                             opacity=descriptor(bytes(1024)), opacity_identity_retained=True)
    return result, case


class ChunkReferenceGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.helper = load_helper()

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="chunk-data-reference-guards-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name).resolve()

    def test_complete_array_bytes_and_integer_types(self):
        data = local_data(bytes(32768))
        self.helper.validate_data(data, "synthetic")
        for name in self.helper.ARRAY_LENGTHS:
            changed = copy.deepcopy(data)
            changed[name]["values"] = changed[name]["values"][:-2]
            with self.subTest(field=name), self.assertRaises(self.helper.Failure):
                self.helper.validate_data(changed, "truncated")
        for value in (True, -1, 128):
            changed = copy.deepcopy(data)
            changed["minimum_height"] = value
            with self.subTest(value=value), self.assertRaises(self.helper.Failure):
                self.helper.validate_data(changed, "bad minimum")

    def test_shared_helper_runs_owned_child_with_live_inventory_and_strict_final_hashes(self):
        helper, _ = self.helper.shared_helper(ROOT)
        world = self.directory / "shared-owned-world"
        world.mkdir()
        (world / "before.bin").write_bytes(b"owned before")
        stdout, stderr = self.directory / "child.stdout", self.directory / "child.stderr"
        command = [sys.executable, "-c",
                   "from pathlib import Path; import sys; "
                   "Path(sys.argv[1]).joinpath('after.bin').write_bytes(b'owned after'); "
                   "sys.stdout.buffer.write(b'owned shared helper\\n')", str(world)]
        result = helper.run_live(command, stdout, stderr, self.directory, seconds=5, world_root=world)
        self.assertEqual((result.returncode, result.timed_out, result.stop_reason), (0, False, None))
        self.assertGreaterEqual(result.inventory_polls, 1)
        self.assertEqual(stdout.read_bytes(), b"owned shared helper\n")
        self.assertEqual(stderr.read_bytes(), b"")
        final = helper.world_inventory(world, hashes=True)
        self.assertTrue(final["complete"], final)
        self.assertIsNone(final["error"])
        self.assertEqual(final["entries"], [
            {"path": name, "kind": "file", "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
            for name, raw in (("after.bin", b"owned after"), ("before.bin", b"owned before"))])
        self.assertIn("cannot hash final state", helper.world_inventory(world, hashes=True, live_poll=True)["error"])

    def test_shared_helper_keeps_entry_and_byte_caps_in_both_inventory_modes(self):
        helper, _ = self.helper.shared_helper(ROOT)
        world = self.directory / "bounded-owned-world"
        world.mkdir()
        for index in range(256):
            (world / str(index)).write_bytes(b"")
        for live_poll in (False, True):
            self.assertTrue(helper.world_inventory(world, live_poll=live_poll)["complete"])
        extra = world / "overflow"
        extra.write_bytes(b"")
        for live_poll in (False, True):
            result = helper.world_inventory(world, live_poll=live_poll)
            self.assertFalse(result["complete"])
            self.assertIn("entry limit", result["error"])
        for path in world.iterdir():
            path.unlink()
        large = world / "bounded.bin"
        with large.open("wb") as output:
            output.truncate(32 * 1024 * 1024)
        for live_poll in (False, True):
            self.assertTrue(helper.world_inventory(world, live_poll=live_poll)["complete"])
        with large.open("ab") as output:
            output.write(b"x")
        for live_poll in (False, True):
            result = helper.world_inventory(world, live_poll=live_poll)
            self.assertFalse(result["complete"])
            self.assertIn("byte limit", result["error"])

    def test_duplicate_json_nonfinite_and_partial_records_rejected(self):
        for text in ('{"x":0,"x":1}', '{"x":NaN}', '{"x":Infinity}', '{"x":'):
            with self.subTest(text=text), self.assertRaises(self.helper.Failure):
                self.helper.strict_json(text)
        path = self.directory / "output"
        path.write_bytes(b'{"value":"complete"}\n')
        self.assertEqual(self.helper.read_output(path), {"value": "complete"})
        for payload in (b"{}", b"{}\n{}\n", b"{\n}\n"):
            path.write_bytes(payload)
            with self.subTest(payload=payload), self.assertRaises(self.helper.Failure):
                self.helper.read_output(path)

    def test_completed_measurement_never_becomes_setup_unavailable(self):
        process = types.SimpleNamespace(returncode=77, stop_reason=None)
        allowed = "SETUP_BEGIN\nUNAVAILABLE: genuine original world setup\n"
        self.assertEqual(self.helper.original_status(process, b"", allowed), "unavailable")
        for prefix, stdout, stop_reason in (
                ("SETUP_BEGIN\nSETUP_DONE\nMEASUREMENT_BEGIN\n", b"", None),
                ("SETUP_BEGIN\n", b"{}\n", None),
                ("SETUP_BEGIN\n", b"", "child timeout"),
                ("", b"", None)):
            process.stop_reason = stop_reason
            stderr = prefix + "UNAVAILABLE: genuine original world setup\n"
            with self.subTest(prefix=prefix, output=stdout, stop=stop_reason), self.assertRaises(self.helper.Failure):
                self.helper.original_status(process, stdout, stderr)
        process.returncode = 0
        process.stop_reason = None
        markers = "SETUP_BEGIN\nSETUP_DONE\nMEASUREMENT_BEGIN\nMEASUREMENT_DONE\n"
        self.assertEqual(self.helper.original_status(process, b"{}\n", markers), "completed")

    def test_cache_table_queue_bounds_and_order_remain_complete(self):
        world = synthetic_world()
        world["queue"]["entries"] = [
            dict(ordinal=0, object=reference("r9", "st"),
                 kind=dict(identity="a", name="Sky", ordinal=0, default=15),
                 bounds=[-1, -100, 3, 4, 400, 6])]
        self.helper.validate_world(world, 4096, "synthetic")
        changed = copy.deepcopy(world)
        changed["queue"]["entries"][0]["ordinal"] = 1
        with self.assertRaises(self.helper.Failure):
            self.helper.validate_world(changed, 4096, "order lost")
        changed = copy.deepcopy(world)
        changed["chunks"] = []
        with self.assertRaises(self.helper.Failure):
            self.helper.validate_world(changed, 4096, "fallback omitted")
        changed = copy.deepcopy(world)
        changed["presence"][0]["contains"] = True
        with self.assertRaises(self.helper.Failure):
            self.helper.validate_world(changed, 4096, "presence changed")
        changed = copy.deepcopy(world)
        changed["queue"]["entries"][0]["bounds"].pop()
        with self.assertRaises(self.helper.Failure):
            self.helper.validate_world(changed, 4096, "bound omitted")
        changed = copy.deepcopy(world)
        changed["queue"]["entries"][0]["kind"]["ordinal"] = False
        with self.assertRaises(self.helper.Failure):
            self.helper.validate_world(changed, 4096, "boolean enum ordinal")

    def test_original_controls_detect_cache_and_retained_array_changes(self):
        value, case = synthetic_original(self.helper)
        # Deliberately synthetic opacity only for schema/control tests.
        # Runtime always enforces the complete observed original table hash.
        synthetic_sha = hashlib.sha256(bytes(1024)).hexdigest()
        with mock.patch.object(self.helper, "OPACITY_SHA", synthetic_sha):
            self.helper.validate_original(value, case, "client")
            for field in ("metadata", "block_light"):
                changed = copy.deepcopy(value)
                changed["after"]["local"]["data"][field]["values"] = "01" + changed["after"]["local"]["data"][field]["values"][2:]
                with self.subTest(field=field), self.assertRaises(self.helper.Failure):
                    self.helper.validate_original(changed, case, "client")
            changed = copy.deepcopy(value)
            changed["after"]["world"]["chunks"][0]["minimum_height"] = 17
            with self.assertRaises(self.helper.Failure):
                self.helper.validate_original(changed, case, "client")
            changed = copy.deepcopy(value)
            changed["repeat_after"]["local"]["identities"]["roles_retained"][-1] = False
            with self.assertRaises(self.helper.Failure):
                self.helper.validate_original(changed, case, "client")

    def test_queue_growth_uses_actual_setup_and_raw_edits_preserve_regions(self):
        value, case = synthetic_original(self.helper)
        regions = [dict(ordinal=index, object=reference(f"r{100 + index}", "st"),
                        kind=dict(identity="a", name="Sky", ordinal=0, default=15),
                        bounds=[-1, 0, 0, 0, 127, 0]) for index in range(1024)]
        for stage in ("after", "repeat_before", "repeat_after"):
            value[stage]["world"]["queue"]["entries"] = copy.deepcopy(regions)
        synthetic_sha = hashlib.sha256(bytes(1024)).hexdigest()
        with mock.patch.object(self.helper, "OPACITY_SHA", synthetic_sha):
            self.helper.validate_original(value, case, "client")
            changed = copy.deepcopy(value)
            extra = copy.deepcopy(regions[-1])
            extra["ordinal"] = 1024
            extra["object"]["ref"] = "r1124"
            changed["after"]["world"]["queue"]["entries"].append(extra)
            with self.assertRaises(self.helper.Failure):
                self.helper.validate_original(changed, case, "client")
            changed, _ = synthetic_original(self.helper)
            changed["repeat_after"]["world"]["queue"]["entries"] = copy.deepcopy(regions) + [extra]
            with self.assertRaises(self.helper.Failure):
                self.helper.validate_original(changed, case, "client")
            changed = copy.deepcopy(value)
            changed["repeat_before"]["world"]["queue"]["entries"][0]["bounds"][1] = 1
            with self.assertRaises(self.helper.Failure):
                self.helper.validate_original(changed, case, "client")

    def test_setup_latch_differences_preserve_diagnostics_and_exact_local_comparison(self):
        left, case = synthetic_original(self.helper)
        right = copy.deepcopy(left)
        right["setup_world"]["chunk_static_a"] = True
        for stage in self.helper.STAGES:
            right[stage]["world"]["chunk_static_a"] = True
        synthetic_sha = hashlib.sha256(bytes(1024)).hexdigest()
        with mock.patch.object(self.helper, "OPACITY_SHA", synthetic_sha):
            a = self.helper.validate_original(left, case, "client")
            b = self.helper.validate_original(right, case, "client")
            self.assertIsNone(self.helper.first_difference(a, b))
            self.assertIs(left["setup_world"]["chunk_static_a"], False)
            self.assertIs(right["setup_world"]["chunk_static_a"], True)
            for stage in self.helper.STAGES:
                self.assertIs(left[stage]["world"]["chunk_static_a"], False)
                self.assertIs(right[stage]["world"]["chunk_static_a"], True)
                self.assertNotIn("static_a", left[stage]["local"])
                self.assertIsNone(self.helper.first_difference(left[stage]["local"], right[stage]["local"]))
            changed = copy.deepcopy(right)
            changed["after"]["world"]["chunk_static_a"] = False
            with self.assertRaises(self.helper.Failure):
                self.helper.validate_original(changed, case, "client")
        changed = copy.deepcopy(left["after"]["local"])
        changed["data"]["skylight"]["values"] = "00" * 37 + "01" + "00" * (16384 - 38)
        difference = self.helper.first_difference(left["after"]["local"], changed, "$.after.local")
        self.assertEqual(difference, dict(path="$.after.local.data.skylight.values", reason="byte",
                                          byte_index=37, left="00", right="01"))
        changed = copy.deepcopy(left["after"]["local"])
        changed["flags"]["o"] = False
        difference = self.helper.first_difference(left["after"]["local"], changed, "$.after.local")
        self.assertEqual(difference, dict(path="$.after.local.flags.o", reason="value", left=True, right=False))

    def test_hardlinked_manifest_and_ancestor_work_are_rejected_without_writes(self):
        source = self.directory / "source"
        source.write_bytes(b"preserve original bytes")
        work = self.directory / "output"
        work.mkdir()
        (work / "manifest.json").hardlink_to(source)
        with self.assertRaises(self.helper.Failure):
            self.helper.guard_work(work, [source])
        self.assertEqual(source.read_bytes(), b"preserve original bytes")
        with self.assertRaises(self.helper.Failure):
            self.helper.guard_work(self.directory, [source])

    def test_frozen_natural_manifest_is_protected_before_any_output_creation(self):
        natural = self.directory / "frozen-natural"
        natural.mkdir()
        manifest = natural / "manifest.json"
        manifest.write_bytes(b"preserve frozen input manifest")
        argv = ["--capture-only", "--client", str(self.directory / "client.jar"),
                "--server", str(self.directory / "server.jar"), "--work-dir", str(natural),
                "--natural-root", str(natural), "--tested-commit", "0" * 40]
        with self.assertRaises(self.helper.Failure):
            self.helper.main(argv)
        self.assertEqual(manifest.read_bytes(), b"preserve frozen input manifest")
        self.assertEqual({entry.name for entry in natural.iterdir()}, {"manifest.json"})

    def test_source_loader_uses_attributed_source_bytes(self):
        source = self.directory / "helper.py"
        source.write_text("VALUE = 17\n", encoding="utf-8")
        subject, attributed = self.helper.source_module(source, "_chunk_loader_test")
        self.assertEqual(subject.VALUE, 17)
        self.assertEqual(attributed["sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
        source.write_text("VALUE = 29\n", encoding="utf-8")
        subject, new_attribution = self.helper.source_module(source, "_chunk_loader_test")
        self.assertEqual(subject.VALUE, 29)
        self.assertNotEqual(attributed["sha256"], new_attribution["sha256"])

    def test_renamed_driver_cannot_attribute_the_canonical_source(self):
        renamed = self.directory / "project/scripts/reference/chunk/renamed.py"
        renamed.parent.mkdir(parents=True)
        renamed.write_bytes(SOURCE.read_bytes())
        renamed.with_name("compare_chunk.py").write_text("UNEXECUTED = True\n", encoding="utf-8")
        subject = types.ModuleType("_renamed_chunk_driver")
        subject.__file__ = str(renamed)
        exec(compile(renamed.read_bytes(), str(renamed), "exec"), subject.__dict__)
        output = self.directory / "output"
        argv = ["--capture-only", "--smoke", "--client", str(self.directory / "client.jar"),
                "--server", str(self.directory / "server.jar"), "--work-dir", str(output),
                "--tested-commit", "0" * 40]
        with self.assertRaisesRegex(subject.Failure, "executing driver must be"):
            subject.main(argv)
        self.assertFalse(output.exists())

    def test_injected_compiler_options_are_rejected_before_output_creation(self):
        output = self.directory / "output"
        argv = ["--capture-only", "--smoke", "--client", str(self.directory / "client.jar"),
                "--server", str(self.directory / "server.jar"), "--work-dir", str(output),
                "--tested-commit", "0" * 40]
        environment = dict.fromkeys(("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS",
                                     "JDK_JAVAC_OPTIONS", "CLASSPATH"), "")
        for name, value in (("JDK_JAVAC_OPTIONS", "-g:none"), ("CLASSPATH", "unattributed-classes")):
            with self.subTest(variable=name), mock.patch.dict(os.environ, environment | {name: value}):
                with self.assertRaisesRegex(self.helper.Failure, "unattributed injected Java options"):
                    self.helper.main(argv)
                self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
