"""Fourteen-profile native renderer contracts with optional audited Original pairing.

Bare execution runs authored native checks without opening Original observations.
External Original records are audited before optional comparison, never used as
execution inputs. Public helper byte bindings are explicitly post-capture.
"""

import argparse
import collections
import copy
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
REFERENCE = ROOT / "tests/reference/client/mob_draw_remaining"
sys.path.insert(0, str(REFERENCE))
import native_source_audit as source_audit
import native_source_records as source_v1
import native_device_audit as device_audit

need, digest = source_audit.need, source_audit.digest


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


shared = module("remaining_existing_creature_pair", ROOT / "tests/tools/client/test_creature_draw.py")


class Pair(shared.Pair):
    def __init__(self, device=None):
        super().__init__()
        self.unpaired_current_attributes = []
        self.device = device
        self.current_normal_supplement = []

    def value(self, actual, expected, label):
        if label.endswith("/iy.b"):
            self.name(actual, expected, "texture")
            return
        super().value(actual, expected, label)

    def gl(self, actual, expected, label):
        # Preserve raw input dictionaries. Source legacy post-array attributes
        # stay in the immutable packet; actual native unknown values are NULL.
        left, right = dict(actual), dict(expected)
        for name, width in (("color", 4), ("normal", 3)):
            known = name + "_known"
            need(known in left and type(left[known]) is bool, "pair/actual-current-attribute-known-state")
            if left.pop(known):
                need(isinstance(left[name], list) and len(left[name]) == width, "pair/defined-current-attribute")
                if name == "normal" and self.device is not None:
                    location = re.fullmatch(r"case([0-9]|1[0-3])/(before|after)/gl", label)
                    need(location is not None, "device/finite-normal-profile-location")
                    normal = self.device["normals"][(int(location[1]), location[2])]
                    need(isinstance(normal, list) and len(normal) == 3 and
                         all(type(v) is str and re.fullmatch(r"[0-9a-f]{8}", v) for v in normal),
                         "device/raw-actual-normal-words")
                    self.current_normal_supplement.append({"location": label + "/normal", "native_known": True,
                        "native_raw": left[name], "Linux_original_raw": right[name],
                        "Windows_ordinary_original_raw": normal})
                    right[name] = normal
            else:
                need(left[name] is None, "pair/no-invented-unknown-current-attribute")
                self.unpaired_current_attributes.append({"location": label + "/" + name,
                    "native_known": False, "native_value": None, "original_raw_legacy": right[name],
                    "scope": "undefined post-array current attribute; no equality credit"})
                del left[name]
                del right[name]
        super().gl(left, right, label)


