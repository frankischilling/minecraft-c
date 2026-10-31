"""Finite actual LocalPlayer input attachment/forwarding comparison; captures remain private."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import threading
import zipfile

HERE = Path(__file__).resolve().parent


def require(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def execute(command, cwd, name, env, timeout=90):
    chunks = [[], []]
    overflow = threading.Event()
    with subprocess.Popen([str(x) for x in command], cwd=cwd, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env) as process:
        def collect(pipe, index):
            size = 0
            while data := pipe.read(32768):
                size += len(data)
                if size > 1048576:
                    overflow.set()
                    process.kill()
                    return
                chunks[index].append(data)
        readers = [threading.Thread(target=collect, args=(pipe, index), daemon=True)
                   for index, pipe in enumerate((process.stdout, process.stderr))]
        for reader in readers:
            reader.start()
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise ValueError(name + " timeout") from None
        finally:
            for reader in readers:
                reader.join(timeout=5)
    output, error = (b"".join(parts) for parts in chunks)
    (cwd / (name + ".stdout")).write_bytes(output)
    (cwd / (name + ".stderr")).write_bytes(error)
    require(not overflow.is_set() and not any(reader.is_alive() for reader in readers),
            name + " capture bound/shutdown")
    return subprocess.CompletedProcess(command, code, output, error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path)
    args = parser.parse_args()
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        require(not env.get(name), "unset ambient Java options")
        env.pop(name, None)
    env.pop("CLASSPATH", None)
    pins = json.loads((HERE / "source-pins.json").read_bytes())
    require(pins["schema"] == "LOCAL_PLAYER_INPUT_REFERENCE_V1", "reference schema")
    jar, java, javac, native = (p.resolve() for p in
                               (args.client_jar, args.java, args.javac, args.native))
    require(pin(jar) == pins["jar"], "exact supplied client JAR")
    for name, expected in pins["reference_files"].items():
        require(pin(HERE / name) == expected, "reference source: " + name)
    source = args.source_root.resolve() if args.source_root else next(
        (p for p in HERE.parents if (p / "include/mc/entity/player/input.h").is_file()), None)
    require(source is not None, "matching source root")
    for name, expected in pins["native_sources"].items():
        require(pin(source / name) == expected, "native source: " + name)
    with zipfile.ZipFile(jar) as archive:
        require(len(archive.namelist()) == len(set(archive.namelist())), "unique JAR entries")
        require("PlayerInputObservation.class" not in archive.namelist(),
                "helper/original class collision")
        for name, expected in pins["classes"].items():
            data = archive.read(name.replace(".", "/") + ".class")
            require(len(data) == expected["class_bytes"] and
                    hashlib.sha256(data).hexdigest() == expected["class_sha256"],
                    "class/selected-Code binding: " + name)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="player-input-", dir=args.work_dir.resolve()))
    require(java.parent == javac.parent, "matching explicit JDK17 executables")
    report = {"run_directory": str(run), "jar": pin(jar), "native": pin(native), "tools": {}}
    for name, tool in (("java", java), ("javac", javac)):
        result = execute([tool, "-version"], run, name + "-version", env)
        version = (result.stdout + result.stderr).decode("utf-8")
        require(result.returncode == 0 and re.search(r'\b(?:version\s+")?17\.', version),
                "explicit JDK17 version")
        report["tools"][name] = {"path": str(tool), **pin(tool), "version": version}
    classes = run / "helpers"
    classes.mkdir()
    result = execute([javac, "-source", "17", "-target", "17", "-g:none", "-encoding", "UTF-8",
                      "-classpath", jar, "-d", classes,
                      HERE / "PlayerInputObservation.java"], run, "compile", env)
    require(result.returncode == 0 and not result.stdout and not result.stderr, "helper compilation")
    require({p.name for p in classes.iterdir()} == {"PlayerInputObservation.class"},
            "authored helper-only classpath")
    require(pin(classes / "PlayerInputObservation.class") == pins["helper_class"],
            "helper class/Code binding")
    result = execute([java, "-Xmx256m", "-Xverify:all", "-Xlog:class+init=info:file=class-init.log",
                      "-classpath", str(classes) + os.pathsep + str(jar),
                      "PlayerInputObservation"], run, "original", env)
    require(result.returncode == 0 and not result.stderr, "original exit/diagnostics")
    original = result.stdout.splitlines()
    require(original[:2] == [b"16 achievements", b"151 recipes"] and len(original) == 23,
            "preserve original banners and rows")
    require((run / "class-init.log").stat().st_size <= 2097152, "class-init log extent")
    initialized = set(re.findall(r"\bInitializing '([^']+)'",
                                 (run / "class-init.log").read_text(encoding="utf-8")))
    require({"fd", "gs", "ls", "sn", "dc", "ok", "gr", "uo", "lr", "kv", "qb"} <= initialized,
            "actual World/Player/input initialization")
    require(not {"net/minecraft/client/Minecraft", "yo", "vy", "px"} & initialized and
            not any(x.startswith(("org/lwjgl/", "paulscode/sound/")) for x in initialized),
            "exclude client/device bootstrap")
    resources = run / "private-resources"
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins["private_resources"].items():
            require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"),
                    "resource allowlist")
            data = archive.read(name)
            require(len(data) == expected["bytes"] and
                    hashlib.sha256(data).hexdigest() == expected["sha256"], "private resource pin")
            path = resources / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    result = execute([native, resources, run / "world"],
                     run, "native", env)
    require(result.returncode == 0 and not result.stderr, "native exit/diagnostics")
    lines = result.stdout.splitlines()
    projected = [line for line in lines if line.startswith(b"INPUT\t")]
    require(projected == original[2:], "actual input projections")
    count = re.fullmatch(rb"LocalPlayer input attachment: ([1-9][0-9]*) checks", lines[-1])
    require(count is not None and lines[:2] == [b"151 recipes", b"16 achievements"] and
            lines[2:-1] == projected,
            "actual native banners/check count")
    require(pin(jar) == pins["jar"], "JAR unchanged after execution")
    for name, expected in pins["native_sources"].items():
        require(pin(source / name) == expected, "source changed during comparison")
    report.update(status="passed", projected_rows=21, native_checks=int(count.group(1)),
                  source_exceptions_observed="real NULL input and nested NULL settings/binding failures",
                  source_scope=pins["scope"], stderr_bytes=0)
    (run / "report.json").write_bytes((json.dumps(report, indent=2) + "\n").encode("ascii"))
    print(json.dumps(report))


if __name__ == "__main__":
    main()
