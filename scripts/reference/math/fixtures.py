"""Bounded lookup/floor inputs; no numerical expected outputs are generated."""

from dataclasses import dataclass
import struct


@dataclass(frozen=True)
class Fixture:
    operation: str
    input_bits: str
    coverage: tuple[str, ...] = ()

    @property
    def descriptor(self) -> str:
        return "(D)I" if self.operation == "floor" else "(F)F"

    @property
    def label(self) -> str:
        return self.operation + "/" + self.input_bits

    @property
    def request(self) -> str:
        return self.operation + "\t" + self.input_bits


def fixtures() -> list[Fixture]:
    float_cases, double_cases = {}, {}

    def add(target, bits, reason):
        target.setdefault(bits, []).append(reason)

    def float_bits(number):
        return int.from_bytes(struct.pack(">f", number), "big")

    def double_bits(number):
        return int.from_bytes(struct.pack(">d", number), "big")

    for value in (0, 0x80000000, 1, 0x80000001, 0x007fffff, 0x807fffff, 0x00800000, 0x80800000,
                  0x3f000000, 0xbf000000, 0x3f800000, 0xbf800000, 0x40490fdb, 0xc0490fdb,
                  0x3fc90fdb, 0xbfc90fdb, 0x40c90fdb, 0xc0c90fdb, 0x7f7fffff, 0xff7fffff,
                  0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7fc00001, 0x7fa00001, 0xff800001):
        add(float_cases, value, "signed zero/subnormal/normal/angle/extreme/nonfinite")
    scale = struct.unpack(">f", bytes.fromhex("4622f983"))[0]
    for index in (-65537, -65536, -65535, -32768, -16384, -2, -1, 0, 1, 2, 16383, 16384, 32767, 32768, 65535, 65536, 65537):
        raw = float_bits(index / scale)
        for delta in (-2, -1, 0, 1, 2):
            if 0 <= raw + delta <= 0xffffffff:
                add(float_cases, raw + delta, "adjacent floats near scaled index " + str(index))
    for value in (-(1 << 31) / scale, ((1 << 31) - 1) / scale, (1 << 31) / scale, -1e10, 1e10, -1e30, 1e30):
        raw = float_bits(value)
        for delta in (-2, -1, 0, 1, 2):
            add(float_cases, raw + delta, "float-to-int saturation or large finite lookup")
    for value in (0, 0x8000000000000000, 1, 0x8000000000000001, 0x000fffffffffffff, 0x800fffffffffffff,
                  0x0010000000000000, 0x8010000000000000, 0x7fefffffffffffff, 0xffefffffffffffff,
                  0x7ff0000000000000, 0xfff0000000000000, 0x7ff8000000000000, 0xfff8000000000000,
                  0x7ff8000000000001, 0x7ff0000000000001, 0xfff0000000000001):
        add(double_cases, value, "signed zero/subnormal/normal/extreme/nonfinite")
    for value in (-2147483649., -2147483648., -2147483647., -1024., -2., -1., -.5, .5, 1., 2., 1024.,
                  2147483646., 2147483647., 2147483648., 2147483649., -(1 << 53), 1 << 53):
        raw = double_bits(value)
        for delta in (-2, -1, 0, 1, 2):
            add(double_cases, raw + delta, "adjacent doubles near integer/narrowing boundary " + str(value))
    result = []
    for raw, reasons in float_cases.items():
        for operation in ("sin", "cos"):
            result.append({"operation": operation, "descriptor": "(F)F", "input_bits": f"{raw:08x}", "coverage": sorted(set(reasons))})
    for raw, reasons in double_cases.items():
        result.append({"operation": "floor", "descriptor": "(D)I", "input_bits": f"{raw:016x}", "coverage": sorted(set(reasons))})
    if len(result) > 768:
        raise RuntimeError("bounded input contract")
    return [Fixture(item["operation"], item["input_bits"], tuple(item["coverage"])) for item in result]
