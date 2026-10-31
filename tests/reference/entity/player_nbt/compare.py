"""Compare completed, bounded real-owner NBT runs; raw stdout alone is insufficient."""

import argparse
import importlib.util
import json
from pathlib import Path, PureWindowsPath
import re
from urllib.parse import unquote, urlsplit


HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("player_nbt_original_driver", HERE / "run_original.py")
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)
need, digest = original.need, original.digest


def capture_receipt(report, stdout, stderr, *, empty_stderr=True):
    need(type(report.get("exit")) is int and report["exit"] == 0 and report.get("reader_errors") == {} and
         report.get("pipe_capture_complete") is True and report.get("reaped") is True and
         report.get("timed_out") is False and report.get("output_overflow") is False,
         "unsuccessful/incomplete capture receipt")
    need((not empty_stderr or stderr == b"") and type(report.get("stderr_bytes")) is int and
         report["stderr_bytes"] == len(stderr) and type(report.get("stdout_bytes")) is int and
         report["stdout_bytes"] == len(stdout), "stream inventory/stderr differs")


def captured_receipt(root, products, phase, embedded):
    name = phase + ".capture.json"
    need(name in products and digest(root / name) == products[name] and
         json.loads((root / name).read_text()) == embedded,
         "embedded receipt differs from exact captured receipt: " + phase)


def required_products(products, names):
    need(set(names) <= set(products), "required captured stream/product hash is missing")


def original_input_path(recorded, relocations):
    if recorded in relocations:
        actual = Path(relocations[recorded])
    else:
        actual = Path(recorded)
        need(actual.is_file(), "original input is missing without an explicit mapping: " + recorded)
    need(actual.is_file(), "mapped original input is missing: " + recorded)
    return actual.resolve()


def windows_origin(value):
    if value.lower().startswith("file:"):
        uri = urlsplit(value)
        need(uri.scheme.lower() == "file" and uri.netloc.lower() in ("", "localhost"),
             "unsupported original origin URI host")
        need(not uri.query and not uri.fragment, "original origin URI has a query or fragment")
        value = unquote(uri.path)
        if re.match(r"^/[A-Za-z]:[/\\]", value):
            value = value[1:]
    path = PureWindowsPath(value)
    need(path.is_absolute(), "original origin is not an absolute recorded Windows path")
    return path


def original_origin_view(text, recorded_jar, actual_jar):
    """Derived validator input only; no raw file, command or game value changes."""
    expected = windows_origin(recorded_jar)
    lines = []
    origins = 0
    for line in text.splitlines():
        if line.startswith("ORIGIN "):
            fields = line.split(" ", 2)
            need(len(fields) == 3 and windows_origin(fields[2]) == expected,
                 "raw original origin differs from its recorded pinned JAR input")
            line = fields[0] + " " + fields[1] + " " + str(actual_jar)
            origins += 1
        lines.append(line)
    return "\n".join(lines) + "\n", origins


def checked_helpers(report, root, metadata=None):
    before, after = report["helper_before_sha256"], report["helper_after_sha256"]
    classes = root / "classes"
    need(set(before) == set(after) == original.HELPERS and before == after,
         "original helper inventory is missing, incomplete or changed")
    actual = {path.relative_to(classes).as_posix() for path in classes.rglob("*.class")}
    need(actual == original.HELPERS, "actual compiled helper inventory differs")
    code = report["helper_class_code"]
    need(set(code) == original.HELPERS, "compiled helper Code inventory differs")
    for name, value in after.items():
        helper = classes / name
        need(digest(helper) == value, "original helper product differs")
        need(metadata is not None and metadata.inspect_class(helper.read_bytes()) == code[name],
             "compiled helper class/Code metadata differs")


