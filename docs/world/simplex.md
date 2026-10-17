# Seeded 2D simplex noise

Issue [#19](https://github.com/frankischilling/minecraft-c/issues/19) implements
the original simplex and octave family used by biome climate inputs. Its
[reference notes](../reference/simplex.md) identify both original classes and
their operation order. Climate blending, biome classification/caches, dimensions,
terrain density, surfaces, caves, population and lighting remain separate work.

The allocation-free `mc_world` library exposes
[simplex.h](../../include/mc/world/noise/simplex.h) alongside the
[Perlin interface](noise.md). Callers own sampler state, octave layers and output
storage. Initialize from an explicit `mc_java_random`. Each constructor consumes
three offset doubles, including the unused third offset, before shuffling all
256 permutation values. Sharing a Random preserves the order of later Perlin
and simplex constructors. Sampling does not consume it.

State fields are immutable after initialization. Multiple readers may sample
them; initialization and shared Random mutation require one owner or external
synchronization. The original initialized skew/unskew constants are public
read-only doubles, with raw bits `3fd76cf5d0b09954` and `3fcb0cb174df99c8`.
Both were observed before choosing their exact C hexadecimal literals.

`mc_noise_simplex_add` adds samples multiplied by amplitude to existing output.
Coordinates are `(origin + index) * scale + offset`, with X outermost and Z
innermost. Any suffix after the grid product is retained. A one-by-one grid
uses the actual original bulk operation; this interface adds no original scalar
or three-dimensional method.

The original floor returns the Java int cast for a value greater than zero,
otherwise the wrapping cast-minus-one. Zero and exact negative integers therefore
differ from mathematical floor. Cell-index addition wraps as Java int before
conversion to double. The implementation retains the projected gradient table's
duplicate rows, strict triangle comparison, zero-coefficient multiplication,
attenuation operations and corner-sum grouping.

`mc_noise_simplex_octaves_fill` clears the entire supplied span, including its
suffix. Initial scales are divided by 1.5. Frequency and amplitude divisor start
at one; each layer receives the scaled frequency and amplitude `0.55 / divisor`,
then advances each factor by its own caller multiplier. Passing
`MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER` reproduces the original short
overload's second factor of 0.5. There is no final normalization. Zero layers
still clear output.

Dimensions and their product must fit `INT32_MAX`. Capacity must cover the product,
and the full output span must fit `SIZE_MAX / sizeof(double)`. Invalid arguments
return false before changing state, Random or output. Zero-size additive grids
do nothing; octave fill still clears the span. Null output is valid only with
zero length. Output must not overlap state, layers or the grid descriptor.
Initialized state and valid storage are caller preconditions, not detection of
arbitrary invalid pointers.

Exact comparisons assume binary64 round-to-nearest on tested x86-64 platforms,
with contraction and fast-math disabled. Nonfinite inputs have no NaN payload
identity guarantee. [Testing](../testing.md) separates public fixed observations
from the optional original-class comparison. Primitive noise agreement does not
prove original biome or world generation.
