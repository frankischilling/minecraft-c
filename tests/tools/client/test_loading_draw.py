"""Run authored real-file/real-owner loading-prefix controls; no original JVM."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ("normal", "negative", "zero", "over", "wrapped", "null-engine", "null-font",
         "begin", "fp-bind", "fp-background", "font-reload", "font-null-late", "drop-engine",
         "font-retained", "message", "repeat", "input", "update-fp", "no-sdk")


def require(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def write(path, value):
    with Path(path).open("x", encoding="utf-8", newline="\n") as stream:
        data = json.dumps(value, indent=2) + "\n"
        require(stream.write(data) == len(data), "complete report write")


def tree(path):
    return {p.relative_to(path).as_posix(): pin(p) for p in sorted(path.rglob("*")) if p.is_file()}


def png(width, height):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    pixels = (b"\0" + b"\xff\xff\xff\xff" * width) * height
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b""))


def prepare(directory):
    resources = directory / "resources"
    for name in ("font/white.png", "pack.png", "gui/background.png"):
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(png(128, 128))
    # Authored native prerequisites; these pixels are not SDK archive claims.
    (resources / "sdk").mkdir()
    for edge in (16, 32):
        (resources / f"sdk/lwjgl-icon-{edge}.rgba").write_bytes(b"\xff\xff\xff\xff" * edge * edge)
    (resources / "font.txt").write_bytes(bytes(range(32, 127)) + b"\n")
    (resources / "lang").mkdir()
    (resources / "lang/en_US.lang").write_bytes(b"texturePack.title=Texture Packs\n")
    game = directory / "game"
    (game / "texturepacks").mkdir(parents=True)
    (game / "options.txt").write_bytes(b"skin:Default\nguiScale:1\n")
    return resources, game


def admit(capture, stdout, stderr, executable, mode):
    require(type(capture) is dict and type(capture.get("exit")) is int and capture["exit"] == 0,
            "actual successful exit")
    for key in ("pipe_capture_complete", "reaped"):
        require(capture.get(key) is True, key)
    for key in ("timed_out", "output_overflow"):
        require(capture.get(key) is False, key)
    require(capture.get("reader_errors") == {}, "successful explicit readers")
    require(type(capture.get("stdout_bytes")) is int and capture["stdout_bytes"] == len(stdout),
            "complete stdout extent")
    require(type(capture.get("stderr_bytes")) is int and capture["stderr_bytes"] == len(stderr) and
            stderr == b"", "complete empty stderr")
    command = capture.get("command")
    require(type(command) is list and command and all(type(x) is str for x in command), "actual argv")
    require(Path(command[0]).samefile(executable), "actual native executable")
    require(mode in MODES, "exact source control identity")
    found = re.fullmatch(rb"loading prefix " + re.escape(mode.encode("ascii")) +
                        rb": ([1-9][0-9]*) checks passed\r?\n", stdout)
    require(found is not None, "complete positive exact-mode footer; LF/CRLF only")
    return int(found[1])


def bindings():
    path = ROOT / "tests/reference/client/loading_draw/provenance.json"
    values = json.loads(path.read_text(encoding="utf-8"))["native_sources"]
    require(values and type(values) is dict, "complete explicit loading source bindings")
    for name, expected in values.items():
        require(not Path(name).is_absolute() and ".." not in Path(name).parts and "\\" not in name,
                "relative source binding")
        require(pin(ROOT / name) == expected, "actual bound native source: " + name)
    return {"provenance": pin(path), "native_sources": values}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=MODES)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="loading-prefix-", dir=args.work_dir.resolve()))
    support_file = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("actual_loading_capture", support_file)
    require(spec is not None and spec.loader is not None, "existing bounded capture module")
    support = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = support
    spec.loader.exec_module(support)
    source_before = bindings()
    tool_before = pin(Path(__file__))
    support_before = pin(support_file)
    exe_before = pin(executable)
    selected = (args.mode,) if args.mode is not None else MODES
    results = []
    for mode in selected:
        case = work / mode
        case.mkdir()
        resources, game = prepare(case)
        inputs_before = tree(resources)
        game_before = tree(game)
        command = [str(executable), str(resources), str(game), mode]
        try:
            result = support.run_bounded(command, case, "native", timeout=180, limit=8 * 1024 * 1024)
        except BaseException as error:
            write(case / "failure.json", {"command": command, "error": repr(error),
                                          "capture": getattr(error, "capture_report", None)})
            raise
        capture = result.capture_report
        write(case / "process.json", capture)
        require(type(result.returncode) is int and result.returncode == 0, "actual process returncode")
        checks = admit(capture, result.stdout, result.stderr, executable, mode)
        require(pin(executable) == exe_before, "native binary unchanged")
        require(tree(resources) == inputs_before, "genuine authored resource input unchanged")
        require(tree(game) == game_before, "genuine authored settings/Pack input unchanged")
        require(bindings() == source_before and pin(Path(__file__)) == tool_before and
                pin(support_file) == support_before, "source/controller/capture unchanged")
        products = {name: pin(case / name) for name in ("native.capture.json", "native.stdout",
                    "native.stderr", "process.json")}
        record = {"status": "DRAW_CONTROLS_PASS", "mode": mode, "checks": checks,
                  "command": command, "executable_before": exe_before, "executable_after": pin(executable),
                  "capture": capture, "products": products, "resources": inputs_before,
                  "game_inputs": game_before,
                  "completion_scope": "per-profile native contracts; earlier failures explicit", "original_JVM_or_GL": False}
        write(case / "report.json", record)
        results.append(record)
    require(tuple(x["mode"] for x in results) == selected, "all selected source controls ran once")
    write(work / "report.json", {"status": "DRAW_CONTROLS_PASS", "modes": list(selected),
          "source_before": source_before, "source_after": bindings(), "results": results,
          "scope": "Real draw/update/yield/source-return and native-retirement contracts; no startup/playability"})
    print(work)


if __name__ == "__main__":
    main()
