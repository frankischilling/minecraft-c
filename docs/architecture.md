# Runtime boundaries

Project-owned runtime code uses C17. `mc_core` contains numerical compatibility,
the shared Random profile and allocation-free modified UTF-8 text helpers.
Its profile uses the private `mc_platform_time` library and C17 atomics/threads.
`mc_storage` depends on core and contains raw NBT, gzip/zlib memory compression,
level metadata and region images.
Its independent NBT tree owner keeps immutable source provenance separate from
decoded-document lifetime. `mc_item` builds on storage with stable
[ItemStack fields, holds and serialization](item/stack.md).
`mc_block` owns the distinct [material and palette identities](reference/materials.md)
and their receiver methods. Every world in one material-class lifetime must
borrow the same stable owner; material identity is separate from block IDs.
`mc_registry` builds on that owner and the shared Random profile with actual
[Block](block/registry.md) and [Item](item/registry.md) class construction. It
retains source-ordered static, base and subtype state through their recursive
initialization cycle, including failed prefixes. The same owner executes actual
[Translation](resources/translation.md), [key loading](statistics/keys.md) and
[statistic registration and arrays](statistics/registry.md), followed by real
[achievement construction](statistics/achievements.md), using the reached
[String/null formatter](reference/string-format.md) in core. Unfinished
recipe and smelting dependencies still prevent complete registry startup
and World binding.
`mc_world_info` owns per-World [metadata fields and shared references](world/info.md)
above storage, with a private platform-clock dependency for actual no-list
serialization. It borrows no store or World runtime. Runtime contexts can retain
an Info and borrow its actual scalar cells; persistence accepts the typed
no-list record without making the store the live metadata authority.
`mc_persistence` composes storage with the C world model and explicit native
filesystem ownership. `mc_world_saved_data` builds on persistence with independent
[ID-counter ownership](world/saved-data.md). It copies its fixed file path and
UTF-16 keys, without borrowing the store or WorldInfo. Complete World startup
and named SavedData subtypes remain separate work.
`mc_simulation` builds on persistence and items with the
world runtime and [Entity construction](entity/construction.md). Keeping these
mutually dependent owners together avoids a storage-to-Entity dependency.
The [native base-World constructor](world/construction.md) keeps the same
43-field runtime through RAW allocation, ordered construction, failure and
retirement. Typed Dimension/biome owners and the concrete normal provider
retain actual shared identities; block reads use the installed cache
coordinator. Direct, copied and already-loaded normal construction can finish.
Fresh spawn, full bootstrap/subclass startup and complete population remain
unfinished. [Standalone Dimension observations](reference/dimension.md) have
their own finite scope.
These libraries initialize no display, graphics, audio,
networking, assets or Java. Tests and research helpers are
separate executables/scripts; none is a hidden game engine.

The [float lookup/floor module](core/math.md) uses a caller-owned 65536-float
table initialized once with the measured system double-sine backend. Later
lookups share it as const data; the Java cast/compare floor needs no table.
There is no hidden global initializer or Java dependency in this module.

`mc_world` depends on `mc_core` and implements the original seeded
Perlin/octave [noise module](world/noise.md) and the separate
[2D simplex/octave family](world/simplex.md). The noise primitives own no allocations
or platform services. Caller-supplied layer arrays keep construction and shared-Random order
explicit. Bulk sampling preserves the original height-one path and Y-cell cache.

The [climate and biome module](world/biome.md) composes independently seeded
simplex channels with original climate processing and float-table lookup.
Caller-owned managers retain mutable climate/biome buffers under an explicit
allocation budget. External temperature/direct biome outputs stay separate
from those retained fields; calls stage growth before publishing changes.

The [scheduled-state modules](world/scheduling.md) own records, exact full-key
membership and historical ordered-tree state separately. Shared record identities
survive time changes and membership removal. Current comparison searches and
structural-first lookup remain distinct after time mutation. World scheduling,
admission and tick callbacks remain separate implementation work.

The [normal terrain module](world/terrain.md) composes all original seeded
Perlin groups into density, then interpolates base blocks. Immutable constructor
state is separate from borrowed workspace/output spans and processed climate.
Its fixed normal shape retains original flat layouts and buffer suffix behavior.

The [normal surface pass](world/surface.md) reuses immutable terrain layers,
accepts current top/filler byte pairs and advances a separate caller-owned
Random. Field sampling and material application remain usable independently;
composed calls validate all borrowed storage before any mutation.

The [normal cave pass](world/caves.md) uses a separate caller-owned Random,
immutable math table and existing block prefix. It owns the fixed neighbor
schedule, ordered local path Randoms, geometry and selective material changes.
Checked preflight precedes mutation; successful calls allocate nothing.
Private room/path helpers serve both normal runtime and focused reference adapters.

