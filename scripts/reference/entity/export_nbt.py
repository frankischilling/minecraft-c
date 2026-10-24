#!/usr/bin/env python3
"""Export complete captured original ItemEntity NBT stdout, without game assets.

Only a successful retained five-child capture supplies expected bytes. Public
provenance contains pins and scoped receipt summaries, never private commands,
paths, environment values, logs, JARs or executable/source contents.
"""

import argparse
import base64
import hashlib
import importlib.util
import json
import math
from pathlib import Path, PurePosixPath
import re
import zlib

ROOT = Path(__file__).resolve().parents[3]
PARSER = ROOT / "scripts/reference/entity/capture_nbt.py"
OBSERVER = ROOT / "tests/reference/entity/EntityNbtObservation.java"
PARSER_PIN = {"bytes":20954,"sha256":"abdab449aff652e42eb8a383037c2785ca97bfa50ccaebbcd44e47b52cd18ab0"}
OBSERVER_PIN = {"bytes":19856,"sha256":"33aa338d986852cbb7ef596bdc4011b5d12de81c9b989a54628f93a9fcf115f3"}
CLASS_PIN = {"bytes":24298,"sha256":"ce440ac29bfcf5d3f9c854ebeeebd7ab542d63eb26b02347b15c69190defae41"}
EMPTY = {"bytes":0,"sha256":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}
JAR_PINS = {
    "client":{"bytes":1465375,"sha256":"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"},
    "server":{"bytes":503100,"sha256":"033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"},
}
JDK_PINS = {
    "java":{"bytes":50344,"sha256":"974fd93b1279f87db5c86e15469dc5f76e708c3a341847523170c532118c8841"},
    "javac":{"bytes":24232,"sha256":"cb8056d3878c2bd252e0f6316e009cdf8a140ef7cb38878c62b8aa2c75fbd5f3"},
    "modules":{"bytes":128648619,"sha256":"2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8"},
    "jvm":{"bytes":12681896,"sha256":"5539382b75556a9873fafa2d8b7d0b36b169ac436a3608467da4f74c2600cbce"},
    "java_native":{"bytes":148136,"sha256":"18b105254c16e1d83614ff4e32cd62ce402fbe6339bdd1b24ccc7d999fed7f5f"},
    "release":{"bytes":1667,"sha256":"fdf03ea4c142f886ff60e4f2fb44b1a9f8c47aa9784c8d34fc7f8cbada48cf44"},
    "source_archive":{"bytes":50855389,"sha256":"5d0afb30b29d1eff95502604e9415d361f0f503553f32416320c58c1f0619e1e"},
}
# Reviewed immutable original bytes. A different Throwable identity or corpus
# needs explicit review/pin update; no normalization fabricates these bytes.
RAW_PINS = {
    "client":{"bytes":126301,"sha256":"617a75f530c85ad0f2e5e0f50910b945d7899463195a4c652da9a8f9bcb379db"},
    "server":{"bytes":126301,"sha256":"80aa14511ca8a5c75893eceda152313813147f0791c7b11ee5b03eee09bd048f"},
}
SCHEMA = "ENTITY_NBT_CAPTURED_ORIGINAL_V1"
SCOPE = "twenty constructed original ItemEntity NBT cases per side; 52 selected owner fields"
TRANSFORM = "zlib+base64 of complete original stdout; no rows removed or normalized"
PRIVACY = "complete stdout and decoded THROW texts reviewed; no private paths or secrets"
MAX_RAW = 131072
MAX_COMPRESSED = 65536
MAX_FIXTURE = 512 * 1024
ROLES = ("compile","original-client","native-client","original-server","native-server")
SHA = re.compile(r"[0-9a-f]{64}\Z")
PRIVATE = re.compile(r"(?i)(?:\b[a-z]:[\\/]|(?:^|[\s\"'(])/(?:home|users|private|tmp|var|mnt|etc|opt|root)/|"
                     r"\\\\[a-z0-9_.-]+[\\/]|(?:https?|file|plugin)://|"
                     r"(?:password|secret|api[_-]?key|authorization)\s*[:=]|bearer\s+[a-z0-9])")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(raw):
    return {"bytes":len(raw),"sha256":hashlib.sha256(raw).hexdigest()}


def file_pin(path):
    digest = hashlib.sha256()
    length = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            length += len(block)
            require(length <= 512 * 1024 * 1024,"bounded retained input")
            digest.update(block)
    return {"bytes":length,"sha256":digest.hexdigest()}


def bounded_read(path, maximum):
    require(path.is_file() and not path.is_symlink(),"ordinary retained file: " + path.name)
    with path.open("rb") as stream:
        raw = stream.read(maximum + 1)
    require(len(raw) <= maximum,"bounded retained file: " + path.name)
    return raw


