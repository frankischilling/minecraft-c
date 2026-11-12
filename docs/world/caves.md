# Overworld caves

The Overworld cave stage runs after [base terrain](terrain.md) and
[surface replacement](surface.md). The public pass is
[`mc_terrain_overworld_caves`](../../include/mc/world/terrain/caves.h).
Its [reference evidence](../reference/caves.md) covers the supplied Beta binaries.

```c
bool mc_terrain_overworld_caves(const mc_math_trig *table, int64_t world_seed,
                             int32_t chunk_x, int32_t chunk_z,
                             mc_java_random *cave_random,
                             uint8_t *blocks, size_t blocks_length);
```

Provide an initialized, immutable [math table](../core/math.md), the world seed,
target chunk, separate mutable Random storage and an existing block buffer.
The cave Random belongs to this stage; preserve the generator/surface Random
separately. Its prior contents need no initialization: every successful public
call seeds it from the world seed before drawing.

Only the first 32768 bytes are accessed, in `(x*16+z)*128+y` order. The suffix
is retained. Arbitrary byte patterns are accepted; stone, dirt, grass and water
have the original selective behavior. The caller owns all storage and lifetime.
Writable blocks and Random must be mutually disjoint and must not overlap the
table. The table must remain unmodified while readers use it. Independent calls
can share one immutable table with separate blocks and Random objects.

Null pointers, capacities below 32768, or either target outside the inclusive
range `INT32_MIN+8 .. INT32_MAX-9` return false before any mutation. Invalid
pointers, fictional capacities and overlapping objects violate caller storage
preconditions. After successful preflight the pass completes without allocation,
I/O or a mid-pass resource failure.

The coordinate range ensures both inclusive neighbor loops terminate without
counter overflow. At `INT32_MAX-8`, the original upper bound is `INT32_MAX` and
its increment wraps indefinitely. Coordinates outside the declared C domain
are rejected; equivalence there is unverified. Wrapped int multiplication and
translation inside the accepted domain are retained.

The pass owns these implementation details:

- Fixed range 8, X-outer/Z-inner scheduling of 289 source neighbors.
- World-seeded odd multipliers using Java `(nextLong()/2)*2+1`, wrapped neighbor
  seeds, and a cave Random independent of generator noise and surface state.
- Ordered bounded draws, room widths, local path Randoms seeded by the shared
  cave Random, and complete left-child execution before the right child.
- Binary32 angle/radius operations, original table lookup and cast/floor helpers,
  with strict binary32/binary64 evaluation and no floating contraction.
- Water screening on the original box shell, eligible stone/dirt/grass carving,
  the one-cell geometric/addressed-Y offset, flowing lava and grass repair.

The default step count is at most 112. Children have width at most 1, including
the binary32 rounding endpoint, so they cannot split again. At most two geometry
frames are active. Fixed range and bounded nested counts give finite structural
work and constant storage; these bounds are not a runtime deadline or a cap on
the Java Random rejection loop.

Protected room/path helpers live in the private
[`caves_internal.h`](../../src/world/terrain/caves_internal.h) seam. Normal runtime
and focused reference adapters use the same geometry implementation. Their
finite validated adapter domain is separate from the installed public pass:
target `(0,0)`, centers X/Z `0..15` and Y `0..127`, and path width `0..3`, yaw
`0..2*pi` at the original binary32 bound, pitch `-0.125..0.125`, end `85..112`,
and `0 <= step < end`. Paths use vertical ratio 1; the room wrapper supplies
its own width, step/end signals and ratio 0.5. These adapters validate their
inputs before calling the private helpers.

Original-only evidence now includes ten complete natural cases with three
public passes each, and 44 protected room/path cases on explicitly owned
32771-byte buffers. The protected patterns cover eligible and ineligible byte
IDs, water points/planes, grass repair, lava height, bounded path geometry and
raw signed-zero inputs. Shared draw counts and forks are inferred by unique
primitive-Random continuation matches; they are not local branch traces.
Both canonical comparison helpers preserve complete bytes and continuations
and measure the C table bits before comparing against the originals. Their
public transport suites each have 18 asset-independent guards; the
[evidence record](../reference/caves.md) preserves the original-only scope and
artifact hashes.

## Local acceptance

Exact source [`a507f59d756acf2d035ac7d4ea2da7171f60ad2f`](https://github.com/frankischilling/minecraft-c/commit/a507f59d756acf2d035ac7d4ea2da7171f60ad2f)
passed all 33 configured CTests without skips on native MSVC and on WSL
GCC/Clang with ASan/UBSan. All 30 original comparison manifests passed for that
source.
The natural ten-case and protected 44-case comparisons each passed C/client,
C/server and client/server pairings on all three toolchains, using fresh
Temurin 17/OpenJDK 21 original executions and measured table bits. Natural and
protected unit suites passed 2021631/2917405 checks per toolchain; both helper
suites passed 18 asset-independent guards each. See the
[integrated local evidence](../reference/caves.md#integrated-local-acceptance)
for preserved input/class/artifact identities and the acceptance-record hash.

The runtime is integrated.
Exact-head and resulting-main public CI pass 23 tests per compiler and formatting
for all 50 C/header files; the published wiki matches the versioned cave pages.
The [evidence record](../reference/caves.md#public-integration) pins those results.

This implementation covers the
Overworld cave stage and its focused controls. Complete chunk finishing, lighting,
population, other dimensions and save integration remain separate work in the
[broader parity inventory](../parity.md).
