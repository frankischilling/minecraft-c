# Build and tests

The [World/Player integration](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
passes all 265 Windows tests and all 247 headless tests each on fresh sanitized
Clang/GCC builds, with no failures or skips. All 745 C/header files pass
formatting. Real original comparisons cover finite World lighting, Player
publication/input and provider maintenance. Full startup and gameplay remain
unfinished; the detailed contracts qualify every comparison.

The [Player constructor checkpoint `42b9169`](https://github.com/frankischilling/minecraft-c/actions/runs/40232088249)
passes all four hosted jobs: 244 Windows, 250 GCC and 250 Clang tests, with
744 complete Start/Passed pairs. Every checkout tree equals that code head.

The [local Player constructor](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/player-construction.md)
passes full ABI rebuilds and 254 Windows tests plus 236 headless tests each on
sanitized Clang/GCC, without failures or skips. Fresh JDK 17 normal/JDI runs
verify actual completed and failed receivers; production Math entropy remains
separate from scalar replay. All 730 C/header files pass formatting.

The [crafting checkpoint `d113527`](https://github.com/frankischilling/minecraft-c/actions/runs/40224136061)
passes all four hosted jobs: 242 Windows, 248 GCC and 248 Clang tests, with
738 complete Start/Passed pairs. Every checkout tree equals that code head.

The [crafting integration](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/inventory/crafting.md)
passes three selected tests on MSVC and sanitized Clang/GCC. Each product matches
1,153 original rows per source side across 1,047 cases and passes 48 strict
protocol checks. The full Windows suite passes all 251 tests without failures
or skips; all 723 C/header files pass formatting.
Crafting-output effects remain unfinished; the later Player constructor has
separate coverage above.

The [input/turn checkpoint `5143b58`](https://github.com/frankischilling/minecraft-c/actions/runs/40218311052)
passes all four hosted jobs: 240 Windows, 246 GCC and 246 Clang tests, with
732 complete Start/Passed pairs. All checkouts use merge `1d012bd6`; its tree
equals that code head. The later crafting increment has separate validation.

The [renderer/audio checkpoint `3d26621`](https://github.com/frankischilling/minecraft-c/actions/runs/40213051132)
passes all four hosted jobs: 237 Windows, 243 GCC and 243 Clang tests, with
723 complete Start/Passed pairs. All checkouts use merge `17ad0d82`; its tree
equals that code head.

The next input/turn and allocation-error increment passes 248 Windows tests
and 250 tests each on sanitized Clang/GCC, without failures or skips.
MovementInput matches 857 original records; Entity turning matches 3,330,
including raw NaN bits. All 707 C/header files pass formatting. Actual Player
construction and the World gameplay loop remain unfinished.

The frozen World/audio increment passes 243 Windows MSVC tests and 245 tests
each on sanitized Ubuntu Clang 18/GCC 13, without failures or skips. It includes
60 exact original WAV profiles, exact CPU face/AO/family comparisons, real
OpenAL loopback/workers and three actual client captures with authored Worlds.
All 697 C/header files pass clang-format 18; controller/lifecycle checks pass.
The final color-map error fix passes three focused World tests per compiler.
See [test scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md),
[World rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-renderer.md)
and [audio backend](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/audio-backend.md).
The scene remains a fixed inspection, without Player gameplay.

SoundManager now passes 20,066 Windows native checks and 62 separate settings
integration checks. Its backend recorder verifies call ordering and genuine
nested saves; no device playback is claimed. The source-policy Vorbis codec
matches 14 fresh original/native page/buffer profiles. All 21,248 stereo PCM
bytes match; short/long mono synthesis differences remain. See the
[manager contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/sound-manager.md)
and [codec evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/vorbis-codec.md).

The [nonzero sound checkpoint `aa39f8f`](https://github.com/frankischilling/minecraft-c/actions/runs/40190426923)
passes all four hosted jobs: 229 Windows, 233 GCC and 233 Clang tests, with 695
complete Start/Passed pairs and no failures, skips or missing cases. All 658
C/header files, controller mirrors and 556 positive/2,011 negative lifecycle
checks pass. The four checkouts use synthetic merge `839f22e`, with a complete
tree equal to that head. Independent readback reconstructs 331 trees/1,566
entries and joins all 1,236 literal files and 1,056 Windows-tested non-Markdown
inputs.

The [sound checkpoint `e10a5e2`](https://github.com/frankischilling/minecraft-c/actions/runs/40171237434)
passes all four hosted jobs: 228 Windows, 232 GCC and 232 Clang cases, with 692
complete Start/Passed pairs and no failures, skips or missing tests. Formatting
covers 657 C/header files; controller mirrors and all 556 positive/2,011 negative
lifecycle controls pass. All checkouts use synthetic merge `ec63077`, with a
complete tree equal to that head. Independent readback reconstructs 331 trees
and joins all 1,234 literal source files and 1,054 Windows-tested non-Markdown
inputs. Original PCM and device playback remain unqualified.

The [option-action checkpoint `2afd39a`](https://github.com/frankischilling/minecraft-c/actions/runs/40154472178)
passes all four hosted jobs: 225 MSVC, 229 GCC and 229 Clang cases, 683 complete
Start/Passed pairs, with zero failures, skips or missing runs. All 645 C/header
files pass formatting; mirror/lifecycle guards pass. Every host runs the native
option-action test. The four actual checkouts share synthetic merge `c37ce73`,
whose complete tree equals that head. Independent readback reconstructs 328
trees and joins all 1,215 literal committed bodies and 1,037 non-Markdown tested
inputs. Original actions, real effects and complete client behavior remain open.

The [sound foundation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/sound.md#verified-native-scope)
passes six selected Windows MSVC x64 Debug tests: 18,654 pool, 581 file, 579
Vorbis, 5,480 nonzero Vorbis, 43 integer-parser and 191 Random-profile checks.
All seven stages and eleven audits pass; independent readback consumes 14,431
retained files, and all 658 C/header files pass formatting. Authored decoder
cases cover mono, stereo,
MUS, a format-changing chain and real packet holes. No audio device or original
PCM comparison runs. At that checkpoint, shared SoundManager, playback
and real client integration remained unfinished. The 683-case hosted result above
predates these sound modules.

Separate nonzero vectors exercise the production converter and planar suffix
helper. The earlier silence-fixture [sanitized sound CI](https://github.com/frankischilling/minecraft-c/actions/runs/40169607338)
passes 228 Windows and 232 GCC cases, plus 657-file formatting. Clang passes
231 of 232 tests; decoder checking emits a libogg null-pointer-offset report
despite exiting 0. The strict stderr assertion rejects it. An exact-commit
Ubuntu Clang 18 build reproduces the report. The private Ogg copy now guards
that empty-buffer pointer. Earlier Vorbis corrections pass the focused Ubuntu
GCC ASan/UBSan decoder driver with 563 checks, seven authored inputs and four
exact PCM outputs, with independent file readback. Hosted qualification of
the Ogg correction is covered by the later 692-case result above. A fresh corrected
Ubuntu Clang ASan/UBSan decoder run passes all 563 checks and independent file
readback, with empty child stderr.

The new `client.vorbis_nonzero` regression first exposes a negative signed shift
in libvorbis floor encoding. A pinned private-copy multiplication correction
passes 5,469 nonzero checks on Clang 18 ASan/UBSan, alongside the existing 563
decoder checks. Independent readback checks all 1,236 source members and four
nonzero PCM outputs, including the exact chain join. The fresh Windows profile
above passes 5,480 checks in this regression and independent capture readback.
These are native codec contracts, without original PCM or device comparison.

A separate [arranged original codec control](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/client-sound.md#arranged-original-codec-observation)
initializes but returns null/EOS for the authored split-page mono stream. Its
later readAll returns a SoundBuffer with null data. Native decoding emits
6,400 PCM bytes from the same input. This policy difference remains open;
the completed observation does not qualify floating synthesis or PCM equality.

A separate [nonzero original mono comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/client-sound.md#nonzero-original-pcm-observation)
returns 8,192 PCM bytes on both sides. Four of 4,096 samples differ by one
signed16 step; exact equality fails. Independent readback passes 2,368 final
file/tree checks and retains every differing value and both complete-output
hashes. No tolerance or prefix-only acceptance is applied. The numerical cause
and conversion-entry float comparison remain unresolved.

The preceding [code checkpoint `ea1aefc`](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841)
passes all four hosted jobs: 224 MSVC, 228 GCC and 228 Clang cases, or 680
complete Start/Passed pairs with zero failures, skips or missing runs.
Formatting covers 644 C/header files; mirrors and all 556 positive/2,011 negative
lifecycle checks pass. All four actual checkouts use synthetic merge `2be1dc6`
with a complete tree equal to the head. The audit rejoins all 1,213 literal
publication bodies and 1,035 non-Markdown tested inputs. GameDirectory passes on
all three compilers and startup on Linux, alongside the binding/query/label
contracts. Linux requests software OpenGL/Xvfb without recording an actual
vendor; hosted JUnit and artifacts are not retained. Original-label,
original-action, original-startup and ordinary-default comparisons remain open.

The newer `client.game_settings_actions` test passes 702 native checks and
eight independent file-check categories in one Windows MSVC x64 Debug headless
profile. The same profile passes 548 settings checks plus six file checks and
883 GameOption checks: three CTests, seven stages and eleven final audits.
All 645 C/header files pass formatting before execution. Tests exit 0 naturally
and are reaped; configure/build keep the explicit pinned-vctip allowance.
An independent file-only readback rejoins 2,788 retained members, eight products
and two capture trees from the 1,215-member/1,037-non-Markdown tested source.
No new client-mode or Linux action profile ran. Six audio/renderer paths stop
after mutation at missing receiver fields. Original actions, real effects and
Options UI remain unfinished; the preceding 680-case CI excludes these bodies,
while the later 683-case checkpoint includes their native contracts.

The [App startup connection](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/startup.md#settings-at-startup)
now loads real GameSettings after Display and uses loaded scale and initial
anaglyph before title consumers. A retained
[GameDirectory](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-directory.md)
owner supplies actual path selection and creation. Startup and retirement
perform no options save; native failures do not choose a second directory.
Fresh Windows MSVC x64 Debug headless/client profiles pass six selected CTests:
57 directory checks plus six file checks, 548 settings checks plus six file
checks, 883 option checks, 100 display checks, 53 scene controls and nine App
cases with eight exact decoded-pixel relations on NVIDIA RTX 3060. Both profiles
finish seven stages and eleven final audits; test trees close naturally while
configure/build retain their explicit pinned-helper cleanup allowance. The
tested family has 1,213 members and 1,035 non-Markdown inputs. The earlier
fixture C4310 build failure remains failed; [testing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#startup-settings-and-game-directory)
records its correction and precise formatting/readback scope. The hosted run
above adds Linux startup execution; original/default-directory correspondence,
custom packs, audio, source pacing, child menus and complete startup remain
unfinished.

[GameOption queries and labels](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-options.md)
passed six earlier local Windows MSVC cases: 883 metadata/query, 1,241 label, 463 Assets,
4,404 Translation and 481 persistence checks, plus six file checks and 26 probe
controls. All 641 C/header files pass clang-format 18.1.3. A separate file audit
qualifies the completed headless tests after their controller's summary-parser
failure; the failed report and missing after-test inventory remain explicit.
The corrected client profile closes all seven stages. The hosted checkpoint
above supplies the later Linux native coverage without relabeling that earlier
failed report. Original-label comparison remains pending.

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
retains the finite scope. Original static-label correspondence, options screens
and effectful setters remain unfinished. The new App connection has its own
finite Windows tests and is outside those original persistence comparisons.

The new [binding-code action](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-settings.md#binding-code-action)
stores the code in the current array's binding, then reaches the real save even
when the code is unchanged or the noarg File is null. Its report separates
uncaught source null/index failures from native failures and the nested save's
caught errors. Earlier code/file effects survive later failure. The raw field
edit remains a no-save operation. Added alias, real-file, partial-prefix and
diagnostic reentry tests pass on Windows MSVC. Fresh headless/client profiles
pass six CTests with 548 native settings checks, six independent file checks,
26 controls and the option/label/Assets/Translation regressions. Both profiles
close all seven stages and ten final audits; test trees exit naturally while
configure/build retain their pinned compiler-helper cleanup allowance. All
641 C/header files pass formatting. The later hosted run at the top also
passes native action contracts on Linux GCC/Clang. Original-action comparison
and Controls UI adoption remain pending.

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
