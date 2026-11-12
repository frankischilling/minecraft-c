"""JAR-free contracts for the actual private ZIP provider and authored archives."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ValueError("module " + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate(result):
    if result.returncode != 0 or result.stderr != b"":
        raise ValueError("ZIP native exit/stderr")
    rows = result.stdout.splitlines()
    if len(rows) != 1 or re.fullmatch(rb"texture ZIP: [1-9][0-9]* checks", rows[0]) is None:
        raise ValueError("ZIP native result")
    return int(rows[0].split()[2])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve()
    refs = here.parents[2] / "reference/client/texture_packs"
    inputs = load(refs / "inputs.py", "texture_inputs")
    # Reuse the existing reviewed process owner, rather than add another pipe
    # implementation. The ancestor search also permits this ignored stage to
    # be prepared without copying a public helper or a complete source graph.
    root = next((path for path in here.parents
                 if (path / "tests/reference/client/statistics/support.py").is_file()), None)
    if root is None:
        raise ValueError("reviewed stats capture helper not found")
    support = load(root / "tests/reference/client/statistics/support.py", "texture_capture")
    executable = args.executable.resolve(strict=True)
    work = support.fresh_directory(args.work_dir, "texture-zip-")
    fixtures = work / "inputs"
    inputs.zip_inputs(fixtures)
    before = support.fingerprint(executable)
    native = support.run_bounded([executable, fixtures], work, "native")
    checks = validate(native)
    support.require(native.capture_report.get("reader_errors") == {}, "native pipe errors")
    rejected = support.run_bounded([executable], work, "argc")
    support.require(rejected.returncode == 2 and rejected.stdout == b""
                    and rejected.stderr.splitlines()
                    == [b"ZIP inputs: expected authored fixture directory"], "exact argc rejection")
    support.require(rejected.capture_report.get("reader_errors") == {}, "argc pipe errors")
    controls = [
        subprocess.CompletedProcess([], 0, b"texture ZIP: 1 checks\n", b""),
        subprocess.CompletedProcess([], 1, native.stdout, b""),
        subprocess.CompletedProcess([], 139, native.stdout, b"AddressSanitizer: crash\n"),
        subprocess.CompletedProcess([], 0, native.stdout, b"runtime error: bounds\n"),
        subprocess.CompletedProcess([], 0, native.stdout, b"leak diagnostic\n"),
        subprocess.CompletedProcess([], 0, native.stdout + b"extra\n", b""),
        subprocess.CompletedProcess([], 0, b"texture ZIP: 0 checks\n", b""),
        subprocess.CompletedProcess([], 0, b"", b""),
        subprocess.CompletedProcess([], 0, b"texture ZIP: -1 checks\n", b""),
    ]
    support.require(validate(controls[0]) == 1, "positive parser control")
    for index, control in enumerate(controls[1:], 1):
        try:
            validate(control)
        except ValueError:
            continue
        raise ValueError("accepted malformed ZIP result " + str(index))
    support.require(support.fingerprint(executable) == before, "native product changed")
    report = {"status": "PASS", "scope": "actual ZIP native provider, authored archives; no JAR",
              "checks": checks, "controller_checks": len(controls), "product": before,
              "inputs": {path.name: support.fingerprint(path) for path in fixtures.iterdir()},
              "native": native.capture_report, "argc": rejected.capture_report}
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(work / "report.json")


if __name__ == "__main__":
    main()
