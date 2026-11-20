"""Compare actual native startup captures using authored resources and settings.

This graphics-only test retains fresh inputs, streams, PNGs and receipts. It
compares decoded pixels between runs, not against an original image or model.
The enclosing CTest owner supervises the complete process tree.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import stat
import struct
import subprocess
import tempfile
import time
import zlib


WIDTH, HEIGHT = 854, 480
CASE_SECONDS = 20
STREAM_LIMIT = 1024 * 1024
PNG_LIMIT = 16 * 1024 * 1024
FILE_LIMIT = 128 * 1024 * 1024
EXECUTABLE_LIMIT = 1024 * 1024 * 1024
EXECUTABLE_CHUNK = 1024 * 1024
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise AssertionError(reason)


def ordinary(path: Path) -> None:
    info = path.lstat()
    need(not path.is_symlink() and not getattr(info, "st_file_attributes", 0) & 0x400,
         f"ordinary path required: {path}")


def raw_file(path: Path, limit: int = FILE_LIMIT) -> bytes:
    ordinary(path)
    info = path.stat()
    need(stat.S_ISREG(info.st_mode) and info.st_size <= limit,
         f"bounded file: {path} (mode={stat.filemode(info.st_mode)}, "
         f"bytes={info.st_size}, limit={limit})")
    raw = path.read_bytes()
    need(len(raw) <= limit and path.stat().st_size == len(raw), f"complete file: {path}")
    return raw


def raw_pin(raw: bytes) -> dict:
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def pin(path: Path, limit: int = FILE_LIMIT) -> dict:
    return raw_pin(raw_file(path, limit))


def file_identity(info: os.stat_result) -> tuple:
    # Windows path stat infers execute bits from the filename; descriptor stat
    # cannot. Their st_ctime meanings also differ. Compare the actual file type
    # across APIs and check the complete mode separately within each API.
    return (info.st_dev, info.st_ino, stat.S_IFMT(info.st_mode), info.st_size,
            info.st_mtime_ns)


def executable_pin(path: Path) -> dict:
    """Hash a bounded compiled artifact without loading it into a resource buffer."""
    ordinary(path)
    before = path.stat()
    need(stat.S_ISREG(before.st_mode) and before.st_size <= EXECUTABLE_LIMIT,
         f"bounded executable: {path} (mode={stat.filemode(before.st_mode)}, "
         f"bytes={before.st_size}, limit={EXECUTABLE_LIMIT})")
    digest = hashlib.sha256()
    count = 0
    with path.open("rb") as stream:
        opened = os.fstat(stream.fileno())
        need(file_identity(opened) == file_identity(before),
             f"unchanged executable before reading: {path}")
        while True:
            block = stream.read(min(EXECUTABLE_CHUNK, before.st_size - count + 1))
            if not block:
                break
            count += len(block)
            need(count <= before.st_size, f"executable grew while reading: {path}")
            digest.update(block)
        read = os.fstat(stream.fileno())
        need(count == before.st_size and file_identity(read) == file_identity(opened) and
             read.st_mode == opened.st_mode,
             f"complete unchanged executable read: {path}")
    ordinary(path)
    after = path.stat()
    need(file_identity(after) == file_identity(before) and after.st_mode == before.st_mode,
         f"unchanged executable after reading: {path}")
    return {"bytes": count, "sha256": digest.hexdigest()}


def tree(root: Path) -> dict:
    if not root.exists():
        need(not root.is_symlink(), "no dangling input link")
        return {"kind": "absent"}
    ordinary(root)
    need(root.is_dir(), "owned input root must be a directory")
    directories, files = [], {}
    for path in sorted(root.rglob("*")):
        ordinary(path)
        name = path.relative_to(root).as_posix()
        if path.is_dir():
            directories.append(name)
        else:
            need(path.is_file(), f"ordinary retained member: {name}")
            files[name] = pin(path)
    return {"kind": "directory", "directories": directories, "files": files}


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def authored_png(width: int, height: int, pixel) -> bytes:
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        for x in range(width):
            scanlines.extend(pixel(x, y))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (PNG_SIGNATURE + chunk(b"IHDR", header) +
            chunk(b"IDAT", zlib.compress(scanlines, 9)) + chunk(b"IEND", b""))


def resources(root: Path) -> None:
    def font(x, y):
        glyph = x // 8 + 16 * (y // 8)
        lit = (glyph != 32 and 1 <= x % 8 <= 5 and 1 <= y % 8 <= 6 and
               (x + y + glyph) % 3 != 0)
        return (220, 240, 255, 255) if lit else (0, 0, 0, 0)

    # Button pixels are independent of row. Normal/hover atlas rows therefore
    # match without controlling the real OS pointer. Empty button labels below
    # likewise remove hover-only text color; other title text uses this font.
    images = {
        "font/default.png": authored_png(128, 128, font),
        "gui/background.png": authored_png(32, 32, lambda x, y:
            (210, 35, 70, 255) if (x // 4 + y // 4) % 2 else (30, 175, 225, 255)),
        "gui/gui.png": authored_png(256, 256, lambda x, y:
            (25 + (x % 8) * 20, 190 - (x % 5) * 25, 65 + (x % 7) * 20, 255)),
        "title/mclogo.png": authored_png(256, 256, lambda x, y:
            (230, 70, 25, 255) if (x // 8 + y // 8) % 2 else (35, 110, 235, 255)),
    }
    text = {
        "font.txt": ("".join(chr(value) for value in range(32, 127)) + "\n").encode("ascii"),
        "title/splashes.txt": b"Authored startup!\n",
        "lang/en_US.lang": (b"menu.singleplayer=\nmenu.multiplayer=\nmenu.mods=\n"
                            b"menu.options=\nmenu.quit=\n"),
        "lang/stats_US.lang": b"# Authored empty statistics translation resource\n",
        "achievement/map.txt": b"",
    }
    for name, raw in {**images, **text}.items():
        path = root / "jar" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(raw)


def decoded_png(raw: bytes) -> tuple[int, int, bytes]:
    need(len(raw) <= PNG_LIMIT and raw.startswith(PNG_SIGNATURE), "complete captured PNG signature")
    position, compressed = len(PNG_SIGNATURE), bytearray()
    header = None
    ended = False
    data_started = data_ended = False
    while position < len(raw):
        need(position + 12 <= len(raw), "complete PNG chunk framing")
        size = struct.unpack_from(">I", raw, position)[0]
        kind = raw[position + 4:position + 8]
        need(all(65 <= unit <= 90 or 97 <= unit <= 122 for unit in kind) and
             65 <= kind[2] <= 90, "valid PNG chunk letters and reserved bit")
        end = position + size + 12
        need(end <= len(raw), "complete PNG chunk body")
        data = raw[position + 8:position + 8 + size]
        need(struct.unpack_from(">I", raw, end - 4)[0] == zlib.crc32(kind + data), "PNG CRC")
        if header is None:
            need(kind == b"IHDR" and size == 13, "first unique PNG IHDR")
            header = struct.unpack(">IIBBBBB", data)
        elif kind == b"IHDR":
            raise AssertionError("duplicate PNG IHDR")
        elif kind == b"IDAT":
            need(not data_ended, "contiguous PNG image data")
            data_started = True
            compressed.extend(data)
        elif kind == b"IEND":
            need(size == 0 and data_started and end == len(raw), "complete final PNG IEND")
            ended = True
        else:
            need(kind not in (b"PLTE", b"tRNS"),
                 "capture profile uses opaque RGB/RGBA without palette or transparency key")
            need(kind[0] & 0x20, "supported PNG critical chunks")
            if data_started:
                data_ended = True
        position = end
        if ended:
            break
    need(ended and header is not None, "complete PNG image")
    width, height, depth, color, compression, filtering, interlace = header
    need(0 < width <= 4096 and 0 < height <= 4096 and width * height <= 4 * 1024 * 1024,
         "bounded captured dimensions")
    need(depth == 8 and color in (2, 6) and compression == filtering == interlace == 0,
         "captured PNG must be noninterlaced 8-bit RGB/RGBA")
    channels = 3 if color == 2 else 4
    stride = width * channels
    expected = (stride + 1) * height
    decoder = zlib.decompressobj()
    encoded = decoder.decompress(compressed, expected + 1)
    need(len(encoded) == expected and decoder.eof and not decoder.unused_data and
         not decoder.unconsumed_tail, "complete bounded PNG deflate stream")
    previous = bytearray(stride)
    pixels = bytearray()
    for y in range(height):
        start = y * (stride + 1)
        method = encoded[start]
        need(method <= 4, "PNG scanline filter")
        line = bytearray(encoded[start + 1:start + 1 + stride])
        for x in range(stride):
            left = line[x - channels] if x >= channels else 0
            above = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            if method == 1:
                predict = left
            elif method == 2:
                predict = above
            elif method == 3:
                predict = (left + above) // 2
            elif method == 4:
                estimate = left + above - corner
                distances = (abs(estimate - left), abs(estimate - above), abs(estimate - corner))
                predict = left if distances[0] <= distances[1] and distances[0] <= distances[2] else (
                    above if distances[1] <= distances[2] else corner)
            else:
                predict = 0
            line[x] = (line[x] + predict) & 255
        if channels == 4:
            pixels.extend(line)
        else:
            for x in range(0, stride, 3):
                pixels.extend(line[x:x + 3])
                pixels.append(255)
        previous = line
    return width, height, bytes(pixels)


def options(scale: int, anaglyph: bool = False, bad_line: bool = False) -> bytes:
    return ((b"guiScale:not-an-int\n" if bad_line else b"") +
            b"authoredUnknown:preserve:every:byte\nskin:Default\n" +
            f"guiScale:{scale}\nanaglyph3d:{str(anaglyph).lower()}\n".encode("ascii"))


def run_case(binary: Path, assets: Path, root: Path, name: str, source: bytes | str,
             override: int | None, diagnostics: list[bytes],
             extra_arguments: tuple[str, ...] = (),
             startup_lines: tuple[bytes, ...] = (b"16 achievements", b"151 recipes"),
             timeout_seconds: int = CASE_SECONDS) -> tuple[dict, tuple | None]:
    case = root / name
    case.mkdir()
    game = case / "game"
    if source != "absent":
        game.mkdir()
        if source == "directory":
            (game / "options.txt").mkdir()
            (game / "options.txt" / "retained.bin").write_bytes(b"authored-directory-child\x00\xff")
        else:
            (game / "options.txt").write_bytes(source)
    before = tree(game)
    capture = case / "frame.png"
    command = [str(binary), "--assets", str(assets), "--game-directory", str(game),
               "--hidden", "--frames", "1", "--time-millis", "0", "--width", str(WIDTH),
               "--height", str(HEIGHT), "--capture", str(capture)]
    command.extend(extra_arguments)
    if override is not None:
        command.extend(["--gui-scale", str(override)])
    receipt = {"passed": False, "command": command, "cwd": str(case),
               "game_before": before, "timeout_seconds": timeout_seconds,
               "stdout_limit": STREAM_LIMIT, "stderr_limit": STREAM_LIMIT,
               "ownership": "direct Popen child; enclosing CTest owner supervises descendants"}
    child, pixels = None, None
    cleanup_errors = []
    started = time.monotonic()
    try:
        with (case / "native.stdout").open("xb") as stdout, (case / "native.stderr").open("xb") as stderr:
            child = subprocess.Popen(command, cwd=case, stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr)
            receipt["pid"] = child.pid
            while child.poll() is None:
                need(time.monotonic() - started <= timeout_seconds, "native startup deadline")
                for stream in ("native.stdout", "native.stderr"):
                    need((case / stream).stat().st_size <= STREAM_LIMIT, "native startup stream bound")
                if capture.exists():
                    need(capture.stat().st_size <= PNG_LIMIT, "native capture byte bound")
                time.sleep(0.02)
            receipt["returncode"] = child.wait(timeout=5)
        # Decode only after natural exit closes the PNG: no read-publication race.
        raw_stdout = raw_file(case / "native.stdout", STREAM_LIMIT)
        raw_stderr = raw_file(case / "native.stderr", STREAM_LIMIT)
        receipt["consumed_outputs"] = {"native.stdout": raw_pin(raw_stdout),
                                       "native.stderr": raw_pin(raw_stderr)}
        need(receipt["returncode"] == 0, "native startup return status")
        need(raw_stderr == b"", "unexpected native startup stderr")
        lines = raw_stdout.splitlines()
        prefix = list(startup_lines) + diagnostics
        need(lines[:len(prefix)] == prefix and len(lines) == len(prefix) + 1,
             "exact reached settings diagnostics and one graphics banner")
        banner = re.fullmatch(rb"OpenGL ([0-9]+)\.([0-9]+) core: .+ / .+", lines[-1])
        need(banner is not None and (int(banner[1]), int(banner[2])) >= (3, 3), "actual OpenGL3.3+ banner")
        receipt["graphics_banner_hex"] = lines[-1].hex()
        raw_capture = raw_file(capture, PNG_LIMIT)
        receipt["consumed_outputs"]["frame.png"] = raw_pin(raw_capture)
        pixels = decoded_png(raw_capture)
        width, height, rgba = pixels
        receipt["decoded"] = {"width": width, "height": height, "rgba": raw_pin(rgba)}
        need((width, height) == (WIDTH, HEIGHT),
             "unsupported startup pixel profile: requested854x480 must produce physical854x480 for Auto2 relation")
        need(all(rgba[index] == 255 for index in range(3, len(rgba), 4)), "opaque App capture")
        need(len({rgba[index:index + 3] for index in range(0, len(rgba), 4)}) > 8,
             "authored title capture contains varied rendered content")
        receipt["passed"] = True
    except BaseException as error:
        receipt["failure"] = f"{type(error).__name__}: {error}"
    finally:
        if child is not None:
            needs_kill = True
            try:
                needs_kill = child.poll() is None
            except BaseException as error:
                cleanup_errors.append(f"poll: {type(error).__name__}: {error}")
            if needs_kill:
                try:
                    child.kill()
                    receipt["forced_cleanup"] = True
                    receipt["passed"] = False
                except BaseException as error:
                    cleanup_errors.append(f"kill: {type(error).__name__}: {error}")
            try:
                receipt["returncode"] = child.wait(timeout=5)
                receipt["reaped"] = True
            except BaseException as error:
                cleanup_errors.append(f"reap: {type(error).__name__}: {error}")
        receipt["elapsed_seconds"] = time.monotonic() - started
        try:
            after = tree(game)
            receipt["game_after"] = after
            expected = {"kind": "directory", "directories": [], "files": {}} if source == "absent" else before
            expected = {**expected, "directories": sorted(expected["directories"] + ["stats"])}
            need(after == expected, "startup must create only its empty stats directory and preserve settings inputs")
        except BaseException as error:
            cleanup_errors.append(f"game inputs: {type(error).__name__}: {error}")
        receipt["outputs"] = {}
        for filename in ("native.stdout", "native.stderr", "frame.png"):
            try:
                path = case / filename
                receipt["outputs"][filename] = pin(path) if path.exists() else {"kind": "absent"}
                if filename in receipt.get("consumed_outputs", {}):
                    need(receipt["outputs"][filename] == receipt["consumed_outputs"][filename],
                         f"unchanged consumed output: {filename}")
            except BaseException as error:
                cleanup_errors.append(f"{filename}: {type(error).__name__}: {error}")
        if cleanup_errors:
            receipt["cleanup_errors"] = cleanup_errors
            receipt["passed"] = False
        with (case / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
    return receipt, pixels


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    ordinary(work)
    need(work.is_dir(), "existing owned work directory")
    if os.name != "nt":
        need(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL,
             "startup tests require waitable child exit status")
    root = Path(tempfile.mkdtemp(prefix="mc-startup-settings-", dir=work))
    assets = root / "authored-import"
    receipt = {"schema": 1, "scope": "authored-native-startup-settings-pixel-relations",
               "passed": False, "root": str(root), "cases": {}, "relations": [],
               "original_rendering_claim": False, "physical_capture_profile": [WIDTH, HEIGHT]}
    local_day = datetime.datetime.now().astimezone().date().isoformat()
    failure = None
    errors = []
    try:
        receipt["binary_before"] = executable_pin(binary)
        resources(assets)
        receipt["assets_before"] = tree(assets)
        cases = (
            ("loaded-one", options(1), None, []),
            ("explicit-one", options(2), 1, []),
            ("loaded-two", options(2), None, []),
            ("auto-over-one", options(1), 0, []),
            ("negative-over-two", options(2), -7, []),
            ("anaglyph-on", options(1, True), None, []),
            ("bad-line-later-one", options(1, bad_line=True), None,
             [b"Skipping bad option: guiScale:not-an-int"]),
            ("options-directory", "directory", None, [b"Failed to load options"]),
            ("absent-options", "absent", None, []),
        )
        images = {}
        for name, source, override, diagnostics in cases:
            result, image = run_case(binary, assets, root, name, source, override, diagnostics)
            receipt["cases"][name] = result
            need(result["passed"] and image is not None, f"startup case failed: {name}")
            images[name] = image
        relations = (
            ("loaded-one", "explicit-one", True),
            ("loaded-one", "loaded-two", False),
            ("loaded-two", "auto-over-one", True),
            ("loaded-one", "negative-over-two", True),
            ("loaded-one", "anaglyph-on", False),
            ("loaded-one", "bad-line-later-one", True),
            ("loaded-two", "options-directory", True),
            ("loaded-two", "absent-options", True),
        )
        for left, right, equal in relations:
            actual = images[left] == images[right]
            receipt["relations"].append({"left": left, "right": right,
                                         "expected_equal": equal, "actual_equal": actual})
            need(actual == equal, f"decoded-pixel relation: {left} / {right}")
        need(datetime.datetime.now().astimezone().date().isoformat() == local_day,
             "local date changed during holiday-splash comparison profile")
        receipt["passed"] = True
    except BaseException as error:
        failure = error
        receipt["failure"] = f"{type(error).__name__}: {error}"
    finally:
        for label, action in (("binary_after", lambda: executable_pin(binary)),
                              ("assets_after", lambda: tree(assets))):
            try:
                receipt[label] = action()
                need(receipt[label] == receipt[label.replace("_after", "_before")], f"unchanged {label}")
            except BaseException as error:
                errors.append(f"{label}: {type(error).__name__}: {error}")
        if errors:
            receipt["passed"] = False
            receipt["final_errors"] = errors
        with (root / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
    print("Capture: " + json.dumps(str(root), ensure_ascii=True))
    if not receipt["passed"]:
        raise RuntimeError(f"Startup settings failure retained at {root}") from failure
    print("PASS: 9 native startup cases and 8 decoded-pixel relations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
