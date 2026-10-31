"""Real stored-Chunk World paths and optional unchanged-original graph comparison."""
import argparse
import copy
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "tests/reference/world/path"
sys.path.insert(0, str(ROOT / "tests/tools/entity"))
from test_mob_tick import authored_world, original_inputs
from test_mob_construction import RESOURCES


def need(value, message):
    if not value:
        raise AssertionError(message)


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1048576), b""):
            result.update(block)
    return result.hexdigest()


def read_profiles():
    with (REFERENCE / "profiles.tsv").open(encoding="ascii", newline="") as source:
        rows = csv.DictReader(source, delimiter="\t")
        need(rows.fieldnames == "case operation scene sx sy sz tx ty tz range_bits width_bits height_bits".split(),
             "complete authored path caller schema")
        result = list(rows)
    need([int(row["case"]) for row in result] == list(range(len(result))), "ordered path profile identities")
    return result


def reference(value):
    if isinstance(value, dict) and "class" in value and ("identity" in value or "oid" in value):
        keys = [key for key in ("identity", "oid") if key in value]
        need(len(keys) == 1 and isinstance(value["class"], str) and
             type(value[keys[0]]) in (int, str), "actual process-local typed reference")
        return value["class"], value[keys[0]]
    return None


class GraphComparison:
    """An injective identity pairing inside actual process-local namespaces.

    One instance spans the paired before/steps/after states, so a fresh object
    with the same class and value cannot replace an existing source owner.
    """
    def __init__(self):
        self.forward = {}
        self.reverse = {}
        self.checks = 0

    def value(self, actual, expected, label):
        left, right = reference(actual), reference(expected)
        if left is not None or right is not None:
            need(left is not None and right is not None and left[0] == right[0],
                 "actual reference class differs: " + label)
            need(left not in self.forward or self.forward[left] == right,
                 "actual reference identity continuity differs: " + label)
            need(right not in self.reverse or self.reverse[right] == left,
                 "actual reference alias partition differs: " + label)
            self.forward[left] = right
            self.reverse[right] = left
            self.checks += 3
            return
        need(type(actual) is type(expected), "actual value type differs: " + label)
        if isinstance(expected, dict):
            need(set(actual) == set(expected), "complete actual field inventory differs: " + label)
            for key in sorted(expected):
                self.value(actual[key], expected[key], label + "/" + key)
        elif isinstance(expected, list):
            need(len(actual) == len(expected), "actual array extent differs: " + label)
            for index, (a, b) in enumerate(zip(actual, expected)):
                self.value(a, b, label + "/" + str(index))
        else:
            need(actual == expected, f"actual value differs: {label}: {actual!r} != {expected!r}")
            self.checks += 1

    def graph(self, actual, expected, label):
        need(set(actual) == set(expected) == {"roots", "objects"}, "complete retained graph transport")
        aobjects = {reference(row): row for row in actual["objects"]}
        bobjects = {reference(row): row for row in expected["objects"]}
        need(None not in aobjects and None not in bobjects and len(aobjects) == len(actual["objects"]) and
             len(bobjects) == len(expected["objects"]), "unique actual graph object identities")
        self.value(actual["roots"], expected["roots"], label + "/roots")
        visited = set()
        while True:
            pending = [(left, right) for left, right in self.forward.items()
                       if (left, right) not in visited and (left in aobjects or right in bobjects)]
            if not pending:
                break
            for left, right in pending:
                visited.add((left, right))
                need(left in aobjects and right in bobjects, "expanded/opaque object scope differs: " + label)
                a = {key: value for key, value in aobjects[left].items() if key not in ("identity", "oid")}
                b = {key: value for key, value in bobjects[right].items() if key not in ("identity", "oid")}
                self.value(a, b, label + "/" + left[0] + "/" + str(len(visited)))
        return len(visited)


SEARCH_FIELDS = frozenset(("sn.aM", "sn.aN", "sn.aO", "sn.bg", "sn.bh", "sn.aI", "sn.aW", "sn.bs", "sn.bD"))


