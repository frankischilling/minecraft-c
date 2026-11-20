"""Real lava animation and shared Math owner contracts, authored inputs only."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / "reference/client/lava_animation/support.py"
    spec = importlib.util.spec_from_file_location("lava_support", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("lava support unavailable")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="lava-animation-", dir=args.work_dir.resolve()))
    resources = root / "resources"
    support.resources_at(resources)
    executable = args.executable.resolve(strict=True)
    trace = root / "native.trace"
    result = support.run([executable, resources, trace], root, "native")
    checks = support.native_success(result)
    support.require(trace.stat().st_size <= support.MAX_TRACE_BYTES, "trace byte bound")
    observation = support.inspect_trace(trace.read_bytes())
    observation.pop("lines")
    for label, command in (("argc", [executable]), ("output", [executable, resources, root])):
        rejected = support.run(command, root, label)
        support.require(rejected.returncode == 2 and rejected.stdout == b"" and
            rejected.stderr.replace(b"\r\n", b"\n") == b"Lava animation input: resources output-file\n",
            "exact malformed admission; crash/sanitizer output is not rejection")
    report = {"status": "PASS", "checks": checks, "observation": observation,
              "capture": result.capture_report, "work_directory": str(root),
              "executable": str(executable), "executable_before": result.binary_before,
              "executable_after": result.binary_after,
              "products": {path.name: support.pin(path) for path in root.iterdir() if path.is_file()}}
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
