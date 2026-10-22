# Test scope and reproduction

Fresh builds pass all 169 MSVC and all 171 tests on each GCC/Clang build, with zero skips.
Java standard-library numerical comparisons are enabled;
original-JAR comparison options remain off. Linux enables ASan, UBSan and leak
checks. The same per-test deadlines and output policies remain in force.

The state-shape contract exercises all declared virtual bodies through actual
World/Chunk/Registry owners. Its cases include setter tuples, all six directions,
object identity versus numeric ID, late progress, partial stores, raw NaN bits,
lazy invalid-Tile cleanup and hold/pool preservation.

| Case | Checks per host |
| --- | ---: |
| `entity.bounds` | 4,751 |
| `world.piston_tick` | 575 |
| `world.piston_finalization` | 460 |
| `world.piston_tiles` | 296 |
| `world.piston_geometry` | 386 |
| `world.mutations` | 218 |
| `world.collision_boxes` | 732 |
| `entity.movement` | 544 |
| `world.movement_dependencies` | 202 |

Reports retain complete configured/executed CTest names, zero-skip JUnit results,
command arguments, actual child PIDs/exits, full stdout/stderr and before/after
source maps. Controller mirrors and lifecycle interval checks also pass. The
Windows run uses the working tree. Linux uses a fresh native-filesystem source
copy with all 940 public file bytes equal to the working tree before and after
execution. These are public regression results, not complete original acceptance.

Earlier Linux runs using `/mnt/c` sources each hit the existing 180-second
deadlines in `tools.world_storage_reference`, `tools.chunk_storage_reference`,
`tools.level_reference` and `tools.callbacks_reference`. The exact copied-source storage guard completes
all 76 cases in 15.9 seconds. Fresh Linux source/build trees pass without changing
test limits. Failed mounted-source attempts and their complete streams remain
retained; they are not relabeled as successful runs.

