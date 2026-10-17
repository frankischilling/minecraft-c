#!/usr/bin/env python3
"""Compare raw NBT with the pinned original client and server classes."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys

from fixtures import construction_fixtures, packet_fixtures, read_fixtures, u16hex


JAR_HASHES = {
    "client": "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
    "server": "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d",
}


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
    if override:
        return None
    if os.environ.get("JAVA_HOME"):
        candidate = Path(os.environ["JAVA_HOME"]) / "bin" / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return candidate.resolve()
    return None


def run(command: list[str], data: str | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, input=data, text=True, encoding="utf-8", errors="strict",
                          capture_output=True, timeout=120, check=False)


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


def response(line: str) -> dict:
    fields = line.split("\t")
    if len(fields) == 2 and fields[0] == "ERR":
        return {"status": "ERR", "error": fields[1]}
    if len(fields) != 4 or fields[0] != "OK" or not fields[1].isdigit():
        raise ValueError(f"invalid probe response: {line[:160]!r}")
    if not re.fullmatch(r"(?:[0-9a-f]{2})*", fields[2]):
        raise ValueError("invalid canonical byte hex in response")
    return {"status": "OK", "consumed": int(fields[1]), "wire": fields[2],
            "tree": json.loads(fields[3])}


def written_tree(tree: list) -> list:
    """Expected read-back change from Java writeFloat/writeDouble, preserving initial raw evidence."""
    kind, name, value = tree
    if kind == 5:
        bits = int(value, 16)
        if bits & 0x7F800000 == 0x7F800000 and bits & 0x007FFFFF:
            value = "7fc00000"
    elif kind == 6:
        bits = int(value, 16)
        if bits & 0x7FF0000000000000 == 0x7FF0000000000000 and bits & 0x000FFFFFFFFFFFFF:
            value = "7ff8000000000000"
    elif kind == 9:
        value = [value[0], [written_tree(child) for child in value[1]]]
    elif kind == 10:
        value = [written_tree(child) for child in value]
    return [kind, name, value]


def compound_order_wire(wire: bytes) -> bytes:
    """Sort compound child slices only; preserve every other raw byte, including trailing data."""
    if len(wire) > 256 * 1024:
        raise ValueError("compound-order comparison exceeds byte limit")
    position = 0
    nodes = 0

    def take(length: int) -> bytes:
        nonlocal position
        if length < 0 or position + length > len(wire):
            raise ValueError("truncated compound-order comparison input")
        result = wire[position:position + length]
        position += length
        return result

    def payload(kind: int, depth: int) -> bytes:
        nonlocal nodes
        nodes += 1
        if nodes > 256 * 1024 or depth > 32:
            raise ValueError("compound-order comparison exceeds structural limit")
        fixed = {0: 0, 1: 1, 2: 2, 3: 4, 4: 8, 5: 4, 6: 8}
        if kind in fixed:
            return take(fixed[kind])
        if kind == 7:
            header = take(4)
            length = int.from_bytes(header, "big", signed=True)
            return header + take(length)
        if kind == 8:
            header = take(2)
            return header + take(int.from_bytes(header, "big"))
        if kind == 9:
            header = take(5)
            length = int.from_bytes(header[1:], "big", signed=True)
            if length < 0 or length > 256 * 1024:
                raise ValueError("invalid compound-order list length")
            children = [payload(header[0], depth + 1) for _ in range(length)]
            return header + b"".join(children)
        if kind == 10:
            children = []
            while True:
                if position == len(wire):
                    raise ValueError("missing compound-order compound terminator")
                if wire[position] == 0:
                    terminator = take(1)
                    break
                children.append(named(depth + 1))
            return b"".join(sorted(children)) + terminator
        raise ValueError("unsupported tag in compound-order comparison")

    def named(depth: int) -> bytes:
        header = take(1)
        if header[0] == 0:
            return header
        name_length = take(2)
        name = take(int.from_bytes(name_length, "big"))
        return header + name_length + name + payload(header[0], depth)

    normalized = named(0)
    return normalized + wire[position:]


class Comparison:
    def __init__(self, work: Path):
        self.work = work
        self.errors: list[str] = []
        self.departures: list[dict] = []
        self.order_differences: list[dict] = []
        self.artifacts: list[dict] = []

    def check(self, condition: bool, message: str) -> None:
        if not condition:
            self.errors.append(message)

    def batch(self, label: str, command: list[str], requests: list[str]) -> list[dict]:
        data = "\n".join(requests) + "\n"
        process = run(command, data)
        for suffix, content in [("requests.txt", data), ("stdout.txt", process.stdout),
                                ("stderr.txt", process.stderr)]:
            path = self.work / f"{label}-{suffix}"
            path.write_text(content, encoding="utf-8", newline="\n")
            self.artifacts.append({"file": path.name, "sha256": sha256(path)})
        if process.returncode != 0:
            raise RuntimeError(f"{label}: process exited {process.returncode}: {process.stderr[:1000]}")
        lines = process.stdout.splitlines()
        if len(lines) != len(requests):
            raise RuntimeError(f"{label}: {len(lines)} responses for {len(requests)} requests")
        return [response(line) for line in lines]

    def outcome(self, label: str, result: dict, expected_error: str | None) -> bool:
        if expected_error:
            self.check(result.get("error") == expected_error,
                       f"{label}: expected {expected_error!r}, got {result.get('error', result['status'])!r}")
            return False
        self.check(result["status"] == "OK", f"{label}: unexpected {result.get('error')}")
        return result["status"] == "OK"

    def equal(self, label: str, original: dict, actual: dict, *, compound_order: bool,
              consumed: int | None = None, after_write: bool = False,
              actual_after_write: bool = False) -> None:
        if original["status"] != "OK" or actual["status"] != "OK":
            self.errors.append(f"{label}: cannot compare unsuccessful responses")
            return
        expected_tree = written_tree(original["tree"]) if after_write else original["tree"]
        actual_tree = written_tree(actual["tree"]) if actual_after_write else actual["tree"]
        self.check(expected_tree == actual_tree, f"{label}: logical trees differ")
        self.check(actual["consumed"] == (original["consumed"] if consumed is None else consumed),
                   f"{label}: consumed lengths differ")
        if original["wire"] != actual["wire"]:
            if compound_order:
                try:
                    equivalent = compound_order_wire(bytes.fromhex(original["wire"])) == \
                        compound_order_wire(bytes.fromhex(actual["wire"]))
                except ValueError as error:
                    self.errors.append(f"{label}: compound-order comparison failed: {error}")
                    return
                if not equivalent:
                    self.errors.append(f"{label}: canonical bytes differ beyond compound child order")
                    return
                self.order_differences.append({"comparison": label,
                                               "reason": "Only compound child record order differs; raw byte slices match after recursively sorting compound children",
                                               "original_sha256": hashlib.sha256(bytes.fromhex(original["wire"])).hexdigest(),
                                               "c_sha256": hashlib.sha256(bytes.fromhex(actual["wire"])).hexdigest()})
            else:
                self.errors.append(f"{label}: canonical bytes differ")


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--client", type=Path, default=root / "b1.7.3.jar")
    parser.add_argument("--server", type=Path, default=root / "b1.7.3_server.jar")
    parser.add_argument("--java")
    parser.add_argument("--javac")
    parser.add_argument("--work-dir", type=Path, default=root / ".local/reference/nbt")
    parser.add_argument("--tested-commit", help="Exact 40-character commit associated with the C build")
    args = parser.parse_args()
    if args.tested_commit and not re.fullmatch(r"[0-9a-fA-F]{40}", args.tested_commit):
        parser.error("--tested-commit must contain exactly 40 hexadecimal characters")
    java, javac = find_tool(args.java, "java"), find_tool(args.javac, "javac")
    executable = args.executable.resolve()
    jars = {"client": args.client.resolve(), "server": args.server.resolve()}
    work = args.work_dir.resolve()
    comparison = Comparison(work)
    source = root / "tests/reference/nbt/NbtProbe.java"
    sources = [source, Path(__file__).resolve(), Path(__file__).resolve().with_name("fixtures.py")]
    protected = {path.resolve() for path in [executable, *jars.values(), *sources, java, javac] if path is not None}
    protected.update(Path(value).resolve() for value in [args.java, args.javac]
                     if value and ("/" in value or "\\" in value))
    conflicting_inputs = sorted(str(path) for path in protected if path == work or work in path.parents)
    if conflicting_inputs:
        print("FAIL: work directory contains supplied inputs or source files: " + "; ".join(conflicting_inputs),
              file=sys.stderr)
        return 1
    output_names = ["manifest.json", "compile-stdout.txt", "compile-stderr.txt", "NbtProbe.class"]
    for label in ["c-read", "client-original", "client-original-to-c", "client-c-to-original",
                  "server-original", "server-original-to-c", "server-c-to-original"]:
        output_names += [f"{label}-{suffix}" for suffix in ["requests.txt", "stdout.txt", "stderr.txt"]]
    unsafe_outputs = [name for name in output_names if (work / name).resolve() in protected]
    if unsafe_outputs:
        print("FAIL: output paths overlap supplied inputs or source files: " + ", ".join(unsafe_outputs),
              file=sys.stderr)
        return 1
    manifest = {
        "schema": 1, "started_utc": datetime.now(timezone.utc).isoformat(), "status": "running",
        "scope": "Raw named NBT and original object writes; packet string framing controls; no gzip, region, or world-save claim",
        "harness_java_release": 11,
        "tested_commit": args.tested_commit.lower() if args.tested_commit else None,
        "commit_record": "supplied by caller" if args.tested_commit else "unrecorded",
        "host": platform.platform(), "python": sys.version,
        "executable": {"path": str(executable)},
        "java": {"path": str(java) if java else None},
        "javac": {"path": str(javac) if javac else None},
        "jars": {side: {"path": str(jar)} for side, jar in jars.items()},
        "symbols": {"client": {"base": "ij", "compound_helper": "as", "packet": "ki"},
                    "server": {"base": "fo", "compound_helper": "ag", "packet": "gt"}},
        "reflected_signatures": ["base.b(java.io.DataInput)", "base.a(base,java.io.DataOutput)",
                                 "compound_helper.a(java.io.DataInput)", "base.a(byte)",
                                 "packet.a(java.lang.String,java.io.DataOutputStream)",
                                 "packet.a(java.io.DataInputStream,int)"],
        "floating_point_evidence": "Initial logical trees retain raw float/double bits. Writer-boundary comparisons explicitly expect all NaNs to become 7fc00000/7ff8000000000000; original constructed writes independently assert those bytes.",
        "compound_order_evidence": "Flagged wire differences must match after sorting only compound child byte slices recursively. Names and string bytes, types, primitive and array bytes, list headers and order, root bytes, and trailing bytes are preserved exactly. The bounded walker does not decode text or replace original-class parse evidence.",
    }
    try:
        work.mkdir(parents=True, exist_ok=True)
        (work / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
                                             encoding="utf-8")
        missing = [name for name, tool in [("java", java), ("javac", javac)] if tool is None]
        if not executable.is_file():
            missing.append(f"C probe {executable}")
        for side, jar in jars.items():
            if not jar.is_file():
                missing.append(f"{side} JAR {jar}")
            elif sha256(jar) != JAR_HASHES[side]:
                missing.append(f"{side} JAR SHA-256 mismatch ({jar})")
        if missing:
            reason = "; ".join(missing)
            manifest.update({"finished_utc": datetime.now(timezone.utc).isoformat(),
                             "status": "unavailable", "reason": reason})
            (work / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
                                                 encoding="utf-8")
            print("UNAVAILABLE: " + reason, file=sys.stderr)
            return 77
        assert java is not None and javac is not None
        manifest["executable"]["sha256"] = sha256(executable)
        manifest["java"]["sha256"] = sha256(java)
        manifest["javac"]["sha256"] = sha256(javac)
        for side, jar in jars.items():
            manifest["jars"][side]["sha256"] = sha256(jar)
        manifest["sources"] = [{"file": path.relative_to(root).as_posix(), "sha256": sha256(path)} for path in sources]
        fixtures, constructions, packets = read_fixtures(), construction_fixtures(), packet_fixtures()
        manifest["fixture_counts"] = {"read": len(fixtures), "constructed_write": len(constructions),
                                      "packet_controls": len(packets)}
        runtime = run([str(java), "-XshowSettings:properties", "-version"])
        compiler = run([str(javac), "-version"])
        if runtime.returncode or compiler.returncode:
            raise RuntimeError("Java runtime/compiler version command failed")
        manifest["runtime_info"] = runtime.stdout + runtime.stderr
        manifest["compiler_info"] = compiler.stdout + compiler.stderr
        runtime_major = java_major(manifest["runtime_info"], "java")
        compiler_major = java_major(manifest["compiler_info"], "javac")
        manifest["java"]["major"] = runtime_major
        manifest["javac"]["major"] = compiler_major
        if runtime_major < 11 or compiler_major < 11:
            manifest.update({"finished_utc": datetime.now(timezone.utc).isoformat(),
                             "status": "unavailable", "reason": "Java and javac 11 or newer required"})
            (work / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
                                                 encoding="utf-8")
            print(f"UNAVAILABLE: Java and javac 11 or newer required "
                  f"(selected java {runtime_major}, javac {compiler_major})", file=sys.stderr)
            return 77
        for artifact in [java.parent.parent / "release", java.parent.parent / "lib/modules"]:
            if artifact.is_file():
                manifest["java"].setdefault("runtime_files", []).append({"path": str(artifact), "sha256": sha256(artifact)})
        compiled = run([str(javac), "--release", "11", "-encoding", "UTF-8", "-d", str(work), str(source)])
        (work / "compile-stdout.txt").write_text(compiled.stdout, encoding="utf-8")
        (work / "compile-stderr.txt").write_text(compiled.stderr, encoding="utf-8")
        if compiled.returncode:
            raise RuntimeError(f"Java harness compilation failed: {compiled.stderr[:1000]}")
        manifest["compiled_harness_sha256"] = sha256(work / "NbtProbe.class")
        original_requests = [item.request() for item in fixtures] + [item.request() for item in constructions]
        original_requests += ["packet\t" + u16hex(text) for _, text, _ in packets]
        c_baseline = comparison.batch("c-read", [str(executable)], [item.request() for item in fixtures])
        side_counts = {}
        for side, jar in jars.items():
            command = [str(java), "-cp", str(work), "NbtProbe", side, str(jar)]
            originals = comparison.batch(f"{side}-original", command, original_requests)
            reads = originals[:len(fixtures)]
            writes = originals[len(fixtures):len(fixtures) + len(constructions)]
            controls = originals[len(fixtures) + len(constructions):]
            transfers = []
            for item, original, actual in zip(fixtures, reads, c_baseline):
                label = f"{side}/{item.label}"
                java_ok = comparison.outcome(label + "/original", original, item.java_error)
                c_ok = comparison.outcome(label + "/C", actual, item.c_error)
                if item.departure and java_ok and not c_ok:
                    comparison.departures.append({"side": side, "fixture": item.label,
                                                   "reason": item.departure, "original": original,
                                                   "c": actual})
                if java_ok and c_ok:
                    comparison.equal(label, original, actual, compound_order=item.compound_order)
                if java_ok:
                    transfers.append((label, original, item.compound_order, False))
            for item, original in zip(constructions, writes):
                label = f"{side}/{item.label}"
                if comparison.outcome(label, original, item.java_error):
                    if item.expected_wire is not None:
                        comparison.check(original["wire"] == item.expected_wire.hex(),
                                         label + ": original constructed write differs from fixture bytes")
                    transfers.append((label, original, item.compound_order, item.heterogeneous))
            for (label, text, expected_error), original in zip(packets, controls):
                if comparison.outcome(f"{side}/{label}", original, expected_error):
                    raw = text.encode("utf-16-be", "surrogatepass")
                    expected = struct.pack(">H", len(raw) // 2) + raw
                    comparison.check(original["wire"] == expected.hex(), f"{side}/{label}: packet framing differs")
                    comparison.check(original["tree"] == u16hex(text), f"{side}/{label}: packet value differs")
                    comparison.check(original["consumed"] == len(expected), f"{side}/{label}: packet consumed length differs")
            transferred = comparison.batch(f"{side}-original-to-c", [str(executable)],
                                           ["generic\t" + item[1]["wire"] for item in transfers])
            reread_requests = []
            reread_expectations = []
            for item, actual in zip(transfers, transferred):
                label, original, compound_order, heterogeneous = item
                if not comparison.outcome(label + "/original-to-C", actual, None):
                    continue
                expected_consumed = len(bytes.fromhex(original["wire"])) - (2 if heterogeneous else 0)
                if heterogeneous:
                    expected_tree = [9, "", [2, [[2, "", 1], [2, "", 0]]]]
                    comparison.check(actual["tree"] == expected_tree, label + ": heterogeneous-list parsed values differ")
                    comparison.check(actual["consumed"] == expected_consumed, label + ": heterogeneous-list consumed length differs")
                else:
                    comparison.equal(label + "/original-to-C", original, actual,
                                     compound_order=compound_order, consumed=expected_consumed, after_write=True)
                # Re-read original-produced bytes and C-produced bytes in the actual original classes.
                reread_requests += ["generic\t" + original["wire"], "generic\t" + actual["wire"]]
                reread_expectations += [(label + "/original-write-read", actual, compound_order, expected_consumed),
                                        (label + "/C-to-original", actual, compound_order,
                                         len(bytes.fromhex(actual["wire"])))]
            # Include the C rewrite of every successful initial parse, even when it differs in compound order.
            for item, actual in zip(fixtures, c_baseline):
                if actual["status"] == "OK":
                    reread_requests.append("generic\t" + actual["wire"])
                    reread_expectations.append((f"{side}/{item.label}/initial-C-to-original", actual,
                                                item.compound_order, len(bytes.fromhex(actual["wire"]))))
            rereads = comparison.batch(f"{side}-c-to-original", command, reread_requests)
            for original, (label, actual, compound_order, expected_consumed) in zip(rereads, reread_expectations):
                if comparison.outcome(label, original, None):
                    comparison.equal(label, original, actual, compound_order=compound_order,
                                     consumed=actual["consumed"], actual_after_write=True)
                    comparison.check(original["consumed"] == expected_consumed, label + ": original consumed length differs")
            side_counts[side] = {"original_reads": len(fixtures), "original_constructed_writes": len(constructions),
                                 "packet_controls": len(packets), "original_to_c": len(transfers),
                                 "c_to_original_and_constructed_rereads": len(reread_requests)}
        manifest["side_counts"] = side_counts
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError, RuntimeError) as error:
        comparison.errors.append(str(error))
    manifest.update({"finished_utc": datetime.now(timezone.utc).isoformat(),
                     "status": "fail" if comparison.errors else "pass",
                     "errors": comparison.errors, "intentional_departures": comparison.departures,
                     "compound_order_differences": comparison.order_differences,
                     "artifacts": comparison.artifacts})
    try:
        work.mkdir(parents=True, exist_ok=True)
        (work / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
                                             encoding="utf-8")
    except OSError as error:
        print(f"FAIL: cannot write manifest: {error}", file=sys.stderr)
        return 1
    if comparison.errors:
        for error in comparison.errors[:30]:
            print("FAIL: " + error, file=sys.stderr)
        if len(comparison.errors) > 30:
            print(f"({len(comparison.errors) - 30} further failures recorded in manifest)", file=sys.stderr)
        print(f"Full inputs, outputs, and manifest: {work}", file=sys.stderr)
        return 1
    for side, counts in manifest["side_counts"].items():
        print(f"PASS {side}: {counts['original_reads']} original reads, "
              f"{counts['original_constructed_writes']} constructed writes, {counts['packet_controls']} packet controls, "
              f"{counts['original_to_c']} original-to-C and "
              f"{counts['c_to_original_and_constructed_rereads']} original re-reads")
    print(f"Recorded {len(comparison.departures)} intentional negative-list departures and "
          f"{len(comparison.order_differences)} compound-order byte differences.")
    print(f"Raw NBT only; manifest and full records: {work}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
