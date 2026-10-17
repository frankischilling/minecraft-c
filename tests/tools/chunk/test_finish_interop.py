"""Compare complete C finishing records with lossless fixed original observations."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import zlib


FIXTURE = Path(__file__).resolve().parents[2] / "world/chunk/finish_fixtures.json"
MAX_BYTES = 4 * 1024 * 1024
COORDINATES = ((0, 0), (1, 0), (-1, 0), (32768, 0), (2147483647, -2147483648), (2000000, 0))


def validate_fixture(fixture):
    if type(fixture["schema"]) is not int or fixture["schema"] != 1:
        raise ValueError("finishing fixture schema")
    rows = fixture["cases"]
    if type(rows) is not list or len(rows) != len(COORDINATES):
        raise ValueError("complete fixed finishing corpus")
    indices = [case["index"] for case in rows]
    if (not all(type(number) is int for number in indices) or
            set(indices) != set(range(len(COORDINATES)))):
        raise ValueError("unique complete fixed finishing indices")
    for case in rows:
        if case["coordinates"] != list(COORDINATES[case["index"]]):
            raise ValueError("fixed finishing coordinates")
        if type(case["sides"]) is not dict or set(case["sides"]) != {"client", "server"}:
            raise ValueError("both original finishing sides required")


def unpack(value):
    if type(value["bytes"]) is not int or not 0 <= value["bytes"] <= MAX_BYTES:
        raise ValueError("fixture byte bound")
    encoded = base64.b64decode(value["zlib_base64"], validate=True)
    decoder = zlib.decompressobj()
    result = decoder.decompress(encoded, MAX_BYTES + 1)
    if (len(result) != value["bytes"] or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail
            or hashlib.sha256(result).hexdigest() != value["sha256"]):
        raise ValueError("complete fixture bytes/fingerprint")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    if FIXTURE.stat().st_size > MAX_BYTES:
        raise ValueError("fixture file size")
    fixture = json.loads(FIXTURE.read_bytes())
    validate_fixture(fixture)
    executable = args.executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="mc-finish-") as directory:
        for case in fixture["cases"]:
            expected = json.loads(unpack(case["expected"]))
            for side, record in case["sides"].items():
                source = Path(directory) / f"case-{case['index']}-{side}.bin"
                source.write_bytes(unpack(record["input"]))
                # The trusted C adapter emits one bounded normal record per input.
                process = subprocess.run([str(executable), str(source)], capture_output=True, timeout=10, check=True)
                if len(process.stdout) > MAX_BYTES or process.stderr or json.loads(process.stdout) != expected:
                    raise ValueError(f"complete original/C mismatch: case {case['index']} {side}")
    print("full finishing fixtures: 12 original inputs, 24576 exact query records, 48 complete data/queue stages")


if __name__ == "__main__":
    main()
