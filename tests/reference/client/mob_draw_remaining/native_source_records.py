"""Read already-audited v2 rows; select observed entries, never generate answers."""

import collections
import json
from pathlib import Path


def need(value, label):
    if not value:
        raise AssertionError(label)


def rows(path):
    with Path(path).open(encoding="utf-8") as stream:
        for ordinal, line in enumerate(stream):
            need(line.strip(), "source/nonempty-ndjson-line:" + str(ordinal))
            yield json.loads(line)


def class_name(name):
    """Exact JDI `type[][]` to reflection descriptor, with component/dimensions."""
    if not name.endswith("[]"):
        return name
    dimensions = 0
    while name.endswith("[]"):
        dimensions += 1
        name = name[:-2]
    primitive = {"boolean": "Z", "byte": "B", "char": "C", "short": "S",
                 "int": "I", "long": "J", "float": "F", "double": "D"}
    return "[" * dimensions + (primitive[name] if name in primitive else "L" + name + ";")


def key(ref):
    need(isinstance(ref, dict) and set(ref) >= {"class", "oid"}, "source/actual-jdi-reference")
    return class_name(ref["class"]), ref["oid"]


def bridge(event):
    result = {}
    reverse = set()
    for row in event["identity_bridge"]:
        need(set(row) == {"identity", "actual"}, "source/complete-bridge-row")
        ident = row["identity"]
        actual = key(row["actual"])
        need(type(ident) is int and ident > 0 and ident not in result and actual not in reverse,
             "source/injective-actual-bridge")
        result[ident] = actual
        reverse.add(actual)
    return result


def signature(row):
    site = row["location"]
    return site["class"], site["method"], site["descriptor"]


