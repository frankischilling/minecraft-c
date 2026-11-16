# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and performance
qualification remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

The C region transport now handles long Windows target and temporary names.
The [storage path record](world/native-storage-paths.md) covers the reproduced
260-unit temporary-open failure, five Unicode path boundaries, replacement,
collision ownership and fresh-cache reopen. Four focused storage tests pass on
MSVC Release and GCC Debug with ASan/UBSan and leak detection. The Source File
provider now shares that transport and passes long Unicode attribute,
enumeration, rename and input-stream controls. A separate 400-unit region-output
scenario exercises actual RAF saves and independent NBT readback. Full-client
save and network-share qualification remain open.

Ordinary World setters now avoid initializing an unused successful acquisition
diagnostic. The [quiet-setter record](performance/quiet-setter-acquisition.md)
retains 64 measured processes and all 448 samples: the eight resident AIR no-op
scenes show 4.635% to 10.086% lower scene-median elapsed time, with three paired
regressions retained. Ten fresh setter controls and eight normal mutation,
query and population observations pass. The newly compiled public harness also
passes its two semantic smokes and 15 reader controls in both Python modes.
The complete strict MSVC Release build and all 1,270 Windows tests pass in
525.01 seconds, with no skips and all 6,238 inputs unchanged during validation.
Three focused Linux tests pass with ASan, UBSan and leak detection in 19.16
seconds; the changed C unit also passes strict GCC13 compilation. Formatting
passes all 2,410 C files and fragments. Changed-cell throughput,
natural-population speedup and gameplay FPS remain unqualified by this result.

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

At the preceding cache checkpoint, the complete Windows MSVC Release build
and all 1,268 tests passed in 460.51
seconds, with no skips or changes to the 6,232 frozen inputs. Strict GCC checks
cover four C units; the normal and optimized public runner controls also pass
on both native Windows and Linux.

Balanced fixed-work measurements reduce the 256-resident lookup loop from
about 67 to 68 down to 26 to 27 ns per call; the eight-resident results are mixed. Four
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
