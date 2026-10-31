#!/usr/bin/env python3
"""Capture the original dedicated profile through an owned loopback debugger.

The report records bounded mechanical completion and selected input provenance.
Graph, Random-event and disk semantics require the separate file-only audit.
"""
from __future__ import annotations

import argparse
import ast
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import time
import traceback
import types

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[5]
PROCESS_SUPPORT = Path(__file__).with_name("process.py").resolve()
P = types.ModuleType("dedicated_capture_process")
P.__file__ = str(PROCESS_SUPPORT)
exec(compile(PROCESS_SUPPORT.read_bytes(), str(PROCESS_SUPPORT), "exec"), P.__dict__)
need, ordinary, fingerprint = P.need, P.ordinary, P.fingerprint
input_snapshot, inventory = P.input_snapshot, P.inventory
launch, stream_bounds, cleanup = P.launch, P.stream_bounds, P.cleanup
run, environment = P.run, P.environment
MIB, CLEANUP_SECONDS = P.MIB, P.CLEANUP_SECONDS
SEED = "0000000000000017"
PROFILE = "dedicated-natural-unload"
SERVER_SHA = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"
SOURCE_CONTEXT = ROOT / "scripts/reference/world/population/compare_natural.py"
SOURCES = tuple(ROOT / name for name in (
    "tests/reference/chunk/ChunkObservation.java",
    "tests/reference/cache/CacheCallbacksObservation.java",
    "tests/reference/cache/ChunkLifecycleObservation.java",
    "tests/reference/world/population/NaturalPopulationObservation.java",
    "tests/reference/world/population/dedicated/PopulationDedicatedObservation.java",
    "tests/reference/world/population/dedicated/PopulationDedicatedRandomObserver.java",
))
SOURCE_PINS = {
    "ChunkObservation.java": "213c7ffb98eb8aaf2aeca9e38d65ba528ab5f4f42e0fab4c07b341ffd93b899e",
    "CacheCallbacksObservation.java": "5aac2c1a0bf28e8d2f45d2b504eba66f27faa4990b537210ef066736eab7482f",
    "ChunkLifecycleObservation.java": "5be3fe26d8bd7e9ce47fbe8f1adb9718bbe204b7279a3f7ab023856088a4644e",
    "NaturalPopulationObservation.java": "f6bc4f17eb2132c2c22b961355b593e530f083de0b198b8936a24371a358f1ba",
    "PopulationDedicatedObservation.java": "f54a8dd01c924c3d5949a60fb9894c89cfec8c8f35e81a2f129068062ce80835",
    "PopulationDedicatedRandomObserver.java": "798b7e60665ec6f14cdd8ee5c5cd58459e83f93cce2d6876f5665db449a8bffd",
}
BANNER = re.compile(rb"^Listening for transport dt_socket at address: ([0-9]{1,5})\r?\n", re.MULTILINE)
HEAD = re.compile(rb"[0-9a-f]{40}\r?\n\Z")
DECIMAL = re.compile(r"(?:0|[1-9][0-9]{0,8})\Z")
PAIR_SECONDS, OBSERVER_SECONDS = 210, 180
TRACE_LIMIT, JSON_LIMIT = 16 * MIB, 64 * MIB