def checked_run(path, *, native, relocations=None, executable=None, metadata=None):
    path = Path(path).resolve()
    report = json.loads(path.read_text(encoding="utf-8"))
    need(report.get("status") == "PASS", "run did not pass")
    before, after = report["input_before_sha256"], report["input_after_sha256"]
    need(before and before == after, "run input inventory changed")
    for name, value in before.items():
        actual = Path(name)
        if not native:
            actual = original_input_path(name, {} if relocations is None else relocations)
        else:
            need(actual.is_file(), "native input is missing; native path mappings are forbidden")
        need(digest(actual) == value, "run input no longer matches: " + name)
    products = report["products"]
    need(products, "missing captured products")
    phases = ("native",) if native else ("java-version", "javac-version", "compile", "original", "prefix")
    required = [phase + suffix for phase in phases for suffix in (".stdout", ".stderr", ".capture.json")]
    if not native:
        required += ["sleep-target.stdout", "sleep-target.stderr"]
    required_products(products, required)
    for name, value in products.items():
        need(Path(name).name == name and digest(path.parent / name) == value,
             "captured product differs: " + name)
    if native:
        need(executable is not None, "configured native executable is required")
        executable = Path(executable).resolve()
        need(executable.is_file(), "configured native executable is missing")
        command = report["capture"].get("command")
        need(isinstance(command, list) and command and isinstance(command[0], str),
             "missing native executable command")
        invoked = Path(command[0]).resolve()
        need(invoked.is_file() and invoked.samefile(executable), "native command executable differs")
        bound = [name for name in before if Path(name).is_file() and Path(name).samefile(executable)]
        need(len(bound) == 1 and before[bound[0]] == after[bound[0]] == digest(executable),
             "native executable is not bound to before/after input hashes")
        need(report.get("kind") == "nbt" and type(report.get("checks")) is int and
             report["checks"] > 0, "wrong native kind/empty completion")
        stdout, stderr = (path.parent / "native.stdout").read_bytes(), (path.parent / "native.stderr").read_bytes()
        captured_receipt(path.parent, products, "native", report["capture"])
        capture_receipt(report["capture"], stdout, stderr)
        lines = stdout.decode("ascii", "strict").splitlines()
        need(lines[-2:] == [f"Player active tick: {report['checks']} checks",
                            f"Player NBT: {report['checks']} checks"], "native final completion differs")
    else:
        for phase in phases:
            captured_receipt(path.parent, products, phase, report[phase])
            capture_receipt(report[phase], (path.parent / (phase + ".stdout")).read_bytes(),
                            (path.parent / (phase + ".stderr")).read_bytes(),
                            empty_stderr=phase not in ("java-version", "javac-version"))
        checked_helpers(report, path.parent, metadata)
        # The pinned authored JDI helper owns/reaps this target and verifies
        # its two readers. Its bounded parent receipt and exact helper product
        # are checked above; target bytes are also in the products inventory.
        need((path.parent / "sleep-target.stderr").read_bytes() == b"", "JDI target stderr")
    return report


def living_getter_sites(prefixes):
    # Actual ls.a(Lnu;)V Short getters. The following putfield instructions
    # (7/32/42/52) have not executed when their corresponding getter throws.
    expected = {"health_wrong": 4, "hurt_wrong": 29, "death_wrong": 39, "attack_wrong": 49}
    selected = [row for row in prefixes if row[1] in expected]
    need([row[1] for row in selected] == list(expected), "Living getter failure inventory/order differs")
    for row in selected:
        need(row[2:8] == ["311", str(expected[row[1]]), "2", "2", "0", "0"],
             "Living getter failure source-frame differs: " + row[1])


