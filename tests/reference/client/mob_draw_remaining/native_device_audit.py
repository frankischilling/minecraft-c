"""Pinned same-device ordinary evidence for known current normals only.

The separate Windows capture is not JDI or new Code authority. Its complete
packet and actual tools are audited before any target record is consumed.
"""

import copy
import json
from pathlib import Path

import native_source_audit as source_audit

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
need, digest = source_audit.need, source_audit.digest


def frozen_values(receipt=None):
    provenance, _ = source_audit.audit_authored()
    authority = provenance["external_windows_authority"]
    if receipt is None:
        return authority
    receipt = Path(receipt).resolve()
    capture_hash(digest(receipt), authority["capture_sha256"])
    data = json.loads(receipt.read_text())
    return {"capture_sha256": authority["capture_sha256"], "source_device": authority["source_device"],
            "native_device": authority["native_device"], "input_roles": data["input_roles"],
            "input_pins": data["input_after_sha256"], "normal_artifacts": data["artifacts"],
            "all_artifacts": data["all_artifacts_sha256"]}


def capture_hash(actual, expected):
    need(actual == expected, "device/capture-file-drift")


def manifest_values(data, frozen, primary):
    need(data["status"] == "PASS" and data["ordinary_only"] is True and
         data["no_JDI_credit"] is True and data["no_new_Code_credit"] is True and
         data["inputs_unchanged"] is True and data["exit"] == 0,
         "device/actual-ordinary-completion")
    need(data["original_linux_capture_sha256"] == primary["receipt_sha256"] and
         data["original_linux_analysis_sha256"] == primary["analysis_sha256"] and
         data["original_linux_audit_sha256"] == primary["audit_sha256"],
         "device/same-frozen-v2-authority")
    need(len(data["input_roles"]) == 27 and data["input_roles"] == frozen["input_roles"] and
         len(set(data["input_roles"].values())) == 27,
         "device/exact27-role-inventory")
    need(len(data["input_before_sha256"]) == 27 and
         data["input_before_sha256"] == data["input_after_sha256"] == frozen["input_pins"] and
         set(data["input_roles"].values()) == set(data["input_after_sha256"]),
         "device/exact27-input-pins")
    need(data["source_input_before_sha256"] == data["source_input_after_sha256"] == primary["capture_inputs"] and
         len(data["source_input_after_sha256"]) == 30,
         "device/exact-Source30-input-pins")
    for name in ("helper_before_sha256", "helper_after_sha256", "source_helper_before_sha256", "source_helper_after_sha256"):
        need(data[name] == primary["helpers"] and len(data[name]) == 4,
             "device/exact-four-identical-helpers")
    need(set(data["artifacts"]) == set(frozen["normal_artifacts"]) and len(data["artifacts"]) == 69 and
         data["exact_normal_artifact_inventory"] == sorted(frozen["normal_artifacts"]),
         "device/exact69-normal-artifact-names")
    need(data["artifacts"] == frozen["normal_artifacts"], "device/normal-artifact-pins")
    need(set(data["all_artifacts_sha256"]) == set(frozen["all_artifacts"]) and
         len(data["all_artifacts_sha256"]) == 83,
         "device/exact83-packet-artifact-names")
    need(data["all_artifacts_sha256"] == frozen["all_artifacts"] and
         data["exact_all_artifact_inventory"] == sorted(frozen["all_artifacts"]), "device/packet-artifact-pins")
    for name, metadata in data["artifacts"].items():
        need(data["all_artifacts_sha256"].get(name) == metadata,
             "device/normal-and-full-manifest-agree")
    need("normal/target.ndjson" in data["artifacts"], "device/consumed-target-required")
    need(data["devices"] == [frozen["source_device"]], "device/exact-source-context")
    need(data["draws"] == 14 and data["textures"] == 7 and len(data["constructor_contexts"]) == 8 and len(data["constructor_before_contexts"]) == 8 and data["original_linux_live_Code_count"] == 213 and
         len(data["bindings"]) == 253 and len(data["initialized_original_classes"]) == 245 and
         data["network_guard"] == [{"event": "network_guard", "blocked_attempts": []}],
         "device/complete-fourteen-call-context")


def matching_device(native, original):
    frozen = frozen_values()
    need(native == frozen["native_device"] and original == frozen["source_device"],
         "device/exact-native-source-profile")
    need(native["vendor"] == original["vendor"] and native["renderer"] == original["renderer"] and
         native["version"].endswith(" NVIDIA 581.42") and original["version"].endswith(" NVIDIA 581.42"),
         "device/exact-vendor-renderer-driver")
    return {"native": native, "ordinary_original": original, "driver": "NVIDIA 581.42",
            "scope": "Exact observed core3.3/compatibility4.6 context pair on the same vendor/renderer/driver"}


