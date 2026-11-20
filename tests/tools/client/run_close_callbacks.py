#!/usr/bin/env python3
"""Run the close-callback contract in a unique retained capture directory."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile

CONTROLLER_SHA256 = "240a4c8e1e40aff4da544677e8f90e3cc76d3eba0a8db4ac9f57af6a06ac34ba"


def pin(path):
    data = path.read_bytes()
    return len(data), hashlib.sha256(data).hexdigest()


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    args = parser.parse_args()
    controller = Path(__file__).with_name("test_close_callbacks.py").resolve(strict=True)
    executable = args.executable.resolve(strict=True)
    own = Path(__file__).resolve(strict=True)
    require(executable.is_file(), "native executable must be an existing regular file")
    require(pin(controller)[1] == CONTROLLER_SHA256, "unexpected close-callback controller")
    before = {path: pin(path) for path in (own, controller, executable)}
    args.work_dir.mkdir(parents=True, exist_ok=True)
    retained = Path(tempfile.mkdtemp(prefix="close-callbacks-", dir=args.work_dir))
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("source_close_controller", controller)
    require(spec is not None and spec.loader is not None, "controller import spec")
    module = importlib.util.module_from_spec(spec)
    old_argv = sys.argv
    try:
        spec.loader.exec_module(module)
        sys.argv = [str(controller), "--executable", str(executable), "--source-root", str(args.source_root),
                    "--work-dir", str(retained / "capture")]
        result = module.main()
        require(type(result) is int and result in (0, 1), "invalid controller result")
    finally:
        sys.argv = old_argv
        require({path: pin(path) for path in before} == before, "wrapper/controller/executable changed")
    return result


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("Close callback launcher: " + str(error), file=sys.stderr)
        raise SystemExit(1)
