"""Bounded deterministic inputs for the original improved-Perlin/octave methods."""

from dataclasses import dataclass
import math
import struct


def bits(value: float) -> str:
    return struct.pack(">d", value).hex()


@dataclass(frozen=True)
class Fixture:
    label: str
    operation: str
    seed: int
    skip: int
    count: int
    arguments: tuple[str, ...] = ()
    coverage: tuple[str, ...] = ()

    def request(self) -> str:
        return "\t".join((self.operation, str(self.seed), str(self.skip), str(self.count), *self.arguments))

    def output_count(self) -> int:
        if self.operation == "state":
            return self.count * 3
        if self.operation in {"point", "sample2"}:
            return 1
        return math.prod(int(value) for value in self.arguments[3:6]) + 3

    def permutation_count(self) -> int:
        return self.count * 512 if self.operation == "state" else 0


def grid_arguments(origin: tuple[float, float, float], dimensions: tuple[int, int, int],
                   scales: tuple[float, float, float], divisor: float | None = None) -> tuple[str, ...]:
    result = tuple(bits(value) for value in origin) + tuple(str(value) for value in dimensions)
    result += tuple(bits(value) for value in scales)
    return result if divisor is None else result + (bits(divisor),)


def fixtures() -> list[Fixture]:
    result: list[Fixture] = []
    seeds = (0, 1, -1, 17, -17, -(1 << 63), (1 << 63) - 1, 123456789, -123456789)
    counts = (0, 1, 2, 4, 8, 16)
    for seed in seeds:
        for count in counts:
            result.append(Fixture(f"state/seed{seed}/layers{count}", "state", seed, 0, count,
                                  coverage=("all512-permutations", "offsets", "random-continuation")))

    # Original terrain constructor sequence, preserving the shared Random stream.
    # Each skip is a number of Perlin constructors, not a number of random draws.
    sequence = (16, 16, 8, 4, 4, 10, 16, 8)
    for seed in (0, -17, 123456789):
        skip = 0
        for index, count in enumerate(sequence):
            result.append(Fixture(f"terrain-sequence/seed{seed}/generator{index}", "state", seed, skip, count,
                                  coverage=("shared-random-construction", "all512-permutations")))
            result.append(Fixture(f"terrain-sequence/seed{seed}/sample{index}", "sample2", seed, skip, count,
                                  (bits(-17.25), bits(31.875)), ("shared-random-construction", "octave-order")))
            skip += count
    for seed in (0, -1):
        result.append(Fixture(f"state/seed{seed}/maximum-bounds", "state", seed, 128, 128,
                              coverage=("probe-count-boundary", "all512-permutations")))

    points = (
        (0.0, 0.0, 0.0), (-0.0, 0.0, -0.0), (0.25, -0.5, 3.0),
        (-1.0, -1.0, -1.0), (1.0, 1.0, 1.0),
        (-16.0000000001, 255.9999999999, 16.5),
        (-30000000.25, 64.5, 30000000.75),
        (33554432.25, -0.125, -33554432.5),
        (1000000000.125, -256.5, -1000000000.875),
        (2147483646.5, -2147483648.5, -2147483649.5),
    )
    for seed in seeds:
        for index, point in enumerate(points):
            result.append(Fixture(f"point/seed{seed}/case{index}", "point", seed, 0, 1,
                                  tuple(bits(value) for value in point),
                                  ("scalar3d", "negative-large-coordinates", "no-later-coordinate-wrap")))

    # Offsets observed from the pinned original constructor for seed zero.
    # Cancel them to place coordinates exactly on a lattice boundary, then
    # inspect adjacent representable inputs. No Python noise/RNG implementation.
    offsets = tuple(struct.unpack(">d", bytes.fromhex(value))[0] for value in (
        "406764168ea6ca89", "404ec9e5b3672e14", "406465b93a78ef81"))
    for lattice in (-256.0, -1.0, 0.0, 1.0, 256.0):
        for direction in (-math.inf, None, math.inf):
            coordinates = tuple(lattice - offset for offset in offsets)
            if direction is not None:
                coordinates = tuple(math.nextafter(value, direction) for value in coordinates)
            label = "exact" if direction is None else "before" if direction < 0 else "after"
            result.append(Fixture(f"point/lattice{lattice}/{label}", "point", 0, 0, 1,
                                  tuple(bits(value) for value in coordinates), ("floor-boundary", "adjacent-double")))

    for seed in (0, -1, 17, -(1 << 63), (1 << 63) - 1, -123456789):
        for count in counts:
            for index, (x, y) in enumerate(((0.0, -0.0), (0.25, -0.5), (-16.125, 31.875),
                                             (33554432.25, -1000000000.5))):
                result.append(Fixture(f"sample2/seed{seed}/layers{count}/case{index}", "sample2", seed, 0,
                                      count, (bits(x), bits(y)), ("octave-scalar2d", "octave-order")))

    grids = (
        ("odd", (-17.25, 2.5, 31.75), (3, 5, 2), (0.5, 0.25, 0.75), 1.0),
        ("height-one", (0.0, 10.0, 0.0), (2, 1, 2), (0.5, 1.0, 0.5), 1.0),
        ("height-one-ignore-y", (0.0, -1000000.25, 0.0), (2, 1, 2), (0.5, -0.0, 0.5), 1.0),
        ("same-y-cell", (0.0, 0.0, 0.0), (1, 4, 1), (1.0, 0.0625, 1.0), 1.0),
        ("cross-y-cell", (-0.5, -2.0, 0.25), (2, 5, 3), (0.125, 0.75, 0.25), 2.0),
        ("y256-wrap", (-0.5, 0.0, 0.25), (2, 3, 3), (0.125, 256.0, 0.25), 0.25),
        ("negative-scale", (-0.25, -8.5, 16.5), (3, 3, 2), (-0.5, -0.25, -0.125), -2.0),
        ("zero-scale", (17.25, -8.5, -16.5), (3, 3, 2), (0.0, 0.0, 0.0), 1.0),
        ("signed-zero-scale", (-17.25, 8.5, 16.5), (3, 2, 2), (-0.0, -0.0, -0.0), -0.25),
        ("large-terrain", (-30000000.25, -256.5, 33554432.75), (3, 3, 2), (1.0, 0.5, 1.0), 4.0),
        ("large-unwrapped", (1000000000.125, 0.25, -1000000000.875), (3, 2, 3), (1.0, 1.0, 1.0), 1.0),
        ("narrowing-limit", (2147483646.5, -2147483648.5, -2147483649.5), (2, 2, 2), (1.0, 1.0, 1.0), 1.0),
        ("empty-x", (1.0, 2.0, 3.0), (0, 3, 2), (0.5, 1.0, 0.5), 1.0),
        ("empty-y", (1.0, 2.0, 3.0), (2, 0, 3), (0.5, 1.0, 0.5), 1.0),
        ("empty-z", (1.0, 2.0, 3.0), (2, 3, 0), (0.5, 1.0, 0.5), 1.0),
    )
    for seed in (0, -17, (1 << 63) - 1, -123456789):
        for name, origin, dimensions, scales, divisor in grids:
            result.append(Fixture(f"add/seed{seed}/{name}", "add", seed, 0, 1,
                                  grid_arguments(origin, dimensions, scales, divisor),
                                  ("perlin-add", name, "prefilled-output", "unchanged-suffix")))
            for count in counts:
                result.append(Fixture(f"fill/seed{seed}/layers{count}/{name}", "fill", seed, 0, count,
                                      grid_arguments(origin, dimensions, scales),
                                      ("octave-fill", name, "cleared-suffix", "octave-order")))
    result.append(Fixture("add/grid-product-boundary", "add", 0, 0, 1,
                          grid_arguments((-1.5, 3.0, 0.25), (16, 16, 256), (0.25, 0.125, 0.0625), 2.0),
                          ("grid-product65536", "unchanged-suffix")))
    result.append(Fixture("fill/grid-product-boundary", "fill", 0, 0, 2,
                          grid_arguments((-1.5, 3.0, 0.25), (16, 16, 256), (0.25, 0.125, 0.0625)),
                          ("grid-product65536", "cleared-suffix")))
    for index, count in enumerate(sequence):
        skip = sum(sequence[:index])
        result.append(Fixture(f"fill/terrain-shared-stream/generator{index}", "fill", 0, skip, count,
                              grid_arguments((-17.25, 3.0, 31.875), (3, 3, 2), (0.5, 0.125, 0.5)),
                              ("shared-random-construction", "octave-order", "cleared-suffix")))
    if len(result) > 1024 or len({case.label for case in result}) != len(result):
        raise ValueError("fixture count/label invariant failed")
    return result
