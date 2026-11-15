# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and performance
qualification remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

Ordinary client [Flint and Steel use](item/flint-and-steel.md) now reaches
actual Fire addition and Portal creation through the real right-click caller.
The shared body preserves face wrapping, double sound centers, the Item Random
draw, sound-before-live-Fire loading, the discarded setter result and Stack
wear even for occupied targets. Direct YE, the IZ wrapper and OB continuation
retain their separate nullable reads and statistic effects.

Two fresh unchanged-original VMs agree on 104 declared profiles. Windows and
sanitized Linux each match 34,159 selected original scalars and seven retained-receipt
controls. Compared state includes complete Chunk planes, callbacks, queues,
World/Math/Item Random and the saved Item Random owner, Stack fields,
Player/caller state and sorted statistic maps. Native diagnostics retain actual
failed children through checked retirement. Provider-null classification comes
from the same getter that reached the failed invocation.

The complete Windows MSVC Release suite passes all 1,266 tests in 529.65 seconds.
All 13 selected Linux Clang tests pass in 116.65 seconds with ASan, UBSan and
leak detection; strict GCC compilation covers six changed C units. Both native
hosts match the same selected original scalars and receipt controls. The
[test record](testing.md#flint-and-steel-use) retains the predecessor control,
obsolete frontier replacement and explicit comparison limits.

A fresh [native frame baseline](performance/client-frame-baseline.md) records
37.37 FPS and a 63.34 ms frame-time p95 in one authored Tiny sky-view interval.
It includes active simulation and is not an optimization or original-game FPS
comparison. Input/focus, extended play/save drift, original audiovisual equality
and sustained frame-time qualification remain unfinished. The
[Ore checkpoint](history/status-checkpoints.md#redstone-ore-checkpoint) retains
its preceding comparisons and broader regression results.

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