def authored_assets(directory):
    png = module("remaining_authored_png", ROOT / "tests/tools/client/test_startup.py").authored_png
    for name in ("pack.png", "mob/wolf.png", "mob/wolf_tame.png", "mob/wolf_angry.png",
                 "mob/zombie.png", "mob/skeleton.png", "gui/items.png", "misc/shadow.png"):
        atlas, shadow = name == "gui/items.png", name == "misc/shadow.png"
        width, height = (256, 256) if atlas else (128, 128) if shadow else (64, 32)
        seed = 0
        for ch in name:
            seed = (31 * seed + ord(ch)) & 0xffffffff
        def pixel(x, y):
            if shadow:
                distance = math.sqrt(((x + .5) / 64 - 1) ** 2 + ((y + .5) / 64 - 1) ** 2)
                return 0, 0, 0, int(max(0, 1 - distance) * 180)
            if atlas:
                a, b = x & 15, y & 15
                alpha = 255 if a in (3, 12) or b in (2, 13) or abs(a - b) <= 1 else 0
                return (3 * x + 17) & 255, (5 * y + 31) & 255, (((x // 16) ^ (y // 16)) * 13) & 255, alpha
            return 0x60 if x < 32 else 0xd0, 0xb0 if y < 16 else 0x40, (seed >> 3) & 255, 255
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(png(width, height, pixel))


def parse_native(path):
    data = [json.loads(line) for line in Path(path).read_text(encoding="ascii").splitlines()]
    names = ("context", "device", "texture", "hand_setup", "state", "event_work", "model_work", "held",
             "bow_batch", "draw", "list", "queries", "texture_calls", "control", "footer", "hooks",
             "caller_inputs", "alternate_texture")
    groups = {name: [row for row in data if row["event"] == name] for name in names}
    need(sum(map(len, groups.values())) == len(data), "native/complete-declared-record-groups")
    need(len(groups["context"]) == len(groups["device"]) == len(groups["footer"]) == 1 and
         [r["ordinal"] for r in groups["hand_setup"]] == [0, 1], "native/one-context-device-two-hands-footer")
    need(groups["context"][0] == {"event": "context", "scope": "PRIVATE_V2_FOURTEEN_CANDIDATE",
         "RAW_app": True, "namespace_attached": True, "canonical_nonnull": True, "static_holds": 1,
         "default_profile_rebound": False, "displaced_LC_constructor_graph_paired": False,
         "live_GL_order_paired": False}, "native/declared-raw-context")
    need([r["index"] for r in groups["texture"]] == list(range(7)), "native/all-seven-texture-misses-hits")
    for name in ("draw", "queries", "texture_calls", "model_work", "event_work", "hooks"):
        need([r["index" if name == "draw" else "case"] for r in groups[name]] == list(range(14)),
             "native/all-fourteen-" + name)
    need([r["case"] for r in groups["caller_inputs"]] == list(range(7, 14)), "native/seven-declared-variant-inputs")
    need([r["case"] for r in groups["alternate_texture"]] == [12], "native/one-actual-alternate-atlas")
    need([(r["case"], r["phase"]) for r in groups["state"]] == [(i, p) for i in range(14) for p in ("before", "after")],
         "native/all-twentyeight-state-snapshots")
    need([r["case"] for r in groups["held"]] == [4, 5, 6, 11, 12], "native/five-real-held-parent-results")
    need([(r["case"], r["ordinal"]) for r in groups["bow_batch"]] == [(i, j) for i in (4, 5, 6, 11, 12) for j in range(6)],
         "native/all-thirty-Bow-boundaries")
    need(len(groups["list"]) == 25, "native/all-twentyfive-real-model-lists")
    need(groups["footer"] == [{"event": "footer", "cases": 14, "model_lists": 25,
         "Bow_backing_batches": 30, "scope": "private_v2_fourteen"}], "native/footer-inventory")
    control_order = [(name, i) for i in range(14) for name in
                     (("held_guard", "held_retirement", "retained_graph") if i in (4, 5, 6, 11, 12) else ("retained_graph",))]
    need([(r["name"], r["case"]) for r in groups["control"]] == control_order, "native/all-twentyfour-owner-controls")
    for row in groups["control"]:
        expected = {"event": "control", "name": row["name"], "case": row["case"]}
        keys = {"retained_graph": ("app_retire_busy", "render_borrows_restored"),
                "held_guard": ("first_person_busy", "held_tick_busy", "retain_busy", "release_busy",
                               "wrong_token_refused", "nonretiring_refused", "prefix_preserved"),
                "held_retirement": ("guard_discharged", "RA_usable", "prefix_preserved")}[row["name"]]
        expected.update({key: True for key in keys})
        need(row == expected, "native/exact-owner-control:" + row["name"])
    for i, row in enumerate(groups["draw"]):
        need(row["outcome"] == 1 and row["gl_error"] == 0 and row["nw_position"] == len(row["nw_words"]) == 192,
             "native/complete-final-shadow-buffer")
        a, b = groups["state"][2 * i:2 * i + 2]
        need(a["entropy"] == b["entropy"], "native/no-default-stream-progress")
        need(b["pool"] == (264 if i < 2 else 432 if i < 4 else 600), "native/actual-pool-progress")
    for row in groups["bow_batch"]:
        need(row["scope"] == "captured_raw_NW_g" and row["submission_bytes_paired"] is False and
             len(row["words"]) == (48 if row["ordinal"] < 2 else 768), "native/complete-raw-Bow-prefix")
        need(row["before"]["nw.w"] is True and row["after_native_only"]["nw.w"] is False,
             "native/actual-before-after-NW-state")
    return groups


def source_queries(source, case):
    return shared.source_queries({"source_events": source["events"]}, case)


def call_pairs(source, case):
    result, stack = [], []
    for row in source["events"]:
        if row.get("case") != case:
            continue
        if row["event"] == "entry":
            stack.append(row)
        elif row["event"] == "return":
            need(stack and source_v1.signature(stack[-1]) == source_v1.signature(row), "source/call-pair-nesting")
            result.append((stack.pop(), row))
    need(not stack, "source/full-case-calls")
    return result


def compare(groups, source, device=None):
    need(len(groups["state"]) == 28 and len(groups["draw"]) == len(groups["queries"]) ==
         len(groups["texture_calls"]) == len(groups["model_work"]) == len(groups["event_work"]) == 14 and len(groups["texture"]) == 7 and
         len(groups["list"]) == 25 and len(groups["held"]) == 5 and len(groups["bow_batch"]) == 30 and
         len(groups["hooks"]) == 14 and len(groups["caller_inputs"]) == 7 and len(groups["alternate_texture"]) == 1 and
         len(groups["hand_setup"]) == 2,
         "pair/complete-top-level-group-inventory")
    if device is not None:
        device_audit.matching_device(groups["device"][0], device["device"])
    pair = Pair(device)
    for actual, expected in zip(groups["texture"], source["texture_records"]):
        pair.name(actual["name"], expected["name"], "texture")
        pair.name(actual["cached_name"], expected["cached_name"], "texture")
        for name in ("wrap_s", "wrap_t", "min", "mag"):
            pair.value(actual[name], expected["after"][name], "texture/" + name)
        pair.value(actual["transfer_hex"], expected["transfer_hex"], "texture/decoded_transfer")
    query_count, blocks, reverse_blocks = 0, {}, {}
    for case, expected in enumerate(source["draw_records"]):
        for offset, phase in enumerate(("before", "after")):
            actual, original = groups["state"][case * 2 + offset], expected[phase]
            pair.graph(actual["graph"], original["graph"], f"case{case}/{phase}/graph")
            for name in ("parts", "pool", "pool_storage"):
                pair.value(actual[name], original[name], f"case{case}/{phase}/{name}")
            pair.gl(actual["gl"], original["gl"], f"case{case}/{phase}/gl")
        for name in ("outcome", "nw_position", "nw_words", "gl_error"):
            pair.value(groups["draw"][case][name], expected[name], f"case{case}/{name}")
        actuals, originals = groups["queries"][case]["queries"], source_queries(source, case)
        need(len(actuals) == len(originals), "pair/complete-direct-query-extent")
        native_world = groups["state"][case * 2]["graph"]["roots"]["world"]
        for ordinal, (actual, original) in enumerate(zip(actuals, originals)):
            label = f"case{case}/query{ordinal}"
            for name in ("method", "caller", "arguments", "value"):
                pair.value(actual[name], original[name], label + "/" + name)
            need(actual["invoked"] and actual["returned"] and not actual["failed"] and
                 not actual["native_policy"] and not actual["source_exception"] and actual["exception_class"] is None,
                 "pair/actual-query-return")
            token = source_v1.key(original["receiver"])
            if token[0] == "fd":
                need(actual["receiver_role"] == "world" and actual["receiver"] == native_world["identity"], "pair/actual-World-receiver")
                pair.value(native_world, source_v1.normal_ref(source, case, original["receiver"], "entry_bridge"), label + "/receiver")
            else:
                need(token[0] == "eo" and actual["receiver_role"] == "block:1", "pair/actual-canonical-Stone-role")
                pointer = actual["receiver"]
                need(pointer not in blocks or blocks[pointer] == token, "pair/Block-identity-continuity")
                need(token not in reverse_blocks or reverse_blocks[token] == pointer, "pair/Block-alias-partition")
                blocks[pointer], reverse_blocks[token] = token, pointer
            query_count += 1
    # Native list observations are emitted by current field order. Join actual
    # PS identities instead of pretending that observation order is call order.
    expected_lists = {}
    for row in source["list_records"]:
        part = source_v1.normal_ref(source, row["case"], row["part"])
        token = shared.reference(part)
        need(token not in expected_lists, "pair/unique-Source-compiled-part")
        expected_lists[token] = row
    seen = set()
    for ordinal, actual in enumerate(groups["list"]):
        native_part = shared.reference(actual["part"])
        need(native_part in pair.forward, "pair/Native-compiled-part-is-in-actual-graph")
        part = pair.forward[native_part]
        need(part in expected_lists and part not in seen, "pair/all-compiled-parts-once")
        seen.add(part)
        expected = expected_lists[part]
        pair.name(actual["list"], expected["list"], "list")
        need(actual["commands"] == len(actual["batches"]) == len(expected["batches"]) == 6, "pair/six-complete-face-commands")
        for face, (a, b) in enumerate(zip(actual["batches"], expected["batches"])):
            args = b["draw"]["arguments"]
            pair.value(a["vertices"], b["batch"]["nw.h"], f"list{ordinal}/face{face}/vertices")
            pair.value(a["vertices"], args[2], f"list{ordinal}/face{face}/actualDrawCount")
            pair.value(a["mode"], args[0], f"list{ordinal}/face{face}/actualDrawMode")
            pair.value(a["words"], b["batch"]["words"], f"list{ordinal}/face{face}/allWords")
            need(a["arrays"] == 11, "pair/actual-vertex-UV-normal-list-arrays")
    need(seen == set(expected_lists), "pair/no-omitted-compiled-list")
    for actual in groups["held"]:
        case = actual["case"]
        roots = source["draw_records"][case]["after"]["graph"]["roots"]
        for name, root in (("hand", "hand"), ("stack", "class_bow_stack"), ("application", "hand_app"), ("engine", "hand_engine")):
            pair.value(actual[name], roots[root], f"case{case}/held/{name}")
        need(actual["source_returned"] and actual["geometry_returned"], "pair/actual-held-returns")
        calls = call_pairs(source, case)
        icons = [r for e, r in calls if source_v1.signature(e) == ("ls", "c", "(Liz;)I")]
        need(len(icons) == 1, "pair/actual-Source-icon-return")
        pair.value(actual["icon"], icons[0]["result"], f"case{case}/held/icon")
        lookups = [e for e, r in calls if source_v1.signature(e) == ("ji", "b", "(Ljava/lang/String;)I") and
                   len(e["source_frames"]) > 1 and e["source_frames"][1] ==
                   {"class": "ra", "method": "a", "descriptor": "(Lls;Liz;)V", "pc": 118}]
        need(len(lookups) == 1, "pair/actual-RA-atlas-lookup")
        key = lookups[0]["argument_references"][0]
        need(key["class"] == "java.lang.String" and key["string"] == lookups[0]["arguments"][0], "pair/actual-RA-String-reference")
        pair.value(actual["key"], {"class": "java.lang.String", "identity": "source-jdi-string:" + str(key["oid"])}, f"case{case}/held/keyAlias")
        pair.value(actual["key_text"], key["string"], f"case{case}/held/keyText")
        pair.value(actual["engine"], source_v1.normal_ref(source, case, lookups[0]["receiver"]), f"case{case}/held/lookupEngine")
        binds = [r for r in source["events"] if r.get("case") == case and r["event"] == "gl-entry" and
                 r["method"] == "glBindTexture" and r["descriptor"] == "(II)V" and r["source_frames"] and
                 r["source_frames"][0] == {"class": "ra", "method": "a", "descriptor": "(Lls;Liz;)V", "pc": 121}]
        # GL records exclude the GL11 callee from their original-frame list;
        # ordinary method records instead include their callee as frame0.
        need(len(binds) == 1 and len(binds[0]["arguments"]) == 2 and
             all(type(v) is int for v in binds[0]["arguments"]) and binds[0]["arguments"][0] == 3553,
             "pair/actual-unconditional-RA-bind")
        pair.name(actual["texture"], binds[0]["arguments"][1], "texture")
    bow_words = 0
    for actual in groups["bow_batch"]:
        case, ordinal = actual["case"], actual["ordinal"]
        expected = source["analysis"]["cases"][case]["bow_batches"][ordinal]
        pair.value(actual["caller_pc"], expected["caller_pc"], f"case{case}/Bow{ordinal}/actualCaller")
        pair.value(actual["before"], {k: v for k, v in expected["batch"].items() if k != "words"}, f"case{case}/Bow{ordinal}/actualNWFields")
        pair.value(actual["words"], expected["batch"]["words"], f"case{case}/Bow{ordinal}/allRawWords")
        bow_words += len(actual["words"])
    model_count = compare_model_work(pair, groups, source)
    event_count = compare_event_work(pair, groups, source)
    texture_calls = compare_texture_calls(pair, groups, source)
    hook_count = compare_hooks(pair, groups, source)
    compare_variant_inputs(pair, groups, source)
    alternate = compare_alternate_texture(pair, groups, source)
    compare_hand_setup(pair, groups, source)
    return {"typed_graph_state_checks": pair.checks, "identity_pairs": len(pair.forward),
            "expanded_objects": sum(len(r["graph"]["objects"]) for r in groups["state"]),
            "query_attempts": query_count, "compiled_lists": len(seen), "model_faces": len(seen) * 6,
            "Bow_external_boundaries": len(groups["bow_batch"]), "Bow_raw_backing_words": bow_words,
            "final_shadow_words": sum(len(r["nw_words"]) for r in groups["draw"]),
            "selected_model_work_records": model_count, "direct_living_post_texture_calls": texture_calls,
            "actual_event_work_records": event_count,
            "actual_living_hook_records": hook_count,
            "declared_variant_inputs": len(groups["caller_inputs"]),
            "variant_displaced_counts_native_only": [r["displaced_count_native"] for r in groups["caller_inputs"]],
            "alternate_atlas": alternate,
            "biped_pose_store_counters_native_only": [r["pose_stores"] for r in groups["model_work"] if source["draw_records"][r["case"]]["kind"] != "gi"],
            "event_nested_floor_calls_source_only": [e for e in source["events"] if e["event"] == "entry" and
                source_v1.signature(e) == ("in", "b", "(D)I") and any(f["class"] == "o" and
                f["descriptor"] == "(Lls;FFF)V" for f in e["source_frames"])],
            "image_decoder_entropy_source_only": source["image_io_entropy"],
            "matrix_maximum_absolute_error": pair.matrix_maximum, "matrix_opposite_signed_zeros": pair.matrix_signed_zeros,
            "unpaired_unknown_current_attributes": pair.unpaired_current_attributes,
            "known_current_normal_supplement": pair.current_normal_supplement,
            "matrix_allowance": "prepared final MV/projection only: four float ULP and opposite signed-zero numeric equality",
            "GL_authority": ("frozen Linux ordinary/JDI v2; exact same-device fourteen-case ordinary known current-normal only"
                             if device is not None else "strict frozen Linux ordinary/JDI v2; no Windows supplementary authority")}


def compare_model_work(pair, groups, source):
    for actual in groups["model_work"]:
        case = actual["case"]
        wolf = source["draw_records"][case]["kind"] == "gi"
        model = source["draw_records"][case]["after"]["graph"]["roots"]["primary"]
        pair.value(actual["model"], model, f"case{case}/modelWork/receiver")
        calls = call_pairs(source, case)
        selected = "o" if wolf else "fh"
        renders = [(e, r) for e, r in calls if source_v1.signature(e) == (selected, "a", "(FFFFFF)V")]
        poses = [(e, r) for e, r in calls if source_v1.signature(e) == ("o" if wolf else "ej", "b", "(FFFFFF)V")]
        need(len(renders) == len(poses) == 1, "pair/one-actual-selected-render-pose")
        pair.value(actual["model"], source_v1.normal_ref(source, case, renders[0][0]["receiver"]), f"case{case}/actual-render-receiver")
        pair.value(actual["model"], source_v1.normal_ref(source, case, poses[0][0]["receiver"]), f"case{case}/actual-pose-receiver")
        pair.value(actual["render_site"], renders[0][1]["location"], f"case{case}/actual-render-return")
        pair.value(actual["pose_site"], poses[0][1]["location"], f"case{case}/actual-pose-return")
        pair.value(actual["arguments"], renders[0][0]["arguments"], f"case{case}/actual-render-arguments")
        pair.value(actual["arguments"], poses[0][0]["arguments"], f"case{case}/actual-pose-arguments")
        first, last = poses[0][0]["sequence"], poses[0][1]["sequence"]
        math_calls = [e for e, r in calls if e["location"]["class"] == "in" and first < e["sequence"] < last]
        for name, method in (("cosine_calls", "b"), ("sine_calls", "a"), ("sqrt_calls", "c")):
            pair.value(actual[name], sum(e["location"]["method"] == method for e in math_calls), f"case{case}/actual-" + name)
        pair.value(actual["math_calls"], len(math_calls), f"case{case}/actual-Math-total")
        parts = [e for e, r in calls if e["location"]["class"] == "ps" and e["location"]["descriptor"] == "(F)V" and
                 renders[0][0]["sequence"] < e["sequence"] < renders[0][1]["sequence"]]
        for name, method in (("part_a_calls", "a"), ("part_b_calls", "b"), ("part_c_calls", "c")):
            pair.value(actual[name], sum(e["location"]["method"] == method for e in parts), f"case{case}/actual-" + name)
        if wolf:
            pair.value(actual["pose_stores"], 9, f"case{case}/Code-qualified-pose-stores")
        for name, value in (("fh_pose_returned", not wolf), ("ej_pose_returned", not wolf), ("fh_render_returned", not wolf)):
            pair.value(actual[name], value, f"case{case}/" + name)
        for name, sig in (("fh_pose_site", ("fh", "b", "(FFFFFF)V")),
                          ("ej_pose_site", ("ej", "b", "(FFFFFF)V")),
                          ("fh_render_site", ("fh", "a", "(FFFFFF)V"))):
            returns = [r for e, r in calls if source_v1.signature(e) == sig]
            need(len(returns) == (0 if wolf else 1), "pair/actual-superclass-return-inventory")
            pair.value(actual[name], None if not returns else returns[0]["location"], f"case{case}/" + name)
    return len(groups["model_work"])


def compare_event_work(pair, groups, source):
    for actual in groups["event_work"]:
        case = actual["case"]
        wolf = source["draw_records"][case]["kind"] == "gi"
        calls = call_pairs(source, case)
        selected = ("o" if wolf else "ko", "a", "(Lls;FFF)V")
        entered = [(e, r) for e, r in calls if source_v1.signature(e) == selected]
        need(len(entered) == 1, "pair/one-actual-entity-model-event")
        entry, returned = entered[0]
        pair.value(actual["model"], source_v1.normal_ref(source, case, entry["receiver"]), f"case{case}/event/model")
        pair.value(actual["actor"], source_v1.normal_ref(source, case, entry["arguments"][0]), f"case{case}/event/actor")
        pair.value(actual["arguments"], entry["arguments"][1:], f"case{case}/event/arguments")
        pair.value(actual["event_site"], returned["location"], f"case{case}/event/return")
        nested = [e for e, r in calls if entry["sequence"] < e["sequence"] < returned["sequence"]]
        # This producer counter counts actual table trig, including GI shake
        # descendants. Wet brightness's nine IN.b(D)I calls remain separately
        # retained; they are not silently relabelled as model trig calls.
        maths = [e for e in nested if source_v1.signature(e) in (("in", "a", "(F)F"), ("in", "b", "(F)F"))]
        pair.value(actual["math_calls"], len(maths), f"case{case}/event/actual-Math-count")
        for name, method in (("cosine_calls", "b"), ("sine_calls", "a")):
            pair.value(actual[name], sum(e["location"]["method"] == method for e in maths), f"case{case}/event/" + name)
        pivots = [e for e in nested if source_v1.signature(e) == ("ps", "a", "(FFF)V")]
        pair.value(actual["pivot_calls"], len(pivots), f"case{case}/event/actual-pivot-calls")
        pair.value(actual["pivot_stores"], len(pivots) * 3, f"case{case}/event/Code-three-stores-per-returned-PS-pivot")
        leaves = [e for e in nested if e["location"]["class"] in ("gi", "sn") and len(e["source_frames"]) > 1 and
                  tuple(e["source_frames"][1][k] for k in ("class", "method", "descriptor")) == selected]
        pair.value(actual["entity_leaf_calls"], len(leaves), f"case{case}/event/actual-direct-GI-calls")
        direct_stores = 0
        if wolf:
            sitting = [r for e, r in calls if source_v1.signature(e) == ("gi", "B", "()Z") and
                       entry["sequence"] < e["sequence"] < returned["sequence"]]
            need(len(sitting) == 1 and type(sitting[0]["result"]) is bool, "pair/actual-observed-sitting-branch")
            # O Code one tail store + seven common roll stores; the observed
            # sitting branch has seven direct stores, standing has six.
            # Exact class/Code pin and live GI.B return establish this finite
            # branch count, not a JDI field-modification observation.
            direct_stores = 1 + 7 + (7 if sitting[0]["result"] else 6)
        pair.value(actual["pose_stores"], direct_stores, f"case{case}/event/Code-qualified-pose-stores")
    return len(groups["event_work"])


def compare_texture_calls(pair, groups, source):
    count = 0
    for case, record in enumerate(groups["texture_calls"]):
        calls = call_pairs(source, case)
        lookup = [e for e, r in calls if source_v1.signature(e) == ("ji", "b", "(Ljava/lang/String;)I") and
                  len(e["source_frames"]) > 1 and e["source_frames"][1] ==
                  {"class": "bw", "method": "c", "descriptor": "(Lsn;DDDFF)V", "pc": 30}]
        bind = [e for e, r in calls if source_v1.signature(e) == ("ji", "b", "(I)V") and
                len(e["source_frames"]) > 1 and e["source_frames"][1] ==
                {"class": "bw", "method": "c", "descriptor": "(Lsn;DDDFF)V", "pc": 33}]
        need(len(record["calls"]) == len(lookup) == len(bind) == 1, "pair/one-actual-direct-shadow-texture-route")
        actual, entry, binding = record["calls"][0], lookup[0], bind[0]
        need(actual["phase"] == "post" and actual["returned"], "pair/actual-post-texture-return")
        pair.value(actual["engine"], source_v1.normal_ref(source, case, entry["receiver"], "entry_bridge"), f"case{case}/shadowEngine")
        string = entry["argument_references"][0]
        need(string["class"] == "java.lang.String" and string["string"] == entry["arguments"][0], "source/actual-String-identity-value")
        pair.value(actual["key"], {"class": "java.lang.String", "identity": "source-jdi-string:" + str(string["oid"])}, f"case{case}/shadowStringAlias")
        encoded = string["string"].encode("utf-16-be")
        pair.value(actual["units"], [int.from_bytes(encoded[i:i + 2], "big") for i in range(0, len(encoded), 2)], f"case{case}/shadowStringUnits")
        for name, expected in (("caller", entry["source_frames"][1]), ("lookup_site", entry["source_frames"][1]), ("bind_site", binding["source_frames"][1])):
            pair.value(actual[name], expected, f"case{case}/shadowTexture/" + name)
        pair.name(actual["texture_id"], binding["arguments"][0], "texture")
        count += 1
    return count


def compare_hand_setup(pair, groups, source):
    roots = source["draw_records"][0]["before"]["graph"]["roots"]
    for actual, which in zip(groups["hand_setup"], ("initial_hand", "alternate_hand")):
        pair.value(actual["hand"], roots[which], "setup/" + which)
        pair.value(actual["class_stack"], roots["class_bow_stack"], "setup/class-stack")
        need(actual["RAW_app"] is True and actual["IY_width"] == actual["IY_height"] == 128 and
             actual["genuine_publication"] is (actual["ordinal"] == 0), "native/actual-two-hand-setup")
        graph = source["draw_records"][0]["before"]["graph"]
        ra = next(n for n in graph["objects"] if shared.reference(n) == shared.reference(roots[which]))
        iy = next(n for n in graph["objects"] if shared.reference(n) == shared.reference(ra["fields"]["ra.f"]))
        pair.name(actual["IY_texture"], iy["fields"]["iy.b"], "texture")
        if actual["ordinal"] == 1:
            pair.value(actual["application"], roots["alternate_app"], "setup/alternate-App")
            pair.value(actual["engine"], roots["alternate_engine"], "setup/alternate-Engine")


def compare_variant_inputs(pair, groups, source):
    for actual in groups["caller_inputs"]:
        case = actual["case"]
        need(actual["scope"] == "declared_mutable_operands_not_draw_getter_receipts", "native/variant-input-scope")
        before = source["draw_records"][case]["before"]
        roots = before["graph"]["roots"]
        pair.value(actual["actor"], roots["actor"], f"case{case}/declared/actor")
        pair.value(actual["hand"], roots["hand"], f"case{case}/declared/hand")
        item = next(n for n in before["graph"]["objects"] if shared.reference(n) == shared.reference(roots["class_bow_item"]))
        pair.value(actual["item_full3d"], item["fields"]["gm.bi"], f"case{case}/declared/GM.bi")
        # These are only the declared caller operands. The original retains
        # complete watcher records; no boxed scalar/reference alias credit is
        # inferred from the actual native integer/Byte value observations.
        if source["draw_records"][case]["kind"] == "gi":
            records = {r["index"]: r["fields"] for r in before["actor"]["watcher_records"]}
            pair.value(actual["watcher16"], records[16]["ma.c"], f"case{case}/declared/Byte16-value")
            pair.value(actual["watcher18"], records[18]["ma.c"], f"case{case}/declared/Integer18-value")
            fields = before["actor"]["fields"]
            pair.value(actual["wet"], fields["gi.f"], f"case{case}/declared/wet")
            pair.value(actual["wet_fields"], [fields["gi." + n] for n in ("b", "c", "h", "i")], f"case{case}/declared/wet-fields")
        need(actual["displaced_count_native"] == (0 if case < 12 else case - 11), "native/actual-handed-publication-occurrences")


def compare_hooks(pair, groups, source):
    for actual in groups["hooks"]:
        case = actual["case"]
        roots = source["draw_records"][case]["after"]["graph"]["roots"]
        calls = call_pairs(source, case)
        for field, pc in (("rotation_returned", 160), ("prescale_returned", 184), ("color_returned", 382)):
            source_v1.gv_direct_hook(source, case, pc)
            pair.value(actual[field], True, f"case{case}/hook/" + field)
        pair.value(actual["age"], source_v1.gv_direct_hook(source, case, 147)[1]["result"], f"case{case}/hook/age")
        pair.value(actual["color"], source_v1.gv_direct_hook(source, case, 382)[1]["result"], f"case{case}/hook/color")
        v = [(e, r) for e, r in calls if source_v1.signature(e) == ("v", "b", "(Lls;F)V")]
        pair.value(actual["held_entered"], len(v) == 1, f"case{case}/hook/actual-V-entered")
        need(len(v) <= 1, "source/at-most-one-real-V-child")
        getters = [(e, r) for e, r in calls if source_v1.signature(e) in
                   (("ls", "r_", "()Liz;"), ("fr", "r_", "()Liz;"))]
        pair.value(actual["stack_returned"], len(getters) == 1, f"case{case}/hook/actual-stack-getter")
        stack = None if not getters else source_v1.normal_ref(source, case, getters[0][1]["result"])
        pair.value(actual["stack"], stack, f"case{case}/hook/returned-stack")
        arm = [(e, r) for e, r in calls if source_v1.signature(e) == ("ps", "c", "(F)V") and
               len(e["source_frames"]) > 1 and e["source_frames"][1] ==
               {"class": "v", "method": "b", "descriptor": "(Lls;F)V", "pc": 21}]
        own = [r for r in source["events"] if r.get("case") == case and r["event"] == "field-read" and
               r["location"] == {"class": "v", "method": "b", "descriptor": "(Lls;F)V", "pc": 13} and
               r["declaring_class"] == "v" and r["field"] == "a"]
        pair.value(actual["attachment_returned"], len(arm) == 1, f"case{case}/hook/actual-arm-attachment")
        need(len(own) == len(arm) and len(arm) <= 1, "source/actual-V-own-model-and-arm")
        pair.value(actual["own_model"], None if not own else source_v1.normal_ref(source, case, own[0]["value"]), f"case{case}/hook/own-model")
        pair.value(actual["arm"], None if not arm else source_v1.normal_ref(source, case, arm[0][0]["receiver"]), f"case{case}/hook/arm")
        full = [(e, r) for e, r in calls if source_v1.signature(e) == ("gm", "b", "()Z")]
        need(len(full) <= 1, "source/at-most-one-full3d-getter")
        for flag in ("full3d_attempted", "full3d_returned"):
            pair.value(actual[flag], len(full) == 1, f"case{case}/hook/" + flag)
        pair.value(actual["full3d"], False if not full else full[0][1]["result"], f"case{case}/hook/actual-GM-full3d")
        pair.value(actual["item"], None if not full else source_v1.normal_ref(source, case, full[0][0]["receiver"]), f"case{case}/hook/item")
        ra = [(e, r) for e, r in calls if source_v1.signature(e) == ("ra", "a", "(Lls;Liz;)V")]
        pair.value(actual["hand"], None if not ra else source_v1.normal_ref(source, case, ra[0][0]["receiver"]), f"case{case}/hook/hand")
        pair.value(actual["held_returned"], len(v) == 1, f"case{case}/hook/actual-V-returned")
    return len(groups["hooks"])


def compare_alternate_texture(pair, groups, source):
    actual = groups["alternate_texture"][0]
    need(actual["case"] == 12 and actual["scope"] == "native_transfer_bytes_Source_upload_parameters_only", "pair/alternate-atlas-finite-scope")
    calls = call_pairs(source, 12)
    lookups = [(e, r) for e, r in calls if source_v1.signature(e) == ("ji", "b", "(Ljava/lang/String;)I") and
               len(e["source_frames"]) > 1 and e["source_frames"][1] ==
               {"class": "ra", "method": "a", "descriptor": "(Lls;Liz;)V", "pc": 118}]
    need(len(lookups) == 1, "source/one-real-alternate-lookup")
    entry, returned = lookups[0]
    pair.value(actual["engine"], source_v1.normal_ref(source, 12, entry["receiver"]), "alternate/actual-lookup-Engine")
    pair.name(actual["name"], returned["result"], "texture")
    events = [r for r in source["events"] if r["event"] == "gl-entry" and entry["sequence"] < r["sequence"] < returned["sequence"]]
    uploads = [r for r in events if r["method"] == "glTexImage2D"]
    need(len(uploads) == 1 and uploads[0]["arguments"][:8] == [3553, 0, 6408, 256, 256, 0, 6408, 5121], "source/actual-alternate-upload-parameters")
    params = {r["arguments"][1]: r["arguments"][2] for r in events if r["method"] == "glTexParameteri" and r["arguments"][0] == 3553}
    for name, enum in (("min", 10241), ("mag", 10240), ("wrap_s", 10242), ("wrap_t", 10243)):
        pair.value(actual[name], params[enum], "alternate/actual-parameter/" + name)
    need(actual["cache_after"] == 1 and type(actual["transfer_hex"]) is str and
         re.fullmatch(r"[0-9a-f]{524288}", actual["transfer_hex"]) is not None,
         "native/actual-alternate-cache-transfer")
    atlas = groups["texture"][5]
    need(actual["transfer_hex"] == atlas["transfer_hex"], "native/same-authored-asset-two-actual-transfers")
    return {"actual_GL_uploads": 1, "actual_parameter_words": 4, "native_transfer_bytes": len(actual["transfer_hex"]) // 2,
            "native_cache_entries": actual["cache_after"], "native_transfer_sha256": hashlib.sha256(bytes.fromhex(actual["transfer_hex"])).hexdigest(),
            "scope": "Native actual alternate transfer agrees with its own actual primary authored-atlas transfer. Source12 actual upload parameters/Engine/key/return/bind are paired; deep Source12 ByteBuffer payload is unobserved and unpaired."}


def negatives(groups, source, device=None):
    result = []
    tests = {
        "renderer_class": ("reference class",),
        "class_stack_alias_split": ("reference alias partition", "reference continuity"),
        "temporal_same_value_part_replacement": ("reference alias partition", "reference continuity"),
        "Bow_reserved_word": ("exact value",),
        "missing_Bow_boundary": ("pair/complete-top-level-group-inventory",),
        "query_order": ("exact value",),
        "texture_role": ("source name continuity", "source name alias partition"),
        "IY_texture_zero": ("zero source name",),
        "missing_model_face": ("pair/six-complete-face-commands",),
        "held_Engine_identity": ("reference alias partition", "reference continuity"),
        "invented_unknown_attribute": ("pair/no-invented-unknown-current-attribute",),
        "missing_Wolf_event_Math": ("exact value",),
        "missing_hook_record": ("pair/complete-top-level-group-inventory",),
        "missing_variant_input": ("pair/complete-top-level-group-inventory",),
        "alternate_Engine_alias": ("reference continuity", "reference alias partition"),
        "full3d_getter_result": ("exact value",),
        "sitting_trig_count": ("exact value",),
        "wet_extra_entity_calls": ("exact value",),
        "alternate_transfer": ("native/same-authored-asset-two-actual-transfers",),
        "mixed_hand_restore": ("reference continuity", "reference alias partition"),
    }
    for name, messages in tests.items():
        changed = copy.deepcopy(groups)
        if name == "renderer_class":
            changed["state"][0]["graph"]["roots"]["renderer"]["class"] = "v"
        elif name == "class_stack_alias_split":
            changed["held"][2]["stack"]["identity"] = "negative-split-stack"
        elif name == "temporal_same_value_part_replacement":
            state = changed["state"][1]
            token = shared.reference(state["parts"][0]["part"])
            def replace(value):
                if isinstance(value, dict):
                    if shared.reference(value) == token:
                        value["identity"] = "negative-fresh-same-value-PS"
                    for child in value.values():
                        replace(child)
                elif isinstance(value, list):
                    for child in value:
                        replace(child)
            replace(state)
        elif name == "Bow_reserved_word":
            changed["bow_batch"][0]["words"][7] = "deadbeef"
        elif name == "missing_Bow_boundary":
            changed["bow_batch"].pop()
        elif name == "query_order":
            q = changed["queries"][0]["queries"]
            need(len(q) > 1 and q[0] != q[1], "negative/actual-distinct-query-operands")
            q[0], q[1] = q[1], q[0]
        elif name == "texture_role":
            changed["texture"][1]["cached_name"] = changed["texture"][0]["name"]
        elif name == "IY_texture_zero":
            node = next(n for n in changed["state"][0]["graph"]["objects"] if n["class"] == "iy")
            node["fields"]["iy.b"] = 0
        elif name == "missing_model_face":
            changed["list"][0]["batches"].pop()
        elif name == "held_Engine_identity":
            changed["held"][0]["engine"]["identity"] = "negative-unrelated-Engine"
        elif name == "missing_Wolf_event_Math":
            changed["event_work"][0]["math_calls"] = 0
        elif name == "missing_hook_record":
            changed["hooks"].pop()
        elif name == "missing_variant_input":
            changed["caller_inputs"].pop()
        elif name == "alternate_Engine_alias":
            changed["held"][4]["engine"] = copy.deepcopy(changed["held"][0]["engine"])
        elif name == "full3d_getter_result":
            changed["hooks"][11]["full3d"] = False
        elif name == "sitting_trig_count":
            changed["event_work"][7]["cosine_calls"] = 5
        elif name == "wet_extra_entity_calls":
            changed["event_work"][10]["entity_leaf_calls"] = 8
        elif name == "alternate_transfer":
            text = changed["alternate_texture"][0]["transfer_hex"]
            changed["alternate_texture"][0]["transfer_hex"] = ("ff" if text[:2] != "ff" else "00") + text[2:]
        elif name == "mixed_hand_restore":
            changed["caller_inputs"][6]["hand"] = copy.deepcopy(changed["held"][4]["hand"])
        else:
            found = False
            for state in changed["state"]:
                for attribute, width in (("color", 4), ("normal", 3)):
                    if state["gl"].get(attribute + "_known") is False:
                        state["gl"][attribute] = ["3f800000"] * width
                        found = True
                        break
                if found:
                    break
            need(found, "negative/actual-unknown-attribute-branch")
        try:
            compare(changed, source, device)
        except AssertionError as error:
            need(any(text in str(error) for text in messages), "negative/wrong-rejection:" + name + ":" + str(error))
            result.append({"name": name, "rejected": True, "failure": str(error)[:800]})
        else:
            raise AssertionError("negative/admitted:" + name)
    return result


def stdout_footer(text):
    lines = text.splitlines()
    need(len(lines) == 3 and lines[:2] == ["151 recipes", "16 achievements"], "native/exact-declared-stdout")
    footer = re.fullmatch(r"Remaining draw private: ([1-9][0-9]*) checks;14 genuine profiles", lines[2])
    need(footer is not None, "native/one-exact-footer")
    return int(footer[1])


def device_comparison_negatives(groups, source, device):
    result = []
    trials = (("native_device_mismatch", "device/exact-native-source-profile"),
              ("non_normal_GL", "exact value"), ("non_normal_geometry", "exact value"),
              ("known_normal_corruption", "exact value"), ("matrix_corruption", "matrix four-ULP bound"))
    for name, category in trials:
        changed = copy.deepcopy(groups)
        if name == "native_device_mismatch":
            changed["device"][0]["vendor"] = "negative other vendor"
        elif name == "non_normal_GL":
            changed["state"][0]["gl"]["depth_mask"] = not changed["state"][0]["gl"]["depth_mask"]
        elif name == "non_normal_geometry":
            changed["bow_batch"][0]["words"][7] = "deadbeef"
        elif name == "known_normal_corruption":
            need(changed["state"][0]["gl"]["normal_known"] is True, "negative/actual-known-normal")
            changed["state"][0]["gl"]["normal"][0] = "deadbeef"
        else:
            changed["state"][0]["gl"]["modelview"][0] = "40000000"
        try:
            compare(changed, source, device)
        except AssertionError as error:
            need(category in str(error), "device/negative-wrong-category:" + name + ":" + str(error))
            result.append({"name": name, "rejected": True, "category": category, "failure": str(error)[:800]})
        else:
            raise AssertionError("device/negative-admitted:" + name)
    # Preserve a positive proof that the ordinary Linux authority still rejects
    # this known-normal mismatch when the explicit supplement is omitted.
    try:
        compare(groups, source)
    except AssertionError as error:
        need("exact value case0/after/gl/normal/0" in str(error), "device/default-wrong-rejection:" + str(error))
        result.append({"name": "strict_default_Mesa", "rejected": True, "failure": str(error)[:800]})
    else:
        raise AssertionError("device/default-Linux-authority-unexpectedly-admitted-profile")
    return result


def pixel_checks(directory):
    result = []
    for case in range(14):
        files = [directory / f"{phase}{case}.rgba" for phase in ("baseline", "draw")]
        need(all(p.is_file() and p.stat().st_size == 256 * 256 * 4 for p in files), "native/complete-readbacks")
        before, after = (p.read_bytes() for p in files)
        changed = sum(before[i:i + 4] != after[i:i + 4] for i in range(0, len(after), 4))
        darkened = sum(all(after[i + c] <= before[i + c] for c in range(3)) and
                       any(after[i + c] < before[i + c] for c in range(3)) for i in range(0, len(after), 4))
        need(changed > 64 and darkened > 1, "native/actual-visible-authored-draw-and-shadow")
        result.append({"case": case, "changed": changed, "darkened": darkened, "sha256": digest(files[1])})
    return {"scope": "Native authored visibility only; no original raster, compile/reuse or whole-frame equality", "rows": result}


LIMITS = [
    "Private fourteen-case v2 only, not a published Source packet or complete-frame qualification. V1 Windows device evidence is not accepted; optional actual v2 fourteen-case same-device authority supplies known normals only.",
    "Original capture30 inputs/four helpers have actual before/after pins; final-audit is a separate post-capture current/artifact audit. Current Native files were authored later and are not retroactive Source capture-start inputs.",
    "Raw Entity constructor fields/default streams remain independent; compare active within-process no-progress and model/graph effects, never equal unrelated private seeds.",
    "Only selected model/PS/TZ/IB/BT/RA/IY/CV/Stack/Item fields and aliases are expanded. Original actor/camera raw fieldsets, displaced LC per-allocation constructor trace and IY primitive-array contents remain unpaired.",
    "No Native per-part live GL matrices/order trace is exposed. Pair actual PS pivot/angle fields, all model face words, pool geometry, and final prepared MV/projection under the finite4ULP/signed-zero allowance only.",
    "Bow receipts are all six actual external raw NW.g boundaries per Skeleton; not submitted-d byte equality or every possible internal auto-flush. Native failed-prefix after-states are distinct from the successful Source baseline.",
    "Actual Native unknown current color/normal serializes false/null and is not equated to Original undefined legacy raw attributes. Known attributes remain strict; only the explicitly audited fourteen-case Windows ordinary packet may supply actual known current-normal words on its exact observed GPU/driver profile pair. No other GL/state/geometry authority changes.",
    "Native Block dynamic class is unobserved: pair actual Registry Stone identity/alias and declaring methods, not an invented eo class observation.",
    "Native per-render/event counters pair actual selected Source entries/returns/table-trig and direct Entity/pivot call counts. Wolf9 pose, actual-GI.B-selected14 standing/15 sitting direct event stores and three stores per returned pivot are Code-qualified counts, not JDI field-modification events; biped pose_store counters remain Native-only. Wet brightness's nine actual IN.b(D)I descendants are retained Source-only, outside the model trig counter.",
    "Seven Source resource misses/hits pair raw names, decoded transfer and selected sampler enums. No full JDK decoder/private buffer or complete Engine cache-map graph claim.",
    "Direct living/post and RA String identities/values are paired with actual Engine, returned icon/texture and Source bind. Full live Native GL order is not emitted.",
    "Native authored pixels independently establish visibility. Original raw pixels and complete GL trace remain retained source-only.",
    "One actual Source12 SecureRandom nextLong belongs to the exact retained NIO ImageIO temp-file producer under JI.a(InputStream)1/JI.b197/RA118. It is retained separately, not paired to unrelated game streams or generalized into an entropy mask.",
    "Variant caller_inputs are declared typed Byte16/Integer18/wet/GM.bi operands, not draw-getter receipts; boxed scalar reference class/alias and complete actor raw fieldsets are unpaired. Actual V/GM.b getter outcomes and own-model/arm/Stack/RA identities are paired separately from observed JDI calls.",
    "Alternate atlas actual native transfer is compared to its own primary authored-atlas transfer. Source12 supplies actual Engine/key/lookup/bind/upload-parameter evidence; its deep ByteBuffer payload and actual cache-map size are unobserved, so neither receives a direct Source value-pair claim.",
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--observations", type=Path)
    parser.add_argument("--original-audit", type=Path)
    parser.add_argument("--device-observations", type=Path, help="Actual pinned v2 fourteen-case Windows ordinary capture; known normals only")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--audit-only", action="store_true")
    modes.add_argument("--native-output", type=Path, help="Read existing native.ndjson/stdout only; no dispatch")
    modes.add_argument("--execute", action="store_true", help="Run authored native contracts (default unless audit/replay is requested)")
    args = parser.parse_args()
    args.execute = not args.audit_only and args.native_output is None
    need(bool(args.observations) == bool(args.original_audit), "driver/observations-and-post-audit-required-together")
    need(not args.audit_only or args.observations, "driver/audit-only-needs-authority")
    need(not args.execute or args.executable, "driver/execute-needs-binary")
    need(not args.device_observations or args.observations, "driver/device-needs-primary-authority")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="remaining-draw-", dir=args.work_dir.resolve()))
    report = {"status": "FAIL", "scope": "FINITE_ORIGINAL_PAIR" if args.observations else "AUTHORED_NATIVE_ONLY",
              "native_execution_started": False, "comparison_limits": LIMITS,
              "mode": "audit-only" if args.audit_only else "replay" if args.native_output else "execute"}
    inputs, before, audit, native_stdout = [], {}, None, None
    source, device, device_report = None, None, None
    complete = False
    try:
        provenance, authored_paths = source_audit.audit_authored()
        inputs.extend(authored_paths)
        report["post_capture_public_source_binding"] = provenance["chronology"]
        if args.observations:
            paths, audit = source_audit.audit_source(args.observations, args.original_audit)
            inputs.extend(paths)
            report["original_current_audit"] = audit
        # Defined Native execution inputs are authored independently. No Source
        # observation is ever passed to the executable or to these generators.
        inputs.extend([Path(__file__), ROOT / "tests/client/render/test_remaining_mob_draw.c",
                       ROOT / "tests/tools/client/test_creature_draw.py", ROOT / "tests/tools/client/test_startup.py",
                       ROOT / "tests/tools/entity/test_mob_tick.py", ROOT / "tests/tools/entity/test_mob_construction.py"])
        startup = ROOT / "tests/client/startup"
        for name in ("test_application_startup.c", "controller_boundary_observer.h",
                     "application_startup_statistics_controls.inc", "application_startup_effects_controls.inc",
                     "application_startup_rendering_controls.inc"):
            inputs.append(startup / name)
        resources = startup / "fixtures"
        for directory in (ROOT / "src", ROOT / "include", resources):
            inputs.extend(p for p in directory.rglob("*") if p.is_file())
        executable = args.executable.resolve(strict=True) if args.executable else None
        if executable:
            inputs.append(executable)
        if args.execute:
            assets, world = out / "assets", out / "world"
            authored_assets(assets)
            shared.authored_world(world)
            inputs.extend(p for p in assets.rglob("*") if p.is_file())
            inputs.extend(world.rglob("*.mcr"))
        if args.native_output:
            native_directory = args.native_output.resolve(strict=True)
            inputs.extend(p for p in native_directory.iterdir() if p.is_file())
        source = source_v1.load(args.observations.parent) if args.observations else None
        if source:
            report["source_manifest_negative_controls"] = source_audit.manifest_negatives(args.original_audit)
            report["source_selected_inventory"] = {"draws": 14, "textures": 7, "lists": len(source["list_records"]),
                "live_Code": len(source["code_pins"]), "raw_event_counts": source["raw_event_counts"],
                "actual_image_io_entropy": source["image_io_entropy"]}
        if args.device_observations:
            paths, device, device_report = device_audit.audit_device(args.device_observations, source, Pair)
            inputs.extend(paths)
            report["device_original_current_audit"] = device_report
            report["device_manifest_negative_controls"] = device_audit.negatives(args.device_observations, source)
        before = {str(p.resolve()): digest(p) for p in sorted(set(inputs))}
        report["input_before_sha256"] = before
        if not args.audit_only:
            if args.execute:
                environment = os.environ.copy()
                environment["ALSOFT_DRIVERS"] = "null"
                for name in ("APPDATA", "HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
                    directory = out / name.lower()
                    directory.mkdir()
                    environment[name] = str(directory)
                command = [str(executable), str(resources), str(assets), str(world), str(out)]
                report["command"] = command
                report["native_execution_started"] = True
                run = subprocess.run(command, cwd=out, env=environment, capture_output=True, text=True, timeout=300)
                (out / "native.stdout.txt").write_text(run.stdout, encoding="utf-8")
                (out / "native.stderr.txt").write_text(run.stderr, encoding="utf-8")
                report.update(exit=run.returncode, stderr_file=str(out / "native.stderr.txt"), stderr_excerpt=run.stderr[:8000])
                need(run.returncode == 0, "native/executable-failed")
                native_directory = out
            else:
                report["native_replay_directory"] = str(native_directory)
            native_stdout = (native_directory / "native.stdout.txt").read_text()
            stderr = (native_directory / "native.stderr.txt").read_text()
            need(stderr in ("", "[ALSOFT] (WW) No capture backend available!\n"), "native/undeclared-diagnostic")
            report["native_checks"] = stdout_footer(native_stdout)
            groups = parse_native(native_directory / "native.ndjson")
            report["native_device"] = groups["device"][0]
            report["native_controls"] = groups["control"]
            report["pixels"] = pixel_checks(native_directory)
            if source:
                report["comparison"] = compare(groups, source, device)
                report["comparator_negative_controls"] = negatives(groups, source, device)
                if device:
                    report["matching_device"] = device_audit.matching_device(groups["device"][0], device["device"])
                    report["device_comparison_negative_controls"] = device_comparison_negatives(groups, source, device)
        complete = True
    except Exception as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)[:8000]}
    finally:
        after, errors = {}, []
        for name in before:
            try:
                after[name] = digest(Path(name))
            except Exception as error:
                errors.append({"path": name, "type": type(error).__name__, "message": str(error)[:800]})
        report["input_after_sha256"] = after
        report["input_audit_errors"] = errors
        unchanged = not errors and before == after
        report["inputs_unchanged"] = unchanged
        if audit is not None:
            try:
                _, final_audit = source_audit.audit_source(args.observations, args.original_audit)
                need(final_audit == audit, "source/final-current-audit-drift")
                report["final_original_current_audit"] = final_audit
            except Exception as error:
                report["final_source_audit_error"] = {"type": type(error).__name__, "message": str(error)[:8000]}
                unchanged = False
        if device_report is not None:
            try:
                _, final_device, final_device_report = device_audit.audit_device(args.device_observations, source, Pair)
                need(final_device == device and final_device_report == device_report, "device/final-current-audit-drift")
                report["final_device_original_current_audit"] = final_device_report
            except Exception as error:
                report["final_device_audit_error"] = {"type": type(error).__name__, "message": str(error)[:8000]}
                unchanged = False
        report["status"] = "PASS" if complete and unchanged else "FAIL"
        path = out / "run.json"
        path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    # PASS appears only after final current-input and Source audits and receipt.
    print(report["status"] + " remaining-mob " + report["mode"])
    print(path)
    if report["status"] != "PASS":
        print(json.dumps(report.get("error", report.get("final_source_audit_error", report.get("final_device_audit_error", {"message": "input drift"})))), file=sys.stderr)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