def audit_device(receipt, source, pair_type):
    receipt = Path(receipt).resolve()
    frozen = frozen_values(receipt)
    primary = source_audit.frozen_values(Path(source["external_base"]))
    need(receipt.name == "capture.json", "device/exact-capture-file-layout")
    # The receipt is pinned before semantic parse. The full manifest below is
    # checked before any target NDJSON is read, including its consumed file.
    capture_hash(digest(receipt), frozen["capture_sha256"])
    data = json.loads(receipt.read_text())
    manifest_values(data, frozen, primary)
    base = receipt.parent
    need({p.relative_to(base).as_posix() for p in base.rglob("*") if p.is_file()} ==
         set(frozen["all_artifacts"]) | {"capture.json"}, "device/current-exact-packet-files")
    paths = [receipt, HERE / "provenance.json", HERE / "native_device_audit.py"]
    current, local = source_audit.current_hashes(data["input_after_sha256"])
    paths.extend(local)
    source_current, source_local = source_audit.current_hashes(data["source_input_after_sha256"])
    paths.extend(source_local)
    for name, metadata in frozen["all_artifacts"].items():
        path = (base / name).resolve()
        need(path.is_relative_to(base) and path.is_file() and path.stat().st_size == metadata["bytes"] and
             digest(path) == metadata["sha256"], "device/current-artifact-drift:" + name)
        paths.append(path)
    need({p.name for p in (base / "classes").iterdir()} == set(primary["helpers"]),
         "device/current-exact-copied-helper-files")
    for name, expected in primary["helpers"].items():
        need(digest(base / "classes" / name) == expected, "device/current-helper-drift:" + name)
    setup = json.loads((base / "input-freeze.json").read_text())
    need(setup == {"status": "FROZEN_BEFORE_ORIGINAL_EXECUTION", "ordinary_only": True,
         "input_roles": data["input_roles"], "input_sha256": data["input_after_sha256"],
         "source_input_sha256": primary["capture_inputs"], "source_helper_sha256": primary["helpers"],
         "copied_helper_sha256": primary["helpers"], "timeout_seconds": 300},
         "device/actual-capture-start-input-freeze")
    rows = [json.loads(line) for line in (base / "normal/target.ndjson").read_text().splitlines()]
    expected_counts = {"resource_input": 8, "device": 1, "constructor_before": 8, "constructor": 8,
                       "texture_lookup": 7, "draw": 14, "network_guard": 1}
    need({name: sum(row["event"] == name for row in rows) for name in expected_counts} == expected_counts and
         len(rows) == sum(expected_counts.values()), "device/all-declared-raw-record-groups")
    devices = [row for row in rows if row["event"] == "device"]
    draws = [row for row in rows if row["event"] == "draw"]
    textures = [row for row in rows if row["event"] == "texture_lookup"]
    need(devices == data["devices"] and [row["index"] for row in draws] == list(range(14)) and
         textures == source["texture_records"], "device/actual-device-calls-identical-texture-inputs")
    pair = pair_type()
    for case, row in enumerate(draws):
        expected = source["draw_records"][case]
        need(row["before"]["entropy"] == row["after"]["entropy"] and
             row["before"]["actor"] == row["after"]["actor"] and
             row["before"]["camera"] == row["after"]["camera"],
             "device/no-own-process-Entity-or-stream-mutation")
        for name in ("input", "kind", "declared_getters", "outcome", "failure", "nw_position", "nw_words", "gl_error"):
            pair.value(row[name], expected[name], f"device/Source{case}/" + name)
        for phase in ("before", "after"):
            a, b = row[phase], expected[phase]
            pair.graph(a["graph"], b["graph"], f"device/Source{case}/{phase}/graph")
            for name in ("parts", "pool", "pool_storage"):
                pair.value(a[name], b[name], f"device/Source{case}/{phase}/" + name)
        need(data["draw_gl"][case] == {"case": case, "before": row["before"]["gl"], "after": row["after"]["gl"]},
             "device/receipt-GL-is-actual-raw-target")
    # Return only the finite authorized field. Raw complete records remain in
    # the pinned packet and are never a substitute for Linux geometry or GL.
    authority = {"device": devices[0], "normals": {
        (case, phase): draws[case][phase]["gl"]["normal"] for case in range(14) for phase in ("before", "after")}}
    return paths, authority, {"capture": str(receipt), "capture_sha256": digest(receipt),
        "scope": "Actual same-device ordinary known current-normal supplement only; no JDI/new Code/other GL authority",
        "source_device": devices[0], "Windows_input_count": 27, "Source_input_count": 30, "compiled_helpers": 4,
        "normal_artifacts": 69, "full_packet_artifacts": 83, "actual_original_bindings": 253,
        "actual_initialized_classes": 245, "cross_original_graph_state_checks": pair.checks,
        "Windows_current_input_sha256": current, "Source_current_input_sha256": source_current,
        "normal_profile_raw": data["draw_gl"]}


def negatives(receipt, source):
    frozen = frozen_values(receipt)
    primary = source_audit.frozen_values(Path(source["external_base"]))
    original = json.loads(Path(receipt).read_text())
    results = []
    for name, expected in (("changed_capture", "device/capture-file-drift"),
                            ("omitted_target", "device/exact69-normal-artifact-names"),
                            ("changed_target_hash", "device/normal-artifact-pins"),
                            ("omitted_JAR_input", "device/exact27-input-pins"),
                            ("changed_helper", "device/exact-four-identical-helpers"),
                            ("native_driver_mismatch", "device/exact-native-source-profile")):
        changed = copy.deepcopy(original)
        try:
            if name == "changed_capture":
                capture_hash("0" * 64, frozen["capture_sha256"])
            elif name == "native_driver_mismatch":
                native = copy.deepcopy(frozen["native_device"])
                native["version"] = "3.3.0 NVIDIA 000.00"
                matching_device(native, frozen["source_device"])
            else:
                if name == "omitted_target":
                    del changed["artifacts"]["normal/target.ndjson"]
                elif name == "changed_target_hash":
                    changed["artifacts"]["normal/target.ndjson"]["sha256"] = "0" * 64
                elif name == "omitted_JAR_input":
                    del changed["input_after_sha256"][changed["input_roles"]["client_jar"]]
                else:
                    changed["helper_after_sha256"][next(iter(primary["helpers"]))] = "0" * 64
                manifest_values(changed, frozen, primary)
        except AssertionError as error:
            need(str(error) == expected, "device/negative-wrong-category:" + name + ":" + str(error))
            results.append({"name": name, "rejected": True, "category": expected})
        else:
            raise AssertionError("device/negative-admitted:" + name)
    return results
