"""Exercise the ordered source World-frame prefix on a real GL3.3 Display."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "controller"))
from capture import capture, digest, ensure, require_success


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="world-frame-", dir=args.work_dir.resolve()))
    picker_path = HERE.parents[2] / "tools/client/test_picking.py"
    picker = module("world_frame_authored_worlds", picker_path)
    maths = module("world_frame_math_vectors", HERE / "capture_math.py")
    for name, data in picker.RESOURCES.items():
        target = out / "resources" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    picker.authored_world(out / "world")
    fogs = module("world_frame_fog_vectors", HERE / "capture_fog.py")
    fog_expected = json.loads((HERE / "fog-vectors.json").read_text())["rows"]
    fogs.rows("\n".join(fog_expected))
    expected = json.loads((HERE / "math-vectors.json").read_text())["rows"]
    maths.rows("\n".join(expected))
    inputs = [executable, picker_path, HERE / "run_native.py", HERE / "capture_math.py",
              HERE / "math-vectors.json", HERE / "capture_fog.py", HERE / "fog-vectors.json", HERE.parent / "controller/capture.py",
              HERE.parent / "controller/classfile.py"]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before}
    try:
        result = capture([str(executable), str(out / "resources"), str(out / "world")], out,
                         "native", 90, os.environ.copy(), out.resolve())
        require_success(result, empty_stderr=True)
        text = (out / result["stdout_file"]).read_text()
        ensure(maths.rows(text) == expected, "native/JDK fog blend vectors differ")
        ensure(fogs.rows(text) == fog_expected, "native/original full fog rows differ")
        ensure(re.fullmatch(r"Source World frame: [1-9][0-9]* checks; full fog and camera source prefix retained",
                            text.splitlines()[-1]), "native World-frame completion missing")
        report.update(status="PASS", math_vectors=8, fog_vectors=96, native=result,
                      scope="ordered native GL/picker/fog prefix and 96 original full-h numeric rows; no full-frame/visual claim")
    finally:
        report["input_after_sha256"] = {str(path.resolve()): digest(path) for path in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (out / "run.json").write_text(json.dumps(report, indent=2) + "\n",
                                      encoding="utf-8", newline="\n")
        ensure(same, "native World-frame inputs changed")
    print("Source World-frame prefix, 96 original full fog and 8 JDK math vectors passed; " + str(out / "run.json"))


if __name__ == "__main__":
    main()
