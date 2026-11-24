"""Compare unchanged original CodecWav and native source policy with authored input."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile
import time

from authority import identity, need, verify_archive
from fixtures import RECIPES, encode, materialize, profiles


MAX_STREAM_BYTES = 1048576
MAX_FILE_BYTES = 1048576
MAX_DIRECTORY_BYTES = 4194304
TRACE = re.compile(r"^(INIT-(?:true|false)|read|readAll|CLEANUP)\t")
PCM = re.compile(r"^pcm-([0-9]+)\.bin$")


class Unavailable(RuntimeError):
    pass


def consume(path, maximum):
    before = path.lstat()
    need(stat.S_ISREG(before.st_mode) and not path.is_symlink(), "regular retained file")
    need(before.st_size <= maximum, "retained file bound")
    raw = path.read_bytes()
    after = path.lstat()
    need((before.st_ino, before.st_size, before.st_mtime_ns) ==
         (after.st_ino, after.st_size, after.st_mtime_ns) and len(raw) == before.st_size,
         "stable complete retained read")
    return raw


def parse_trace(raw, original):
    rows, diagnostics = [], []
    for line in raw.decode("utf-8").splitlines():
        if not TRACE.match(line):
            need(original, "native trace contains unexpected output")
            diagnostics.append(line)
            continue
        values = line.split("\t")
        need(len(values) == 14, "complete WAV trace row")
        for index in (2, 3, 4):
            need(values[index] in ("true", "false"), "trace boolean spelling")
        for index in (1, 5, 8, 9, 10, 11, 12, 13):
            number = int(values[index])
            need(str(number) == values[index], "canonical trace integer")
        need(int(values[1]) == len(rows), "complete ordered trace sequence")
        need(int(values[5]) >= -1 and (values[4] == "true") == (int(values[5]) >= 0),
             "buffer presence and length agree")
        need(0 <= int(values[11]) <= 0xffffffff and values[12] in ("0", "1"), "format bit fields")
        need(values[6] in ("NONE", "NegativeArraySizeException", "NullPointerException"),
             "declared source exception domain")
        need(values[7] in ("NONE", "PCM_SIGNED", "PCM_UNSIGNED", "PCM_FLOAT", "ALAW", "ULAW"),
             "declared format domain")
        rows.append(values)
    need(2 <= len(rows) <= 14 and rows[0][0].startswith("INIT-") and
         rows[-1][0] == "CLEANUP", "complete WAV lifecycle")
    need(all(row[0] in ("read", "readAll") for row in rows[1:-1]), "declared call chronology")
    need((rows[0][0] == "INIT-true") == (rows[0][2] == "true"), "initialization result")
    if rows[0][0] == "INIT-false":
        need(len(rows) == 2, "failed initialization performs only cleanup")
    return rows, diagnostics


def pcm_inventory(root, rows, original):
    expected = {"pcm-" + row[1] + ".bin": int(row[5]) for row in rows if row[4] == "true"}
    members = {path.name for path in root.iterdir()}
    allowed = set(expected)
    if original:
        allowed |= {"environment.properties", "class-resources.tsv"}
    need(members == allowed, "complete result directory inventory")
    data, pins = {}, {}
    for name, size in expected.items():
        need(PCM.match(name), "result filename")
        raw = consume(root / name, MAX_FILE_BYTES)
        need(len(raw) == size, "returned buffer length equals actual file")
        data[name], pins[name] = raw, identity(raw)
    return data, pins


def call_plan(rows, profile):
    if rows[0][0] == "INIT-false":
        return
    need(all(row[2] == "true" for row in rows), "initialized flag survives calls/cleanup")
    need(all(row[7:14] == rows[0][7:14] for row in rows[:-1]), "retained source format")
    flags = [row[3] == "true" for row in rows]
    need(flags == sorted(flags), "EOS never resets in this call plan")
    if profile["mode"] == "all":
        need([row[0] for row in rows] == ["INIT-true", "readAll", "read", "readAll", "CLEANUP"],
             "complete fresh readAll call plan")
    else:
        need(len(rows) >= 5 and [row[0] for row in rows[-3:]] == ["read", "readAll", "CLEANUP"],
             "complete repeated-read/readAll/cleanup suffix")
        loop = rows[1:-3]
        need(1 <= len(loop) <= 10 and all(row[0] == "read" for row in loop), "declared read loop")
        need(all(row[3] == "false" for row in loop[:-1]), "read loop stops at first EOS")
        need(len(loop) == 10 or loop[-1][3] == "true", "read loop ended at its declared boundary")


def class_resources(root, authority):
    rows = consume(root / "class-resources.tsv", MAX_FILE_BYTES).decode("ascii").splitlines()
    result = {}
    for row in rows:
        name, size, digest = row.split("\t")
        need(name not in result and re.fullmatch(r"[0-9a-f]{64}", digest), "class-resource identity")
        result[name] = {"bytes": int(size), "sha256": digest}
    need(len(result) == 14, "complete original/JDK resource inventory")
    for name, pin in authority["classes"].items():
        need(result.get(name) == pin["identity"], "runtime original resource joins supplied archive")
    environment = consume(root / "environment.properties", MAX_FILE_BYTES).decode("utf-8")
    need("java.specification.version=17" in environment and "native.byte.order=LITTLE_ENDIAN" in environment,
         "observed little-endian JDK17 profile")
    return {"class_resources": result, "environment": environment}


def live_bounds(root):
    members = list(root.iterdir())
    need(len(members) <= 20, "live result member bound")
    total = 0
    for path in members:
        info = path.lstat()
        need(stat.S_ISREG(info.st_mode) and not path.is_symlink(), "live regular result file")
        need(info.st_size <= MAX_FILE_BYTES, "live result file bound")
        total += info.st_size
    need(total <= MAX_DIRECTORY_BYTES, "live result byte bound")


def run(command, root, name, receipt, output=None, timeout=20):
    row = {"name": name, "command": [str(part) for part in command], "exit_code": None,
           "timeout": False, "terminated": False}
    receipt["phases"].append(row)
    stdout_path, stderr_path = root / (name + ".stdout"), root / (name + ".stderr")
    child = None
    start = time.monotonic()
    env = dict(os.environ)
    for key in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS"):
        env.pop(key, None)
    try:
        with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
            child = subprocess.Popen(row["command"], cwd=root, stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr, env=env)
            row["pid"] = child.pid
            while child.poll() is None:
                need(stdout_path.stat().st_size <= MAX_STREAM_BYTES and
                     stderr_path.stat().st_size <= MAX_STREAM_BYTES, "live process stream bound")
                if output is not None:
                    live_bounds(output)
                if time.monotonic() - start > timeout:
                    row["timeout"] = True
                    raise RuntimeError(name + " exceeded its process deadline")
                time.sleep(0.01)
            row["exit_code"] = child.wait()
    finally:
        if child is not None and child.poll() is None:
            row["terminated"] = True
            child.kill()
            row["exit_code"] = child.wait()
        row["elapsed_seconds"] = time.monotonic() - start
        row["stdout"] = identity(consume(stdout_path, MAX_STREAM_BYTES))
        row["stderr"] = identity(consume(stderr_path, MAX_STREAM_BYTES))
    need(row["exit_code"] == 0 and not row["terminated"], name + " completed naturally")
    need(stderr_path.stat().st_size == 0, name + " emitted stderr")
    return consume(stdout_path, MAX_STREAM_BYTES)


def resolve_tool(value):
    path = Path(value)
    if path.is_file():
        return path.resolve()
    found = shutil.which(value)
    if found:
        return Path(found).resolve()
    raise Unavailable("tool not found: " + value)


def compare_case(case, profile, authority):
    left, diagnostics = parse_trace(consume(case / "original.stdout", MAX_STREAM_BYTES), True)
    right, _ = parse_trace(consume(case / "native.stdout", MAX_STREAM_BYTES), False)
    call_plan(left, profile)
    call_plan(right, profile)
    left_data, left_pins = pcm_inventory(case / "original", left, True)
    right_data, right_pins = pcm_inventory(case / "native", right, False)
    resources = class_resources(case / "original", authority)
    return {"profile": profile, "trace_equal": left == right, "bytes_equal": left_data == right_data,
            "original_trace": left, "native_trace": right,
            "original_buffers": left_pins, "native_buffers": right_pins,
            "original_diagnostics": diagnostics, "original_runtime": resources}


def audit_capture(root):
    receipt = json.loads(consume(root / "comparison.json", 16777216))
    need(receipt["schema"] == 1 and len(receipt["cases"]) == 60, "complete sixty-case receipt")
    wanted = profiles()
    need([case["profile"] for case in receipt["cases"]] == wanted, "declared case inventory")
    expected_authority = json.loads(Path(__file__).with_name("pins.json").read_text(encoding="utf-8"))
    need(receipt["authority"] == {key: expected_authority[key] for key in ("archive", "classes")},
         "canonical archive/class/Code authority")
    need(len(receipt["phases"]) == 121, "compile plus sixty paired executions")
    for index, phase in enumerate(receipt["phases"]):
        parent = root
        if index:
            case_index = (index - 1) // 2
            profile = wanted[case_index]
            parent /= f"{case_index:03d}-{profile['name']}-{profile['mode']}"
        need(phase["exit_code"] == 0 and not phase["timeout"] and not phase["terminated"],
             "natural terminal phase")
        for stream in ("stdout", "stderr"):
            raw = consume(parent / (phase["name"] + "." + stream), MAX_STREAM_BYTES)
            need(identity(raw) == phase[stream], "complete retained process stream")
            if stream == "stderr":
                need(not raw, "empty retained stderr")
    for name, pin in receipt["compiled_probe"].items():
        need(identity(consume(root / "classes" / name, MAX_FILE_BYTES)) == pin,
             "retained compiled authored probe")
    for index, row in enumerate(receipt["cases"]):
        name = f"{index:03d}-{row['profile']['name']}-{row['profile']['mode']}"
        case = root / name
        actual = compare_case(case, wanted[index], receipt["authority"])
        need(actual == row, "retained WAV result readback")
        need(actual["trace_equal"] and actual["bytes_equal"], "exact WAV trace and byte equality")
    for name, pin in receipt["inputs"].items():
        raw = consume(root / "inputs" / (name + ".wav"), MAX_FILE_BYTES)
        need(identity(raw) == {key: pin[key] for key in ("bytes", "sha256")}, "retained authored input")
    need(set(receipt["inputs"]) == {recipe.name for recipe in RECIPES}, "complete fixture recipe set")
    for recipe in RECIPES:
        need(consume(root / "inputs" / (recipe.name + ".wav"), MAX_FILE_BYTES) == encode(recipe),
             "authored input matches its declared recipe")
    need(receipt["passed"], "terminal comparison success")
    return len(receipt["cases"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("jar", "java", "javac", "native", "work-dir"):
        parser.add_argument("--" + name)
    parser.add_argument("--tested-commit")
    parser.add_argument("--audit-only", type=Path)
    args = parser.parse_args()
    if args.audit_only:
        print("WAV retained comparison readback:", audit_capture(args.audit_only.resolve()), "cases passed")
        return 0
    need(all(getattr(args, name.replace("-", "_")) for name in
             ("jar", "java", "javac", "native", "work-dir")), "required comparison arguments")
    jar = Path(args.jar).resolve()
    if not jar.is_file():
        raise Unavailable("local supplied client archive is missing")
    authority = verify_archive(jar)
    java, javac, native = (resolve_tool(getattr(args, name)) for name in ("java", "javac", "native"))
    base = Path(args.work_dir).resolve()
    base.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="wav-original-", dir=base))
    here = Path(__file__).resolve().parent
    source_files = {path.name: identity(consume(path, MAX_FILE_BYTES))
                    for path in (here / "authority.py", here / "fixtures.py", here / "compare.py",
                                 here / "pins.json", here / "WavOriginalProbe.java")}
    receipt = {"schema": 1, "scope": "CodecWav WAVE profile; declared state trace and complete returned bytes",
               "authority": authority, "tools": {str(path): identity(consume(path, 268435456))
                                                    for path in (java, javac, native)},
               "probe_sources": source_files, "declared_tested_commit": args.tested_commit,
               "phases": [], "inputs": {}, "cases": [], "passed": False}
    failure = None
    try:
        classes, inputs = work / "classes", work / "inputs"
        classes.mkdir()
        inputs.mkdir()
        receipt["inputs"] = materialize(inputs)
        run([javac, "--release", "17", "-classpath", classes, "-d", classes, here / "WavOriginalProbe.java"],
            work, "compile", receipt)
        receipt["compiled_probe"] = {path.name: identity(consume(path, MAX_FILE_BYTES))
                                      for path in classes.iterdir()}
        need(set(receipt["compiled_probe"]) == {"WavOriginalProbe.class"}, "only authored probe compiled")
        for index, profile in enumerate(profiles()):
            case = work / f"{index:03d}-{profile['name']}-{profile['mode']}"
            case.mkdir()
            original, output = case / "original", case / "native"
            original.mkdir()
            output.mkdir()
            source = inputs / (profile["name"] + ".wav")
            parameters = [profile["mode"], profile["streaming_size"], profile["file_chunk_size"],
                          profile["max_file_size"]]
            run([java, "-Xmx128m", "-classpath", classes, "WavOriginalProbe", jar, source, original,
                 *parameters], case, "original", receipt, original)
            run([native, source, output, "trace", *parameters], case, "native", receipt, output)
            result = compare_case(case, profile, authority)
            receipt["cases"].append(result)
            need(result["trace_equal"] and result["bytes_equal"], "exact WAV comparison: " + str(profile))
        need(verify_archive(jar) == authority, "original archive unchanged")
        for name, pin in source_files.items():
            need(identity(consume(here / name, MAX_FILE_BYTES)) == pin, "probe source unchanged")
        for path in (java, javac, native):
            need(identity(consume(path, 268435456)) == receipt["tools"][str(path)], "executed tool unchanged")
        receipt["passed"] = True
    except Exception as error:
        failure = error
        receipt["failure"] = {"type": type(error).__name__, "message": str(error)}
    finally:
        (work / "comparison.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        print("WAV comparison receipt:", work / "comparison.json")
    if failure is not None:
        raise failure
    print("WAV original/native exact comparison:", audit_capture(work), "cases passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Unavailable as error:
        print("UNAVAILABLE:", error)
        raise SystemExit(77)
