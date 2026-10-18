# Milestones and dependencies

Full parity remains incomplete until the full agreed behavior is implemented
and verified. The [discovery inventory](inventory.md) preserves scope that is
not yet split into individual implementation tasks. Each increment needs original
evidence, concrete acceptance scenarios and dependencies before implementation starts.

| Milestone | Scope and completion evidence | Depends on |
| --- | --- | --- |
| [M1: Reference evidence and deterministic core](https://github.com/frankischilling/minecraft-c/milestone/1) | Verified binaries, research tooling/notes and launch-access audit; defined numerical helpers and exact Java primitive checks; safe local import; formatting and subsystem folders. Native Windows/Linux CI required. | Supplied references and toolchain |
| [M2: Original storage and generation foundations](https://github.com/frankischilling/minecraft-c/milestone/2) | Bounded original NBT/text/region formats; read-only imported-world inspection before writes; exact noise/raw/populated/lit chunk comparisons on both sides, dimensions and edge coordinates; preservation of unsupported state. | M1; more detailed original field/order research |
| [M3: Graphical single-player runtime](https://github.com/frankischilling/minecraft-c/milestone/3) | Native window/input/audio services, OpenGL 3.3 rendering; original menus/settings, player movement/actions/inventory; ordered ticks, world loading and safely implemented persistence. Fixed scenarios compare to original. A playable subset remains an intermediate result. | M1/M2 and reachable behavior inventory |
| [M4: Original protocol and dedicated-server interoperability](https://github.com/frankischilling/minecraft-c/milestone/4) | Original stream framing/packets, offline joins, synchronization, server properties/admin/save/shutdown; all four original/C pairings, multiple clients and reconnect/restart tests. | Numerical/storage/simulation behavior; original loopback runtime baseline |
| [M5: Full inventory parity and platform validation](https://github.com/frankischilling/minecraft-c/milestone/5) | Remaining blocks/items/recipes/entities/farming/updates/fluids/redstone/pistons/dimensions and presentation/audio; native Linux/Windows sessions, extended drift/performance measurements, both save round-trip directions, documentation/hosting/resource setup and audited packaging. | Every prior subsystem and all outstanding inventory items |

M2 through M5 are ordering guides. Implement and merge cohesive scoped increments;
do not postpone original comparisons until a whole milestone is built. Independent
research can proceed in parallel, but shared interfaces, builds, Git operations
and integration need explicit ownership.

The raw NBT and modified UTF increment is integrated, as is bounded gzip/zlib,
with [both-side evidence](reference/compression.md). Read-only original region
inspection passes [exact-source original comparisons](reference/region.md) on
MSVC/GCC/Clang and hosted checks. Its finite local record scope excludes global
file mapping, world schemas and writes. The original seeded Perlin/octave module
is implemented.
The separate 2D simplex/octave primitives are implemented, with exact-source
acceptance recorded in [status](status.md). Climate blending and biome classification
are implemented, with
[actual both-side numerical methods and state comparisons](reference/biome.md).
Normal-dimension density/base interpolation is integrated, with
[actual both-side context and numerical comparisons](reference/density.md).
Surface replacement and chunk seeding are integrated, with
[recorded scope and results](reference/surface.md) and native public CI.
The float lookup/floor prerequisite is integrated, with
[complete table and sampled scalar comparisons](reference/math.md) and native
public CI. The normal cave runtime is integrated, with
[natural and protected comparisons](reference/caves.md). Fresh chunk data,
heightmaps, direct skylight and packed access now have
[exact original comparisons](reference/chunk.md) for their local-state scope.
Light-region merging and ordered queue storage now have a separate
[contract and original observations](reference/lighting.md). The C module
starts after world admission. The [normal finishing boundary](world/finishing.md)
composes those stages with cached heights, admission and dirty effects.
[Actual height traces and untouched controls](reference/finishing.md) cover six
fixed fresh-world cases with two passes per side. Retain the distinction between
the unpublished measured chunk and the existing cached chunks. The separate
acquisition coordinator below adds cache load/generate selection and callback
lifecycle order. Terrain-feature bodies, save/unload and light propagation remain
separate required implementations.

The [direct normal generator composition](world/generation.md) is implemented:
fresh data, direct climate/biomes, density/base, current-material surface,
separately seeded caves and full finishing, with fresh unpublished results.
It borrows the same world's existing biome manager, uses a separate biome
buffer while preserving manager `d`, and accepts all 13 current material pairs
and opacity explicitly. Its accepted coordinates retain the finite ordinary
cave-scheduler domain. The 99073 public contract checks pass on MSVC/GCC/Clang,
including sanitizer checks. The [production capture](reference/generation.md)
for seeds 23, 24, 26 and 51 passes with complete untouched controls. Lossless
public C replay passes eight original inputs, 72 complete chunks and 73728
queries. Exact source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes all 41
selected local tests on MSVC/GCC/Clang without skips, with Linux ASan/UBSan.
All three original pairings and complete untouched controls pass per build;
independent audit verifies 47 exact-commit sources, 57 stable inputs and eight
helper classes. [Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass 36 public/numerical tests per compiler and formatting for 79 C/header
files. These results identify the a08 checkpoint; later acquisition acceptance
is recorded separately below.
`MC_ENABLE_GENERATION_REFERENCE_TESTS` selects
the original comparison; public helper and fixture checks are named
`tools.normal_reference` and `tools.normal_interop`.

The [cache coordinator](world/cache.md) composes retained C generation and
finishing with pending removal, ordered publication, activation and live
population gates. Its [retained-provider comparison](reference/cache.md) observes
actual persistent client and dedicated-owner constructors. All 52 selected tests
pass at exact source `9a50ac67997082c89dbc43cf4274fba1427c1565` on MSVC/GCC/Clang
with Linux sanitizers, independent audit and hosted checks.

Separate [callback observations](reference/cache-callbacks.md) add actual
successful `ld`/`hb` loads and natural population in `fd`/`ok` and `dj`/`jb`
base worlds. Observed effects within the recorded cache/world boundary are
replayed while C computes cache transitions. Exact source
`28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 selected local tests
without skips on MSVC/GCC/Clang with Linux sanitizers and independent audit.
Hosted checks pass 48 tests per compiler and formatting for 89 C/header files.

The [world storage foundation](world/storage.md) now has native metadata and
session ownership, stable decoded chunks, dimension roots and bounded region
writes. At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71 selected tests
pass without skips on native MSVC/GCC/Clang, with Linux sanitizers and
independent platform/provenance/semantic audits. Both finite copied-world
directions pass; [storage evidence](reference/storage.md#exact-source-storage-checkpoint)
records their exact source, captures and limits.
Opaque entity, tile and Player bodies are retained with unsupported edits
refused. This does not implement their simulation or complete save scheduling.

Save/unload lifecycle, terrain-feature population bodies, light propagation,
other dimension generation and protocol framing remain separate required work. Define their
scoped implementations from original class/method evidence before coding.
Further splits must retain the full original requirements
rather than replacing them with a simpler private save format or rewritten-only
network protocol.

No release is planned for the numerical/tooling foundation. Preview releases
need real executable behavior, accurately scoped compatibility claims, dependency
notices, native results, and an artifact inspection excluding original material.
