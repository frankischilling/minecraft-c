# Normal surface replacement

`mc_world` implements the normal-dimension surface pass in
`src/world/terrain/surface.c`, with its interface in
`include/mc/world/terrain/surface.h`. It uses the existing immutable
[82-layer terrain state](terrain.md), explicit mutable Random, material pairs,
borrowed scratch and an existing base-block buffer. It allocates nothing and
initializes no world, files, graphics, assets or Java. The
[original evidence](../reference/surface.md) records exact methods and observations.

## Material and Random inputs

`mc_terrain_surface_material` holds the current top/filler byte patterns for one
column. Supply at least 256 pairs in `x*16+z` order. Original biome fields `p/q`
are mutable, so identity alone does not define their current values. The C
surface interface accepts explicit pairs; it does not freeze an assumed biome
material table. Fresh original defaults are grass/dirt except Desert and Ice
Desert, which use sand/sand. The comparison fixture snapshots those actual
attributes and uses their verified default mapping on the C side.

Random belongs to the caller. For a direct post-constructor call, copy
`state.random_after_init` into a separate instance. For a normal chunk-prefix
context, seed that instance with `mc_terrain_normal_chunk_seed(x,z)` through
`mc_java_random_seed`. The seed helper widens signed chunk coordinates to long,
then multiplies/adds with Java wrapping. It adds no world-seed term. The pass
never implicitly reseeds, so repeated direct calls advance the supplied instance.

Noise state and its stored constructor continuation remain unchanged. Live
surface Random is separate from that immutable constructor snapshot.

## Fields and operations

The workspace borrows three double spans, each at least 256 values. `sand`
and `thickness` use the original general 16×16×1 improved-noise path; `gravel`
uses the distinct 16×1×16 height-one path. Their origin products independently
wrap chunk coordinates times 16 as Java int before conversion to double.
Sampling clears each complete scratch span, including any suffix. Flat field
and material indexing is `x*16+z`.

| Function | Behavior |
| --- | --- |
| `mc_terrain_normal_chunk_seed` | Pure wrapped normal chunk seed; no state changes |
| `mc_terrain_normal_surface_noise` | Fills all three original fields without Random draws or block changes |
| `mc_terrain_normal_surface_apply` | Reads prepared fields/materials and selectively changes base blocks while advancing explicit Random |
| `mc_terrain_normal_surface` | Validates every argument, then samples fields and applies the pass |

Application visits columns in outer-Z/inner-X order and each column from Y=127
to zero. It draws three doubles per column and a bounded integer for every Y,
including high air; sand expiration can consume another bounded draw. Omitting
those draws changes later choices and continuation. Bedrock can override every
input byte at the bottom; elsewhere only stone is replaced. Air resets the
remaining thickness without restoring the column's mutable top/filler locals.
Sea-band choices, sand over gravel precedence and sandstone transitions retain
the original state machine. Blocks use `(x*16+z)*128+y` order; only the first
32768 bytes can change, and a longer suffix remains intact.

## Ownership and checked failures

Callers own all storage. Initialized, unmodified constructor state and valid
borrowed backing arrays are preconditions. Random must be initialized where
used. Mutable scratch, blocks and Random must be disjoint from each other,
state, descriptors and read-only inputs. Standalone apply permits its three
read-only noise arrays to overlap one another. Pointer-range overlap is a caller
precondition rather than a runtime check.

Null pointers, short spans and unrepresentable complete double/material byte
lengths fail before mutation. The composed pass checks block, material and
Random arguments before clearing any scratch. Rejected calls preserve all
workspace contents/descriptors, blocks, Random and constructor state. Descriptors
are never changed. Standalone apply preserves every field/material value.

Prepared double fields may be arbitrary; thickness uses the existing defined
Java narrowing helper, including its nonfinite behavior. Original comparisons
use finite generated fields. Exact finite results assume binary32/binary64
round-to-nearest and no floating contraction on the tested x86-64 targets.

Surface replacement follows [density/base generation](terrain.md). Caves,
population, lighting, chunk finishing and complete game operation remain later
requirements. See [testing](../testing.md) for the fixed original observations,
separate constructed branch controls and exact-source acceptance.