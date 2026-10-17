# Milestones and dependencies

The [parity inventory issue #4](https://github.com/frankischilling/minecraft-c/issues/4)
stays open until the full agreed behavior is implemented and verified. Its
[discovery inventory](inventory.md) preserves scope that is not yet split into
individual implementation issues. Each next issue needs original evidence,
concrete acceptance scenarios and dependencies before implementation starts.

| Milestone | Scope and completion evidence | Depends on |
| --- | --- | --- |
| [M1: Reference evidence and deterministic core](https://github.com/frankischilling/minecraft-c/milestone/1) | [#1](https://github.com/frankischilling/minecraft-c/issues/1): verified binaries, research tooling/notes and launch-access audit; [#2](https://github.com/frankischilling/minecraft-c/issues/2): defined numerical helpers and exact Java primitive checks; [#3](https://github.com/frankischilling/minecraft-c/issues/3): safe local import; [#5](https://github.com/frankischilling/minecraft-c/issues/5): formatting and subsystem folders. Native Windows/Linux CI required. | Supplied references and toolchain |
| M2: Original storage and generation foundations | Bounded original NBT/text/region formats; read-only imported-world inspection before writes; exact noise/raw/populated/lit chunk comparisons on both sides, dimensions and edge coordinates; preservation of unsupported state. | M1; more detailed original field/order research |
| M3: Graphical single-player runtime | Native window/input/audio services, OpenGL3.3 rendering; original menus/settings, player movement/actions/inventory; ordered ticks, world loading and safely implemented persistence. Fixed scenarios compare to original. A playable subset remains an intermediate result. | M1/M2 and reachable behavior inventory |
| M4: Original protocol and dedicated-server interoperability | Original stream framing/packets, offline joins, synchronization, server properties/admin/save/shutdown; all four original/C pairings, multiple clients and reconnect/restart tests. | Numerical/storage/simulation behavior; original loopback runtime baseline |
| M5: Full inventory parity and platform validation | Remaining blocks/items/recipes/entities/farming/updates/fluids/redstone/pistons/dimensions and presentation/audio; native Linux/Windows sessions, extended drift/performance measurements, both save round-trip directions, documentation/hosting/resource setup and audited packaging. | Every prior subsystem and all outstanding inventory items |

M2 through M5 are ordering guides. Implement and merge cohesive scoped increments;
do not postpone original comparisons until a whole milestone is built. Parallel
agents may research independent behavior, but shared interfaces, builds, Git
operations and integration need explicit ownership.

The next implementation candidates are original NBT and its Java text encoding,
then deterministic noise/chunk generation and protocol framing. Create their
scoped issues from the original class/method evidence before coding. Further
splits must retain the full original requirements rather than replacing them
with a simpler private save format or rewritten-only network protocol.

No release is planned for the numerical/tooling foundation. Preview releases
need real executable behavior, accurately scoped compatibility claims, dependency
notices, native results, and an artifact inspection excluding original material.
