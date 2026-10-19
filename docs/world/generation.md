# Unpopulated normal chunk generation

The separate [concrete GAME provider operation](provider-generation.md) uses
actual World/Registry owners and preserves reached failure effects. The
interface below remains the numerical composition with its original preflight.

`mc/world/generation/normal.h` composes the normal generator's direct chunk
entry: fresh chunk data, climate and biomes, density and base terrain, surface
replacement, caves, then full finishing. A successful result is a new normal
chunk that has not been published to the cache. This interface belongs to
`mc_world` and initializes no display, audio, files or Java runtime.

The [reference notes](../reference/generation.md) record the verified
`a08c12056a06261ed01d9f66999459f7ce9543b4` checkpoint: all 41 local tests pass
on native MSVC and WSL GCC/Clang without skips, with four-seed original
comparisons, untouched controls and lossless public C replay. This finite
unpublished entry does not establish gameplay parity.

## Generator and world ownership

Zero-initialize `mc_normal_generator` before calling
`mc_normal_generator_init(generator, world_seed, manager, trig)`. The manager
must already be initialized for that same world seed. The generator borrows
the world's existing mutable `mc_biome_manager`, its allocation owner and an
initialized immutable `mc_math_trig` table. It neither replaces nor reseeds the
manager. Construction initializes all 82 terrain layers and retains the
generator Random's constructor continuation without sampling or allocating
manager buffers.

The generator owns fixed density and surface scratch, separate generator and
cave Random states, and one separately allocated biome output. Only that biome
output owns dynamic storage; it uses the borrowed manager's existing
`mc_biome_memory` owner. Do not copy an owning generator or modify its
descriptors, dependencies, initialized flag or terrain layers. Scratch values
and Random continuations remain available for inspection between calls.

`mc_normal_generator_free` releases the separate biome output and restores the
generator's zero state. It accepts `NULL` or a zero-initialized object. It does
not release the manager, manager arrays `a/b/c/d`, trig table, chunk results or
block storage. The manager and allocation owner must outlive the generator;
free the generator before releasing either borrowed owner.

## Per-call resources and returned chunks

`mc_normal_generation_resources` supplies the current world lighting/cache
context, material pairs, opacity table, optional finishing trace and optional
queue-abort count. The lighting context, generator seed and borrowed biome
manager must describe the same logical world. Keep all inputs stable and grant
exclusive access to the generator, manager, world and outputs for the whole
call. No callbacks may observe or mutate intermediate state.

Supply every original biome identity's current top/filler byte patterns through
`mc_normal_generator_materials.by_biome`. Its 13 slots use `mc_biome_id - 1`;
`MC_BIOME_NONE` has no slot. Hell and Sky still require slots even though the
normal climate classifier ordinarily selects other identities. All byte
patterns are accepted. The implementation substitutes no default materials.

The first 256 opacity entries must be nonnegative signed integers. Additional
entries are ignored. The supplied material and opacity snapshots are borrowed
for the call and remain unchanged.

`mc_normal_generator_generate` receives a caller-owned `mc_normal_chunk` and
separate mutable block storage of at least 32768 bytes. The result embeds both
`mc_chunk_state` and `mc_chunk_data`; its state points to that embedded data,
which borrows the supplied blocks. Keep the wrapper at a stable address while
the result is retained. Distinct retained chunks require distinct wrappers and
block storage. The generator keeps no ownership of returned chunks.

The result's state and data identities must be absent from both live cache
entries and the ordered insertion list. Preflight rejects direct equality of
those identities. Block-buffer disjointness, partial overlap and all other
borrowed storage separation remain caller requirements. Cached objects used
only for height queries need not expose initialized block pointers.

## Composition and retained state

Each successful call performs these stages in order:

1. Seed the generator Random from separately widened signed chunk coordinates:
   `x * 341873128712L + z * 132897987541L`, with Java long wrapping. This seed
   does not include a world-seed XOR.
2. Prepare fresh normal chunk data and zero the 32768-byte block prefix.
3. Sample the direct 16 by 16 biome grid at Java-wrapped `chunk_x * 16` and
   `chunk_z * 16`. The borrowed manager updates processed temperature `a`,
   processed rainfall `b` and raw auxiliary noise `c`. The generator receives
   the biome identities in its separate output; manager `d` remains untouched.
