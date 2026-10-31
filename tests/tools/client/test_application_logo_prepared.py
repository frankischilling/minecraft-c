"""Opt-in real-window PREPARED-CALLEE MC.x controls; no original/JVM execution.

Every receiver retains the genuine failed xi constructor at MC337/CJ48. These
callee calls do not credit Statistics completion or the later normal caller.
Windows uses child-only APPDATA. POSIX uses the real native home; registration
requires the explicit default-directory test option on both platforms.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ("success", "null-settings", "null-engine", "fp", "late-fp")

def require(value, message):
    if not value:
        raise ValueError(message)

def pin(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}

def tree(root):
    return {p.relative_to(root).as_posix(): pin(p) for p in sorted(root.rglob("*")) if p.is_file()}

def source_bindings():
    suffixes = (".c", ".h", ".cpp", ".cxx", ".inc", ".cmake", ".vert", ".frag", ".glsl")
    paths = [p for name in ("include", "src", "tests", "cmake")
             for p in (ROOT / name).rglob("*") if p.is_file() and
             (p.suffix in suffixes or p.name == "CMakeLists.txt")]
    paths.append(ROOT / "CMakeLists.txt")
    paths += [Path(__file__), ROOT / "tests/reference/client/statistics/support.py"]
    return {p.relative_to(ROOT).as_posix(): pin(p) for p in sorted(paths)}

def admit(capture, stdout, stderr, executable, mode):
    require(mode in MODES, "exact prepared-callee mode")
    require(type(capture) is dict and type(capture.get("exit")) is int and capture["exit"] == 0,
            "actual successful native exit")
    for key in ("pipe_capture_complete", "reaped"):
        require(capture.get(key) is True, key)
    for key in ("timed_out", "output_overflow"):
        require(capture.get(key) is False, key)
    require(capture.get("reader_errors") == {}, "complete bounded readers")
    require(type(capture.get("stdout_bytes")) is int and capture["stdout_bytes"] == len(stdout),
            "complete stdout")
    require(type(capture.get("stderr_bytes")) is int and capture["stderr_bytes"] == len(stderr)
            and stderr == b"", "complete empty stderr")
    command = capture.get("command")
    require(type(command) is list and len(command) == 4 and all(type(x) is str for x in command),
            "exact native argv")
    require(Path(command[0]).samefile(executable) and command[3] == mode, "native identity/mode")
    lines = stdout.splitlines(keepends=True)
    require(bool(lines), "complete native output")
    footer = re.fullmatch(rb"PREPARED-CALLEE MC.x " + re.escape(mode.encode("ascii")) +
                         rb": ([1-9][0-9]*) checks; retained XI failure cj48; no Stats success\r?\n",
                         lines[-1])
    require(footer is not None, "exact prepared-callee completion/scope footer")
    for line in lines[:-1]:
        require(re.fullmatch(rb"[1-9][0-9]* (?:achievements|recipes)\r?\n", line) is not None,
                "preserved complete genuine constructor banner")
    return int(footer[1])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=MODES)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    require((assets / "jar").is_dir() and (assets / "manifest.json").is_file(),
            "genuine importer layout, separate jar class root")
    for edge in (16, 32):
        require((assets / f"sdk/lwjgl-icon-{edge}.rgba").stat().st_size == edge * edge * 4,
                "actual complete imported SDK icon")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="prepared-callee-", dir=args.work_dir.resolve()))
    support_file = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("prepared_logo_capture", support_file)
    require(spec is not None and spec.loader is not None, "existing bounded capture support")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    sources_before, binary_before, assets_before = source_bindings(), pin(executable), tree(assets)
    selected = (args.mode,) if args.mode is not None else MODES
    results = []
    for mode in selected:
        case = work / mode
        case.mkdir()
        env = os.environ.copy()
        appdata = None
        if os.name == "nt":
            appdata = case / "appdata"
            appdata.mkdir()
            for key in list(env):
                if key.upper() == "APPDATA":
                    del env[key]
            env["APPDATA"] = str(appdata)
        command = [str(executable), str(assets / "jar"), str(assets), mode]
        result = support.run_bounded(command, case, "native", timeout=180, limit=8 * 1024 * 1024,
                                     env=env)
        report = result.capture_report
        (case / "process.json").write_text(json.dumps(report, indent=2) + "\n")
        try:
            checks = admit(report, result.stdout, result.stderr, executable, mode)
        except Exception as error:
            stdout_tail = result.stdout[-8192:].decode("utf-8", errors="replace")
            stderr_tail = result.stderr[-8192:].decode("utf-8", errors="replace")
            raise ValueError(f"{error}\nstdout tail (at most 8192 bytes):\n{stdout_tail}"
                             f"\nstderr tail (at most 8192 bytes):\n{stderr_tail}") from error
        require(pin(executable) == binary_before and tree(assets) == assets_before,
                "real binary/imported bytes unchanged")
        require(source_bindings() == sources_before, "complete actual source/capture unchanged")
        record = {"status": "PASS_PREPARED_CALLEE_ONLY", "mode": mode, "checks": checks,
                  "capture": report, "stdout": pin(case / "native.stdout"),
                  "stderr": pin(case / "native.stderr"),
                  "appdata": None if appdata is None else str(appdata),
                  "child_environment_overrides": ["APPDATA"] if appdata is not None else [],
                  "native_default_home_on_POSIX": os.name != "nt", "command": command,
                  "MC324_to337_attempted": True, "MC340_to354_completed": False,
                  "retained_XI_failure": "cj:<init>:48", "original_JVM_or_visual_parity": False}
        (case / "report.json").write_text(json.dumps(record, indent=2) + "\n")
        results.append(record)
    require(tuple(x["mode"] for x in results) == selected, "all selected exact modes once")
    (work / "report.json").write_text(json.dumps({"status": "PASS_PREPARED_CALLEE_ONLY",
        "modes": list(selected), "results": results, "binary": binary_before,
        "source_bindings": sources_before, "assets": assets_before,
        "scope": "Private MC.x callee over actual owners retaining failed XI; no Statistics success"},
        indent=2) + "\n")
    print(work)

if __name__ == "__main__":
    main()