def observe(java, classes, server_jar, world_parent, output, work, receipts, env):
    children = []
    failure = None
    started = time.monotonic()
    deadline = started + PAIR_SECONDS
    # Reserve both bounded waits inside the outer pair budget.
    active_deadline = deadline - 2 * CLEANUP_SECONDS
    port = None
    with ExitStack() as files:
        def bounds():
            stream_bounds(children)
            need(time.monotonic() < active_deadline, "outer process-pair deadline with cleanup reserve")
            if output.exists():
                need(output.is_file() and not output.is_symlink() and output.stat().st_size <= JSON_LIMIT,
                     "target JSON output bound")
        try:
            target = launch(
                [java, "-Xint", "-Xmx512m",
                 "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0",
                 "--add-opens", "java.base/java.util=ALL-UNNAMED", "-cp", classes,
                 "PopulationDedicatedObservation", server_jar, "server", SEED, world_parent, output],
                work, "target", receipts, files, children, env, MIB, 4 * MIB)
            target["receipt"]["pair_deadline_seconds"] = PAIR_SECONDS
            banner_deadline = min(active_deadline, time.monotonic() + 10)
            while port is None:
                bounds()
                need(target["child"].poll() is None, "target exited before debugger attach")
                need(time.monotonic() < banner_deadline, "JDWP listener deadline")
                matches = BANNER.findall(target["stdout"].read_bytes()) + BANNER.findall(target["stderr"].read_bytes())
                need(len(matches) <= 1, "duplicate JDWP listener banners")
                if matches:
                    port = int(matches[0])
                    need(1 <= port <= 65535, "dynamic loopback listener port")
                else:
                    time.sleep(0.025)
            observer = launch(
                [java, "--add-modules", "jdk.jdi", "-Xmx256m", "-cp", classes,
                 "PopulationDedicatedRandomObserver", str(port)],
                work, "observer", receipts, files, children, env, TRACE_LIMIT, 4 * MIB)
            observer["receipt"]["deadline_seconds"] = OBSERVER_SECONDS
            while True:
                target_status = target["child"].poll()
                observer_status = observer["child"].poll()
                bounds()
                need(target_status in (None, 0), "original target failed")
                need(observer_status in (None, 0), "JDI observer failed")
                if observer_status is None and time.monotonic() - observer["started"] >= OBSERVER_SECONDS:
                    observer["receipt"]["timed_out"] = True
                    raise TimeoutError("observer external 180-second deadline")
                if target_status is not None and observer_status is not None:
                    break
                time.sleep(0.025)
        except BaseException as error:
            failure = error
            if isinstance(error, TimeoutError) or "deadline" in str(error):
                for owned in children:
                    if owned["child"].poll() is None:
                        owned["receipt"]["timed_out"] = True
        finally:
            failure = cleanup(children, failure)
    elapsed = time.monotonic() - started
    if failure is not None:
        raise failure
    need(elapsed <= PAIR_SECONDS, "pair exceeded outer deadline including cleanup")
    need(len(children) == 2 and all(owned["receipt"]["natural_exit"] and
         owned["receipt"]["exit_code"] == 0 and not owned["receipt"]["output_limited"]
         for owned in children), "both children must exit naturally and reap")
    outgoing, errors = (work / "target.stdout").read_bytes(), (work / "target.stderr").read_bytes()
    banners = BANNER.findall(outgoing) + BANNER.findall(errors)
    need(len(banners) == 1 and int(banners[0]) == port, "retained unique listener banner")
    # Original bootstrap console remains on stderr; preserve it without imposing
    # the unrelated tiny constructor target's banner-only stream contract.
    need((work / "observer.stderr").stat().st_size == 0, "unexpected observer stderr")
    return {"host": "127.0.0.1", "port": port, "transport": "dt_socket",
            "suspend": "y", "seconds": round(elapsed, 6)}


