# Development status

The sound checkpoint `e10a5e2708e81c81ea2cc9b721c060a2d2f056d7`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40171237434):
228 Windows, 232 GCC and 232 Clang tests, with 692 complete Start/Passed pairs
and no failures, skips or missing cases. Formatting covers 657 C/header files;
controller mirrors and all 556 positive/2,011 negative lifecycle checks pass.
The four checkouts use synthetic merge `ec63077`, whose complete tree equals
that head. Independent readback reconstructs 331 trees and joins all 1,234
literal published files and 1,054 Windows-tested non-Markdown inputs.

The earlier option-action checkpoint `2afd39a70935a3ae167f7cb7fad6452392828c6d`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40154472178):
225 MSVC, 229 GCC and 229 Clang cases, 683 complete Start/Passed pairs with zero
failures, skips or missing runs. Formatting covers 645 C/header files; controller
mirrors and lifecycle guards pass. All four actual checkouts use synthetic merge
`c37ce7331a8bf643587081c7ae07e5e2a117797b`, with a complete tree equal to that head.
Independent readback reconstructs 328 trees and joins all 1,215 literal committed
bodies and 1,037 non-Markdown tested inputs. Native option actions run on every
host; original actions and real effect owners remain unfinished.

The [sound resource foundation](client/sound.md) owns SoundPool entries, resolved
URLs, raw/MUS streams and sequential Vorbis decoding. A Windows MSVC x64 Debug
headless run passes 18,654 pool, 581 file, 579 decoder and 5,480 nonzero decoder
checks, alongside 43 integer-parser and 191 Random-profile checks. All seven
stages and eleven audits pass; independent readback consumes 14,431 retained
files and joins all 1,236 public members. Formatting covers all
658 C/header files. These authored contracts initialize no audio device and
make no original PCM comparison. SoundManager, WAV, backend playback, real
client adoption and original PCM correspondence remain unfinished.