The [chunk voxel-data module](world/chunk.md) borrows the block buffer and embeds
independent packed metadata/light arrays plus column heights. It provides bounded
nibble access and local height/direct-skylight rebuilding without allocation.
World cache queries, lighting queues, dirty state and chunk lifecycle remain
separate responsibilities.

The [light-region queue](world/lighting.md) owns ordered storage and original
region merging after world admission. Caller-owned capacity makes allocation
explicit. Shared Java helpers preserve wrapped coordinate/volume arithmetic.
The newest five-entry search, in-place merges and million-entry abort policy
remain distinct from world admission and lighting propagation.

The [finishing boundary](world/finishing.md) adds a caller-owned packed-key cache,
world-height lookup, light-request admission and complete normal finishing.
Stable chunk identities let queries see rebuilt heights when that identity is
already cached. Fresh generated chunks finish before explicit publication.
Publication covers map replacement and insertion-list append only; loading,
unloading, lifecycle callbacks, population and light propagation remain separate.

The [normal generation module](world/generation.md) composes fresh chunk data,
direct climate/biome sampling, base terrain, surface, caves and full finishing.
`mc_normal_generator_init` binds the world seed, the world's initialized biome
manager and immutable trig data. Each `mc_normal_generator_generate` call receives
that same world's lighting context. The generator retains all 82 terrain layers,
fixed scratch and separate generator/cave Randoms. Its separately owned biome
output uses the manager's allocation owner; `mc_normal_generator_free` releases
only that output. Direct sampling updates `a/b/c`
while preserving `d`. Each call receives all 13 current biome material pairs
and current opacity explicitly. Fresh caller-owned results remain unpublished;
both coordinates must lie in `INT32_MIN + 8` through `INT32_MAX - 9`, the finite
ordinary cave-scheduler domain. Checked resource/allocation failures preserve
state; defensive downstream invariant failures report an unusable result.

