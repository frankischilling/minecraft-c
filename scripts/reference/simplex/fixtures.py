"""Bounded inputs for original 2D simplex grids; contains no sampling algorithm."""

from dataclasses import dataclass
import math
import struct


def bits(value: float) -> str:
    return struct.pack(">d", value).hex()


def value(raw: str) -> float:
    return struct.unpack(">d", bytes.fromhex(raw))[0]


@dataclass(frozen=True)
class Fixture:
    label: str
    operation: str
    seed: int
    prefix: str
    count: int
    arguments: tuple[str, ...] = ()
    coverage: tuple[str, ...] = ()
    equivalent_to: str | None = None
    different_from: str | None = None

    def request(self) -> str:
        return "\t".join((self.operation, str(self.seed), self.prefix, str(self.count), *self.arguments))

    def output_count(self) -> int:
        if self.operation == "constants":
            return 2
        if self.operation == "state":
            return self.count * 3
        return int(self.arguments[2]) * int(self.arguments[3]) + 3

    def permutation_count(self) -> int:
        return self.count * 512 if self.operation == "state" else 0


def grid_arguments(origin: tuple[float, float], dimensions: tuple[int, int], scales: tuple[float, float],
                   factor: float, divisor_multiplier: float | None = None) -> tuple[str, ...]:
    result = tuple(bits(number) for number in origin) + tuple(str(number) for number in dimensions)
    result += tuple(bits(number) for number in scales) + (bits(factor),)
    return result if divisor_multiplier is None else result + (bits(divisor_multiplier),)


def golden_fixtures() -> list[Fixture]:
    # Public regression inputs captured from both untouched originals before C
    # results existed. Full request/output records also remain in local captures.
    x = value("406764168ea6ca89")
    z = value("404ec9e5b3672e14")
    result = [Fixture("golden/constants", "constants", 0, "-", 0),
              Fixture("golden/state", "state", 0, "-", 1),
              Fixture("golden/state-PS", "state", 0, "PS", 1),
              Fixture("golden/state-SP", "state", 0, "SP", 1, equivalent_to="golden/state-PS"),
              Fixture("golden/state-PSSP", "state", 0, "PSSP", 2)]
    specifications = (
        ("nonsquare", (0.0, 0.0), (3, 2), (0.125, 0.25)),
        ("zero-offset-cancel", (-x, -z), (1, 1), (1.0, 1.0)),
        ("negative-integer-cell", (-1.0 - x, 1.0 - z), (1, 1), (1.0, 1.0)),
        ("triangle-tie", (0.25 - x, 0.25 - z), (1, 1), (1.0, 1.0)),
        ("attenuation-boundary", (0.5 - x, 0.5 - z), (1, 1), (1.0, 1.0)),
        ("wrapped-cell-sum", (1e9, 1e9), (1, 1), (1.0, 1.0)),
    )
    for name, origin, dimensions, scales in specifications:
        result.append(Fixture("golden/add-" + name, "add", 0, "-", 1,
                              grid_arguments(origin, dimensions, scales, 1.0)))
    args = ((-17.25, 31.875), (3, 2), (0.125, 0.25))
    result += [Fixture("golden/fill-nonsquare", "fill", 0, "-", 4, grid_arguments(*args, 0.25, 0.5)),
               Fixture("golden/default-nonsquare", "default", 0, "-", 4, grid_arguments(*args, 0.25),
                       equivalent_to="golden/fill-nonsquare"),
               Fixture("golden/fill-independent-factors", "fill", 0, "-", 4, grid_arguments(*args, 0.75, 2.0),
                       different_from="golden/fill-nonsquare"),
               Fixture("control/fill-frequency-only", "fill", 0, "-", 4, grid_arguments(*args, 0.75, 0.5),
                       different_from="golden/fill-nonsquare"),
               Fixture("control/fill-amplitude-only", "fill", 0, "-", 4, grid_arguments(*args, 0.25, 2.0),
                       different_from="golden/fill-nonsquare")]
    return result


