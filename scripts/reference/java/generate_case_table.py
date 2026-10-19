#!/usr/bin/env python3
"""Generate English uppercase data from the immutable complete Java observation."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re

from compare_string_case import FIXTURE, SHA256


def generate(raw):
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise ValueError("captured fixture fingerprint")
    rows = [line.split("\t") for line in raw.decode("ascii").splitlines()]
    if rows[0] != ["STRING_UPPER_EN_V1"] or rows[-1] != ["END", "1114112", "1485", "3"]:
        raise ValueError("complete uppercase traversal")
    output = ["/* English full-uppercase mappings observed from the pinned JDK17",
              " * over every code-point value; unchanged values use identity.",
              " * Source capture SHA256:", " * " + SHA256 + ". */",
              "static const uppercase_mapping uppercase_mappings[] = {"]
    previous = -1
    for row in rows[1:-1]:
        if len(row) != 3 or row[0] != "MAP" or re.fullmatch(r"0|[1-9][0-9]*", row[1]) is None:
            raise ValueError("mapping declaration")
        point = int(row[1])
        if not previous < point <= 0x10ffff or re.fullmatch(r"(?:[0-9a-f]{4}){1,3}", row[2]) is None:
            raise ValueError("ordered mapping and UTF16 width")
        previous = point
        units = [int(row[2][i:i + 4], 16) for i in range(0, len(row[2]), 4)]
        values = units + [0] * (3 - len(units))
        output.append("    {UINT32_C(" + str(point) + "), {" +
                      ", ".join(f"UINT16_C(0x{value:04x})" for value in values) +
                      "}, " + str(len(units)) + "},")
    if len(rows) != 1487:
        raise ValueError("mapping count")
    return ("\n".join(output + ["};"]) + "\n").encode("ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with args.output.open("xb") as stream:
        stream.write(generate(FIXTURE.read_bytes()))


if __name__ == "__main__":
    main()
