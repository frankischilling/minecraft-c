"""Authored-only Player probe and malformed scalar-tape contracts; no JAR required."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys

REFERENCE = Path(__file__).resolve().parents[2] / "reference/entity/player"
SPEC = importlib.util.spec_from_file_location("player_reference_support", REFERENCE / "support.py")
SUPPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUPPORT)
require = SUPPORT.require


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def fbits(value):
    return {"F": struct.pack(">f", value).hex()}


def check_result(path, tape):
    require(path.is_file() and path.stat().st_size < 1048576, "native row extent")
    rows = [json.loads(line) for line in path.read_text(encoding="ascii").splitlines()]
    require([row["case"] for row in rows] == list(range(8)), "eight actual constructor cases")
    tokens = tape.split()
    for i, row in enumerate(rows):
        require(row["status"] == (0 if i < 6 else 10) and
                row["failure_pc"] == (0 if i < 6 else 103 if i == 6 else 68),
                "actual completion/null-failure prefixes")
        require(len(row["fields"]) == 156 and len(row["bounds"]) == 6 and
                len(row["aliases"]) == 3 and len(row["slots"]) == (0 if i == 7 else 45),
                "complete constructor output shape")
        draws = [struct.unpack(">d", bytes.fromhex(t.decode("ascii")))[0]
                 for t in tokens[4 * i:4 * i + 4]]
        formulas = {"ls.aj": fbits(draws[0] * f32(.9) + f32(.1)),
                    "ls.G": fbits(f32(draws[1] + 1) * f32(.01)),
                    "ls.F": fbits(f32(draws[2]) * f32(12398)),
                    "initial_yaw": fbits((draws[3] * 3.1415927410125732) * 2)}
        require(row["formula_replay"] == formulas, "separately labelled scalar formulas")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    require(sys.flags.optimize == 0, "Python assertions must remain enabled")
    executable = args.executable.resolve()
    root = SUPPORT.fresh_directory(args.work_dir, "protocol-")
    resources = root / "resources"
    for name, value in {
            "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                               b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
            "lang/stats_US.lang": b"# Authored statistics resource\n",
            "achievement/map.txt": b""}.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
    bits = ["0000000000000000", "3fd0000000000000", "3fe0000000000000", "3fe8000000000000"] * 8
    canonical = ("\n".join(bits) + "\n").encode("ascii")
    short = " ".join(bits).encode("ascii")
    valid = {"lf": canonical, "mixed-whitespace": b" \t\r\n".join(x.encode() for x in bits),
             "positive-zero": b"0000000000000000 " * 32,
             "maximum-domain": b"3FEFFFFFFFFFFFFF " * 32,
             "smallest-subnormal": b"0000000000000001 " * 32,
             "maximum-bytes": b" " * (4096 - len(short)) + short}
    checks = []
    control = None
    for name, tape in valid.items():
        SUPPORT.validate_tape(tape)
        case = root / name
        case.mkdir()
        tape_path, output, world = case / "draws.txt", case / "native.ndjson", case / "world"
        tape_path.write_bytes(tape)
        result = SUPPORT.run_bounded([executable, resources, world, tape_path, output],
                                     case, "native", timeout=60)
        require(result.returncode == 0 and not result.stderr and
                result.stdout.splitlines() == [b"151 recipes", b"16 achievements"],
                name + " valid exit/diagnostics/banners")
        rows = check_result(output, tape)
        if control is None:
            control = rows
        else:
            for left, right in zip(control, rows):
                def deterministic(row):
                    row = json.loads(json.dumps(row))
                    row.pop("formula_replay")
                    for field in ("sn.aD", "ls.aj", "ls.G", "ls.F"):
                        row["fields"].pop(field)
                    if row["case"] == 7:
                        row["fields"].pop("sn.aS")
                    return row
                require(deterministic(left) == deterministic(right),
                        "independent production constructor deterministic state")
        checks.append({"case": name, "valid": True, "exit": result.returncode})
    base = [x.encode() for x in bits]
    replacements = {
        "short-token": b"0" * 15, "long-token": b"0" * 17, "signed-plus": b"+" + base[0],
        "signed-minus": b"-" + base[0], "hex-prefix": b"0x" + base[0],
        "invalid-hex": b"g" + base[0][1:], "negative-zero": b"8000000000000000",
        "negative": b"bfe0000000000000", "one": b"3ff0000000000000",
        "positive-infinity": b"7ff0000000000000", "negative-infinity": b"fff0000000000000",
        "nan": b"7ff8000000000000", "signaling-nan": b"7ff0000000000001",
        "overflow-token": b"1" * 64, "nul-byte": b"\0" + base[0][1:],
        "nonascii": b"\x80" + base[0][1:]}
    bad = {name: b" ".join([token, *base[1:]]) for name, token in replacements.items()}
    bad.update({"empty": b"", "only-whitespace": b" \t\r\n", "short-tape": b" ".join(base[:31]),
                "extra-token": b" ".join([*base, base[0]]), "trailing-garbage": canonical + b"x",
                "formfeed": b"\f" + canonical, "vertical-tab": b"\v" + canonical,
                "bom": b"\xef\xbb\xbf" + canonical,
                "over-byte-bound": b" " * (4097 - len(short)) + short})
    for name, tape in [*bad.items(), ("missing-tape", None)]:
        if tape is not None:
            try:
                SUPPORT.validate_tape(tape)
            except ValueError:
                pass
            else:
                raise ValueError(name + " was not malformed to the protocol validator")
        case = root / name
        case.mkdir()
        tape_path, output, world = case / "draws.txt", case / "native.ndjson", case / "world"
        if tape is not None:
            tape_path.write_bytes(tape)
        result = SUPPORT.run_bounded([executable, resources, world, tape_path, output],
                                     case, "native", timeout=60)
        require(result.returncode == 2 and not result.stdout and
                result.stderr.splitlines() == [b"Player probe input: invalid draw tape"],
                name + " exact rejection; crashes/sanitizer diagnostics are failures")
        require(not output.exists() and not world.exists(), name + " before output/World creation")
        checks.append({"case": name, "valid": False, "exit": result.returncode})
    report = {"status": "passed", "checks": len(checks), "cases": checks,
              "run_directory": str(root), "executable": SUPPORT.fingerprint(executable),
              "scope": "Authored tape parser and scalar formulas; production constructors use independent Math draws."}
    (root / "report.json").write_bytes((json.dumps(report, indent=2) + "\n").encode("ascii"))
    print(json.dumps(report))


if __name__ == "__main__":
    main()