def canonical(value):
    return json.dumps(value,ensure_ascii=True,sort_keys=True,separators=(",",":")).encode("ascii")


def strict_json(raw):
    def unique(pairs):
        result = {}
        for key,value in pairs:
            require(key not in result,"duplicate JSON field")
            result[key] = value
        return result
    def invalid(value):
        raise ValueError("nonfinite JSON number: " + value)
    return json.loads(raw.decode("ascii"),object_pairs_hook=unique,parse_constant=invalid)


def pin(value):
    require(type(value) is dict and set(value) == {"bytes","sha256"} and type(value["bytes"]) is int and
            0 <= value["bytes"] <= 512 * 1024 * 1024 and type(value["sha256"]) is str and SHA.fullmatch(value["sha256"]),
            "strict length/SHA-256 pin")
    return value


def load_parser():
    require(file_pin(PARSER) == PARSER_PIN,"reviewed immutable NBT protocol parser")
    spec = importlib.util.spec_from_file_location("entity_original_nbt_protocol",PARSER)
    require(spec is not None and spec.loader is not None,"NBT protocol loader")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    require(len(module.CASES) == 20 and len(module.FIELD_KEYS) == 52,"finite recorded protocol")
    return module


def privacy(raw):
    text = raw.decode("ascii")
    require(PRIVATE.search(text) is None,"private content in original stdout; preserve artifact privately")
    count = 0
    for line in text.splitlines():
        if line.startswith("THROW\t"):
            fields = line.split("\t")
            require(len(fields) == 4,"complete Throwable row for privacy inspection")
            decoded = bytes.fromhex(fields[3]).decode("utf-8")
            require(PRIVATE.search(decoded) is None,"private content in actual Throwable; preserve artifact privately")
            count += 1
    require(count == 9,"complete reviewed Throwable inventory")


def capture_inputs(values):
    require(type(values) is dict and 16 <= len(values) <= 128,"bounded original capture input inventory")
    for name,value in values.items():
        require(type(name) is str and re.fullmatch(r"(?:[a-z_]+|(?:src|include)/[A-Za-z0-9_./-]+\.[ch])",name),
                "public role or repository-relative input key")
        require(all(part not in (".","..") for part in name.split("/")),"contained public input key")
        pin(value)
    require(values.get("observer") == OBSERVER_PIN,"actual original observer pin")
    for side,value in JAR_PINS.items(): require(values.get(side) == value,"actual original archive pin")
    for name,value in JDK_PINS.items(): require(values.get(name) == value,"actual original JDK pin")
    require(all(name in values for name in ("controller","profile_source","probe_source","native_probe")),
            "complete collector/native/profile input pins")


def receipt_summaries(values):
    require(type(values) is list and len(values) == 5,"exact five child summaries")
    for role,row in zip(ROLES,values):
        require(type(row) is dict and set(row) == {"role","exit_code","reaped","stdin_empty","stdout","stderr"} and
                row["role"] == role and type(row["exit_code"]) is int and row["exit_code"] == 0 and
                row["reaped"] is True and row["stdin_empty"] is True,"complete successful child summary")
        pin(row["stdout"]); pin(row["stderr"])
        require(row["stderr"] == EMPTY,"empty actual child stderr")
    require(values[0]["stdout"] == EMPTY,"empty actual compiler stdout")
    require(values[1]["stdout"] == RAW_PINS["client"] and values[3]["stdout"] == RAW_PINS["server"],
            "actual original stdout receipt binding")


