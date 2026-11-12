"""Run World raytrace against authored native GAME storage in a fresh directory."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def encode_native_inputs(data):
    """Authored Java number tokens -> exact binary64 input wire values.

    Keep the raw wire separate from measured output. In particular MSVC's CRT
    textual NaN payload is not Java Double.parseDouble's canonical NaN.
    An explicitly encoded payload is transferred without floating conversion.
    """
    special = {"NaN": "7ff8000000000000", "+NaN": "7ff8000000000000",
               "-NaN": "7ff8000000000000", "Infinity": "7ff0000000000000",
               "+Infinity": "7ff0000000000000", "-Infinity": "fff0000000000000"}
    rows = []
    for line in data.decode("ascii").splitlines():
        tokens = line.split()
        if len(tokens) != 12:
            raise ValueError("twelve authored ray input columns required")
        for index in range(6, 12):
            token = tokens[index]
            if token.startswith("bits:"):
                bits = token[5:]
                if len(bits) != 16 or any(c not in "0123456789abcdefABCDEF" for c in bits):
                    raise ValueError("exact sixteen-digit raw binary64 input required")
            elif token in special:
                bits = special[token]
            else:
                value = float.fromhex(token) if "0x" in token.lower() else float(token)
                bits = struct.pack(">d", value).hex()
            tokens[index] = "bits:" + bits
        rows.append(" ".join(tokens))
    return ("\n".join(rows) + "\n").encode("ascii")


def replay(executable):
    fixture = json.loads((ROOT / "tests/reference/world/raytrace/finite.json").read_text())
    if fixture["schema"] != 1 or fixture["jar_sha256"] != (
            "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"):
        raise AssertionError("World ray observation provenance")
    for path, expected in fixture["source_sha256"].items():
        if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != expected:
            raise AssertionError(f"World ray observer source changed: {path}")
    cases = fixture["cases"]
    if len(cases) != 365 or len({row["input"].split()[0] for row in cases}) != len(cases):
        raise AssertionError("complete unique World ray observations")
    data = ("\n".join(row["input"] for row in cases) + "\n").encode("ascii")
    with tempfile.TemporaryDirectory(prefix="minecraft-c-raytrace-replay-") as directory:
        result = subprocess.run([str(executable), "world-raytrace-probe",
                                 directory.encode().hex()], input=encode_native_inputs(data),
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=160)
    if result.returncode:
        raise AssertionError(f"native World ray replay failed: {result.returncode}\n"
                             + result.stderr.decode(errors="replace"))
    actual = [line for line in result.stdout.decode("ascii").splitlines()
              if line.startswith("RAY\t")]
    if len(actual) != len(cases):
        raise AssertionError("native World ray replay row count")
    for row, observed in zip(cases, actual):
        if row["expected"] != observed:
            raise AssertionError(f"World ray {row['input'].split()[0]}\n"
                                 f"original: {row['expected']}\nnative:   {observed}")
    print(f"PASS: {len(cases)} exact untouched-client World ray observation rows")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="minecraft-c-raytrace-") as directory:
        result = subprocess.run([str(executable), "world-raytrace", directory.encode().hex()],
                                stdin=subprocess.DEVNULL, timeout=160)
    if result.returncode:
        return result.returncode
    replay(executable)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
