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
inspection remains a draft implementation, with live original-region validation
pending. The original seeded Perlin/octave module is implemented.
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
[natural and protected comparisons](reference/caves.md). Continue with
[fresh chunk data, heightmaps and direct skylight](world/chunk.md).
Complete raw chunks, population, lighting and protocol framing remain separate
required work. Define their scoped implementations from original class/method
evidence before coding. Further splits must retain the full original requirements
rather than replacing them with a simpler private save format or rewritten-only
network protocol.

No release is planned for the numerical/tooling foundation. Preview releases
need real executable behavior, accurately scoped compatibility claims, dependency
notices, native results, and an artifact inspection excluding original material.