def load_fixture(raw):
    require(0 < len(raw) <= MAX_FIXTURE,"bounded public fixture JSON")
    package = strict_json(raw)
    require(type(package) is dict and set(package) == {"schema","scope","tested_commit","whole_game_parity",
            "natural_population","cases_per_side","normalized_rows_per_side","selected_fields","transformation","provenance","traces"},
            "exact public fixture fields")
    require(package["schema"] == SCHEMA and package["scope"] == SCOPE and package["tested_commit"] is None and
            package["whole_game_parity"] is False and package["natural_population"] is False and
            type(package["cases_per_side"]) is int and package["cases_per_side"] == 20 and
            type(package["normalized_rows_per_side"]) is int and package["normalized_rows_per_side"] == 2422 and
            type(package["selected_fields"]) is int and package["selected_fields"] == 52 and package["transformation"] == TRANSFORM,
            "immutable finite original observation scope")
    provenance = package["provenance"]
    require(type(provenance) is dict and set(provenance) == {"capture_receipt","capture_inputs","capture_inputs_pin",
            "collector","protocol_parser","observer","compiled_observer","exporter","archives","jdk","children","privacy"},
            "public provenance whitelist")
    pin(provenance["capture_receipt"]); pin(provenance["exporter"])
    capture_inputs(provenance["capture_inputs"])
    require(provenance["capture_inputs_pin"] == fingerprint(canonical(provenance["capture_inputs"])) and
            provenance["collector"] == provenance["capture_inputs"]["controller"] and
            provenance["protocol_parser"] == PARSER_PIN and provenance["observer"] == OBSERVER_PIN and
            provenance["compiled_observer"] == CLASS_PIN and provenance["archives"] == JAR_PINS and
            provenance["jdk"] == JDK_PINS and provenance["privacy"] == PRIVACY,"exact original/protocol provenance joins")
    require(provenance["exporter"] == file_pin(Path(__file__).resolve()),"reviewed exporter source binding")
    receipt_summaries(provenance["children"])
    require(type(package["traces"]) is dict and set(package["traces"]) == {"client","server"},"both original sides")
    parser = load_parser()
    originals, normalized = {}, {}
    for side in ("client","server"):
        trace = package["traces"][side]
        require(type(trace) is dict and set(trace) == {"raw","compressed","encoding","payload"} and
                trace["raw"] == RAW_PINS[side] and trace["encoding"] == "zlib+base64","immutable complete original byte identity")
        pin(trace["compressed"])
        require(0 < trace["compressed"]["bytes"] <= MAX_COMPRESSED and type(trace["payload"]) is str and
                0 < len(trace["payload"]) <= 4 * ((MAX_COMPRESSED + 2) // 3),"bounded compressed original payload")
        compressed = base64.b64decode(trace["payload"],validate=True)
        require(base64.b64encode(compressed).decode("ascii") == trace["payload"] and fingerprint(compressed) == trace["compressed"],
                "canonical complete compressed payload")
        decoder = zlib.decompressobj()
        original = decoder.decompress(compressed,trace["raw"]["bytes"] + 1)
        require(len(original) <= MAX_RAW and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and
                fingerprint(original) == trace["raw"],"bounded exact one-stream decompression")
        privacy(original)
        rows = parser.parse(original,side,True)
        require(len(rows) == 2422,"complete captured original normalized rows")
        originals[side], normalized[side] = original,rows
    return package,originals,normalized


def retained_path(capture, name):
    require(type(name) is str and "\\" not in name and ":" not in name,"relative retained name")
    parts = PurePosixPath(name).parts
    require(parts and not PurePosixPath(name).is_absolute() and all(part not in (".","..") for part in parts),"contained retained path")
    path = capture.joinpath(*parts)
    for ancestor in (path,*path.parents):
        require(not ancestor.is_symlink(),"ordinary retained path")
        if ancestor == capture: break
    return path


def validate_capture(capture):
    receipt_path = capture / "capture.json"
    receipt_raw = bounded_read(receipt_path,1024 * 1024)
    report = strict_json(receipt_raw)
    require(report["schema"] == 1 and report["status"] == "passed" and report["whole_game_parity"] is False and
            report["natural_population"] is False and report["inputs_before"] == report["inputs_after"],
            "actual successful unchanged-input original capture")
    require(report["scope"] == "twenty constructed original/native ItemEntity NBT cases per side","recorded finite capture scope")
    inputs = report["inputs_before"]
    capture_inputs(inputs)
    require(report["compiled_before"] == report["compiled_after"] == {"EntityNbtObservation.class":CLASS_PIN},
            "unchanged actual compiled observer")
    retained = report["retained_files"]
    require(type(retained) is dict and 10 <= len(retained) <= 256,"complete bounded retained file inventory")
    actual = {path.relative_to(capture).as_posix() for path in capture.rglob("*") if path.is_file() and path != receipt_path}
    require(actual == set(retained),"complete exact retained capture inventory")
    for name,value in retained.items():
        pin(value)
        require(file_pin(retained_path(capture,name)) == value,"retained capture bytes changed: " + name)
    require(retained.get("classes/EntityNbtObservation.class") == CLASS_PIN,"actual class bytes bound to receipt")
    for name in ("client","server","observer","controller","profile_source","native_probe","probe_source"):
        require(any(key.startswith("inputs/") and value == inputs[name] for key,value in retained.items()),
                "actual archived input copy: " + name)
    children = report["children"]
    require(type(children) is list and len(children) == 5,"exact five complete original capture children")
    summaries, pids = [],set()
    for role,child in zip(ROLES,children):
        require(type(child["exit_code"]) is int and child["exit_code"] == 0 and child["reaped"] is True and
                child["stdin"] == "DEVNULL" and type(child["pid"]) is int and child["pid"] > 0 and child["pid"] not in pids and
                type(child["seconds"]) in (int,float) and math.isfinite(child["seconds"]) and 0 <= child["seconds"] <= 65 and
                not any(key in child for key in ("failure","journal_error")),"complete natural-zero/reaped child")
        pids.add(child["pid"])
        command = child["command"]
        require(type(command) is list and command and all(type(value) is str for value in command),"actual bounded child command")
        if role == "compile":
            require(command[-1].replace("\\","/").split("/")[-1] == "EntityNbtObservation.java" and
                    command[1:5] == ["-J-Xmx256m","-encoding","UTF-8","-d"],"actual observer compilation command")
        elif role.startswith("original-"):
            side = role.split("-")[1]
            require(command[-3:-1] == ["EntityNbtObservation",side] and "-Xint" in command and "-Xmx256m" in command,
                    "actual original side invocation")
        else:
            require(command[-1] == role.split("-")[1],"actual native side invocation")
        for stream in ("stdout","stderr"):
            require(retained.get(role + "." + stream) == pin(child[stream]),"actual child stream receipt binding")
        summaries.append({"role":role,"exit_code":0,"reaped":True,"stdin_empty":True,
                          "stdout":child["stdout"],"stderr":child["stderr"]})
    receipt_summaries(summaries)
    parser = load_parser()
    originals = {}
    for side,index in (("client",1),("server",3)):
        comparison = report["comparison"][side]
        require(comparison["passed"] is True and type(comparison["cases"]) is int and comparison["cases"] == 20 and
                type(comparison["compared_rows"]) is int and comparison["compared_rows"] == 2422 and
                retained.get(side + ".classes.log") == comparison["original_class_log"],"complete paired capture comparison")
        raw = bounded_read(capture / ("original-" + side + ".stdout"),MAX_RAW)
        require(fingerprint(raw) == children[index]["stdout"] == RAW_PINS[side],"actual reviewed original stdout bytes")
        privacy(raw)
        rows = parser.parse(raw,side,True)
        native = bounded_read(capture / ("native-" + side + ".stdout"),1024 * 1024)
        require(len(rows) == 2422 and parser.parse(native,side,False) == rows,"recomputed complete original/native captured comparison")
        originals[side] = raw
    provenance = {"capture_receipt":fingerprint(receipt_raw),"capture_inputs":inputs,"capture_inputs_pin":fingerprint(canonical(inputs)),
                  "collector":inputs["controller"],"protocol_parser":PARSER_PIN,"observer":OBSERVER_PIN,"compiled_observer":CLASS_PIN,
                  "exporter":file_pin(Path(__file__).resolve()),"archives":JAR_PINS,"jdk":JDK_PINS,"children":summaries,"privacy":PRIVACY}
    return provenance,originals,retained


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-dir",type=Path,required=True)
    parser.add_argument("--fixture",type=Path,required=True)
    args = parser.parse_args()
    capture = args.capture_dir.resolve(strict=True)
    output = args.fixture.absolute()
    require(capture.is_dir() and not output.exists() and not output.is_symlink() and not output.resolve().is_relative_to(capture),
            "fresh public destination outside retained capture")
    provenance,originals,retained = validate_capture(capture)
    traces = {}
    for side,raw in originals.items():
        compressed = zlib.compress(raw,9)
        require(len(compressed) <= MAX_COMPRESSED,"bounded exported compressed original")
        traces[side] = {"raw":fingerprint(raw),"compressed":fingerprint(compressed),"encoding":"zlib+base64",
                        "payload":base64.b64encode(compressed).decode("ascii")}
    package = {"schema":SCHEMA,"scope":SCOPE,"tested_commit":None,"whole_game_parity":False,"natural_population":False,
               "cases_per_side":20,"normalized_rows_per_side":2422,"selected_fields":52,"transformation":TRANSFORM,
               "provenance":provenance,"traces":traces}
    encoded = (json.dumps(package,ensure_ascii=True,indent=2,sort_keys=True) + "\n").encode("ascii")
    _,restored,_ = load_fixture(encoded)
    require(restored == originals,"exact round-trip original bytes")
    require(file_pin(capture / "capture.json") == provenance["capture_receipt"] and
            file_pin(Path(__file__).resolve()) == provenance["exporter"],"unchanged capture/exporter before public write")
    for name,value in retained.items(): require(file_pin(retained_path(capture,name)) == value,"capture changed during export")
    with output.open("xb") as stream: stream.write(encoded)
    print("Exported both complete original stdout traces: 20 cases and 2422 comparison rows per side; no game assets.")


if __name__ == "__main__":
    main()
