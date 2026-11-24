"""Genuine protected GB bodies; complete returns and explicit explosion-entry prefixes."""
import argparse
import copy
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from test_mob_construction import RESOURCES

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / "tests/reference/entity/creeper_attack"
AUDIT_SCHEMA = "creeper-attack-postcapture-audit-v1"
SUMMARY = dict(cases=12, returned=9, source_null=1, explosion_prefixes=2,
               live_code=16, trace_records=282, negative_controls=4)


def need(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(paths):
    return {str(p.resolve()): digest(p) for p in sorted(set(paths))}


def local_path(name):
    if os.name == "nt" and re.match(r"^/mnt/[a-z]/", name):
        return Path(name[5].upper() + ":/" + name[7:])
    if os.name != "nt" and re.match(r"^[A-Za-z]:[\\/]", name):
        return Path("/mnt") / name[0].lower() / name[3:].replace("\\", "/")
    return Path(name)


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    prior = list(sys.path)
    try:
        spec.loader.exec_module(module)
    finally:
        sys.path[:] = prior
    return module


def artifact_inventory(directory):
    return {p.relative_to(directory).as_posix(): digest(p)
            for p in sorted(directory.rglob("*")) if p.is_file()}


def capture_inputs(observations, data):
    """Check the exact recorded closure and its current repository associations."""
    receipt = observations.with_name("capture.json")
    need(data.get("status") == "PASS" and data.get("inputs_unchanged") is True and
         data.get("input_before_sha256") == data.get("input_after_sha256") and
         data.get("helper_before_sha256") == data.get("helper_after_sha256") and
         data.get("observations") == SUMMARY, "audited original capture status/unchanged inputs/helpers/summary")
    recorded = data["input_after_sha256"]
    normalized = {name.replace("\\", "/"): (name, value) for name, value in recorded.items()}
    need(len(normalized) == len(recorded), "no aliased original input manifest keys")
    suffix = "/tests/reference/entity/creeper_attack/provenance.json"
    roots = [name[:-len(suffix)] for name in normalized if name.endswith(suffix)]
    need(len(roots) == 1, "one actual captured repository association")
    captured_root = roots[0]
    provenance = json.loads((PACKAGE / "provenance.json").read_text(encoding="utf-8"))
    relative = {"tests/reference/entity/creeper_attack/provenance.json": digest(PACKAGE / "provenance.json"),
                **{"tests/reference/entity/creeper_attack/" + name: value
                   for name, value in provenance["reference_files"].items()},
                **provenance["external_helpers"]}
    source_pins = json.loads((PACKAGE / "source-pins.json").read_text(encoding="utf-8"))
    expected = {captured_root + "/" + name: value for name, value in relative.items()}
    expected[captured_root + "/b1.7.3.jar"] = source_pins["client_jar_sha256"]
    tools = [name for name in normalized if re.search(r"/bin/(?:java|javac)(?:\.exe)?$", name)]
    need(len(tools) == 2 and {Path(name).stem for name in tools} == {"java", "javac"},
         "actual original Java/javac tool closure")
    need(set(normalized) == set(expected) | set(tools), "complete original input inventory; no missing or extra source/helper")
    for name, value in expected.items():
        need(normalized[name][1] == value, "recorded original input versus current frozen package: " + name)
    paths = [receipt, observations]
    for name, value in recorded.items():
        actual = local_path(name)
        need(actual.is_file() and digest(actual) == value, "current recorded original input: " + str(actual))
        paths.append(actual)
    # Also bind a current checkout if the original receipt was produced elsewhere.
    for name, value in relative.items():
        actual = ROOT / name
        need(actual.is_file() and digest(actual) == value, "current native checkout Source/helper association: " + name)
        paths.append(actual)
    helpers = data["helper_after_sha256"]
    need(set(helpers) == {name + ".class" for name in provenance["helper_classes"]},
         "complete compiled original helper inventory")
    parser = load_module("creeper_original_classfile", ROOT / "tests/reference/client/controller/classfile.py")
    for name, value in helpers.items():
        actual = receipt.parent / "classes" / name
        need(actual.is_file() and digest(actual) == value, "actual captured compiled helper bytes: " + name)
        need(parser.inspect_class(actual.read_bytes()) == provenance["helper_classes"][Path(name).stem],
             "actual helper Code matches immutable authored provenance: " + name)
        paths.append(actual)
    return paths


def verify_manifest(observations, data, proof):
    paths = capture_inputs(observations, data)
    receipt = observations.with_name("capture.json")
    need(proof.get("schema") == AUDIT_SCHEMA and proof.get("status") == "PASS" and
         proof.get("artifact_observation_scope") == "post-capture revalidation; no capture-start artifact hashes recorded",
         "independent post-capture audit identity and honest observation scope")
    need(proof.get("audit_producer_sha256") == {
        "tests/tools/entity/test_creeper_attack.py": digest(Path(__file__).resolve()),
        "tests/tools/entity/test_mob_construction.py": digest(Path(__file__).with_name("test_mob_construction.py"))},
        "current independent audit producer/helper binding")
    need(proof.get("capture_sha256") == digest(receipt) and
         proof.get("observations_sha256") == digest(observations), "original receipt/observation binding")
    before, after = proof.get("audit_before_sha256"), proof.get("audit_after_sha256")
    need(isinstance(before, dict) and before == after and before, "post-capture audit artifact stability")
    current = artifact_inventory(receipt.parent)
    need(before == current, "complete current original artifact inventory and digests")
    for name in before:
        pure = Path(name)
        need(not pure.is_absolute() and ".." not in pure.parts and "\\" not in name,
             "relative original artifact manifest path")
        paths.append(receipt.parent / name)
    required = {"capture.json", "observations.json", "normal/target.ndjson", "normal/class-init.log",
                "debugger/target.ndjson", "debugger/class-init.log", "debugger/checkpoints.ndjson",
                "classes/ConstructorJson.class", "classes/CreeperTarget.class", "classes/CreeperJdi.class"}
    need(required <= set(before), "all consumed raw transports/logs/classes bound before native execution")
    recorded_paths = [receipt, observations, *(local_path(name) for name in data["input_after_sha256"]),
                      *(receipt.parent / "classes" / name for name in data["helper_after_sha256"]),
                      *(receipt.parent / name for name in before)]
    need(proof.get("validation_summary") == SUMMARY and
         proof.get("input_before_sha256") == proof.get("input_after_sha256") == inventory(recorded_paths),
         "complete independently audited Source input and raw artifact closure")
    return paths


def replay_original(observations):
    """Revalidate raw outputs in a disposable copy; never write into the capture."""
    validator = load_module("creeper_original_validator", PACKAGE / "validate.py")
    pins = json.loads((PACKAGE / "source-pins.json").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="creeper-original-revalidate-") as temporary:
        directory = Path(temporary)
        for name in ("normal/target.ndjson", "normal/class-init.log", "debugger/target.ndjson",
                     "debugger/class-init.log", "debugger/checkpoints.ndjson"):
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(observations.parent / name, target)
        summary = validator.validate(directory, pins)
        need(summary == SUMMARY and json.loads((directory / "observations.json").read_text(encoding="utf-8")) ==
             json.loads(observations.read_text(encoding="utf-8")),
             "raw original transports regenerate the exact consumed observations")
    return summary


def default_audit(observations):
    return observations.parent.with_name(observations.parent.name + ".audit.json")


def write_original_audit(observations, destination):
    need(not destination.exists() and destination.parent != observations.parent,
         "new independent audit outside immutable original capture")
    data = json.loads(observations.with_name("capture.json").read_text(encoding="utf-8"))
    capture_inputs(observations, data)
    artifacts = artifact_inventory(observations.parent)
    paths = [observations.with_name("capture.json"), observations,
             *(local_path(name) for name in data["input_after_sha256"]),
             *(observations.parent / "classes" / name for name in data["helper_after_sha256"]),
             *(observations.parent / name for name in artifacts)]
    before = inventory(paths)
    summary = replay_original(observations)
    after = inventory(paths)
    need(before == after and artifacts == artifact_inventory(observations.parent),
         "post-capture revalidation observed no input/artifact drift")
    proof = dict(schema=AUDIT_SCHEMA, status="PASS",
                 audit_producer_sha256={
                     "tests/tools/entity/test_creeper_attack.py": digest(Path(__file__).resolve()),
                     "tests/tools/entity/test_mob_construction.py": digest(Path(__file__).with_name("test_mob_construction.py"))},
                 artifact_observation_scope="post-capture revalidation; no capture-start artifact hashes recorded",
                 capture_sha256=digest(observations.with_name("capture.json")),
                 observations_sha256=digest(observations), validation_summary=summary,
                 audit_before_sha256=artifacts, audit_after_sha256=artifacts,
                 input_before_sha256=before, input_after_sha256=after)
    # Stored filesystem names are translated at consumption, including this input inventory.
    destination.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8", newline="\n")
    return proof


def proof_localized(proof):
    proof = copy.deepcopy(proof)
    for key in ("input_before_sha256", "input_after_sha256"):
        proof[key] = {str(local_path(name).resolve()): value for name, value in proof[key].items()}
    return proof


def original_audit(observations, audit):
    receipt = observations.with_name("capture.json")
    data = json.loads(receipt.read_text(encoding="utf-8"))
    proof = proof_localized(json.loads(audit.read_text(encoding="utf-8")))
    paths = verify_manifest(observations, data, proof)
    replay_original(observations)
    return [audit, *paths], dict(capture_sha256=digest(receipt), observations_sha256=digest(observations),
                                 audit_sha256=digest(audit), artifacts=len(proof["audit_before_sha256"]),
                                 recorded_inputs=len(data["input_after_sha256"]),
                                 compiled_helpers=len(data["helper_after_sha256"]),
                                 artifact_observation_scope=proof["artifact_observation_scope"])


def manifest_negatives(observations, audit):
    data = json.loads(observations.with_name("capture.json").read_text(encoding="utf-8"))
    proof = proof_localized(json.loads(audit.read_text(encoding="utf-8")))
    mutations = []
    bad = copy.deepcopy(data); bad["status"] = "FAIL"; mutations.append(("failed capture", bad, proof))
    bad = copy.deepcopy(data)
    for key in ("input_before_sha256", "input_after_sha256"):
        bad[key].pop(next(name for name in bad[key] if name.endswith("CreeperTarget.java")))
    mutations.append(("omitted original input", bad, proof))
    bad = copy.deepcopy(data)
    for key in ("helper_before_sha256", "helper_after_sha256"):
        bad[key].pop("CreeperTarget.class")
    mutations.append(("omitted compiled helper", bad, proof))
    bad = copy.deepcopy(proof)
    for key in ("audit_before_sha256", "audit_after_sha256"):
        bad[key].pop("debugger/checkpoints.ndjson")
    mutations.append(("omitted raw trace", data, bad))
    bad = copy.deepcopy(proof)
    for key in ("audit_before_sha256", "audit_after_sha256"):
        bad[key]["normal/target.ndjson"] = "0" * 64
    mutations.append(("changed raw transport", data, bad))
    bad = copy.deepcopy(proof); bad["observations_sha256"] = "0" * 64
    mutations.append(("replaced observations", data, bad))
    bad = copy.deepcopy(proof); bad["audit_producer_sha256"]["tests/tools/entity/test_creeper_attack.py"] = "0" * 64
    mutations.append(("replaced audit producer", data, bad))
    for name, changed_data, changed_proof in mutations:
        rejected = False
        try:
            verify_manifest(observations, changed_data, changed_proof)
        except AssertionError:
            rejected = True
        need(rejected, "manifest negative control admitted " + name)
    return len(mutations)


def native_inputs(executable, resources):
    paths = [executable, Path(sys.executable).resolve(), Path(__file__).resolve(),
             Path(__file__).with_name("test_mob_construction.py")]
    # The genuine World fixture is textually included; collect every quoted
    # include recursively, including test-only helpers outside src/include.
    pending = [ROOT / "tests/entity/test_creeper_attack.c"]
    included = set()
    while pending:
        path = pending.pop().resolve(strict=True)
        if path in included:
            continue
        included.add(path)
        for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', path.read_text(encoding="utf-8"), re.MULTILINE):
            candidates = [path.parent / name, ROOT / "include" / name, ROOT / "src" / name,
                          ROOT / "src/client/startup" / name]
            actual = next((p for p in candidates if p.is_file()), None)
            need(actual is not None, "actual native fixture quoted include: " + name)
            pending.append(actual)
    paths.extend(included)
    # A conservative production closure also binds the code behind the linked
    # executable; it makes no claim that every captured source unit was entered.
    for directory in (ROOT / "src", ROOT / "include", PACKAGE, resources):
        paths.extend(p for p in directory.rglob("*") if p.is_file() and "__pycache__" not in p.parts)
    return paths


def final_inventory(paths):
    values = {}
    for path in sorted(set(paths)):
        try:
            values[str(path.resolve())] = digest(path)
        except OSError as error:
            values[str(path.resolve())] = "UNAVAILABLE:" + type(error).__name__
    return values


def complete_report(out, report):
    if not report["inputs_unchanged"]:
        report["status"] = "FAIL"
    (out / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    need(report["inputs_unchanged"], "native/source/helper/artifact input drift or failed pre-execution audit")


def profiles():
    with (PACKAGE / "profiles.tsv").open(newline="") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def parse_stdout(data):
    lines = data.decode("utf-8").splitlines()
    need(len(lines) == 25 and lines[:24] == ["151 recipes", "16 achievements"] * 12,
         "exact twelve genuine fixture setup output pairs")
    footer = re.fullmatch(r"Creeper attack: (615) checks;12 genuine protected-body profiles", lines[24])
    need(footer is not None, "exact native check receipt")
    return int(footer[1])


def expected(p, after):
    return dict(fuse=int(p["expected_fuse"] if after else p["fuse"]),
                previous=int(p["previous"]),
                attacking=bool(int(p["expected_attacking"] if after else p["attacking"])),
                dead=False, world_null=bool(int(p["null_world"])),
                direction=int(p["expected_direction"] if after else p["direction"]),
                powered=int(p["powered"]),
                dirty16=bool(after and int(p["expected_dirty"])), dirty17=False,
                dirty=bool(after and int(p["expected_dirty"])),
                x="4021000000000000", y="4050200000000000", z="4021000000000000")


def compare(rows, inputs, original=None):
    if len(rows) != len(inputs) or len(rows) != 12:
        raise AssertionError("twelve actual component rows required")
    checks = 0
    for index, (row, p) in enumerate(zip(rows, inputs)):
        if row["case"] != index or row["scenario"] != p["scenario"]:
            raise AssertionError("declared case order")
        for when in ("before", "after"):
            if row[when] != expected(p, when == "after"):
                raise AssertionError(f"case{index} independent {when} field oracle")
            checks += len(row[when])
        returned = p["projection"] == "return"
        explosion = p["projection"] == "explosion"
        if (row["returned"], row["native_policy"], row["source_exception"], row["pc"],
                row["sounds"], row["explosion"], row["radius_bits"]) != (
                returned, explosion, p["projection"] == "null", int(p["pc"]),
                int(p["sounds"]), explosion,
                ("40c00000" if int(p["powered"]) == 1 else "40400000") if explosion else "00000000"):
            raise AssertionError(f"case{index} exact return/source/native boundary")
        if not all(row[k] for k in ("entropy_unchanged", "guard_released", "null_input_preserved")):
            raise AssertionError("actual native lifetime/entropy controls")
        ctor = row["constructor"]
        if ctor["rebound"] or ctor["raw"]["fuse"] or ctor["raw"]["previous"] or ctor["raw"]["direction"] != -1:
            raise AssertionError("genuine raw constructor prefix")
        cold = ctor["math_holder_before"] == "cold"
        need(ctor["math_holder_before"] in ("cold", "ready") and ctor["math_holder_after"] == "ready" and
             (ctor["math_before"] is None) == cold, "actual independent native Math-holder context")
        constructors = 2 if cold else 1  # SN Random; actual holder Random only when COLD.
        if int(ctor["U_after"], 16) != (int(ctor["U_before"], 16) *
                                      pow(1181783497276652981, constructors, 1 << 64)) & ((1 << 64) - 1):
            raise AssertionError("actual observed cold/ready default constructor context, no entropy rebinding")
        if not cold and ctor["math_before"] == ctor["math_after"]:
            raise AssertionError("actual LS constructor shared-Math consumption")
        if original is not None:
            src = original["comparison_records"][index]
            if src["case"] != index or src["before"] != row["before"] or src["after"] != row["after"]:
                raise AssertionError(f"case{index} actual unchanged-Source selected field state")
            if src["sounds"] != row["sounds"] or src["pc"] != row["pc"]:
                raise AssertionError("actual JDI sound/source invocation order")
            checks += len(src["before"]) + len(src["after"]) + 2
        checks += 18
    return checks


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--observations", type=Path)
    parser.add_argument("--original-audit", type=Path,
                        help="independent post-capture audit; default is CAPTURE_DIRECTORY.audit.json")
    parser.add_argument("--write-original-audit", type=Path, metavar="OBSERVATIONS",
                        help="revalidate immutable original raw data and write a new external audit only")
    args = parser.parse_args()
    if args.write_original_audit:
        need(args.executable is None and args.observations is None, "audit-only mode does not run native code")
        observations = local_path(str(args.write_original_audit)).resolve(strict=True)
        destination = args.original_audit.resolve() if args.original_audit else default_audit(observations)
        write_original_audit(observations, destination)
        original_audit(observations, destination)
        need(manifest_negatives(observations, destination) == 7, "independent audit manifest controls")
        print("Original Creeper post-capture audit: " + str(destination))
        return
    need(args.executable is not None, "--executable is required for native verification")
    need(args.observations is not None or args.original_audit is None, "--original-audit requires --observations")
    executable = args.executable.resolve(strict=True)
    parent = args.work_dir.resolve() if args.work_dir else None
    if parent:
        parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="creeper-attack-", dir=parent))
    resources, worlds = out / "resources", out / "worlds"
    worlds.mkdir()
    for name, data in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for i in range(12):
        (worlds / f"case{i}").mkdir()
    paths = native_inputs(executable, resources)
    before = {}
    audit_receipt = None
    observations = local_path(str(args.observations)).resolve(strict=True) if args.observations else None
    audit = (args.original_audit.resolve(strict=True) if args.original_audit else default_audit(observations)) if observations else None
    report = {"status": "FAIL", "input_before_sha256": before, "native_execution_started": False,
              "world_inputs": "twelve actual empty fixture directories; no authored stored Chunk input",
              "constructor_context_limit": "Source observation prepares its Math holder with Class.forName. Native has no no-draw prepare API and observes its real independent COLD/READY context, emitting null for an uncreated before-stream. Raw contexts are not matched or reverse-inferred; the protected body must preserve all actual reached streams and default-U.",
              "original_artifact_limit": "The original capture recorded input/helper before/after hashes. Its output artifact hashes were independently recorded and raw-validated later; no capture-start output hashes are claimed."}
    try:
        original = None
        if observations:
            original_paths, audit_receipt = original_audit(observations, audit)
            paths.extend(original_paths)
            report["manifest_negative_controls"] = manifest_negatives(observations, audit)
            original = json.loads(observations.read_text(encoding="utf-8"))
            report["original_capture_audit"] = audit_receipt
        before = inventory(paths)
        report["input_before_sha256"] = before
        report["native_execution_started"] = True
        result = subprocess.run([str(executable), str(resources), str(worlds), str(out),
                                 str(PACKAGE / "profiles.tsv")], capture_output=True, timeout=180)
        (out / "native.stdout").write_bytes(result.stdout)
        (out / "native.stderr").write_bytes(result.stderr)
        if result.returncode or result.stderr:
            preview = result.stderr[:2048].decode(errors="replace")
            suffix = "\n[preview truncated]" if len(result.stderr) > 2048 else ""
            raise AssertionError(f"native exit{result.returncode}; full stderr: {out / 'native.stderr'}\n{preview}{suffix}")
        native_checks = parse_stdout(result.stdout)
        rows = [json.loads(line) for line in (out / "native.ndjson").read_text().splitlines()]
        report.update(status="PASS", native_checks=native_checks, comparisons=compare(rows, profiles(), original),
                      source_paired=original is not None,
                      scope="GB protected attack/unseen only. Nine actual returns, one actual NULL-World throwable, two evaluated FD explosion-entry prefixes. Genuine empty WorldAccess List gives sound-body credit, no audio callback credit. Raw constructor contexts are independent and never rebound. Original full explosion after-states remain separate.")
    except BaseException as error:
        report["error"] = type(error).__name__ + ": " + str(error)
        raise
    finally:
        report["input_after_sha256"] = final_inventory(paths)
        report["inputs_unchanged"] = bool(before) and before == report["input_after_sha256"]
        if audit_receipt is not None:
            try:
                _, final_audit = original_audit(observations, audit)
                report["inputs_unchanged"] = report["inputs_unchanged"] and final_audit == audit_receipt
            except Exception as error:
                report["inputs_unchanged"] = False
                report["final_audit_error"] = type(error).__name__ + ": " + str(error)
        complete_report(out, report)
    print(f"Creeper attack: {out / 'run.json'}")


if __name__ == "__main__":
    main()
