# Build and tests

[GameOption queries and labels](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-options.md)
pass six actual Windows MSVC cases: 883 metadata/query, 1,241 label, 463 Assets,
4,404 Translation and 481 persistence checks, plus six file checks and 26 probe
controls. All 641 C/header files pass clang-format 18.1.3. A separate file audit
qualifies the completed headless tests after their controller's summary-parser
failure; the failed report and missing after-test inventory remain explicit.
The corrected client profile closes all seven stages. Linux validation and
original-label comparison remain pending.

The [GameSettings checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40129932174)
`adb573d` passes all four hosted jobs: 221 MSVC, 224 GCC and 224 Clang tests,
or 669 complete Start/Passed pairs, with zero failures, skips or missing runs.
All 635 C/header files pass formatting; mirrors and lifecycle interval checks
pass. The actual synthetic checkout has the same complete tree as the head,
and all 1,199 literal source bodies match the published snapshot.

The [writer code checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40117293002)
`389b573` passes all four hosted jobs: 218 MSVC, 221 GCC and 221 Clang tests.
All 660 cases passed without skips, alongside 630-file formatting, controller
mirrors and lifecycle interval checks. A compile-time Font assertion resolves
MSVC 19.44's C4127 diagnostic without relaxing strict warnings.

The [shared output writer](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/core/java-writer.md)
passes 110,708 stream checks, 19 real-file checks and five independent file
checks per compiler within 15 selected headless cases. Linux uses sanitizers.
At that checkpoint, all 630 C/header files pass formatting. Independent raw-output audits and
later metadata inspections qualify the successful MSVC/GCC tests while
retaining their JUnit-footer controller failures. Clang's complete controller
passes. The later writer observer passes 25 transport controls per compiler.
A finite Windows original/native comparison matches 13,391 fields across 68
scenarios; all 14 corruption controls reject the intended changes. Thread and
private-state evidence remains unpaired.

