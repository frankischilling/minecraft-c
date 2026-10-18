"""Java-free acquisition guards using the public original-protocol fixture."""

import base64
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zlib


ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / "scripts/reference/cache/compare_acquire.py"
FIXTURE = ROOT / "tests/reference/cache/acquire_fixtures.json"
SPEC = importlib.util.spec_from_file_location("acquire_reference_subject", DRIVER)
ACQUIRE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ACQUIRE)
COMMON, OLD, FINISH, NORMAL, EXECUTED = ACQUIRE.helpers()
REJECTIONS = (ACQUIRE.Failure, FINISH.Failure, NORMAL.Failure)
MAX_BYTES = 32 * 1024 * 1024


def pairs(values):
    result = {}
    for key, value in values:
        if key in result:
            raise ValueError("duplicate fixture key")
        result[key] = value
    return result


def strict_json(data):
    return json.loads(data, object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError("nonfinite fixture number")))


def unpack(value):
    if (type(value) is not dict or set(value) != {"bytes", "sha256", "zlib_base64"} or
            type(value["bytes"]) is not int or not 0 <= value["bytes"] <= MAX_BYTES or
            type(value["sha256"]) is not str or len(value["sha256"]) != 64 or
            type(value["zlib_base64"]) is not str or len(value["zlib_base64"]) > 8 * 1024 * 1024):
        raise ValueError("bounded complete packed protocol")
    encoded = base64.b64decode(value["zlib_base64"], validate=True)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(encoded, value["bytes"] + 1)
    if (len(raw) != value["bytes"] or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail or
            hashlib.sha256(raw).hexdigest() != value["sha256"]):
        raise ValueError("complete protocol stream and fingerprint")
    return raw


def public_cases():
    raw = FIXTURE.read_bytes()
    if len(raw) > 8 * 1024 * 1024:
        raise ValueError("public acquisition fixture bound")
    value = strict_json(raw)
    rows = value["cases"]
    if type(rows) is not list or len(rows) != 3:
        raise ValueError("all three original contexts required")
    result = []
    for index, (record, context) in enumerate(zip(rows, ACQUIRE.CASES)):
        if (record["index"] != index or (record["side"], record["mode"], record["scenario"]) != context or
                record["seed_hex"] != ACQUIRE.SEED or set(record["protocol"]) != {"snapshot", "trace"}):
            raise ValueError("ordered original acquisition contexts")
        snapshot = strict_json(unpack(record["protocol"]["snapshot"]))
        trace = strict_json(unpack(record["protocol"]["trace"]))
        if set(snapshot["class_origins"].values()) != {"supplied:" + record["side"]}:
            raise ValueError("only selected archive host provenance is redacted")
        result.append((record, snapshot, trace))
    return result


def event(trace, kind, code=None, operation=0):
    return next(row for row in trace["operations"][operation]["events"]
                if row["kind"] == kind and (code is None or row["code_index"] == code))


def remove_events(trace, kinds, operation=0):
    rows = trace["operations"][operation]["events"]
    rows[:] = [row for row in rows if row["kind"] not in kinds]
    for ordinal, row in enumerate(rows):
        row["ordinal"] = ordinal


class AcquisitionProtocolGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = public_cases()

    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="mc-acquire-protocol-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()

    def original(self, index=0):
        record, snapshot, trace = self.cases[index]
        snapshot, trace = copy.deepcopy(snapshot), copy.deepcopy(trace)
        jar = self.root / (record["side"] + " selected archive.jar")
        # Protocol validation uses this URL only; no archive is read or executed.
        origin = jar.as_uri().replace("file:///", "file:/", 1)
        snapshot["class_origins"] = dict.fromkeys(snapshot["class_origins"], origin)
        return snapshot, trace, jar

    def validate(self, snapshot, trace, jar, index=0):
        side, mode, scenario = ACQUIRE.CASES[index]
        return ACQUIRE.validate_original(snapshot, trace, side, mode, scenario, jar, NORMAL, FINISH)

    def rejected(self, mutation, index=0):
        snapshot, trace, jar = self.original(index)
        mutation(snapshot, trace)
        with self.assertRaises(REJECTIONS):
            self.validate(snapshot, trace, jar, index)

    def test_all_three_original_protocols_reproduce_public_projection_and_input(self):
        for index, (record, _, _) in enumerate(self.cases):
            with self.subTest(context=ACQUIRE.CASES[index]):
                snapshot, trace, jar = self.original(index)
                projected = self.validate(snapshot, trace, jar, index)
                self.assertEqual(projected, strict_json(unpack(record["expected"])))
                self.assertEqual(ACQUIRE.replay_input(snapshot, FINISH), unpack(record["input"]))
                self.assertEqual(len(projected["operations"]), 8 if index == 2 else 6)
                queries = sum(len(finish["queries"]) for row in projected["operations"] for finish in row["finishes"])
                self.assertEqual(queries, 1024 if index == 2 else 2048)

    def test_missing_loader_finish_and_return_events_fail_even_with_renumbered_ordinals(self):
        for kind in ("loader", "loader_return", "load_wrapper_return", "finish", "generator_ready",
                     "get_return", "acquire_return", "register_tiles_return", "register_entities_return"):
            with self.subTest(kind=kind):
                self.rejected(lambda snapshot, trace: remove_events(trace, {kind}))

    def test_hit_and_pending_event_coverage_is_required(self):
        def truncate(snapshot, trace):
            trace["operations"][1]["events"] = trace["operations"][1]["events"][:1]
        self.rejected(truncate)
        self.rejected(truncate, 2)

    def test_unknown_or_reordered_lifecycle_events_fail(self):
        def unknown(snapshot, trace):
            event(trace, "loader")["kind"] = "unrecognized_loader"
        def reorder(snapshot, trace):
            rows = trace["operations"][0]["events"]
            rows[4], rows[5] = rows[5], rows[4]
            for ordinal, row in enumerate(rows):
                row["ordinal"] = ordinal
        self.rejected(unknown)
        self.rejected(reorder)

    def test_publication_map_key_and_value_are_checked_beyond_counts(self):
        def wrong_key(snapshot, trace):
            event(trace, "acquire", 100)["cache"]["entries"][0]["key"] ^= 12345
        def wrong_value(snapshot, trace):
            cache = event(trace, "acquire", 100)["cache"]
            cache["entries"][0]["value"] = copy.deepcopy(cache["entries"][1]["value"])
        self.rejected(wrong_key)
        self.rejected(wrong_value)

    def test_publication_list_pending_and_queue_contents_are_checked(self):
        def wrong_list(snapshot, trace):
            cache = event(trace, "acquire", 112)["cache"]
            cache["insertions"].reverse()
        def wrong_pending(snapshot, trace):
            event(trace, "acquire", 100)["cache"]["pending"] = [12345]
        def wrong_queue(snapshot, trace):
            region = event(trace, "acquire", 100)["cache"]["queue"][0]["region"]
            region[1] ^= 1
        for name, mutation in (("ordered list", wrong_list), ("pending membership", wrong_pending), ("queue bounds", wrong_queue)):
            with self.subTest(state=name):
                self.rejected(mutation)

    def test_context_ids_and_traced_cache_container_ids_bind_actual_objects(self):
        for name in ("world_id", "cache_id", "generator_id"):
            with self.subTest(context=name):
                self.rejected(lambda snapshot, trace: trace["operations"][0].__setitem__(name, 99999999))
        for name in ("cache_id", "map_id", "list_id", "pending_id", "queue_id"):
            with self.subTest(container=name):
                self.rejected(lambda snapshot, trace: event(trace, "acquire", 100)["cache"].__setitem__(name, 99999999))

    def test_generator_manager_and_chunk_array_ownership_is_retained(self):
        for owner, role in (("generator", "generator"), ("generator", "world"), ("generator", "random"),
                            ("generator", "cave_random"), ("generator", "q"), ("manager", "manager"), ("manager", "a")):
            with self.subTest(owner=owner, role=role):
                self.rejected(lambda snapshot, trace: snapshot["operations"][-1]["after"][owner]["identities"][role].__setitem__("ref", "r99999999"))
        def changed_array(snapshot, trace):
            snapshot["operations"][-1]["after"]["cached_locals"][0]["array_references"][0]["ref"] = "r99999999"
        self.rejected(changed_array)

    def test_missing_or_aliased_identity_roles_fail(self):
        def missing(snapshot, trace):
            snapshot["operations"][-1]["after"]["generator"]["identities"] = {}
        def aliased(snapshot, trace):
            identities = snapshot["operations"][-1]["after"]["generator"]["identities"]
            identities["q"] = copy.deepcopy(identities["r"])
        self.rejected(missing)
        self.rejected(aliased)

    def test_live_policy_time_material_and_opacity_mutations_fail(self):
        def final(snapshot):
            return snapshot["operations"][-1]["after"]
        mutations = {
            "world force": lambda snapshot, trace: final(snapshot).__setitem__("world_force_load", not final(snapshot)["world_force_load"]),
            "provider force": lambda snapshot, trace: final(snapshot).__setitem__("provider_force_load", not final(snapshot)["provider_force_load"]),
            "world time": lambda snapshot, trace: final(snapshot).__setitem__("world_time", "0123456789abcdef"),
            "materials": lambda snapshot, trace: final(snapshot)["materials"][0].__setitem__("top", 255),
            "opacity": lambda snapshot, trace: final(snapshot)["opacity"].__setitem__("values", "00000001" + final(snapshot)["opacity"]["values"][8:]),
            "no sky": lambda snapshot, trace: final(snapshot)["world"].__setitem__("no_sky", not final(snapshot)["world"]["no_sky"]),
        }
        for name, mutation in mutations.items():
            with self.subTest(state=name):
                self.rejected(mutation, 2 if name == "provider force" else 0)

    def test_actual_event_method_receiver_arguments_and_original_frames_are_bound(self):
        mutations = {
            "class": "forged", "method": "forged", "signature": "()V", "receiver": 99999999,
            "arguments": [9, 9], "original_frames": [],
        }
        for name, value in mutations.items():
            with self.subTest(binding=name):
                self.rejected(lambda snapshot, trace: event(trace, "acquire", 0).__setitem__(name, value))

    def test_complete_original_height_query_order_and_results_are_required(self):
        def query_rows(trace):
            return trace["operations"][0]["finishes"][0]["records"]
        def omit(snapshot, trace):
            query_rows(trace).pop()
        def reorder(snapshot, trace):
            rows = query_rows(trace)
            rows[0], rows[1] = rows[1], rows[0]
        def wrong(snapshot, trace):
            query_rows(trace)[0][3] ^= 1
        for name, mutation in (("omitted", omit), ("reordered", reorder), ("wrong unpublished result", wrong)):
            with self.subTest(query=name):
                self.rejected(mutation)

    def test_original_random_continuation_and_independent_jdi_seed_must_agree(self):
        def continuation(snapshot, trace):
            snapshot["initial"]["generator"]["generator_random"]["next_longs"][0] = "0000000000000000"
        def serialized(snapshot, trace):
            value = snapshot["initial"]["generator"]["cave_random"]
            value["serialized_hex"] = value["serialized_hex"][:-18] + "000000000000000078"
        def independent_seed(snapshot, trace):
            trace["operations"][0]["begin_random"]["generator_seed"] = "000000000000"
        for name, mutation in (("cloned continuation", continuation), ("serialized seed", serialized),
                               ("independent JDI seed", independent_seed)):
            with self.subTest(random=name):
                self.rejected(mutation)

    def test_measured_thread_and_complete_stack_inventories_are_required(self):
        def missing_frame(snapshot, trace):
            trace["operations"][0]["begin_threads"]["threads"][0]["frames"].pop()
        def other_live_thread(snapshot, trace):
            trace["operations"][0]["end_threads"]["threads"][1]["daemon"] = False
        def spoof_original(snapshot, trace):
            inventory = trace["operations"][0]["begin_threads"]
            inventory["threads"][0]["frames"][0]["class_loader_id"] = inventory["original_loader_id"]
        def wrong_event_thread(snapshot, trace):
            event(trace, "acquire")["thread_id"] += 1
        for name, mutation in (("omitted frame", missing_frame), ("non-daemon", other_live_thread),
                               ("original loader", spoof_original), ("event thread", wrong_event_thread)):
            with self.subTest(inventory=name):
                self.rejected(mutation)

    def test_dedicated_daemon_requires_exact_inert_body_and_bootstrap_sleep_frames(self):
        def inert(trace):
            return next(thread for thread in trace["operations"][0]["begin_threads"]["threads"] if thread["exact_inert_bt"])
        def wrong_body(snapshot, trace):
            trace["operations"][0]["begin_threads"]["bt_run_hex"] = "00"
        def wrong_frame(snapshot, trace):
            inert(trace)["frames"][-1]["code_index"] += 1
        def fake_sleep(snapshot, trace):
            inert(trace)["frames"][0]["class"] = "forged.Sleep"
        for name, mutation in (("bytecodes", wrong_body), ("run frame", wrong_frame), ("sleep frame", fake_sleep)):
            with self.subTest(daemon=name):
                self.rejected(mutation, 2)


