"""Run the native named-factory fixture with authored resources and bounded pipes."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile
import traceback

sys.dont_write_bytecode = True
import packet as P

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
TRANSPORT = P.C.C
RESOURCE_HELPER = ROOT / "tests/tools/entity/test_mob_construction.py"


def pin(path):
    raw = Path(path).read_bytes()
    return {"bytes": len(raw), "sha256": P.digest(raw)}


def authored_resources():
    spec = importlib.util.spec_from_file_location("named_factory_mob_inputs", RESOURCE_HELPER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    resources = module.RESOURCES
    P.need(type(resources) is dict and set(resources) == {
        "lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"},
        "exact authored Mob-construction resource census")
    P.need(all(type(value) is bytes for value in resources.values()), "authored resource bytes")
    return resources


def native_stdout(raw):
    """A transport gate, not an oracle for the fixture's individual assertions."""
    lines = raw.decode("utf8", errors="strict").splitlines()
    marker = re.fullmatch(r"PASS: ([1-9][0-9]*) named Entity factory/World binding checks",
                          lines[-1]) if lines else None
    P.need(marker is not None, "exact terminal named-factory check footer")
    # The fixture constructs its setup Registry, then a separate Registry for
    # its genuine constructor-allocation failure control.
    P.need(lines[:-1] == ["151 recipes", "16 achievements"] * 2,
           "exact two ordered Registry class-initialization banner pairs")
    return int(marker[1])


def validate_transport(directory, receipt, command):
    P.need(receipt["command"] == command, "receipt describes the invoked native command")
    P.need(P.same(P.parse((directory / "native.capture.json").read_bytes()), receipt),
           "stored raw native receipt matches the returned capture")
    P.need(receipt["timeout_seconds"] == 110 and receipt["stdout_limit"] == 4194304 and
           receipt["stderr_limit"] == 1048576 and receipt["stdout_file"] == "native.stdout" and
           receipt["stderr_file"] == "native.stderr", "exact native capture limits and stream names")
    P.C.transport(directory, receipt)
    P.need(receipt["stderr_bytes"] == 0, "native fixture stderr must be empty")
    return native_stdout((directory / receipt["stdout_file"]).read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    P.need(executable.is_file(), "native executable is a file")
    # The full publication audit also derives the C table from the original finite fields.
    P.audit(P.files(HERE))
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="named-factory-", dir=args.work_dir.resolve()))
    resources = output / "resources"
    authored = authored_resources()
    for name, raw in authored.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    worlds = [output / name for name in ("world-a", "world-b")]
    for world in worlds:
        world.mkdir()
    inputs = [executable, Path(__file__).resolve(), HERE / "packet.py", RESOURCE_HELPER,
              Path(TRANSPORT.__file__).resolve(), Path(P.C.__file__).resolve(),
              Path(P.C.R.__file__).resolve(), Path(P.C.I.__file__).resolve(),
              Path(P.C.R.N.__file__).resolve(), HERE / "publication.json",
              HERE / "native-observations.h", HERE / "finite.json",
              *[resources / name for name in sorted(authored)]]
    before = {str(path): pin(path) for path in inputs}
    command = [str(executable), str(resources), *map(str, worlds)]
    report = {"schema": "NATIVE_NAMED_FACTORY_V1", "status": "FAIL", "command": command,
              "inputs_before": before, "worlds_initially_empty": all(not any(w.iterdir()) for w in worlds),
              "comparison_scope": "Three original finite Pig projections and original E0 World-reference transition; additional fixture assertions are native source controls. No constructor entropy or abstract-Mob print parity.",
              "publication_sha256": before[str(HERE / "publication.json")]["sha256"],
              "native_header_sha256": before[str(HERE / "native-observations.h")]["sha256"]}
    try:
        P.need(report["worlds_initially_empty"], "both native World directories start empty")
        receipt = TRANSPORT.capture(command, output, "native", 110, working_directory=output)
        report["process"] = receipt
        report["checks"] = validate_transport(output, receipt, command)
        report["inputs_after"] = {str(path): pin(path) for path in inputs}
        P.need(report["inputs_after"] == before, "native executable, helpers, packet and authored resources unchanged")
        P.audit(P.files(HERE))
        report["inputs_unchanged"] = True
        report["status"] = "PASS"
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        report["traceback"] = traceback.format_exc()
        raise
    finally:
        try:
            report["inputs_after"] = {str(path): pin(path) for path in inputs}
            report["inputs_unchanged"] = report["inputs_after"] == before
            if not report["inputs_unchanged"]:
                report["status"] = "FAIL"
        except OSError as error:
            report["status"] = "FAIL"
            report["inputs_unchanged"] = False
            report["continuity_error"] = {"type": type(error).__name__, "message": str(error)}
        (output / "native-result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        print("Native named-factory artifacts: " + str(output), flush=True)
    P.need(report["status"] == "PASS", "native input continuity failed after capture validation")
    print("PASS: " + str(report["checks"]) + " named Entity factory/World binding checks")


if __name__ == "__main__":
    main()
