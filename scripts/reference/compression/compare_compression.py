#!/usr/bin/env python3
"""Compare memory containers with pinned original gzip wrappers and JDK controls."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zlib

from fixtures import fixtures, payload_fixtures, root_fixtures


JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}
MAX_BATCH_BYTES = 4 * 1024 * 1024
MAX_OUTPUT_BYTES = 16 * 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_tool(override: str | None, name: str) -> Path | None:
    found = shutil.which(override or name)
    if found:
        return Path(found).resolve()
    if not override and os.environ.get("JAVA_HOME"):
        candidate = Path(os.environ["JAVA_HOME"]) / "bin" / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return candidate.resolve()
    return None


def run(command: list[str], data: str | None = None) -> subprocess.CompletedProcess[str]:
    if data is not None and len(data.encode("utf-8")) > MAX_BATCH_BYTES:
        raise RuntimeError("probe input batch exceeds limit")
    # Files prevent unexpected child output from growing the parent's memory without limit.
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        result = subprocess.run(command, input=data, text=True, encoding="utf-8", errors="strict",
                                stdout=stdout, stderr=stderr, timeout=120, check=False)
        stdout.seek(0)
        stderr.seek(0)
        output, errors = stdout.read(MAX_OUTPUT_BYTES + 1), stderr.read(MAX_OUTPUT_BYTES + 1)
        if len(output) > MAX_OUTPUT_BYTES or len(errors) > MAX_OUTPUT_BYTES:
            raise RuntimeError("probe process output exceeds limit")
        return subprocess.CompletedProcess(command, result.returncode, output.decode("utf-8"), errors.decode("utf-8"))


def java_major(output: str, tool: str) -> int:
    if tool == "java":
        match = re.search(r"(?m)^\s*java\.specification\.version\s*=\s*(\d+(?:\.\d+)?)", output)
        if not match:
            match = re.search(r'(?m)^\s*(?:openjdk|java) version "(\d+(?:\.\d+)?)', output)
    else:
        match = re.search(r"(?m)^\s*javac\s+(\d+(?:\.\d+)?)", output)
    if not match:
        raise RuntimeError(f"could not determine {tool} major version")
    parts = match.group(1).split(".")
    return int(parts[1] if parts[0] == "1" and len(parts) > 1 else parts[0])


def response(line: str, java: bool) -> dict:
    fields = line.split("\t")
    if fields[0] == "ERR" and len(fields) == (3 if java else 2):
        result = {"status": "ERR", "error": fields[1]}
    elif fields[0] == "OK" and len(fields) == (4 if java else 3) and fields[1].isdigit():
        if not re.fullmatch(r"(?:[0-9a-f]{2})*", fields[2]):
            raise ValueError("invalid output hex from probe")
        result = {"status": "OK", "consumed": int(fields[1]), "hex": fields[2]}
    else:
        raise ValueError(f"invalid probe response: {line[:120]!r}")
    if java:
        result["metadata"] = json.loads(fields[-1])
    return result


class Comparison:
    def __init__(self, work: Path):
        self.work = work
        self.errors: list[str] = []
        self.departures: list[dict] = []
        self.artifacts: list[dict] = []
        self.records: dict[str, list[dict]] = {}

    def check(self, condition: bool, message: str) -> None:
        if not condition:
            self.errors.append(message)

    def batch(self, label: str, command: list[str], requests: list[str], *, java: bool = False) -> list[dict]:
        if not requests or len(requests) > 1024:
            raise RuntimeError("invalid probe batch count")
        data = "\n".join(requests) + "\n"
        result = run(command, data)
        for suffix, content in [("requests.txt", data), ("stdout.txt", result.stdout), ("stderr.txt", result.stderr)]:
            path = self.work / f"{label}-{suffix}"
            path.write_text(content, encoding="utf-8", newline="\n")
            self.artifacts.append({"file": path.name, "sha256": sha256(path)})
        if result.returncode != 0:
            raise RuntimeError(f"{label}: process exited {result.returncode}: {result.stderr[:1000]}")
        lines = result.stdout.splitlines()
        if len(lines) != len(requests):
            raise RuntimeError(f"{label}: {len(lines)} responses for {len(requests)} requests")
        parsed = [response(line, java) for line in lines]
        self.records[label] = parsed
        return parsed

    def expect(self, label: str, result: dict, *, error: str | None = None, data: bytes | None = None,
               consumed: int | None = None, input_closes: int | None = None,
               output_closes: int | None = None) -> bool:
        if error is not None:
            self.check(result.get("error") == error, f"{label}: expected {error!r}, got {result.get('error', result['status'])!r}")
            ok = False
        else:
            ok = result["status"] == "OK"
            self.check(ok, f"{label}: unexpected {result.get('error')}")
            if ok and data is not None:
                self.check(result["hex"] == data.hex(), f"{label}: bytes differ")
            if ok and consumed is not None:
                self.check(result["consumed"] == consumed, f"{label}: consumed count differs")
        if input_closes is not None:
            self.check(result["metadata"]["input_close_calls"] == input_closes, f"{label}: input close count differs")
        if output_closes is not None:
            self.check(result["metadata"]["output_close_calls"] == output_closes, f"{label}: output close count differs")
        return ok


def plans_for_java(cases: list) -> list[dict]:
    plans = []
    for item in cases:
        constructor_error = item.format == "gzip" and item.label.startswith((
            "gzip-truncated-header-", "gzip-invalid-header-", "gzip-truncated-optional-", "gzip-wrong-framing-"))
        plans.append({"label": item.label + "/JDK-drain", "mode": "drain-" + item.format, "wire": item.wire,
                      "error": item.jdk_error, "data": item.decoded,
                      "consumed": item.consumed if item.format == "zlib" and not item.jdk_error else None,
                      "input_closes": 0 if constructor_error else 1, "fixture": item.label})
        if item.format == "gzip":
            plans.append({"label": item.label + "/original-read", "mode": "original-read", "wire": item.wire,
                          "error": item.original_error, "data": item.root, "input_closes": 0 if constructor_error else 1,
                          "fixture": item.label})
        elif item.decoded is not None and not item.jdk_error:
            plans.append({"label": item.label + "/original-raw-root", "mode": "raw-root", "wire": item.decoded,
                          "error": item.original_error, "data": item.root, "input_closes": 0,
                          "consumed": len(item.root) if item.root is not None else None, "fixture": item.label})
    # One-byte delivery separates root completion from compressed read-ahead.
    for item in cases:
        if item.label in {"gzip-root-empty", "gzip-concat-roots", "gzip-concat-split-root", "gzip-corrupt-crc"}:
            for mode, error, data in [("original-read", item.original_error, item.root),
                                       ("drain-gzip", item.jdk_error, item.decoded)]:
                plans.append({"label": item.label + "/one-byte/" + mode, "mode": mode, "wire": item.wire,
                              "error": error, "data": data, "input_closes": 1, "chunk": 1, "fixture": item.label})
    for label, raw in root_fixtures():
        plans.append({"label": "original-write/" + label, "mode": "original-write", "wire": raw,
                      "data": None, "raw": raw, "consumed": len(raw), "input_closes": 0, "output_closes": 1})
    for label, raw in payload_fixtures():
        plans.append({"label": "JDK-write-zlib/" + label, "mode": "write-zlib", "wire": raw,
                      "data": None, "raw": raw, "consumed": len(raw), "input_closes": 0, "output_closes": 1})
    return plans


def request(plan: dict) -> str:
    return plan["mode"] + "\t" + plan["wire"].hex() + ("\t" + str(plan["chunk"]) if "chunk" in plan else "")


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", type=Path, default=root / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=root / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=root / ".local/reference/compression")
    parser.add_argument("--tested-commit")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch(r"[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit requires exactly 40 hexadecimal characters")
    java, javac = find_tool(args.java, "java"), find_tool(args.javac, "javac")
    executable, work = args.executable.resolve(), args.work_dir.resolve()
    jars = {"client": args.client.resolve(), "server": args.server.resolve()}
    source = root / "tests/reference/compression/CompressionProbe.java"
    sources = [source, Path(__file__).resolve(), Path(__file__).resolve().with_name("fixtures.py"),
               root / "docs/reference/compression.md"]
    protected = {path.resolve() for path in [executable, *jars.values(), *sources, java, javac] if path is not None}
    protected.update(Path(value).resolve() for value in [args.java, args.javac] if value and ("/" in value or "\\" in value))
    if any(path == work or work in path.parents for path in protected):
        print("FAIL: work directory contains supplied inputs or source files", file=sys.stderr)
        return 1
    batch_labels = ["c-fixtures", "c-encode", "c-roundtrip"]
    batch_labels += [side + "-" + phase for side in jars for phase in ["reference", "original-to-c", "c-to-reference"]]
    output_names = ["manifest.json", "compile-stdout.txt", "compile-stderr.txt", "CompressionProbe.class",
                    "CompressionProbe$InputSpy.class", "CompressionProbe$OutputSpy.class"]
    output_names += [label + "-" + suffix for label in batch_labels for suffix in ["requests.txt", "stdout.txt", "stderr.txt"]]
    if any((work / name).resolve() in protected for name in output_names):
        print("FAIL: output paths overlap supplied inputs or source files", file=sys.stderr)
        return 1
    comparison = Comparison(work)
    manifest = {"schema": 1, "status": "running", "started_utc": datetime.now(timezone.utc).isoformat(),
                "tested_commit": args.tested_commit.lower() if args.tested_commit else None,
                "commit_record": "supplied by caller" if args.tested_commit else "unrecorded",
                "scope": "Memory gzip/zlib interoperability and separate original raw compound checks; no region/world-save claim",
                "harness_java_release": 11, "python": sys.version, "host": platform.platform(),
                "fixture_zlib_compile": zlib.ZLIB_VERSION, "fixture_zlib_runtime": zlib.ZLIB_RUNTIME_VERSION,
                "java": {"path": str(java) if java else None}, "javac": {"path": str(javac) if javac else None},
                "executable": {"path": str(executable)}, "jars": {side: {"path": str(path)} for side, path in jars.items()},
                "comparison_rules": ["All raw payloads and single-entry canonical NBT bytes compare exactly",
                                     "Compressed block bytes remain recorded but are not required to match across compressors",
                                     "C consumed counts are exact member-end offsets; original gzip delivered bytes are read-ahead observations",
                                     "C full-container, reserved-flag, and recognized-next-header departures are named per fixture"],
                "symbols": {"client": {"utility": "as", "compound": "nu"}, "server": {"utility": "ag", "compound": "iq"}},
                "signatures": ["utility.a(java.io.InputStream)", "utility.a(compound,java.io.OutputStream)",
                               "utility.a(java.io.DataInput)", "utility.a(compound,java.io.DataOutput)"]}

    def save() -> None:
        (work / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")

    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
        missing = [name for name, tool in [("java", java), ("javac", javac)] if tool is None]
        if not executable.is_file():
            missing.append(f"C probe {executable}")
        for side, path in jars.items():
            if not path.is_file():
                missing.append(f"{side} JAR {path}")
            elif sha256(path) != JAR_HASHES[side]:
                missing.append(f"{side} JAR SHA-256 mismatch ({path})")
        if missing:
            manifest.update({"status": "unavailable", "reason": "; ".join(missing),
                             "finished_utc": datetime.now(timezone.utc).isoformat()})
            save()
            print("UNAVAILABLE: " + manifest["reason"], file=sys.stderr)
            return 77
        assert java is not None and javac is not None
        for key, path in [("executable", executable), ("java", java), ("javac", javac)]:
            manifest[key]["sha256"] = sha256(path)
        for side, path in jars.items():
            manifest["jars"][side]["sha256"] = sha256(path)
        manifest["sources"] = [{"file": path.relative_to(root).as_posix(), "sha256": sha256(path)} for path in sources]
        runtime, compiler = run([str(java), "-XshowSettings:properties", "-version"]), run([str(javac), "-version"])
        if runtime.returncode or compiler.returncode:
            raise RuntimeError("Java runtime/compiler version command failed")
        manifest["runtime_info"], manifest["compiler_info"] = runtime.stdout + runtime.stderr, compiler.stdout + compiler.stderr
        runtime_major, compiler_major = java_major(manifest["runtime_info"], "java"), java_major(manifest["compiler_info"], "javac")
        manifest["java"]["major"], manifest["javac"]["major"] = runtime_major, compiler_major
        if runtime_major < 11 or compiler_major < 11:
            manifest.update({"status": "unavailable", "reason": "Java and javac 11 or newer required",
                             "finished_utc": datetime.now(timezone.utc).isoformat()})
            save()
            print("UNAVAILABLE: " + manifest["reason"], file=sys.stderr)
            return 77
        for path in [java.parent.parent / "release", java.parent.parent / "lib/modules"]:
            if path.is_file():
                manifest["java"].setdefault("runtime_files", []).append({"path": str(path), "sha256": sha256(path)})
        compiled = run([str(javac), "--release", "11", "-encoding", "UTF-8", "-d", str(work), str(source)])
        for suffix, value in [("stdout", compiled.stdout), ("stderr", compiled.stderr)]:
            (work / f"compile-{suffix}.txt").write_text(value, encoding="utf-8")
        if compiled.returncode:
            raise RuntimeError("Java harness compilation failed: " + compiled.stderr[:1000])
        manifest["compiled_harness_sha256"] = sha256(work / "CompressionProbe.class")
        cases = fixtures()
        java_plans = plans_for_java(cases)
        roots = dict(root_fixtures())
        payloads = payload_fixtures()
        manifest["fixture_counts"] = {"containers": len(cases), "gzip": sum(case.format == "gzip" for case in cases),
                                      "zlib": sum(case.format == "zlib" for case in cases), "encode_payloads": len(payloads)}
        c_requests = [f"decode-{item.format}{suffix}\t{item.wire.hex()}" for item in cases for suffix in ["", "-nbt"]]
        c_results = comparison.batch("c-fixtures", [str(executable)], c_requests)
        for index, item in enumerate(cases):
            comparison.expect("C/" + item.label, c_results[index * 2], error=item.c_error,
                              data=item.decoded, consumed=item.consumed)
            comparison.expect("C/" + item.label + "/NBT", c_results[index * 2 + 1],
                              error=item.c_error or item.nbt_error, data=item.root, consumed=item.consumed)
        encode_plans = [(fmt, label, raw) for fmt in ["gzip", "zlib"] for label, raw in payloads]
        encoded = comparison.batch("c-encode", [str(executable)],
                                   ["encode-" + fmt + "\t" + raw.hex() for fmt, _, raw in encode_plans])
        roundtrip_requests, roundtrip_plans = [], []
        for (fmt, label, raw), result in zip(encode_plans, encoded):
            if comparison.expect("C/encode-" + fmt + "/" + label, result, consumed=len(raw)):
                if fmt == "gzip":
                    comparison.check(result["hex"].startswith("1f8b08000000000000ff"), "C gzip header differs")
                for suffix in (["", "-nbt"] if label in roots else [""]):
                    roundtrip_requests.append("decode-" + fmt + suffix + "\t" + result["hex"])
                    roundtrip_plans.append((fmt + "/" + label + suffix, raw, len(bytes.fromhex(result["hex"]))))
        roundtrips = comparison.batch("c-roundtrip", [str(executable)], roundtrip_requests)
        for (label, raw, consumed), result in zip(roundtrip_plans, roundtrips):
            comparison.expect("C/roundtrip/" + label, result, data=raw, consumed=consumed)
        side_counts = {}
        for side, jar in jars.items():
            command = [str(java), "-cp", str(work), "CompressionProbe", side, str(jar)]
            originals = comparison.batch(side + "-reference", command, [request(plan) for plan in java_plans], java=True)
            original_by_label = {}
            transfers, transfer_plans = [], []
            for plan, result in zip(java_plans, originals):
                original_by_label[plan["label"]] = result
                ok = comparison.expect(side + "/" + plan["label"], result, error=plan.get("error"), data=plan.get("data"),
                                       consumed=plan.get("consumed"), input_closes=plan.get("input_closes"),
                                       output_closes=plan.get("output_closes"))
                if ok and plan["mode"] in {"original-write", "write-zlib"}:
                    fmt = "gzip" if plan["mode"] == "original-write" else "zlib"
                    label = plan["label"].split("/", 1)[1]
                    for suffix in (["", "-nbt"] if label in roots else [""]):
                        transfers.append("decode-" + fmt + suffix + "\t" + result["hex"])
                        transfer_plans.append((plan["label"] + suffix, plan["raw"], len(bytes.fromhex(result["hex"]))))
            for index, item in enumerate(cases):
                if item.departure:
                    comparison.departures.append({"side": side, "fixture": item.label, "reason": item.departure,
                                                   "C": c_results[index * 2],
                                                   "original": original_by_label.get(item.label + "/original-read"),
                                                   "JDK_drain": original_by_label[item.label + "/JDK-drain"]})
            transferred = comparison.batch(side + "-original-to-c", [str(executable)], transfers)
            for (label, raw, consumed), result in zip(transfer_plans, transferred):
                comparison.expect(side + "/original-to-C/" + label, result, data=raw, consumed=consumed)
            reread_plans = []
            for (fmt, label, raw), result in zip(encode_plans, encoded):
                if result["status"] != "OK":
                    continue
                wire = bytes.fromhex(result["hex"])
                reread_plans.append({"label": "C-to-JDK/" + fmt + "/" + label, "mode": "drain-" + fmt,
                                     "wire": wire, "data": raw, "input_closes": 1,
                                     "consumed": len(wire) if fmt == "zlib" else None})
                if label in roots:
                    reread_plans.append({"label": "C-to-original/" + fmt + "/" + label,
                                         "mode": "original-read" if fmt == "gzip" else "raw-root",
                                         "wire": wire if fmt == "gzip" else raw, "data": raw,
                                         "input_closes": 1 if fmt == "gzip" else 0})
            rereads = comparison.batch(side + "-c-to-reference", command, [request(plan) for plan in reread_plans], java=True)
            for plan, result in zip(reread_plans, rereads):
                comparison.expect(side + "/" + plan["label"], result, data=plan["data"], consumed=plan.get("consumed"),
                                  input_closes=plan["input_closes"])
            side_counts[side] = {"reference_records": len(originals), "original_gzip_reads": sum(p["mode"] == "original-read" for p in java_plans),
                                 "original_gzip_writes": len(roots), "JDK_zlib_writes": len(payloads),
                                 "original_to_c": len(transfers), "c_to_reference": len(reread_plans)}
        manifest["side_counts"] = side_counts
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError, RuntimeError) as error:
        comparison.errors.append(str(error))
    manifest.update({"status": "fail" if comparison.errors else "pass", "finished_utc": datetime.now(timezone.utc).isoformat(),
                     "errors": comparison.errors, "intentional_departures": comparison.departures,
                     "artifacts": comparison.artifacts, "records": comparison.records})
    try:
        work.mkdir(parents=True, exist_ok=True)
        save()
    except OSError as error:
        print(f"FAIL: cannot write manifest: {error}", file=sys.stderr)
        return 1
    if comparison.errors:
        for error in comparison.errors[:30]:
            print("FAIL: " + error, file=sys.stderr)
        print(f"{len(comparison.errors)} failures; full records: {work}", file=sys.stderr)
        return 1
    for side, counts in manifest["side_counts"].items():
        print(f"PASS {side}: {counts['reference_records']} reference records, {counts['original_gzip_reads']} original gzip reads, "
              f"{counts['original_gzip_writes']} original gzip writes, {counts['original_to_c']} original-to-C, "
              f"{counts['c_to_reference']} C-to-reference")
    print(f"PASS: {len(cases)} containers; {len(encoded)} C encodes; {len(comparison.departures)} explicit departure records")
    print(f"Memory containers and separate raw-root checks only; full manifest: {work}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