The [sanitized sound checkpoint `184a6ce`](https://github.com/frankischilling/minecraft-c/actions/runs/40169607338)
passes 228 Windows and 232 GCC cases, plus 657-file formatting. Clang passes
231 of 232 tests; only decoder checking fails, on libogg's empty-buffer pointer
arithmetic. Exact-commit Ubuntu Clang reproduction confirms the report.
The private Ogg copy now keeps that page pointer null, preserving the normal
need-more-data result and the unchanged supplied source. Earlier private Vorbis
corrections pass the focused Ubuntu GCC decoder driver with 563 checks and
exact authored PCM. With the Ogg correction, a fresh Ubuntu Clang ASan/UBSan
decoder run also passes all 563 checks and its independent file readback.
The later 692-case checkpoint above qualifies these native contracts on all
three compilers. It does not qualify device playback or original PCM.

A new nonzero Vorbis regression reproduces a signed floor-deviation shift in
libvorbis. The pinned private-copy correction passes a fresh Clang 18
ASan/UBSan run: 5,469 nonzero checks and all 563 existing decoder checks.
Independent readback verifies four authored encoded inputs, four nonzero PCM
outputs, exact chain joins and the complete 1,236-member source snapshot.
The fresh Windows profile above passes the same regression with 5,480 checks
and independent source, product, stream and capture readback.

An arranged original CodecJOrbis mono control initializes successfully but
returns null and reaches EOS on its first read. A later readAll returns a
SoundBuffer with null data. The same authored split-page input produces 6,400
PCM bytes through the generic native decoder. The [reference note](reference/client-sound.md#arranged-original-codec-observation)
retains this output-policy difference; it supplies no floating-synthesis match.

The preceding code checkpoint `ea1aefc952739fd6e7fba023646039856324e721`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841):
224 MSVC, 228 GCC and 228 Clang tests. All 680 cases have complete Start/Passed
records with zero failures, skips or missing runs; 644 C/header files pass
formatting. Controller mirrors and all 556 positive/2,011 negative lifecycle
checks pass. All four actual checkouts use synthetic merge `2be1dc6`, whose
complete tree equals the head. The audit rejoins all 1,213 literal source bodies
and 1,035 non-Markdown tested inputs. This includes GameDirectory on all three
compilers and startup graphics tests on Linux, which request software OpenGL
under Xvfb without retaining an actual vendor measurement.

The newer [float/discrete option actions](client/game-options.md#float-and-discrete-actions)
pass one Windows MSVC x64 Debug headless profile: 702 native checks and eight
independent file-check categories, alongside 548 persistence checks/six file
checks and 883 GameOption checks. All three CTests, seven stages and eleven
final audits pass; all 645 C/header files pass formatting. The tested family
has 1,215 public members and 1,037 non-Markdown inputs. An independent file-only
readback rehashes 2,788 retained members, eight products and two capture trees.
No new local client-mode or Linux action profile ran. Six audio/renderer
paths retain assignment then stop at their first missing field; actual effects,
Options UI and original-action correspondence remain unfinished. The preceding
680-case CI result excludes these bodies; the later 683-case result above
includes their native contracts.

The App now loads one real file-backed GameSettings owner after Display and
before font/menu consumers. [GameDirectory](client/game-directory.md) performs
native path selection and real creation; load failures keep defaults and any
partial field changes without choosing another directory. Loaded GUI scale
and initial anaglyph feed the title, and explicit GUI-scale overrides include
zero. Startup and retirement do not save. Fresh Windows MSVC x64 Debug profiles
pass six selected CTests: 57 directory checks plus six file checks, 548 settings
checks plus six file checks, 883 option checks, 100 display checks, 53 scene
admission controls and nine App cases with eight exact decoded-pixel relations.
Both profiles finish all seven stages and eleven final audits. App runs use an
NVIDIA RTX 3060 core context and exit naturally. These results cover the frozen
1,213-member source, including 1,035 non-Markdown inputs. The hosted result
above adds Linux startup execution; ordinary default selection, broader
File/locale behavior and original startup/pixel correspondence remain unqualified.

Earlier code checkpoint `adb573dbc6998dcd5fabf72f81e2342e0feb147c` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40129932174):
221 MSVC, 224 GCC and 224 Clang tests. All 669 cases have complete Start/Passed
records, with zero failures, skips or missing runs. Formatting covers all 635
C/header files; controller mirrors and lifecycle interval checks pass. The
actual synthetic checkout has the same complete tree as that head, and all
1,199 literal source bodies match the published snapshot. The separate
original/native persistence comparisons below retain their own finite scope.

At code checkpoint `389b57361d9454b447c0f956cc39c5a1d32245b3`,
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40117293002)
pass: 218 MSVC, 221 GCC and 221 Clang tests, with no missing cases or skips.
Formatting covers 630 C/header files; controller mirrors and lifecycle interval
checks also pass. Linux includes actual software OpenGL tests and ASan/UBSan.
An earlier Windows build rejected a constant test condition under MSVC
19.44's C4127 diagnostic. A compile-time assertion replaces that condition;
strict warnings remain enabled. The earlier image flexible-array diagnostic
was resolved with a pointer into the same checked allocation.

The [native client](client/startup.md) draws the title screen on Windows with an
NVIDIA RTX 3060 and on Linux through x11 with Mesa llvmpipe. A later focused
checkpoint passes all eight frontend tests on MSVC and sanitized GCC/Clang,
including 156 resource checks, 96 Timer checks, 100 real OpenGL checks and the
scene parser controls. The ordered FontRenderer interface adds 262,377 checks
while preserving the existing 220 glyph checks. Each platform also completed a
bounded hidden 200-frame invocation with an actual exit0 and unchanged source,
assets and binary hashes. Separate earlier Windows controls passed window
close, Quit, and a resize to 512x384 followed by Quit, using the actual owned
hidden window.

The preceding complete source snapshot passes all 211 MSVC and 213 Clang cases.
GCC passed 212 of 213 initially; the remaining strict named-data replay passed
once with unchanged code and fresh OS samples. The first capture retained a
measured 1,253 ms backward wall-clock step and remains failed at its chronological
admission guard. No timestamps or comparisons were relaxed. The final
minimized-window adjustment was then rebuilt and retested through the six
frontend cases and actual captures on all three compilers. Earlier full suites
below retain their own checkpoint scope.

Child menus, effectful settings setters, audio, complete startup, gameplay and real
fullscreen/zero-drawable controls remain unfinished or unverified. The shared
[GuiMainMenu renderer](client/gui-main-menu.md) supports a matched conditional
scene with ordered binary32 modelview operations. Windows now matches all
409,920 original RGBA pixels in that scene. Linux/Mesa retains one differing
pixel against the original Windows/NVIDIA capture; the cause remains unresolved.
An original title draw and exact clock witness have
been observed in an arranged receiver; the first helper's teardown failed.
The second helper completed with natural target/observer exits; its controller
failed a JDI version-property check. A separate corrected file-only audit
passed without a redraw. The [reference note](reference/client-title.md)
records that lineage. These results do not qualify complete original startup
or rendering equality.

