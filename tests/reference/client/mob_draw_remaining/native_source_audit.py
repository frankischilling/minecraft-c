"""Audit explicitly supplied immutable Original evidence and current authored bytes.

The public helper bindings were made after the historical capture. They are
not presented as files that existed at that capture's start.
"""

import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def need(value, label):
    if not value:
        raise AssertionError(label)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            h.update(block)
    return h.hexdigest()


def local_path(name):
    name = str(name)
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", name):
        return Path(name[5].upper() + ":/" + name[7:])
    if os.name != "nt" and re.match(r"^[A-Za-z]:[\\/]", name):
        return Path("/mnt") / name[0].lower() / name[3:].replace("\\", "/")
    return Path(name)


def current_hashes(pins):
    current, foreign, paths = {}, [], []
    for name in pins:
        path = local_path(name)
        if path.is_file():
            current[name] = digest(path)
            paths.append(path)
        else:
            need(os.name == "nt" and name.startswith("/usr/"), "source/input-missing:" + name)
            foreign.append(name)
    if foreign:
        code = ("import hashlib,json,sys;from pathlib import Path;"
                "print(json.dumps({p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()"
                " for p in json.load(sys.stdin)}))")
        run = subprocess.run(["wsl.exe", "--distribution", "Ubuntu", "--exec", "/usr/bin/python3", "-c", code],
                             input=json.dumps(foreign), capture_output=True, text=True, timeout=60)
        need(run.returncode == 0 and not run.stderr, "source/foreign-runtime-readonly-audit")
        current.update(json.loads(run.stdout))
    need(current == pins, "source/current-hash-drift")
    return current, paths


def audit_authored():
    provenance = json.loads((HERE / "provenance.json").read_text())
    need(provenance["schema"] == 1 and len(provenance["authored_inputs_sha256"]) == 5 and
         len(provenance["compiled_helpers_sha256"]) == 4 and
         provenance["chronology"]["public_sources_were_capture_start_inputs"] is False,
         "source/declared-post-capture-rehoming")
    for name, expected in provenance["authored_inputs_sha256"].items():
        need(name in ("ConstructorJson.java", "RemainingDrawJdi.java", "RemainingDrawTarget.java", "profiles.tsv", "preparation.tsv") and
             digest(HERE / name) == expected, "source/current-authored-input-drift:" + name)
    need(digest(HERE / "source-pins.json") == provenance["source_pins_sha256"], "source/static-pins-file-drift")
    pins = json.loads((HERE / "source-pins.json").read_text())
    need(len(pins["selected_live_Code"]) == 213 and len(pins["original_classes"]) == 245 and
         len(pins["original_bound_classes"]) == 253,
         "source/static-finite-Code-inventory")
    selected = set()
    for code in pins["selected_live_Code"]:
        key = code["class"], code["method"], code["descriptor"]
        need(key not in selected and set(code) == {"class", "method", "descriptor", "bytes", "sha256"},
             "source/static-selected-method-identity")
        selected.add(key)
        cls = pins["original_classes"][code["class"].replace(".", "/")]
        method = [m for m in cls["methods"] if m["name"] == code["method"] and m["descriptor"] == code["descriptor"]]
        need(len(method) == 1 and method[0]["code_bytes"] == code["bytes"] and
             method[0]["code_sha256"] == code["sha256"], "source/static-selected-method-Code-pin")
    return provenance, [p for p in HERE.iterdir() if p.is_file()]


def frozen_values(base):
    """Construct a trusted manifest only after receipt/audit bytes are pinned."""
    provenance, _ = audit_authored()
    authority, base = provenance["external_original_authority"], Path(base).resolve()
    receipt_path = base / authority["capture_relative"]
    audit_path = base / authority["post_capture_audit_name"]
    need(digest(receipt_path) == authority["capture_sha256"], "source/receipt-file-drift")
    need(digest(audit_path) == authority["post_capture_audit_sha256"], "source/audit-file-drift")
    receipt, audit = json.loads(receipt_path.read_text()), json.loads(audit_path.read_text())
    return {"analysis_sha256": authority["analysis_sha256"], "receipt_sha256": authority["capture_sha256"],
            "audit_sha256": authority["post_capture_audit_sha256"], "capture_inputs": receipt["input_after_sha256"],
            "helpers": provenance["compiled_helpers_sha256"], "v2_artifacts": receipt["artifacts_sha256"],
            "packet_artifacts": audit["artifact_sha256"], "older_packets": audit["earlier_public_packet_pins_current"],
            "actual_image_io_entropy": audit["executions"]["v2"]["draw_scope_random_events"]}