The [GameSettings persistence owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-settings.md)
adds real-file load/save without graphics dependencies. Both new CTests pass
on MSVC and sanitized GCC/Clang: 481 native checks, six independent file checks
and 26 observer controls per compiler. Formatting passes for all 635 C/header
files. A separate file audit rehashes 1,706 retained artifacts and actual
products. Separate Windows-1252/CRLF and explicit UTF-8/CRLF comparisons each
match all 11,389 persistence fields in 16 cases, including 64 paired stages
and 2,877 typed reference joins. The separate GCC Linux UTF-8/LF comparison
also matches; Clang Linux comparison remains pending. All 14 file-only controls
on the Windows-1252 baseline reject their intended changes. The
[reference record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/game-settings.md#finite-native-comparisons)
retains the finite scope. Static labels, application integration, options
screens and effectful setters remain unfinished.

The [native frontend](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/startup.md)
passes eight focused tests on MSVC and sanitized GCC/Clang, including actual OpenGL
contexts, and a bounded 200-frame run per compiler. Earlier Windows controls also
cover window close, Quit, and resize to 512x384 followed by Quit. Linux CI
enables the display test under Xvfb with software Mesa; a missing context
fails the test.

The later shared title renderer replays ordered binary32 matrix operations.
Its conditional Windows/NVIDIA image matches all 409,920 original RGBA pixels.
Linux retains one differing pixel against that Windows baseline; the cause
remains unresolved. All eight focused cases pass per compiler, including
100 actual display checks. Its ordered FontRenderer interface adds 262,377
checks while retaining the existing 220 glyph checks. The
[legacy float formatter](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/core/java-float-format.md)
matches 5,708 retained JDK rows and 49,532 separate model cases per compiler
through the public core library. All 13 selected client-disabled tests pass
per compiler. These finite results do not complete options persistence or
the client.

The preceding complete snapshot passed 211 MSVC and 213 Clang cases.
GCC passed 212 of 213 initially; its strict named-data replay rejected an
observed 1,253 ms backward wall-clock step. One unchanged-code retry with
fresh OS samples passed. The failed capture remains failed. The final
minimized-window change then passed focused frontend tests and captures on
all three compilers. These finite results do not establish playability or
general original rendering parity. See the
[test scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
and [original title evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/client-title.md).

The [canonical naming build](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecraft-names.md)
passes all 619 public/JDK tests: 205 MSVC, 207 GCC and 207 Clang. Linux enables
ASan/UBSan. Formatting covers 587 C/header files; controller mirrors and lifecycle
interval controls pass on both platforms. Eight separate file-only controls
check historical/current module families and immutable recorder provenance.
The complete retained naming source has 1,130 files, with its non-Markdown
inputs unchanged after testing. Earlier checkpoints below retain their original counts.

Dedicated constructor, Entity-index and public fixture/replay tests run without
original archives. `tools.population_dedicated` runs eighteen Python checks;
`world.population_dedicated_replay` runs the production probe and eight
actual-native corruption controls. The population reference option also enables
`reference.population_dedicated` when both archives and JDK 17 are configured.
That workflow captures and audits the original before comparing a new native
run. The fluid contract also checks a synchronous enclosed column on both sides,
including every block/metadata result, exactly 34 schedule requests and
unchanged Random state. See the
[test commands and limits](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#dedicated-construction-and-synchronous-fluid-recursion).

The [dedicated checkpoint](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/dedicated-world-construction.md#committed-checkpoint)
at `9c87827` passes all 622 local executions: 206 MSVC, 208 GCC and 208 Clang,
with zero failures, errors or skips. All 1,099 source files match their literal
committed Git blobs. Source, selected tools and products remain unchanged across
testing. Linux enables ASan, UBSan and leak checks. Every compiler passes fresh
schema-2 original audits, complete native comparisons and 18 original/18 native
actual Store-token joins. Independent retained audits repeat those checks.

The [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40000554912)
passes all four jobs and 601 public/JDK tests: 199 Windows, 201 GCC and 201 Clang.
All actual checkouts use a merge commit whose entire tree equals the code
checkpoint. All 569 C/header files pass clang-format 18. Mirrors and 556
positive/2,011 negative lifecycle-interval controls pass on every local and
hosted build. Seven supplied-archive tests per compiler retain their local scope.

The earlier 203/205/205 development suites and the original-only fixture retain
their dirty source and null tested commits. The reference record corrects the
historical source-byte claim. Empty Entity/TileEntity/named-data/index collections and
the finite seed23 schedule leave broader gameplay, server startup and general
restart outside this checkpoint.

Build setup uses C17, CMake 3.24+ and Python 3.10+; ordinary public tests need
no Java or original archives. Follow [building](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/building.md) for
Linux and Windows tools, dependency setup and separate build directories.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The preceding code checkpoint `5b57bc1` passes 199 tests with MSVC 19.51.36260.0, 201 with
GCC 13.3.0 and 201 with Clang 18.1.3: 601 executions with zero failures, errors
or skips. Every compiler includes five fresh population profiles, their public
native replays and the plant comparison. Linux enables ASan, UBSan and leak
checks. All 559 C/header files pass clang-format 18. Mirrors and lifecycle
intervals pass on all three hosts, including 556 positive and 2,011 negative
interval controls.

The independent [source and capture audit](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-named-data.md#committed-checkpoint)
verifies 1,073 clean-source files, all 30 fresh/public population captures and
30 resource-variant pairings. The [hosted code run](https://github.com/frankischilling/minecraft-c/actions/runs/39964552445)
passes all four jobs and 583 public/JDK tests: 193 MSVC, 195 GCC and 195 Clang.
Formatting and every mirror/interval check pass; all four actual checkout trees
match the code checkpoint. Six supplied-archive comparisons per compiler retain
their local scope.

Optional `reference.natural_population` and `reference.natural_population_ring`
compare fresh original/native origin and ring profiles on both sides. Ring
retains nine normal Chunks, populates four and ends with 6,968 ordered light
regions. The [fresh World record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/natural-population.md)
defines the full cache, numerical-array, alias and Random projection.

Public `world.natural_population_replay` checks the older cache oracle;
`world.natural_population_ring_replay` also checks full original numerical
golden records. Both run the real native probe with declared minimal resources
and require no Java or archives. Select both fresh comparisons with
`-R '^reference\.natural_population(_ring)?$'` after enabling
`MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`; the
[test commands](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#fresh-world-population)
include the separate public replay commands.

The same population reference flag also enables
`reference.population_save_readback`. Public
`world.population_save_readback_replay` needs no Java or archive, and
`tools.population_persistence` checks corruption controls. The
[save/readback record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-persistence.md)
defines its twenty complete boundaries, disk/NBT projection and remaining full
World save, unload and restart requirements.

`reference.population_full_save` and public `world.population_full_save_replay`
exercise two actual full saves after origin population with empty Player and
named collections. Eleven corruption tests protect that profile's lossless
original oracle. The [full-save record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-full-save.md)
covers those collection owners, typed metadata and exact rotation.

The same population reference flag also enables `reference.population_named_data`.
Public `world.population_named_data_replay` needs no Java or archives;
`tools.population_named_data` runs 24 Python tests with 132 rejection controls.
The [named MapData profile](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-named-data.md)
adds one default dirty value, two full saves and a separate manager readback.
Each side checks 24 numerical boundaries, 72 copied Random records and
360 array descriptors, complete named graphs and all saved typed NBT/files.
Nonempty Players, map updates/trackers/packets, other subtypes, source I/O
failures, unrestricted ordering, dedicated runtime and restart remain open.

The plant comparison has 59 client and 59 server cases, with zero mismatches. Its
[declared projection and exclusions](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/plant-lifecycle.md)
bound this result; it does not establish original World/Chunk parity.

Public `world.plant_lifecycle` and `world.plant_lifecycle_replay` need no Java.
Replay uses original-derived fixtures: 59 cases, twelve semantic corruption
guards and 154 malformed-input guards per side. For fresh captures, enable `MC_ENABLE_PLANT_REFERENCE_TESTS=ON`,
supply both verified JARs and select Java/javac from one JDK17+ installation.
The [test commands](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#plant-lifecycle-validation) list the
JAR, tool and tested-source options.

See [testing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md), [development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md),
the [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md) and
[World simulation](https://github.com/frankischilling/minecraft-c/wiki/World-simulation) for the remaining acceptance work.
The [plant contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md) separates implemented callbacks
from the unfinished game loop.

<a id="normal-generation-checks"></a>

The [complete former build page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md), including its
[normal-generation checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md#normal-generation-checks),
preserves older counts, commands and checkpoints as historical evidence.