4. Compute normal density and base blocks from that processed climate and the
   retained terrain layers. Retain density and octave scratch for inspection.
5. Select each column's current top/filler material pair and apply the original
   surface operation using the chunk-seeded generator Random.
6. Run the fixed-range cave scheduler with a separate Random reseeded from the
   world seed. Cave generation preserves the generator/surface Random.
7. Finish the chunk completely, including local heights/direct skylight,
   1024 ordered world-height queries, sky queue requests and the dirty effect.

The fresh constructor state is staged until biome sampling's allocations
succeed. This changes no externally observable successful behavior under the
required exclusive access and absence of callbacks. It permits checked
allocation failure to preserve the caller's existing result and block bytes.

Success returns fresh zero metadata and block light, computed heights and
direct skylight, and a nonblank dirty chunk at the requested coordinates. The
block capacity suffix is retained. Both Random continuations commit only on
success. A cave Random's arbitrary constructor state has no reference-defined
value; every successful cave pass reseeds it before use.

The generator's density and surface spans have fixed addresses and sizes.
Biome buffers can allocate on their first use, then retain their storage when
the existing capacity suffices. Direct sampling leaves the world's cached
biome output `d` alone, including any preexisting contents. Calling
`mc_biome_cached_square` would implement a different ownership operation.

## Coordinates, preflight and failure

Both chunk coordinates must be in `INT32_MIN + 8` through `INT32_MAX - 9`,
inclusive. This is the existing finite ordinary cave-scheduler domain. Climate,
density, surface and finishing calculations still retain their own Java
coordinate wrapping within that domain. Excluded integer-edge scheduler
behavior has not been accepted through this composition.

Preflight checks the generator and manager, valid classifier identities, fixed
spans and byte-size bounds, unpublished result identities, current resources,
the complete imported world, trace storage and worst-case finishing reserve.
A bounded queue needs 1024 free entries. Full million-entry storage instead
uses the original automatic clear policy; `queue_aborts` reports how many
original diagnostics the caller must issue. An optional trace needs 1024 slots
and follows the [finishing contract](finishing.md).

| Status | Meaning and retained state |
| --- | --- |
| `MC_NORMAL_GENERATOR_OK` | The complete chunk and final Random states are usable. |
| `MC_NORMAL_GENERATOR_INVALID_ARGUMENT` | Invalid input or imported state; preserves caller outputs, generator, manager, world, trace and allocation accounting. |
| `MC_NORMAL_GENERATOR_LIMIT` | Insufficient queue/trace reserve or biome allocation budget; preserves the same state. |
| `MC_NORMAL_GENERATOR_ALLOCATION_FAILED` | A required biome allocation failed; preserves the same state. |
| `MC_NORMAL_GENERATOR_STATE_ERROR` | A defensive downstream invariant failed; partial effects may remain and the result is unusable. Random continuations are not committed. |

Biome sampling is the only allocating stage. Its existing allocator stages all
needed replacements before committing any buffer or accounting change. Fixed
stages have their checked preconditions validated before sampling and cannot
reject valid stable inputs afterward. The interface does not promise rollback
after violated ownership or state invariants.

## Cache boundary and remaining behavior

Generation finishes before publication. World-height and admission queries
therefore resolve the actual existing cache, which can contain an older chunk
at the requested key or a packed-key alias. They do not automatically see the
newly generated result's heights. Queue order and coalesced bounds remain world
effects even when returned block bytes reproduce exactly.

Publication is a separate caller operation. The existing
`mc_chunk_cache_publish` covers only explicit map replacement/insertion and
ordered-list append; it does not implement the original acquisition method's
pending-set removal, loading, fallback, lifecycle hooks or population. This
generator entry performs none of those operations, queue draining, light
propagation, persistence or I/O. Other dimensions and complete playable world
generation remain separate requirements.

The production corpus uses client base world `fd`/cache `ok` and server base
world `dj`/cache `jb`. Actual `MinecraftServer` construction uses `dp`/`he`.
Cache acquisition and genuine `ie` load research must preserve that distinction;
the current comparison does not cover a running dedicated server. The active
branch continues from this verified normal-composition checkpoint toward those
cache boundaries.