def native_projection(text):
    lines = text.splitlines()
    endings = [line.split() for line in lines if line.startswith("END ")]
    need([row[1] for row in endings] == list(original.CASES) and all(len(row) == 3 for row in endings),
         "native case inventory/order differs")
    states = [line.split() for line in lines if line.startswith("STATE ")]
    need([row[1] for row in states] == list(original.CASES) + ["persisted_load", "sleep"] and
         all(len(row) == 35 for row in states), "native state inventory/width differs")
    need([row[2] for row in states[:33]] == [row[2] for row in endings], "native outcomes disagree")
    need(all(row[2] in ("OK", "CAST", "NULL", "INDEX") for row in endings),
         "unknown native error was normalized as a source exception")
    complete = sum(row[2] == "OK" for row in endings[2:])
    interrupted = len(endings[2:]) - complete
    markers = [line for line in lines if line.startswith("Native Player NBT observations:")]
    need(markers == [f"Native Player NBT observations: 33 cases, {complete} complete reads, "
                     f"{interrupted} interrupted reads"], "native read-count completion differs")
    prefixes = [line.split() for line in lines if line.startswith("PREFIX ")]
    need([row[1] for row in prefixes] == [row[1] for row in endings if row[2] != "OK"] and
         all(len(row) == 9 and all(token.isdecimal() for token in row[2:]) for row in prefixes),
         "native interrupted source-frame inventory differs")
    living_getter_sites(prefixes)
    randoms = [line.split() for line in lines if line.startswith("RANDOM ")]
    need([row[1] for row in randoms] == list(original.CASES) + ["sleep"] and
         all(len(row) == 4 and all(re.fullmatch(r"[0-9a-f]{12},[01],[0-9a-f]{16}", value)
         for value in row[2:]) for row in randoms), "native owner Random inventory differs")
    trees = original.validate_trees(lines)
    expected = {(name, "after") for name in original.CASES} | {
        ("write_fresh", "direct"), ("write_alias_narrow", "direct"), ("roundtrip", "input"),
        ("full_save", "player"), ("select_empty", "data"), ("select_null", "data"),
        ("select_first_only", "data"), ("persisted_load", "after"), ("sleep", "prefix")}
    need(set(trees) == expected, "native tree inventory differs")
    for key, nodes in trees.items():
        original.schema(nodes, info=key[1] == "data")
    need([line for line in lines if line.startswith("NAMED ")] ==
         ["NAMED write_fresh 0 1", "NAMED write_alias_narrow 0 1"], "native named-save decision differs")
    need([line for line in lines if line.startswith("CLOCK ")] ==
         ["CLOCK select_empty 1", "CLOCK select_null 1", "CLOCK select_first_only 1"],
         "native clock bracket inventory differs")
    need(lines.count("PERSISTED_LOAD 1") == 1 and lines.count("SELECTION 3 1") == 1,
         "native actual persisted-load/selection completion missing")
    return lines, trees, complete, interrupted