def trace_envelope(path):
    raw = path.read_bytes()
    need(0 < len(raw) <= TRACE_LIMIT and raw.endswith(b"\n") and b"\r" not in raw, "complete bounded LF trace")
    need(all(value in (9, 10) or 32 <= value <= 126 for value in raw), "ASCII TSV trace")
    counts, life, metadata, result_count, rows = {}, {}, {}, 0, 0
    first = last = None
    for line in raw[:-1].split(b"\n"):
        need(0 < len(line) <= 16384, "trace line bound")
        fields = line.decode("ascii").split("\t")
        need(all(0 < len(field) <= 8192 for field in fields), "trace field bounds")
        rows += 1
        if first is None:
            first = fields
        last = fields
        if fields[0] == "meta":
            need(len(fields) >= 3 and fields[1] not in metadata, "unique metadata receipt")
            metadata[fields[1]] = fields[2:]
        elif fields[0] == "count":
            expected_length = 9 if len(fields) > 1 and fields[1] == "default_constructors" else 7
            need(len(fields) == expected_length and fields[1] not in counts, "unique complete count receipt")
            need(all(DECIMAL.fullmatch(fields[index]) for index in range(2, len(fields), 2)),
                 "count decimal framing")
            counts[fields[1]] = fields
        elif fields[0] == "life":
            need(len(fields) >= 2 and fields[1] not in life, "unique lifecycle receipt")
            life[fields[1]] = fields
        elif fields[0] == "result":
            result_count += 1
    need(first == ["meta", "protocol", "dedicated-random-observation-2"] and
         last == ["result", "complete"] and result_count == 1, "observer protocol completion")
    for key, expected in {
        "source_context": ["outer-capture-report"], "mechanical_capture_only": ["true"],
        "tested_commit": ["null"], "native_comparison": ["false"],
        "can_get_method_return_values": ["1"], "can_get_bytecodes": ["1"],
        "can_use_instance_filters": ["1"],
    }.items():
        need(metadata.get(key) == expected, "observer required metadata: " + key)
    need(set(counts) == {"events", "default_constructors", "ordinary_defaults",
                        "World_r_method_entries", "operations"}, "complete count envelope")
    expected_labels = {
        "events": ("breakpoints", "class_prepares"),
        "default_constructors": ("uniquifier_returns", "nano_time_returns", "delegate_returns"),
        "ordinary_defaults": ("subclass_base_defaults", "validated_subclass_base_setSeed_returns"),
        "World_r_method_entries": ("World_r_method_returns", "validated_constructor_scramble_returns"),
        "operations": ("far_scopes", "held_mirrors"),
    }
    for key in ("events", "ordinary_defaults", "World_r_method_entries", "operations"):
        need(counts[key][3] == expected_labels[key][0] and
             counts[key][5] == expected_labels[key][1], "count label envelope: " + key)
    constructor = counts["default_constructors"]
    need(constructor[3::2] == list(expected_labels["default_constructors"]), "constructor count labels")
    values = {key: {fields[index]: int(fields[index + 1])
                    for index in range(1, len(fields), 2)}
              for key, fields in counts.items()}
    constructor = values["default_constructors"]
    defaults = constructor["default_constructors"]
    need(1 <= defaults <= 4096 and set(constructor.values()) == {defaults}, "equal constructor completion counts")
    ordinary = values["ordinary_defaults"]
    need(ordinary["ordinary_defaults"] + ordinary["subclass_base_defaults"] == defaults,
         "ordinary/subclass constructor completion counts")
    need(ordinary["validated_subclass_base_setSeed_returns"] <= ordinary["subclass_base_defaults"],
         "subclass count envelope")
    draws = values["World_r_method_entries"]
    need(draws["World_r_method_entries"] == draws["World_r_method_returns"], "paired draw completion counts")
    need(draws["validated_constructor_scramble_returns"] ==
         ordinary["ordinary_defaults"] + ordinary["validated_subclass_base_setSeed_returns"],
         "constructor scramble completion counts")
    operations = values["operations"]
    need(operations["operations"] == 17 and operations["far_scopes"] == 1 and
         1 <= operations["held_mirrors"] <= 16384, "operation/far/mirror completion counts")
    need(1 <= values["events"]["events"] <= 25000, "event completion bound")
    need(set(life) == {"VM_start", "target_System_exit", "VM_death", "VM_disconnect"} and
         life["VM_death"] == ["life", "VM_death"] and
         life["VM_disconnect"] == ["life", "VM_disconnect"] and
         len(life["target_System_exit"]) == 4 and life["target_System_exit"][2] == "0",
         "complete target lifecycle envelope")
    return {"rows": rows, "reported_counts": values, **fingerprint(path),
            "independent_event_semantic_audit": False}


def target_envelope(path):
    need(0 < path.stat().st_size <= JSON_LIMIT, "bounded target JSON")
    raw = path.read_bytes()
    need(not raw.startswith(b"\xef\xbb\xbf"), "target UTF-8 without BOM")
    def pairs(entries):
        result = {}
        for key, value in entries:
            need(key not in result, "duplicate target JSON key")
            result[key] = value
        return result
    def constant(value):
        raise ValueError("nonfinite target JSON token: " + value)
    document = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
    need(type(document) is dict, "target JSON object")
    need(type(document.get("schema")) is int and document["schema"] == 2 and
         document.get("scope") == "original-only-dedicated-natural-population-unload" and
         document.get("side") == "server" and document.get("seed_hex") == SEED and
         document.get("jar_sha256") == SERVER_SHA, "target JSON schema/input attribution")
    for key in ("passed", "experiment", "mechanical_capture_only"):
        need(document.get(key) is True, "target original-only flag: " + key)
    for key in ("qualified", "native_comparison"):
        need(document.get(key) is False, "target unqualified flag: " + key)
    need("tested_commit" in document and document["tested_commit"] is None and
         document.get("source_context") == "outer-capture-report" and
         "custom_observer_uncommitted" not in document and
         document.get("profile") == PROFILE, "target original-only profile attribution")
    need("failure" in document and document["failure"] is None and
         document.get("region_close_completed") is True and
         type(document.get("operations")) is list and len(document["operations"]) == 17 and
         type(document.get("states")) is list and document["states"] and
         type(document.get("blobs")) is dict and document["blobs"], "target complete output envelope")
    return {**fingerprint(path), "path": path.name, "independent_graph_semantic_audit": False}


