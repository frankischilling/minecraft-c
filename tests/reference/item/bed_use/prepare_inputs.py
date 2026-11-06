"""Write the bounded private Item355 input inventory and immutable class hashes."""
import importlib.util
import json
from pathlib import Path
import struct
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
COLUMNS = ("face", "x", "y", "z", "yaw_bits", "count", "foot_id", "head_id",
           "support_foot", "support_head", "remote", "null_mask", "bed_id",
           "foot_air_metadata", "head_air_metadata", "callback_flags")


def bits(value):
    return struct.unpack(">I", struct.pack(">f", value))[0]


def main():
    rows = []

    def add(name, **changes):
        p = dict(zip(COLUMNS, (1, 8, 63, 8, "00000000", 2, 0, 0, 1, 1, 0, 0, 26, 0, 0, 0)))
        p.update(changes)
        rows.append({"name": name, "values": [p[c] for c in COLUMNS]})

    for yaw in (0, 90, 180, 270):
        add("direction_" + str(yaw), yaw_bits=f"{bits(yaw):08x}")
    for yaw in (45, 135, 225, 315, -45, -135, -225, -315):
        middle = bits(yaw)
        for label, word in (("below", middle + (-1 if yaw > 0 else 1)),
                            ("at", middle), ("above", middle + (1 if yaw > 0 else -1))):
            add("threshold_" + str(yaw).replace("-", "minus_") + "_" + label,
                yaw_bits=f"{word:08x}")
    for label, word in (("minus_zero", 0x80000000), ("positive_infinity", 0x7f800000),
                        ("negative_infinity", 0xff800000), ("quiet_nan", 0x7fc01234),
                        ("negative_nan", 0xffc05678), ("maximum_float", 0x7f7fffff),
                        ("minimum_float", 0xff7fffff), ("large_turns", bits(16777216.0))):
        add("yaw_" + label, yaw_bits=f"{word:08x}")
    for face in (-2147483648, -1, 0, 2, 3, 4, 5, 6, 2147483647):
        add("face_guard_" + str(face).replace("-", "minus_"), face=face, null_mask=7)
    add("first_air_blocked", foot_id=1)
    add("second_air_blocked", head_id=1)
    add("first_support_false", support_foot=0)
    add("second_support_false", support_head=0)
    for y in (-1, 126, 127, 2147483647, -2147483648):
        add("height_" + str(y).replace("-", "minus_"), y=y)
    for label, mask in (("world", 1), ("stack", 2), ("player", 4), ("all", 7)):
        add("null_" + label, null_mask=mask)
    add("remote_success", remote=1)
    add("remote_ceiling_both_false", remote=1, y=127)
    for count in (0, -1, -2147483648, 2147483647):
        add("late_count_" + str(count).replace("-", "minus_"), count=count)
    add("bed_id_zero_first_false_head_true", bed_id=0)
    add("bed_id_zero_both_false", bed_id=0, head_air_metadata=8)
    for label, flags in (("head_already_equal", 1), ("head_equal_late_count", 9),
                         ("fail_foot_notice", 2), ("fail_head_notice", 4),
                         ("reload_bed_id", 16), ("late_count_after_notice", 8)):
        add("callback_" + label, callback_flags=flags)
    need_names = {r["name"] for r in rows}
    if len(need_names) != len(rows) or len(rows) > 96:
        raise ValueError("finite unique profile inventory")
    text = "\n".join("\t".join([r["name"], *map(str, r["values"])]) for r in rows) + "\n"
    (HERE / "profiles.tsv").write_text(text, encoding="ascii", newline="\n")
    finite = {"schema": "PRIVATE_BED_PLACEMENT_FINITE_V1", "columns": COLUMNS, "rows": rows,
              "callback_flags": {"1": "first foot cell notice prepares head equal to the later requested Bed ID/metadata",
                 "2": "first foot cell notice throws declared callback failure",
                 "4": "first head cell notice throws declared callback failure",
                 "8": "first foot cell notice writes actual Stack count100",
                 "16": "first foot cell notice changes captured actual Bed.bn to1"},
              "scope": "Actual direct LP calls; ordinary source booleans and declared mutable source operands/callbacks. No IZ wrapper, GUI, full world tick or native parity claim."}
    (HERE / "finite.json").write_text(json.dumps(finite, indent=2) + "\n", encoding="utf-8", newline="\n")
    spec = importlib.util.spec_from_file_location("bed_placement_classfile", ROOT / "tests/reference/client/controller/classfile.py")
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    names = ("lp", "gm", "iz", "fd", "lm", "wi", "mx", "dc", "gs", "ls", "sn", "uu", "ve", "in", "pm")
    with zipfile.ZipFile(ROOT / "b1.7.3.jar") as z:
        pins = {"client_jar_sha256": JAR, "classes": {name: reader.inspect_class(z.read(name + ".class")) for name in names}}
    (HERE / "source-pins.json").write_text(json.dumps(pins, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("Prepared " + str(len(rows)) + " private Item355 profiles and 15 complete original class hashes")


if __name__ == "__main__":
    main()
