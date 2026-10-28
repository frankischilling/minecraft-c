"""Finite actual startup statistics/input controls; no original/JVM execution.

Only window-session and its real GL-error boundary mode credit the genuine
normal prefix through actual GL setup/diagnostics to the next unexecuted cx470.
Nonempty Controllers discovery remains an honest SDK Native12 refusal.
Other modes test explicit native admission
or private-helper ownership over genuine, partially constructed owners.
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
MODES = ("statistics-admission", "statistics-null-z", "window-session",
         "window-statistics-foreign-ready", "window-session-gl-error")


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
    paths += [ROOT / "CMakeLists.txt", Path(__file__),
              ROOT / "tests/reference/client/statistics/support.py"]
    return {p.relative_to(ROOT).as_posix(): pin(p) for p in sorted(paths)}


def admit(capture, stdout, stderr, executable, mode):
    require(mode in MODES, "exact tail mode")
    require(type(capture) is dict and type(capture.get("exit")) is int and capture["exit"] == 0,
            "actual native success")
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
    argc = 4 if mode.startswith("window-") else 3
    require(type(command) is list and len(command) == argc and
            all(type(x) is str for x in command), "exact native argv")
    require(Path(command[0]).samefile(executable) and command[2] == mode, "native identity/mode")
    lines = stdout.splitlines(keepends=True)
    require(bool(lines), "complete native output")
    footer = re.fullmatch(rb"Actual source startup authored contracts: ([1-9][0-9]*) checks\r?\n",
                          lines[-1])
    require(footer is not None, "exact native assertion footer")
    banners = lines[:-1]
    if mode == "window-session-gl-error":
        header = rb"########## GL ERROR ##########\r?\n"
        starts = [index for index, line in enumerate(banners) if re.fullmatch(header, line)]
        require(len(starts) == 1, "exact one complete real GL diagnostic block")
        index = starts[0]
        require(index + 3 <= len(banners) and
                re.fullmatch(rb"@ Pre startup\r?\n", banners[index + 1]) is not None and
                re.fullmatch(rb"1280: Invalid enum\r?\n", banners[index + 2]) is not None,
                "exact genuine three println results")
        # Preserve the raw stream. Native PrintStream and C printf banners
        # have separate buffering; do not insert a flush or reorder output.
        banners = banners[:index] + banners[index + 3:]
    for line in banners:
        require(re.fullmatch(rb"[1-9][0-9]* (?:achievements|recipes)\r?\n", line) is not None,
                "preserved complete genuine constructor banner")
    return int(footer[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=MODES, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    resources = args.resources.resolve(strict=True)
    assets = None if args.assets is None else args.assets.resolve(strict=True)
    if args.mode.startswith("window-"):
        require(assets is not None and (assets / "jar").is_dir() and
                (assets / "manifest.json").is_file(), "genuine imported assets")
        for edge in (16, 32):
            require((assets / f"sdk/lwjgl-icon-{edge}.rgba").stat().st_size == edge * edge * 4,
                    "actual complete imported SDK icon")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=args.mode + "-", dir=args.work_dir.resolve()))
    support_file = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("startup_tail_capture", support_file)
    require(spec is not None and spec.loader is not None, "existing bounded capture support")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    env = os.environ.copy()
    appdata = None
    if os.name == "nt":
        appdata = work / "appdata"
        appdata.mkdir()
        for key in list(env):
            if key.upper() == "APPDATA":
                del env[key]
        env["APPDATA"] = str(appdata)
    before = {"source": source_bindings(), "binary": pin(executable), "resources": tree(resources),
              "assets": None if assets is None else tree(assets)}
    command = [str(executable), str(resources), args.mode]
    if args.mode.startswith("window-"):
        command.append(str(assets))
    result = support.run_bounded(command, work, "native", timeout=180, limit=8 * 1024 * 1024,
                                 env=env)
    try:
        checks = admit(result.capture_report, result.stdout, result.stderr, executable, args.mode)
    except Exception as error:
        stdout_tail = result.stdout[-8192:].decode("utf-8", errors="replace")
        stderr_tail = result.stderr[-8192:].decode("utf-8", errors="replace")
        raise ValueError(f"{error}\nstdout tail (at most 8192 bytes):\n{stdout_tail}"
                         f"\nstderr tail (at most 8192 bytes):\n{stderr_tail}") from error
    require(source_bindings() == before["source"] and pin(executable) == before["binary"] and
            tree(resources) == before["resources"] and
            (None if assets is None else tree(assets)) == before["assets"],
            "actual source/binary/resource inputs unchanged")
    record = {"status": "PASS_FINITE_STARTUP_TAIL_MODE", "mode": args.mode, "checks": checks,
              "capture": result.capture_report, "stdout": pin(work / "native.stdout"),
              "stderr": pin(work / "native.stderr"), "inputs": before,
              "appdata": None if appdata is None else str(appdata),
              "child_environment_overrides": [] if appdata is None else ["APPDATA"],
              "native_default_home_on_POSIX": os.name != "nt",
              "normal_source_through_C380": args.mode in ("window-session", "window-session-gl-error"),
              "normal_source_through_GL460_and_diagnostic466": args.mode in
                  ("window-session", "window-session-gl-error"),
              "next_unexecuted_MC470": args.mode in ("window-session", "window-session-gl-error"),
              "real_controller_boundary_invalid_enum": args.mode == "window-session-gl-error",
              "original_JVM_or_visual_parity": False}
    (work / "report.json").write_text(json.dumps(record, indent=2) + "\n")
    print(work)


if __name__ == "__main__":
    main()