def selected_tool(path):
    """Resolve normal tool aliases, then pin the selected regular binary."""
    candidate = shutil.which(str(path))
    need(candidate is not None, "selected executable missing: " + str(path))
    return ordinary(Path(candidate).resolve(strict=True))


def jdk_inputs(java, javac):
    need(java.parent == javac.parent, "one selected runtime/compiler JDK required")
    root = java.parent.parent
    release = (root / "release").read_text(encoding="utf-8")
    need(re.search(r'(?m)^JAVA_VERSION="17(?:\.[^"]*)?"$', release) is not None,
         "selected full JDK17 required; class/PC invariants remain mandatory")
    selected = {"java": java, "javac": javac, "release": root / "release",
                "modules": root / "lib/modules", "source_archive": root / "lib/src.zip"}
    if os.name == "nt":
        native = {"jvm": "bin/server/jvm.dll", "java_native": "bin/java.dll",
                  "jdwp": "bin/jdwp.dll", "dt_socket": "bin/dt_socket.dll"}
        support = ("bin/jli.dll", "bin/zip.dll", "bin/verify.dll", "bin/net.dll", "bin/nio.dll",
                   "bin/attach.dll", "bin/jimage.dll")
        optional = ("bin/msvcp140.dll", "bin/vcruntime140.dll", "bin/vcruntime140_1.dll",
                    "bin/ucrtbase.dll")
    else:
        need(sys.platform.startswith("linux"), "dedicated capture supports Windows and Linux hosts")
        native = {"jvm": "lib/server/libjvm.so", "java_native": "lib/libjava.so",
                  "jdwp": "lib/libjdwp.so", "dt_socket": "lib/libdt_socket.so"}
        support = ("lib/libjli.so", "lib/libzip.so", "lib/libverify.so", "lib/libnet.so",
                   "lib/libnio.so", "lib/libattach.so", "lib/libjimage.so")
        optional = ()
    selected.update({role: root / relative for role, relative in native.items()})
    for relative in (*support, "lib/ct.sym", "lib/jrt-fs.jar", "jmods/java.base.jmod",
                     "jmods/jdk.compiler.jmod", "jmods/jdk.jdi.jmod"):
        selected["jdk/" + relative] = root / relative
    for relative in optional:
        if (root / relative).is_file():
            selected["jdk/" + relative] = root / relative
    return {role: ordinary(path.resolve(strict=True)) for role, path in selected.items()}


def strict_json(path, limit=JSON_LIMIT):
    path = ordinary(path)
    need(path.is_file() and 0 < path.stat().st_size <= limit, "bounded JSON input: " + str(path))
    raw = path.read_bytes()
    need(0 < len(raw) <= limit, "bounded JSON input read: " + str(path))

    def pairs(rows):
        result = {}
        for key, value in rows:
            need(key not in result, "duplicate JSON key")
            result[key] = value
        return result

    def invalid(value):
        raise ValueError("nonfinite JSON number: " + value)

    return json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=invalid)


