# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and performance
qualification remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

Source-owned [Chunk cache scopes](performance/provider-cache-scopes.md) now use
bounded positive hints for resident reads during callbacks. Every hint checks
the live key and current Chunk; misses retain linear lookup and outermost full
validation. Descriptor exports and callback scope closure follow the actual
constructed cache owner when the replaceable Source provider field changes.

Sixteen native modes cover map transitions, callback corruption, mutable
exports and provider rebinding. One compiled test object gives thirteen exact
old/new positive controls and three genuine old-owner failures; all sixteen
candidate modes pass. Twelve focused Linux tests pass with ASan, UBSan and leak
detection. The [test record](testing.md#provider-cache-scopes-and-owner-rebinding)
keeps the source and ownership limits explicit.

The complete Windows MSVC Release build and all 1,268 tests pass in 460.51
seconds, with no skips or changes to the 6,232 frozen inputs. Strict GCC checks
cover four C units; the normal and optimized public runner controls also pass
on both native Windows and Linux.

Balanced fixed-work measurements reduce the 256-resident lookup loop from
about 67?68 to 26?27 ns per call; the eight-resident results are mixed. Four
qualified monitor-2 native intervals record FPS medians of 36.85 and 38.00.
Their work totals differ, and the small cohort establishes a performance
opportunity rather than unrestricted smooth play. Acquisition-associated
stalls remain. The [performance record](performance/provider-cache-scopes.md)
retains every interval, the small-case overhead and the measured clock scope.

The [Flint and Steel checkpoint](history/status-checkpoints.md#flint-and-steel-checkpoint)
preserves the preceding original/native comparison and broader local suite.
Its original Item/Fire/Portal construction scope remains separate from delayed
TNT, Portal travel, sustained play and original audiovisual qualification.

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
