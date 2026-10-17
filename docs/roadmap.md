# Milestones and dependencies

The [parity inventory issue #4](https://github.com/frankischilling/minecraft-c/issues/4)
stays open until the full agreed behavior is implemented and verified. Its
[discovery inventory](inventory.md) preserves scope that is not yet split into
individual implementation issues. Each next issue needs original evidence,
concrete acceptance scenarios and dependencies before implementation starts.

| Milestone | Scope and completion evidence | Depends on |
| --- | --- | --- |
| [M1: Reference evidence and deterministic core](https://github.com/frankischilling/minecraft-c/milestone/1) | [#1](https://github.com/frankischilling/minecraft-c/issues/1): verified binaries, research tooling/notes and launch-access audit; [#2](https://github.com/frankischilling/minecraft-c/issues/2): defined numerical helpers and exact Java primitive checks; [#3](https://github.com/frankischilling/minecraft-c/issues/3): safe local import; [#5](https://github.com/frankischilling/minecraft-c/issues/5): formatting and subsystem folders. Native Windows/Linux CI required. | Supplied references and toolchain |
| [M2: Original storage and generation foundations](https://github.com/frankischilling/minecraft-c/milestone/2) | Bounded original NBT/text/region formats; read-only imported-world inspection before writes; exact noise/raw/populated/lit chunk comparisons on both sides, dimensions and edge coordinates; preservation of unsupported state. | M1; more detailed original field/order research |
| [M3: Graphical single-player runtime](https://github.com/frankischilling/minecraft-c/milestone/3) | Native window/input/audio services, OpenGL 3.3 rendering; original menus/settings, player movement/actions/inventory; ordered ticks, world loading and safely implemented persistence. Fixed scenarios compare to original. A playable subset remains an intermediate result. | M1/M2 and reachable behavior inventory |
| [M4: Original protocol and dedicated-server interoperability](https://github.com/frankischilling/minecraft-c/milestone/4) | Original stream framing/packets, offline joins, synchronization, server properties/admin/save/shutdown; all four original/C pairings, multiple clients and reconnect/restart tests. | Numerical/storage/simulation behavior; original loopback runtime baseline |
| [M5: Full inventory parity and platform validation](https://github.com/frankischilling/minecraft-c/milestone/5) | Remaining blocks/items/recipes/entities/farming/updates/fluids/redstone/pistons/dimensions and presentation/audio; native Linux/Windows sessions, extended drift/performance measurements, both save round-trip directions, documentation/hosting/resource setup and audited packaging. | Every prior subsystem and all outstanding inventory items |

M2 through M5 are ordering guides. Implement and merge cohesive scoped increments;
do not postpone original comparisons until a whole milestone is built. Parallel
agents may research independent behavior, but shared interfaces, builds, Git
operations and integration need explicit ownership.

The raw NBT and modified UTF increment [#9](https://github.com/frankischilling/minecraft-c/issues/9)
is merged, as is [bounded gzip/zlib, issue #11](https://github.com/frankischilling/minecraft-c/issues/11),
with [both-side evidence](reference/compression.md). Read-only original region
inspection remains in [draft PR #15](https://github.com/frankischilling/minecraft-c/pull/15)
under [#14](https://github.com/frankischilling/minecraft-c/issues/14), with live
original-region validation pending. The original seeded Perlin/octave module
is implemented under [#16](https://github.com/frankischilling/minecraft-c/issues/16).
The separate 2D simplex/octave primitives are implemented under
[#19](https://github.com/frankischilling/minecraft-c/issues/19), with exact-source
acceptance recorded in [status](status.md). Climate blending and biome classification
are implemented under [#21](https://github.com/frankischilling/minecraft-c/issues/21),
with [actual both-side numerical methods and state comparisons](reference/biome.md).
Normal-dimension density/base interpolation is integrated under
[#24](https://github.com/frankischilling/minecraft-c/issues/24), with
[actual both-side context and numerical comparisons](reference/density.md).
Surface replacement and chunk seeding are integrated under closed
[#26](https://github.com/frankischilling/minecraft-c/issues/26), with
[recorded scope and results](reference/surface.md) and native public CI in
[PR #31](https://github.com/frankischilling/minecraft-c/pull/31).
The float lookup/floor prerequisite is integrated under closed
[#29](https://github.com/frankischilling/minecraft-c/issues/29), with
[complete table and sampled scalar comparisons](reference/math.md) and native
public CI in [PR #33](https://github.com/frankischilling/minecraft-c/pull/33).
The [normal cave runtime #30](https://github.com/frankischilling/minecraft-c/issues/30)
is integrated in [PR #35](https://github.com/frankischilling/minecraft-c/pull/35),
with [natural and protected comparisons](reference/caves.md). Continue with
[fresh chunk data, heightmaps and direct skylight #36](https://github.com/frankischilling/minecraft-c/issues/36).
Complete raw chunks, population, lighting and protocol framing remain separate
required work. Create their scoped issues from original class/method evidence
before coding. Further
splits must retain the full original requirements rather than replacing them
with a simpler private save format or rewritten-only network protocol.

No release is planned for the numerical/tooling foundation. Preview releases
need real executable behavior, accurately scoped compatibility claims, dependency
notices, native results, and an artifact inspection excluding original material.
