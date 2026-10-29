"""Optionally repeat the unchanged-JAR picker observation on an authored World."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR_SHA = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar":
        "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar":
        "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar":
        "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar":
        "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def pin(path):
    data = path.read_bytes()
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def world(path):
    fixture = load("picker_inputs", ROOT / "tests/tools/client/test_picking.py")
    fixture.authored_world(path)
    tag = fixture.tag
    data = [tag(4, "RandomSeed", struct.pack(">q", 23)),
            *(tag(3, name, struct.pack(">i", value)) for name, value in
              (("SpawnX", 0), ("SpawnY", 64), ("SpawnZ", 0), ("version", 19132),
               ("rainTime", 0), ("thunderTime", 0))),
            *(tag(4, name, struct.pack(">q", value)) for name, value in
              (("Time", 0), ("LastPlayed", 123), ("SizeOnDisk", 0))),
            tag(8, "LevelName", b"\0\4Pick"),
            tag(1, "raining", b"\0"), tag(1, "thundering", b"\0")]
    nbt = tag(10, "", tag(10, "Data", b"".join(data) + b"\0") + b"\0")
    (path / "level.dat").write_bytes(gzip.compress(nbt, mtime=0))
    (path / "session.lock").write_bytes(struct.pack(">q", 137))


def normal_row(row):
    return {key: value for key, value in row.items() if key != "bt_pool_identity"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--libraries-root", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    libraries = [(args.libraries_root / name).resolve(strict=True) for name in LIBRARIES]
    require(pin(jar)["sha256"] == JAR_SHA, "original client JAR pin")
    for library, sha in zip(libraries, LIBRARIES.values()):
        require(pin(library)["sha256"] == sha, f"original SDK metadata pin: {library.name}")
    reference = json.loads((HERE / "finite.json").read_text(encoding="utf-8"))
    for source, sha in reference["source_sha256"].items():
        require(pin(HERE / source)["sha256"] == sha, f"authored observer source: {source}")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="picker-original-", dir=args.work_dir.resolve()))
    classes = out / "classes"
    classes.mkdir()
    directory = out / "world"
    world(directory)
    before = {path.relative_to(directory).as_posix(): pin(path)
              for path in directory.rglob("*") if path.is_file()}
    commands = [
        ["javac", "-cp", str(jar), "-d", str(classes), str(HERE / "PickerObservation.java")],
        ["java", "-Xverify:all", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
         "-cp", os.pathsep.join(map(str, [classes, jar, *libraries])),
         "PickerObservation", str(jar), str(directory)],
    ]
    for phase, command in zip(("compile", "observe"), commands):
        process = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True,
                                 timeout=90, check=False)
        (out / (phase + ".stdout")).write_bytes(process.stdout)
        (out / (phase + ".stderr")).write_bytes(process.stderr)
        require(process.returncode == 0, f"{phase} failed; captures {out}")
        if phase == "compile":
            with zipfile.ZipFile(jar) as archive:
                originals = set(archive.namelist())
            require(not any(path.relative_to(classes).as_posix() in originals
                            for path in classes.rglob("*.class")), "observer cannot shadow originals")
    lines = (out / "observe.stdout").read_text(encoding="utf-8").splitlines()
    observed = [json.loads(line) for line in lines if line.startswith("{")]
    require(len(observed) == 28 and observed[-1]["complete"] and observed[-1]["cases"] == 27,
            "complete original 27-case inventory")
    completion = observed[-1]
    require(completion["jar_sha256"] == JAR_SHA and completion["network_attempts"] == 0
            and completion["native_link_attempts"] == 0, "original archive and no-network guards")
    rows = list(map(normal_row, observed[:-1]))
    require(rows == reference["rows"], f"original observation differs; captures {out}")
    after = {path.relative_to(directory).as_posix(): pin(path)
             for path in directory.rglob("*") if path.is_file()}
    require(before.keys() == after.keys() and all(before[name] == after[name]
            for name in before if name != "session.lock"), "only private session.lock may change")
    require(pin(jar)["sha256"] == JAR_SHA, "unchanged original archive after observation")
    for library, sha in zip(libraries, LIBRARIES.values()):
        require(pin(library)["sha256"] == sha, "unchanged SDK metadata after observation")
    result = dict(status="PASS", rows=rows, completion=completion, commands=commands,
                  fixture_before=before, fixture_after=after,
                  scope=reference["scope"], captures=str(out))
    (out / "report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8",
                                     newline="\n")
    print(json.dumps(dict(status="PASS", cases=27, network_attempts=0,
                         native_link_attempts=0, captures=str(out))))


if __name__ == "__main__":
    main()