def compare_state(pair, actual, expected, label, ordinary_fd=False):
    for owner in ("actor_fields", "target_fields"):
        source = expected[owner]
        selected = None if source is None else {key: source[key] for key in SEARCH_FIELDS}
        pair.value(actual[owner], selected, label + "/" + owner)
    for key in ("actor_bounds", "target_bounds", "own_seed", "target_seed", "world_seed",
                "box_cursor", "vector_cursor", "loaded_entities", "cache_size"):
        pair.value(actual[key], expected[key], label + "/" + key)
    graph = actual["graph"]
    if ordinary_fd:
        # The ordinary original wrapper observer genuinely has no retained FW/EW
        # locals. Its result graph is a distinct observation from the read-only
        # JDI graph below; the complete native internals are paired separately.
        graph = dict(graph, roots=dict(graph["roots"], cache=None, finder=None))
    pair.graph(graph, expected["graph"], label + "/graph")


def native_rows(path, profiles):
    states = {index: {} for index in range(len(profiles))}
    results, steps, queries, controls = {}, {index: [] for index in states}, {}, []
    for line in path.read_text(encoding="ascii").splitlines():
        row = json.loads(line)
        index = row["case"]
        if row["event"] == "path_state":
            need(index in states and row["phase"] not in states[index], "unique actual path phase")
            states[index][row["phase"]] = row
        elif row["event"] == "path_result":
            need(index not in results, "unique actual path outcome")
            results[index] = row
        elif row["event"] == "path_step":
            steps[index].append(row)
        elif row["event"] == "path_queries":
            need(index not in queries, "unique ordered query receipts")
            queries[index] = row
        else:
            need(row["event"] == "path_control", "declared native path observations only")
            controls.append(row)
    need(set(results) == set(states), "all real path calls have actual outcomes")
    failures = {13: "NullPointerException", 14: "NullPointerException", 15: "NullPointerException",
                16: "NullPointerException", 45: "ArrayIndexOutOfBoundsException",
                56: "ArrayIndexOutOfBoundsException", 57: "NullPointerException", 58: "NullPointerException",
                59: "NullPointerException", 60: "IllegalStateException", 63: "NegativeArraySizeException"}
    need(set(queries) == set(states), "every caller explicitly records its actual query attempts, including zero")
    for index, profile in enumerate(profiles):
        need(set(states[index]) == {"before", "after"}, "complete native before/after state")
        before, after, result = states[index]["before"], states[index]["after"], results[index]
        need(all(row["operation"] == profile["operation"] and row["scene"] == profile["scene"]
                 for row in (before, after)), "matching actual caller operands")
        for key in ("actor_fields", "actor_bounds", "target_fields", "target_bounds", "own_seed",
                    "target_seed", "world_seed", "math_seed", "box_cursor", "loaded_entities"):
            need(before[key] == after[key], "native search changes no source Entity/stream/bounds field: " + key)
        need(after["vector_cursor"] == (2 if index == 55 else 0), "only actual DH waypoint calls advance BT pool")
        need(result["outcome"] == (-1 if index in failures else 1) and not result["native_policy"],
             f"complete source return or declared source failure required: case{index}")
        if index in failures:
            need(result["source_exception"] and result["exception_class"] == "java/lang/" + failures[index],
                 "actual reached source exception classification")
        need(all((row["site"]["class"] == "fw" and row["site"]["pc"] in (57, 101)) or
                 (profile["operation"].startswith("EW_") and row["site"]["class"] == "ew" and row["site"]["pc"] == 0)
                 for row in queries[index]["queries"]), "actual entered EW or FW invocation query sites")
    need(len(controls) == 9 and {row["scene"] for row in controls} ==
         {"limit_" + name for name in ("cache_cells", "bytes", "allocations", "points", "expanded",
                                       "cell_queries", "predecessors", "work")} | {"shared_path_lifetime"},
         "all eight independent ceilings and actual retained Path ownership controls")
    return states, results, steps, queries, controls


