# Seeded terrain noise

This module implements
the original improved-Perlin and octave family. See the [reference notes](../reference/noise.md).
The separate [2D simplex family](simplex.md) supplies other noise primitives.
Biome climate/classification, terrain density, block filling, surfaces, caves,
population and lighting remain separate work.

The allocation-free `mc_world` library depends only on `mc_core`. Callers own
each sampler and supply its octave layer array. Initialize with a seeded
`mc_java_random`; consecutive layers consume that same generator in original
constructor order. Independently seeding each layer changes the result and
subsequent generator state. Treat initialized fields and layer pointer/count
as immutable. Multiple readers may sample a state; initialization and shared
Random mutation need one owner or external synchronization.

`mc_noise_perlin_sample` implements the scalar three-coordinate method. The
original scalar two-coordinate slice is `(x, y, 0)`, also used by
`mc_noise_octaves_sample2`. A height-one bulk grid is different.

`mc_noise_perlin_add` adds contributions divided by the supplied divisor to an
existing output span, retaining any suffix. Coordinates are
`(origin + index) * scale + offset`; output order is X outermost, then Z, then Y.
Size-Y one uses the original special path, ignoring Y origin/scale/offset.
Other heights retain the original cache of gradient values within masked Y
cells. Replacing either path with repeated scalar calls changes the result.

`mc_noise_octaves_fill` clears the entire supplied span, including its suffix,
then accumulates layers. Frequency starts at one and halves after each layer;
it is also the contribution divisor. Zero layers still clear the span. The
original convenience overload's ignored parameter has no separate C entry.

Each dimension and its product must fit `INT32_MAX`; capacity must cover that
product, and the full output span must fit `SIZE_MAX / sizeof(double)`.
Invalid pointers, dimensions or capacities return false before changing output.
Zero-size grids are valid: additive sampling does nothing; octave fill still
clears its span. Output must not overlap state, layers or the grid descriptor.
Scalar calls require nonnull initialized state. These ownership preconditions
do not detect arbitrary invalid pointers. See [noise.h](../../include/mc/world/noise/noise.h).

Coordinate conversion uses Java's saturating cast and wrapping decrement,
without later-version coordinate reduction. Evaluation preserves binary64
grouping and disables fast-math/contraction. Exact comparisons assume normal
round-to-nearest on tested x86-64 platforms. Nonfinite inputs have no NaN payload
identity guarantee. [Tests](../testing.md) distinguish public fixed observations
from optional live original-class comparisons; neither proves full terrain parity.