class AcquisitionExecutionGuards(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="mc-acquire-execution-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name).resolve()

    def setup(self):
        paths = {}
        for name in ("client", "server", "executable", "java", "javac", "javap", "jdk_modules", "jdk_ct_sym"):
            path = self.root / name
            path.write_bytes(b"synthetic input")
            paths[name] = path
        release = self.root / "release"
        release.write_text('JAVA_VERSION="17.0.17"\n', encoding="utf-8")
        paths["jdk_release"] = release
        tools = {name: paths[name] for name in ("java", "javac", "javap", "jdk_release", "jdk_modules", "jdk_ct_sym")}
        arguments = ["--client", str(paths["client"]), "--server", str(paths["server"]),
                     "--executable", str(paths["executable"]), "--work-dir", str(self.root / "reports"),
                     "--tested-commit", "a" * 40]
        return paths, tools, arguments

    def test_renamed_driver_and_changed_loaded_helper_fingerprint_fail(self):
        with patch.object(ACQUIRE, "__file__", str(DRIVER.with_name("renamed_acquire.py"))), self.assertRaises(ACQUIRE.Failure):
            ACQUIRE.helpers()
        executed = copy.deepcopy(EXECUTED)
        executed[0]["sha256"] = "0" * 64
        status, report, calls = self.fake_run("compile", 77, executed=executed)
        self.assertEqual((status, report, calls), (1, None, []))

    def test_invalid_selection_environment_and_source_label_fail_before_runtime(self):
        _, _, arguments = self.setup()
        selections = (("duplicate", ["--case", "0", "--case", "0"]), ("outside corpus", ["--case", "3"]),
                      ("bad source", None), ("injected environment", []))
        for name, suffix in selections:
            selected = arguments.copy()
            environment = dict.fromkeys(FINISH.JAVA_ENVIRONMENT, "")
            if name == "bad source":
                selected[-1] = "A" * 40
            else:
                selected += suffix
            if name == "injected environment":
                environment[FINISH.JAVA_ENVIRONMENT[0]] = "injected"
            with self.subTest(preflight=name), patch.dict(os.environ, environment), \
                    patch.object(ACQUIRE, "helpers", return_value=(COMMON, OLD, FINISH, NORMAL, EXECUTED)), \
                    patch.object(COMMON, "runtime_tools", side_effect=COMMON.Unavailable("synthetic missing runtime")) as runtime, \
                    patch.object(COMMON, "run_live", side_effect=AssertionError("external process forbidden")), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ACQUIRE.main(selected), 1)
                runtime.assert_not_called()
        self.assertFalse((self.root / "reports").exists())

    def test_missing_runtime_is_unavailable_without_process_or_capture(self):
        _, _, arguments = self.setup()
        with patch.dict(os.environ, dict.fromkeys(FINISH.JAVA_ENVIRONMENT, "")), \
                patch.object(ACQUIRE, "helpers", return_value=(COMMON, OLD, FINISH, NORMAL, EXECUTED)), \
                patch.object(COMMON, "runtime_tools", side_effect=COMMON.Unavailable("synthetic missing runtime")), \
                patch.object(COMMON, "run_live", side_effect=AssertionError("external process forbidden")), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(ACQUIRE.main(arguments), 77)
        self.assertFalse((self.root / "reports").exists())

    def test_setup77_proof_rejects_any_measurement_snapshot_or_stopped_process(self):
        trace, snapshot = self.root / "trace.json", self.root / "snapshots.json"
        sidecar = Path(str(trace) + ".unavailable.json")
        proof = {"schema": 1, "status": "unavailable", "stage": "original_setup", "child_exit": 77,
                 "side": "client", "mode": "persistent-client", "scenario": "shared",
                 "jar_sha256": FINISH.JAR_HASHES["client"], "begin_markers": 0, "end_markers": 0,
                 "completed_operations": 0, "snapshot_exists": False, "child_command": ["synthetic-child"],
                 "debuggee_mode": "interpreted", "elapsed_ms": 0}
        process = COMMON.ProcessResult([], 77, 0.0, False, None, 0, 0, 0, 0, 0)

        def validate():
            return ACQUIRE.setup_unavailable(process, trace, snapshot, "client", "persistent-client", "shared",
                                             COMMON, FINISH.JAR_HASHES["client"])

        sidecar.write_text(json.dumps(proof), encoding="utf-8")
        self.assertEqual(validate(), proof)
        for name in ("begin_markers", "end_markers", "completed_operations", "snapshot_exists"):
            changed = dict(proof)
            changed[name] = True if name == "snapshot_exists" else 1
            sidecar.write_text(json.dumps(changed), encoding="utf-8")
            with self.subTest(evidence=name), self.assertRaises(ACQUIRE.Failure):
                validate()
        sidecar.write_text(json.dumps(proof), encoding="utf-8")
        for artifact in (trace, snapshot):
            artifact.write_text("{}", encoding="utf-8")
            with self.subTest(artifact=artifact.name), self.assertRaises(ACQUIRE.Failure):
                validate()
            artifact.unlink()
        process.stop_reason = "output limit"
        with self.assertRaises(ACQUIRE.Failure):
            validate()

    def fake_run(self, stage, code, *, mutate=None, executed=None, module_text="jdk.jdi@17\njdk.jdwp.agent@17\n"):
        paths, tools, arguments = self.setup()
        original_record = COMMON.file_record
        calls = []
        previous = set((self.root / "reports").glob("capture-*/manifest.json"))

        def record(path, byte_limit=None):
            value = original_record(path, byte_limit=byte_limit)
            for side in ("client", "server"):
                if Path(path) == paths[side]:
                    value["sha256"] = FINISH.JAR_HASHES[side]
            return value

        def run(command, stdout, stderr, cwd, **kwargs):
            current = "jdk-modules" if "--list-modules" in command else "compile" if Path(command[0]) == paths["javac"] else "controller"
            calls.append(current)
            Path(stdout).write_bytes(module_text.encode("utf-8") if current == "jdk-modules" else b"")
            Path(stderr).write_bytes(b"")
            classes = next((Path(command[n + 1]) for n, value in enumerate(command) if value == "-d"), None)
            if classes is not None:
                for name in ("ChunkObservation", "CacheAcquisitionObservation", "CacheAcquisitionController"):
                    (classes / (name + ".class")).write_bytes(bytes.fromhex("cafebabe0000003d") + bytes(8))
            if current == "controller" and mutate == "input":
                paths["executable"].write_bytes(b"SYNTHETIC INPUT")
            if current == "controller" and mutate == "class":
                (Path(command[command.index("-cp") + 1]) / "CacheAcquisitionController.class").write_bytes(b"changed class")
            return COMMON.ProcessResult(list(map(str, command)), code if current == stage else 0,
                                        0.0, False, None, 0, 0, 0, 0, 0)

        with patch.dict(os.environ, dict.fromkeys(FINISH.JAVA_ENVIRONMENT, "")), \
                patch.object(ACQUIRE, "helpers", return_value=(COMMON, OLD, FINISH, NORMAL, EXECUTED if executed is None else executed)), \
                patch.object(COMMON, "runtime_tools", return_value=tools), patch.object(COMMON, "tool_versions"), \
                patch.object(COMMON, "file_record", side_effect=record), patch.object(COMMON, "run_live", side_effect=run), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            status = ACQUIRE.main(arguments)
        manifests = set((self.root / "reports").glob("capture-*/manifest.json")) - previous
        self.assertLessEqual(len(manifests), 1)
        report = strict_json(next(iter(manifests)).read_bytes()) if manifests else None
        return status, report, calls

    def test_compile_and_unproved_controller_exit77_are_failures(self):
        for stage in ("compile", "controller"):
            with self.subTest(stage=stage):
                status, report, calls = self.fake_run(stage, 77)
                self.assertEqual((status, report["status"]), (1, "fail"))
                self.assertTrue(report["errors"])
                self.assertEqual(calls, ["jdk-modules", "compile"] if stage == "compile" else ["jdk-modules", "compile", "controller"])

    def test_module_prerequisite_absence_and_failed_module_process_are_distinct(self):
        status, report, calls = self.fake_run("compile", 77, module_text="java.base@17\n")
        self.assertEqual((status, report["status"], calls), (77, "unavailable", ["jdk-modules"]))
        self.assertNotIn("classes_before", report)
        status, report, calls = self.fake_run("jdk-modules", 77)
        self.assertEqual((status, report["status"], calls), (1, "fail", ["jdk-modules"]))

    def test_debuggee_command_requires_exact_runtime_options_helpers_and_arguments(self):
        paths, tools, _ = self.setup()
        classes = self.root / "classes"
        arguments = [str(paths["client"]), "client", "persistent-client", ACQUIRE.SEED,
                     str(self.root / "owned-world"), str(self.root / "snapshots.json"), "shared"]
        command = [str(tools["java"]), "-Xint", "-Xmx512m",
                   "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                   "-cp", str(classes), "CacheAcquisitionObservation", *arguments]
        ACQUIRE.validate_command(command, arguments, tools, classes)
        for index, value in ((0, str(self.root / "other-java")), (1, "-Xcomp"), (2, "-Xmx4g"),
                             (3, "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=*:0"),
                             (5, str(self.root / "foreign-classes")), (6, "OtherObservation"), (7, "other.jar"),
                             (10, "0000000000000018"), (13, "other-scenario")):
            changed = command.copy()
            changed[index] = value
            with self.subTest(argument=index), self.assertRaises(ACQUIRE.Failure):
                ACQUIRE.validate_command(changed, arguments, tools, classes)

    def test_attributed_input_and_helper_class_changes_force_failure(self):
        for mutation, message in (("input", "attributed input changed"), ("class", "compiled helper")):
            with self.subTest(preservation=mutation):
                status, report, _ = self.fake_run("controller", 77, mutate=mutation)
                self.assertEqual((status, report["status"]), (1, "fail"))
                self.assertTrue(any(message in error for error in report["errors"]), report["errors"])


if __name__ == "__main__":
    unittest.main()
