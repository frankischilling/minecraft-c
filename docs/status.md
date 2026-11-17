# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and performance
qualification remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

[Loaded Signs](world/signs.md) now reach the ordinary World renderer with actual
TileEntity text, board/pole models, texture lookup, Java text centering and
state-dependent bounds. Client support and removal hooks preserve drop order,
lazy Tile lookup and retained failure ownership. Four ordinary null-World
removal controls also cover a reviewed failure-publication crash.

The original/native direct-draw comparison passes 36 finite cases. Both original
debugger modes supply 140 measurements and 140 draws whose exact UTF-16 argument
lengths/digests, widths and coordinates match native row receipts. Original
active Font buffers remain separate evidence; native active GL-list payloads
and pixel equality have not been compared.

The shared [whole-String lowercase hex projection](../tests/reference/core/string_lower_hex/README.md)
admits formatted Unicode in its qualified domain, including the Sign case that
previously stopped at the ASCII-only dependency. It conservatively refuses
possible Locale-dependent UTF-16 insertion/removal. A genuine monitor-2
SourceFont control passes 681 checks, preserving the reached GL/NIO prefix and
queued glyph through that native refusal and owner retirement. Post-fix GCC
and Clang selections pass seven and eleven tests in 35.11 and 40.38 seconds
with ASan, UBSan and leak detection. The complete MSVC Release build and
formatting of 2,528 files pass. All eight final monitor-2 Sign profiles pass
7,895 checks with ordinary save/reselection and checked closure. Independent
readback covers 48 draws and 40 selected saved Chunk/metadata copies; complete
413-file asset inventories agree across the sweep. Pushed-Stone, Furnace and
dense-Chest regressions pass 961, 9,710 and 971 checks. Placement, editing,
sustained play and gameplay FPS remain open.

At `d572d6dc`, the complete serial MSVC Release sweep passes all 1,380 tests in 1804.48
seconds. All 1,372 preceding names remain plus exactly eight additions, with
complete inventory/log/JUnit and Start/completion joins and no skips or unrun
tests. All 6,601 public files and 689 native products stay unchanged during
the sweep. The preceding local suite used four workers; these elapsed times
are not a performance comparison. That checkpoint's validation annotations,
admission fingerprints and CRLF declarations preserved its tested code/products.

The first hosted Sign run passes formatting and Windows, while GCC and Clang
each pass 1,414 of 1,415 tests. Their sole failure is the new SourceFont control:
it expected a committed color where the measured Mesa profile retains pending
attributes. The corrected control snapshots and byte-checks both states through
retirement. Its Windows control passes 681 checks; production code is unchanged.
New hosted confirmation is pending.

The Tile Set snapshot declaration now uses the existing scoped MSVC C4200
guard for standard C17 flexible arrays. This addresses the hosted VS2022 build
failure while retaining strict diagnostics elsewhere. Local MSVC Debug Tile
Set controls pass; MSVC/GCC/Clang preprocessing preserves all non-diagnostic
C output. Hosted validation at `4a550e57` passes all four jobs: Windows 1,348
tests, GCC and Clang 1,407 tests each, and formatting of 2,515 files. Every
preceding test remains, with exactly the three identity additions and no skips
or unrun tests. The checked merge tree matches that source head.

The [native identity integration](reference/object-identity.md) removes the
multiple-new-Tile compilation blocker. Final monitor-2 pushed-Stone, Furnace
and dense-Chest scenarios pass 964, 9,540 and 1,016 checks through ordinary
save/reselection and checked shutdown. Lazy Class-hash and collision-order
controls add 299 checks. Final affected selections pass six Windows, nine GCC
and twelve Clang tests, with sanitizers and leak detection on Linux.
At `2543f72e`, the full MSVC Release build and all 1,372 Windows tests pass in 539.74
seconds. All 1,369 preceding names remain plus exactly three additions, with
complete inventory/log/JUnit and Start/completion joins and no skips or unrun
tests. All 6,544 public files and 684 native products stay unchanged during
the sweep. Formatting checks 2,515 files. Later documentation, LF normalization
and current admission updates preserve runtime and native C harness bytes.
The [Integer measurements](performance/integer-sets.md)
show faster sparse copies and a remaining dispersed-operation cost; they
establish no gameplay FPS result.