def source_context(work, receipts, env, git, suffix):
    """Reuse the existing source-context body with bounded, journaled children."""
    raw = SOURCE_CONTEXT.read_bytes()
    module = ast.parse(raw, filename=str(SOURCE_CONTEXT))
    bodies = [node for node in module.body if isinstance(node, ast.FunctionDef) and
              node.name == "source_context"]
    need(len(bodies) == 1, "one existing source_context function")
    directory = work / ("source-" + suffix)
    directory.mkdir()

    def bounded(command, context_work, name, records, seconds=60):
        command = list(command)
        if command[0] == "git":
            need(git is not None, "Git executable required for checkout context")
            command[0] = str(git)
        return run(command, context_work, name, records, env, seconds=seconds,
                   receipt_root=work, receipt_name=directory.name + "-" + name)

    def equal(actual, expected, label):
        need(actual == expected, label)

    namespace = {"ROOT": ROOT, "re": re, "run": bounded, "read_json": strict_json,
                 "require": need, "equal": equal,
                 "CAPTURE": types.SimpleNamespace(file_fingerprint=fingerprint)}
    exec(compile(ast.Module(body=bodies, type_ignores=[]), str(SOURCE_CONTEXT), "exec"), namespace)
    context = namespace["source_context"](directory, receipts)
    need(type(context.get("source_head")) is str and
         re.fullmatch(r"[0-9a-f]{40}", context["source_head"]) is not None and
         type(context.get("source_dirty")) is bool, "complete source context")
    if not context["source_export"]:
        raw_tree, errors = run([git, "-C", ROOT, "rev-parse", context["source_head"] + "^{tree}"],
                               directory, "git-tree", receipts, env, receipt_root=work,
                               receipt_name=directory.name + "-git-tree")
        need(not errors and HEAD.fullmatch(raw_tree), "Git tree framing")
        context["source_tree"] = raw_tree.strip().decode("ascii")
    segment = ast.get_source_segment(raw.decode("utf-8"), bodies[0]).encode("utf-8")
    context["source_context_body_sha256"] = hashlib.sha256(segment).hexdigest()
    return context


def copy_input(source, target):
    with ordinary(source).open("rb") as incoming, target.open("xb") as outgoing:
        shutil.copyfileobj(incoming, outgoing, MIB)
    need(fingerprint(source) == fingerprint(target), "exact retained input copy")


def compiled_classes(directory):
    result = inventory(directory)
    names = {name for name, row in result.items() if row["kind"] == "file"}
    need(names and all(name.endswith(".class") for name in names) and
         "PopulationDedicatedObservation.class" in names and
         "PopulationDedicatedRandomObserver.class" in names, "complete fresh compiler class output")
    for name in names:
        header = (directory / name).read_bytes()[:8]
        need(header[:4] == b"\xca\xfe\xba\xbe" and len(header) == 8 and
             int.from_bytes(header[6:8], "big") == 61, "actual release17 helper class")
    return result


