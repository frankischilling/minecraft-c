"""Generate the invariant-lowercase CCC partition from qualified JDK observations."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

STDOUT_SHA = "2bc85c1819ea3b202ca4f9d2911f96c3f87a7a57220b1c12e490c130b8de6ad5"
OBSERVATION_SHA = "d6534bd6994f642d0115db8221731a806b6e937eb883cf5b5df9df0298b88d80"


def need(value, message):
    if not value:
        raise ValueError(message)


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def bad_constant(value):
    raise ValueError("nonfinite JSON " + value)


def observed(raw):
    need(hashlib.sha256(raw).hexdigest() == STDOUT_SHA, "qualified exact raw stdout required")
    rows = []
    for line in raw.splitlines():
        kind, separator, body = line.partition(b" ")
        need(separator == b" " and kind.startswith(b"LOWER_HEX_"), "closed transport row")
        rows.append((kind.decode("ascii")[10:], json.loads(body, object_pairs_hook=unique, parse_constant=bad_constant)))
    need([kind for kind, _ in rows] == ["ORIGINS"] + ["BLOCK"] * 17 + ["EXHAUSTIVE"] + ["CONTEXT"] * 222 + ["CONTEXT_SUMMARY", "COMPLETE"], "complete ordered observation grammar")
    result = rows[18][1]
    need((result["first"], result["end"], result["codepoint_values"], result["unicode_scalars"], result["isolated_surrogate_values"]) == (0, 0x110000, 1114112, 1112064, 2048), "full observed codepoint inventory")
    need(result["observation_sha256"] == OBSERVATION_SHA, "complete binary observation digest")
    need(result["simple_width_changes"] == [] and result["non_ascii_simple_hex"] == [] and result["non_ascii_root_hex"] == [], "hex/position proof obligations")
    need(result["root_width_changes"] == [{"cp": 304, "input": {"length": 1, "units": [304]}, "lowered": {"length": 2, "units": [105, 775]}}], "actual ROOT width change")
    full = bytearray(0x110000)
    previous = -1
    for row in result["ccc_nonzero_ranges"]:
        first, last, ccc = row["first"], row["last"], row["ccc"]
        need(all(type(v) is int for v in (first, last, ccc)), "integer CCC fields")
        need(previous < first <= last < len(full) and 0 < ccc < 256, "ordered nonzero CCC ranges")
        full[first:last + 1] = bytes([ccc]) * (last - first + 1)
        previous = last
    need(Counter(full) == Counter({row["ccc"]: row["values"] for row in result["ccc_distribution"]}), "full CCC distribution")
    return result, full


def partition_ranges(full):
    ranges = []
    for point, ccc in enumerate(full):
        if ccc == 0:
            continue
        value = 1 if ccc == 230 else 2
        if ranges and ranges[-1][1] + 1 == point and ranges[-1][2] == value:
            ranges[-1][1] = point
        else:
            ranges.append([point, point, value])
    return ranges


def domain_fingerprint(full):
    # Two independent source contexts per codepoint: I+cp and I+cp+dot.
    # Status bytes 0=admitted, 2=unsupported (the public C enum).
    value = 0xcbf29ce484222325
    counts = [0, 0]
    for cp, ccc in enumerate(full):
        width_risk = cp in (0x130, 0xcc, 0xcd, 0x128)
        first = width_risk or ccc == 230
        second = width_risk or ccc != 0 or cp in (0x49, 0x4a, 0x12e)
        counts[0] += first
        counts[1] += second
        for byte in (2 if first else 0, 2 if second else 0):
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return {"fnv64": f"{value:016x}", "first_refused": counts[0], "second_refused": counts[1], "status_bytes": len(full) * 2}


def generated(raw):
    observation, full = observed(raw)
    ranges = partition_ranges(full)
    lines = [
        "/* Generated from qualified selected-JDK17 actual combining-class observations.",
        " * Generator: tests/reference/core/string_lower_hex/generate_ccc_partition.py",
        " * Raw stdout SHA256: " + STDOUT_SHA,
        " * Ordered full-observation SHA256: " + OBSERVATION_SHA,
        " * Partition 1 = CCC230; 2 = other nonzero CCC; unlisted = CCC0.",
        " * This is property-derived data, not redistributed JDK Class/data bytes. */",
        "static const mc_lower_hex_ccc_range mc_lower_hex_ccc_ranges[] = {",
    ]
    entries = [f"{{UINT32_C(0x{first:06x}), UINT32_C(0x{last:06x}), {value}}},"
               for first, last, value in ranges]
    # Fixed-width entries pack two per line under the pinned repository style.
    lines.extend("    " + " ".join(entries[index:index + 2]) for index in range(0, len(entries), 2))
    lines.append("};")
    output = ("\n".join(lines) + "\n").encode("ascii")
    return output, {"codepoint_values": len(full), "source_ccc_ranges": len(observation["ccc_nonzero_ranges"]), "partition_ranges": len(ranges), "partition_distribution": dict(Counter(0 if c == 0 else 1 if c == 230 else 2 for c in full)), "ccc_bytes_sha256": hashlib.sha256(full).hexdigest(), "domain_context_fingerprint": domain_fingerprint(full), "table_sha256": hashlib.sha256(output).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stdout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output, summary = generated(args.stdout.read_bytes())
    if args.check:
        need(args.output.read_bytes() == output, "tracked partition table matches qualified observations")
    else:
        args.output.write_bytes(output)
    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
