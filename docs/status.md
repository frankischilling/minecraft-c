# Development status

The [Entity movement module](entity/movement.md) implements Item and FallingSand
movement using their permanent bounds, actual pooled copies and mutable collision
Lists. Typed effects, Block contacts and World rain/burning/sound queries preserve
completed effects on failure. Five focused cases pass on MSVC, GCC and Clang,
including 544 movement and 202 World dependency checks. All 475 C files and
headers pass the requested formatting.

Full local suites were not run because retained builds exceeded available disk
space. Hosted CI runs the configured 167 MSVC and 169 GCC/Clang cases separately.
Native/original movement replay remains unfinished. Separate observations qualify
59,654 client records and 24,109 intact server records, with explicit disk and
provenance limits. Other kinds, active ticks, positive pressure/detector helpers,
nonempty sound observers and unowned passenger recursion remain unfinished.

The following Piston checkpoint predates Entity movement integration.

The [Piston Tile and geometry module](world/tiles/piston.md) implements actual
Piston constructors, separate class initialization, ordered NBT and GAME storage
joins. MovingPiston collision holds the actual Tile, invokes the carried Block's
real getter and shifts that same pooled box through independent Direction arrays.

All 165/167/167 MSVC/GCC/Clang configured cases pass without skips. New cases
pass 295 Direction, 296 Piston Tile and 144 geometry checks per host. Native
replay matches 18,292 interpolation records and 10,986 geometry successes plus
15 precise body-failure prefixes per side/compiler. Geometry keeps 25 explicit
exclusions. All 464 C files and headers pass the requested formatting.

Active piston ticking, finalization, Entity pushing, the separate state-shape
setter, general Entity movement and original nonnull-World traversal remain
unfinished. These helper comparisons do not establish whole-game acceptance.

The following collision checkpoint predates Piston integration.

The [collision module](world/collision/boxes.md) collects actual Block shapes
into the World's mutable K List, then runs real nearby collection and the
currently constructible Item/FallingSand inherited null virtuals. Typed
occurrences preserve pooled-box or permanent-Entity ownership, duplicate
identities, growth failures and terminal retirement. Multipart shapes preserve
their reached singleton bounds on failure. Other Entity descendants and
movement remain unfinished.

All 162/164/164 MSVC/GCC/Clang cases pass without skips, including 732 collision
checks per host. Fresh original/native tools compare all 19 fields of 623
successful source-unused-World Block getter records per side and compiler.
Only verified LF/CRLF record terminators may differ; raw streams stay intact.
The complete 2,200-row original corpus remains retained, with 1,577 rows per
side explicitly outside this native replay. Native GAME Worlds supply pool
ownership; this does not prove original nonnull-World metadata or traversal.
Fresh nearby comparisons also pass 353 exact successful rows and three precise
native nullable-failure prefixes per side/compiler. Formatting passes all 455
C source/header files.

The following nearby checkpoint predates collision integration.

The [nearby Entity module](world/collision/entities.md) implements the complete
base World query and normal/Blank Chunk gather bodies. It preserves actual
mutable List identities, duplicates, source ordering and late failure prefixes.
Each occurrence owns a real Entity hold; returned views keep the owner live
while allowing later queries to clear and refill the same List.

All 161/163/163 configured MSVC/GCC/Clang cases pass without skips, including
606 nearby checks per host. Fresh public original comparisons pass 353 exact
successful Chunk rows and three precisely checked nullable-failure prefixes
per side and compiler. Eight malformed Java collection cases remain explicitly
excluded from native replay. Formatting passes all 448 C source/header files.

Original World/provider traversal replay, collision K and concrete Block shapes,
Entity movement/environment, application pool reset and FallingSand physics
remain unfinished. Nearby collection does not close those gameplay routes.

The following bounds checkpoint predates nearby integration.

The [shared bounds module](entity/bounds.md) implements actual pooled box
identities, reset, client clear and complete movement geometry. Native holds
preserve detached boxes and allow aliases to observe reuse. Its focused case
passes 4,601 checks on MSVC and sanitized GCC/Clang. All 160/162/162 configured
cases pass without skips. The exact rebuilt probes match all 17,388 client and
16,146 server original raw-bit rows on each compiler, including the full
quiet/signaling NaN matrix. Earlier payload mismatches remain retained.
The requested attached-brace style and separated definitions are applied;
all 443 C source/header files pass formatting. Entity movement, World collision
collection, ray/vector/text helpers and FallingSand physics remain unfinished.

The following snow checkpoint predates bounds integration.