def fixtures() -> list[Fixture]:
    result = golden_fixtures()
    seeds = (0, 1, -1, 17, -17, -(1 << 63), (1 << 63) - 1, 123456789, -123456789)
    counts = (0, 1, 2, 4, 8, 16)
    for seed in seeds:
        for count in counts:
            result.append(Fixture(f"state/seed{seed}/layers{count}", "state", seed, "-", count,
                                  coverage=("all512-permutations", "three-offsets", "random-continuation")))
    prefixes = ("P", "S", "PS", "SP", "PSSP", "SSPPPS", "PS" * 8)
    for seed in (0, -17, -(1 << 63), (1 << 63) - 1):
        for prefix in prefixes:
            for count in (0, 1, 4):
                result.append(Fixture(f"state/seed{seed}/prefix{prefix}/layers{count}", "state", seed, prefix, count,
                                      coverage=("mixed-constructor-prefix", "all512-permutations", "random-continuation")))
    for seed in (0, -1):
        result.append(Fixture(f"state/seed{seed}/maximum-bounds", "state", seed, "PS" * 64, 128,
                              coverage=("probe-count-boundary", "mixed-constructor-prefix", "all512-permutations")))

    promoted_025 = value("3f999999a0000000")  # (double)0.025f
    promoted_050 = value("3fa99999a0000000")  # (double)0.05f
    grids = (
        ("single", (-0.25, 0.75), (1, 1), (1.0, 1.0), 1.0),
        ("nonsquare", (-17.25, 31.875), (3, 5), (0.125, 0.25), 0.25),
        ("negative-scale", (-32.5, -0.25), (5, 2), (-0.5, -0.125), -2.0),
        ("signed-zero-scale", (-17.25, 3.5), (2, 3), (-0.0, 0.0), -0.0),
        ("zero-amplitude", (-17.25, 3.5), (2, 3), (0.5, 0.125), 0.0),
        ("large-terrain", (-30000000.25, 33554432.75), (3, 2), (1.0, 1.0), 2.5),
        ("wrapped-cell-sum-positive", (1000000000.125, 1000000000.875), (2, 3), (1.0, 1.0), 1.0),
        ("wrapped-cell-sum-negative", (-1000000000.125, -1000000000.875), (2, 3), (1.0, 1.0), 1.0),
        ("cast-extremes", (2147483646.5, -2147483649.5), (3, 2), (1.0, 1.0), 1.0),
        ("large-scale", (-123.5, 17.25), (2, 3), (2.0 ** 60, -(2.0 ** 60)), 0.125),
        ("promoted-float-scale", (-17.25, 31.875), (5, 3), (promoted_025, promoted_050), 1.0),
        ("empty-x", (1.0, 2.0), (0, 3), (0.5, 0.25), 1.0),
        ("empty-z", (1.0, 2.0), (3, 0), (0.5, 0.25), 1.0),
    )
    for seed in (0, -17, (1 << 63) - 1, -123456789):
        for name, origin, dimensions, scales, amplitude in grids:
            result.append(Fixture(f"add/seed{seed}/{name}", "add", seed, "-", 1,
                                  grid_arguments(origin, dimensions, scales, amplitude),
                                  ("additive-output", "unchanged-suffix", name)))
            for count in (0, 1, 2, 4, 8):
                explicit_label = f"fill/seed{seed}/layers{count}/{name}"
                result.append(Fixture(explicit_label, "fill", seed, "-", count,
                                      grid_arguments(origin, dimensions, scales, 0.25, 0.5),
                                      ("octave-fill", "cleared-suffix", "frequency-and-amplitude", name)))
                if count in (0, 1, 4):
                    result.append(Fixture(f"default/seed{seed}/layers{count}/{name}", "default", seed, "-", count,
                                          grid_arguments(origin, dimensions, scales, 0.25),
                                          ("actual-short-overload", "cleared-suffix", name),
                                          equivalent_to=explicit_label))

    # Offset cancellation exercises the sampler's actual integer and corner
    # choices. Only input coordinates are generated; original code is the oracle.
    offset_x = value("406764168ea6ca89")
    offset_z = value("404ec9e5b3672e14")
    targets = ((0.0, 0.0), (-1.0, 1.0), (1.0, -1.0), (-256.0, 256.0),
               (0.25, 0.25), (0.5, 0.5), (0.625, 0.0), (0.0, 0.625))
    for target_index, (x, z) in enumerate(targets):
        origin = (x - offset_x, z - offset_z)
        for variation, modified in (
            ("exact", origin),
            ("x-before", (math.nextafter(origin[0], -math.inf), origin[1])),
            ("x-after", (math.nextafter(origin[0], math.inf), origin[1])),
            ("z-before", (origin[0], math.nextafter(origin[1], -math.inf))),
            ("z-after", (origin[0], math.nextafter(origin[1], math.inf))),
        ):
            result.append(Fixture(f"add/cell-boundary{target_index}/{variation}", "add", 0, "-", 1,
                                  grid_arguments(modified, (1, 1), (1.0, 1.0), 1.0),
                                  ("offset-cancellation", "floor-cell-boundary", "triangle-and-attenuation", "adjacent-double")))

    for seed in (0, -17, (1 << 63) - 1):
        for prefix in ("P", "S", "PS", "SP", "PSSP", "SSPPPS"):
            result.append(Fixture(f"add/seed{seed}/prefix{prefix}", "add", seed, prefix, 1,
                                  grid_arguments((-17.25, 31.875), (3, 2), (0.125, -0.25), -0.5),
                                  ("mixed-constructor-prefix", "additive-output")))
            for count in (2, 4):
                result.append(Fixture(f"fill/seed{seed}/prefix{prefix}/layers{count}", "fill", seed, prefix, count,
                                      grid_arguments((-17.25, 31.875), (3, 2), (0.125, 0.25), -0.5, -2.0),
                                      ("mixed-constructor-prefix", "independent-controls", "signed-factors")))

    for seed in (0, -17, -123456789):
        for count in (1, 2, 4):
            for index, (frequency, divisor_multiplier) in enumerate(
                    ((0.0, 0.5), (-0.0, 0.5), (-0.5, -0.5), (0.75, 2.0), (1.5, 1.0), (0.25, -2.0))):
                result.append(Fixture(f"fill/seed{seed}/layers{count}/controls{index}", "fill", seed, "-", count,
                                      grid_arguments((-17.25, 31.875), (3, 2), (0.125, 0.25), frequency, divisor_multiplier),
                                      ("independent-controls", "signed-zero-frequency", "frequency-and-amplitude")))

    # Immediate consumers construct independently seeded channels. These cases
    # verify only their numerical inputs, not climate blending or biome results.
    channels = ((9871, 4, promoted_025, 0.25), (39811, 4, promoted_050, 1.0 / 3.0),
                (543321, 2, 0.25, 0.5882352941176471))
    for world_seed in (0, 17, -17, -(1 << 63), (1 << 63) - 1, 123456789):
        for multiplier, count, scale, frequency in channels:
            seed = ((world_seed * multiplier + (1 << 63)) % (1 << 64)) - (1 << 63)
            explicit_label = f"consumer-seed{world_seed}/multiplier{multiplier}/explicit"
            result.append(Fixture(explicit_label, "fill", seed, "-", count,
                                  grid_arguments((-17.0, 31.0), (3, 5), (scale, scale), frequency, 0.5),
                                  ("consumer-input-only", "seed-multiplication-wrap", "promoted-float-scale")))
            result.append(Fixture(f"consumer-seed{world_seed}/multiplier{multiplier}/default", "default", seed, "-", count,
                                  grid_arguments((-17.0, 31.0), (3, 5), (scale, scale), frequency),
                                  ("consumer-input-only", "actual-short-overload"), equivalent_to=explicit_label))
    for op in ("add", "fill", "default"):
        result.append(Fixture(op + "/grid-product-boundary", op, 0, "-", 1 if op == "add" else 2,
                              grid_arguments((-1.5, 3.0), (256, 256), (0.125, 0.0625), 0.25,
                                             0.5 if op == "fill" else None),
                              ("grid-product65536", "output-suffix"),
                              equivalent_to="fill/grid-product-boundary" if op == "default" else None))
    result.append(Fixture("fill/layers16", "fill", 0, "SPPS", 16,
                          grid_arguments((-17.25, 31.875), (5, 3), (promoted_025, promoted_050), 0.25, 0.5),
                          ("bounded-large-octaves", "mixed-constructor-prefix")))
    labels = {case.label for case in result}
    if len(result) > 1024 or len(labels) != len(result):
        raise ValueError("fixture count/label invariant failed")
    if any(target is not None and target not in labels for case in result
           for target in (case.equivalent_to, case.different_from)):
        raise ValueError("fixture control references missing label")
    return result
