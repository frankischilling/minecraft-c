#!/usr/bin/env python3
"""Capture pinned originals and compare finite actual native ItemEntity NBT calls.

Raw original/native NBT bytes and original Throwables remain evidence. The
comparison checks semantic tags and selected raw source-field bits; it does
not compare Java HashMap serialization order or nondeterministic constructors.
"""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
OBSERVER = ROOT / "tests/reference/entity/EntityNbtObservation.java"
PROBE = ROOT / "tests/entity/nbt/nbt_probe.c"
PROFILE_HELPER = ROOT / "scripts/reference/java/capture_nanotime_conversion.py"
PROFILE_HELPER_SHA = "f08873d98d14ee3ce1fa8c650a4f41f71c11934e0319582a21212d8adc4ab02c"
JARS = {"client": (1465375,"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"),
        "server": (503100,"033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d")}
CASES = ("save_narrow", "save_dead", "factory", "factory_missing_item", "read_good", "read_missing_item",
         "motion_nan_ten", "motion_infinities", "motion_neighbors", "motion_short", "motion_wrong", "pos_wrong",
         "rotation_short", "fire_wrong", "count_wrong", "getter_wrong", "null_compound", "empty_vectors",
         "angle_infinity", "missing_scalars")
FIELD_KEYS = ("x", "y", "z", "previous_x", "previous_y", "previous_z", "interpolation_x", "interpolation_y",
              "interpolation_z", "velocity_x", "velocity_y", "velocity_z", "yaw", "pitch", "previous_yaw",
              "previous_pitch", "fall", "y_size", "offset", "width", "height", "fire", "air", "ground",
              "health", "age", "bounds_min_x", "bounds_min_y", "bounds_min_z", "bounds_max_x", "bounds_max_y",
              "bounds_max_z", "stack_present", "stack_id", "stack_count", "stack_damage", "stack_animation",
              "id", "dead", "entity_membership", "chunk_x", "chunk_y", "chunk_z", "pickup_delay", "update_counter",
              "world_null", "watcher_size", "watcher_index", "watcher_type", "watcher_value", "watcher_record_dirty", "watcher_dirty")
