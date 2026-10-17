"""Reviewed natural public-cave requests; coverage labels are observations only."""
from dataclasses import dataclass

@dataclass(frozen=True)
class Fixture:
    label: str
    request: str
    coverage: tuple[str, ...]

CASES = ((0, 0, 0), (1, 0, 0), (-1, 0, 0), (-(1 << 63), 0, 0),
         ((1 << 63) - 1, 0, 0), (0, -1099, 279), (0, -8, 1),
         (0, 1, 0), (1, -1, 0), (0, -17, 31))

def fixtures():
    return tuple(Fixture(f"natural-{index:02d}", f"case\t{seed}\t{x}\t{z}",
                         ("constructor82", "actual-climate-base-surface", "public-caves",
                          "restored-input-reseeding", "changed-input-observation", "suffix"))
                 for index, (seed, x, z) in enumerate(CASES, 1))