def original_records(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    need(isinstance(data.get("path_records"), list) and isinstance(data.get("finder_records"), list),
         "complete ordinary and actual JDI finder graph transport required")
    records = data["path_records"]
    need([row["case"] for row in records] == list(range(len(records))), "ordered unchanged-original cases")
    return data, records


def source_queries(data, index):
    entries = [row for row in data["source_records"] if row["case"] == index and row["event"] == "entry" and
               row["location"]["class"] == "ew" and row["location"]["method"] in ("a", "e") and
               row["location"]["descriptor"] == "(III)I"]
    returns = [row for row in data["query_records"] if row["case"] == index]
    result = []
    returned_index = 0
    for ordinal, entry in enumerate(entries):
        end = entries[ordinal + 1]["sequence"] if ordinal + 1 < len(entries) else float("inf")
        completed = []
        if returned_index < len(returns) and returns[returned_index]["sequence"] < end:
            need(returns[returned_index]["sequence"] > entry["sequence"], "actual EW return follows its entry")
            completed = [returns[returned_index]]
            returned_index += 1
        metadata = entry["location"]["method"] == "e"
        callers = [frame for frame in (completed[0]["source_frames"] if completed else
                   next(row["source_frames"] for row in data["exception_records"] if row["case"] == index))
                   if frame["class"] == "fw" and frame["descriptor"] == "(Lsn;IIILd;)I" and
                   frame["pc"] in (57, 101)]
        site = callers[0] if callers else dict(entry["location"], pc=0)
        row = dict(metadata=metadata, x=entry["arguments"][0], y=entry["arguments"][1],
                   z=entry["arguments"][2], completed=bool(completed), site=site)
        if completed:
            need(completed[0]["receiver"] == entry["receiver"] and
                 completed[0]["arguments"] == entry["arguments"], "same actual EW receiver/arguments at entry and return")
            row["value"] = completed[0]["result"]
        result.append(row)
    need(returned_index == len(returns), "every actual EW return has its recorded source entry")
    # A null XP receiver fails at the real FW invokevirtual before any EW
    # callee entry exists. Pair its evaluated arguments to that actual FW entry
    # and source exception, rather than inventing an EW return or invocation.
    if index == 59:
        failures = [row for row in data["exception_records"] if row["case"] == index]
        need(not entries and len(failures) == 1 and failures[0]["location"]["pc"] == 57 and
             failures[0]["location"]["class"] == "fw", "actual null XP invocation source failure")
        volume = [row for row in data["source_records"] if row["case"] == index and row["event"] == "entry" and
                  row["location"]["class"] == "fw" and row["location"]["descriptor"] == "(Lsn;IIILd;)I"]
        need(len(volume) == 1, "actual FW volume arguments before the null receiver")
        args = volume[0]["arguments"]
        result.append(dict(metadata=False, completed=False, x=args[1], y=args[2], z=args[3],
                           site=failures[0]["location"]))
    return result


def compare_queries(actual, expected, label):
    need(len(actual) == len(expected), "complete actual query sequence differs: " + label)
    checks = 0
    for index, (a, b) in enumerate(zip(actual, expected)):
        # A failed invocation has no return value. Its preserved native output
        # storage is not treated as a Java result or successful air query.
        selected = {key: a[key] for key in b}
        need(selected == b, f"actual ordered query arguments/result/site differ: {label}/{index}")
        checks += len(b)
    return checks


def compare_extent(data, index, actual):
    searches = [row for row in data["source_records"] if row["case"] == index and row["event"] == "entry" and
                row["location"]["class"] == "fw" and
                row["location"]["descriptor"] == "(Lsn;Ld;Ld;Ld;F)Ldh;"]
    if not searches:
        need(actual is None, "no fabricated FW extent before the source search entry")
        return 0
    need(len(searches) == 1 and actual is not None, "one actual prepared search extent")
    receiver = searches[0]["arguments"][3]
    constructors = [row for row in data["source_records"] if row["case"] == index and row["event"] == "return" and
                    row["location"]["class"] == "d" and row["location"]["method"] == "<init>" and
                    row["location"]["descriptor"] == "(III)V" and row["receiver"] == receiver]
    need(len(constructors) == 1 and constructors[0]["location"]["pc"] == 39,
         "actual D constructor-return fields selected by the live search extent identity")
    pair = GraphComparison()
    pair.value({key: actual[key] for key in ("class", "identity")}, receiver, f"extent-identity{index}")
    pair.value(actual["fields"], constructors[0]["fields"], f"extent-fields{index}")
    return pair.checks


def compare_pending_constructor(data, index, actual):
    if index != 58:
        need(actual is None, "no invented pending DH outside the reached failed constructor")
        return 0
    failures = [row for row in data["exception_records"] if row["case"] == index]
    need(len(failures) == 1 and failures[0]["location"] ==
         {"class": "dh", "method": "<init>", "descriptor": "([Ld;)V", "pc": 11},
         "actual failed DH constructor array-length operation")
    graph = failures[0]["state"]["graph"]
    receiver = graph["roots"]["pending"]
    nodes = [row for row in graph["objects"] if reference(row) == reference(receiver)]
    need(actual is not None and len(nodes) == 1 and receiver["class"] == "dh",
         "same actual pending DH receiver, not a returned or fabricated Path")
    pair = GraphComparison()
    pair.value({key: actual[key] for key in ("class", "identity")}, receiver, "pending-DH-identity")
    pair.value(actual["fields"], nodes[0]["fields"], "pending-DH-complete-source-fields")
    return pair.checks


def compare_original(data, records, profiles, states, results, steps, queries):
    need(len(records) == len(profiles), "same complete path matrix")
    checks, objects, internals, query_checks, query_count, extent_checks, pending_checks = 0, 0, 0, 0, 0, 0, 0
    for index, row in enumerate(records):
        need(row["operation"] == profiles[index]["operation"] and row["scene"] == profiles[index]["scene"] and
             not row["default_constructor_entropy_rebound"], "original authored caller identity")
        pair = GraphComparison()
        ordinary_fd = row["operation"].startswith("FD_")
        compare_state(pair, states[index]["before"], row["before"], f"before{index}")
        need(len(steps[index]) == len(row["steps"]), f"all actual component operations: {index}")
        for actual, expected in zip(steps[index], row["steps"]):
            need(actual["step"] == expected["step"], "actual component invocation order")
            if "graph" in expected:
                objects += pair.graph(actual["graph"], expected["graph"], f"step{index}/{expected['step']}")
                pair.value(actual["box_cursor"], expected["box_cursor"], "step bounds cursor")
                pair.value(actual["vector_cursor"], expected["vector_cursor"], "step vector cursor")
            else:
                pair.value(actual.get("values", actual.get("value")), expected.get("values", expected.get("value")),
                           "actual component scalar/reference result")
        compare_state(pair, states[index]["after"], row["after"], f"after{index}", ordinary_fd)
        need(results[index]["outcome"] == row["outcome"], "actual source outcome without normalization")
        if row["outcome"] < 0:
            need(results[index]["source_exception"] and
                 results[index]["exception_class"].replace("/", ".") == row["failure"],
                 "actual source Throwable class at reached failure")
            exceptions = [value for value in data["exception_records"] if value["case"] == index]
            need(len(exceptions) == 1 and exceptions[0]["location"] in
                 (results[index]["site"], results[index]["child"]),
                 "actual failed producer class/method/descriptor/BCI, including source array-load order")
        need(states[index]["before"]["math_seed"] == states[index]["after"]["math_seed"] and
             row["before"]["math_seed"] == row["after"]["math_seed"],
             "independent constructor Math streams unchanged, never equated or rebound")
        checks += pair.checks
        expected_queries = source_queries(data, index)
        query_checks += compare_queries(queries[index]["queries"], expected_queries, f"source{index}")
        query_count += len(expected_queries)
        extent_checks += compare_extent(data, index, queries[index]["extent"])
        pending_checks += compare_pending_constructor(data, index, queries[index]["pending_path"])
        if ordinary_fd and (states[index]["after"]["graph"]["roots"]["cache"] is not None or
                            states[index]["after"]["graph"]["roots"]["finder"] is not None):
            candidates = [value for value in data["finder_records"] if value["case"] == index]
            need(candidates, f"actual JDI retained FD internals required: {index}")
            observed = candidates[-1]["state"]["graph"]
            native_graph = states[index]["after"]["graph"]
            if index == 63:
                exceptions = [value for value in data["exception_records"] if value["case"] == index]
                need(len(exceptions) == 1 and exceptions[0]["location"]["class"] == "ew" and
                     exceptions[0]["location"]["pc"] == 55, "actual partial EW constructor exception")
                observed = exceptions[0]["state"]["graph"]
                native_graph = dict(native_graph, roots=dict(native_graph["roots"], cache=None, finder=None,
                                                             pending=native_graph["roots"]["cache"]))
            internal = GraphComparison()
            objects += internal.graph(native_graph, observed, f"retained-FD{index}")
            checks += internal.checks
            internals += 1
    return {"profiles": len(records), "typed_graph_and_state_checks": checks,
            "expanded_objects": objects, "separate_actual_FD_internal_graphs": internals,
            "ordered_query_attempts": query_count, "ordered_query_checks": query_checks,
            "actual_extent_constructor_checks": extent_checks,
            "actual_pending_Path_constructor_checks": pending_checks,
            "scope": "Every reached path/point/predecessor, concrete LF/JX/WM field and backing-array slot retains its actual class and process-local identity. Identity pairing is injective and spans before/steps/after states; source-generated IDs and native pointers are never equated. Ordinary FD result observations and separately read-only JDI FW/EW graphs are compared as distinct views. Source constructor contexts remain raw independent evidence. No failed native child is normalized to returned null."}


def comparison_controls(states, steps, queries):
    rejected = []
    def reject(name, operation):
        try:
            operation()
        except AssertionError:
            rejected.append(name)
        else:
            raise AssertionError("Path comparator accepted corruption: " + name)
    def objects(graph):
        return {reference(row): row for row in graph["objects"]}
    def replace_identity(value, old, new):
        if reference(value) == old:
            value["identity"] = new
        if isinstance(value, dict):
            for child in value.values():
                replace_identity(child, old, new)
        elif isinstance(value, list):
            for child in value:
                replace_identity(child, old, new)

    graph = states[1]["after"]["graph"]
    GraphComparison().graph(graph, graph, "negative-control baseline")
    corrupt = copy.deepcopy(graph)
    node = next(row for row in corrupt["objects"] if row["class"] == "d")
    node["class"] = "dh"
    reject("wrong_point_class", lambda: GraphComparison().graph(corrupt, graph, "wrong-class"))
    corrupt = copy.deepcopy(graph)
    nodes = objects(corrupt)
    path = nodes[reference(corrupt["roots"]["result"])]
    array = nodes[reference(path["fields"]["dh.b"])]
    second = nodes[reference(array["values"][1])]
    second["fields"]["d.h"] = dict(array["values"][1])
    reject("wrong_predecessor_alias", lambda: GraphComparison().graph(corrupt, graph, "wrong-predecessor"))

    shared = states[55]["after"]["graph"]
    corrupt = copy.deepcopy(shared)
    nodes = objects(corrupt)
    holder = nodes[reference(corrupt["roots"]["extra"])]
    alias = holder["values"][2]
    duplicate = copy.deepcopy(nodes[reference(alias)])
    duplicate["identity"] = "negative-only-Path-clone"
    alias["identity"] = duplicate["identity"]
    corrupt["objects"].append(duplicate)
    reject("split_shared_Path_alias", lambda: GraphComparison().graph(corrupt, shared, "split-Path"))

    initial = next(row["graph"] for row in steps[55] if row["step"] == "initial")
    advanced = next(row["graph"] for row in steps[55] if row["step"] == "advanced0")
    corrupt = copy.deepcopy(advanced)
    old = next(reference(row) for row in corrupt["objects"] if row["class"] == "d")
    replace_identity(corrupt, old, "negative-only-same-value-Point")
    GraphComparison().graph(corrupt, advanced, "same-value single-state baseline")
    temporal = GraphComparison()
    temporal.graph(initial, initial, "actual initial identity context")
    reject("replaced_same_value_Point_between_steps", lambda: temporal.graph(corrupt, advanced, "replaced-Point"))

    original = queries[1]["queries"]
    need(len(original) >= 2 and original[0] != original[1], "ordered-query rejection control operands")
    swapped = copy.deepcopy(original)
    swapped[0], swapped[1] = swapped[1], swapped[0]
    reject("reordered_query", lambda: compare_queries(swapped, original, "reordered"))
    reject("omitted_query", lambda: compare_queries(original[1:], original, "omitted"))
    corrupt = copy.deepcopy(states[29]["after"]["graph"])
    corrupt["roots"]["result"] = 0
    reject("water_classified_as_air", lambda: GraphComparison().graph(
        corrupt, states[29]["after"]["graph"], "wrong-water-classification"))
    return {"count": len(rejected), "rejected": rejected,
            "scope": "Copied observation corruptions exercise comparison rejection only. Same-value per-state graphs pass before the temporal replacement is rejected. None execute or replace a production input/result."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--observations", type=Path)
    args = parser.parse_args()
    profiles = read_profiles()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="path-", dir=args.work_dir.resolve()))
    resources = out / "resources"
    for name, data in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    worlds = out / "worlds"
    for index in range(len(profiles) + 9):
        authored_world(worlds / f"case{index}")
    executable = args.executable.resolve(strict=True)
    paths = {executable, Path(__file__).resolve(), ROOT / "tests/entity/test_mob_tick.c",
             ROOT / "tests/tools/entity/test_mob_tick.py", ROOT / "tests/tools/entity/test_mob_construction.py",
             ROOT / "tests/world/runtime/test_path.c", ROOT / "tests/world/runtime/test_world_random_tick.c",
             *resources.rglob("*.*"), *worlds.rglob("*.mcr"),
             *(path for path in REFERENCE.iterdir() if path.is_file())}
    for directory in (ROOT / "include", ROOT / "src", ROOT / "tests/support"):
        paths.update(path for path in directory.rglob("*") if path.suffix in (".c", ".h"))
    if args.observations:
        observation = args.observations.resolve(strict=True)
        paths.update([observation, *original_inputs(observation)])
    paths = sorted(path.resolve(strict=True) for path in paths)
    before = {str(path): digest(path) for path in paths}
    report = {"status": "FAIL", "input_before_sha256": before, "original_comparison": None}
    try:
        command = [str(executable), str(resources), str(worlds), str(REFERENCE / "profiles.tsv"),
                   str(REFERENCE / "scene-preparation.tsv"), str(out / "native.ndjson")]
        report["command"] = command
        result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=300)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        lines = result.stdout.decode("ascii").splitlines()
        footer = re.fullmatch(r"World paths: ([1-9][0-9]*) checks;([1-9][0-9]*) profiles", lines[-1]) if lines else None
        need(result.returncode == 0 and not result.stderr and footer and int(footer.group(2)) == len(profiles) and
             all(line in ("151 recipes", "16 achievements") for line in lines[:-1]),
             f"genuine path executable failed ({result.returncode}); inspect {out}")
        states, results, steps, queries, controls = native_rows(out / "native.ndjson", profiles)
        report["comparator_negative_controls"] = comparison_controls(states, steps, queries)
        if args.observations:
            data, original = original_records(observation)
            report["original_comparison"] = compare_original(data, original, profiles, states, results, steps, queries)
        report.update(status="PASS", checks=int(footer.group(1)), profiles=len(profiles),
                      native_controls=controls, ordered_native_query_count=sum(len(row["queries"]) for row in queries.values()),
                      scope="Actual GAME World/Registry/Entity/Math and25stored Stone-plane Chunks, genuine ordinary Cow publication, retained FD calls and the real EW/FW/LF/JX/D/DH methods. Constructor entropy is independent; declared own/World setSeed caller inputs are applied before observation. No fake BlockAccess or successful missing-path substitute.")
        print(f"World paths: {report['checks']} checks; {out / 'run.json'}")
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in paths}
        unchanged = report["input_after_sha256"] == before
        if not unchanged:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        need(unchanged, "actual path executable/source/helper/fixture inputs changed")


if __name__ == "__main__":
    main()