DOUBLE_FIELDS = set(FIELD_KEYS[:12] + FIELD_KEYS[26:32])
BOOLEAN_FIELDS = {"ground", "stack_present", "dead", "entity_membership", "world_null", "watcher_record_dirty", "watcher_dirty"}
HEX = re.compile(r"[0-9a-f]+\Z")
ENV_KEYS = ("JAVA_HOME", "PATH", "PATHEXT", "SystemRoot", "WINDIR", "COMSPEC", "TEMP", "TMP", "CLASSPATH",
            "JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "_JAVA_OPTIONS")
INJECTION_KEYS = ("CLASSPATH", "JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "_JAVA_OPTIONS")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    digest = hashlib.sha256()
    length = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
            length += len(block)
    return {"bytes": length, "sha256": digest.hexdigest()}


def run(command, directory, name, receipts, environment, deadline, log=None):
    """Run only this owned child; preserve streams and reap even on failure."""
    arguments = [str(value) for value in command]
    receipt = {"command": arguments, "cwd": str(directory), "stdin": "DEVNULL", "reaped": False,
               "environment": {key: environment.get(key) for key in ENV_KEYS}, "exit_code": None}
    receipts.append(receipt)
    stdout_path, stderr_path = directory / (name + ".stdout"), directory / (name + ".stderr")
    child = None
    failure = None
    started = time.monotonic()
    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
        try:
            child = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                     cwd=directory, env=environment)
            receipt["pid"] = child.pid
            while child.poll() is None:
                if time.monotonic() >= min(started + 60, deadline):
                    failure = "owned child deadline exceeded"
                if os.fstat(stdout.fileno()).st_size > 1024 * 1024 or os.fstat(stderr.fileno()).st_size > 1024 * 1024:
                    failure = "owned child output bound exceeded"
                if log is not None and log.exists() and log.stat().st_size > 4 * 1024 * 1024:
                    failure = "owned child class log bound exceeded"
                if failure:
                    break
                try:
                    child.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    pass
        except BaseException as error:
            failure = f"{type(error).__name__}: {error}"
            raise
        finally:
            if child is not None:
                try:
                    if child.poll() is None:
                        child.kill()
                except OSError as error:
                    failure = f"owned child termination failed: {error}"
                finally:
                    try:
                        child.wait(timeout=5)
                    except (subprocess.TimeoutExpired, OSError) as error:
                        failure = f"owned child reap failed: {error}"
                receipt.update(exit_code=child.returncode, reaped=child.returncode is not None)
            receipt["seconds"] = time.monotonic() - started
            if failure:
                receipt["failure"] = failure
            stdout.flush()
            stderr.flush()
            receipt["stdout"], receipt["stderr"] = fingerprint(stdout_path), fingerprint(stderr_path)
    require(failure is None, failure)
    require(child is not None and child.returncode == 0 and receipt["reaped"], "actual child failed")
    require(receipt["stderr"]["bytes"] == 0, "unexpected actual child stderr")
    require(receipt["stdout"]["bytes"] <= 1024 * 1024, "complete bounded actual stdout")
    if log is not None:
        require(log.is_file() and log.stat().st_size <= 4 * 1024 * 1024,"complete bounded original class log")
    return stdout_path.read_bytes()


def expected_result(name):
    if name.startswith("save_"):
        return "FALSE" if name == "save_dead" else "TRUE"
    if name in ("motion_short", "rotation_short", "empty_vectors"):
        return "INDEX"
    if name in ("motion_wrong", "pos_wrong", "fire_wrong", "count_wrong", "getter_wrong"):
        return "CAST"
    return "NULL" if name == "null_compound" else "OK"


def encoded_tags(raw, case):
    """Independently consume complete actual saved NBT; these fixtures are ASCII."""
    require(0 < len(raw) <= 65536, "bounded complete saved NBT")
    offset = 0
    rows = []
    tags = 0

    def take(count):
        nonlocal offset
        require(0 <= count <= len(raw) - offset, "complete encoded NBT payload")
        value = raw[offset:offset + count]
        offset += count
        return value

    def number(code):
        return struct.unpack(code, take(struct.calcsize(code)))[0]

    def text():
        return take(number(">H")).decode("ascii")

    def payload(kind, path, depth):
        nonlocal tags
        tags += 1
        require(tags <= 64 and depth <= 8, "bounded saved NBT graph")
        if kind == 10:
            children = []
            while True:
                child = number(">B")
                if child == 0:
                    break
                name = text()
                require(name not in [entry[0] for entry in children], "unique encoded field")
                children.append((name, payload(child, path + "/" + name, depth + 1)))
            return [("TAG",case,"output",path,"10",str(len(children)))] + [row for _, values in sorted(children) for row in values]
        if kind == 9:
            child, count = number(">B"), number(">i")
            require(0 <= count <= 16, "bounded encoded List")
            result = [("TAG",case,"output",path,"9",f"{child},{count}")]
            for index in range(count):
                result += payload(child,path + "/" + str(index),depth + 1)
            return result
        if kind in (1,2,3):
            value = f"{number({1:'>b',2:'>h',3:'>i'}[kind]) & 0xffffffff:08x}"
        elif kind in (5,6):
            value = take(4 if kind == 5 else 8).hex()
        elif kind == 8:
            value = "".join(f"{ord(unit):04x}" for unit in text()) or "-"
        else:
            raise ValueError("unexpected actual saved tag kind")
        return [("TAG",case,"output",path,str(kind),value)]

    require(number(">B") == 10 and text() == "", "actual empty-name Compound root")
    rows = payload(10,"$",1)
    require(offset == len(raw), "complete saved NBT byte consumption")
    return rows


def parse(raw, side, original):
    require(0 < len(raw) <= 1024 * 1024 and raw.endswith(b"\n") and b"\r" not in raw, "literal bounded LF trace")
    rows = [tuple(line.split("\t")) for line in raw[:-1].decode("ascii").split("\n")]
    require(rows[:2] == [("ENTITY_NBT_V1",side),
        ("SCOPE","PREPARED_SCALARS","EXCLUDE_ONLY_RANDOM_INTERNALS_AND_HOVER_FROM_SELECTED_OWNER_FIELDS")], "scope/header")
    require(rows[-1] == ("END",str(len(CASES))), "complete finite footer")
    require([row[1] for row in rows if row[0] == "CASE"] == list(CASES), "all twenty cases in source order")
    normalized = []
    bodies = {}
    current = None
    for row in rows[2:-1]:
        require(len(row) >= 2 and row[1] in CASES, "known finite row case")
        if row[0] == "CASE":
            current = row[1]
            require(len(row) == 3 and row[2] == ("SAVE" if current.startswith("save_") else "FACTORY" if current.startswith("factory") else "READ"), "actual operation schema")
            bodies[current] = []
        require(row[1] == current, "complete contiguous case body")
        bodies[current].append(row)
        if row[0] not in ("BYTES", "THROW", "DETAIL"):
            normalized.append(row)
    for name, body in bodies.items():
        results = [row for row in body if row[0] == "RESULT"]
        require(results == [("RESULT",name,expected_result(name))], "actual source result: " + name)
        fields = [row for row in body if row[0] == "FIELD"]
        stages = ("after",) if name.startswith("factory") else ("before","after")
        require([(row[2],row[3]) for row in fields] == [(stage,key) for stage in stages for key in FIELD_KEYS], "complete selected fields: " + name)
        for row in fields:
            require(len(row) == 5, "field row width")
            key, value = row[3:]
            width = 1 if key in BOOLEAN_FIELDS else 16 if key in DOUBLE_FIELDS else 8
            require(len(value) == width and HEX.fullmatch(value) is not None and (width != 1 or value in ("0","1")), "raw field schema")
        require(all(row[4] == f"{CASES.index(name):08x}" for row in fields if row[3] == "id"),
                "actual fresh-class constructor ID continuation")
        refs = [row for row in body if row[0] == "REF"]
        require(len(refs) == 1 and len(refs[0]) == 4, "one complete reference comparison")
        require(refs[0][2:] == ("NA","NA") if name.startswith("factory") else refs[0][2] == "1" and refs[0][3] in ("0","1"), "bounds identity retained")
        held = [row for row in body if row[0] == "HELD"]
        require(not held if name.startswith("factory") else len(held) == 1 and len(held[0]) == 6, "actual old Stack hold capture")
        for row in held:
            require(all(len(value) == 8 and HEX.fullmatch(value) for value in row[2:]), "old Stack fields")
        tag_rows = [row for row in body if row[0] == "TAG"]
        require(tag_rows and all(len(row) == 6 and row[2] == ("output" if name.startswith("save_") else "input") and
                re.fullmatch(r"\$(?:/[A-Za-z_0-9]+)*",row[3]) for row in tag_rows), "complete actual tag view")
        require(len({(row[2],row[3]) for row in tag_rows}) == len(tag_rows), "unique observed logical tag path")
        binaries = [row for row in body if row[0] == "BYTES"]
        if name.startswith("save_"):
            require(len(binaries) == 1 and len(binaries[0]) == 3 and HEX.fullmatch(binaries[0][2]) and len(binaries[0][2]) % 2 == 0, "complete raw saved NBT")
            require(encoded_tags(bytes.fromhex(binaries[0][2]),name) == tag_rows, "encoded NBT matches actual observed semantic tags")
        else:
            require(not binaries, "only successful/false Boolean save records are encoded")
        errors = [row for row in body if row[0] == ("THROW" if original else "DETAIL")]
        exceptional = expected_result(name) in ("CAST","INDEX","NULL")
        require(len(errors) == (1 if exceptional else 0), "complete per-call failure capture")
        if original and errors:
            require(len(errors[0]) == 4 and HEX.fullmatch(errors[0][3]) and len(errors[0][3]) <= 65536,
                    "bounded actual complete Throwable")
            traceback = bytes.fromhex(errors[0][3]).decode("utf-8")
            require(errors[0][2] in ("java.lang.ClassCastException","java.lang.NullPointerException","java.lang.IndexOutOfBoundsException") and
                    errors[0][2] in traceback and "EntityNbtObservation" in traceback, "actual original Throwable frame")
        for row in body:
            require(row[0] in ("CASE","FIELD","TAG","RESULT","REF","HELD","BYTES","THROW" if original else "DETAIL"), "known exact row type")
        if not original and errors:
            require(len(errors[0]) == 8 and all(re.fullmatch(r"(?:0|[1-9][0-9]*)",value) for value in errors[0][2:7]) and
                    re.fullmatch(r"[A-Za-z_]+",errors[0][7]) is not None,
                    "complete native source/child diagnostic frame")
    return normalized


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, required=True)
    parser.add_argument("--native-probe", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    directory = args.output_dir.absolute()
    require(not directory.exists() and not directory.is_symlink(), "fresh retained evidence directory required")
    directory.mkdir(parents=False, exist_ok=False)
    report = {"schema":1,"status":"failed","scope":"twenty constructed original/native ItemEntity NBT cases per side",
              "whole_game_parity":False,"natural_population":False,"children":[],"comparison":{}}
    inputs = {}
    before = None
    deadline = time.monotonic() + 300
    try:
        require(fingerprint(PROFILE_HELPER)["sha256"] == PROFILE_HELPER_SHA,"reviewed JDK profile source")
        spec = importlib.util.spec_from_file_location("entity_nbt_jdk_profile",PROFILE_HELPER)
        profile = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(profile)
        java, javac = args.java.resolve(strict=True), args.javac.resolve(strict=True)
        require(java.parent == javac.parent,"same actual JDK")
        jdk = java.parent.parent
        inputs = {"java":java,"javac":javac,"modules":jdk / "lib/modules","jvm":jdk / "bin/server/jvm.dll",
                  "java_native":jdk / "bin/java.dll","release":jdk / "release","source_archive":jdk / "lib/src.zip",
                  "observer":OBSERVER,"probe_source":PROBE,"controller":Path(__file__).resolve(),
                  "profile_source":PROFILE_HELPER,"native_probe":args.native_probe.resolve(strict=True),
                  "client":args.client_jar.resolve(strict=True),"server":args.server_jar.resolve(strict=True),
                  "constructor_fixture":ROOT / "tests/entity/test_entity.c","cmake":ROOT / "CMakeLists.txt"}
        for folder in ("src/entity","include/mc/entity","src/item/stack","include/mc/item/stack","src/storage/nbt","include/mc/storage/nbt"):
            for path in sorted((ROOT / folder).rglob("*")):
                if path.is_file() and path.suffix in (".c",".h"):
                    inputs[path.relative_to(ROOT).as_posix()] = path
        require(all(path.is_file() and not path.is_symlink() for path in inputs.values()),"actual ordinary input files")
        before = {name:fingerprint(path) for name,path in inputs.items()}
        report["inputs_before"] = before
        require(all(before[name]["sha256"] == value for name,value in profile.PROFILE.items()),"pinned Windows JDK profile")
        require(all(before[side] == {"bytes":value[0],"sha256":value[1]} for side,value in JARS.items()),"verified original archive bytes")
        copied = directory / "inputs"
        copied.mkdir()
        for index,(name,path) in enumerate(inputs.items()):
            if path.suffix in (".c",".h",".java",".py",".txt") or name in JARS or name == "native_probe":
                (copied / (str(index) + "-" + path.name)).write_bytes(path.read_bytes())
        environment = os.environ.copy()
        report["environment"] = {key:environment.get(key) for key in ENV_KEYS}
        report["inherited_environment_keys"] = sorted(environment)
        report["inherited_environment_sha256"] = hashlib.sha256(json.dumps(environment,sort_keys=True).encode("utf-8")).hexdigest()
        require(all(not environment.get(key) for key in INJECTION_KEYS),"inherited VM/tool/classpath injection must be absent, never silently removed")
        classes = directory / "classes"
        classes.mkdir()
        compiled = run([javac,"-J-Xmx256m","-encoding","UTF-8","-d",classes,OBSERVER],directory,"compile",report["children"],environment,deadline)
        require(compiled == b"","unexpected compiler stdout")
        artifacts = sorted(classes.glob("*.class"))
        require([path.name for path in artifacts] == ["EntityNbtObservation.class"],"one complete compiled observer")
        report["compiled_before"] = {path.name:fingerprint(path) for path in artifacts}
        for side in JARS:
            original = run([java,"-Xint","-Xmx256m",f"-Xlog:class+load=info,class+init=info:file={side}.classes.log",
                            "-cp",classes,"EntityNbtObservation",side,inputs[side]],directory,"original-" + side,
                           report["children"],environment,deadline,directory / (side + ".classes.log"))
            native = run([inputs["native_probe"],side],directory,"native-" + side,report["children"],environment,deadline)
            expected, actual = parse(original,side,True),parse(native,side,False)
            if expected != actual:
                differences = [{"index":index,"original":expected[index] if index < len(expected) else None,
                                "native":actual[index] if index < len(actual) else None}
                               for index in range(max(len(expected),len(actual)))
                               if index >= len(expected) or index >= len(actual) or expected[index] != actual[index]]
                report["comparison"][side] = {"passed":False,"differences":differences}
                raise ValueError("actual original/native selected fields differ: " + side)
            report["comparison"][side] = {"passed":True,"cases":len(CASES),"compared_rows":len(expected),
                                          "original_class_log":fingerprint(directory / (side + ".classes.log"))}
        require(len(report["children"]) == 5 and all(child["reaped"] and child["exit_code"] == 0 for child in report["children"]),"five successful reaped owned children")
        report["compiled_after"] = {path.name:fingerprint(path) for path in sorted(classes.glob("*.class"))}
        require(report["compiled_after"] == report["compiled_before"],"compiled observer unchanged")
        report["status"] = "passed"
    except BaseException as error:
        report["failure"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        try:
            if before is not None:
                report["inputs_after"] = {name:fingerprint(path) for name,path in inputs.items()}
                require(report["inputs_after"] == before,"capture inputs changed")
        except BaseException as error:
            report["status"] = "failed"
            report["input_failure"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            report["retained_files"] = {path.relative_to(directory).as_posix():fingerprint(path)
                                         for path in sorted(directory.rglob("*")) if path.is_file()}
            with (directory / "capture.json").open("x",encoding="utf-8",newline="\n") as stream:
                json.dump(report,stream,indent=2,sort_keys=True)
                stream.write("\n")
    print("PASS: 20 actual ItemEntity NBT original/native cases per side; constructed scope only.")


if __name__ == "__main__":
    main()
