# Build and tests

Fresh full local suites pass all 173 MSVC and all 175 tests on each GCC/Clang build, with zero skips.
Controller mirrors and lifecycle checks also pass.
Linux uses ASan/UBSan/leak checks and a byte-identical 961-file source copy on
Linux storage. The mounted-source timeout attempts remain retained; no deadline
was relaxed. The expanded Piston geometry contract passes 386 checks per compiler.
All 499 C files and headers pass clang-format 18 with the requested style.
Power queries pass 7,593 checks per host, horizontal directions 359 and existing
public support 4,359. Power uses two real Worlds and two loaded records; its
source and native checks do not establish original power-runtime equality.
Class queries pass 373 checks per host; passive identity/ancestry passes 13,819,
pressure plates pass 561 and movement passes 560. The Class fixture uses eight
Worlds/eight loaded records; pressure uses eight Worlds/thirty-two records.
These native contracts do not establish original pressure/Class runtime equality.
Validation prose was added after execution; implementation/test/driver bytes are
unchanged. The [pressure contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/pressure-plates.md) records its scope.

The [test scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
records commands, ownership, source-copy checks and remaining original-acceptance
limits. Earlier [hosted checks at `eab4533`](https://github.com/frankischilling/minecraft-c/actions/runs/39705949294) also pass.
At the earlier exact `ee5a52b` checkpoint,
[hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39700565668)
passed 169 Windows and 171 tests per Linux compiler, plus 483-file formatting.
That result predates singleton state-shape integration.

The original tick family admits 114 inputs and 228 snapshots per side.
At source `eab4533`, MSVC, GCC and Clang native protocols qualify against the same
original family. The finite comparison matches all 114 literal inputs and binary
action classifications per side, and all 5,016 comparisons of eleven primitive
fields per compiler: the Piston's seven own fields, including raw progress bits,
and the actual Tile's inherited x/y/z/invalid.
Earlier malformed MSVC streams and their failed protocol audit remain retained.
Caller Context coordinates, World references or aliases, complete errors,
World/Entity/Block tick graphs, original-runtime state-shape equality
and whole-engine parity remain unqualified. Earlier finalization and movement
replays retain their own source and comparison scope.

The following movement checkpoint predates finalization integration.

The [movement contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/movement.md) pass five focused cases on native
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

The [movement source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-movement.md) records native reproduction
commands and a separate original family: all 59,654 client records and only
24,109 intact, non-prefix server records qualify after disk exhaustion. Failed
receipts and missing historical before-pin objects remain explicit.

The [retained-original native replay](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-movement.md#retained-original-native-replay)
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

The [Piston contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles/piston.md)
pass 295 Direction, 296 Tile and 144 geometry checks per native compiler. All
165/167/167 MSVC/GCC/Clang cases pass without skips, with numerical Java checks;
Linux enables ASan/UBSan/leak checks. Separate retained-original replays compare
18,292 interpolation records and 10,986 geometry successes plus 15 native
failure prefixes per side/compiler, keeping 25 explicit geometry exclusions.
All children finish naturally. All 464 C files and headers pass formatting.
Active piston mechanics, Entity movement and complete original World traversal
remain unfinished.

The following collision checkpoint predates Piston integration.

The collision contracts pass 732 checks per compiler, with all
162/164/164 MSVC/GCC/Clang cases passing without skips. Fresh original/native
getter tools compare 623 complete record payloads per side/compiler, retaining
all 1,577 exclusions and raw streams. Nearby comparisons are freshly rerun on
the rebuilt probes. All 455 C files and headers pass formatting.
The [testing guide](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) records ownership,
multipart failure, framing and native/original scope limits.

The following nearby checkpoint predates collision integration.

The final LF-only correction to one test source was rebuilt and rerun on
all three compilers. Complete regression evidence remains in the preceding
full builds, whose runtime and probe source bytes are unchanged.

The [nearby contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/collision/entities.md)
pass 606 checks per native compiler and all 161/163/163 configured cases, without
skips and with numerical Java checks. Linux uses ASan/UBSan/leak checks. Final
formatting passes 448 C source/header files. Each host retains 754 native inputs,
254 dependency files, 225 selected products and 41 captures.

The [public reproduction tool](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/tools/world/collision/compare_nearby.py)
passes fresh original/native Chunk comparisons: 353 exact successful rows and
three precise nullable-failure prefixes per side/compiler, with eight malformed
Java collection cases explicitly excluded. Original World/provider-loop replay,
full collision shapes, Entity movement and whole-game acceptance remain unfinished.

The following bounds checkpoint predates nearby integration.

The [bounds case](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/bounds.md)
passes 4,601 checks on each native compiler. Final fresh builds pass all
160 MSVC and 162 per sanitized GCC/Clang configured cases, without skips.
Byte audits verify 749 source inputs, 254 dependencies, 220 products and
40 captures per host. All 33,534 original client/server raw rows match each
exact final probe; independent joins verify complete rows, streams and products.
Earlier NaN payload failures remain retained, with no normalization of results.
Attached braces and separated definitions are applied; all 443 C source/header
files pass clang-format18. Full Entity movement and FallingSand physics remain
unfinished.

The following snow checkpoint predates bounds integration.

The [snow case](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/snow.md)
passes all 159 configured MSVC cases and 161 per sanitized GCC/Clang build in
focused/complementary partitions, without skips and with numerical Java checks.
Snow adds 380 checks per host using both Registry sides, four GAME Worlds and
sixteen serialized input Chunks. Fresh audits verify 742 source inputs, 254
dependency pins, 215 products and 40 captures per host. Formatting passes 438
C source/header files. The generated-terrain regression completes the controller,
retains actual snow and resets the falling flag. No original snow replay or
whole-game qualification follows.

The following falling checkpoint predates snow integration.

The [falling contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/falling.md)
pass all 158 configured MSVC cases in two sequential partitions using unchanged
compiled outputs, plus nineteen affected cases on each sanitized GCC/Clang build.
The new cases add 230 constructor/NBT checks and 300 runtime checks per host.
Ordinary gravel continues to the prepared terrain's snow-neighbor failure at
controller PC2886. An earlier disk-exhausted Windows attempt remains retained.
Formatting passes all 436 C source/header files. Later Entity ticks/movement,
full original replay and complete population qualification remain unfinished.

The following controller checkpoint predates falling integration.

The [normal controller checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/normal.md)
pass all 156 configured MSVC cases and 158 on each sanitized GCC/Clang build,
with zero skips and local JDK comparisons. Prepared all-air cache calls pass
2,219 checks per host; genuine shared falling-flag access adds 408. The retained
driver independently inspects all eight saved Chunk slots. The generated-terrain
activation case preserves real writes at the gravel-hook frontier. Full original
continuation, all selectors/reentry, falling/snow descendants and natural terrain
remain unqualified. Formatting covers 432 C files/headers.

The following temperature checkpoint predates controller integration.

The [staged temperature checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/temperature.md)
pass all 154 configured MSVC cases and 156 on each sanitized GCC/Clang build,
with zero skips and local JDK comparisons. Each host retains 726 unchanged
selected inputs, 254 zlib pins, 194 products and 37 captures. The new test passes
371 checks per host: exact retained original numerical vectors, actual array
identity/lifetime, rebind, allocation failure prefixes, accounting and FP order.
The finite original values and native sequencing/quota controls have distinct
scope. No fresh original or full controller/population qualification is claimed.
Repository formatting covers 425 C files/headers.

The following Dungeon checkpoint predates temperature integration.

The [Dungeon checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/dungeon.md)
pass all 153 configured MSVC cases and 155 on each sanitized Linux GCC/Clang
build, with zero skips and local JDK comparisons. Each host retains 723 selected
unchanged inputs, 254 zlib pins, 191 products and 36 captures. Dungeon generation,
failure prefixes and real prepared save/unload/reload pass 3,655 checks per host;
ordinary Item runtime access passes 292. Formatting covers 422 C files/headers.
Six fresh original helper captures preserve cold failures and two explicitly
Block-initialized contexts per side. Across all native hosts, 924 comparisons
match actual Stack fields, exact UTF16 mob names and five subsequent Random
draws. The separate [whole-placement comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/dungeon-placement.md)
captures 24 prepared rooms per original side and passes all 144 matching native
comparisons. Complete arrays, all physical Chest slots, real Map keys, ordered
Tile/light queues and five consuming Random draws match. Natural population,
broader original routes and full closure remain open.

The following ordinary Item checkpoint predates Dungeon integration.

The [ordinary Item persistence checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-game-storage.md)
pass all 151 configured MSVC cases and 153 on each sanitized Linux GCC/Clang
build, with zero skips and local JDK comparisons. Each host retains 710 selected
unchanged inputs, 254 zlib pins, 185 products and 35 captures. The GAME Entity
test passes 468 checks and the lower overlay passes 255. Clang-format18 covers
414 C files and headers.

Four fresh original processes on the selected Windows JDK17 profile each run
ten prepared cases and 117 explicit top-level calls. Twenty-four comparisons
match complete known two-Item tag lists, retaining types, UTF16 names, scalar
values, floating bits and List order/metadata. Compound key order and the native
unknown-preservation policy remain separate. This is not complete World, natural
population, or broader original/native save-route qualification.

The following leaf checkpoint predates ordinary Item disk integration.

The [Item NBT evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-nbt.md)
records twenty actual original cases per side and their exact native field/tag
comparisons on MSVC/GCC/Clang. `entity.nbt_observation` loads both complete
captured stdout streams and compares 4,844 rows from fresh native runs without
a JAR or JDK. Separate fixture/comparator controls exercise malformed streams,
pins, scope, provenance and a changed scalar. These checks cover prepared Item
calls, not ordinary Chunk persistence or complete World startup.

All 149 configured MSVC cases and 151 cases on each Linux GCC/Clang build pass,
with zero skips and local JDK comparisons enabled. Linux uses ASan/UBSan and
leak checks over fresh byte-verified source/dependency copies. Each host retains
699 unchanged selected inputs, 254 external zlib pins, 178 products and 34
captures. Clang-format 18 checks all 404 C files. NBT/Class/factory/batch native
checks number 1,084/7,759/364/353 per host. The final binaries also match fresh
calls into both original archives for the finite twenty-case NBT corpus.
Selected build/tool/product joins and all retained capture bytes were rehashed;
the detailed NBT and batch audits do not establish whole-game or full process
tree coverage.

At the preceding live Tile checkpoint:

The [live Tile contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles.md)
exercise actual Chest/Spawner, Stack, Map and List owners, staged GAME loading
and terminal disposal. The [live save contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md)
add 1,003 checks per host, complete current saved arrays/records, retained
unknown fields, late failures, retries and explicit native coordinator reload.
Both installed GAME constructors bind BASE policy; no natural dedicated startup
or original/native whole-save comparison follows. All 144 configured public cases pass on MSVC and all 146
on each sanitized GCC/Clang build, with zero skips and local JDK comparisons.
Each host passes 468 Tile checks and 98,868 provider-activation checks, retaining
678 unchanged selected source inputs, 254 external zlib pins, 163 products and
32 captures. Linux source/dependency copies match before and after execution.
Clang-format 18 checks all 388 C sources and headers. Complete population,
original/native replay, live Entity serialization and whole World saves remain unfinished.

The following checkpoint predates live Tile integration.

The [World-query and plant contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/queries.md),
[plant features](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/plants.md),
[trees](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/trees.md),
[ore/clay](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/ore-clay.md)
and [lake/spring features](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/lakes-springs.md)
pass 6,110 combined checks per host. All 142 configured public cases pass on MSVC and all
144 on each sanitized GCC/Clang build, including local JDK comparisons.
Each host retains 661 unchanged selected inputs, 254 external zlib source pins,
143 products and 30 new captures. Linux source/dependency copies match before
and after execution. Selected tools include Java/javac. Six full baselines,
eight occupied records and two exact empty region headers close the complete
eight-region-file inventory. Run
`ctest --test-dir build -R "^(world.queries|world.entity_publication)$" --output-on-failure`.
These finite native contracts and separate original API captures do not supply
native replay or complete population qualification.

At the preceding checkpoint, the
[Entity publication contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/entities.md#validation-and-limits)
pass within all 66 selected cases on MSVC and sanitized GCC/Clang. Publication
checks number 730 per host; fluid checks are 529/528/528. Each host retains 640
inputs and 57 products. Eight publication regions stay byte-equal after refused
writes and terminal teardown; sixteen fluid records receive actual GAME saves
before a genuine drop publication. [Separate original observations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-publication.md)
each complete 21 calls, 44 snapshots and 88 Contexts with full Chunk bytes and
normal/cross-World cases. These have no native replay, complete local-original
inventory, full population or Entity persistence qualification claim.

The [World scheduling contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/scheduling.md#native-validation)
pass 424 checks within all 60 selected cases on MSVC/GCC/Clang, with Linux
ASan/UBSan and leak detection. Each host retains 624 selected inputs and 51 actual
products. Twelve loaded known-empty records finish actual GAME maintenance saves.
Separate original client/base-server experiments each complete 18 calls and
retain 39 full contexts; all 18 framing controls reject per side. The optional
present-stone call was skipped on both sides. These captures have no native
replay, dedicated-route, full-population or complete persistence qualification.

The [mutation foundation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/mutations.md)
passes 218 new checks within 52 selected cases on MSVC/GCC/Clang, with Linux
ASan/UBSan and leak detection. All 618 selected inputs and 44 products per host
are retained. Separate fresh original direct-Chunk experiments cover 25 calls
per side, with matching local arrays/scalars/outcomes and preserved context.
They have no native replay yet and do not qualify complete cached population,
dedicated routes or original save interoperability.

The [generated Chunk contracts](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/chunk-objects.md)
pass 33,458 checks within all 38 selected cases on MSVC/GCC/Clang, with Linux
ASan/UBSan and leak detection. Real owners retain their identities through
finishing and array save/reload; quota and lifetime failures preserve reached
effects. All 596 inputs and 35 products per host are retained; formatting passes
313 C/header files. Full GAME generation, population and save acceptance remain
unfinished; these tests provide no new executed-original Chunk comparison.

The [concrete World height/queue contracts](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/runtime-lighting.md)
pass 177 height and 2,000,382 queue checks within 33 selected cases on
MSVC/GCC/Clang, with Linux ASan/UBSan and leak detection. Real million-entry
overflow, nonempty allocation-peak failure and exact console order are covered.
All 588 inputs and 33 products per host are retained; formatting passes 305
C/header files. These are native client-World contracts, without a new original
World or complete generation/population comparison.

The [Biome observations](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/biome-registry.md)
capture ordinary bootstrap, six graph snapshots and 3,824 raw-bit queries per
side. Public replay uses these observations without Java or original archives.
All 26 selected cases pass on MSVC/GCC/Clang, including 101,396 registry,
609 reference-sampling and 33,155 numerical checks. Graph replay passes with
synthetic and imported resources, including 20 corruption guards. Linux uses
ASan/UBSan and leak detection; all 575 inputs and 26 products per host are
retained. Formatting passes 292 C/header files.

The [World registry integration](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/registry.md)
passes 22 selected cases on MSVC/GCC/Clang, including 150 constructed World
checks, existing recorded-observation replays and Linux sanitizers. All 559
code/tool/fixture inputs match retained copies; 22 products per host are
retained. Formatting passes 284 C/header files. This does not qualify complete
fresh-world startup, population or gameplay persistence.

The preceding [recipe initialization](https://github.com/frankischilling/minecraft-c/blob/main/docs/item/recipes.md)
checkpoint constructs all 151 recipes and ten smelts, and completes the crafting
statistics array. Thirty original captures cover five entry orders on both
sides. All ten numeric graphs and catalogs, plus both Stack projections, match
MSVC/GCC/Clang. All 30 selected Windows and 32 cases per Linux compiler pass,
with Linux sanitizers. Ordinary Statistics and Block initialization returns;
Item and direct managers preserve their recorded null-reference failures.
Full World startup, crafting/furnace gameplay, population, synchronous mutations
and persistence acceptance remain unfinished.

Use a C17 compiler, CMake 3.24+, Ninja, and Python 3.10+ for tests. The current
baseline is x86-64 Linux and Windows. Public tests need no display, audio,
Minecraft assets, account, or JVM. Storage links statically to zlib 1.3.2;
initial configuration downloads its pinned, SHA-256-verified official archive.

On Linux install GCC or Clang, CMake, Ninja and Python. On Windows use Visual
Studio's x64 C developer shell. Then run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For a Visual Studio generator use `cmake -S . -B build -A x64`, build with
`--config Debug`, and add `-C Debug` to CTest. Keep separate directories for
different generators/compilers. Set `BUILD_TESTING=OFF` for a library-only build.
The [build guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/building.md)
contains complete setup and formatting commands; the
[dependency guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/dependencies.md)
explains offline source overrides and license notices.

Public checks cover numerical/text semantics, raw NBT, compression, both noise
families, climate/biome classification, normal density/base blocks, surface
replacement, normal cave carving, packed chunk arrays, height/skylight data
and full finishing, unpublished normal generation, metadata/schema ownership,
region image writing, native region transport, world-store acquisition,
Integer-set/pending history, lifecycle/runtime ownership and the local resource
importer.
GCC/Clang builds support `MC_ENABLE_SANITIZERS=ON`, including
the zlib dependency. CI runs native Windows MSVC and Linux GCC/Clang plus
clang-format18. No original JARs/assets are downloaded by CI.

The [joined Block](https://github.com/frankischilling/minecraft-c/blob/main/docs/block/registry.md)
and [Item construction](https://github.com/frankischilling/minecraft-c/blob/main/docs/item/registry.md)
passes six selected cases on MSVC and Linux GCC/Clang, with Linux sanitizers.
The units have 102,659 Block and 530,295 Item checks covering both entry orders,
retained aliases/subtypes, Random continuation and real allocation-budget
failures. The finite comparisons match all
[33,064 Item rows](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/item-registry.md)
and [3,950 Block rows](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/block-registry.md)
per side. Full bootstrap and broader original equivalence remain open.

The [quiet metadata addition](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/blocks.md)
passes six selected native cases on MSVC and Linux GCC/Clang, with Linux
sanitizers. The GAME case has 4,397 checks, including actual stored/blank
identities, packed-key aliases, equal writes, bounds, retained failure effects
and an in-memory typed codec round trip. This addition has no fresh full-suite
or executed-original result.

The [native World construction checkpoint](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/construction.md)
at `e227e57` passes all 97 public cases on MSVC and Linux GCC/Clang, with Linux sanitizers.
Five new cases pass 12,474 native checks. The separate
[Dimension replay](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/dimension.md)
matches 48,782 original-derived rows per side and checks corruption/protocol
refusals. All 377 code/tool/fixture inputs stay unchanged; clang-format 18
passes 196 C/header files. This does not qualify complete original World
startup, population or the entire local-original inventory.

The earlier SavedData ID checkpoint passed 25 selected storage/runtime cases on native
MSVC and Linux GCC/Clang, with Linux sanitizers. Its new unit has 196,823 checks;
the public memory replay matches 78 original-derived rows, rejects fourteen
altered outputs and executes four protocol refusals. The declared inventory
was then 91, without a fresh complete-suite claim at that checkpoint.
[SavedData evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/saved-data-ids.md)
keeps original HashMap disk order, named subtypes and actual World startup open.

The preceding metadata binding checkpoint passes all 89 public cases on native
MSVC and Linux GCC/Clang, with Linux sanitizers. Its new integration case has
173 checks for actual live-cell ownership, callback failures, typed files and
per-Info growth. [Binding evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/world-info-runtime.md)
keeps original World construction and live-player metadata saving open.

The earlier WorldInfo ownership and no-list serialization checkpoint passed 8,465 native checks and all
750 stable original-derived graph rows on MSVC/GCC/Clang, with Linux sanitizers.
Fourteen verifier corruptions are rejected per compiler. This is selected
two-case validation; its declared inventory was 88, without a fresh full-suite
claim. [WorldInfo evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/world-info.md)
documents shared references, native error mapping and original-construction limits.

The native millisecond wall clock adds 251 checks on MSVC/GCC/Clang, including
four ordered OS brackets per compiler. Its selected five-case clock/Random run
passes with Linux sanitizers. World construction remains unfinished; see the
[source and test limits](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/wall-clock.md).

`core/math` adds original float lookup/floor tests without Java. Its optional
`MC_ENABLE_MATH_REFERENCE_TESTS` checks every initialized table entry and
sampled scalar calls from both supplied originals; it needs no world setup.

`MC_ENABLE_JAVA_REFERENCE_TESTS=ON` adds standard-library numerical and collection
oracles with JDK17+. Original-class NBT/compression comparisons require JDK11+
and the locally supplied verified client/server JARs, enabled separately through
`MC_ENABLE_NBT_REFERENCE_TESTS` and `MC_ENABLE_COMPRESSION_REFERENCE_TESTS`.
`MC_ENABLE_NOISE_REFERENCE_TESTS` adds exact original client/server noise
comparisons with the same supplied-file/JDK/source-commit requirements.
`MC_ENABLE_SIMPLEX_REFERENCE_TESTS` separately checks actual original 2D simplex
grid overloads, constants, mixed construction and octave controls.
`MC_ENABLE_BIOME_REFERENCE_TESTS` checks original classification and climate
methods, exact table contents and mutable buffer sequences, with client-only
raw temperature tested separately.
`MC_ENABLE_DENSITY_REFERENCE_TESTS` compares normal density/base methods in
genuine original world contexts created under fresh ignored directories. It
checks exact constructor state, climate, density, scratch and block values.
`MC_ENABLE_SURFACE_REFERENCE_TESTS` checks the untouched surface pass in
constructor-Random and explicit chunk-reseed contexts, including current
material bytes, complete fields/blocks, suffixes and live Random continuation.
`MC_ENABLE_CAVES_REFERENCE_TESTS` adds two comparisons: the public cave scheduler
in genuine base/surface contexts and protected room/path controls over owned
buffers. These need a JDK containing `java`, `javac` and `javap`. Both compare
complete buffers, Random continuations and preservation controls independently
between C/client, C/server and client/server.
`MC_ENABLE_NIBBLES_REFERENCE_TESTS` checks original packed-array constructors,
getter sweeps and setter snapshots. `MC_ENABLE_CHUNK_REFERENCE_TESTS` checks
fresh/repeated local chunk arrays and heights in genuine original world contexts.
The full corpus also needs frozen original post-cave inputs; see
[chunk reproduction](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/chunk.md).
`MC_ENABLE_LIGHT_REFERENCE_TESTS` checks actual original light-region merges
and world queues in genuine fresh worlds, preserving each starting queue. The
separate million-entry experiment observes the original automatic abort.
Public C tests replay recorded complete queues and storage contracts without
original files or Java. See the
[queue evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lighting.md).
`MC_ENABLE_FINISH_REFERENCE_TESTS` adds full normal finishing with JDK17+ and
`jdk.jdi`. It observes actual ordered height entries/returns over two consecutive
passes in each of six fresh cases. A separate untouched control must match
complete snapshots exactly. The debugger uses loopback only. Public fixtures
compare all decoded bytes, queues and queries without Java; see
[finishing evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/finishing.md).

## Storage foundation checks

The normal build includes `mc_persistence`, the metadata codec, stored-chunk
owner, region image writer, native region cache and world store. Public checks
include `storage.level`, `storage.region_writer`, `world.stored_chunk`, the
`tools.level_*`, `tools.region_write_*`, `tools.chunk_storage_*` and
`tools.regions_*` checks, `tools.world_store_interop` and
`tools.world_storage_reference`. They use retained
observations and constructed C contracts without original archives or Java.

`MC_ENABLE_STORAGE_REFERENCE_TESTS=ON` enables `reference.level`,
`reference.region_write`, `reference.chunk_storage` and `reference.world_storage`.
These require both
locally supplied verified JARs, JDK17+ and `MC_REFERENCE_TESTED_COMMIT`. Keep
reference worlds in fresh owned or copied directories. The
[storage evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md)
distinguishes codec/image comparisons from native ownership and copied-world
acceptance; the
[ownership contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/storage.md)
describes lifetimes, limits and unsupported operations.

At storage checkpoint `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71 selected
local tests pass without skips on native MSVC and WSL GCC/Clang, with Linux
ASan/UBSan. The suite includes 58 public tests, 12 original comparisons and the
numerical Java oracle. Times are 744.00, 1255.43 and 1256.05 seconds respectively.
Independent audits bind source, tools, executables/build inputs, original
captures and complete supported copied-world state/control/byte projections.
All 74 Java-free world-harness guards pass. Hosted checks pass 59 tests per
compiler and clang-format18 for 111 C/header files. See the
[exact evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md#exact-source-storage-checkpoint)
for capture/report hashes and excluded attempts. These finite profiles do not
establish gameplay save/unload scheduling, subtype simulation or a running C
server. Older checkpoint counts below retain their original source and scope.

## Lifecycle checks

`MC_ENABLE_LIFECYCLE_REFERENCE_TESTS=ON` selects ten serial fresh
original/control profiles and native comparisons. Supply both verified JARs,
`Java_JAVA_EXECUTABLE` and `Java_JAVAC_EXECUTABLE` from one supported JDK17/21
installation, plus `MC_REFERENCE_TESTED_COMMIT`. After building, run:

```sh
ctest --test-dir <build> -C Debug -R '^reference.lifecycle_' --output-on-failure
```

Public `world.lifecycle_callbacks_replay` needs no original archives or Java.
Other public contracts cover flags/cutoffs, live mutation, history-sensitive
pending selection, leases, retirement, generation retention and capacity recovery.
The standard-library Integer-set oracle is enabled separately by
`MC_ENABLE_JAVA_REFERENCE_TESTS=ON`. Its expectations do not establish Java6 or
original game behavior.

A separate JDK17+ regression uses synthetic rooted objects and no original assets:

```sh
python tests/tools/cache/check_controller_mirrors.py --work-dir build/controller-mirrors
```

Both marker and pre-label cases pass on Windows JDK17 and Linux JDK21. Required
controls prove disposal of 128 discarded mirrors, stable anchor IDs, bounded
retention and duplicate-label rejection. Missing tools and inconclusive
collection fail this command. CI runs it separately from CTest.

The Python-only interval regression also needs no original assets:

```sh
python tests/tools/cache/check_lifecycle_intervals.py
```

It compiles the actual validator checks and verifies their role/profile and
operation bindings. Both native hosts pass 556 positive/2011 negative cases,
including the retained operands that failed both old checks. Only observed
dedicated101 intervals permit 600 seconds; other profiles, controls, preparation
and constructors retain 180 seconds. Events must remain inside their own
sampled interval. CI runs this regression separately from CTest.

The exact qualified selection is the complete Windows v10, GCC v9 and Clang v9
suites at source `46546d8`. Each passed 92 tests without failures or skips:
1940.77s on MSVC, 2969.23s on GCC and 2961.75s on Clang. Windows v9
is excluded after its original chunk-data setup refusal; its rows cannot fill
any gap in the fresh Windows v10 suite. Independent source/semantic audits,
storage rechecks and capture preservation also pass, with Linux ASan/UBSan.
Exact source, report hashes and scope are recorded in the
[lifecycle ledger](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lifecycle.md#exact-source-lifecycle-checkpoint).
Dedicated `unload101` alone has 600/660-second controller/exporter allowances.
Other observed profiles retain 300/360 seconds and control/preparation processes
retain 180 seconds. Graph, data, process and comparison requirements remain
unchanged. Timeouts and skipped prerequisites are never passing parity results.

## Normal generation checks

`world.normal_generator` passes 99073 public contract checks on native MSVC and
WSL GCC/Clang; sanitizer checks also pass. They cover borrowed ownership,
unpublished identities, repeated outputs, current materials, finite-coordinate
preflight and allocation-failure preservation. Four-seed original capture and
lossless public C replay pass eight inputs, 72 complete chunks and 73728 exact
height queries. Exact source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes
all 41 selected local tests without skips on those toolchains. Each generation
comparison retains 1224 stage events, 144 marker inventories and eight complete
byte-equal untouched controls, with all three pairings passing independently.

`MC_ENABLE_GENERATION_REFERENCE_TESTS=ON` enables `reference.normal_generation`
with JDK17+ and `jdk.jdi`, both supplied JARs and the exact source commit setting.
It uses fresh actual original contexts, loopback debugging and separate
untouched controls. The corpus uses seeds 23, 24, 26 and 51, retaining
one generator across nine calls per side. It includes repeated, negative,
packed-key-alias, world-border and accepted cave-domain endpoint coordinates.
Complete data, queues, climate/scratch, 82 layers, separate Random continuations
and all 1024 height queries per call form the comparison boundary.

After configuring the reference inputs, use these production commands:

```sh
cmake --build build --target mc_normal_generation_probe
ctest --test-dir build -R '^tools\.normal_(reference|interop)$' --output-on-failure
ctest --test-dir build -R '^reference\.normal_generation$' --output-on-failure
```

Add `-C Debug` for multi-configuration builds. The public
`tools.normal_reference` and `tools.normal_interop` checks use synthetic guards
and retained original fixtures respectively, without Java or private inputs.
At the a08 checkpoint, the suite has 35 public tests, 36 with the numerical
Java oracle, or 41 with the selected original finishing, queue, chunk, nibble
and generation comparisons. [Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass all 36 public/numerical tests per compiler on native Windows MSVC and Linux
GCC/Clang, plus formatting for 79 C/header files. These counts identify that
exact checkpoint; later cache/region additions need separate acceptance. See the
[generation contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/generation.md)
and [evidence boundary](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/generation.md).

## Acquisition and region checks

The cache coordinator adds 838 public contract checks. Its retained-provider
fixture replays three continuous original contexts, 20 operations, five generated
chunks and 5120 exact height queries. The public protocol and fixture checks need
neither Java nor original archives. `MC_ENABLE_CACHE_REFERENCE_TESTS=ON` enables
`reference.chunk_acquire` and `reference.chunk_callbacks` with JDK17+, `jdk.jdi`,
`jdk.jdwp.agent`, both supplied archives and the exact source commit setting.
The callback fixture adds four contexts, 12 calls, 394 lifecycle events and 24
inventories. Public checks cover 34 guards, four genuine C replays, four
constructed cases and 28 additional C guards. These replay observed cache/world
effects while C computes coordinator transitions. Exact source
`28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 selected tests without
skips on MSVC/GCC/Clang with Linux sanitizers and independent audit. Hosted checks
pass 48 tests per compiler and formatting for 89 C/header files. See the
[cache evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache.md).
The separate [callback evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache-callbacks.md)
records saved-world preparation, ownership and explicit external-effect limits.

The read-only region reader has 49771 public checks. At source
`092301ae8282d50a734df9018f791925aa9be99b`, `reference.region` passes on native
MSVC and WSL GCC/Clang with Linux sanitizers. Each capture retains 36 fixtures,
49 queries per side and two byte-equal untouched controls. Eight queries are
compatible; 41 retain explicit bounded diagnostic departures.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39082649705)
pass 42 public/numerical tests per compiler and formatting for 87 C/header files.
See the [region evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/region.md).

Expanded source `9a50ac67997082c89dbc43cf4274fba1427c1565` passes all 52
selected local tests without skips on MSVC/GCC/Clang with Linux sanitizers and
independent exact-source audit. The suite contains 44 public tests, the numerical
Java oracle and seven selected original comparisons.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39085300876)
pass 45 tests per compiler and formatting for 88 C/header files.

Missing references are unavailable/skipped results, never parity passes. Pin the
source commit when recording comparisons. See the
[testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md)
and [storage/reference overview](Storage-and-reference).

The prior finishing checkpoint passes 37 local tests per compiler on native
Windows/MSVC and WSL GCC/Clang, including original finishing, queue, chunk and
nibble comparisons. The Linux builds use ASan/UBSan. All six finishing cases
match independently in three pairings; complete untouched controls agree.
Its public CI passes 33 tests per compiler and formatting for 75 C/header files.
Exact tested commits, manifest hashes and integration results live in the
development status and linked evidence pages.

Original resources are never bundled. The
[resource guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/resources.md)
describes local import into a fresh ignored directory and its current limits.

There is currently no C game executable or C server to host. The storage
library's explicit save operations pass the finite storage comparisons above;
complete original-compatible gameplay save workflows remain unverified. Client controls
and Linux/Windows hosting, backup and shutdown instructions will be added with
verified game implementations.
