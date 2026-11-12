"""Compare original/native page policy; report exact PCM separately.

Known synthesis differences remain failed PCM comparisons. Policy success
never qualifies PCM equality or playback. No sample tolerance is applied.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def prepared_zero(data, end_trim=False):
    """Change page metadata only; keep every authored audio packet unchanged."""
    data = bytearray(data)
    offset, number = 0, 0
    while offset < len(data):
        need(data[offset:offset + 4] == b"OggS", "authored Ogg page")
        header = 27 + data[offset + 26]
        size = header + sum(data[offset + 27:offset + header])
        need(offset + size <= len(data), "complete authored page")
        if number == 2:
            data[offset + 6:offset + 14] = b"\xff" * 8
        if end_trim and number == 5:
            data[offset + 5] |= 4
            data[offset + 6:offset + 14] = (1000).to_bytes(8, "little")
        data[offset + 22:offset + 26] = b"\0" * 4
        crc = 0
        for value in data[offset:offset + size]:
            crc ^= value << 24
            for _ in range(8):
                crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
        data[offset + 22:offset + 26] = crc.to_bytes(4, "little")
        offset += size
        number += 1
        if end_trim and number == 6:
            return bytes(data[:offset]) + b"tail"
    return bytes(data)


def run(command, root, name):
    result = subprocess.run([str(part) for part in command], stdin=subprocess.DEVNULL,
                            capture_output=True, timeout=60, check=False)
    (root / (name + ".stdout")).write_bytes(result.stdout)
    (root / (name + ".stderr")).write_bytes(result.stderr)
    need(result.returncode == 0, f"{name} exit {result.returncode}: {result.stderr[:2048]!r}")
    need(not result.stderr and len(result.stdout) <= 1048576, f"{name} clean bounded trace")
    return result.stdout.decode("ascii")


def trace(text):
    rows = []
    for line in text.splitlines():
        columns = line.split("\t")
        need(len(columns) == 12, "complete trace record")
        rows.append([columns[0], *map(int, columns[1:])])
    need(rows and rows[0][0] == "INIT" and rows[-1][0] == "CLEANUP", "complete lifecycle")
    need([row[1] for row in rows] == list(range(len(rows))), "complete ordered trace")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("jar", "java", "javac", "native", "fixtures", "work-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--require-pcm", action="store_true")
    args = parser.parse_args()
    need(hashlib.sha256(args.jar.read_bytes()).hexdigest() ==
         "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c", "verified original")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="vorbis-policy-", dir=args.work_dir.resolve()))
    classes = work / "classes"
    classes.mkdir()
    source = Path(__file__).resolve().parents[2] / "reference/client/sound/CodecPolicyProbe.java"
    run([args.javac, "-classpath", classes, "-d", classes, source], work, "javac")
    manifest = json.loads((args.fixtures / "manifest.json").read_text(encoding="utf-8"))
    receipt = {"scope": "page/buffer policy; exact PCM separately", "cases": []}
    profiles = [(name, 131072, None) for name in
                ("zero-mono32k.ogg", "mono32k.ogg", "stereo48k.ogg", "mono44k-long.ogg", "chain.ogg")]
    profiles += [("mono44k-long.ogg", 131072, maximum) for maximum in (-1, 0, 1, 25000, 268435456)]
    profiles += [("prepared-zero.ogg", size, None) for size in (256, 512, 2048)]
    profiles += [("prepared-trim.ogg", 512, None)]
    for name, streaming_size, maximum in profiles:
        case = work / f"{name}-{streaming_size}-{maximum}"
        case.mkdir()
        original, native = case / "original", case / "native"
        original.mkdir()
        native.mkdir()
        if name == "chain.ogg":
            encoded = b"".join((args.fixtures / item).read_bytes()
                               for item in manifest["chain_recipe"])
        else:
            source_name = "zero-mono32k.ogg" if name.startswith("prepared-") else name
            encoded = (args.fixtures / source_name).read_bytes()
            need(digest(encoded) == {key: manifest["files"][source_name][key]
                                    for key in ("bytes", "sha256")}, "pinned authored input")
            if name.startswith("prepared-"):
                encoded = prepared_zero(encoded, name == "prepared-trim.ogg")
        path = case / "input.ogg"
        path.write_bytes(encoded)
        original_command = [args.java, "-Xmx256m", "-classpath", classes, "CodecPolicyProbe",
                            args.jar.resolve(), path, original, streaming_size]
        native_command = [args.native, path, native, "trace", streaming_size]
        if maximum is not None:
            original_command += ["all", maximum]
            native_command = [args.native, path, native, "trace-all", maximum, streaming_size]
        left = trace(run(original_command, case, "original"))
        right = trace(run(native_command, case, "native"))
        left_files = {p.name: p.read_bytes() for p in original.glob("*.pcm")}
        right_files = {p.name: p.read_bytes() for p in native.glob("*.pcm")}
        pairs = []
        for filename in sorted(left_files.keys() | right_files.keys()):
            expected, actual = left_files.get(filename, b""), right_files.get(filename, b"")
            pairs.append({"file": filename, "original": digest(expected), "native": digest(actual),
                          "different_bytes": sum(a != b for a, b in zip(expected, actual)),
                          "unpaired_bytes": abs(len(expected) - len(actual))})
        receipt["cases"].append({"name": name, "streaming_size": streaming_size,
                                  "maximum": maximum, "input": digest(encoded),
                                  "policy_equal": left == right, "pcm_equal": left_files == right_files,
                                  "original_trace": left, "native_trace": right, "pcm": pairs})
    receipt["policy_equal"] = all(case["policy_equal"] for case in receipt["cases"])
    receipt["pcm_equal"] = all(case["pcm_equal"] for case in receipt["cases"])
    (work / "comparison.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"Vorbis policy: {len(receipt['cases'])} inputs; policy_equal={receipt['policy_equal']}; "
          f"pcm_equal={receipt['pcm_equal']}")
    print(f"Receipt: {work / 'comparison.json'}")
    need(receipt["policy_equal"], "exact page policy mismatch; see comparison.json")
    if args.require_pcm:
        need(receipt["pcm_equal"], "exact PCM mismatch; see comparison.json")


if __name__ == "__main__":
    main()
