"""Java-free callback driver execution and setup-only unavailability guards."""

from __future__ import annotations

import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import types
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / "scripts/reference/cache/compare_callbacks.py"
SPEC = importlib.util.spec_from_file_location("callbacks_reference_subject", DRIVER)
CALLBACKS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CALLBACKS)
COMMON, OLD, FINISH, NORMAL, ACQUIRE, EXECUTED = CALLBACKS.helpers()
INTEROP_SPEC = importlib.util.spec_from_file_location("callbacks_public_fixture_reader", ROOT / "tests/tools/cache/test_callbacks_interop.py")
INTEROP = importlib.util.module_from_spec(INTEROP_SPEC)
INTEROP_SPEC.loader.exec_module(INTEROP)
PROTOCOL, PROTOCOL_SOURCE = CALLBACKS.load_protocol()
REJECTIONS = (CALLBACKS.Failure, ACQUIRE.Failure, FINISH.Failure, NORMAL.Failure)


class CallbackExecutionGuards(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="mc-callback-execution-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()

    def prerequisites(self):
        paths = {}
        for name in ("client", "server", "executable", "java", "javac", "javap", "jdk_modules", "jdk_ct_sym"):
            paths[name] = self.root / name
            paths[name].write_bytes(b"synthetic input")
        paths["jdk_release"] = self.root / "release"
        paths["jdk_release"].write_text('JAVA_VERSION="17.0.17"\n', encoding="utf-8")
        tools = {name: paths[name] for name in ("java", "javac", "javap", "jdk_release", "jdk_modules", "jdk_ct_sym")}
        arguments = ["--client", str(paths["client"]), "--server", str(paths["server"]),
                     "--executable", str(paths["executable"]), "--work-dir", str(self.root / "reports"),
                     "--tested-commit", "a" * 40]
        return paths, tools, arguments

    @staticmethod
    def process(command=(), code=77):
        return COMMON.ProcessResult(list(command), code, 0.0, False, None, 0, 0, 0, 0, 0)

    @staticmethod
    def child_command(arguments, tools, classes):
        return [str(tools["java"]), "-Xint", "-Xmx512m",
                "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                "-cp", str(classes), "CacheCallbacksObservation", *map(str, arguments)]

    def unavailable_proof(self, arguments, tools, classes):
        return {"schema": 1, "status": "unavailable", "stage": "original_setup", "child_exit": 77,
                "side": arguments[1], "mode": arguments[2], "scenario": arguments[2],
                "jar_sha256": FINISH.JAR_HASHES[arguments[1]], "begin_markers": 0, "end_markers": 0,
                "completed_operations": 0, "identity_markers": 0, "snapshot_started": False,
                "snapshot_exists": False, "child_command": self.child_command(arguments, tools, classes),
                "debuggee_mode": "interpreted", "elapsed_ms": 1}

    def fake_run(self, stage, code=77, *, case=2, proof=False, mutation=None, artifact=None,
                 module_text="jdk.jdi@17\njdk.jdwp.agent@17\n", executed=None):
        """Drive classification/preservation with files and processes owned by this test."""
        paths, tools, arguments = self.prerequisites()
        arguments += ["--case", str(case)]
        previous = set((self.root / "reports").glob("capture-*/manifest.json"))
        actual_record = COMMON.file_record
        calls = []
        preservation = {"changed_source": False}
        protocol_path = ROOT / "scripts/reference/cache/callback_protocol.py"
        source = protocol_path.read_bytes()
        protocol_source = {"path": str(protocol_path), "bytes": len(source), "sha256": hashlib.sha256(source).hexdigest()}
        protocol = types.SimpleNamespace(validate_trace=lambda *unused: ({}, {"operations": []}))

        def file_record(path, byte_limit=None):
            record = actual_record(path, byte_limit=byte_limit)
            for side in ("client", "server"):
                if Path(path) == paths[side]:
                    record["sha256"] = FINISH.JAR_HASHES[side]
            if preservation["changed_source"] and Path(path) == ROOT / "tests/reference/cache/CacheCallbacksObservation.java":
                record["sha256"] = "0" * 64
            return record

        def run(command, stdout, stderr, cwd, **kwargs):
            if "--list-modules" in command:
                current = "jdk-modules"
            elif Path(command[0]) == paths["javac"]:
                current = "compile"
            elif "CacheCallbacksController" in command:
                current = "controller"
            else:
                current = "prepare" if "prepare" in command else "control"
            calls.append(current)
            current_code = code if current == stage else 0
            Path(stdout).write_bytes(module_text.encode("utf-8") if current == "jdk-modules" else b"")
            Path(stderr).write_bytes(b"")
            directory = Path(cwd)
            if current == "compile":
                classes = Path(command[command.index("-d") + 1])
                for name in ("ChunkObservation", "CacheCallbacksObservation", "CacheCallbacksController"):
                    (classes / (name + ".class")).write_bytes(bytes.fromhex("cafebabe0000003d") + bytes(8))
            elif current in ("prepare", "controller", "control"):
                world = Path(kwargs["world_root"])
                world.mkdir(parents=True, exist_ok=True)
                if current == "prepare":
                    snapshot = Path(command[-1])
                    if current_code == 0:
                        saved = world / "world"
                        (saved / "region").mkdir(parents=True)
                        for relative, data in (("level.dat", b"synthetic metadata"), ("region/r.0.0.mcr", b"synthetic region"),
                                               ("session.lock", b"12345678")):
                            (saved / relative).write_bytes(data)
                        snapshot.write_text("{}", encoding="utf-8")
                elif current == "controller":
                    trace, snapshot = Path(command[-1]), Path(command[-2])
                    classes = Path(command[command.index("-cp") + 1])
                    original_arguments = command[command.index("CacheCallbacksController") + 1:-1]
                    if current_code == 0:
                        snapshot.write_text("{}", encoding="utf-8")
                        measured = {"child_command": self.child_command(original_arguments, tools, classes), "operations": []}
                        for key, suffix in (("child_stdout", ".child-stdout.txt"), ("child_stderr", ".child-stderr.txt")):
                            log = Path(str(trace) + suffix)
                            log.write_bytes(b"")
                            measured[key] = {"file": log.name, "bytes": 0, "sha256": hashlib.sha256(b"").hexdigest()}
                        trace.write_text(json.dumps(measured), encoding="utf-8")
                    elif proof:
                        Path(str(trace) + ".unavailable.json").write_text(json.dumps(
                            self.unavailable_proof(original_arguments, tools, classes)), encoding="utf-8")
                        Path(str(trace) + ".child-stderr.txt").write_text("UNAVAILABLE: genuine callback setup\n", encoding="utf-8")
                    if artifact == "snapshot":
                        snapshot.write_text("{}", encoding="utf-8")
                    elif artifact == "trace":
                        trace.write_text("{}", encoding="utf-8")
                    if mutation == "input":
                        paths["executable"].write_bytes(b"SYNTHETIC INPUT")
                    elif mutation == "source":
                        # Simulate a changed attributed source without writing
                        # another agent's tracked file in the shared workspace.
                        preservation["changed_source"] = True
                    elif mutation == "class":
                        (classes / "CacheCallbacksController.class").write_bytes(b"changed class")
                    elif mutation == "baseline":
                        baseline = directory.parent / "prepare-client/baseline/world/level.dat"
                        baseline.write_bytes(b"changed preserved metadata")
                else:
                    snapshot = Path(command[-1])
                if current in ("prepare", "control") and current_code == 77:
                    Path(stderr).write_text("UNAVAILABLE: genuine callback setup\n", encoding="utf-8")
                    if artifact == "snapshot":
                        snapshot.write_text("{}", encoding="utf-8")
            return self.process(command, current_code)

        with patch.dict(os.environ, dict.fromkeys(FINISH.JAVA_ENVIRONMENT, "")), \
                patch.object(CALLBACKS, "helpers", return_value=(COMMON, OLD, FINISH, NORMAL, ACQUIRE,
                             copy.deepcopy(EXECUTED if executed is None else executed))), \
                patch.object(CALLBACKS, "load_protocol", return_value=(protocol, protocol_source)), \
                patch.object(CALLBACKS, "validate_observation", return_value=({"operations": []}, {})), \
                patch.object(CALLBACKS, "validate_saved_payload"), \
                patch.object(COMMON, "runtime_tools", return_value=tools), patch.object(COMMON, "tool_versions"), \
                patch.object(COMMON, "file_record", side_effect=file_record), patch.object(COMMON, "run_live", side_effect=run), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            status = CALLBACKS.main(arguments)
        manifests = set((self.root / "reports").glob("capture-*/manifest.json")) - previous
        self.assertLessEqual(len(manifests), 1)
        report = COMMON.strict_json(next(iter(manifests)).read_bytes()) if manifests else None
        return status, report, calls

    def test_invalid_case_source_label_and_environment_fail_before_runtime(self):
        _, _, arguments = self.prerequisites()
        for name in ("duplicate", "outside", "source", "environment"):
            selected = arguments.copy()
            environment = dict.fromkeys(FINISH.JAVA_ENVIRONMENT, "")
            if name == "duplicate": selected += ["--case", "0", "--case", "0"]
            elif name == "outside": selected += ["--case", "4"]
            elif name == "source": selected[-1] = "A" * 40
            else: environment[FINISH.JAVA_ENVIRONMENT[0]] = "injected"
            with self.subTest(preflight=name), patch.dict(os.environ, environment), \
                    patch.object(CALLBACKS, "helpers", return_value=(COMMON, OLD, FINISH, NORMAL, ACQUIRE, copy.deepcopy(EXECUTED))), \
                    patch.object(COMMON, "runtime_tools", side_effect=AssertionError("runtime must not be queried")) as runtime, \
                    patch.object(COMMON, "run_live", side_effect=AssertionError("external process forbidden")), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(CALLBACKS.main(selected), 1)
                runtime.assert_not_called()
        self.assertFalse((self.root / "reports").exists())

    def test_renamed_source_and_changed_loaded_helper_attribution_fail(self):
        with patch.object(CALLBACKS, "__file__", str(DRIVER.with_name("renamed_callbacks.py"))), self.assertRaises(CALLBACKS.Failure):
            CALLBACKS.helpers()
        executed = copy.deepcopy(EXECUTED)
        executed[0]["sha256"] = "0" * 64
        self.assertEqual(self.fake_run("compile", executed=executed), (1, None, []))

    def test_missing_runtime_and_old_jdk_are_unavailable_without_processes_or_capture(self):
        _, tools, arguments = self.prerequisites()
        for name in ("missing", "old"):
            tools["jdk_release"].write_text('JAVA_VERSION="16.0.1"\n', encoding="utf-8")
            result = {"side_effect": COMMON.Unavailable("synthetic missing JDK")} if name == "missing" else {"return_value": tools}
            with self.subTest(prerequisite=name), patch.dict(os.environ, dict.fromkeys(FINISH.JAVA_ENVIRONMENT, "")), \
                    patch.object(CALLBACKS, "helpers", return_value=(COMMON, OLD, FINISH, NORMAL, ACQUIRE, copy.deepcopy(EXECUTED))), \
                    patch.object(COMMON, "runtime_tools", **result), \
                    patch.object(COMMON, "run_live", side_effect=AssertionError("external process forbidden")), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(CALLBACKS.main(arguments), 77)
        self.assertFalse((self.root / "reports").exists())

    def test_missing_module_and_failed_module_process_have_distinct_classification(self):
        status, report, calls = self.fake_run("compile", module_text="java.base@17\n")
        self.assertEqual((status, report["status"], calls), (77, "unavailable", ["jdk-modules"]))
        self.assertNotIn("classes_before", report)
        status, report, calls = self.fake_run("jdk-modules")
        self.assertEqual((status, report["status"], calls), (1, "fail", ["jdk-modules"]))
        self.assertTrue(report["errors"])

    def test_compile77_unproved_controller77_and_post_snapshot77_are_failures(self):
        for stage, artifact in (("compile", None), ("controller", None), ("controller", "snapshot"), ("controller", "trace")):
            with self.subTest(stage=stage, artifact=artifact):
                status, report, calls = self.fake_run(stage, proof=artifact is not None, artifact=artifact)
                self.assertEqual((status, report["status"]), (1, "fail"))
                self.assertTrue(report["errors"])
                self.assertEqual(calls, ["jdk-modules", "compile"] if stage == "compile" else ["jdk-modules", "compile", "controller"])
        status, report, calls = self.fake_run("controller", proof=True)
        self.assertEqual((status, report["status"], calls), (77, "unavailable", ["jdk-modules", "compile", "controller"]))

    def test_setup_sidecar_requires_zero_identity_snapshot_and_measurement_evidence(self):
        paths, tools, _ = self.prerequisites()
        trace, snapshot, classes = self.root / "trace.json", self.root / "snapshot.json", self.root / "classes"
        arguments = [str(paths["client"]), "client", "population", CALLBACKS.SEED, str(self.root / "world"), str(snapshot)]
        proof = self.unavailable_proof(arguments, tools, classes)
        sidecar = Path(str(trace) + ".unavailable.json")
        child_error = Path(str(trace) + ".child-stderr.txt")
        child_error.write_text("UNAVAILABLE: genuine callback setup\n", encoding="utf-8")
        process = self.process()

        def validate(value):
            sidecar.write_text(json.dumps(value), encoding="utf-8")
            return CALLBACKS.setup_unavailable(COMMON, process, trace, snapshot, "client", "population",
                                               arguments, tools, classes, FINISH)

        self.assertEqual(validate(proof), proof)
        for name in ("begin_markers", "end_markers", "completed_operations", "identity_markers", "snapshot_started", "snapshot_exists"):
            changed = dict(proof)
            changed[name] = True if name.startswith("snapshot_") else 1
            with self.subTest(evidence=name), self.assertRaises(CALLBACKS.Failure):
                validate(changed)
        for name, value in (("identity_markers", False), ("snapshot_started", 0), ("elapsed_ms", 0),
                             ("elapsed_ms", 90001), ("side", "server"), ("debuggee_mode", "compiled")):
            changed = dict(proof, **{name: value})
            with self.subTest(field=name, value=value), self.assertRaises(CALLBACKS.Failure):
                validate(changed)
        changed = dict(proof)
        del changed["identity_markers"]
        with self.assertRaises(CALLBACKS.Failure):
            validate(changed)
        for artifact in (snapshot, trace):
            artifact.write_text("{}", encoding="utf-8")
            with self.subTest(artifact=artifact.name), self.assertRaises(CALLBACKS.Failure):
                validate(proof)
            artifact.unlink()
        original_symlink = Path.is_symlink
        with patch.object(Path, "is_symlink", lambda path: path == snapshot or original_symlink(path)), self.assertRaises(CALLBACKS.Failure):
            validate(proof)
        for field, value in (("timed_out", True), ("stop_reason", "output bound"), ("returncode", 1)):
            previous = getattr(process, field)
            setattr(process, field, value)
            with self.subTest(process=field), self.assertRaises(CALLBACKS.Failure):
                validate(proof)
            setattr(process, field, previous)
        child_error.write_text("UNAVAILABLE: genuine callback setup\nFAILURE: callback observation\n", encoding="utf-8")
        with self.assertRaises(CALLBACKS.Failure):
            validate(proof)

    def test_direct_constructor77_requires_owned_marker_and_absent_completed_output(self):
        snapshot = self.root / "snapshot.json"
        error = self.root / "prepare-err.txt"
        process = self.process()
        for text in ("", "arbitrary exit77\n", "UNAVAILABLE: genuine callback setup\nFAILURE: callback observation\n"):
            error.write_text(text, encoding="utf-8")
            with self.subTest(stderr=text), self.assertRaises(CALLBACKS.Failure):
                CALLBACKS.direct_unavailable(process, snapshot, self.root, "prepare")
        error.write_text("UNAVAILABLE: genuine callback setup\n", encoding="utf-8")
        CALLBACKS.direct_unavailable(process, snapshot, self.root, "prepare")
        snapshot.write_text("{}", encoding="utf-8")
        with self.assertRaises(CALLBACKS.Failure):
            CALLBACKS.direct_unavailable(process, snapshot, self.root, "prepare")
        snapshot.unlink()
        for stage in ("prepare", "control"):
            with self.subTest(stage=stage):
                status, report, _ = self.fake_run(stage, case=0 if stage == "prepare" else 2)
                self.assertEqual((status, report["status"]), (77, "unavailable"))
                status, report, _ = self.fake_run(stage, case=0 if stage == "prepare" else 2, artifact="snapshot")
                self.assertEqual((status, report["status"]), (1, "fail"))

    def test_exact_debuggee_argv_binds_runtime_mode_helper_archive_and_owned_paths(self):
        paths, tools, _ = self.prerequisites()
        classes = self.root / "classes"
        arguments = [str(paths["client"]), "client", "population", CALLBACKS.SEED,
                     str(self.root / "owned-world"), str(self.root / "snapshot.json")]
        command = self.child_command(arguments, tools, classes)
        CALLBACKS.validate_command(command, arguments, tools, classes)
        for index, value in ((0, "other-java"), (1, "-Xcomp"), (2, "-Xmx4g"),
                             (3, "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=*:0"),
                             (5, "foreign-classes"), (6, "OtherObservation"), (7, "other.jar"),
                             (8, "server"), (9, "load"), (10, "0000000000000018"), (11, "foreign-world"), (12, "foreign-snapshot")):
            changed = command.copy()
            changed[index] = value
            with self.subTest(argument=index), self.assertRaises(CALLBACKS.Failure):
                CALLBACKS.validate_command(changed, arguments, tools, classes)

    def test_child_streams_require_bounded_relative_names_and_actual_fingerprints(self):
        trace = {}
        for name in ("child_stdout", "child_stderr"):
            path = self.root / ("trace.json." + name.replace("_", "-") + ".txt")
            path.write_bytes((name + ": original\n").encode("ascii"))
            trace[name] = {"file": path.name, "bytes": path.stat().st_size,
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        CALLBACKS.check_child_streams(COMMON, trace, self.root)
        for name in ("name", "parent", "size", "digest", "extra"):
            changed = copy.deepcopy(trace)
            record = changed["child_stdout"]
            if name == "name": record["file"] = str(self.root / "foreign.txt")
            elif name == "parent": record["file"] = "../foreign.txt"
            elif name == "size": record["bytes"] += 1
            elif name == "digest": record["sha256"] = "0" * 64
            else: record["unrecognized"] = True
            with self.subTest(stream=name), self.assertRaises(CALLBACKS.Failure):
                CALLBACKS.check_child_streams(COMMON, changed, self.root)
        for alias in ("control.json", "sibling-output.txt"):
            changed = copy.deepcopy(trace)
            original = self.root / trace["child_stdout"]["file"]
            (self.root / alias).write_bytes(original.read_bytes())
            changed["child_stdout"]["file"] = alias
            with self.subTest(matching_hash_alias=alias), self.assertRaises(CALLBACKS.Failure):
                CALLBACKS.check_child_streams(COMMON, changed, self.root)
        swapped = {"child_stdout": copy.deepcopy(trace["child_stderr"]),
                   "child_stderr": copy.deepcopy(trace["child_stdout"])}
        with self.subTest(swapped_matching_records=True), self.assertRaises(CALLBACKS.Failure):
            CALLBACKS.check_child_streams(COMMON, swapped, self.root)

    def test_input_class_and_preserved_baseline_changes_override_proved_unavailability(self):
        for mutation, message in (("input", "attributed input changed"), ("source", "attributed input changed"),
                                  ("class", "compiled helper"),
                                  ("baseline", "preserved creator baseline changed")):
            with self.subTest(preservation=mutation):
                status, report, _ = self.fake_run("controller", proof=True, case=0 if mutation == "baseline" else 2, mutation=mutation)
                self.assertEqual((status, report["status"]), (1, "fail"))
                self.assertTrue(any(message in error for error in report["errors"]), report["errors"])

    def test_reload_preserves_all_baseline_files_except_existing_eight_byte_session_lock(self):
        baseline = self.root / "baseline"
        world = baseline / "world"
        (world / "region").mkdir(parents=True)
        for name, data in (("level.dat", b"metadata"), ("region/r.0.0.mcr", b"region"), ("session.lock", b"12345678")):
            (world / name).write_bytes(data)
        before = CALLBACKS.fingerprint_inventory(COMMON, baseline)
        copied = self.root / "copied"
        self.assertEqual(CALLBACKS.inventory_contents(CALLBACKS.copy_world(COMMON, baseline, copied)),
                         CALLBACKS.inventory_contents(before))
        (copied / "world/session.lock").write_bytes(b"87654321")
        after = CALLBACKS.fingerprint_inventory(COMMON, copied)
        CALLBACKS.preserved_reload_files(before, after)
        for name in ("changed_payload", "extra_file", "removed_file", "short_session"):
            changed = copy.deepcopy(after)
            if name == "changed_payload":
                next(row for row in changed["entries"] if row["path"].endswith(".mcr"))["sha256"] = "0" * 64
            elif name == "extra_file": changed["entries"].append({"path": "world/extra", "kind": "file", "bytes": 0, "sha256": "0" * 64})
            elif name == "removed_file": changed["entries"] = [row for row in changed["entries"] if not row["path"].endswith(".mcr")]
            else: next(row for row in changed["entries"] if row["path"].endswith("session.lock"))["bytes"] = 7
            with self.subTest(preservation=name), self.assertRaises(CALLBACKS.Failure):
                CALLBACKS.preserved_reload_files(before, changed)


def selected_event(trace, kind, offset=None, operation=0):
    return next(row for row in trace["operations"][operation]["events"]
                if row["kind"] == kind and (offset is None or row["code_index"] == offset))


def renumber_events(trace, operation):
    for ordinal, event in enumerate(trace["operations"][operation]["events"]):
        event["ordinal"] = ordinal


def visit_dictionaries(value):
    if type(value) is dict:
        yield value
        for child in value.values():
            yield from visit_dictionaries(child)
    elif type(value) is list:
        for child in value:
            yield from visit_dictionaries(child)


class CallbackProtocolGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture, cls.cases = INTEROP.load_fixture()

    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="mc-callback-protocol-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()

    def original(self, index):
        row = self.cases[index]
        return INTEROP.rebind_public(row["snapshot"], row["trace"], row["record"]["side"], row["record"]["scenario"], self.root)

    def preparation(self, side):
        document = INTEROP.strict_json(INTEROP.unpack(self.fixture["preparations"][side]["snapshot"], INTEROP.MAX_ORIGINAL))
        return INTEROP.rebind_public(document, None, side, "prepare", self.root)[0]

    def validate(self, value, trace, jar, index):
        side, scenario = CALLBACKS.CASES[index]
        expected, ids = CALLBACKS.validate_observation(value, side, scenario, jar, ACQUIRE, FINISH)
        if scenario == "load":
            prepared = self.preparation(side)
            CALLBACKS.validate_observation(prepared, side, "prepare", jar, ACQUIRE, FINISH)
            CALLBACKS.validate_saved_payload(prepared, value)
        return PROTOCOL.validate_trace(value, trace, expected, ids, ACQUIRE, FINISH, public_proof=True)

    def rejected(self, mutation, index=2):
        value, trace, jar = self.original(index)
        mutation(value, trace)
        with self.assertRaises(REJECTIONS):
            self.validate(value, trace, jar, index)

    def test_all_four_actual_protocols_reconstruct_complete_public_inputs_and_projections(self):
        for index, row in enumerate(self.cases):
            with self.subTest(context=CALLBACKS.CASES[index]):
                value, trace, jar = self.original(index)
                replay, expected = self.validate(value, trace, jar, index)
                self.assertEqual(CALLBACKS.encode_replay(replay), row["input"])
                self.assertTrue(CALLBACKS.same_json(expected, row["expected"]))
                self.assertEqual(len(replay["scripts"]), 1 if index < 2 else 7)

    def test_method_descriptors_modifiers_lengths_and_hashes_are_pinned_without_original_bytes(self):
        changes = dict(class_="forged", method="forged", signature="()V", modifiers=9,
                       bytecode_length=1, bytecode_sha256="0" * 64, bytecode_hex="00")
        for index in range(4):
            for field, value in changes.items():
                name = "class" if field == "class_" else field
                with self.subTest(context=index, binding=name):
                    self.rejected(lambda snapshot, trace: trace["bindings"]["acquire"].__setitem__(name, value), index)
            with self.subTest(context=index, missing_binding=True):
                self.rejected(lambda snapshot, trace: trace["bindings"].pop("finish"), index)

    def test_missing_unknown_reordered_and_hit_events_fail_after_ordinals_are_repaired(self):
        for index in range(4):
            for mode in ("missing_return", "unknown", "reorder", "short_hit"):
                def mutation(value, trace):
                    operation = len(trace["operations"]) - 1 if mode == "short_hit" else 0
                    events = trace["operations"][operation]["events"]
                    if mode == "missing_return": events.remove(selected_event(trace, "loader_return"))
                    elif mode == "unknown": events[4]["kind"] = "ignored_loader"
                    elif mode == "reorder": events[3], events[4] = events[4], events[3]
                    else: events.pop()
                    renumber_events(trace, operation)
                with self.subTest(context=index, coverage=mode):
                    self.rejected(mutation, index)

    def test_map_key_value_list_pending_and_queue_contents_are_checked_beyond_counts(self):
        for index in range(4):
            for mode in ("key", "value", "list", "pending"):
                def mutation(value, trace):
                    cache = selected_event(trace, "acquire", 112)["cache"]
                    if mode == "key": cache["entries"][0]["key"] ^= 2
                    elif mode == "value": cache["entries"][0]["value"] = copy.deepcopy(cache["fallback"])
                    elif mode == "list": cache["insertions"][0] = copy.deepcopy(cache["fallback"])
                    else: cache["pending"] = [123]
                with self.subTest(context=index, graph=mode):
                    self.rejected(mutation, index)
        for index in (2, 3):
            self.rejected(lambda value, trace: selected_event(trace, "acquire", 100)["cache"]["queue"][0]["region"].__setitem__(1, 123), index)

    def test_all_five_arrays_and_local_minimum_are_checked_at_external_return_boundaries(self):
        for index in range(4):
            for name in ("blocks", "metadata", "skylight", "block_light", "heightmap", "minimum_height"):
                def mutation(value, trace):
                    seam = next(item for item in trace["operations"][0]["seams"] if item["kind"] == ("load" if index < 2 else "generate"))
                    row = next(item for item in seam["after"]["chunks"] if item["chunk"]["id"] == seam["result"]["id"])
                    if name == "minimum_height":
                        row["minimum_height"] += 1
                        row["data"]["minimum_height"] = row["minimum_height"]
                    else:
                        raw = row["data"][name]["values"]
                        row["data"][name]["values"] = ("01" if raw[:2] != "01" else "02") + raw[2:]
                        if name == "heightmap": row["heightmap"] = copy.deepcopy(row["data"][name])
                with self.subTest(context=index, payload=name):
                    self.rejected(mutation, index)

    def test_populate_provider_argument_target_and_flag_before_provider_are_required(self):
        for index in (2, 3):
            for mode in ("receiver", "cache_argument", "target", "flag"):
                def mutation(value, trace):
                    op = trace["operations"][2]
                    seam = next(item for item in op["seams"] if item["kind"] == "populate")
                    if mode == "receiver": seam["receiver_id"] += 10000
                    elif mode == "cache_argument": seam["provider_argument_id"] += 10000
                    elif mode == "target": seam["population_target_id"] += 10000
                    else:
                        for record in visit_dictionaries(seam["before"]):
                            if record.get("id") == seam["population_target_id"] and "flags" in record:
                                record["flags"]["n"] = False
                with self.subTest(context=index, provider=mode):
                    self.rejected(mutation, index)

    def test_dirty_mark_follows_provider_return_and_all_eight_registration_slices_are_retained(self):
        for index in (2, 3):
            for kind in ("mark_dirty", "provider_populate_return", "register_entities_return"):
                def mutation(value, trace):
                    events = trace["operations"][2]["events"]
                    events.remove(next(item for item in events if item["kind"] == kind))
                    renumber_events(trace, 2)
                with self.subTest(context=index, missing=kind):
                    self.rejected(mutation, index)
            def swapped(value, trace):
                events = trace["operations"][2]["events"]
                returned = events.index(selected_event(trace, "provider_populate_return", operation=2))
                dirty = events.index(selected_event(trace, "mark_dirty", 0, operation=2))
                events[returned], events[dirty] = events[dirty], events[returned]
                renumber_events(trace, 2)
            self.rejected(swapped, index)

    def test_main_original_frame_injection_fails_with_coherent_classification_and_frame_counts(self):
        for index in range(4):
            value, trace, jar = self.original(index)
            inventory = trace["operations"][0]["begin_threads"]
            main = next(thread for thread in inventory["threads"] if thread["measured"])
            main["frames"].append({"ordinal": 2, "class": value["bindings"]["cache"], "method": "c",
                "signature": "(II)L" + ("lm" if index % 2 == 0 else "hi") + ";", "code_index": 0,
                "class_loader_id": inventory["original_loader_id"], "original_frame": True})
            main["frame_count"] = len(main["frames"])
            main["original_frame"] = True
            with self.subTest(context=index), self.assertRaisesRegex(ACQUIRE.Failure, "measurement marker is outside"):
                self.validate(value, trace, jar, index)

    def test_consistently_replaced_eight_blank_owner_ids_still_require_actual_helper_tag_anchors(self):
        for index in range(4):
            value, trace, jar = self.original(index)
            replacements = list(range(9000001, 9000009))
            for operation in trace["operations"]:
                for seam in operation["seams"]:
                    for boundary in (seam["before"], seam["after"]):
                        for row in boundary["chunks"]:
                            if row["blank"]:
                                row["array_ids"] = replacements.copy()
            # Every boundary agrees and keeps eight distinct owners. The actual
            # reflected helper tags/JDI links remain the independent anchor.
            with self.subTest(context=index), self.assertRaisesRegex(ACQUIRE.Failure, "blank backing owner IDs match"):
                self.validate(value, trace, jar, index)

    def test_local_world_array_tile_and_entity_owners_are_retained(self):
        for index in range(4):
            for mode in ("world", "array", "tile", "slice", "missing"):
                def mutation(value, trace):
                    seam = next(item for item in trace["operations"][0]["seams"] if item["kind"] == ("load" if index < 2 else "generate"))
                    row = next(item for item in seam["after"]["chunks"] if item["chunk"]["id"] == seam["result"]["id"])
                    if mode == "world": row["world_id"] += 10000
                    elif mode == "array": row["array_ids"][0] += 10000
                    elif mode == "tile": row["tile_map"]["id"] += 10000
                    elif mode == "slice": row["entity_slices"][0]["id"] += 10000
                    else: row["array_ids"].pop()
                with self.subTest(context=index, owner=mode):
                    self.rejected(mutation, index)

    def test_context_object_links_rng_continuity_and_nonzero_time_proof_are_checked(self):
        for index in range(4):
            for mode in ("context", "missing_link", "seed", "rng_owner", "time"):
                def mutation(value, trace):
                    if mode == "context": trace["operations"][0]["loader_id"] += 10000
                    elif mode == "missing_link": trace["object_references"].pop(0)
                    elif mode == "seed": trace["operations"][1]["begin_random"]["generator_seed"] = "000000000000"
                    elif mode == "rng_owner": trace["operations"][0]["begin_random"]["generator_random_id"] += 10000
                    else: trace["operations"][0]["seams"][0]["before"]["world_time"] = "000000000000ffff"
                with self.subTest(context=index, state=mode):
                    self.rejected(mutation, index)
        for index in (0, 1):
            self.rejected(lambda value, trace: trace["time_proof"][2].__setitem__("world_time", "0000000000000000"), index)

    def test_missing_seams_extra_generation_and_opaque_interior_cannot_be_silently_ignored(self):
        for index in range(4):
            for mode in ("seam", "generation_count", "opaque", "event_frame"):
                def mutation(value, trace):
                    operation = trace["operations"][0]
                    if mode == "seam": operation["seams"].pop()
                    elif mode == "generation_count": operation["generated_count"] += 1
                    elif mode == "opaque": operation["events"][4]["opaque_interior"] = True
                    else: operation["events"][4]["original_frames"].pop()
                with self.subTest(context=index, omission=mode):
                    self.rejected(mutation, index)

    def test_actual_saved_loaded_payloads_require_all_arrays_and_observed_minimum_distinction(self):
        for index in (0, 1):
            loaded, trace, jar = self.original(index)
            side, _ = CALLBACKS.CASES[index]
            prepared = self.preparation(side)
            CALLBACKS.validate_saved_payload(prepared, loaded)
            for name in ("blocks", "metadata", "skylight", "block_light", "heightmap", "minimum_height"):
                changed = copy.deepcopy(prepared)
                local = next(row for row in changed["saved"]["cached_locals"] if row["coordinates"] == [1, 0])
                if name == "minimum_height": local["data"][name] = 0
                else: local["data"][name]["values"] = "ff" + local["data"][name]["values"][2:]
                with self.subTest(side=side, saved_array=name), self.assertRaises(CALLBACKS.Failure):
                    CALLBACKS.validate_saved_payload(changed, loaded)


if __name__ == "__main__":
    unittest.main()