[Piston activation](world/pistons.md) now reaches real Lever notifications,
synchronous block events, push/retract bodies and Piston ticks. Nine native
gameplay cases pass 273 checks. The [moving renderer](client/piston-rendering.md)
matches all 27 finite original direct-draw cases, including active vertex words
and declared retained state. A separate late lighting NULL-buffer control
checks the drawn prefix and native cleanup. The ordinary monitor-2 empty-front
scene passes 951 checks through extension, retraction, save and reselection.
The later pushed-Stone scene passes 948 checks after the
[identity Set join](reference/object-identity.md), including both moving Tiles,
settled extension/retraction and ordinary save/reselection on monitor 2.
Its earlier multiple-new-Tile failure remains in the historical capture.
The approved [native identity startup](adr/0001-native-object-identity-profile.md)
preserves lazy hashing and collection behavior while allowing concrete hashes
and traversal order to differ from a JVM launch.

The original/native activation replay matches six scenarios, 48 phases,
94 ordered callbacks and 315 independently hashed array planes. Piston pitch
and World Random remain separate unsaved-entropy observations. Both raw Map
orders are retained; general Map iteration equivalence is excluded.

The final affected sanitizer selection passes nineteen tests on GCC and
twenty-two on Clang in 85.07 and 90.77 seconds. GCC's local profile has the
client disabled; Clang also exercises CPU block/draw controls. Both enable
ASan, UBSan and leak detection. These are selected results, not full local
Linux sweeps or an FPS measurement. A selective preceding notification object
fails the unchanged new acquisition-detail assertion after the actual producer
and getter controls pass.

The complete Windows Release build and all 1,369 CTests pass in 462.59 seconds.
All 1,352 preceding names remain plus exactly seventeen additions, with unique
inventory/log/JUnit and Start/completion joins and no skips or unrun tests.
All 6,528 public files and 682 executables/DLLs stayed unchanged during the
sweep. Formatting checks 2,503 C/header/fragment files. Current admission
bindings cover 16,062 entries and 6,385 files, with five live bundles and 232
source/helper entries. Validation annotations and admission fingerprints were
updated afterward; runtime and native harness bytes stayed unchanged.

The [MobSpawner tick](world/spawner-tick.md) now completes expired-delay
construction, population limits, real Pig eligibility and publication, both
particle methods and four-attempt continuation. MSVC and sanitized GCC/Clang
each pass 518 Spawner and 277 Living-effect checks. The original packet retains
twenty direct ticks across two ordinary JVM modes, including their differing
entropy and success counts. Four serial monitor-2 SDK processes pass countdown,
two genuine save/reselection runs and post-publication failure cleanup.
Published Pigs remain distinct from the renderer's cached preview. This does
not establish full gameplay parity, sustained play or an FPS result.

At `c1edfca2`, the full Windows Release build and all 1,352 tests pass in 448.92 seconds,
retaining every prior name plus seven additions with no skips or unrun tests.
The complete log/inventory/JUnit joins agree, and all 6,465 public worktree
files stayed unchanged during the sweep. GCC and Clang pass the eight-test
affected selection in 9.11 and 9.07 seconds with sanitizers and leak detection.
These are selected Linux results. Validation annotations and current admission
fingerprints were updated afterward; runtime and native harness bytes stayed
unchanged.

The [MobSpawner preview](client/spawner-rendering.md) now reaches actual LL/AG
and the existing Pig renderer from a loaded Chunk. A Windows monitor-2 capture
passes 1,236 checks and sixteen full World/HUD frames across ordinary save and
reselection. One cached Pig is rebound to the new World. The fixture checks its
absence from the live World Entity list and the selected saved Chunk's empty
Entity list. The first draw compiles six model parts and later
draws reuse them. Multi-addition HashSet order, Sign drawing, full original
frame equality and sustained-play/FPS qualification remain open.

At `af9d031d`, the complete MSVC Release build and all 1,345 configured Windows tests pass in
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
