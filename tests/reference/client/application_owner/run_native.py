"""Run actual authored owner contracts and normal records without a JAR/JVM."""
import argparse
import json
import os
from pathlib import Path
import tempfile
from support import capture, fingerprint, require, source_binding, validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="application-owner-", dir=args.work_dir.resolve()))
    executable = args.executable.resolve(strict=True)
    bound = source_binding()
    before = fingerprint(executable)
    report = {"status": "RUNNING", "source_binding": bound, "executable": str(executable),
              "executable_before": before, "profiles": {}}
    try:
        for mode in ["contracts", "original"]:
            command = [executable] + (["original"] if mode == "original" else [])
            process = capture(command, out, mode, environment=os.environ.copy())
            stdout = (out / process["stdout_file"]).read_bytes()
            stderr = (out / process["stderr_file"]).read_bytes()
            records = validate(process, stdout, stderr, mode)
            report["profiles"][mode] = {"capture": process, "records": len(records)}
        require(source_binding() == bound and fingerprint(executable) == before,
                "unchanged source/native executable through actual execution")
        report.update(status="PASS", executable_after=fingerprint(executable))
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "work_directory": str(out)}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