def load(base):
    base = Path(base)
    normal_all = list(rows(base / "capture-v2/normal/target.ndjson"))
    debug_all = list(rows(base / "capture-v2/debugger/target.ndjson"))
    wanted = {"resource_input": 8, "device": 1, "constructor_before": 8,
              "constructor": 8, "texture_lookup": 7, "draw": 14, "network_guard": 1}
    for mode, data in (("normal", normal_all), ("debugger", debug_all)):
        need(collections.Counter(r["event"] for r in data) == wanted, "source/all-target-record-groups:" + mode)
        need([r["index"] for r in data if r["event"] == "draw"] == list(range(14)), "source/fourteen-ordered-draws")
        need([r["index"] for r in data if r["event"] == "texture_lookup"] == list(range(7)), "source/seven-ordered-textures")
    normal = [r for r in normal_all if r["event"] == "draw"]
    debugger = [r for r in debug_all if r["event"] == "draw"]
    # Defined draw state is independently comparable between original VMs;
    # raw actor/camera fields and all default entropy remain unmodified/source-only.
    for a, b in zip(normal, debugger):
        for name in ("index", "kind", "input", "declared_getters", "outcome", "failure", "nw_position", "nw_words", "gl_error"):
            need(a[name] == b[name], "source/normal-debugger-defined:" + name)
        for phase in ("before", "after"):
            for name in ("graph", "parts", "pool", "pool_storage", "gl"):
                need(a[phase][name] == b[phase][name], "source/normal-debugger-defined:" + phase + "/" + name)
        need(a["before"]["entropy"] == a["after"]["entropy"] and
             b["before"]["entropy"] == b["after"]["entropy"], "source/within-vm-no-active-rng-progress")
    result = {"external_base": str(base.resolve()), "draw_records": normal,
              "texture_records": [r for r in normal_all if r["event"] == "texture_lookup"],
              "device": next(r for r in normal_all if r["event"] == "device"),
              "constructor_contexts": {"normal": [r for r in normal_all if r["event"].startswith("constructor")],
                                        "debugger": [r for r in debug_all if r["event"].startswith("constructor")]},
              "events": [], "case_records": [{"case": i} for i in range(14)],
              "list_records": [], "code_pins": [], "raw_event_counts": collections.Counter()}
    stack, compiling, sequence = [], None, -1
    for event in rows(base / "capture-v2/debugger/checkpoints.ndjson"):
        need(type(event["sequence"]) is int and event["sequence"] > sequence, "source/actual-total-journal-sequence")
        sequence = event["sequence"]
        result["raw_event_counts"][event["event"]] += 1
        if event["event"] == "code-pin":
            result["code_pins"].append(event)
        case = event.get("case", -1)
        if case < 0:
            continue
        need(case in range(14), "source/declared-active-case")
        result["events"].append(event)
        if event["event"] in ("draw-entry", "draw-return", "state-complete"):
            name = {"draw-entry": "entry_bridge", "draw-return": "return_bridge", "state-complete": "after_bridge"}[event["event"]]
            need(name not in result["case_records"][case], "source/one-actual-case-boundary")
            result["case_records"][case][name] = bridge(event)
        if event["event"] == "entry":
            stack.append(event)
            if signature(event) == ("ps", "d", "(F)V"):
                need(compiling is None, "source/no-nested-PS-compiler")
                compiling = {"case": case, "part": event["receiver"], "batches": []}
            if signature(event) == ("nw", "a", "()V") and compiling is not None:
                need(any(f["class"] == "ps" and f["method"] == "d" for f in event["source_frames"]),
                     "source/actual-PS-batch-source-stack")
                compiling["batches"].append({"batch": event["batch"], "entry_sequence": event["sequence"]})
        elif event["event"] == "gl-entry" and event["method"] == "glDrawArrays" and compiling is not None:
            need(compiling["batches"] and "draw" not in compiling["batches"][-1], "source/one-observed-draw-per-face")
            compiling["batches"][-1]["draw"] = {"arguments": event["arguments"], "sequence": event["sequence"]}
        elif event["event"] == "return":
            need(stack and signature(stack[-1]) == signature(event) and stack[-1]["receiver"] == event["receiver"],
                 "source/actual-entry-return-receiver")
            stack.pop()
            if signature(event) == ("ps", "d", "(F)V"):
                need(compiling is not None and compiling["part"] == event["receiver"] and
                     len(compiling["batches"]) == 6 and all("draw" in r for r in compiling["batches"]),
                     "source/complete-real-compiled-list")
                compiling["list"] = event["receiver_fields"]["ps.o"]
                compiling["returned"] = event
                result["list_records"].append(compiling)
                compiling = None
        need(event["event"] != "exception", "source/baseline-has-no-Throwable")
    need(not stack and compiling is None, "source/complete-baseline-nesting")
    need(len(result["code_pins"]) == 213 and len(result["list_records"]) == 25, "source/213-code-and-25-actual-lists")
    for case in result["case_records"]:
        need(set(case) == {"case", "entry_bridge", "return_bridge", "after_bridge"}, "source/all-three-observed-bridges")
        previous = {}
        for phase in ("entry_bridge", "return_bridge", "after_bridge"):
            for ident, actual in case[phase].items():
                need(ident not in previous or previous[ident] == actual, "source/temporal-bridge-continuity")
                previous[ident] = actual
    result["analysis"] = json.loads((base / "analysis-v2.json").read_text())
    need(result["analysis"]["status"] == "ORIGINAL_FOURTEEN_VARIANTS_VALIDATED" and
         result["analysis"]["live_code_count"] == 213 and not result["analysis"]["all_exceptions"], "source/frozen-v2-analysis")
    active_random = [r for r in result["events"] if r["event"] == "random"]
    need(len(active_random) == 1, "source/one-observed-v2-ImageIO-entropy-event")
    io = active_random[0]
    need(io["case"] == 12 and io["receiver_class"] == "java.security.SecureRandom" and
         io["method"] == "nextLong" and io["arguments"] == [] and
         io["source_frames"][:3] == [
             {"class": "ji", "method": "a", "descriptor": "(Ljava/io/InputStream;)Ljava/awt/image/BufferedImage;", "pc": 1},
             {"class": "ji", "method": "b", "descriptor": "(Ljava/lang/String;)I", "pc": 197},
             {"class": "ra", "method": "a", "descriptor": "(Lls;Liz;)V", "pc": 118}] and
         {"class": "java.nio.file.TempFileHelper", "method": "generatePath",
          "descriptor": "(Ljava/lang/String;Ljava/lang/String;Ljava/nio/file/Path;)Ljava/nio/file/Path;", "pc": 3} in io["runtime_frames"] and
         {"class": "java.nio.file.Files", "method": "createTempFile",
          "descriptor": "(Ljava/lang/String;Ljava/lang/String;[Ljava/nio/file/attribute/FileAttribute;)Ljava/nio/file/Path;", "pc": 4} in io["runtime_frames"] and
         {"class": "javax.imageio.stream.FileCacheImageInputStream", "method": "<init>",
          "descriptor": "(Ljava/io/InputStream;Ljava/io/File;)V", "pc": 76} in io["runtime_frames"],
         "source/exact-observed-v2-ImageIO-producer")
    result["image_io_entropy"] = active_random
    return result


def normal_ref(source, case, actual, phase="after_bridge"):
    if actual is None:
        return None
    token = key(actual)
    mapping = source["case_records"][case][phase]
    found = [ident for ident, value in mapping.items() if value == token]
    need(len(found) == 1, "source/exact-observed-reflection-JDI-bridge:" + str(token))
    return {"class": token[0], "identity": found[0]}


def paired_calls(source, case):
    """Actual entered/returned pairs; never infer missing calls from state."""
    stack, result = [], []
    for event in source["events"]:
        if event.get("case") != case:
            continue
        if event["event"] == "entry":
            stack.append(event)
        elif event["event"] == "return":
            need(stack and signature(stack[-1]) == signature(event) and
                 stack[-1]["receiver"] == event["receiver"], "source/actual-paired-call-receiver")
            result.append((stack.pop(), event))
    need(not stack, "source/complete-paired-case")
    return result


def gv_direct_hook(source, case, pc):
    expected = {"class": "gv", "method": "a", "descriptor": "(Lls;DDDFF)V", "pc": pc}
    found = [(entry, returned) for entry, returned in paired_calls(source, case)
             if len(entry["source_frames"]) > 1 and entry["source_frames"][1] == expected]
    need(len(found) == 1, "source/one-actual-GV-hook-at:" + str(pc))
    return found[0]