The [counted float parser](core/java-float.md) constructs binary32 values with
bounded integer arithmetic. Standalone builds on MSVC/GCC/Clang each pass 100
unit checks and 5,632 probe records. Public client-disabled integration also
passes 13 selected tests and the complete corpus per compiler, without graphics
targets or direct imports. The [legacy float formatter](core/java-float-format.md)
also matches 5,708 retained JDK rows and 49,532 separately derived model cases
per compiler. The [integer parser](core/java-integer.md) also passes 43 unit
checks and all 65,536 one-unit comparisons per compiler through the public
client-disabled core. The new GameSettings owner builds on those primitives;
its verification has the separate scope below.

The [output writer](core/java-writer.md) adds counted UTF-16 encoding and
PrintWriter policy without graphics dependencies. Its public native builds
pass all 15 selected cases per compiler: 110,708 stream checks, 19 real-file
checks and five independent file checks, with Linux sanitizers. Independent
raw-output audits and later metadata inspections retain two controller footer
failures separately from the successful tests. A later input-driven observer
passes 25 transport controls per compiler. Its Windows comparison matches all
13,391 declared fields across 68 original/native scenarios; all 14 file
corruption controls reject the intended changes. Thread controls and private
state remain unpaired. See the writer contract for the finite projection.

The [GameSettings reference](reference/game-settings.md) records all 40 declared
fields and three original persistence profiles. The
[native persistence owner](client/game-settings.md) now implements defaults,
retained KeyBindings, options loading and ordered saving through real files.
All six persistence CTest cases passed across MSVC and sanitized GCC/Clang in
their earlier 1,199-file cohort. Each compiler passed 481 native checks,
six independent file checks and 26 observer controls;
all 635 C/header files pass formatting. Independent file audits retain the
complete tested outputs and product bytes. Separate Windows-1252 and explicit
Windows UTF-8 comparisons each match all 11,389 projected persistence fields in
16 cases, including full file bytes and 2,877 logical reference joins. All 14
file-only controls on the Windows-1252 baseline reject their intended changes.
The separate GCC Linux UTF-8/LF comparison also matches that finite projection;
Clang Linux comparison remains pending. Static-label correspondence and
effectful menu setters remain unfinished. The new App connection is implemented
with finite Windows tests above; these persistence comparisons do not qualify it.

The [GameOption and label module](client/game-options.md) now implements option
metadata, value queries, counted UTF-16 labels and the retained legacy key-name
table. Direct Translation initialization is separate from wrapper use. Native
Windows builds pass 883 GameOption, 1,241 label and 463 Assets checks, with
persistence and Translation regressions. A file-only audit qualifies the
headless tests after their controller's summary-parser failure; its failed
report remains unchanged. The later 675-case hosted run also passes the native
contracts on sanitized Linux GCC and Clang. Original label correspondence and
options screens remain separate. The new App startup connection has only the
finite Windows validation above, with Linux startup execution still pending.