The [snow-layer subsystem](world/snow.md) implements concrete support,
neighbor survival and saved Block-light melting. Its focused native case
passes 380 checks on MSVC and sanitized GCC/Clang. All 159 configured MSVC cases
and 161 per Linux compiler pass in focused/complementary partitions, without
skips and with numerical Java comparisons enabled. Fresh audits verify 742
source inputs, 254 dependency pins, 215 products and 40 captures per host.
The prepared generated-terrain controller completes through notifying snow
writes and resets its falling flag. Player harvesting, rendering, later
FallingSand physics, original snow replay and whole-game acceptance remain
unfinished.

The following falling checkpoint predates snow integration.

The [falling subsystem](world/falling.md) now implements inherited sand/gravel
Block hooks and ticks, real FallingSand constructors and World publication,
and subtype NBT/factory/provenance. All 158 configured MSVC cases pass across
two sequential partitions using unchanged compiled outputs; nineteen affected
cases pass on each sanitized GCC/Clang build. The new cases pass 230 constructor/
NBT checks and 300 runtime checks per host. Generated terrain now reaches the
later snow-neighbor failure at controller PC2886. An earlier Windows attempt
hit host disk exhaustion; its failures, incomplete stdout and damaged GCC
capture copies remain retained. Fresh GCC verification used unchanged binaries
and stable capture hashes.
Later Entity movement/ticks, original replay and whole-game acceptance remain
unfinished. Linux local verification covers the selected affected inventory.

The following controller checkpoint predates falling integration.

The [normal population controller](world/population/normal.md) now executes its
complete schedule through the real GAME cache adapter. Prepared all-air Worlds
pass 2,219 controller/ownership checks per host; the shared Registry flag adds
408. All 156 configured MSVC cases and 158 on each sanitized GCC/Clang build pass
without skips, with local JDK comparisons. The real generated-terrain activation
case reaches the unfinished gravel addition hook and retains earlier writes.
Full original controller continuation, all selectors/reentry, falling/snow
descendants, natural terrain and complete persistence remain unfinished.

The following save checkpoint predates controller integration.

The [live GAME save path](world/saving.md) now writes actual Chest/Spawner Map
occurrences and physical Stack slots over the same Store-owned Chunk. It
preserves source q/error prefixes, replaces both live NBT lists and updates
the selected Info's actual region-growth accounting. The focused Windows run
passes all eleven rebuilt integration suites, including 1,003 new save checks.
Same-World unload/reload uses an explicit native dedicated-policy coordinator;
both installed GAME constructors still bind BASE policy. The complete local
suites pass all 144 MSVC and 146 GCC/Clang cases, without skips and with local
JDK comparisons enabled. Linux uses ASan/UBSan and leak checks. Each host
retains 678 unchanged selected inputs, 254 zlib pins, 163 products and 32 captures;
clang-format18 checks 388 C files/headers. Nonempty Entity NBT, whole World metadata, natural
dedicated startup, original save replay and complete population remain open.

The following checkpoint predates live Tile saving.

The [World and Chunk Entity publication methods](world/entities.md) now retain
actual typed occurrences, duplicates and cross-World bindings. Base/crop drops
use real publication, and a displaced flower allows fluid flow to continue.
Nonempty or terminal Chunk persistence refuses before writing. Terminal native
disposal releases each occurrence without pretending to save or unload it.
All 66 selected cases pass on MSVC and sanitized GCC/Clang: 730 publication
checks per host and 529/528/528 fluid checks, with 640 inputs and 57 products
retained per host. Separate [original observations](reference/entity-publication.md)
each complete 21 calls, 44 snapshots and 88 Context records with full Chunk bytes,
normal substrates and cross-World calls. They have no native replay. Player and
dedicated descendants, nonempty activation/observers, Entity persistence,
falling, complete population and full original/persistence acceptance remain
unfinished.

At the preceding checkpoint, the
[flowing/still fluid bodies](world/fluids.md) execute updates, conversions,
routes, mixing and selected Block drop construction prefixes. All 65 selected
cases pass on MSVC and sanitized GCC/Clang, retaining the same 634 inputs and
56 products per host. Fluid cases perform 487/487/486 checks; support and literals
add 4,359 and 4,409. The new support/fluid cases retain twenty exact region slots,
real saved clocks or whole-file unchanged limit inputs as appropriate. Separate
originals each complete 56 support/presentation subject calls. Their descriptors
contain array fingerprints and have no native replay. At that checkpoint, Entity insertion,
moving-piston lookup, fire/portal, nonempty observers, falling behavior, complete
population and full original/persistence acceptance remain unfinished.

