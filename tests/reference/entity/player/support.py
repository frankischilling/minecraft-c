"""Bounded reference-driver utilities; no runtime Player implementation."""

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


def run_bounded(command, cwd, prefix, timeout=120, limit=2097152, env=None):
    chunks = [[], []]
    overflow = threading.Event()
    with subprocess.Popen([str(x) for x in command], cwd=cwd, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env) as process:
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


def extract_resources(jar, destination, expected):
    with zipfile.ZipFile(jar) as archive:
        for name, pin in expected.items():
            require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"),
                    "private resource allowlist")
            require(archive.getinfo(name).file_size == pin["bytes"] <= 1048576,
                    "private resource extent")
            data = archive.read(name)
            require(hashlib.sha256(data).hexdigest() == pin["sha256"], "resource digest")
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)


def validate_tape(raw):
    require(len(raw) <= 4096, "tape byte bound")
    require(all(byte in (9, 10, 13, 32) or 48 <= byte <= 57 or 65 <= byte <= 70 or
                97 <= byte <= 102 for byte in raw), "tape ASCII alphabet")
    tokens = raw.split()
    require(len(tokens) == 32, "exact32scalar-replaydraws")
    for token in tokens:
        require(re.fullmatch(rb"[0-9a-fA-F]{16}", token) is not None, "draw bit syntax")
        require(int(token, 16) < 0x3ff0000000000000, "positive-zero/finite Math domain")
    return 32


def check_initialization(path, jar, expected):
    require(path.is_file() and path.stat().st_size <= 2097152, "class initialization log")
    initialized = set(re.findall(r"\bInitializing '([^']+)'", path.read_text(encoding="utf-8")))
    with zipfile.ZipFile(jar) as archive:
        names = {name[:-6] for name in archive.namelist() if name.endswith(".class")}
    original = initialized & names
    require(original == {name.replace(".", "/") for name in expected},
            "exact original class initialization closure")
    require(not {"yo", "vy", "px", "net/minecraft/client/Minecraft"} & initialized,
            "excluded client bootstrap initialization")
    require(not any(name.startswith(("net/minecraft/client/", "org/lwjgl/", "paulscode/sound/"))
                    for name in initialized), "excluded device/network/client initialization")
    return {"original_classes": len(original), "initialized_classes": len(initialized),
            "excluded_client_graphics_audio": True}


def verify_binding(path, jar, classes):
    from urllib.parse import urlparse
    from urllib.request import url2pathname
    require(path.is_file() and path.stat().st_size <= 262144, "runtime binding extent")
    lines = path.read_text(encoding="ascii").splitlines()
    require(lines and lines[0] == "PLAYER_BINDING_V1", "runtime binding header")
    expected = {c["name"].replace("/", "."): c for c in classes}
    require(len(lines) == 1 + len(expected), "runtime binding count")
    seen = set()
    for line in lines[1:]:
        name, extent, digest, origin = line.split("\t")
        require(name in expected and name not in seen, "runtime binding class")
        seen.add(name)
        pin = expected[name]
        require(int(extent) == pin["class_bytes"] and digest == pin["class_sha256"],
                "runtime class bytes")
        parsed = urlparse(origin)
        require(parsed.scheme == "file" and not parsed.netloc and
                Path(url2pathname(parsed.path)).resolve() == jar.resolve(), "runtime JAR origin")
    return len(seen)