The [binding-code action](client/game-settings.md#binding-code-action) now
selects the current array entry, commits its code and calls the real save body.
Source null/index errors precede save; caught save errors and native failures
retain earlier mutation and file effects. The raw field mutator remains
unchanged and does not save. Fresh Windows MSVC builds pass all six selected
CTest cases: 548 native settings checks, six independent file checks, 26 probe
controls, 883 option checks, 1,241 label checks, 463 Assets checks and 4,404
Translation checks. Both profiles finish their before/after inventories and
input/product audits. Configure/build retain the pinned compiler-helper
cleanup allowance; every test tree closes naturally. No original-action or
Controls UI correspondence is claimed. The later hosted run at the top also
passes these native action contracts on Linux GCC and Clang.

The [WorldAccess interface](world/world-access.md) now dispatches typed
cell, range, sound, particle, Entity-added and TileEntity-dirty callbacks. Its
native controls pass 295 checks on MSVC and sanitized GCC/Clang, including live-list
edits, retained receivers, failure prefixes and redstone reentry across Worlds
sharing a Registry. The existing-World redstone probe also accepts declared
constructor inputs. Its 52 controls per compiler check consumption, failure
ordering and file preservation. Original nonempty WorldAccess traces, concrete presentation
and tracking receivers, and original redstone constructor-input admission remain
unfinished.

At `ef1434015420678559cfa3d3b2f21018c5b733ce`, the local public/JDK suites passed
all 619 tests across MSVC and sanitized GCC/Clang. All 587 C/header files passed
formatting; controller mirrors and lifecycle interval controls also passed.
The [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40052114598)
passed all four jobs with the same 619-test and 587-file result. These results
precede the [canonical naming migration](reference/minecraft-names.md).

The renamed code also passes all 619 local tests and 587-file formatting,
including the schema-2 redstone producer's native controls. Controller mirrors
and lifecycle interval checks pass on Windows and Linux. Recorded fixtures keep
their source pins, with explicit historical/current module families and a
separate immutable WorldProvider recorder. These checks cover the represented
native contracts and finite replays; original nonempty WorldAccess and redstone
constructor-input correspondence remain unfinished.

The [synchronous redstone hooks](world/redstone.md) now use actual World mutations,
recursive strength updates and independently retained notification snapshots.
The native contracts cover both sides, a 25-cell path, rise/decay, vertical
connections, loops, drops and partial failures. A second 350-check case covers
negative/cross-chunk paths across four Worlds, dedicated loading gates and a
snapshot retained while another World uses the shared Registry. It passes on
MSVC and sanitized GCC/Clang. Code checkpoint `2150299` passes all 613 hosted
public/JDK tests and 583-file formatting. A bounded existing-World probe
matches 600 complete arrays per compiler against original observations while
retaining absolute World/cave Random differences. Linux Debug traces also match
2,087 ordered engine calls per side, including arguments, returns, flags and
Set structure, plus 605 local observations of 151 retained snapshots. The
comparison declares its storage and final-counter adaptations; twelve file-only
Windows corruption controls reject the intended errors. A separate GCC Debug
step comparison matches upward/downward redstone paths and all retained arrays,
while keeping absolute World/cave Random differences. Full World-level
redstone qualification, original callback-driven reentry comparison, wider persistence/failure
schedules and the default identity source remain required.

The [redstone position Set](reference/redstone-position-set.md) has reclaimable
Registry storage, full coordinate equality, bucket/tree ordering and snapshots
that survive clear/refill. Finite original comparisons pass conditionally on
measured identity hashes on MSVC/GCC/Clang. Windows development tests pass;
five Linux mounted-source timeouts pass separately from an identical native
source copy. The reference record preserves the failed full-suite results and
development-source attribution. Its [committed collection checkpoint](reference/redstone-position-set.md#committed-collection-checkpoint)
separately passes all 607 hosted public/JDK tests and predates redstone integration.

The [dedicated checkpoint](reference/dedicated-world-construction.md#committed-checkpoint)
at `9c87827` passes all 622 local tests: 206 MSVC, 208 GCC and 208 Clang, with
zero failures, errors or skips. All 1,099 source files match their committed Git
blobs. Fresh schema-2 original audits and native comparisons pass on every
compiler, covering seed23, seventeen calls, eighteen boundaries and actual Store
tokens on both sides. Linux enables ASan, UBSan and leak checks. The hosted run
passes 601 public/JDK tests and 569-file formatting; its actual merge checkout
has the same entire tree as the code checkpoint. Mirrors and interval controls
pass on all local and hosted builds.

Public capture, audit, comparison and replay tools retain the complete finite
observations and nine constructor inputs. The original-only fixture keeps its
inherited dirty-source attribution. Public replay runs the production probe with
minimal resources; fresh reference runs audit the supplied original and use its
exact extracted resources. The reference record defines the empty-collection
profile and remaining server-startup, scheduling and restart limits.

Synchronous fluid and scheduler diagnostics use checked heap workspaces. The
dedicated capture exposed large diagnostic copies exhausting the Windows
stack during valid fluid recursion. A public enclosed-column test checks both
sides, complete block/metadata results, 34 schedule requests, unchanged Random
state and balanced ownership. Earlier development suites passed 613 executions:
203 MSVC, 205 GCC and 205 Clang, with no failures, errors or skips. Linux enabled
ASan, UBSan and leak checks. All compilers passed controller mirrors and
556 positive/2,011 negative interval controls; all 569 C/header files passed
clang-format 18. Those counts precede the public dedicated workflow and retain
their dirty source and null tested commits. The
[reference record](reference/dedicated-world-construction.md) corrects the earlier
byte-equality claim. The eighteen public Python checks and eight actual-native
corruption controls also pass in the current checkpoint. Earlier named-data
results below retain their own qualified source and scope.

The production GAME World matches the original seed-23 origin and ring
population profiles on client and base-server sides. The ten-call ring retains
nine normal Chunks, populates four and ends with 6,968 ordered light regions.
The comparisons include complete retained arrays, cache state, provider
numerical state, array aliases and Random continuations. The
[fresh World evidence](reference/natural-population.md) defines the finite
schedules and their limits. Broader population acceptance remains open.

The public ring replay checks full original-derived numerical and cache golden
records. The public origin replay keeps its cache-only oracle. Both run the real
native probe with separately declared minimal resource files.

The [populated Chunk persistence comparison](reference/population-persistence.md)
adds two real forced cache saves and four direct region loads after the origin
prefix. It checks saved typed NBT, five loaded arrays, held Info,
cache/numerical state and raw disk preservation. The
[held World full-save profile](reference/population-full-save.md) adds metadata
and empty named SavedData lists. The separate
[named MapData profile](reference/population-named-data.md) publishes one dirty
default value under a different lookup key, performs two full saves and loads
its file through a fresh manager. Both original/native sides and all five
public replays pass on MSVC, GCC and Clang. Map terrain updates, trackers and
packets, broader manager/subtype behavior, nonempty Players, source file-failure
semantics, ordering, dedicated runtime, unload and restart remain open.

At code checkpoint `5b57bc1`, local suites pass 199 MSVC, 201 GCC and 201 Clang
tests: 601 executions without failures, errors or skips. Every compiler includes
five fresh population profiles, their public replays and the plant comparison.
Linux uses ASan, UBSan and leak checks. All 559 C/header files pass
clang-format 18; every compiler passes mirrors and 556 positive/2,011 negative
lifecycle-interval controls. The [source and capture audit](reference/population-named-data.md#committed-checkpoint)
verifies 1,073 clean-source files, 30 fresh/public captures and 30 resource-variant
pairings. [Hosted verification](https://github.com/frankischilling/minecraft-c/actions/runs/39964552445)
passes all 583 public/JDK executions and formatting/mirror/interval checks;
all four actual checkout trees match the code checkpoint. The eighteen
supplied-archive comparisons retain their local scope.

Plant lifecycle callbacks, farmland behavior and World block-light queries are
implemented.
The [plant](world/plants.md) and [World-query](world/queries.md) contracts describe
the implemented behavior and its limits.

Original client and server projections match all 59 prepared cases per side
on each compiler. The comparison covers block/metadata maps, both Random
continuations and ordered deterministic Item fields. These fixtures execute
unchanged original Block and selected tree bodies with prepared World services;
original World/Chunk construction, general notification and natural population
retain separate acceptance requirements. See the [evidence](reference/plant-lifecycle.md).

Other implemented modules have separate contracts and verification scopes:

- [Minecart construction](entity/minecart.md) and
  [inventory, semantic NBT and storage](entity/minecart-inventory.md).
- [Rails](world/rails.md), [World power queries](world/power.md),
  [pressure plates](world/pressure-plates.md) and
  [Piston TileEntities and geometry](world/tile-entities/piston.md).
- [Item and FallingSand movement](entity/movement.md), including collision and
  reached World effects.
- [Overworld population](world/population/overworld.md), its feature bodies and the
  [live save path](world/saving.md).

The [finite Minecart constructor comparison](reference/minecart-construction.md#constructor-observations-and-qualification)
matches ten null-World inputs per side against Windows MSVC. A separate
[inventory and semantic NBT comparison](reference/minecart-inventory-nbt.md#finite-inventory-and-semantic-nbt-comparison)
matches its complete declared projections on both sides. GCC/Clang inventory
comparisons remain pending. These results do not establish Minecart movement,
Player interaction or rail physics.

Whole-game behavior remains unfinished. Active Minecart ticks and rail travel,
complete redstone propagation, population and persistence acceptance, all four
multiplayer pairings, native graphical play, controls and audio remain required.
There is no playable C client or dedicated server. Account authentication is the
only approved gameplay exception.

See [testing](testing.md), the [parity matrix](parity.md) and
[roadmap](roadmap.md) for current requirements. Detailed earlier results remain
in the [population and runtime history](history/population.md) and
[earlier integration history](history.md).
