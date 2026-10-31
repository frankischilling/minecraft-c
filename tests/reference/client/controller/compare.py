"""Compare a fresh completed native controller run with unchanged original bodies."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from capture import ensure, validate_native_result
from capture_reference import main as capture_original


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--natives", type=Path, required=True)
    parser.add_argument("--java", required=True)
    parser.add_argument("--javac", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    options = parser.parse_args()
    options.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="controller-comparison-", dir=options.work_dir))
    native = output / "native"
    command = [sys.executable, "-B", str(Path(__file__).with_name("run_native.py")),
               "--executable", str(options.executable.resolve(strict=True)),
               "--output", str(native.resolve())]
    # The native observer owns its bounded process and complete output capture.
    # A failed observer must stop before original correspondence is reported.
    subprocess.run(command, check=True)
    validate_native_result(native / "native.stdout", native / "run.json", "controller")
    original = output / "original"
    capture_original([
        "--client-jar", str(options.client_jar.resolve(strict=True)),
        "--libraries", str(options.libraries.resolve(strict=True)),
        "--natives", str(options.natives.resolve(strict=True)),
        "--java", options.java, "--javac", options.javac,
        "--output", str(original.resolve()),
        "--native-stdout", str((native / "native.stdout").resolve()),
        "--native-report", str((native / "run.json").resolve()),
    ])
    report = json.loads((original / "capture.json").read_text(encoding="utf-8"))
    ensure(report.get("status") == "PASS" and report.get("native_compared") is True,
           "controller original/native comparison did not complete")
    ensure(report.get("states") == 20, "controller original/native state inventory differs")
    print("Controller correspondence: 20 original/native records; report=" +
          str(original / "capture.json"))


if __name__ == "__main__":
    main()
