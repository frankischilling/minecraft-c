# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and performance
qualification remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

The [MobSpawner preview](client/spawner-rendering.md) now reaches actual LL/AG
and the existing Pig renderer from a loaded Chunk. A Windows monitor-2 capture
passes 1,236 checks and sixteen full World/HUD frames across ordinary save and
reselection. One cached Pig is rebound to the new World. The fixture checks its
absence from the live World Entity list and the selected saved Chunk's empty
Entity list. The first draw compiles six model parts and later
draws reuse them. Multi-addition HashSet order, Sign/Piston drawing, full original
frame equality and sustained-play/FPS qualification remain open.

The complete MSVC Release build and all 1,345 configured Windows tests pass in
472.08 seconds, retaining all 1,341 preceding names with four additions and no
skips or unrun tests. GCC 15.2 passes 29 selected sanitizer tests in 103.05
seconds. Clang 21.1.8 passes 20 selected tests in 53.86 seconds and both corrected
selector/recenter regressions in 1.60 seconds. Both Linux runs enable ASan,
UBSan and leak detection. Their full local builds exhausted the 31 GB WSL
filesystem; generated products were cleaned and only the affected targets
rebuilt. These are selected Linux results, not full local suite passes.

The [opt-in focus trace](testing/native-focus-trace.md) now records current-thread
native focus delivery with checked hook lifetime and a bound compile/link
receipt. Thirty-five instrumented and twenty untraced monitor-2 save/title
cycles pass without reproducing the earlier mismatch; its cause remains
unresolved. The [Fire reader change](testing/fire-reader-cost.md) reduces measured
synthetic-reader elapsed time by about 85% on one Windows host while preserving
the existing tests and validation. It provides no gameplay FPS result.

At `bd1a164a`, the complete Windows Release build and all 1,341 tests pass in 454.53 seconds,
with every configured name/ID, ordinal and Start/completion join audited and
all preceding names retained. Four existing Fire reader/packet tests also pass
under WSL. Final monitor-2 Furnace, Lever and Redstone Torch cycles pass with
unchanged selected inputs/assets and checked window cleanup.


[Furnace storage and smelting](client/furnaces.md) use actual TileEntity,
World, container and screen owners. The [retained lazy addition](world/furnace-lazy-lookup.md)
now completes client orientation with checked Arrow/Ray ownership, preserving
source failures and native cleanup refusals separately. At the preceding
Furnace checkpoint `82a0a89d`, the full Windows Release build and all 1,341
tests pass in 618.75 seconds, with every configured
case and all 1,335 preceding Furnace names accounted for. GCC passes 27 selected
sanitizer tests and Clang passes 28, including the retained server prefix and
shared lookup/Arrow/Ray regressions. The final monitor-2 Furnace,
Chest-placement and Workbench cycles pass with unchanged selected inputs/assets
and completed window cleanup. Earlier hosted results retain their tested revisions.

Copies of the earlier native Furnace save also load through the unchanged
original client in two fresh VMs. The finite readback recovers the real saved
Furnace and serialized Player inventory. Original saves/ticks, concrete Player
construction, unrestricted Worlds and bidirectional round trips remain outside
that boundary.

Existing [Chest access](client/chests.md) now connects right-click activation,
single/double inventory views, container transfers, drawing, close and saved
World reselection. All five authored Windows monitor-2 scenarios pass, including
90 occupied slots, blocked access and native shutdown with the screen open.
Twelve selected Windows tests and five GCC sanitizer tests pass. A separate
111-call original component packet qualifies declared container/screen behavior;
it does not qualify concrete original Worlds or drawing.
[Chest placement](world/chest-placement.md) has separate original predicate/query
observations and native Item/SDK controls. Unrestricted play and sustained frame
performance remain unfinished.

The C region transport now handles long Windows target and temporary names.
The [storage path record](world/native-storage-paths.md) covers the reproduced
260-unit temporary-open failure, five Unicode path boundaries, replacement,
collision ownership and fresh-cache reopen. Four focused storage tests pass on
MSVC Release and GCC Debug with ASan/UBSan and leak detection. The Source File
provider now shares that transport and passes long Unicode attribute,
enumeration, rename and input-stream controls. A separate 400-unit region-output
scenario exercises actual RAF saves and independent NBT readback. The real
Windows client also passes authored save/title and ten reload epochs with a
300-unit game directory on monitor 2, plus short-path controls. The
[Frame focus listener](client/frame-canvas.md#retained-windows-focus-events)
now preserves peer focus when dispatching an old Frame gain; a native
200-cycle regression control passes. A separate native Canvas/SDL keyboard
focus mismatch remains unresolved. These finite cases do not qualify
general long-path play, original save round trips or network shares.

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