The separate original tick family qualifies 114 inputs and 228 snapshots per
side. Intact-record MSVC diagnostics match all inputs, binary action outcomes
and 456 selected current/previous-progress bit comparisons per side. The full
native observer protocol remains invalid because genuine production messages
split records and payload keys collide with header keys. Raw streams remain
unchanged. A checked record-buffer emitter is reviewed but unexecuted.
[Original observations](reference/piston.md#active-tick-input-family) and these
finite diagnostics do not establish whole-graph equality or state-shape parity.

The following movement checkpoint predates tick and standalone-factory integration.

The [movement contracts](entity/movement.md) pass five focused cases on native
Windows MSVC and sanitized GCC/Clang:

| Case | Checks per host | Scope |
| --- | ---: | --- |
| `entity.movement` | 544 | Actual Item/Falling owners, axes/step, effects, contacts and failure prefixes |
| `world.movement_dependencies` | 202 | Actual area, rain, burning and Entity sound dependencies |
| `world.nearby_entities` | 606 | Existing nearby ownership regressions |
| `world.collision_boxes` | 732 | Existing mutable collision and shape regressions |
| `world.queries` | 6,114 | Existing shared acquiring-query regressions |

GCC's final World test passes a fresh recheck after explicit output-structure
initialization; production bytes are unchanged. Independent audits join retained
source snapshots, compiled products and all five captures per host, plus the
single-case recheck. All 476 C files and headers pass the requested format.
Full local suites were not run because retained builds exceeded available disk
space. Hosted CI runs the configured 167 MSVC and 169 per Linux compiler cases;
that full-suite scope is distinct from these focused local results.

The [movement source map](reference/entity-movement.md) records native reproduction
commands and a separate original family: all 59,654 client records and only
24,109 intact, non-prefix server records qualify after disk exhaustion. Failed
receipts and missing historical before-pin objects remain explicit.

The [retained-original native replay](reference/entity-movement.md#retained-original-native-replay)
passes all 83,763 selected rows on each of MSVC and sanitized GCC/Clang: 59,654
client and 24,109 intact server rows, including 10,874/4,741 exact failure
prefixes. Each server process also executes 11,639 unobserved state-only inputs
to preserve pool history. New captures retain every raw byte through gzip,
with raw/encoded hashes, decompression/CRC and natural-zero ownership closure.
Probe objects relink unchanged qualified core archives; a subsequent Python-only
fix has a separate unchanged-binary input binding. These results add no full
current local-suite run, complete original server/normal movement or whole-game
acceptance.

The following Piston checkpoint predates Entity movement integration.

The [Piston contracts](world/tiles/piston.md) pass on native Windows MSVC and
sanitized GCC/Clang. All 165/167/167 configured cases pass in focused and
complementary partitions, without skips and with numerical Java checks.
The new cases pass 295 Direction, 296 Tile and 144 geometry checks per host.
They exercise real class owners, ordered NBT, GAME save/readback, all six faces,
installed extending/retracting Tiles, late failures and live pooled aliases.

The [interpolation replay](../tests/tools/world/tiles/compare_piston_interpolation.py)
compares 18,292 exact raw records per side/compiler. The separate
[geometry replay](../tests/tools/world/collision/compare_piston_geometry.py)
compares 10,986 exact successful payloads and 15 native failure prefixes per
side/compiler. Three reflection receiver-admission failures and 22 null-World
dependency failures remain explicit exclusions. Both tools consume the retained
completed original captures without launching another original JVM. Raw streams,
prior failures and the original geometry producer's final bookkeeping defect
remain retained. All replay children exit naturally and finish ownership cleanup.

All 464 C files and headers pass formatting. The [source map](reference/piston.md)
keeps original helper projections separate from constructed nonnull-World tests.
Active piston mechanics, complete original World traversal, Entity movement and
whole-game behavior remain unfinished.

The following collision checkpoint predates Piston integration.

The [collision contracts](world/collision/boxes.md) pass 732 checks per
compiler. All 162 MSVC and 164 GCC/Clang configured cases pass without skips,
with numerical Java checks enabled. Linux uses ASan/UBSan/leak checks and
strict floating-point compilation. Tests exercise real GAME owners, all
concrete getter families, multipart ordering, metadata and numeric edges,
below-floor fences, mutable views, detached pooled boxes, permanent-owner
release, late growth/retain failure and retirement of live results with a
foreign source K projection. Eight prepared region records remain on disk;
their slots and timestamps are checked, without claiming full persistence.

The public [getter comparison tool](../tests/tools/world/collision/compare_block_getters.py)
runs a fresh 2,200-row original capture, then compares the 623 successful rows
from its first 672-case getter prefix. Both sides and all three compiled probes
match all 19 record payload fields. Original null World is unused on selected
routes; native real GAME Worlds provide pool ownership. The 49 null-World
failures and remaining 1,528 corpus rows are retained exclusions. Only strict
LF/CRLF framing differs between hosts; coordinates, identities and counters
receive no normalization. Native nearby probes are freshly compared too.
That getter comparison excludes positive Piston geometry, original
nonnull-World metadata/traversal, complete contributions, movement and whole-game
behavior. The separate Piston module records its later verification scope.

The following nearby checkpoint predates collision integration.

The final LF-only correction to one test source was rebuilt and rerun on
all three compilers. Complete regression evidence remains in the preceding
full builds, whose runtime and probe source bytes are unchanged.

The [nearby contracts](world/collision/entities.md) pass 606 checks per compiler.
Fresh MSVC/GCC/Clang builds pass all 161/163/163 configured cases in focused and
complementary partitions, with numerical Java checks and no skips. Linux runs
use ASan, UBSan, leak checks and strict floating-point flags. Each host retains
754 native inputs, 254 dependency files, 225 selected products and 41 captures.

Tests cover actual GAME provider/cache acquisition, packed-key repeated Chunk
identity, slice and candidate order, duplicate holds, exclusion, mutable returned
views, capacity replacement, final Entity release during source clear, native
budget/retain failures and terminal ownership. Private nullable/hold-overflow
controls are labeled native invariants, rather than original gameplay evidence.

The public [comparison tool](../tests/tools/world/collision/compare_nearby.py)
compiles its adjacent reflection helper and runs untouched client/server JARs.
Each fresh original capture has 364 rows per side. The native replay selects
356: 353 successful rows compare exactly, while three nullable cases compare
the reached native failure PC and unchanged/partial destination prefix. Eight
malformed Java array/List cases are retained and explicitly excluded. Native
Registry bootstrap output is checked separately and all raw streams remain
retained. Independent qualification regenerates every input row and joins
process, tool, original JAR, source and compiled-product identities.

These experiments exercise actual Chunk gather projections. Original reflection
sets slice contents and bounds on real null-World constructed objects; native
setup uses real store-owned Chunks, construction/insertion and actual permanent
bounds. They do not compare World construction, provider traversal, complete
collision shapes, movement or whole-game state. The [source map](reference/nearby-entities.md)
records the method boundaries and reproduction command.

The following bounds checkpoint predates nearby integration.

The [bounds contracts](entity/bounds.md) pass 4,601 native checks per compiler.
Fresh final-source MSVC/GCC/Clang builds pass all 160/162/162 configured cases
in bounds-first and complementary partitions, without skips and with numerical
Java comparisons enabled. Linux uses ASan/UBSan/leak checks and strict FP.
Each graph verifies 749 source inputs, 254 dependency pins, 220 products and
40 captures, complete case rows and selected strict compile commands.

The public [original comparison tool](../tests/tools/entity/bounds/compare_bounds.py)
invokes the genuine pinned JAR bodies and the exact final native probes. All
17,388 client and 16,146 server rows match exactly on all three compilers.
An independent audit regenerates the complete corpus and checks every raw row,
command/stream outcome, selected tool and compiled-product join. No NaN
canonicalization or tolerance hides a mismatch. Earlier raw payload failures
remain unchanged; a narrow compatibility operation fixes two-NaN addition.
These are numerical/pool scenarios, without Entity movement, World collision,
ray/Vec3/HitResult/String or full-game qualification.

The requested clang-format settings use attached braces and separate definition
blocks. Applying them changes 308 files among 443 scanned C sources/headers;
the final check passes all 443. All earlier evidence remains retained.

The following snow checkpoint predates bounds integration.

The [snow contracts](world/snow.md) pass all 159 configured MSVC cases and 161
per sanitized GCC/Clang build, without skips and with numerical Java comparisons
enabled. Each fresh build runs the Snow case first, then the complementary
158/160/160 cases using its unchanged executables. Snow passes 380 checks per
host using both Registry sides, four GAME Worlds and sixteen serialized input
Chunks. It checks virtual opacity before a fresh material query, zero-quantity
RNG/ownership behavior, recursive removal and partial effects, light eleven
versus twelve, remote melting and integer boundaries. The stronger generated-
terrain regression passes 98,871 checks, including whole-controller completion,
actual snow and the final flag reset.

Fresh qualification checks all 742 selected source inputs, 254 dependency pins,
215 retained products and 40 captures per host, complete case rows and the
actual Snow process/stream/executable joins. Linux capture collection and a
separate fresh inventory/hash audit run on the native filesystem. Failed earlier
compile/test attempts and their evidence remain retained. All 438 C source/
header files pass clang-format18. These are native constructed scenarios;
saved Snow regions are fixture inputs, and no fresh original snow execution,
original/native replay or arbitrary recursive resource qualification follows.

The following falling checkpoint predates snow integration.

The [falling contracts](world/falling.md) pass all 158 configured MSVC cases
across two sequential partitions and nineteen affected cases on each Linux
GCC/Clang build with ASan/UBSan/leak checks. `entity.falling` passes 230 checks
and `world.falling` passes 300 per host, covering real owners, source ordering,
notifying descent, constructor/publication branches, queue failures and subtype
NBT/provenance. Ordinary gravel geometry passes 6,114 checks per host. Generated
terrain reaches the actual snow-neighbor failure at controller PC2886.

The Windows retry reused unchanged compiled executables after an earlier full
attempt hit disk exhaustion. It ran the six failed tool cases first, then the
complementary 152, retaining both complete streams. Earlier failures and
incomplete stdout after case132 remain historical evidence. The disk-full
period also left empty files in the first GCC captures. A fresh GCC verification
reused unchanged compiled outputs, flushed the filesystem and checked two
complete capture hash passes; the damaged run remains preserved. Each native run
binds the same 739 selected inputs; Windows verifies all 211 retained compiled
products, while each Linux build retains 85 selected products. Linux local runs
cover the affected inventory rather than all 160 configured cases. Formatting
passes all 436 C source/header files. No original falling replay, later Entity
movement/tick or full natural population qualification follows from these tests.

The following controller checkpoint predates falling integration.

The [normal controller checks](world/population/normal.md) pass all 156 configured
MSVC cases and 158 on each sanitized GCC/Clang build, with zero skips and local
JDK comparisons enabled. The new prepared cache scenarios pass 2,219 checks per
host, plus 408 shared falling-flag checks. Both real class sides execute the full
schedule over four saved all-air chunks; the retained driver independently
inspects all eight final Chunk slots. The activation regression reaches the real
gravel-hook frontier and preserves earlier dirty writes without retry.
Full original continuation, all selectors/reentry, falling/snow descendants,
natural terrain and complete persistence remain unqualified. Formatting covers
432 C files/headers.

The following temperature checkpoint predates controller integration.

The [staged temperature checks](world/temperature.md) pass all 154 configured
MSVC cases and 156 per Linux GCC/Clang build, with zero skips and local JDK
comparisons enabled. Linux uses ASan/UBSan/leak checks and fresh byte-verified
native filesystem source/dependency copies. Each host retains 726 unchanged
selected inputs, 254 zlib pins, 194 products and 37 captures. The new
`world.temperature_runtime` case passes 371 checks per host over genuine World,
Dimension, array and allocator owners. Four retained original rectangular
value/auxiliary vectors supply successful numerical expectations; native quota,
raw-write sequencing and FP controls retain their separate scope. No fresh
original experiment or full population/controller qualification follows.

The following live-save checkpoint predates temperature integration.

The [live save integration](world/saving.md) passes all 144 configured MSVC and
146 GCC/Clang cases with zero skips and local JDK comparisons enabled. Linux
uses ASan/UBSan and leak checks with fresh byte-verified source/dependency copies.
Each host retains 678 unchanged selected inputs, 254 zlib pins, 163 products and
32 captures. Formatting passes all 388 C files/headers with clang-format18.
The new `world.saving` case passes 1,003 checks per host, including actual saved
arrays/records, provenance, session and late errors, retry freshness and explicit
native coordinator reload on the same World. It does not establish natural
dedicated startup, Entity persistence or original/native whole-save replay.

The preceding [live Tile contracts](world/tiles.md) add real Chest/Spawner owners, actual
NBT imports before normalization, Map/List holds, item drops, detailed acquisition
failures and joint native retirement. All 143 configured cases pass on Windows
MSVC and all 145 on each Ubuntu GCC/Clang build, with zero skips and local JDK
comparisons enabled. Linux uses ASan/UBSan and leak checks. Each host passes
468 Tile checks and 98,868 provider-activation checks, retaining 672 unchanged
selected source inputs, 254 external zlib pins, 161 products and 31 captures.
Linux source/dependency copies match before and after execution. Clang-format
18 checks all 383 C sources and headers. These native contracts do not complete
live Entity/Tile saving or original/native feature replay.

The preceding captured Registry-array and integrated tree/geology checkpoint passes all 142 configured public cases
on native Windows MSVC and all 144 on each sanitized GCC/Clang build, with zero
skips and local JDK comparisons enabled. Each host retains 661 unchanged selected
source inputs, 254 unchanged external zlib pins, 143 products and 30 new capture
directories. Linux runs use fresh byte-verified native source/dependency copies.
This configured public inventory remains smaller than complete local-original
and population acceptance.

The same runs pass 6,110 combined World query, predicate and feature checks,
including captured `Block.p` reference/element controls. Real resource callbacks
exercise cold active use, nested BUSY admission, captured-array reads while
initialization is entered, and retained first-cause retries after failure.
The Registry-only target performs 4,543 runtime literal/array checks. These
checks do not complete live Tile ownership, factories, Map/List joins or saving.

At the preceding tree/geology checkpoint,
the [tree](world/population/trees.md), [ore/clay](world/population/ore-clay.md)
and [lake/spring](world/population/lakes-springs.md) bodies are integrated into
`world.queries` before provider-capacity exhaustion. Focused MSVC and sanitized
GCC/Clang runs pass 6,062 combined query, predicate and feature checks, plus
370 membership and 984 ordered-collection checks. Each host retains 661 selected
source inputs and 254 external zlib source pins unchanged before/after. The
focused runs retain 10/21/21 products and one complete capture per host.

These scenarios inspect actual private tree Random/node state, complete lake
arrays, recursive spring ticks, full E/F inventories and source flag/Random
prefixes. The shared block getter now rejects a null current provider even on
cached hits; out-of-height air still precedes that receiver access. Two real
spring-presence publications remain in cache history. The explicit fixture
capacity accommodates them, then later trials exhaust that actual owner and
still verify the exact region inventory and empty capacity headers.

The focused result does not qualify the complete configured suite, original
feature equivalence, the host numeric backend or complete population. Exact
lake geometry, high lava-rim draw counts, unsupported gravel hooks and other
descendants remain explicit limits. See the [tree source map](reference/population-trees.md)
and [geology source map](reference/population-geology.md).

Separate original tree experiments complete ten canonical whole calls: five
forms on each original client/base-server side, seed0, actual height64 and true
source returns. Thirteen direct children exit naturally with zero status; 27
fresh compiled helper classes remain unchanged. All retained array-image bytes,
raw streams, ten JVM class-load/init logs and owned world-file inventories pass
an offline binding audit. These are prepared feature-API observations, not a
helper-free control or native replay. Their scheduler view captures E/F field
identities rather than full members and shared sequence; wider queue comparison
remains pending.

Separate [original geology API experiments](reference/population-geology.md)
retain eight canonical true returns across ore, clay, lake and spring on both
sides. Seven complete calls use one capture epoch; server spring uses a
separate capture-only encoder-limit successor. Their class/stream/array-image
and world-file bindings pass an offline audit. Earlier failed captures remain
retained. These observations do not supply native replay, helper-free controls,
dedicated construction or complete population qualification.

At the preceding plant checkpoint,
the [World-query, predicate and plant-feature contracts](world/population/plants.md)
pass 2,870 checks per host. All 142 configured public cases pass on native MSVC;
all 144 pass on each sanitized GCC/Clang build, including local JDK comparisons.
Run
`ctest --test-dir build -R "^(world.queries|world.entity_publication)$" --output-on-failure`.
Each host retains 651 unchanged selected inputs, 254 unchanged external zlib
source files, 140 products and 30 new capture directories. Linux uses fresh
source/dependency copies whose bytes match the working inputs before and after
execution. Selected executable pins include the compiler, build tools, Python,
Java and javac; they do not cover every backend, header or runtime library.
Tests require six byte-equal region baselines, eight occupied input records,
the exact eight-file inventory and two exact empty provider-capacity headers.
Plant cases include later-layer placement after earlier survival rejection,
ordinary false setters, preserved partial effects and a primed Gaussian cache.
Separate [original queries](reference/world-queries.md)
each record 44 calls and 170 raw Contexts; they supply no native replay or full
population qualification. A separate offline audit completes both positive
side checks and twenty semantic corruption refusals with 45 unchanged inputs.
The complete configured public suite does not cover the complete declared
local-original inventory or establish original/native feature equivalence.

At the preceding checkpoint, the
[Entity publication contracts](world/entities.md#validation-and-limits) passed
within all 66 selected cases on MSVC and sanitized GCC/Clang. Run
`ctest --test-dir build -R "^(world.entity_publication|world.fluids)$" --output-on-failure`.
Publication performs 730 checks per host. Fluid performs 529/528/528 checks in
that host order; actual World Random branches select additional assertions.
Each host retains the same 640 inputs and 57 products, with selected tool
executable pins before/after. Eight publication region files remain byte-equal
to actual saved baselines after refused writes and terminal teardown. The
sixteen-record fluid maintenance round trip precedes its real drop publication.
Separate [original publication observations](reference/entity-publication.md)
complete 21 calls, 44 snapshots and 88 Context records per side, with 12 framing
refusals and full payload/identity joins. They do not supply native replay,
dedicated/player coverage or complete population/save qualification.

At the preceding checkpoint, the
[fluid/support/literal contracts](world/fluids.md#reference-mapping-and-validation-limits)
pass within all 65 selected cases on MSVC and sanitized GCC/Clang. Run the new
cases with `ctest --test-dir build -R "^(world.fluids|world.solid_support|block.registry_runtime_literals)$" --output-on-failure`.
Fluid check counts are 487/487/486 in that host order; actual World Random
branches select additional assertions. Support passes 4,359 and literals 4,409.
Each host retains 634 selected inputs, 56 products and new capture deltas.
Fluid maintenance saves all sixteen records with actual GAME clocks and
independent disk reload; support verifies two actual saves and two whole-file
unchanged limit records. [Original support/presentation observations](reference/world-solid-support.md)
each complete 56 calls, with 132 Context descriptors and eight framing refusals.
They have no native replay or full fluid/population qualification. This is a
selected local run, not the complete local-original inventory.

At the preceding checkpoint, the [World scheduling contracts](world/scheduling.md#native-validation) pass 424
checks within all 60 selected cases on MSVC/GCC/Clang, with Linux ASan/UBSan and
leak detection. Reproduce with `ctest --test-dir build -R "^world.scheduling$"
--output-on-failure`. Real constructed client Worlds cover shared sequence state,
full keys, immediate dispatch, separate queue failures and ownership. All twelve
loaded known-empty records finish actual GAME save/unload maintenance with
timestamps inside the child execution interval. Each host retains 624 selected
inputs and 51 products. Separate original client/base-server experiments each
complete 18 calls and retain 39 full contexts; all 18 framing controls reject per
side. The optional present-stone call was skipped on both sides. These original
captures have no native replay or complete gameplay/persistence qualification.

At the preceding checkpoint, the [World mutation contracts](world/mutations.md#native-validation) pass 218
checks within all 52 selected cases on MSVC/GCC/Clang, with Linux ASan/UBSan
and leak detection. Reproduce the new case with `ctest --test-dir build -R
"^world.mutations$" --output-on-failure`. It creates fresh owned worlds and
retains source, outputs and save files through the normal runtime driver.
Prepared arrays, a restored actual q element and nonowning null-array guards
are explicit controls; they do not establish whole population or new
original/native mutation parity. All 618 selected inputs and 44 products per
compiler are retained. Formatting passes 335 C/header files.

The [generated Chunk contracts](world/chunk-objects.md) pass 33,458 checks within
all 38 selected cases on MSVC/GCC/Clang, with Linux ASan/UBSan and leak detection.
They use actual constructed Worlds and store-owned Chunks. Tests cover stable
child/array identities, unchanged block transfer, real reserved-slot and byte
limits, reached registry failure, no-sky and queue partial effects, retired-World
guards, and live generated-to-loaded persistence. Negative q elements and
synthetic nonowning array guards are explicit native controls. All 596 measured
inputs match before/after and retained host copies, with 35 products per host;
formatting passes 313 C/header files. This is native integration and existing
fixture replay, without a new executed-original Chunk comparison or complete
GAME generation, propagation, population or save interoperability acceptance.

The [concrete World height and lighting contracts](world/runtime-lighting.md)
pass 177 height and 2,000,382 queue checks within 38 selected cases on MSVC,
GCC and Clang. Linux uses ASan/UBSan and leak detection. The queue unit performs
one million-and-first actual append, verifies the complete ordered bootstrap
and overflow transcript, and tests real nonempty pointer-array growth at a
limit that permits retained storage but rejects its allocation peak. Explicit
shared-A scalar controls remain distinct from natural recursion observations.
Height tests preserve disk-only absence, pending history, unsigned bytes,
packed-key sign behavior and asymmetric borders. All 596 code/tool/fixture
inputs match before/after and retained host copies, with 35 products per host.
Formatting passes 313 C/header files. Existing original Biome protocols replay
with synthetic and imported resources. These native client-World contracts
provide no new executed-original World comparison, forced queue stdout-failure
observation, full GAME generation, propagation or population acceptance.

The [World/Dimension manager contracts](world/biome-registry.md#world-and-dimension-managers)
pass 304 normal/World and 386 fixed-manager checks within all 28 selected cases
on MSVC, GCC and Clang. Linux uses ASan/UBSan and leak detection. Tests cover
actual World construction, manager lifetime after World teardown, synchronous
numerical replacement during class initialization, cached publication and
source-ordered fixed-buffer failures. Direct, copy and load/create client
constructors preserve their full registry cause at300/315/398 and stop before
provider setup. All 581 measured inputs match retained host copies, with
28 products per host; formatting passes 298 C/header files. The existing
original graph and imported-resource replays also pass. No new original World,
provider/surface, population or complete persistence comparison follows.

The [Biome registry observations](reference/biome-registry.md) record actual
ordinary initialization on both originals in three JVM modes. Each side
supplies six graph snapshots, 3,824 raw-bit queries and all 4,096 table entries.
Public replay needs no original archives or JVM. All 26 selected cases pass on
MSVC, GCC and Clang, including 101,396 registry, 609 reference-sampling and
33,155 numerical checks. Full graph/query replay passes with synthetic and
imported resources, including ten corruption guards per side. Linux uses
ASan/UBSan and leak detection. All 575 code/tool/fixture inputs match retained
copies, with 26 products per host; formatting passes 292 C/header files.
The larger class owner changes allocation-budget search counts to 11,312,040
array, 103,163 Block and 531,361 Item checks; no assertion was removed.
Full World startup, generation, population and persistence remain unfinished.

The preceding [World registry binding](world/registry.md) passes 22 selected cases on
MSVC, GCC and Clang, with Linux ASan/UBSan and leak detection. This includes
150 new constructed World checks, the existing 4,397 GAME checks, and the
recorded Dimension, registry, recipe/Stack and statistic graph replays.
All 559 code/tool/fixture inputs match before/after and across retained host
copies; 22 products per host are retained. Formatting passes 284 C/header files.
The array unit now reports 11,312,060 checks: the added native borrow counter
changes its allocation-budget search, while every selected assertion passes.
No new original World, fresh-generation or full population/persistence result
follows from these constructed cache baselines.

The preceding [recipe initialization](item/recipes.md) checkpoint passes 30 selected
Windows cases and 32 cases per Linux compiler, with Linux ASan/UBSan and leak
detection. Contracts report 2,872 recipe, 556 registry-Stack, 11,312,084 array,
32,892 achievement and 14,566 statistics checks; each explicit Linux locale
case adds one statistics check. Thirty actual original captures cover both
sides, five entry orders and three JVM modes. All ten numeric graphs, ten
catalogs and both numeric Stack projections match on MSVC/GCC/Clang using
imported and synthetic resources. Thirty-six historical observer replays remain
byte-equal. All 555 code/tool/fixture inputs agree before/after, across retained
host source copies and with that checkpoint's source; 32 build products per host are
retained. clang-format18 checks 280 C/header files. This selected run qualifies
the recorded initialization scenarios; full World startup, gameplay,
population and persistence acceptance remain unfinished.

At the preceding checkpoint, the [achievement addition](statistics/achievements.md) had eighteen successful
original stopped-prefix captures, unchanged eighteen-case legacy replay, and
MSVC/GCC/Clang comparisons of all six published graphs plus actual console bytes.
The three-mode JDK PrintStream capture reproduces through the public recorder.
All 26 selected Windows and 28 cases per Linux compiler pass. Contracts report
7,208 achievement and 7,598,064 array checks; fifteen Windows and seventeen
Linux stdout-handle controls pass. Linux uses ASan/UBSan and leak detection.
All 516 code/tool/fixture inputs match before/after and across retained host
copies. This selected run is not a full bootstrap, population or
gameplay-persistence result.

CTest separates public unit/tool tests from explicitly requested reference
comparisons. Public tests do not require Minecraft files, Java, a
display, assets, or an account. Missing private references are unavailable results.
The numerical and collection Java oracles use only the standard library and can
run in public CI without original game files. Original-class math, storage,
noise, climate and terrain comparisons remain local.

The preceding [statistic-array addition](statistics/registry.md) passes 23 selected
cases on native Windows MSVC and 25 on Linux GCC/Clang with ASan/UBSan and leak
detection. Its units report 14,564 statistic checks and 3,841,912 array checks;
each explicit Linux locale regression reports 14,565 statistic checks.
All six numeric array/map/list graphs match actual original prefixes from 18
JDI captures. The [formatter](reference/string-format.md) matches 275 captured
cases, 109 typed error operands and all 1,114,112 uppercase code-point values.
Its unit has 613 checks. Integer-set allocator failure coverage passes
15,313,212 checks and retains the historical captured JDK histories.
The imported-resource consumers retain every earlier common-stat, Reader,
Properties, Translation, key and Block/Item comparison. All 495 code/tool/fixture
inputs match before/after and across the three hosts. This selected local run
does not replace the separately recorded complete local matrix. Complete
native bootstrap remains unfinished before recipe PC21 or achievement PC624.

The current [Reader](reference/reader.md) and [achievement-key](statistics/keys.md)
checkpoint passed 14 selected cases on MSVC/GCC/Clang with Linux sanitizers.
All 800 recorded reader owners, 7,136 actions and 19,594 projected rows match,
along with all 446 key entries and 453 lookups from both originals. Native
units report 134,207 reader, 4,299 key and 4,504 translation checks. The selected
run also retains the existing Properties and Block/Item comparisons. Check
counts from native allocation-budget searches reflect the current class-owner
storage size; their actual failure/publication assertions remain active.

The [joined Block](block/registry.md) and [Item](item/registry.md) construction
passes six selected cases on MSVC, GCC and Clang: both `registry_construction`
units, both `registry_base_observation` comparisons, `block.material` and
`core.java_random_profile`. Linux uses ASan/UBSan and leak checks. Item has
530,295 checks covering both entry orders, all 32,000 array cells, subtype and
tool identities, five real Random owners, unchanged retries and actual
allocation-budget publication boundaries. Block has 102,659 checks, including
the retained class/PC of a nested failure after another class fails.
The finite original comparisons match all
[33,064 Item rows](reference/item-registry.md) and
[3,950 Block rows](reference/block-registry.md) per side. These are selected
runs; the complete local matrix below remains bound to its recorded source.
Full bootstrap and broader original equivalence remain unfinished.

The [quiet metadata addition](world/blocks.md) passes six selected cases on
MSVC, GCC and Clang, with Linux ASan/UBSan and leak
checks. These cover World construction/GAME, normal provider construction,
chunk acquisition, packed nibbles and stored chunks. The GAME case now has
4,397 native checks, including actual stored and blank identities, bounds,
packed-key aliases, equal writes, retained acquisition failures and an
in-memory typed codec round trip. All 377 code/tool/fixture inputs remain
unchanged across these runs. No fresh complete-suite or new executed-original
claim is attached to this selected addition.

A following MSVC 19.44 compatibility change scopes C4200 suppression to the
standard C17 flexible-array declaration and evaluates material-test conditions
through a local boolean. The material unit, SavedData unit/replay and two World
constructor cases pass on MSVC (0.61s), GCC (0.69s) and Clang (0.70s), with Linux
sanitizers. Counts remain 28,623 material, 196,823 ID, 5,548 prefix and 4,318 GAME
checks, plus 78 original-derived memory rows. All 377 source inputs remain
unchanged across these selected runs; formatting still passes 196 C/header
files. The complete local matrix below remains bound to its recorded source.

The [World construction checkpoint](world/construction.md) at `e227e57` passes all 97 public
cases on native MSVC (103.08s), Linux GCC (243.25s) and Linux
Clang (250.72s). Linux uses ASan/UBSan and leak checks; Windows uses
strict Debug without sanitizers. All 377 code/tool/fixture inputs remain
unchanged across the runs and retained copies. clang-format 18 passes all
196 C/header files.

The five new native cases cover 5,548 prefix, 4,318 GAME World, 348 typed Info,
2,225 provider and 35 environment checks. The sixth new case compares all
48,782 original-derived Dimension rows per side, including 48,739 angles,
and executes 28 corruption guards and four native protocol refusals. This
finite standalone observation does not qualify whole-World startup, complete
population, all-route controls or the complete local-original inventory.

Earlier Linux qualification attempts hit outer deadlines in unchanged fixture
guards. Isolated region-live and callback rechecks passed in 27.83s and 87.99s;
timing localized much of the cost to repeated source-preservation reads on
the mounted workspace. Their CTest budgets are now 90s and 180s. Assertions,
fixtures and inner process deadlines stay unchanged. The accepted complete
matrix runs hosts sequentially, with two CTest workers per host; unsuccessful
attempts and diagnostic timings remain retained separately. These are local
results, without a hosted-CI or full original-World claim.

The preceding [SavedData ID checkpoint](reference/saved-data-ids.md) passes 25 selected
storage/runtime checks on native MSVC (66.32s), Linux GCC (139.64s) and Linux
Clang (139.38s), with Linux ASan/UBSan and leak checks. Its new unit passes
196,823 checks per compiler. The memory replay matches 78 original-derived
rows, rejects fourteen altered outputs and executes four protocol refusals.
All 352 code/tool/fixture inputs remain unchanged across these runs;
clang-format 18 passes all 176 C/header files. The declared public inventory
was then 91; that selected run made no fresh complete-suite or hosted-CI
claim. Original disk captures and native disk contracts have distinct limits.

The preceding [WorldInfo binding checkpoint](reference/world-info-runtime.md)
passes all 89 public cases on native MSVC (126.50s), Linux GCC (274.28s) and
Linux Clang (272.83s). Linux uses ASan/UBSan and leak checks; Windows uses strict
Debug without sanitizers. Its new `world.runtime_info_bindings` case passes
173 checks, initially selected and again in each complete run. All 339
code/tool/fixture files stay unchanged across the runs. clang-format 18 passes
all 172 C/header files. This matrix refreshes the existing Info unit and graph
replay, without adding a new original-game capture or hosted-CI result.

At the Entity construction checkpoint, all 85 public CTest cases passed on
native MSVC (114.62s), WSL GCC (244.09s) and WSL Clang (246.16s). Linux used
ASan/UBSan and leak checks; Windows used strict Debug without sanitizers.
After that matrix, test field macros were adjusted for stable clang-format 18
output, retaining all 63 ordered selectors. The 1,276-check Entity unit was
rebuilt and passed again on all three compilers. No production body changed
between those runs. These runs exclude opt-in original-game comparisons;
the fresh original constructor capture and public scalar replay have their own
[evidence boundary](reference/entity-construction.md).

The earlier standalone WorldInfo checkpoint separately passed `world.info` (8,465 checks,
two ordered serialization clock brackets) and `world.info_observation` (750
stable original-derived rows and fourteen rejected verifier corruptions) on
MSVC/GCC/Clang. Linux uses sanitizers. After a GCC test-initializer warning,
the corrected unit was rebuilt and passed on all three compilers; the guarded
replay then passed separately on each. No production body changed between those
checkpoint runs. [WorldInfo evidence](reference/world-info.md) records source/capture limits.
That declared public inventory was 88, without a fresh complete 88-case claim.

The earlier millisecond-clock increment passed five selected clock/Random cases on the same three compilers,
including 251 new `platform.millis` checks and four ordered live OS brackets per
compiler. The [wall-clock record](reference/wall-clock.md) documents its source
binding and limits. This run does not refresh the complete 85-case checkpoint
above; that increment brought the declared public inventory to 86 cases.

| Check | Coverage | Limits |
| --- | --- | --- |
| `world.saved_data_ids` | 196,823 native checks per compiler: complete signed-Short cycle, copied nullable UTF-16 keys, stable counters, raw load/write, independent owners, stale sessions, caught errors, reached partial files and resource bounds | Constructed contracts; original HashMap disk order, named SavedData, forced allocator/stdio failures and actual World startup remain unfinished |
| `world.saved_data_ids_replay` | 78 original-derived memory rows match on MSVC/GCC/Clang; fourteen altered-output guards and four real malformed-input child refusals pass per compiler | NULL-store history only; excludes String argument identity, original disk interoperability and complete manager behavior |
| `world.runtime_info_bindings` | 173 native checks: actual Info/context cell identity, independent copied scalars, failing callback visibility, retained lifetimes, per-Info wrapped region growth, real no-list metadata files and bounded record packaging | Constructed native contracts; no original World constructor/list save, new original handler interoperability, forced allocator/OS failure or general mutable NBT |
| `world.info` | 8,465 checks on MSVC/GCC/Clang: all fifteen fields, source-ordered failure prefixes, copied independent scalars, shared/nullable references, stale cached dimension, lifetime/budget rules, real-clock serializer and shared Player rename | Native/source contracts; no forced allocator/hold/OS failure, general mutable NBT, live-player-list serialization or World/store binding |
| `world.info_observation` | 750 stable rows from one unmodified original run per side match all three compilers; fourteen graph/order/value/refusal corruptions are rejected per compiler | Only 5/3 returned clock rows excluded; native status maps the recorded null-name exception category; no natural World startup or historical-VM claim |
| `block.material` | 28,623 checks for all 28 material result vectors, 14 palettes, 16 slots, stable/distinct identities, shared-alias mutation and independent native owners on MSVC/GCC/Clang; Linux sanitizers | Receiver and native ownership contracts; no full block registry, world or allocation-failure injection |
| `block.registry_construction` | 102,659 native checks for both 96-Block construction paths, retained base/subtype/static identities, shared-profile effects, lifetimes and six allocation-budget boundaries on MSVC/GCC/Clang; Linux sanitizers | Stops FAILED at unfinished statistics; no completed bootstrap, original subtype equivalence or World integration |
| `block.registry_base_observation` | All 3,950 recorded-original base-field/array/reference rows per side match MSVC/GCC/Clang; Linux sanitizers | Finite explicit Block entry stopped by debugger before Item PC3398; excludes subtype/JDK/clock state, initialization phase, full bootstrap and World integration |
| `item.registry_construction` | 530,295 native checks for both entry orders, all 106 named Items and 32,000 cells, subtype/tool/armor/dye state, conditional fire setup, five actual Random owners, retry snapshots and real allocation-budget prefixes on MSVC/GCC/Clang; Linux sanitizers | Explicit unfinished statistics/resource edges; native resource controls do not reproduce Java OOM, and subtype fields are source-derived unit expectations |
| `item.registry_base_observation` | All 33,064 recorded-original base-field/static/array/String rows per side match MSVC/GCC/Clang; Linux sanitizers | Actual nested Item2664 with outer Block3398; excludes phase, subtype/class metadata, JDK/clock/profile state and completed bootstrap |
| `block.material_observation` | 116 captured original client/server rows match exactly on MSVC/GCC/Clang, alongside the material unit in a separate two-case run; [provenance and reproduction](reference/materials.md) | Public replay of one fresh JVM run per original side; no world/bootstrap or historical-VM claim |
| `item.stack` | 93 native construction/read-prefix, identity, provenance, overlay, budget and independent-document checks; Linux GCC/Clang with sanitizers and Windows MSVC without sanitizers | No original or gameplay/container comparison |
| `entity.construction` | 1,276 native checks per compiler for both sources/overloads, all field defaults, owned children, real profile continuation, stack holds, ID wrap, strict FP and fifteen allocation boundaries per source | Native constructor/lifetime contracts; cold first-draw count, JVM resource equivalence and world insertion remain unqualified |
| `entity.item_formula_observation` | Ten cases replay 34 inferred original inputs against 50 directly observed phase/yaw/motion fields through the actual production scalar seam, plus three CLI refusals | Scalar formula replay only; no injected constructor entropy or whole-field parity; [provenance](reference/entity-construction.md) |
| `world.runtime_profile-and-entity-bindings` | 104 checks per compiler using real stores/profiles/runtimes; nullable binding, typed diagnostics, RAW profile mismatch and READY/failed persistent world holds | Existing loaded-world foundation; no original World constructor order, collection insertion, registry or Tile behavior |
| `world.scheduled_membership` | 329 native full-key collision, duplicate identity, held-alias, time-wrap, growth-budget and output-preservation checks; Linux GCC/Clang with sanitizers and Windows MSVC without sanitizers | Membership layer only; no world scheduler, original comparison or forced allocator failure |
| `world.scheduling` | 424 checks using actual client Worlds, shared sequence owners, full keys, immediate ticks, RAW Info, separate F/E failure effects and twelve actual maintenance saves; MSVC/GCC/Clang with Linux sanitizers | Client native contracts; separate original observations have no native replay; tick draining, nonempty tick descendants and scheduled persistence remain unfinished |
| `world.scheduled_ordered` | 940 native identity, successor-hold, deletion-history, mutable-key, limit and output-preservation checks on GCC/Clang/MSVC | Finite collection contracts; no world drain/reentry or forced allocator/hold failure |
| `world.scheduled_ordered_replay` | All 13,584 captured JDK public-API steps match GCC/Clang/MSVC; immutable expected trace and provenance are checked | Captured Temurin 17 profile with project-owned comparable records; no original game classes, Java 6 or whole-world qualification |
| `tools.scheduled_ordered_protocol` | 101 constructed native protocol cases pass on GCC/Clang/MSVC, covering numeric/ID bounds, line endings, control bytes, the 160-byte line limit and exact failure output | Probe boundary only; no JDK/assets, operation-limit stress or world behavior |
| `core.java_gaussian_replay` | All 14,013 immutable JDK result/full-owner-state events match GCC/Clang with sanitizers and native MSVC; includes seeded Gaussian histories and positive-finite strict log/sqrt | Captured Windows Temurin 17 profile; no constructor entropy, clocks, original game classes or exhaustive input claim |
| `core.java_gaussian` | 85 captured-result/cache contracts pass on GCC/Clang/MSVC, including stale bits after reseed, signed zero, no-draw hits, owner copies and strict-math boundaries | Explicit seeds and admitted FP environment only; no constructor entropy or arbitrary math-input claim |
| `tools.java_gaussian_protocol` | 157 exact native cases pass on GCC/Clang/MSVC: seed/ID/bound limits, malformed math, line endings/control bytes, 160/161-byte boundaries, full cache snapshots and exact rejection prefixes | No operation-limit stress, changed-FP experiment, JDK or original-game execution |
| `platform.nanotime` | 172 Windows / 173 Linux checks pass on MSVC/GCC/Clang: exact conversions, native OS brackets, unchanged outputs/clock storage and controlled unsupported MXCSR states | Native clock contracts; no original nanoTime/QPC conversion or timing-performance proof |
| `platform.millis` | 251 checks pass on MSVC/GCC/Clang: signed epoch/unit conversions, checked ranges, output guards, six FP-control variants and four ordered real OS brackets per compiler | Native wall-clock contracts and pinned upstream source; no forced OS failure, installed-library bridge or World-constructor qualification |
| `platform.nanotime_conversion` | 17,769 checks over 4,866 actual Java scalar vectors pass on all three compilers | Java operator formula and explicit native range policy; no clock sampled by the Java helper |
| `core.java_random_profile` | 191 native ownership, budget, failure-latch, constructor-commit, cold-holder contention, atomic Math and FP checks pass on all three compilers | Native contracts; no original-game startup or cross-language concurrent scheduling proof |
| `core.java_random_constructor_observation` | 18 checks pass on MSVC/GCC/Clang for recorded constructor inputs through the production tail and plain Random continuations | No production clock sample, shared-uniquifier advance or atomic Math-holder replay |
| `world.normal_generator` | 99082 public checks for composed generation, fresh lifecycle flags, borrowed ownership, unpublished identities, mutable materials, repeated results, populated-cache resets, coordinate/resource preflight and allocation-failure preservation | Finite ordinary normal generator; no cache acquisition, population or propagation |
| `world.chunk_acquire` | 3945 constructed checks recorded in development for load/generate/fallback, gates, pending membership, publication, activation, population gates, recursion, local leases and partial failures | Lifecycle additions pass the complete exact-source suites; injected effects remain separate from genuine gameplay reachability |
| `core.java_int_set` / `world.chunk_pending` | 6913747 / 35495 constructed checks for bounded history, tree bins, removal and mediated pending mutations | Modern-JDK history domain; no historical Java6 claim |
| `core.java_int_set_replay` / `world.chunk_pending_replay` | Seven captured JDK histories, 574 operations, 588 complete responses and seven rejection controls per replay | Actual public Integer/HashSet APIs; game unload selection has separate original evidence |
| `world.chunk_lifecycle` | 12371 constructed checks for eligibility, 24/25 cutoff, live/reentrant traversal, detachment, 100/101 unloads, failures and leases | Development count; callback bodies are explicit external contracts |
| `world.runtime_*` / `world.lifecycle_protocol` | Fresh native owned worlds, retirement/capacity recovery, generated retention, session errors and 67705 protocol checks | Constructed values; native fatal storage policy and empty runtime records are explicit restrictions |
| `world.lifecycle_callbacks_replay` | Three original-derived finite cases, 61 callback pairs, full arrays/headers, 39 response mutations and seven native input rejections | Original raw decoder output is imported; Boat/Sign bodies and world deferred cleanup are external |
| `reference.lifecycle_*` (local opt-in) | Ten fresh paired original/control profiles with strict class-file/clock/provenance validation and native comparison; nonempty native activation refusal also preserves copied files | All ten profiles pass in the qualified Windows v10/GCC v9/Clang v9 suites; see [exact-source ledger](reference/lifecycle.md#exact-source-lifecycle-checkpoint) |
| `tools.acquire_reference` | 23 original-protocol and execution guards covering complete events, object ownership, intermediate cache contents, query order, Random states and unavailable/failure classification | Java-free fixed three-context protocol corpus |
| `tools.acquire_fixture` | Five lossless corpus, provenance, compression, MCA1 import and original-only export guards | Retained original expectations; no original execution |
| `tools.acquire_interop` | Three continuous C inputs, 20 operations, five generated chunks and 5120 exact queries; malformed binary rejection | Actual retained-provider composition with fresh-world load misses; no population body or disk decoder |
| `reference.chunk_acquire` (local opt-in) | At 9a50ac6, genuine persistent client and dedicated-owner constructors, retained providers, loader misses, 20 operations, 5120 query pairs, 252 lifecycle events, 40 inventories and three complete untouched controls pass on MSVC/GCC/Clang | Finite seed23 constructor/API corpus; successful-load and natural population callback comparisons remain separate |
| `tools.callbacks_reference` | 24 Java-free execution and original-protocol guards, including setup-only unavailable proofs, full events/state/owners, loaded-time and population boundaries | Retained four-context original evidence; no original execution |
| `tools.callbacks_fixture` | Ten bounded fixture, provenance, preparation, complete MCC1/expected projection and strict JSON guards | Original-only expectations; fingerprint-only public method proofs |
| `tools.callbacks_interop` | Four genuine continuous inputs, 12 operations and 16 external callbacks, plus four constructed cases and 28 malformed/state/owner guards | C computes coordinator transitions; observed external effects within the cache/world boundary are explicitly replayed |
| `reference.chunk_callbacks` (local opt-in) | Genuine successful loads and natural population in persistent base worlds; 394 lifecycle events, 24 inventories, preserved saved baselines and four complete untouched controls | At 28a55b5, all three local toolchains and independent acceptance audit pass; no C decoder or terrain-feature body implementation |
| `storage.region`, `tools.region_inspect`, `tools.region_capture`, `tools.region_live`, `tools.region_probe` | Read-only storage/CLI, historical replay guards, 17 live-protocol guards and real Unicode stdin paths | Local record inspection; no world schema, global addressing or writes |
| `reference.region` (local opt-in) | All 49 queries per side with fresh copies, exact controls, complete metadata and bounded diagnostic differences; exact 092301ae MSVC/GCC/Clang checks and independent audit pass | Eight compatible cases and 41 explicit malformed/diagnostic departures; no original error-parity claim |
| `tools.normal_reference` | Production helper schema, source/provenance, prerequisite and preservation guards | Synthetic checks; no original methods run |
| `tools.normal_interop` | Lossless retained original baselines and complete sequential C generation replay | Fixed four-seed corpus; eight continuous nine-call inputs pass |
| `reference.normal_generation` (local opt-in) | All four seeds and nine calls per side match in three pairings on MSVC/GCC/Clang at a08c120; complete local/world state, 73728 ordered query pairs, 1224 stage events, 144 inventories and eight untouched controls per build | Fresh finite headless base-world contexts; unpublished normal entry within the finite cave-scheduler domain; no dedicated-server cache lifecycle or population |
| `world.chunk_finish` | 3259 constructed checks for packed cache aliases, publication visibility, unsigned heights, original admission, query order, dirty effects and rejected-call preservation | Valid nonnull cache state; no loader, generator or population lifecycle |
| `tools.finish_reference` | Corpus, packed-key, wrapped-query, boundary and fixture omission guards | Synthetic helper checks; no original methods run |
| `tools.finish_interop` | Twelve captured original inputs, 24576 exact query records and 48 complete data/queue stages | Six fixed fresh-world cases with two passes; no propagation or complete cache lifecycle |
| `reference.chunk_finish` (local opt-in) | Actual original entry/exit traces, complete retained local/cache/queue snapshots and untouched controls, independently compared in three pairings | Finite headless marker inventories; normal finishing on the fixed corpus only |
| `world.light_queue` | 72 observed merges, complete live queues across 43 requests per original side, storage/alias/failure contracts and the million-entry abort | C queue storage after admission; no C world admission or propagation |
| `tools.light_reference` | Five parser, ordering, rejected-request preservation and old-JDK prerequisite guards | No original methods run |
| `reference.light_queue` (local opt-in) | Actual original merge methods and world queue overloads, complete ordered queues, unchanged world/cache state and actual million-entry abort | Replays each observed baseline/admission result; full world finishing remains open |
| `core.java` | 146 fixed arithmetic/cast/Random edge and captured-oracle checks | No game simulation or game random-consumption ordering |
| `core.math` | 903 checks: all observed lookup/floor cases, complete-table fingerprint, 37 exact entries, repeated initialization and immutable reads | Float table and cast/compare floor only; sampled scalar inputs, measured native backends |
| `core.mutf8` | 596848 checks, including every UTF-16 unit, accepted byte forms, malformed input and unchanged outputs | Payload codec only; NBT and packet framing are distinct |
| `storage.nbt` | 394978 checks for all original tag types, boundaries, retained state, ownership, budgets, errors and deterministic mutations | Raw tags only; no world schema or compression |
| `storage.compression` | 2351 checks for gzip/zlib round trips, headers/trailers, ownership, byte/allocation/member limits and mutations, including failed large-output growth | Memory containers only; no region/world files |
| `storage.level` | 1055 checks for exact metadata types/defaults, UTF-16 ownership, explicit clocks, opaque fields and bounded failure preservation | Metadata codec; filesystem behavior is separate |
| `storage.region_writer` | 431 runtime checks and two compile-time assertions for owned image allocation/reuse, exact-boundary sector counts, retained bytes, growth and failure atomicity | Memory images only; no native file replacement or clock source |
| `world.stored_chunk` | 67681 checks for stable state/array ownership, fixed schema shape, transient defaults, normalization and opaque edit guards | No entity/tile factories, registration or subtype simulation |
| `world.noise` | 4850 checks: seeded permutation/state, independently traced original scalar/bulk/octave observations, zero sizes/layers and unchanged invalid-call outputs | Perlin/octave primitives only; no complete terrain or simplex biome noise |
| `world.simplex` | 5005 checks: observed constants/state/grids, mixed constructor ordering, original numerical boundaries, independent/default octave factors and unchanged rejected-call state/output | 2D simplex primitives only; no biome climate/classification or complete terrain |
| `world.biome` | 33155 checks: all 4096 observed table cells, 212 float and 79 double cases, climate vectors, buffer growth/suffixes, staged allocation rollback and unchanged rejected-call state | Climate/classification module; no world startup, terrain or complete biome behavior |
| `world.biome_registry` | Actual shared objects, field/list/Class identities, literal selectors, cold/warm and retained failure contracts | Class bootstrap and ordinary helpers; no World binding or gameplay |
| `world.biome_reference_sampling` | Cold, warm, partial channel/class failure, array reuse and whole-call lifetime contracts | Direct square sampler with native bounds; no World generation or surface join |
| `world.biome_runtime` | 304 checks: constructed Worlds, same-registry binding, actual cache and pointer results, callback replacement, retained failures, all three client constructor cause frames and manager lifetime | Native integration and existing climate fixtures; no new original World observation or generation |
| `world.fixed_biome_runtime` | 386 checks: both sides and fixed kinds, cold l/m setup, rectangle/empty/suffix effects, natural allocation/fill failures, aliases and registry retirement | Native/source contracts; no fabricated recursive null-e state or Java OOM/GC timing |
| `world.temperature_runtime` | 371 checks: retained original temperature/auxiliary bits, actual normal/fixed owners, null/empty/reuse/growth, suffixes, manager/root retirement, real late c/output quota effects and FP ordering | Concrete method and native ownership controls; no fresh original OOME, provider w/controller, snow or natural population qualification |
| `world.normal_population` | 2,219 prepared native checks: real complete cache-triggered schedule on both class sides, full all-air Chunk images and eight saved slots, w/ledger identity, cold resource failure, cache hits, leases and aliases after provider retirement | Native controller/ownership scenarios; full original RNG continuation, all selectors/reentry, falling/snow descendants and natural terrain remain unqualified |
| `block.registry_falling` | 408 checks over actual declaring-class initialization, shared flag access, complete first/cached causes and genuine entered-resource BUSY calls on both sides | Shared flag contract; later FallingSand Entity ticks and physics remain unfinished |
| `world.biome_registry_observation` | Complete original graph/query streams, explicit byte/table controls and corruption guards | Finite Biome bootstrap and methods; no Java Throwable identity or full World startup |
| `world.terrain` | 535745 checks: three full original-captured density/base fixtures, constructor state, retained suffixes, interpolation ordering and unchanged rejected-call output | Density and base blocks only; complete terrain requires later passes |
| `world.surface` | 1114102 checks: seven complete paired-original fixtures, direct/reseed contexts, independent Random schedules, sea/material/bedrock/sandstone controls and failure preservation | Normal surface pass only; constructed controls are separate from original fields |
| `world.caves` | 2021832 checks: ten complete paired-original fixtures and 30 captured passes, reseeding, fresh cache initialization, owner isolation, suffix/table preservation and C argument/coordinate controls | Normal cave pass; protected geometry controls have separate evidence |
| `world.caves_geometry` | 2917405 checks: 44 paired-original protected cases with full buffers and Random continuations, raw bits, draw-count inference, suffixes, canaries and independent owners | Private valid room/path domain only; no public scheduler coverage |
| `world.chunk_data` | 101534 checks for fresh/repeated initialization, unchanged rejected outputs, capacity suffixes, packed-coordinate bounds and extreme-opacity preservation | C ownership/argument contracts; original local-state comparisons are separate |
| `world.chunk_nibbles` | 414 checks against 16 observed setters, complete retained packed bytes and two capacity variants | Packed-array access and preservation; no chunk lifecycle |
| `world.chunk_reference_data` | 335 checks across five paired-original fixtures, 20 stages, 100 complete arrays and ten rebuilds | Local voxel state only; full finishing world effects are separate |
| `tools.noise_reference` | 4 generated prerequisite/report/path/symlink/hard-link regressions | No original classes run; filesystem link cases skip only if unsupported |
| `tools.simplex_reference` | 5 generated prerequisite/hash/report/path/link regressions | No original classes run; link cases skip only if unsupported |
| `tools.biome_reference` | 5 generated prerequisite/hash/report/path/link regressions | No original classes run; link cases skip only if unsupported |
| `tools.density_reference` | 6 generated prerequisite/hash/report/path/link and mismatch-before-unavailable regressions | No original classes run; link cases skip only if unsupported |
| `tools.surface_reference` | 6 generated prerequisite/hash/report/path/link and failure-precedence regressions | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.math_reference` | 10 generated prerequisite/hash/report/path/alias/link, parser and failure-precedence regressions | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.caves_reference` | 24 schema/table/report/path/link, live-rotation and failure-precedence guards, including strict final inventories and renamed-script rejection | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.caves_geometry_reference` | 18 schema/pattern/continuation/table/report/path/link and failure-precedence guards, including renamed-script rejection | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.nibbles_reference` | 18 source/protocol/array/path/prerequisite and failure-precedence guards, including aliased temporary directories | Synthetic protocol checks; original methods are not executed |
| `tools.chunk_data_reference` | 14 schema, array, cache/queue growth, preservation, path, source-attribution, compiler-option, completed-measurement and composed live-child/limit guards | Synthetic protocol checks and an owned Python child; original methods are not executed |
| `tools.compression_interop` | 127 exact payload/framing records between C and Python gzip/zlib, including all optional header field combinations | Container interoperability, not original gameplay |
| `tools.level_reference` | 32 guard methods, 80 original-derived codec comparisons and 6 separate C preservation-policy checks | Frozen original observations; fresh handler/native-world acceptance is separate |
| `tools.region_write_reference` / `tools.region_write_interop` | 30 guard methods and four complete original-derived image profiles with 34 write attempts | Raw images and explicit observed clock inputs; no original GC timing or failed-I/O parity |
| `tools.chunk_storage_reference` | 34 guard methods and 268 C records: 196 original-derived comparisons/departures plus 72 preservation/refusal checks | Fixed-shape supported records; opaque retained data is not live entity simulation |
| `tools.regions_reference` / `tools.regions_interop` | Four guard methods and seven native filesystem scenarios, including 16 malformed/bounded protocol controls | Nine global mappings are original-derived; remaining scenarios check constructed C contracts |
| `tools.world_store_interop` | Seven scenario groups using 52 paired-original metadata inputs, 56 paired chunk imports and constructed session/rollover/acquisition controls | Native contract checks; separate copied-world and exact-source acceptance passes at 9ad690d |
| `tools.world_storage_reference` | 74 Java-free constructed disk, full-state, clock, opaque-data and capture-lifecycle guards | Validation of the comparison harness; fresh original execution is separate |
| `tools.import_assets` | 16 generated-fixture tests for copy/manifest, missing/hash/path/link/size/overwrite checks | No original assets in fixtures; canonical sound contents unverified |
| `reference.java_semantics` (opt-in) | 135768 exact C versus Java records, including float bit patterns | Current installed JVM primitives; not original-client/server game-state parity |
| `reference.java_int_set` (opt-in) | Seven standard-library Integer/HashSet histories compared with native collection and pending-set replay | Selected modern JDK behavior; no historical Java6 claim |
| `reference.math` (local opt-in) | All 65536 raw table entries and 366 scalar calls in three independent pairings; table preservation controls | Original math methods only; no world setup, cave geometry or full input-space claim |
| `reference.nbt` (local opt-in) | Per original side: 194 reads, 40 original writes, 12 packet controls, 157 original-to-C and 435 original re-reads | Selected JDK; 4 malformed negative-list departures and 20 order-only differences recorded separately |
| `reference.compression` (local opt-in) | 80 containers, 22 C encodes; per side 181 baseline records, 65 original gzip reads, 8 original writes, 35 original-to-C and 38 C-to-reference transfers | 36 explicit malformed-container departure records; no copied-world compatibility claim |
| `reference.noise` (local opt-in) | Per side 783 requests, 140218 exact double values, 399872 permutation entries and 3132 following Random longs; three separate pair comparisons | Scoped procedural noise; no chunks, population or lighting |
| `reference.simplex` (local opt-in) | Per side 812 requests, 204923 exact double values, 348160 permutation entries and 3248 following Random longs; actual short/extended overload and factor/suffix controls | Scoped 2D numerical methods; consumer-like inputs do not establish biome results |
| `reference.biome` (local opt-in) | Actual catalogue/table, direct float and quantized double selectors, processed temperature and biome grids with stateful buffer sequences | Explicit-seed numerical fixture; no executed world constructors or terrain |
| `reference.density` (local opt-in) | 150 exact snapshots per side, 82 original density/scratch recomputation checks; all constructor, climate, density and base state across three pairings | Normal dimension only; no surface, caves, population or complete chunk parity |
| `reference.surface` (local opt-in) | 290 exact snapshots per side, 105 original state/material/density/suffix controls and 20 apply-field controls | Measured base/surface contexts; no complete chunk/cave/finishing compatibility claim |
| `reference.caves` (local opt-in) | Ten complete cases, 30 calls and 50 snapshots per participant; all state, blocks and Random observations in three pairings | Three pairings pass on MSVC/GCC/Clang at the recorded source; no complete chunk claim |
| `reference.caves_geometry` (local opt-in) | 44 separately labeled room/path controls, full input/output buffers, Random continuations, table and suffix controls; shared-draw consumption inferred by unique continuation matching | Three pairings pass on MSVC/GCC/Clang at the recorded source; valid private geometry domain only |
| `reference.nibbles` (local opt-in) | Original constructors, two complete getter sweeps and 16 setter snapshots compared independently in three pairings | Valid local packed-array access; explicit C invalid-argument behavior has separate tests |
| `reference.chunk_data` (local opt-in) | Five fresh/repeated input pairs, complete local arrays/heights, opacity and retained-object controls | Genuine original cache/queue diagnostics are retained separately; no C world lifecycle or propagation claim |
| `reference.level` (local opt-in) | Actual original info codecs and metadata handlers, exact types, fallback, clocks, rotation and session observations | Pure codec comparison passes at 9ad690d; native world-owner acceptance is separately recorded in storage evidence |
| `reference.region_write` (local opt-in) | Actual original region image writes, full images, allocation/growth and independently bounded timestamp fields | Global filename/cache observations do not prove original global read/write local-slot mapping |
| `reference.chunk_storage` (local opt-in) | Actual original raw chunk codecs, region loaders and serializers in genuine worlds, with complete arrays and typed opaque data | C activation of nonempty entity/tile data remains refused |
| `reference.world_storage` (local opt-in) | Original-created copied worlds through C and C-created containers through both originals, fresh C reloads, both storage dimensions and nonempty Boat/Sign passthrough | Accepted finite copied-world comparison at 9ad690d; no gameplay dimension transition, C entity activation or full save scheduling |
| Format check | `.clang-format` applied to current project C source and headers | Style only |
| Original JAR inspector (local) | SHA-1/SHA-256, bounded archive/class-header/CRC metadata | Not cryptographic signature trust or JVM verification |
| Original server smoke (local) | Fresh-world loopback offline startup/save/stop | No client joins or C server implementation |

Enable both Java checks with `-DMC_ENABLE_JAVA_REFERENCE_TESTS=ON`; this profile
requires JDK17 or newer. If requested and no suitable JDK is discoverable,
configuration fails clearly. If a prerequisite disappears before execution,
the probe reports `UNAVAILABLE`, exit77,
which CTest marks skipped. Inspect skips separately from passing comparisons.
The standalone numerical helper also returns77 for missing tools or its C
executable, and returns1 for execution errors or mismatches. It only normalizes
platform text newlines; numeric records and floating bit patterns must agree exactly.

The separate debugger identity regression uses JDK17+ with `jdk.jdi` and synthetic
target objects. It needs no original archives or assets:

```sh
python tests/tools/cache/check_controller_mirrors.py --work-dir build/controller-mirrors
```

It exercises the lifecycle controller's actual serialization and identity-marker
methods across debugger collection, including objects observed before their
helper labels arrive. Passing requires completed debugger disposal of separate
control mirrors, stable recorded identities, bounded mirror retention and
duplicate-label rejection. Each run retains its reports and protocol trace in a
fresh capture directory. CI runs this command separately from CTest; missing
tools, inconclusive collection and mismatches fail the command.

The lifecycle interval regression needs only Python and no original assets:

```sh
python tests/tools/cache/check_lifecycle_intervals.py
```

It compiles the validator's actual interval checks and verifies their operation,
role and profile bindings. The observed dedicated 101-chunk profile permits a
600-second helper interval, matching its controller budget. Other profiles and
control, preparation and constructor intervals retain their 180-second bound.
The check also rejects events outside their own sampled interval. CI runs it
separately from CTest; full original capture validation remains required.

NBT comparisons require JDK 11 or newer and both pinned supplied JARs. Enable
`MC_ENABLE_NBT_REFERENCE_TESTS` and set `MC_REFERENCE_CLIENT_JAR`,
`MC_REFERENCE_SERVER_JAR` and `MC_REFERENCE_TESTED_COMMIT` to the exact source
commit used to build the C probe. Missing, mismatched or unsupported prerequisites
produce exit 77 and skipped status, not parity success. The manifest records
an omitted commit as unrecorded. Comparison execution errors return 1. Reports
replace old running/unavailable/fail results; output paths containing inputs or
sources are refused. See [the original-class procedure](reference/nbt.md).

Compression comparisons use the same supplied-JAR paths, JDK11 requirement and
commit field. Enable `MC_ENABLE_COMPRESSION_REFERENCE_TESTS` separately. The
[compression procedure](reference/compression.md) records original wrapper
behavior and distinguishes NBT success from full container validation. Public
cross-reader tests use only generated fixtures and Python's standard library.

Noise comparisons use `MC_ENABLE_NOISE_REFERENCE_TESTS`, both pinned JARs and
the same exact-source commit/JDK settings. Requests exercise actual original
sampler methods through a project-owned reflective probe. The helper compares
finite double bits and state without tolerances; missing references return77.
See [noise evidence](reference/noise.md) for method anchors and remaining scope.

Simplex comparisons separately use `MC_ENABLE_SIMPLEX_REFERENCE_TESTS` with the
same supplied-JAR/JDK/commit settings. They invoke actual original grid overloads,
record initialized constants and compare exact state/results without a finite-bit
tolerance. Mixed Perlin/simplex prefixes test subsequent Random consumption.
See [simplex evidence](reference/simplex.md) and its [interface](world/simplex.md).

Climate/biome comparisons use `MC_ENABLE_BIOME_REFERENCE_TESTS` and the same
supplied-JAR/JDK/commit settings. The fixture invokes actual original protected
manager constructors and original seeded octave objects, then untouched valid
sampling methods. It records exact numerical arrays, object labels and buffer
retention/alias state. Class initialization diagnostics are retained separately
from protocol output. Client-only raw temperature is compared separately from
the shared client/server cases. See [biome evidence](reference/biome.md) and
[ownership and limits](world/biome.md).

Normal density/base comparisons use `MC_ENABLE_DENSITY_REFERENCE_TESTS` with
those same supplied-JAR/JDK/commit settings. The helper constructs real untouched
original world contexts in fresh ignored directories, then fresh normal generators.
It refreshes each original manager's 16?16 processed climate before sampling.
Full constructor state, retained spans, exact double bits and base bytes must
agree between C/client, C/server and client/server. The internal original base
pass density is checked against the separately observed density. See
[density evidence](reference/density.md) and the [terrain interface](world/terrain.md).

Normal surface comparisons use `MC_ENABLE_SURFACE_REFERENCE_TESTS` with the
same private inputs and source settings. They distinguish actual constructor
continuation from mutable live Random, compare prepared chunk-reseed contexts,
and check current material attributes, complete fields/blocks and suffixes.
Genuine server constructor setup can itself generate chunks; measured calls
use a fresh generator afterward. See [surface evidence](reference/surface.md)
and [ownership/operations](world/surface.md).

Normal cave comparisons use `MC_ENABLE_CAVES_REFERENCE_TESTS` with both supplied
JARs, a JDK containing `java`, `javac` and `javap`, and the exact-source commit
field. The natural helper prepares genuine climate/base/surface inputs and
invokes the actual public scheduler three times per case. The C probe emits
its complete table bits; the helper measures their digest before comparing
snapshots. Missing setup is unavailable, while differences, incomplete controls
and measured-stage timeouts fail. See [cave evidence](reference/caves.md) and
the [interface contract](world/caves.md).

The same option enables `reference.caves_geometry`, which creates no worlds and
tests protected room/path calls on separately labeled owned buffer patterns.
These cover eligible and retained block bytes, interior/shell water, lava and
addressed-Y boundaries, grass repair, rooms and inferred branching. Complete
buffers and observed Random continuations compare independently in all three
pairings. Public helper tests reject renamed executing scripts before report
creation so recorded source attribution identifies the helper actually run.

Packed-array and chunk comparisons use `MC_ENABLE_NIBBLES_REFERENCE_TESTS`
and `MC_ENABLE_CHUNK_REFERENCE_TESTS` respectively, with both supplied JARs,
JDK11+ and the exact source commit used for the C probes. The chunk corpus
includes air/stone transitions, asymmetric boundary/material controls and
two frozen original post-cave buffers. It records complete world-height/cache
and queued-light diagnostics without draining lighting. Read the additional
input requirements and commands in [chunk reproduction](reference/chunk.md).

Use `ctest --test-dir build -L public --output-on-failure` for public tests.
Use `ctest --test-dir build -L reference --output-on-failure` for enabled Java
checks. For multi-configuration Windows builds add `-C Debug`.
`BUILD_TESTING=OFF` builds the C libraries and pinned zlib without Python or Java discovery.

`MC_ENABLE_STORAGE_REFERENCE_TESTS` enables the four persistence comparisons
above with JDK17+, both pinned supplied JARs and the exact-source commit field.
The public storage checks replay original-only fixtures and constructed C
policies without Java. Read [storage evidence](reference/storage.md) for raw
producer attribution, full-state comparison boundaries and deliberate malformed
input restrictions. Fresh reference worlds and copied-world experiments stay
under owned ignored directories; no personal world is used as writable input.

Storage checkpoint `9ad690d83592f127b9bb39d7233e8eef06fd0029` passes all 71
selected tests without skips on native MSVC and WSL GCC/Clang, with Linux
ASan/UBSan. Independent audits bind source/tools/executables/build inputs,
original captures and supported world-state/control/byte-preservation results.
Hosted checks pass 59 public/numerical tests per compiler and formatting for
111 C/header files. See the [exact checkpoint](reference/storage.md#exact-source-storage-checkpoint)
for timings, all twelve new capture hashes, audit hashes and finite limits.

Full normal finishing uses `MC_ENABLE_FINISH_REFERENCE_TESTS`, JDK17+ with
`jdk.jdi`, both supplied JARs and the exact source commit field. The controller
observes actual world-height entries and matching return values during two
consecutive passes. Four suspended marker inventories check thread roles and
original-loader frames; these are finite headless observations. Each traced
run must match a separate untouched control byte-for-byte across complete
snapshots. See [finishing evidence](reference/finishing.md) for the corpus,
loopback debugger, captured state and remaining cache/propagation work.

Complete unpublished normal generation uses `MC_ENABLE_GENERATION_REFERENCE_TESTS`
and the same JDK17+/`jdk.jdi`, supplied-JAR and exact-source settings. Its C target
is `mc_normal_generation_probe`; the opt-in test is `reference.normal_generation`.
The verified corpus retains one fresh generator through nine ordered calls for
each seed 23, 24, 26 and 51, including repeated, negative, packed-key-alias,
world-border and accepted cave-domain endpoint coordinates. It compares complete
data, queues, climate/scratch, all 82 terrain layers and separate Random
continuations, plus 1024 ordered height queries per call. Each original traced
run needs a byte-equal untouched control. See [generation evidence](reference/generation.md)
and the [generation contract](world/generation.md) for exact capture hashes,
source attribution and observation bounds. The measured server context is base
world `dj`/cache `jb`; actual `MinecraftServer` construction uses `dp`/`he`.
The current results do not cover a running dedicated server's cache lifecycle.

At source `a08c12056a06261ed01d9f66999459f7ce9543b4`, the suite contains 35
public tests, or 36 with the numerical Java oracle. The selected original
finishing, light queue, chunk, nibble and normal generation comparisons bring
that configuration to 41 tests. These are exact-checkpoint counts; later
cache/region additions on the active branch need their own validation.

## Shared Random profile reproduction

The [profile reference](reference/random-profile.md) records the native contracts,
public fixtures, exact capture hashes and commands for both scalar conversion
and instrumented constructor capture. The native targets use committed data
without Java or original game files. All four new targets pass within the
complete 80-case public suite on native MSVC strict Debug and Linux GCC/Clang
with ASan/UBSan and leak checks. Windows runs use no sanitizers. This checkpoint
does not include opt-in original-game comparisons.

Both capture scripts require the pinned Windows Temurin 17.0.17 installation
and fresh output directories. Scalar capture compares three complete JVM-mode
outputs; constructor capture records one instrumented `-Xint` execution whose
clock samples change under debugger pauses. Its full nanoTime returns are not
raw QPC measurements. Game constructor history and production population/world
wiring remain separate work.

## Lifecycle reproduction and qualification

The public tests exercise Integer-set and mediated pending history, live
acquisition/save/detach/unload contracts, native runtime ownership and the
original-only nonempty callback fixture. Constructed failures/reentry and JDK-only
histories remain distinct from genuine game observations. Boat/Sign callback
replay imports observed decoder data and callback effects while C computes
coordinator transitions. Native activation of those nonempty records must refuse
without changing copied files.

Set `MC_ENABLE_LIFECYCLE_REFERENCE_TESTS=ON`, both supplied JAR paths,
`Java_JAVA_EXECUTABLE`, `Java_JAVAC_EXECUTABLE` from the same supported JDK17/21
installation, and `MC_REFERENCE_TESTED_COMMIT`. Build the test targets and run:

```sh
ctest --test-dir <build> -C Debug -R '^reference.lifecycle_' --output-on-failure
```

Ten serial profiles use fresh preparation/observed/control worlds and strict
original class-file, graph, clock, process and file checks before native
comparison. Missing prerequisites return77 and count as unavailable/skipped.
Public `world.lifecycle_callbacks_replay` needs no Java or original archives.
`MC_ENABLE_JAVA_REFERENCE_TESTS=ON` separately enables the JDK-only numerical
and Integer-set checks.

At `46546d8322de52beb6c3a51f30e827c0260d2b30`, complete Windows v10, GCC v9
and Clang v9 suites each passed 92 tests with zero failures/skips, in 1940.77s,
2969.23s and 2961.75s respectively. Independent source/semantic audits, storage
rechecks and all 22 capture roots per platform also pass. The finite qualification
includes Linux ASan/UBSan and unchanged measured sources, tools and executables.
The [lifecycle ledger](reference/lifecycle.md#exact-source-lifecycle-checkpoint)
binds actual CTest rows, manifests, inputs, native build/executable bytes,
sanitizer flags, independent audits and hosted results to the source.

Dedicated `unload101` alone has a 600-second controller/660-second exporter
allowance. Other observed profiles retain 300/360 seconds, with unchanged
180-second control/preparation process limits. Complete evidence and every other
bound remain required. A timeout is failed observation, never native parity.

The controller now retains each serialized JDI object mirror through the session,
with a 32768-mirror bound. The standalone regression above passed both marker and
pre-label observation cases on Windows JDK17 and Linux JDK21, including proved
disposal of 128 separate control mirrors and capacity/duplicate-label checks.
The preceding v7 matrix remains excluded: Windows passed 92 tests; GCC passed 91
and failed 1; Clang passed 90, failed 1 and skipped 1. Its failures are recorded in
the [v7 history](reference/lifecycle.md#excluded-v7-attempt). The v8 matrix also
remains excluded: Windows passed 92 tests; GCC and Clang each passed 91 and failed
1, with zero skips. Their Linux dedicated101 helper epochs exceeded the old
validator cap before C comparison. Retained semantic and native development
diagnostics pass without qualifying those failed captures. See the
[v8 history](reference/lifecycle.md#excluded-v8-attempt). Windows v9 at the current
source is also excluded: 91 passed, 1 skipped and none failed. The original
chunk-data setup queue exceeded its observation bound before measurement. The
[Windows v9 history](reference/lifecycle.md#excluded-windows-v9-attempt) records
that refusal. The fresh complete Windows v10 suite supplies every selected row
and capture independently.

The interval regression above failed both actual checks on the retained Linux
operands before the fix, then passed 556 positive and 2011 negative cases on
native Windows and Linux. Its clock-policy checks do not replace fresh
original/control captures or independent exact-source audits.


## Local environments

Expanded source `28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56
selected tests without skips on native MSVC19.51 and WSL GCC13.3/Clang18.1,
with Linux ASan/UBSan. The suite has 47 public tests, the numerical Java oracle
and eight original comparisons. Independent audit verifies 168 actual passed
rows, complete callback projections/controls, exact callback source objects,
source attribution for all 24 original manifests and the archived logs.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39089309334)
pass 48 tests per compiler and formatting for 89 C/header files. The
[callback evidence](reference/cache-callbacks.md#exact-source-platform-checkpoint)
records manifest/audit hashes, timings and the excluded initial Windows
setup-unavailable attempt before its complete zero-skip retry.

Expanded acquisition source `9a50ac67997082c89dbc43cf4274fba1427c1565` passes
all 52 selected tests without skips on native MSVC19.51 and WSL
GCC13.3/Clang18.1, with Linux ASan/UBSan. Each suite runs 44 public tests, the
numerical Java oracle and seven selected original comparisons. Independent audit
verifies all 53 project sources, 63 stable inputs, nine helper classes, complete
records and all 156 passed test rows across the three builds.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39085300876)
pass 45 tests per compiler and formatting for 88 C/header files. See the
[cache evidence](reference/cache.md#exact-source-platform-checkpoint) for exact
manifest hashes and observation bounds. The separate
[callback expansion](reference/cache-callbacks.md) adds three public tests and
one original comparison, yielding the separately accepted 56-test checkpoint above.

Normal composition source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes
all 41 configured tests without skips on native MSVC19.51 and WSL
GCC13.3/Clang18.1, with ASan/UBSan on both Linux builds. The generator unit
passes 99073 checks; all nine public schema/continuation/stage/preservation
guards and retained-fixture replay pass. Each build compares all four seeds in
C/client, C/server and client/server pairings, retaining eight byte-equal
untouched controls, 72 generations, 73728 queries, 1224 stages and 144 complete
marker inventories. Windows uses Temurin17.0.17 and WSL OpenJDK21.0.12.

Independent audit verifies all 47 project source files against the exact Git
objects, 57 unchanged attributed inputs, eight unchanged helper classes and
every retained record/control/world-file hash per build. Actual 41-test logs,
zero CTest exits and all selected original pass manifests establish the
no-skip result. [Generation evidence](reference/generation.md#exact-source-platform-checkpoint)
records the three manifest hashes and audit fingerprint.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass all 36 public/numerical tests per compiler and formatting for 79 C/header
files. These results apply to a08; later branch changes require separate evidence.

Finishing source `a18615f0d9d90758f0ea95b0218cec97733d2adf` passes all 37
configured tests on native MSVC19.51 and WSL GCC13.3/Clang18.1 without skips.
The Linux builds enable ASan/UBSan. Each suite runs 32 public tests, the numerical
Java oracle and four actual-original comparisons: finishing, queues, chunks and
nibbles. Original finishing uses Temurin17 on Windows and OpenJDK21 on WSL.
All six cases match in three independent pairings; all untouched controls and
marker inventories pass. See [exact finishing results](reference/finishing.md)
for source audits and manifest hashes. Native public CI passes all 33 configured
tests per compiler plus formatting for 75 C/header files on the final branch
source and runtime integration.

Chunk source `67c928db7c0d87e599f16228833b0234fba4bc24` passes all 30 configured
tests on native Windows/MSVC and WSL GCC/Clang without skips: 27 public tests,
the standard Java numerical oracle and two original chunk comparisons. Both
WSL builds enable ASan/UBSan. All six original manifests identify that source,
pass with empty error/unavailable lists and retain matching file fingerprints.
Fresh observations use Temurin17 on both platforms.

All five fresh/repeated chunk cases and complete packed-array observations
match separately between C/client, C/server and client/server. The fixture
audit compares 800 complete arrays to retained original records, including all
three fresh runs. The report audit verifies 660 fingerprints and the tested
source snapshot. See [chunk evidence](reference/chunk.md) for hashes and scope.

Chunk contract, nibble and full-fixture units pass 101534, 414 and 335 checks.
The chunk-data helper has 12 public guards, including renamed-source and
injected-compiler-option regressions. The nibble helper has 18 after adding a
temporary-directory alias regression. Canonicalizing the synthetic fixture
root fixes the Windows short-path mismatch; both successful and unavailable
captures preserve their expected results. All 18 guards pass on native
Python 3.14 and WSL Python 3.12, with Windows short paths also checked directly.
The C and comparison sources are unchanged from the recorded original runs.
Clang-format18 checks 61 C/header files. Public CI runs 28 tests per compiler,
without private JARs or world captures. Integration results belong in
[status](status.md).

Previous normal cave increment:

Normal cave source `a507f59d756acf2d035ac7d4ea2da7171f60ad2f` passes all 33
configured tests on native MSVC and WSL GCC/Clang without skips: 22 public
tests, the standard Java numerical comparison and ten original comparisons.
All 30 original manifests identify that source and pass with empty errors.
Natural ten-case and protected 44-case records match independently between
C/client, C/server and client/server, including full arrays, Random continuation,
measured tables, preserved generator state, classes, inputs and artifacts.
Independent review also verifies the source Git objects and 634 current file
hashes across the six new reports. See [cave evidence](reference/caves.md).

The natural/protected units pass 2021631/2917405 checks and both helper suites
pass 18 guards per toolchain. Isolated native MSVC AddressSanitizer runs also
pass. Clang-format18 checks 50 C/header files; the native testing-disabled build
produces the three libraries without Python or Java discovery. Public CI runs
23 tests per compiler; integration results belong in [status](status.md).

Previous float lookup/floor increment:

Float lookup/floor source `d6665e7f3e1b57c16e9e12c14983abd2ba227441` passes all 27
configured tests on native Windows/MSVC and WSL GCC/Clang without skips:
eighteen public tests, the numerical Java oracle and eight original comparisons.
All 24 original manifests identify this source and pass with empty errors;
every probe hash matches. Fresh math comparisons match all 65536 table entries
and 366 scalar calls independently for C/client, C/server and client/server,
including actual initialized original tables on Temurin17 and OpenJDK21.

Public math tests pass 903 checks and all ten helper guards run without skips.
The alias regression reaches the completed comparison through a directory
alias, covering the fixture-path failure exposed by Windows CI. Runtime code
and fixed numerical data are unchanged; all 27 tests were repeated at this source.
The complete-table fingerprint, 37 selected entries, 132 float pairs and 102
floor cases were independently traced to both original-only captures. Source
Git-blob, executable, class, artifact and input-preservation checks agree.
Clang-format18 checks 41 C/header files; all three libraries build with testing
off and no Python/Java discovery. Public CI runs nineteen tests per compiler
without private files; exact-head and integration results belong in
[status](status.md). These results cover the documented numerical prerequisite.

Previous surface increment:

Normal surface source `f7ac19ebed8a14c499bd233c2dcf3fb0f2502360` passes all 24
configured CTests on native Windows/MSVC and WSL GCC/Clang without skips:
sixteen public tests, the numerical Java oracle and seven original comparisons.
All 21 original manifests identify this source and pass with empty errors;
every current probe hash matches its report. Surface's 290 snapshots match
independently in all three pairings, including complete fields, blocks,
materials, climate, constructor state and live Random continuation. All
equivalence, preservation, suffix and retained-apply controls agree.

Public surface tests pass 1114102 checks and all six helper guards, including
filesystem links and completed-mismatch precedence, run without skips. Seven
fixed fixtures are traced to paired untouched originals; constructed C controls
separately exercise state-machine boundaries. Clang-format18 checks 36 C/header
files. The three libraries build with testing off and no Python/Java discovery.
Public CI runs seventeen tests per compiler without private files; current
results and integration state are recorded in [status](status.md).

Previous density/base increment:

Normal density/base implementation source `1817417e8891e93ed5dbc205c9d1f6d8e6436a8d` passed
all 21 configured tests on native Windows/MSVC and WSL GCC/Clang without skips:
fourteen public checks, the numerical Java oracle and six original comparisons.
All eighteen original manifests identify that source, pass status and empty
errors, with matching executable hashes. Density's 150 snapshots match in all
three pairings, including 341732 binary64 values, 1427456 permutation entries,
600 continuation longs, 4358904 block bytes and 31744 biome identities per side.
All state/equivalence/preservation/suffix controls and 82 original q/scratch
checks agree. Public tests include 535745 terrain checks and six density helper
regressions; filesystem link cases ran without skips. Public CI runs fifteen
checks per compiler without private files; exact-head results are recorded in
[status](status.md). The three runtime libraries also build with testing off.

Previous climate/biome increment:

Climate/biome implementation source `6607b3650410bac2707276dd07769cda85215a6a`
passed all 18 configured tests on native Windows/MSVC and WSL GCC/Clang without
skips: twelve public checks, the numerical Java oracle and five original
storage/noise/climate comparisons. All fifteen original manifests identify that
source and pass with empty errors. Biome checks match 2879 common snapshots in
three pairings and 32 separate client-only snapshots; all table, field-preservation,
repeat, temperature-path and point/cache controls agree. Public helper link
cases ran without skips on all three toolchains. Native public CI runs thirteen
tests without private references. The three libraries also build with testing off.

Previous simplex increment:
Simplex implementation source `ec92cbf3774b4be154d6c2894324b6fe33ebceac` passed
all 15 configured tests on the same native Windows/MSVC and WSL GCC/Clang
toolchains below, without skips: ten public checks, the numerical Java oracle
and four original storage/noise comparisons. All twelve original manifests pin
that source and pass with empty errors. Five simplex helper regressions ran on
each platform without skips. That increment's public CI ran eleven tests without private
references. The three C libraries also build with testing off.

Previous Perlin implementation source `1188da0f126db8d623a45d801638d645df054c76` passed all
12 configured CTests without skips on the same three toolchains below: eight
public checks, the numerical Java comparison and three original NBT/compression/
noise comparisons. All nine original-comparison manifests record that commit,
pass status and empty errors. Public CI runs nine tests without private game
files; see [status](status.md) for CI and integration results at the tested commits.
All four helper regression tests ran on each local platform without skips.
The three runtime libraries also build with testing off and no Python/Java discovery.

Observed on 2026-10-17, Intel Core i9-10900F, x86-64:

| Environment | Toolchain and configuration | Result before integration |
| --- | --- | --- |
| Native Windows 10 Home 10.0.19045 | Visual Studio 18 Community, MSVC19.51.36260.0, CMake4.2.3, Ninja, Debug, `/W4 /WX /fp:strict`, Python3.14.3 | All 33 CTests pass at the cave source, with explicit Temurin17 |
| Ubuntu WSL2, Linux6.6.87.2, Ubuntu24.04 userspace | GCC13.3.0, CMake3.28.3, Ninja, Debug, ASan/UBSan, strict diagnostics, Python3.12.3 | All 33 CTests pass at the cave source with JDK21 |
| Same Linux environment | Clang18.1.3, same diagnostics/sanitizers | All 33 CTests pass at the cave source with JDK21 |

Current exact-source Windows checks explicitly select Temurin17.0.17; Linux
selects Ubuntu OpenJDK21.0.12. The numerical foundation also passed with the
installed Windows Oracle JDK21.0.10. The Temurin17 numerical comparison passes
135768 records with vector SHA-256
`6e267821b006c8219a806e69ac82f8d46bd208c24ebdfcaf02af89d8d6139c82`.
This agreement does not substitute for historical game execution.
Native GitHub runner results and the exact integrated commit are recorded in
[status.md](status.md). WSL is reported separately from native Linux CI.

`.github/workflows/ci.yml` builds/tests with GCC and Clang on Ubuntu24.04 and MSVC
on Windows2022, plus a format job. The workflow fetches no private JARs/assets or
launcher data. It has read-only repository permissions and produces no releases.
The actions use verified official release commit IDs. Checkout credentials are
not persisted. Storage statically links zlib; retain its
[license notice](dependencies.md) with future packages.

## Private reference checks

Follow [binary setup](reference/binaries.md), [resource import](reference/resources.md),
and [launch smoke](reference/launch.md), plus [NBT comparisons](reference/nbt.md).
Compression comparisons are described [separately](reference/compression.md).
Keep complete disassemblies, generated
vectors, logs, imported assets and worlds in ignored directories. Record
`git rev-parse HEAD` alongside local results. A local result without a commit
identifier must not be treated as a result for a later source revision.

Copy reference worlds before writable tests. The smoke helper creates a fresh
directory and refuses existing ones. Retain its generated world as a baseline,
make a separate copy for each later scenario, and restore from the untouched
baseline. Finite copied-world storage round trips now pass at the
[storage checkpoint](reference/storage.md#exact-source-storage-checkpoint).
Complete running-game restart behavior, dimension transitions, inventory/entity/
tile simulation and recovery beyond checked host errors remain unimplemented.

No matched graphics/audio comparison, interactive rewrite play, extended session,
four-way multiplayer matrix, state-drift or performance measurement has occurred.
No performance target or additional platform support follows from these tests.
