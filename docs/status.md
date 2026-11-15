# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and measured FPS
remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

Ordinary client [Redstone Ore activation and decay](world/redstone-ore.md)
now joins left-click mining, walking, right-click Item continuation, display
and scheduled/random ticks. The shared particle and activation bodies preserve
their original reads and Random order. Right activation returns false after
its effects, allowing the held BlockItem to place and update its use statistic.

Two fresh unchanged-original VMs agree on 67 prepared profiles. Windows and
sanitized Linux each match 19,187 selected original scalars and 15 separate
retained receipt controls. The comparison includes complete Chunk planes,
callbacks, light and scheduled queues, both Random streams, actual OS/OB caller state and Item/stat
effects. Native controls distinguish the cached old ID from a fresh World read,
and a fresh Block array lookup from a cached Block pointer.

The full Windows MSVC Release suite passes all 1,261 tests in 528.75 seconds.
All 15 selected Linux Clang tests pass in 184.19 seconds with ASan, UBSan and
leak detection; strict GCC compilation covers 12 changed C units. The
[test record](testing.md#redstone-ore-activation-and-decay) states the platform
and scenario limits. Typed null-list diagnostics now survive legacy particle
and notification calls; placement retains the actual failed producers through
checked retirement.

The [preceding Fire checkpoint](history/status-checkpoints.md#ordinary-fire-checkpoint)
retains its 71-profile comparison, full Windows and focused Linux results.
Fresh graphical input/focus qualification remains incomplete. General mining
destruction ownership, other ordinary-play blockers, sustained play/save drift,
original audiovisual equality and measured FPS remain unfinished.

## Next single-player work

- Complete remaining owned neighbor and entity paths reached by ordinary play.
- Extend natural encounters, inventory interactions, travel, death/respawn and
  save/reload scenarios beyond the existing prepared cases.
- Validate extended Windows and Linux play, including Chunk unload/reload,
  dimension changes, persistence and state drift.
- Measure frame times, stutters, memory and loading on matched original/native
  scenarios before adopting performance changes.

Multiplayer interoperability and the dedicated server follow the client work.
Account authentication is the only approved gameplay exception. A playable
subset does not complete the original behavior inventory.

Earlier results remain in [development checkpoints](history/status-checkpoints.md),
[client runtime history](history/client-runtime.md), [population history](history/population.md)
and [integration history](history.md). Their results apply to their own source
and scenario scopes.
