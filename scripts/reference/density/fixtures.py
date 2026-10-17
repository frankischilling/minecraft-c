"""Bounded normal-shape terrain requests; all numerical oracles are untouched originals."""

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
    seeds = (0, 1, -1, -(1 << 63), (1 << 63) - 1, 17, -17, 123456789)
    mutable = ("d", "e", "f", "g", "h", "density", "blocks")
    for seed in seeds:
        prefix = f"seed{seed}"

        def add(name: str, request: str, *coverage: str, **controls: object) -> None:
            result.append(Fixture(prefix + "/" + name, request, tuple(coverage), **controls))

        add("init", f"init\t{seed}", "all82-actual-layer-states", "shared-random-construction", "fresh-genuine-generator")
        add("state-initial", "state", "all82-actual-layer-states", equivalent_to=prefix + "/init")
        add("buffers-normal", "buffers\t425\t425\t425\t25\t25\t425\t32768", "minimum-valid-spans",
            preserve_fields=("climate", "continuation"))
        add("density-origin", "density\t0\t0", "425-exact-density-values", "actual-normal-climate16x16", "five-scratch-channels",
            preserve_fields=("blocks", "continuation"))
        add("base-origin", "base\t0\t0", "standalone-base-adapter", "all32768-base-bytes", "q425-and-scratch-recompute-control",
            preserve_fields=("climate", "d", "e", "f", "g", "h", "density", "continuation"))
        add("generate-origin", "generate\t0\t0", "composed-generate", "q425-and-scratch-recompute-control",
            equivalent_to=prefix + "/base-origin")
        add("state-after-origin", "state", "constructor-state-unchanged", equivalent_to=prefix + "/init",
            equal_fields=("state", "continuation"), preserve_fields=("climate", *mutable, "continuation"))
        add("buffers-oversize", "buffers\t428\t431\t437\t28\t31\t432\t32773", "distinct-oversized-spans", "prefilled-suffixes",
            preserve_fields=("climate", "continuation"))
        add("neighbor-positive", "generate\t1\t0", "positive-neighbor", "scratch-suffix-clearing", "density-block-suffix-retention")
        add("negative-density", "density\t-1\t0", "negative-coordinate", "five-scratch-channels", preserve_fields=("blocks", "continuation"))
        add("negative-base", "base\t-1\t0", "standalone-base-adapter", "q425-and-scratch-recompute-control",
            preserve_fields=("climate", "d", "e", "f", "g", "h", "density", "continuation"))
        add("negative-repeat", "generate\t-1\t0", "repeat-same-chunk", equivalent_to=prefix + "/negative-base")
        add("negative-moved", "generate\t-17\t31", "moved-negative-chunk", "climate-input-order")
        add("observed-cold-location", "generate\t-1099\t279", "seed0-observed-ice-location", "cold-warm-sea-selection")
        add("independent-origin-products", "generate\t134217728\t-134217729", "independent-java-int-products4-and16", "numeric-coordinate-boundary")
        add("integer-extremes", "generate\t-2147483648\t2147483647", "java-int-coordinate-extremes", "numeric-coordinate-boundary")
        add("wrapped-equivalent", "generate\t0\t-1", "wrapped-origin-equivalence", equivalent_to=prefix + "/integer-extremes")
        add("state-final", "state", "constructor-state-unchanged", equivalent_to=prefix + "/init",
            equal_fields=("state", "continuation"), preserve_fields=("climate", *mutable, "continuation"))

    result += [Fixture("max-capacity/init", "init\t0", ("fresh-genuine-generator",)),
               Fixture("max-capacity/buffers", "buffers\t512\t512\t512\t64\t64\t512\t32832", ("bounded-max-capacity",)),
               Fixture("max-capacity/generate", "generate\t-1099\t279", ("scratch-suffix-clearing", "density-block-suffix-retention")),
               Fixture("max-capacity/density-moved", "density\t1\t1", ("retained-block-output",), preserve_fields=("blocks", "continuation")),
               Fixture("max-capacity/base-moved", "base\t1\t1", ("standalone-base-adapter", "q425-and-scratch-recompute-control"),
                       preserve_fields=("climate", "d", "e", "f", "g", "h", "density", "continuation")),
               Fixture("max-capacity/state", "state", ("constructor-state-unchanged",), equivalent_to="max-capacity/init",
                       equal_fields=("state", "continuation"))]
    labels = {case.label for case in result}
    if len(result) > 1024 or len(labels) != len(result):
        raise ValueError("fixture count/label contract")
    if any(case.equivalent_to is not None and case.equivalent_to not in labels for case in result):
        raise ValueError("control target missing")
    return result