def run_capture(server_jar, java, javac, work_dir, tested_commit=""):
    """Return the fresh capture and report, including an explicit failed report.

    Admission errors before creating the owned directory raise ValueError or an
    OS error. Once created, the directory retains report.json on failure.
    """
    server_jar, java, javac, work_dir = map(Path, (server_jar, java, javac, work_dir))
    need(not tested_commit or re.fullmatch(r"[0-9a-f]{40}", tested_commit),
         "tested commit must be a full lowercase object ID")
    env = environment()
    java, javac = selected_tool(java), selected_tool(javac)
    jar = ordinary(server_jar.absolute()).resolve(strict=True)
    git = selected_tool("git") if (ROOT / ".git").exists() else None
    work = ordinary(work_dir.absolute())
    need(work.parent.is_dir(), "capture parent directory must already exist")
    work.mkdir(parents=False, exist_ok=False)
    report = {"schema": 2, "scope": "original-only instrumented dedicated capture",
              "profile": PROFILE, "source_head": None, "source_dirty": True,
              "tested_commit": tested_commit or None, "native_comparison": False,
              "qualification": False, "qualified": False, "qualification_complete": False,
              "semantic_audit_complete": False, "mechanical_capture_only": True,
              "jdi_observed_unqualified": False, "passed": False, "inputs_unchanged": False,
              "processes": [], "copied_sources": {}, "copied_support": {},
              "process_policy": {"stdin": "DEVNULL", "cwd": str(ROOT),
                  "pair_outer_seconds": PAIR_SECONDS,
                  "pair_cleanup_reserve_seconds": 2 * CLEANUP_SECONDS,
                  "observer_seconds": OBSERVER_SECONDS, "compile_seconds": 120,
                  "ordinary_child_seconds": 60, "cleanup_seconds_per_child": CLEANUP_SECONDS,
                  "target_stdout_bytes": MIB, "target_stderr_bytes": 4 * MIB,
                  "observer_stdout_bytes": TRACE_LIMIT, "observer_stderr_bytes": 4 * MIB,
                  "threshold_policy": "Poll all streams; reject overshoot and retain complete produced streams without truncation.",
                  "java_environment_options": {name: "absent" for name in P.OPTIONS},
                  "git_environment": {"GIT_OPTIONAL_LOCKS": "0", "GIT_TERMINAL_PROMPT": "0", "GIT_PAGER": "cat"}},
              "payload_source_context": "Original graph and trace defer source attribution to this independently retained outer report.",
              "provenance_limit": "Selected project/helper/tool inputs and modular JDK files; no transitive OS-library, full Python standard-library or whole-process attestation.",
              "interpretation_limit": "Debugger suspension and held mirrors affect clocks, scheduling and lifetime. Independent constructor/draw, owner/graph, NBT/disk and source-semantic audits are separate; this is not an uninstrumented replay or native qualification."}
    paths = {"runner": Path(__file__).resolve(), "process_support": PROCESS_SUPPORT,
             "source_context_support": SOURCE_CONTEXT, "python": Path(sys.executable).resolve(),
             "server_jar": jar, **{source.name: source for source in SOURCES}}
    if git is not None:
        paths["git"] = git
    manifest = ROOT / ".source-manifest.json"
    if manifest.is_file():
        paths["source_export_manifest"] = manifest
    entry = Path(sys.argv[0]).absolute()
    if entry.is_file():
        paths["actual_entry_source"] = ordinary(entry.resolve(strict=True))
    for pattern in ("python*.dll", "vcruntime*.dll"):
        for path in sorted(Path(sys.executable).parent.glob(pattern)):
            paths["python/" + path.name] = path.resolve(strict=True)
    started = time.monotonic()
    exit_code = 1
    try:
        paths.update(jdk_inputs(java, javac))
        report["inputs_before"] = input_snapshot(paths)
        before = report["inputs_before"]
        need(before["server_jar"]["sha256"] == SERVER_SHA and before["server_jar"]["bytes"] <= 2 * MIB,
             "pinned supplied original server archive")
        for role, expected in SOURCE_PINS.items():
            need(before[role]["sha256"] == expected, "reviewed helper source SHA-256: " + role)
        report["python_version"] = sys.version
        report["python_runtime"] = {"executable": sys.executable, "prefix": sys.prefix, "base_prefix": sys.base_prefix}
        report["runner_process"] = {"pid": os.getpid(), "parent_pid": os.getppid(),
                                    "argv": sys.argv, "cwd": str(Path.cwd()),
                                    "entry_source_role": "actual_entry_source" if "actual_entry_source" in paths else None}
        baseline = source_context(work, report["processes"], env, git, "before")
        report.update(baseline, source_before=baseline)
        need(not tested_commit or baseline["source_head"] == tested_commit and
             baseline["source_dirty"] is False, "tested commit requires its matching clean source context")
        for name, command in (("java-version", [java, "-version"]), ("javac-version", [javac, "-version"])):
            run(command, work, name, report["processes"], env)
        if git is not None:
            run([git, "--version"], work, "git-version", report["processes"], env)
        source_dir, support_dir, classes = work / "sources", work / "support", work / "classes"
        for directory in (source_dir, support_dir, classes):
            directory.mkdir()
        exported = strict_json(manifest)["files"] if baseline["source_export"] else None
        for source in SOURCES:
            target = source_dir / source.name
            copy_input(source, target)
            journal = None
            exact = False
            if tested_commit and git is not None:
                journal = "git-blob-" + source.stem
                stdout, stderr = run([git, "-C", ROOT, "show", baseline["source_head"] + ":" + source.relative_to(ROOT).as_posix()],
                                     work, journal, report["processes"], env)
                need(not stderr and stdout == source.read_bytes(), "exact tested source blob: " + source.name)
                exact = True
            if exported is not None:
                need(exported.get(source.relative_to(ROOT).as_posix()) == fingerprint(source),
                     "selected helper belongs to verified export")
            report["copied_sources"][source.name] = {"path": target.relative_to(work).as_posix(),
                **fingerprint(target), "tracked_exact_blob": exact, "git_blob_process": journal}
        support_roles = ["runner", "process_support", "source_context_support"]
        if "source_export_manifest" in paths:
            support_roles.append("source_export_manifest")
        if "actual_entry_source" in paths and paths["actual_entry_source"].suffix == ".py":
            support_roles.append("actual_entry_source")
        for role in support_roles:
            source = paths[role]
            target = support_dir / (role + source.suffix)
            copy_input(source, target)
            if exported is not None and source.is_relative_to(ROOT) and role != "source_export_manifest":
                need(exported.get(source.relative_to(ROOT).as_posix()) == fingerprint(source),
                     "selected support belongs to verified export")
            report["copied_support"][role] = {"path": target.relative_to(work).as_posix(), **fingerprint(target)}
        report["retained_sources_before_compile"] = inventory(source_dir)
        report["retained_support_before_compile"] = inventory(support_dir)
        for name, command in (
            ("compile-target", [javac, "-J-Xmx256m", "--release", "17", "-encoding", "UTF-8",
                "-classpath", classes, "-sourcepath", source_dir, "-d", classes,
                *(source_dir / source.name for source in SOURCES[:-1])]),
            ("compile-observer", [javac, "-J-Xmx256m", "--add-modules", "jdk.jdi", "--release", "17", "-g",
                "-encoding", "UTF-8", "-classpath", classes, "-sourcepath", source_dir,
                "-d", classes, source_dir / SOURCES[-1].name])):
            stdout, stderr = run(command, work, name, report["processes"], env, seconds=120)
            need(not stdout and not stderr, "unexpected compiler output: " + name)
        report["classes_before_pair"] = compiled_classes(classes)
        world_parent = work / "original-parent"
        world_parent.mkdir()
        need(not any(world_parent.iterdir()), "fresh empty target world parent")
        output = work / "original.json"
        report["debugger_pair"] = observe(java, classes, jar, world_parent, output,
                                           work, report["processes"], env)
        report["observer_trace"] = {"path": "observer.stdout", **trace_envelope(work / "observer.stdout")}
        report["original_output"] = target_envelope(output)
        report["final_disk_inventory"] = inventory(world_parent)
        report["classes_after_pair"] = compiled_classes(classes)
        need(report["classes_before_pair"] == report["classes_after_pair"], "compiled classes changed during capture")
        need(report["retained_sources_before_compile"] == inventory(source_dir), "retained Java source changed")
        need(report["retained_support_before_compile"] == inventory(support_dir), "retained support changed")
        after = source_context(work, report["processes"], env, git, "after")
        report["source_after"] = after
        need(after == baseline, "source context changed during capture")
        report["jdi_observed_unqualified"] = report["passed"] = True
        exit_code = 0
    except BaseException as error:
        report["failure"] = f"{type(error).__name__}: {error}"
        traceback.print_exc()
    finally:
        try:
            report["inputs_after"] = input_snapshot(paths)
            report["inputs_unchanged"] = report.get("inputs_before") == report["inputs_after"]
            need(report["inputs_unchanged"], "selected input changed during capture")
        except BaseException as error:
            report["passed"] = report["jdi_observed_unqualified"] = False
            report["input_failure"] = f"{type(error).__name__}: {error}"
            exit_code = 1
        try:
            if (work / "original-parent").is_dir():
                report["final_disk_inventory_after_reaping"] = inventory(work / "original-parent")
                if "final_disk_inventory" in report:
                    need(report["final_disk_inventory"] == report["final_disk_inventory_after_reaping"],
                         "final disk changed after complete reaping")
            report["retained_files_before_report"] = inventory(work)
            report["retention_self_exclusion"] = "This inventory precedes the exclusive final report.json write; that sole report file is excluded."
        except BaseException as error:
            report["passed"] = report["jdi_observed_unqualified"] = False
            report["retention_failure"] = f"{type(error).__name__}: {error}"
            exit_code = 1
        report["seconds"] = round(time.monotonic() - started, 3)
        with (work / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(report, stream, indent=2, allow_nan=False)
            stream.write("\n")
        print("Capture:", work, flush=True)
        print(json.dumps({key: report.get(key) for key in ("passed", "source_head", "source_dirty",
            "tested_commit", "native_comparison", "qualification", "semantic_audit_complete",
            "failure", "seconds")}), flush=True)
    need(report["passed"] is (exit_code == 0), "capture result/report agreement")
    return work, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("server-jar", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--tested-commit", default="")
    args = parser.parse_args()
    _, report = run_capture(args.server_jar, args.java, args.javac, args.work_dir, args.tested_commit)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
