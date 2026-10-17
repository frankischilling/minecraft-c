"""Bounded valid original surface requests; no C-derived numerical expectations."""
from dataclasses import dataclass


@dataclass(frozen=True)
class Fixture:
    label: str
    request: str
    coverage: tuple[str, ...] = ()
    equivalent_to: str | None = None
    equal_fields: tuple[str, ...] = ()
    preserve_fields: tuple[str, ...] = ()

    @property
    def operation(self) -> str:
        return self.request.split("\t", 1)[0]


def fixtures() -> list[Fixture]:
    result = []
    spans = ("sand", "gravel", "thickness", "blocks")
    observations = ("constructor_continuation", "continuation", "climate", "materials", "chunk_seed", "density")
    static = ("constructor_continuation", "climate", "materials", "chunk_seed", "density")
    base_preserve = ("constructor_continuation", "continuation", "chunk_seed", "sand", "gravel", "thickness")
    seed_preserve = ("constructor_continuation", "climate", "materials", "density", *spans)
    seeds = (0, 1, -1, -(1 << 63), (1 << 63) - 1, 17, -17, 123456789)
    for seed in seeds:
        prefix = f"seed{seed}"

        def add(name: str, request: str, *coverage: str, **controls: object) -> None:
            result.append(Fixture(prefix + "/" + name, request, tuple(coverage), **controls))

        add("init", f"init\t{seed}", "fresh-genuine-world-generator", "all82-noise-layers", "constructor-scratch256-zero")
        add("state-initial", "state", "initial-retention1110", equivalent_to=prefix + "/init",
            equal_fields=("state", "constructor_continuation", "continuation"), preserve_fields=(*observations, *spans))
        add("buffers-golden", "buffers\t259\t259\t259\t32771", "three-suffix-cells", preserve_fields=observations)
        add("base-origin", "base\t0\t0", "actual-normal-base-climate-materials", preserve_fields=base_preserve)
        add("surface-origin-direct", "surface\t0\t0", "constructor-random-context", "seed0-and1-paired-goldens",
            preserve_fields=static)
        add("surface-origin-repeat", "surface\t0\t0", "mutated-blocks-live-random-repeat", preserve_fields=static)
        add("base-origin-reset", "base\t0\t0", "restore-original-base-without-reseed", preserve_fields=base_preserve)
        add("seed-origin", "seed\t0\t0", "normal-prefix-random-context", "no-world-seed-xor", preserve_fields=seed_preserve)
        add("surface-origin-normal", "surface\t0\t0", "seed0-and1-paired-goldens", preserve_fields=static)
        add("state-before-apply", "state", "observer-preservation", equivalent_to=prefix + "/init",
            equal_fields=("state", "constructor_continuation"), preserve_fields=(*observations, *spans))
        add("apply-origin", "apply\t0\t0", "standalone-apply-adapter", "actual-recomputed-fields-exact",
            preserve_fields=(*static, "sand", "gravel", "thickness"))
        add("surface-origin-after-apply", "surface\t0\t0", "composed-after-apply", preserve_fields=static)
        add("base-origin-reset-repeat", "base\t0\t0", "restore-reseed-equivalence", preserve_fields=base_preserve)
        add("seed-origin-repeat", "seed\t0\t0", "same-seed-reset", preserve_fields=seed_preserve)
        add("surface-origin-normal-repeat", "surface\t0\t0", "same-base-and-seed-equivalence",
            equivalent_to=prefix + "/surface-origin-normal", preserve_fields=static)
        add("buffers-oversize", "buffers\t263\t269\t277\t32777", "distinct-valid-capacities", preserve_fields=observations)
        add("base-neighbor", "base\t1\t0", "positive-neighbor", preserve_fields=base_preserve)
        add("seed-neighbor", "seed\t1\t0", "positive-long-chunk-seed", preserve_fields=seed_preserve)
        add("surface-neighbor", "surface\t1\t0", "scratch-suffix-clear-block-suffix-retain", preserve_fields=static)
        add("apply-neighbor", "apply\t1\t0", "oversize-retained-fields", preserve_fields=(*static, "sand", "gravel", "thickness"))
        add("base-negative", "base\t-17\t31", "moved-mixed-sign-coordinate", preserve_fields=base_preserve)
        add("seed-negative", "seed\t-17\t31", "negative-long-chunk-seed", preserve_fields=seed_preserve)
        add("surface-negative", "surface\t-17\t31", "negative-surface-origin", preserve_fields=static)
        add("base-origin-products", "base\t134217728\t-134217729", "independently-wrapped-int-products4-and16", preserve_fields=base_preserve)
        add("seed-origin-products", "seed\t134217728\t-134217729", "wrapped-long-products-sum", preserve_fields=seed_preserve)
        add("surface-origin-products", "surface\t134217728\t-134217729", "wrapped-surface-int-origins", preserve_fields=static)
        add("base-extremes", "base\t-2147483648\t2147483647", "signed-int-coordinate-extremes", preserve_fields=base_preserve)
        add("seed-extremes", "seed\t-2147483648\t2147483647", "signed-long-overflow", preserve_fields=seed_preserve)
        add("surface-extremes", "surface\t-2147483648\t2147483647", "extreme-chunk-reseed-context", preserve_fields=static)
        add("base-wrapped", "base\t0\t-1", "same-int-origins-independent-random", equivalent_to=prefix + "/base-extremes",
            equal_fields=("climate", "materials", "density", "blocks"), preserve_fields=base_preserve)
        add("seed-wrapped", "seed\t0\t-1", "different-long-seed-from-wrapped-origins", preserve_fields=seed_preserve)
        add("surface-wrapped", "surface\t0\t-1", "same-noise-different-long-seed", equivalent_to=prefix + "/surface-extremes",
            equal_fields=("sand", "gravel", "thickness", "density", "climate", "materials"), preserve_fields=static)
        add("state-final", "state", "noise82-constructor-continuation-unchanged", equivalent_to=prefix + "/init",
            equal_fields=("state", "constructor_continuation"), preserve_fields=(*observations, *spans))

    def extra(name: str, request: str, *coverage: str, **controls: object) -> None:
        result.append(Fixture("captured/" + name, request, tuple(coverage), **controls))

    extra("init", "init\t0", "fresh-seed0-for-cold-constructor-context")
    extra("buffers", "buffers\t259\t259\t259\t32771", "paired-golden-capacities", preserve_fields=observations)
    extra("base-cold", "base\t-1099\t279", "observed-cold-ice-base", preserve_fields=base_preserve)
    extra("surface-cold-direct", "surface\t-1099\t279", "paired-cold-constructor-golden", preserve_fields=static)
    extra("apply-cold-direct", "apply\t-1099\t279", "retained-ice-and-materials", preserve_fields=(*static, "sand", "gravel", "thickness"))
    extra("base-cold-reset", "base\t-1099\t279", "restore-cold-base", preserve_fields=base_preserve)
    extra("seed-cold", "seed\t-1099\t279", "observed-negative-chunk-seed", preserve_fields=seed_preserve)
    extra("surface-cold-normal", "surface\t-1099\t279", "paired-cold-normal-golden", preserve_fields=static)
    extra("seed-cold-again", "seed\t-1099\t279", "seed-preserves-matching-apply-fields", preserve_fields=seed_preserve)
    extra("apply-cold-reseeded", "apply\t-1099\t279", "apply-after-seed", preserve_fields=(*static, "sand", "gravel", "thickness"))
    extra("base-gravel", "base\t-8\t1", "genuine-observed-gravel-case", preserve_fields=base_preserve)
    extra("seed-gravel", "seed\t-8\t1", "observed-gravel-normal-seed", preserve_fields=seed_preserve)
    extra("surface-gravel", "surface\t-8\t1", "paired-gravel-golden", "306-gravel197-sandstone", "102-guaranteed-both-flags", preserve_fields=static)
    extra("apply-gravel", "apply\t-8\t1", "repeated-gravel-pass", preserve_fields=(*static, "sand", "gravel", "thickness"))
    extra("state-final", "state", "noise82-unchanged", equivalent_to="captured/init", equal_fields=("state", "constructor_continuation"),
          preserve_fields=(*observations, *spans))
    result += [
        Fixture("capacity/init", "init\t0", ("fresh-generator-capacity-controls",)),
        Fixture("capacity/buffers-min", "buffers\t256\t256\t256\t32768", ("minimum-valid-capacities",), preserve_fields=observations),
        Fixture("capacity/base-min", "base\t-1\t0", ("minimum-no-suffix",), preserve_fields=base_preserve),
        Fixture("capacity/seed-min", "seed\t-1\t0", ("negative-neighbor",), preserve_fields=seed_preserve),
        Fixture("capacity/surface-min", "surface\t-1\t0", ("minimum-valid-capacities",), preserve_fields=static),
        Fixture("capacity/apply-min", "apply\t-1\t0", ("minimum-retained-apply",), preserve_fields=(*static, "sand", "gravel", "thickness")),
        Fixture("capacity/buffers-max", "buffers\t512\t512\t512\t32832", ("maximum-bounded-capacities",), preserve_fields=observations),
        Fixture("capacity/base-max", "base\t-1099\t279", ("maximum-cold-base",), preserve_fields=base_preserve),
        Fixture("capacity/seed-max", "seed\t-1099\t279", ("maximum-cold-normal-context",), preserve_fields=seed_preserve),
        Fixture("capacity/surface-max", "surface\t-1099\t279", ("full-scratch-suffix-clear", "block-suffix-retention"), preserve_fields=static),
        Fixture("capacity/state-final", "state", ("maximum-state-observation",), equivalent_to="capacity/init",
                equal_fields=("state", "constructor_continuation"), preserve_fields=(*observations, *spans)),
    ]
    labels = {case.label for case in result}
    if len(labels) != len(result) or len(result) > 1024:
        raise ValueError("fixture labels/count contract")
    if any(case.equivalent_to is not None and case.equivalent_to not in labels for case in result):
        raise ValueError("missing fixture equivalence target")
    return result