At the preceding checkpoint, the [World scheduling foundation](world/scheduling.md#world-insertion) constructs
before every gate, shares the lazy sequence across Worlds, retains radius-eight
presence order and separates F/E insertion effects. Immediate calls dispatch
actual empty ticks; unfinished concrete tick bodies fail explicitly. All 60
selected cases pass on MSVC and sanitized GCC/Clang, including 424 new checks,
with 624 selected inputs and 51 products retained per host. Separate original
client/base-server observations each cover 18 direct calls and complete queue
and context snapshots. Their optional present-stone call was skipped on both
sides. Native replay, scheduled processing, fluid/falling closure, full population
and complete original/persistence acceptance remain unfinished.

At the preceding checkpoint, the [synchronous World mutation foundation](world/mutations.md) preserves
the two concrete setter schedules, incremental height/skylight repair, ordered
notifications and reached partial effects. Real log/leaf removal and inherited
empty hooks are implemented; other concrete descendants fail explicitly.
All 52 selected cases pass on MSVC and sanitized GCC/Clang, including 218 new
checks and actual save/unload/reload. All 618 selected inputs and 44 products
per host are retained; formatting passes 335 C/header files. Population,
fluid/falling/tile/entity/presentation closure, nonempty observers and complete
original/persistence acceptance remain unfinished.

At the preceding Chunk checkpoint, the [constructor and full finalizer](world/chunk-objects.md) now
keep one final store owner, the actual World receiver and stable child/array
identities. Finishing reads live q and World state in source order, retains
partial effects, and saves current arrays without replacing the Chunk. All 38
selected cases pass on MSVC/GCC/Clang, including 33,458 new Chunk checks, four
composition/storage routes and existing registry/Biome replays. Linux uses
ASan/UBSan and leak detection. All 596 measured inputs and 35 products per host
are retained; formatting passes 313 C/header files. Complete GAME generation,
propagation, population, synchronous mutations and save interoperability remain
unfinished. No new original Chunk comparison follows from these native tests.

At the preceding manager checkpoint, the [World/Dimension Biome managers](world/biome-registry.md#world-and-dimension-managers)
now retain actual shared references and independent Registry borrows. Normal
and fixed direct/cached/point/chunk calls preserve receiver identity, source
ordering and failure effects. World constructors retain the full Registry
cause at bind300/315/398. All 28 selected cases pass on MSVC, GCC and Clang,
including 304 World-manager and 386 fixed-manager checks, with Linux
ASan/UBSan and leak detection. The 581 measured code/tool/fixture inputs and
28 retained products per host bind these results to their source; formatting
passes 298 C/header files. These are native integration contracts and existing
original-fixture replays, without a new executed-original World comparison.
Live provider/density/surface reads, lighting, population, complete mutations
and persistence remain unfinished.

At the preceding checkpoint, the [shared Biome registry](world/biome-registry.md) constructs actual
objects, spawn lists, Class handles and a reference table on the shared class
owner. The direct sampler writes each climate cell before the real Biome
active use. Both originals returned from ordinary initialization in three JVM
modes; their complete graphs and 3,824 queries per side match the native
probe on MSVC, GCC and Clang. All 26 selected cases pass, including 101,396
registry, 609 reference-sampling and 33,155 numerical checks. Linux uses
ASan/UBSan and leak detection. All 575 code/tool/fixture inputs match retained
copies; each host retains 26 products. Live generation, lighting, population
and full persistence remain unfinished.

At the preceding checkpoint, the [World registry binding](world/registry.md) shares the actual class
owner across Worlds, performs ordered material and Dimension spawn queries,
and preserves the constructor's live-Random rejection loop. All 22 selected
cases pass on MSVC, GCC and Clang, with Linux ASan/UBSan and leak detection.
The new constructed World contracts pass 150 checks; the existing GAME case
still passes 4,397. All 559 code/tool/fixture inputs match retained copies,
with 22 products retained per host; formatting passes 284 C/header files.
These are native integration checks and existing recorded-observation replays,
without a new executed-original World comparison. Missing-chunk generation,
the World/Biome join, lighting/population joins and full persistence remain open.

At the preceding recipe checkpoint, native
[crafting and smelting initialization](item/recipes.md) constructs
151 recipes and ten smelting entries, sorts the real recipe list and completes
the crafting-statistic array. Ordinary Statistics and Block entry calls return
on both sides; Item and direct-manager entries retain their actual null-reference
failures. Thirty original captures agree across three JVM modes. All ten numeric
graphs, ten catalogs and both Stack projections match MSVC, GCC and Clang.
All 30 selected Windows and 32 cases per Linux compiler pass, with Linux
ASan/UBSan and leak detection. This includes 2,872 recipe, 556 registry-Stack,
11,312,084 array and 32,892 achievement checks. All 555 code/tool/fixture inputs
match the retained host copies; 32 build products per host are retained.
Crafting and furnace gameplay, full World startup, population, synchronous
mutations and persistence acceptance remain unfinished.

At the preceding achievement checkpoint, native [achievement initialization](statistics/achievements.md) ran all
sixteen chains with real ItemStack icons, retained descriptions/parents/bounds,
base-before-achievement registration and actual stdout. Eighteen new original
captures cover both sides, three entry orders and three JVM modes; all six
published numeric graphs and real console bytes match MSVC, GCC and Clang.
All 26 selected Windows and 28 cases per Linux compiler pass, including 7,208
achievement checks, 7,598,064 array checks and real stdout-handle controls.
Linux runs use ASan/UBSan and leak detection. Recipes, smelting, full
bootstrap/population/mutations/persistence and achievement gameplay/UI remain
unfinished.

At the preceding array checkpoint, native [statistics](statistics/registry.md) execute the block/item statistic
array helpers, real translated names, source predicates and alias/list
unification on the shared class owner. Entry order retains distinct prefixes
before the actual recipe PC21 or achievement PC624 dependency. The reached
[String/null formatter](reference/string-format.md) matches 275 captured JDK
cases, 109 typed error operands and all 1,114,112 English-uppercase code-point
values on MSVC, GCC and Clang. All six numeric statistic graph prefixes match
untouched originals. Selected runs pass 23 Windows cases and 25 cases per Linux
compiler with sanitizers, including two locale regressions. Runtime number
formatting, achievements, recipes
and complete bootstrap/population/persistence remain unfinished.

Native [achievement-key loading](statistics/keys.md) now constructs actual
shared class `b`, reads its imported resource, preserves String identities and
closes its reader. All 446 entries and 453 lookups match both original JARs.
The [byte/line reader](reference/reader.md) matches 19,594 projected rows from
800 recorded JDK owners and 7,136 actions. Fourteen selected cases pass on
MSVC/GCC/Clang with Linux sanitizers, including 4,299 key and 134,207 reader
checks at that earlier checkpoint. Caught stack-trace output, runtime formatting,
achievements, recipes and full bootstrap/population/persistence remain unfinished.

Native [Properties decoding](reference/properties.md) and
[translation/resource loading](resources/translation.md) now run on the shared
class owner. Ten selected cases pass on MSVC/GCC/Clang with Linux sanitizers:
252 recorded JDK loads and 980 queries compare exactly, and the native file
consumer matches all 633 properties and String identities from both original
translation classes. Public filesystem tests use synthetic resources and
Unicode roots. Reached String/null formatted lookup now runs; caught-I/O
stack-trace output and complete bootstrap integration remain unfinished.

The shared [Block](block/registry.md) and [Item](item/registry.md) class owner
now executes both orders of the real initialization cycle, including all 106
named Item chains, retained subtype state, BlockItems, conditional fire setup
and five actual profile-created Randoms. It stops explicitly at unfinished
statistics and retains FAILED state. Six selected cases pass on MSVC, GCC and
Clang, with Linux sanitizers: 530,295 Item and 102,659 Block native checks,
plus Material/profile controls and finite original comparisons of
[33,064 Item rows](reference/item-registry.md) and
[3,950 Block rows](reference/block-registry.md) per side. Full bootstrap,
World integration and complete population/persistence remain unfinished.

The [quiet World metadata path](world/blocks.md) now reads and mutates actual
acquired chunk metadata, including dirty equal writes and blank no-op success.
Six selected cases pass on MSVC, GCC and Clang, with Linux sanitizers; the GAME
case reports 4,397 checks. Its typed codec round trip is in memory. Executed
original comparisons, block-ID handlers/notifications, registry bootstrap and
complete population/persistence acceptance remain unfinished.

At `e227e57`, the [native base-World constructor](world/construction.md) retains one
43-field identity through ordered construction, failure and retirement. Direct,
copied and already-loaded normal paths can complete with actual Info,
Dimension/biome and provider owners. Five new cases pass 12,474 native checks;
the separate [Dimension replay](reference/dimension.md) matches 48,782
original-derived rows per side on MSVC, GCC and Clang. All 97 public cases pass
per compiler, with Linux sanitizers. Full registry/bootstrap/subclass startup,
fresh spawn, complete population and live-player saves remain unfinished.

A compiler compatibility follow-up preserves the C17 allocation layout and all
material assertions. Five affected cases pass on MSVC, GCC and Clang; the
[test record](testing.md) distinguishes this selected run from the complete
constructor checkpoint.

The [SavedData ID owner](reference/saved-data-ids.md) passes 196,823 native
checks and matches 78 original-derived memory rows on MSVC, GCC and Clang.
All 25 selected storage/runtime checks pass per compiler, with Linux
sanitizers. Raw ID files, copied nullable UTF-16 keys, signed-Short wrapping
and reached file-error effects have native contracts. Original HashMap disk
ordering, named SavedData and full original World startup remain unfinished.
That selected run brought the declared public inventory to 91; the current
constructor checkpoint above refreshes the complete public suite.

The preceding [metadata binding checkpoint](reference/world-info-runtime.md) passes all
89 public tests on native Windows MSVC and Linux GCC/Clang, with Linux
sanitizers. Its new integration case passes 173 checks. Runtime contexts borrow
the retained Info's actual time/spawn cells, typed region growth updates only
the calling Info, and typed no-list metadata uses the real serializer. Original
World construction and live-player metadata saving remain unfinished.

The per-World [metadata owner](world/info.md) now passes 8,465 native checks and
matches all 750 stable original-derived graph rows on MSVC, GCC and Clang.
Copies keep independent scalars and shared name/Player references; no-list
serialization preserves shared Player renaming and loaded LastPlayed.
Linux uses sanitizers. [Evidence](reference/world-info.md) distinguishes the two
selected checks from the earlier complete-suite checkpoint. World construction
and live-player-list serialization remain unfinished.

The private [millisecond wall-clock backend](reference/wall-clock.md) passes
251 checks on MSVC, GCC and Clang, including four ordered OS brackets per
compiler. All five selected clock/Random cases pass; Linux uses ASan/UBSan and
leak checks. The current native base-World constructor consumes this clock in
its ordered prefix; full original startup remains unfinished.

The C runtime now composes chunk acquisition, saving, pending unloads and explicit
owner retirement. It preserves live callback order, a stable world clock,
metadata-first accounting and retained identities through reentry. Empty native
entity/tile collections are supported; nonempty Boat/Sign behavior remains an
external callback seam.

Standalone [Entity/ItemEntity construction](entity/construction.md) now owns
complete base/item fields, private Randoms, bounds and the reached typed watcher
record. Its 1,276 native checks and 104 real-runtime profile/binding checks pass
on MSVC, GCC and Clang, with Linux sanitizers. Five actual original constructors
per side supply complete returned-state observations and a separate scalar
formula fixture. [Constructor evidence](reference/entity-construction.md)
distinguishes inferred Math inputs from directly observed fields. World startup,
insertion, ticking, loaded Entity activation and chest integration remain open.
At that checkpoint, all 85 public CTest cases passed locally on each compiler,
including the separate 50-field scalar replay. The final test-only formatter adjustment was followed
by a fresh Entity unit build/run on each compiler; details are in
[testing](testing.md). This is local validation of the feature branch.

The [block material library](reference/materials.md) now implements all 28
material identities, 14 palettes, the 16-slot palette table and the original
receiver methods. Its 28,623 ownership/property checks pass on MSVC, GCC and
Clang, with Linux sanitizers. Shared application/world binding and the block
registry remain unfinished. A separate selected two-case run also matches all
116 captured original client/server rows byte for byte on each compiler.

The next state layer adds [ItemStack ownership and NBT provenance](item/stack.md)
and [scheduled records, membership and ordered history](world/scheduling.md).
Three native cases pass 1,362 checks on Linux GCC/Clang with sanitizers and
Windows MSVC without sanitizers. The ordered owner also matches all 13,584 steps
from the captured JDK public-API fixture on those compilers. These are finite
state/collection results; original item/container comparisons and bound-world behavior
remain unverified.

Seeded [Gaussian cache and strict log/sqrt](reference/random-gaussian.md) now
match all 14,013 captured JDK result/full-state operations on GCC/Clang with
sanitizers and native Windows MSVC. Six separate JDK runs agree. This qualifies
the finite explicit-seed/math corpus. The separate
[Random profile and platform clocks](reference/random-profile.md) now pass
191 native profile checks, 172 Windows / 173 Linux clock checks and 17,769 checks
over 4,866 Java scalar vectors on all three compilers. Linux runs use ASan/UBSan
and leak checks; Windows uses strict Debug without sanitizers. A separate
instrumented Windows JDK17 trace records three constructor entropy events and
one retained Math owner. Its recorded-input native replay passes 18 checks on
each compiler. At that checkpoint, all 80 public CTest cases passed on all three
compilers; that run does not include opt-in original-game comparisons. Population, registry,
bound-world Entity integration and application wiring, original gameplay consumption and the installed-native
clock conversion bridge remain unfinished.

At the earlier seeded-Gaussian checkpoint, all 75 public tests passed on each
of the three compilers, plus 157 native protocol controls checked separately.
That suite included the 85-check Gaussian unit and full captured replay.
These results retain the private lifecycle checkpoint below as its own
historical source qualification.

Exact-source lifecycle qualification passes at
`46546d8322de52beb6c3a51f30e827c0260d2b30`. Complete Windows v10, GCC v9 and
Clang v9 suites each passed 92 tests without failures or skips: 1940.77s,
2969.23s and 2961.75s respectively. Independent source/provenance and lifecycle
semantic audits, unchanged storage rechecks and all 22 capture roots per platform
also pass, with Linux ASan/UBSan and stable measured inputs. The controller retains bounded
JDI mirrors from their first recorded IDs; its separate debugger regression
passes on Windows JDK17 and Linux JDK21. The helper interval correction
permits 600 seconds only for observed dedicated101 operations; its separate
Python regression passes on both native hosts. The preceding v7 and v8 matrices
remain excluded. Windows v9 is also excluded after a premeasurement original
setup refusal; its successful rows do not count toward the fresh Windows v10
suite. See the
[lifecycle checkpoint](reference/lifecycle.md#exact-source-lifecycle-checkpoint)
for its evidence ledger and retained failed attempts.

The persistence foundation passes exact-source acceptance at
`9ad690d83592f127b9bb39d7233e8eef06fd0029`. All 71 selected local tests pass
without skips on native MSVC and WSL GCC/Clang, with Linux ASan/UBSan and
independent platform, provenance and copied-world semantic audits. The complete
rewrite remains unfinished. See the [storage checkpoint](reference/storage.md#exact-source-storage-checkpoint).

The preceding integrated main checkpoint `b7b33d4b66e027a6de18d04018cd0ba34dd87f0b`
combines normal
generation, synchronous cache acquisition and read-only region inspection.
Its tree matches final documentation checkpoint
`d0dde15a095424a2103dd755db546a9be2ab3c30`; implementation acceptance is at
`28a55b52c001c4c114a11ca7df844fc1f286d328`.
All 56 selected local tests pass without skips on native MSVC and WSL GCC/Clang,
with Linux ASan/UBSan and independent acceptance audit. Hosted checks pass 48
public/numerical tests per compiler and formatting for 89 C/header files.
Inspect the live worktree before continuing. The complete rewrite remains unfinished.

The [resulting-main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39091121154)
pass all four jobs. The initial Clang configuration rejected a dependency
download with the wrong hash, before compilation. Three fresh local downloads
matched the unchanged official checksum; the failed job passed on an unchanged
source retry. The failing response bytes were unavailable, so the precise
delivery failure is unknown. The published five-page wiki checkpoint is
`c4ecb344bffae2d5bd293f2678063b6bcc985025`, with all five remote page blobs
verified against `docs/wiki/`.

## Persistence foundation checkpoint

The [region image writer](storage/region-writer.md), [level metadata codec](storage/level.md),
stable stored chunks and [native world storage](world/storage.md) now have
finite original-to-C-to-original and C-to-original-to-C acceptance. Metadata
fallback/rotation, session checks, both explicit dimension roots, shared region
ownership and the actual acquisition loader are covered by original observations,
paired inputs and separately identified C failure policies.

At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, MSVC/GCC/Clang each pass 71
selected tests without skips: 744.00, 1255.43 and 1256.05 seconds respectively.
Independent audits bind all 290 tracked files, native tools/executables/build
inputs and all 36 original manifests. Separate audits check deep new-profile
provenance and complete supported world-state/control/preservation projections.
The three original-only fixtures remain unchanged. The
[hosted code checks](https://github.com/frankischilling/minecraft-c/actions/runs/39104921389)
pass 59 public/numerical tests per compiler and formatting for 111 C/header files.

The [evidence](reference/storage.md#exact-source-storage-checkpoint) retains
exact capture/report hashes, clocks, limitations and excluded earlier attempts.
These results cover seed23, eight storage slots and explicit dimensions 0/-1.
Opaque transport does not implement entity/player simulation or every subtype
serializer. Gameplay save/unload scheduling, dimension transitions,
running-server persistence and crash recovery remain open.

## Verified combined checkpoint

The coordinator composes retained C generation and finishing with pending
removal, ordered publication, activation and live population gates. The
retained-provider corpus has 20 calls, five generations and 5120 ordered query
pairs. A separate callback corpus adds successful original loads with live
time stamping and natural population, replaying observed cache/world effects
while C computes coordinator transitions. It retains 12 calls, 16 external
callbacks, 394 lifecycle events, 24 inventories and four untouched controls
per build. Decoding and terrain-feature bodies remain external.

Exact-source suite times are 529.51 seconds on MSVC19.51, 1044.42 on WSL GCC13.3
and 1044.86 on WSL Clang18.1. Each runs 47 public tests, the numerical Java oracle
and eight original comparisons. Independent audit verifies all 55 callback
source files against Git objects, unchanged inputs/classes, complete callback
projections and controls, source attribution for all 24 original manifests,
168 actual passed CTest rows and exact-head hosted checks. Its report SHA-256 is
`ef597d13ffa27582e223bd95f692cc890e4ac48f6d4ce61d8f95c77313932b2e`.
The initial Windows setup-unavailable attempt is preserved and excluded; the
accepted complete fresh-world retry has zero skips. See
[callback evidence](reference/cache-callbacks.md#exact-source-platform-checkpoint)
for all manifests, qualification limits and the unchanged observation cap.

[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39089309334)
pass at that exact source. Loading and population callback profiles each pass
C/client, C/server and client/server independently. The read-only region comparison also reruns
successfully within these suites, preserving its eight compatible cases and 41
explicit diagnostic departures.

## Verified normal composition checkpoint

Source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes all 41 configured
CTests on native MSVC19.51 and WSL GCC13.3/Clang18.1 without skips. Both Linux
builds use ASan/UBSan. Each exact-source suite runs 35 public tests, the numerical
Java oracle and original finishing, queue, chunk, nibble and generation
comparisons. These counts describe this checkpoint, not later branch additions.
The normal generator unit passes 99073 constructed contract checks.

The production generation corpus uses seeds 23, 24, 26 and 51, both original
sides and nine sequential calls per retained generator. All three pairings
pass independently per build. Each build retains 72 complete generations,
73728 ordered height-query pairs, 1224 stage events, 144 marker inventories and
eight complete byte-equal untouched controls. The lossless public fixture also
replays all eight continuous nine-call inputs on each toolchain.

Each generation manifest retains 57 unchanged attributed inputs and eight
unchanged compiled helper classes. Independent audit verifies all 47 project
source files against the exact commit, all complete records and world-file
inventories, actual 41-test logs and zero CTest exits, with no skips or audit
errors. The audit report SHA-256 is
`9acbf7d9865b4f9817bf1125d33078895df592d7e0bae5a973f88ff62ec80843`.
See [generation evidence](reference/generation.md#exact-source-platform-checkpoint)
for all three capture paths, manifest hashes and finite observation conditions.

[Hosted checks at a08c120](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass 36 public/numerical tests per compiler on native Windows MSVC and Linux
GCC/Clang, plus clang-format18 for 79 C/header files. Hosted checks use no
private originals. The [API contract](world/generation.md) documents borrowed
ownership, current materials, finite coordinates and failure preservation.

## Prior finishing integration

Normal finishing connects local heights/direct sky, packed cached-world
queries, original light admission, ordered queues and dirty state. Its runtime
integration is `fa17e3d8885632ca0baecf71bdf9551e47be68eb`; the latest integrated
finishing evidence checkpoint is `491962cfc0cce03e2572432b4ff3a00b5f2fc905`.

The original-only six-case capture passes at
`.local/reference/finish-fixtures/capture-ca34ded514654e0eb6891161b3feaa50/manifest.json`.
Each case has two consecutive full passes per original side and a fresh
untouched control. All complete traced/control snapshot files match byte for
byte. Public replay covers 12 original inputs, 24576 exact height-query records
and 48 complete local/queue stages.

Finishing source `a18615f0d9d90758f0ea95b0218cec97733d2adf` passes all 37
configured local CTests per compiler without skips, with Linux ASan/UBSan.
All six finishing cases match in three pairings. The twelve untouched controls
and 48 marker inventories pass per build. Each manifest retains 54 unchanged
inputs and six unchanged helper classes; all 44 project source files match the
exact commit. Constructed finishing tests pass 3259 checks; seven helper guards
pass. See [finishing evidence](reference/finishing.md) for manifest hashes.

Final finishing branch source `7c82b4a861e742e717b6ea8fd9646c5611211dba` passes
all 33 hosted public/numerical tests per compiler and formatting for 75 C/header
files in [run39076093215](https://github.com/frankischilling/minecraft-c/actions/runs/39076093215).
Its integration tree matches the tested tree. The
[resulting-main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39076208781)
also pass. Earlier [run39075690802](https://github.com/frankischilling/minecraft-c/actions/runs/39075690802)
passes the same hosted test and format counts. That finishing implementation
branch was removed; the normal-generation and cache checkpoints followed it.

The finishing wiki checkpoint was `4e4a9f3853ac8db012bca9aaf179d956e288a686`,
with remote head and five page blobs verified. Current wiki content is versioned
under `docs/wiki/`. README remains unchanged.

## Scope and next work

The normal entry uses the same world's borrowed biome manager, a separate
biome output preserving manager `d`, all 13 current material pairs, and finishing
before unpublished return. Both coordinates stay within `INT32_MIN + 8` through
`INT32_MAX - 9`. Measurements cover fresh finite headless worlds and complete
controls; they do not establish concurrent simulation or general scheduling.

Production observations cover persistent client `fd`/`ok`, base server `dj`/`jb`
and actual `MinecraftServer` construction with `dp`/`he`. Successful reloads
use genuine `mx`/`ie` save handlers and `ld`/`hb` loaders in the base-world
contexts. The dedicated constructor/API corpus covers force gates separately
from a running server. Pending removal, load/fallback selection, publication,
ordered insertion, activation and natural population remain distinct effects;
constructed recursion checks do not establish gameplay reentrancy.
The C cache now also has a [synchronous acquisition coordinator](world/cache.md).
The extended suite contains 3945 constructed checks recorded in development;
its exact-source qualification is recorded above. Independent
review matches all 231 acquisition instructions across `ok`/`jb`/`he`.
The combined retained-provider corpus now has three original-only contexts,
20 operations, five generated chunks, 5120 exact query pairs, 252 lifecycle
events, 40 inventories and three byte-equal untouched controls. Lossless public
replay and 28 Java-free guards pass on MSVC/GCC/Clang. A separate four-context
[callback comparison](reference/cache-callbacks.md) now passes original controls
and exact-source C comparisons for successful loads and natural population. Its
public fixture retains 12 calls, 394 lifecycle events and 24 inventories. Its
expanded exact-source acceptance is recorded above. Earlier source
`9a50ac67997082c89dbc43cf4274fba1427c1565` passes all 52 selected tests without
skips on MSVC/GCC/Clang, including seven original comparisons and Linux
sanitizers. Independent audit verifies all 53 source objects, 63 stable inputs,
nine helper classes and complete records per build; hosted checks pass 45 tests
per compiler and formatting for 88 C/header files. See the
[cache checkpoint](reference/cache.md#exact-source-platform-checkpoint).
These finite checks
do not establish a running server or complete cache lifecycle parity.

Successful original reloads return saved arrays unchanged while the loaded
minimum-height field remains zero. Their callback comparison preserves that
result without implementing a C disk decoder.
The [read-only region inspector](reference/region.md) has a guarded development
comparison covering all 49 queries per original side, with byte-equal controls,
eight compatible cases and 41 explicit diagnostic departures. Exact source
`092301ae8282d50a734df9018f791925aa9be99b` passes the original comparison on all
three platforms; independent audit and hosted 42-test/87-file checks pass.
See [region evidence](reference/region.md#verified-source-checkpoint).
No region result follows from a08 alone; its acceptance uses the separately
recorded exact source.

Terrain-feature population bodies, light propagation, other dimensions,
general tick-driven saving/unloading, gameplay restart/recovery and protocol
remain open. The synchronous lifecycle above does not complete these systems. See the
[roadmap](roadmap.md), [testing](testing.md) and [parity matrix](parity.md).
Earlier integrations remain in [history](history.md). M1 is complete; M2 through
M5 remain open.

There is no playable C client/server or supported gameplay world-loading
workflow. Native library writes have only the finite scope recorded above. All four
multiplayer pairings, native graphical play, controls and presentation/audio
comparisons remain required. Original offline loopback server startup/save/stop
passed without a client. Graphical Launcher capture remains unavailable.
Account authentication is the only approved gameplay exception. No unanswered
owner question blocks current work.
