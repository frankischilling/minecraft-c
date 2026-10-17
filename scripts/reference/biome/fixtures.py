"""Valid classifier inputs and explicit-seed numerical manager call sequences."""

from dataclasses import dataclass
import math
import struct


def fbits(number: float) -> str:
    return struct.pack(">f", number).hex()


def dbits(number: float) -> str:
    return struct.pack(">d", number).hex()


def f32(number: float) -> float:
    return struct.unpack(">f", struct.pack(">f", number))[0]


def float_neighbors(number: float) -> list[float]:
    raw = int(fbits(number), 16)
    return [struct.unpack(">f", candidate.to_bytes(4, "big"))[0]
            for candidate in (raw - 1, raw, raw + 1)
            if 0 <= candidate <= 0x3f800000]


@dataclass(frozen=True)
class Fixture:
    label: str
    request: str
    coverage: tuple[str, ...] = ()
    equivalent_to: str | None = None
    equal_fields: tuple[str, ...] = ()
    different_from: str | None = None
    preserve_fields: tuple[str, ...] = ()
    temperature_from: str | None = None

    @property
    def operation(self) -> str:
        return self.request.split("\t", 1)[0]


def fixtures() -> list[Fixture]:
    result = [Fixture("catalog", "catalog", ("actual-13-identities-and-names",)),
              Fixture("table", "table", ("all4096-actual-table-entries", "temperature-fast-table-order"))]
    # These valid pairs were observed to differ between the actual float tree
    # and the actual quantized double table lookup. Neither is an oracle for the other.
    result += [Fixture("selector-distinction/float", "float\t" + fbits(0.5) + "\t" + fbits(0.2),
                       ("direct-float-selector", "float-vs-quantized-double")),
               Fixture("selector-distinction/double", "double\t" + dbits(0.5) + "\t" + dbits(0.2),
                       ("quantized-double-selector", "float-vs-quantized-double"),
                       different_from="selector-distinction/float")]
    pairs = {(fbits(t), fbits(r)) for t in (0.0, -0.0, 1.0) for r in (0.0, -0.0, 1.0)}
    for threshold in (0.1, 0.5, 0.7, 0.95, 0.97):
        for t in float_neighbors(f32(threshold)):
            for rain in (0.0, 0.1, 0.2, 0.35, 0.45, 0.5, 0.9, 1.0):
                pairs.add((fbits(t), fbits(rain)))
    # Exercise both sides of product thresholds with valid unit-interval inputs.
    # Only input values are generated; the actual original method decides identity.
    for threshold in (0.2, 0.35, 0.45, 0.5, 0.9):
        for t in (0.4, 0.5, 0.6, 0.7, 0.95, 0.97, 1.0):
            rain = f32(f32(threshold) / f32(t))
            if 0 <= rain <= 1:
                for r in float_neighbors(rain):
                    pairs.add((fbits(t), fbits(r)))
    for index, (t, r) in enumerate(sorted(pairs)):
        result.append(Fixture(f"float-threshold/{index}", f"float\t{t}\t{r}",
                              ("direct-float-selector", "threshold-plus-minus-one-ulp", "binary32-product")))

    double_pairs = {(dbits(t), dbits(r)) for t in (0.0, -0.0, 1.0)
                    for r in (0.0, -0.0, 0.2, 0.5, 0.9, 1.0)}
    for cell in range(64):
        boundary = cell / 63.0
        neighbors = (math.nextafter(boundary, -math.inf), boundary, math.nextafter(boundary, math.inf))
        for coordinate in neighbors:
            if 0 <= coordinate <= 1:
                for other in (0.0, 0.2, 0.5, 0.9, 1.0):
                    double_pairs.add((dbits(coordinate), dbits(other)))
                    double_pairs.add((dbits(other), dbits(coordinate)))
    for index, (t, r) in enumerate(sorted(double_pairs)):
        result.append(Fixture(f"double-quantization/{index}", f"double\t{t}\t{r}",
                              ("quantized-double-selector", "valid-table-cell-boundary", "adjacent-binary64")))

    seeds = (0, 1, -1, 17, -17, -(1 << 63), (1 << 63) - 1, 123456789, -123456789)
    origins = ((0, 0), (-17, 31), (-30000000, 33554432), (-(1 << 31), (1 << 31) - 1))
    for seed in seeds:
        for origin_index, (x, z) in enumerate(origins):
            prefix = f"manager/seed{seed}/origin{origin_index}"

            def add(name: str, request: str, *coverage: str, **controls: object) -> None:
                result.append(Fixture(prefix + "/" + name, request, tuple(coverage), **controls))

            add("init", f"init\t{seed}", "explicit-seed-fixture", "wrapped-channel-seeds")
            add("buffers-null", "buffers\t-1\t-1\t-1\t-1\t-1\t-1", "null-arrays")
            add("temperature-rectangle", f"temperature\t{x}\t{z}\t2\t3", "valid-temperature-rectangle",
                preserve_fields=("a", "b", "d", "o"))
            add("direct2", f"biomes\t{x}\t{z}\t2", "square2", "direct-does-not-assign-d",
                preserve_fields=("d", "t"))
            add("cached4", f"cached\t{x}\t{z}\t4", "square4", "cached-assigns-d", preserve_fields=("t", "o"))
            add("temperature-square2", f"temperature\t{x}\t{z}\t2\t2", "temperature-preserves-rainfall-and-biomes",
                preserve_fields=("a", "b", "d", "o"))
            add("direct-temperature-agreement", f"biomes\t{x}\t{z}\t2", "temperature-path-agreement",
                temperature_from=prefix + "/temperature-square2", preserve_fields=("d", "t"))
            add("point", f"point\t{x}\t{z}", "point1x1", preserve_fields=("t", "o"))
            add("cached4-again", f"cached\t{x}\t{z}\t4", "large-small-large", equivalent_to=prefix + "/cached4",
                equal_fields=("a", "b", "c", "d"), preserve_fields=("t", "o"))
            add("chunk", f"chunk\t{x}\t{z}", "wrapping-chunk-shift", preserve_fields=("t", "o"))
            shifted_x = ((x << 4) + (1 << 31)) % (1 << 32) - (1 << 31)
            shifted_z = ((z << 4) + (1 << 31)) % (1 << 32) - (1 << 31)
            add("wrapped-point", f"point\t{shifted_x}\t{shifted_z}", "chunk-point-agreement",
                equivalent_to=prefix + "/chunk", preserve_fields=("t", "o"))
            add("buffers-oversize", "buffers\t271\t273\t277\t281\t283\t293", "oversize-prefilled-arrays")
            add("direct16", f"biomes\t{x}\t{z}\t16", "square16", "retained-suffixes", preserve_fields=("d", "t"))
            add("cached1-oversize", f"cached\t{x}\t{z}\t1", "square1", "retained-suffixes", preserve_fields=("t", "o"))
            add("temperature-empty", f"temperature\t{x}\t{z}\t0\t3", "valid-zero-size-temperature",
                preserve_fields=("a", "b", "d", "o"))
            add("cached-empty", f"cached\t{x}\t{z}\t0", "valid-zero-size-square", preserve_fields=("t", "o"))
            add("direct-empty", f"biomes\t{x}\t{z}\t0", "valid-zero-size-square", preserve_fields=("d", "t"))
            add("buffers-empty", "buffers\t0\t0\t0\t0\t0\t0", "allocated-empty-distinct-from-null")
            add("temperature-empty-retain", f"temperature\t{x}\t{z}\t3\t0", "valid-zero-size-temperature",
                preserve_fields=("a", "b", "d", "o"))
            add("direct-empty-retain", f"biomes\t{x}\t{z}\t0", "valid-zero-size-square", preserve_fields=("d", "t"))
            add("point-after-empty", f"point\t{x}\t{z}", "empty-to-nonempty-growth", preserve_fields=("t", "o"))

    # Large valid numerical rectangle, not a world/chunk operation.
    result += [Fixture("large-rectangle/init", "init\t17"),
               Fixture("large-rectangle/temperature", "temperature\t-17\t31\t64\t64", ("4096-temperature-values",)),
               Fixture("large-rectangle/reuse", "temperature\t-17\t31\t1\t1", ("large-retained-zero-suffix",))]
    labels = {case.label for case in result}
    if len(result) > 8192 or len(labels) != len(result):
        raise ValueError("fixture count/labels exceed contract")
    if any(target is not None and target not in labels for case in result
           for target in (case.equivalent_to, case.different_from, case.temperature_from)):
        raise ValueError("control label missing")
    return result


def client_fixtures() -> list[Fixture]:
    result = []
    for seed in (0, -1, -(1 << 63), (1 << 63) - 1):
        prefix = f"client-raw/seed{seed}"
        result += [Fixture(prefix + "/init", f"init\t{seed}"),
                   Fixture(prefix + "/cached", "cached\t-17\t31\t4"),
                   Fixture(prefix + "/buffers", "buffers\t23\t29\t31\t37\t41\t43"),
                   Fixture(prefix + "/raw", "raw\t0\t0", ("actual-client-only-raw",),
                           preserve_fields=("b", "c", "d", "t", "o")),
                   Fixture(prefix + "/raw-again", "raw\t0\t0", ("repeat-client-only-raw",),
                           equivalent_to=prefix + "/raw", preserve_fields=("b", "c", "d", "t", "o")),
                   Fixture(prefix + "/point", "point\t0\t0"),
                   Fixture(prefix + "/raw-moved", "raw\t-17\t31", ("actual-client-only-raw",),
                           preserve_fields=("b", "c", "d", "t", "o")),
                   Fixture(prefix + "/temperature", "temperature\t-17\t31\t2\t3", ("processed-temperature-after-raw",),
                           preserve_fields=("a", "b", "d", "o"))]
    return result
