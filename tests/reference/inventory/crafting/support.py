"""Private reference-driver utilities; no runtime inventory implementation."""

import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import threading
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def fresh_directory(work_dir, prefix="run-"):
    work_dir.mkdir(parents=True, exist_ok=True)
    return Path(tempfile.mkdtemp(prefix=prefix, dir=work_dir.resolve()))


def run_bounded(command, cwd, prefix, timeout=120, limit=1048576):
    chunks = [[], []]
    overflow = threading.Event()
    with subprocess.Popen([str(x) for x in command], cwd=cwd, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
        def read(pipe, index):
            total = 0
            while True:
                data = pipe.read(32768)
                if not data:
                    return
                total += len(data)
                if total > limit:
                    overflow.set()
                    process.kill()
                    return
                chunks[index].append(data)

        readers = [threading.Thread(target=read, args=(pipe, index), daemon=True)
                   for index, pipe in enumerate((process.stdout, process.stderr))]
        for reader in readers:
            reader.start()
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise ValueError(prefix + " process timeout") from None
        finally:
            for reader in readers:
                reader.join(timeout=5)
        require(not any(reader.is_alive() for reader in readers), prefix + " reader shutdown")
    output, error = (b"".join(parts) for parts in chunks)
    (cwd / (prefix + ".stdout")).write_bytes(output)
    (cwd / (prefix + ".stderr")).write_bytes(error)
    require(not overflow.is_set(), prefix + " process output bound")
    return subprocess.CompletedProcess(command, code, output, error)


def validate_plan(raw):
    require(len(raw) <= 1048576, "plan byte bound")
    require(all(x in (9, 10, 13) or 32 <= x <= 126 for x in raw), "plan ASCII/control byte")
    normalized = raw.replace(b"\r\n", b"\n")
    require(b"\r" not in normalized, "bare CR plan terminator")
    rows = normalized.split(b"\n")
    if rows and rows[-1] == b"":
        rows.pop()
    require(len(rows) <= 4096, "plan row bound")
    labels = set()
    for row in rows:
        require(0 < len(row) <= 1023, "blank/overlong plan row")
        parts = row.split(b"\t")
        require(len(parts) >= 3 and parts[0] and parts[0] not in labels, "plan label/columns")
        labels.add(parts[0])

        def integer(value):
            require(re.fullmatch(rb"[+-]?[0-9]+", value) is not None, "plan integer syntax")
            result = int(value)
            require(-(1 << 31) <= result < (1 << 31), "plan int32 range")
            return result

        width, index = integer(parts[1]), integer(parts[2])
        require(width in (2, 3) and 0 <= index < 151, "plan width/recipe index")
        require(len(parts) == 3 + width * width, "plan cell extent")
        groups = {}
        for token in parts[3:]:
            if token == b"-":
                continue
            cells = token.split(b",")
            require(len(cells) == 4, "plan Stack tuple")
            item, count, damage, group = (integer(value) for value in cells)
            require(1 <= group <= 9, "plan alias group")
            value = (item, count, damage)
            require(group not in groups or groups[group] == value, "plan conflicting alias")
            groups[group] = value
    return len(rows)


def extract_resources(jar, destination, expected):
    with zipfile.ZipFile(jar) as archive:
        for name, pin in expected.items():
            require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"),
                    "private resource allowlist")
            require(archive.getinfo(name).file_size == pin["bytes"] <= 1048576,
                    "private resource extent")
            data = archive.read(name)
            require(hashlib.sha256(data).hexdigest() == pin["sha256"], "private resource digest")
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)


def check_initialization(path, side):
    require(path.is_file() and path.stat().st_size <= 2097152, "class initialization log")
    initialized = set(re.findall(r"\bInitializing '([^']+)'", path.read_text(encoding="utf-8")))
    required = {"jl", "hk", "is", "tt", "mq", "wl", "ix", "aa", "it", "dw", "gp", "yv", "pv"}
    excluded = {"fd", "gs", "ls", "sn", "dc", "net/minecraft/client/Minecraft"}
    if side == "server":
        required = {"gg", "ey", "ft", "ml", "hz", "of", "fx", "o", "fu", "cl", "el", "py", "js"}
        excluded = {"dj", "em", "net/minecraft/server/MinecraftServer"}
    require(required <= initialized, side + " required crafting class initialization")
    require(not excluded.intersection(initialized), side + " excluded Player/World initialization")
    require(not any(name.startswith(("org/lwjgl/", "paulscode/sound/")) for name in initialized),
            "excluded graphics/audio initialization")
    return {"required_initialized": sorted(required), "excluded_uninitialized": sorted(excluded),
            "initialized_classes": len(initialized)}