def manifest_values(audit, receipt, frozen):
    need(audit["status"] == "PRIVATE_ORIGINAL_PROTOTYPE_FROZEN" and
         audit["public_files_written"] is audit["native_build"] is audit["Windows_display"] is False,
         "source/private-scope")
    need(set(audit["artifact_sha256"]) == set(frozen["packet_artifacts"]), "source/packet-artifact-names")
    need(audit["artifact_sha256"] == frozen["packet_artifacts"], "source/packet-artifact-hashes")
    need(audit["earlier_public_packet_pins_current"] == frozen["older_packets"], "source/older-packet-schema")
    v2 = audit["executions"]["v2"]
    need(v2["inputs"] == 30 and v2["helpers"] == 4 and v2["artifacts"] == 149 and
         v2["live_Code"] == 213 and v2["closure"] == {"normal": 245, "debugger": 245} and
         v2["draw_scope_random_events"] == frozen["actual_image_io_entropy"], "source/v2-finite-inventory")
    need(receipt["status"] == "OBSERVED_FOURTEEN_DECLARED_DRAWS" and receipt["inputs_unchanged"] is True and
         receipt["default_constructor_entropy_rebound"] is False and
         receipt["normal_exit"] == receipt["debugger_exit"] == 0, "source/v2-observed-receipt")
    need(set(receipt["input_after_sha256"]) == set(frozen["capture_inputs"]), "source/input-names")
    need(receipt["input_before_sha256"] == receipt["input_after_sha256"] == frozen["capture_inputs"],
         "source/input-hashes")
    need(set(receipt["helper_after_sha256"]) == set(frozen["helpers"]), "source/helper-names")
    need(receipt["helper_before_sha256"] == receipt["helper_after_sha256"] == frozen["helpers"],
         "source/helper-hashes")
    need(set(receipt["artifacts_sha256"]) == set(frozen["v2_artifacts"]), "source/v2-artifact-names")
    need(receipt["artifacts_sha256"] == frozen["v2_artifacts"], "source/v2-artifact-hashes")
    for required in ("normal/target.ndjson", "debugger/target.ndjson", "debugger/checkpoints.ndjson"):
        need(required in receipt["artifacts_sha256"], "source/consumed-artifact-required:" + required)


def audit_source(observations, original_audit):
    observations, original_audit = Path(observations).resolve(), Path(original_audit).resolve()
    provenance, paths = audit_authored()
    base = observations.parent
    frozen = frozen_values(base)
    need(observations.name == "analysis-v2.json" and
         original_audit == (base / "final-audit.json").resolve(), "source/exact-frozen-v2-packet")
    # Audit is a post-capture current-packet inventory, not an output manifest
    # that existed at capture start. Pin its bytes before parsing any records.
    need(digest(original_audit) == frozen["audit_sha256"], "source/audit-file-drift")
    receipt_path = base / "capture-v2/receipt.json"
    need(digest(receipt_path) == frozen["receipt_sha256"], "source/receipt-file-drift")
    audit = json.loads(original_audit.read_text())
    receipt = json.loads(receipt_path.read_text())
    manifest_values(audit, receipt, frozen)
    need({p.relative_to(base).as_posix() for p in base.rglob("*") if p.is_file()} ==
         set(frozen["packet_artifacts"]) | {"final-audit.json"}, "source/current-packet-file-inventory")
    paths.extend([original_audit, receipt_path])
    packet_current = {}
    for name, expected in audit["artifact_sha256"].items():
        path = (base / name).resolve()
        need(path.is_relative_to(base.resolve()), "source/packet-path-escape")
        need(path.is_file() and digest(path) == expected, "source/artifact-drift:" + name)
        packet_current[name] = expected
        paths.append(path)
    need(digest(observations) == frozen["analysis_sha256"] == audit["executions"]["v2"]["analysis_sha256"],
         "source/analysis-file-drift")
    need(digest(receipt_path) == audit["executions"]["v2"]["receipt_sha256"], "source/receipt-binding")
    current, local = current_hashes(receipt["input_after_sha256"])
    paths.extend(local)
    helper_paths = [base / "capture-v2/classes" / name for name in frozen["helpers"]]
    need({p.name: digest(p) for p in helper_paths} == frozen["helpers"], "source/current-helper-drift")
    paths.extend(helper_paths)
    for packet, names in frozen["older_packets"].items():
        directory = ROOT / "tests/reference/client" / packet
        need({p.name for p in directory.iterdir() if p.is_file()} == set(names), "source/older-file-names")
        for name, expected in names.items():
            path = directory / name
            need(digest(path) == expected, "source/older-file-drift:" + packet + "/" + name)
            paths.append(path)
    return paths, {"scope": "Explicit external historical v2 packet; public authored binding is post-capture",
                   "capture_start_outputs_manifest": False,
                   "analysis_sha256": digest(observations), "receipt_sha256": digest(receipt_path),
                   "post_capture_audit_sha256": digest(original_audit), "capture_input_count": 30,
                   "compiled_helpers": 4, "v2_post_capture_artifacts": 149,
                   "whole_private_packet_artifacts": len(packet_current),
                   "current_declared_input_sha256": current, "helper_sha256": frozen["helpers"],
                    "packet_artifact_sha256": packet_current,
                    "post_capture_authored_input_sha256": provenance["authored_inputs_sha256"]}


def manifest_negatives(original_audit):
    base = Path(original_audit).resolve().parent
    frozen = frozen_values(base)
    audit = json.loads(Path(original_audit).read_text())
    receipt = json.loads((base / "capture-v2/receipt.json").read_text())
    trials = []
    for name, category in (("packet-empty", "source/packet-artifact-names"),
                           ("consumed-target-omitted", "source/v2-artifact-names"),
                           ("jar-input-omitted", "source/input-names"),
                           ("compiled-helper-sha", "source/helper-hashes")):
        a, r = copy.deepcopy(audit), copy.deepcopy(receipt)
        if name == "packet-empty":
            a["artifact_sha256"] = {}
        elif name == "consumed-target-omitted":
            r["artifacts_sha256"].pop("normal/target.ndjson")
        elif name == "jar-input-omitted":
            r["input_after_sha256"].pop(next(k for k in r["input_after_sha256"] if k.endswith("/b1.7.3.jar")))
        else:
            key = next(iter(r["helper_after_sha256"]))
            r["helper_after_sha256"][key] = "0" * 64
        try:
            manifest_values(a, r, frozen)
        except AssertionError as error:
            need(str(error) == category, "source/negative-wrong-category:" + name + ":" + str(error))
            trials.append({"name": name, "rejected": True, "category": category})
        else:
            raise AssertionError("source/negative-admitted:" + name)
    return trials