def compare_projection(native, normal, prefix):
    nlines, ntrees, complete, interrupted = native_projection(native)
    olines, plines = normal.splitlines(), prefix.splitlines()
    otrees, ptrees = original.validate_trees(olines), original.validate_trees(plines)
    normal_trees = {(name, stage): tree for (name, stage), tree in ntrees.items()
                    if name not in ("persisted_load", "sleep")}
    compared_nodes = 0
    for key, nodes in normal_trees.items():
        need(key in otrees and set(nodes) == set(otrees[key]), "original/native tree shape differs: " + str(key))
        for path, value in nodes.items():
            # Both actual writers separately bracket their real clock. Only
            # these three specified fields are time-variable observations.
            if key[1] == "data" and path == "$/LastPlayed":
                need(value[0] == otrees[key][path][0] == 4, "LastPlayed kind differs")
                continue
            need(value == otrees[key][path], "original/native tag differs: " + str(key) + path)
            compared_nodes += 1
    for name in original.CASES:
        for kind in ("STATE", "RANDOM", "END"):
            n = [row for row in nlines if row.startswith(kind + " " + name + " ")]
            o = [row for row in olines if row.startswith(kind + " " + name + " ")]
            need(len(n) == len(o) == 1 and n == o, "original/native state/outcome/Random differs: " + name)
    for kind in ("NAMED ", "CLOCK ", "SAVE ", "SELECTION "):
        need([row for row in nlines if row.startswith(kind)] ==
             [row for row in olines if row.startswith(kind)], "original/native composition differs: " + kind)
    # The native actual persisted-file load uses the very same saved input as
    # the original direct round trip. This proves that input identity first;
    # it does not claim an additional original World-reopen execution.
    need(ntrees[("full_save", "player")] == otrees[("roundtrip", "input")],
         "persisted native load input differs from actual original round-trip input")
    need(ntrees[("persisted_load", "after")] == otrees[("roundtrip", "after")],
         "persisted native load result differs from same-input original read")
    ns = next(row.split()[2:] for row in nlines if row.startswith("STATE persisted_load "))
    os = next(row.split()[2:] for row in olines if row.startswith("STATE roundtrip "))
    need(ns == os, "persisted native read scalar/alias result differs")
    for kind in ("STATE sleep ", "RANDOM sleep ", "SITE sleep ", "ATTACH_PREFIX sleep "):
        need([row for row in nlines if row.startswith(kind)] ==
             [row for row in plines if row.startswith(kind)] and
             sum(row.startswith(kind) for row in plines) == 1, "wake93 checkpoint differs: " + kind)
    need(ntrees[("sleep", "prefix")] == ptrees[("sleep", "prefix")], "wake93 direct writer tree differs")
    return {"cases": 33, "complete_reads": complete, "interrupted_reads": interrupted,
            "complete_normal_trees": len(normal_trees), "exact_normal_tag_nodes": compared_nodes,
            "separately_bracketed_LastPlayed_values": 3,
            "persisted_native_load_same_original_roundtrip_input": True,
            "wake93_prefix_tag_nodes": len(ptrees[("sleep", "prefix")]),
            "wake_full_call_parity": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-run", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True,
                        help="the actual configured native observer executable")
    parser.add_argument("--original-run", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[3],
                        help="trusted project source containing its classfile metadata verifier")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--relocations", type=Path,
                        help="optional historical-input to current-file map; exact hashes remain required")
    args = parser.parse_args(argv)
    need(not args.output.exists(), "comparison output must be fresh")
    relocations = {} if args.relocations is None else json.loads(args.relocations.read_text())
    metadata_path = args.source_root.resolve() / "tests/reference/client/notice/classfile.py"
    metadata = original.module(metadata_path, "player_nbt_comparison_classfile")
    inputs = (args.native_run.resolve(), args.original_run.resolve(), args.executable.resolve(), metadata_path, Path(__file__).resolve(),
              HERE / "run_original.py", HERE / "source-pins.json") + (() if args.relocations is None else
              (args.relocations.resolve(), *(Path(value).resolve() for value in relocations.values())))
    before = {str(p): digest(p) for p in inputs}
    native_report = checked_run(args.native_run, native=True, executable=args.executable)
    original_report = checked_run(args.original_run, native=False, relocations=relocations, metadata=metadata)
    need(set(relocations) <= set(original_report["input_before_sha256"]),
         "mapping contains inputs outside the original evidence inventory")
    need(any(original_input_path(name, relocations).samefile(metadata_path) and value == digest(metadata_path)
             for name, value in original_report["input_before_sha256"].items()),
         "class/Code verifier is not bound to original run inputs")
    pins = json.loads((HERE / "source-pins.json").read_text())
    recorded_jar = next(path for path, value in original_report["input_before_sha256"].items()
                        if value == original.CLIENT)
    jar = original_input_path(recorded_jar, relocations)
    nroot, oroot = args.native_run.resolve().parent, args.original_run.resolve().parent
    normal = (oroot / "original.stdout").read_text(encoding="utf-8")
    prefix = (oroot / "prefix.stdout").read_text(encoding="utf-8")
    normal_view, normal_origins = original_origin_view(normal, recorded_jar, jar)
    target_view, target_origins = original_origin_view(
        (oroot / "sleep-target.stdout").read_text(encoding="utf-8"), recorded_jar, jar)
    original.validate_normal(normal_view, pins, jar)
    original.validate_prefix(prefix, target_view, pins, jar)
    result = compare_projection((nroot / "native.stdout").read_text(encoding="ascii"), normal, prefix)
    after = {str(p): digest(p) for p in inputs}
    need(before == after, "comparison inputs changed")
    result.update(status="PASS", scope="declared actual NBT operations plus distinct wake93 prefix",
                  original_filesystem_projection={"recorded_jar": recorded_jar, "current_jar": str(jar),
                      "normal_origins": normal_origins, "target_origins": target_origins,
                      "raw_capture_rewritten": False, "gameplay_or_state_projection": False},
                  native_checks=native_report["checks"], input_before_sha256=before, input_after_sha256=after)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Player NBT comparison: {result['cases']} cases, {result['exact_normal_tag_nodes']} exact tags; "
          f"report {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