Normal composition is verified at
`a08c12056a06261ed01d9f66999459f7ce9543b4`: all 41 selected local tests pass
without skips on MSVC/GCC/Clang, including four-seed original comparisons and
complete untouched controls. [Generation evidence](reference/generation.md)
records the exact sources, manifest hashes and finite headless conditions.
This checkpoint uses base worlds `fd`/`dj` and caches `ok`/`jb`. Actual
`MinecraftServer` construction uses `dp`/`he`. The separate
[acquisition coordinator](world/cache.md) now has a retained-provider comparison
using the actual persistent client and dedicated-owner constructor routes.
[Cache evidence](reference/cache.md) distinguishes this finite API context from
running-server behavior. Separate [callback evidence](reference/cache-callbacks.md)
covers successful original loading and natural population, replaying observed
cache/world effects around C coordinator transitions. Expanded exact source
`28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 selected tests without
skips on MSVC/GCC/Clang, with Linux sanitizers, independent audit and hosted checks.
The [read-only region reader](storage/region.md) retains bounded framing,
compression and raw NBT composition, with exact-source finite acceptance at
`092301ae8282d50a734df9018f791925aa9be99b`. It does not establish a world schema
or a disk chunk decoder.

The newer [world storage module](world/storage.md) owns stable decoded chunks,
native region transport and the world-store composition. Its
[image writer](storage/region-writer.md) accepts explicit compressed bytes and
timestamps; the [metadata codec](storage/level.md) owns schema interpretation
separately from gzip, fallback and file rotation. Storage loads preserve opaque
state; the acquisition adapter refuses unsupported entity/tile activation.
At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71 selected tests pass without
skips on native MSVC/GCC/Clang, with Linux sanitizers and independent provenance
and copied-world semantic audits. The
[storage checkpoint](reference/storage.md#exact-source-storage-checkpoint)
defines its finite original-to-C and C-to-original scenarios. That checkpoint does not add save/unload
scheduling or live entity/player serializers.

The [lifecycle module](world/lifecycle.md) traverses the live insertion list,
saves eligible chunks and selects pending unloads through the bounded Integer
set's actual mutation history. The store registers borrowed cache/fallback
bindings and counts explicit leases. Unpublication never frees an owner;
individual retirement refuses active, mapped, listed, fallback or leased
identities and recovers capacity only after those references end.

`world/runtime` belongs to `mc_simulation`. It binds acquisition, generation
retention, storage and lifecycle callbacks to one stable world clock and fresh
metadata views. Generation reserves an owner slot before running and returns a
stable retained copy; later payload-allocation failure preserves generation's
completed effects. Loaded chunks preserve their source NBT. Runtime destruction
refuses live calls, leases, persistent Entity/Tile bindings or nonempty
cache/list/pending state and does not
save or free store chunks. Empty entity/tile records are native; other bodies
remain explicit external seams. Its [qualification ledger](reference/lifecycle.md#exact-source-lifecycle-checkpoint)
is separate from the accepted storage checkpoint.

The runtime optionally borrows the existing application Random profile. Its
default-construction bridge enters that profile once and retains the exact
typed result separately from storage diagnostics. This foundation binds already
loaded metadata; it does not reproduce original World construction's earlier
Random, clock and Q-field sequence. Standalone ItemEntity constructors use the
shared profile, retain a world at its field store, and remain separate from
world insertion, ticking and loaded chunk entity activation.

The reference controller retains at most 32768 target object mirrors from their
first serialized IDs through a capture session. A separate synthetic JDI check
covers debugger disposal, late helper labels, capacity and duplicate rejection.
This reference-tool ownership is independent of native chunk retirement.

The public header is the boundary for Java arithmetic and explicitly seeded
`Random`. Callers own each generator. Invalid divisors and bounds return failure
without changing outputs or consuming randomness; game-specific exception behavior
belongs to the caller. See [evidence](reference/java-semantics.md) for operation
coverage and the remaining semantics audit.

Each Random owner also retains cached Gaussian bits and validity. Fresh native
initialization clears both; Java-style reseeding clears validity while keeping
the stored double. Gaussian misses use the core's strict log and SSE2 square
root under the [documented FP contract](reference/random-gaussian.md). Native
owners and complete continuation copies include this state.

The [shared Random profile](reference/random-profile.md) owns constructor entropy
and a separate atomic Math stream above individual worlds and registries. A
successful full-width uniquifier CAS precedes one real platform clock sample.
Lazy Math initialization publishes one retained owner; each double has separate
26-bit and 27-bit atomic LCG steps that may interleave across callers. Queries
and destruction require quiescence. Windows QPC and Linux monotonic backends
preserve their distinct conversions and native failure policies. Explicit seed
and reseed stay entropy-free. Game startup and population/entity/registry wiring
to this owner remain unfinished.

The separate private [millisecond wall clock](reference/wall-clock.md) uses
Windows FILETIME or Linux CLOCK_REALTIME. It has checked integer conversions,
no retained state and no monotonicity guarantee. World startup has not yet
been connected to it.

The [NBT boundary](storage/nbt.md) owns retained decoded trees and budgets their
allocations. It accepts bytes and produces bytes without compression, streams
or world schemas. Strings preserve UTF-16 code units, and parser failures leave
caller result outputs unchanged. The separate compression module owns its
decoded bytes and validates whole member checksums. Callers compose these
interfaces; neither implicitly selects a world schema or opens a file.

## System and subsystem folders

Runtime implementations live at `src/<system>/<subsystem>/`; public headers mirror
that path under `include/mc/`, and tests mirror it under `tests/`. This keeps a
subsystem's implementation and interface easy to locate as the project grows.
Create new folders with actual implementations rather than empty placeholders.

```text
include/mc/core/java/java.h
src/core/java/java.c
include/mc/core/java/random_profile.h
src/core/java/random_profile.c
src/core/java/random_profile_internal.h
src/platform/time/nanotime.h
src/platform/time/nanotime.c
src/platform/time/millis.h
src/platform/time/millis.c
include/mc/world/info/info.h
src/world/info/info.c
tests/world/info/test_info.c
tests/core/java/test_random_profile.c
tests/platform/time/test_nanotime.c
tests/platform/time/test_millis.c
include/mc/core/java/int_set.h
src/core/java/int_set.c
tests/core/java/test_java.c
tests/core/java/test_int_set.c
include/mc/core/math/math.h
src/core/math/math.c
tests/core/math/test_math.c
tests/core/math/math_probe.c
tests/reference/math/TrigMathObservation.java
scripts/reference/math/compare_math.py
include/mc/core/text/mutf8.h
src/core/text/mutf8.c
tests/core/text/test_mutf8.c
include/mc/storage/nbt/nbt.h
src/storage/nbt/nbt.c
tests/storage/nbt/test_nbt.c
tests/storage/nbt/nbt_probe.c
include/mc/storage/compression/compression.h
src/storage/compression/compression.c
tests/storage/compression/test_compression.c
tests/storage/compression/compression_probe.c
include/mc/storage/level/level.h
src/storage/level/level.c
tests/storage/level/test_level.c
include/mc/storage/region/writer.h
src/storage/region/writer.c
tests/storage/region/test_writer.c
include/mc/world/storage/chunk.h
include/mc/world/storage/regions.h
include/mc/world/storage/store.h
src/world/storage/chunk.c
src/world/storage/regions.c
src/world/storage/store.c
src/world/storage/files.c
include/mc/world/chunk/references.h
include/mc/world/cache/lifecycle.h
src/world/cache/lifecycle.c
tests/world/cache/test_lifecycle.c
include/mc/world/runtime/runtime.h
src/world/runtime/runtime.c
tests/world/runtime/test_runtime.c
tests/world/storage/test_chunk.c
include/mc/world/noise/noise.h
src/world/noise/noise.c
tests/world/noise/test_noise.c
tests/world/noise/noise_probe.c
tests/reference/noise/NoiseProbe.java
scripts/reference/noise/compare_noise.py
include/mc/world/noise/simplex.h
src/world/noise/simplex.c
tests/world/noise/test_simplex.c
tests/world/noise/simplex_probe.c
tests/reference/simplex/SimplexProbe.java
scripts/reference/simplex/compare_simplex.py
include/mc/world/biome/biome.h
src/world/biome/biome.c
tests/world/biome/test_biome.c
tests/world/biome/biome_probe.c
tests/reference/biome/BiomeProbe.java
scripts/reference/biome/compare_biome.py
include/mc/world/terrain/terrain.h
src/world/terrain/terrain.c
tests/world/terrain/test_terrain.c
tests/world/terrain/terrain_probe.c
tests/reference/density/DensityProbe.java
scripts/reference/density/compare_density.py
include/mc/world/terrain/surface.h
src/world/terrain/surface.c
tests/world/terrain/test_surface.c
tests/world/terrain/surface_probe.c
tests/reference/surface/SurfaceProbe.java
scripts/reference/surface/compare_surface.py
include/mc/world/terrain/caves.h
src/world/terrain/caves.c
src/world/terrain/caves_internal.h
tests/world/terrain/test_caves.c
tests/world/terrain/caves_probe.c
tests/reference/caves/CavesObservation.java
scripts/reference/caves/compare_caves.py
include/mc/world/generation/normal.h
src/world/generation/normal.c
tests/world/generation/test_normal.c
tests/world/generation/normal_probe.c
tests/reference/generation/NormalGenerationObservation.java
tests/reference/generation/NormalGenerationController.java
scripts/reference/generation/compare_normal.py
scripts/reference/generation/export_normal.py
include/mc/world/chunk/data.h
include/mc/world/chunk/nibbles.h
src/world/chunk/data.c
src/world/chunk/nibbles.c
tests/world/chunk/test_data.c
tests/reference/java/JavaSemanticsProbe.java
tests/reference/nbt/NbtProbe.java
tests/tools/resources/test_import_assets.py
scripts/development/format_code.py
scripts/resources/import_assets.py
scripts/reference/binaries/inspect_jars.py
scripts/reference/java/compare_java.py
scripts/reference/nbt/compare_nbt.py
scripts/reference/server/run_smoke.py
```

Formatting follows the repository `.clang-format` and is
repeatable through `scripts/development/format_code.py` and checked in CI with
clang-format 18. The formatter scans project C headers/sources only, keeping
ignored decompiled research outside its input set.

## Planned game systems

As implementation proceeds, the intended boundaries are:

| Module | Responsibility | Dependency rule |
| --- | --- | --- |
| Core | Defined Java numerical/text/serialization behavior | Usable by headless tools and server |
| World and simulation | Chunks, blocks, entities, ordered tick updates, generation | No rendering or audio initialization; preserve evidenced side distinctions |
| Persistence | NBT, region files, world/player/entity state | Preserve unimplemented fields before writable compatibility paths |
| Networking | Original packet encoding, stream framing, connection states | Bounded parsing and explicit buffering; original protocol |
| Platform | Clock, files, sockets, window/input/audio device services | Native Linux/Windows implementations with explicit ownership |
| Renderer and audio | OpenGL 3.3 core presentation, resource loading, audio events | Linked by graphical client only |
| Client | Menus, input sampling, single-player and multiplayer sessions | Render frequency must not change simulation state |
| Dedicated server | Headless configuration, administration, save and shutdown | No display, OpenGL context, or client audio dependency |

Add interfaces as reference-backed game behavior is implemented.
CMake build options apply strict diagnostics
and floating-point rules to project code; future dependencies need their own
licensing and build review. SDL3 is a candidate, and its current APIs must be
verified before use. Storage uses pinned static zlib; its
[license and build configuration](dependencies.md) remain separate from project code.

Use checked allocation, bounded buffers, and explicit ownership/cleanup paths.
Begin with reproducible simulation order. Parallel research/build tasks do not
authorize concurrent simulation updates without equivalence evidence.
