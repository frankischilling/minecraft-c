# Development status

The native C client is pre-playable. Its default path uses the constructed
source App, initialized menu and genuine [World-selection screen](client/world-selection.md).
Four Windows native save-list profiles pass, including row selection, scrollbar drag,
same-parent Cancel, original-receiver trailing input and retained-screen cleanup.
Empty, populated and bottom-scroll captures were inspected. Select now enters the
[source World loader](client/world-loading.md) after constructing the actual
controller. The existing-save profile reaches `READY`, all 289 terrain samples
renderer/particle attachment, real Player insertion and view publication.
World loading and healthy null-screen/focus handling return; the same tick
then reaches gameplay input `Minecraft.k:337`.
Dynamic terrain rendering and gameplay remain unfinished. Explicit flags retain the
diagnostic title and fixed-camera World inspection.

## Current integration

All 482 selected Windows CTests pass without failures or skips in 209.59
seconds after the complete Debug ALL build. The headless Release ALL build,
formatting for 1,405 files, 86 strict compiler checks and 30 selected Linux
sanitizer cases also pass. The latter includes four private Engine/RR CPU
wrapper cases; it does not qualify a complete Linux graphical session. Existing
save and generated-terrain entry both reach World-load return and cleanup.
See the [current qualification](testing.md#renderer-attachment-and-image-worker-qualification).

The current World-entry extension passes its focused native Windows loop.
Its DateFormat module matches 2,926 JDK17 vectors on Windows and Linux for
English/en_US, UTC/US Eastern and UTC years 1967..2099; other profiles fail
explicitly. Create and Rename stop at `NEW162` and `NEW189`. Delete stops at
preparation 21 without skipping its prefix. The preceding source-loading Windows MSVC Debug ALL build
and all 475 selected CTests pass without failures or skips in 208.25 seconds;
46 configured reference-label profiles are excluded. The separate headless
Release ALL build and formatting for 1,379 files also pass. Ten focused source
runtime cases per Linux compiler pass across two ASan/UBSan selections. The separate Windows
generated-terrain Release profile at that preceding checkpoint completed all
289 samples and Player/input through its then-reached renderer frontier. See the
[current checks](testing.md#world-selection-in-the-default-runtime).

The preceding native-menu source passes all 463 selected Windows CTests without
failure or skip, a separate headless Release ALL build and formatting for 1,341
files. That selection excludes 45 configured reference-label profiles.

The [World-selection run at `1334db5`](https://github.com/frankischilling/minecraft-c/actions/runs/40813601193)
is terminal: Windows 440/440 and Clang 461/461 pass, while GCC passes 460/461.
Its sole failure is a real Display creation refusal in the later-owner
constructor profile. Cleanup succeeds; the underlying SDL operation/detail
was not printed, so its cause remains undiagnosed. A diagnostic-only follow-up
prints those captured fields without changing failures or assertions. Both
optional constructor profiles pass separately on Windows after enabling them;
this is separate from the complete 468-case selection recorded at that checkpoint. Formatting passes
for 1,356 files; all checkout trees match the tested head. The fingerprint
regression passes on all three hosts; `client.startup` passes on both Linux
compilers. See the
[hosted record](testing.md#hosted-world-selection-checkpoint-1334db5).

The later [`d71bd94` run](https://github.com/frankischilling/minecraft-c/actions/runs/40815149868)
passes Windows 440/440 and Clang 461/461; GCC passes 460/461, now failing SDK
transport at actual SDL video initialization. The previous later-owner case
passes. All test names remain, with zero skips and matching checkout trees;
formatting passes for 1,356 files. The hosted error string is absent.

A private pinned-SDL3.4.18 reproduction captures `x11 not available` during
parallel window/init/quit cycles under default Xvfb. Three of four default
runs fail; the passing run is retained. Changing only `-noreset` passes 2,400
cycles across three servers and twelve processes. Linux CI now supplies that
server flag while retaining the default screen, four workers and full suite.
The SDK test also preserves failed-init error text without changing its checks.
The [matched evidence](testing.md#sdl-initialization-and-xvfb-lifetime) supports
the environment correction. The exact
[`9d83665` run](https://github.com/frankischilling/minecraft-c/actions/runs/40819123976)
passes all 1,362 tests across Windows 440/GCC 461/Clang 461, with zero skips and
matching trees; formatting passes for 1,356 files. This checkpoint precedes the
source World-loading extension and leaves the old missing SDL strings unknown.

The corrected runtime [hosted run at `de4290b`](https://github.com/frankischilling/minecraft-c/actions/runs/40802203908)
passes all 435 Windows tests in 329.63 seconds and all 454 Clang tests in 189.45
seconds. GCC passes 453 of 454 in 339.00 seconds; `client.startup` stops in the
prelaunch executable fingerprint guard. Formatting passes for 1,341 files. All
unique starts/results and tested checkout trees match that head. The old guard
combined regular-file and 128 MiB size predicates, so the log does not directly
measure the artifact size. The current streaming hash has its own finite 1 GiB
bound, full-file/identity checks and a public sparse-file regression over 128 MiB;
resource/image limits remain unchanged. That fix passes on Windows and Linux
and is included in the current increment. The [audit](testing.md#hosted-runtime-checkpoint-de4290b)
keeps its evidence separate from the later World-selection run.

The first runtime [hosted run at `fab3769`](https://github.com/frankischilling/minecraft-c/actions/runs/40801330270)
stops both Linux builds on a signed-to-unsigned diagnostic-site conversion;
neither Linux test suite starts. The correction preserves native `-1` and
unsigned source bytecode sites in a wider signed record. Strict GCC/Clang syntax
checks and three affected Windows tests pass after rebuilding. The later
`de4290b` run above completes both Linux builds and their full test selections.

Published predecessor `3ec9892` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40796960416):
434 Windows, 452 GCC and 452 Clang tests, 1,338 total, without failures or skips.
Formatting checks 1,325 files. All tested merge trees match that published head.
GCC restored timing estimates and started long cases early: its test step took
189.20 seconds, compared with 253.61 seconds in the prior cold-cache run. Source
and cloud conditions also differ; this observation establishes no causal speedup.

The preceding startup/menu-initialization Windows source builds ALL and passes
460 selected CTests without failures or skips, including the normal startup windows, resource worker,
source menus and RenderGlobal device controls. This selection excludes configured
reference-label profiles. Formatting checks 1,325 files. A separate headless
Release ALL build also passes. See [the current scope](testing.md#normal-startup-through-menu-initialization).
Later hosted results are recorded above with their exact source scopes.


Checkpoint `8288290` has a [failed hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40792366222).
GCC passes all 452 tests. Clang passes 451 of 452; Display creation fails in one
GL setup case without its underlying cause in the old assertion output.
Windows stops on MSVC C4127 in a constant layout check. The current source uses
three equivalent compile-time assertions and retains the actual Display error
on failure. Formatting passes for all 1,319 files in that checkpoint. The later
`3ec9892` hosted run passes these checks; the old Display failure's cause remains
unproven. Newer runtime qualification retains its own scope.

Hosted baseline `5d61f89` passes [all four jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40746811719):
410 Windows, 427 GCC and 427 Clang cases, 1,264 total, without failures or skips.
The later startup increment has selected native qualification below; that scope
is separate from this complete hosted baseline.

Earlier checkpoint `6249a64` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40692181754):
407 Windows, 423 GCC and 423 Clang cases, 1,253 total, without failures or skips.

Earlier checkpoint `671c003` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40639713371):
398 Windows, 414 GCC and 414 Clang cases, 1,226 total. None fail or skip.
Formatting checks 1,164 files. The native jobs check current input bindings
before configuration and retain their complete test suites. This checkpoint
includes the terminal Font metadata cleanup described below; newer statistics,
cursor and startup integration have separate focused local results.

The corrected `bd8d7f1` [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40635329070)
passes Windows and formatting. Each Linux compiler passes 413 of 414 cases.
The remaining Font test omitted terminal lookup-metadata cleanup and correctly
received BUSY from final Engine release. The test now invokes that existing
cleanup API; a focused Clang/GCC run passes all 625 checks with empty sanitizer
stderr. No production Font or GPU behavior changed. Current input preflight
now runs before native configuration; full native tests remain enabled.

Checkpoint `cd346b0` has a [failed hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40632949338).
Both Linux builds complete and each passes 406 of 414 cases. Eight cases reject
stale current-source bindings in the App owner, Biped and source-rendering
controls. Windows stops on an older MSVC C4200 diagnostic for RenderManager's
C17 flexible array. Formatting passes. The correction scopes the compatibility
guard to that one type and refreshes the affected current input bindings;
original authorities and historical qualification scopes remain unchanged.

Checkpoint `f37a96f` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40599463192):
1,188 public/JDK cases, comprising 386 Windows, 401 GCC and 401 Clang cases.
None failed or skipped. Formatting checks 1,115 files. Both Linux SDK transport
cases pass with sanitizer checks enabled. This result is separate from later
renderer, SaveHandler and startup source work. The preceding `37dc83f`
[hosted checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40544932686)
passes its 1,127-case scope.

## Normal startup integration

The separate [normal startup owner](client/normal-startup.md) now constructs
Display, the default game directory, save-format manager, Settings, selected
texture pack, RenderEngine and bitmap Font in source order. It loads the three
water/grass/foliage color maps and calls their real setters. It now also
constructs EntityRenderer and RenderManager with their distinct held-item
renderers. The newer source continues through the real statistics writer and
callback, startup logo, keyboard/mouse creation and MouseHelper publication.
It completes the admitted empty-Controllers path, both GL diagnostics and setup,
then constructs the capability checker and binds SoundManager through the real
typed Settings-save path. Nine texture effects register and perform their first
updates. RenderGlobal construction, viewport setup and the particle constructor
return before App publication `j660`. The resource-thread caller now constructs
and publishes `U676`, starts its worker, and runs the `Post startup` diagnostic.
GuiIngame construction and `v705` publication now complete. Startup constructs
the main menu, saves statistics, clears messages, publishes the current screen
and initializes its scaled layout and buttons. `Minecraft.a()` returns at `749`.
The bounded startup probe stops before the clock call at `Minecraft.run`32;
the default [native menu runtime](client/menu-runtime.md) continues from that
same graph. It adopts Timer frames, canonical screen input, no-World ticks and
source menu drawing. Quit clears the actual App run flag. The later
[World-selection increment](client/world-selection.md) connects Singleplayer,
save rows and Cancel. Selected existing-save loading now constructs the real
World and Player/input; renderer attachment and gameplay remain unfinished. Full source
run/PX rendering and `Minecraft.d()` shutdown parity are not established.

The earlier scene-construction increment passes 24 selected Windows cases, with six recorded on
the production successor and eighteen on its test-only triangle-observation
successor. Production, headers, CMake and flags match across those snapshots.
The real normal Session passes 181 checks; typed Writer/Settings save pass
582/202 and the RenderGlobal device profile passes 16,723 checks. Clang 18
passes 41 selected cases with ASan, UBSan and leak detection. GCC validation is
tracked separately. These results do not establish Linux default-directory
startup, original/JVM or visual correspondence, World entry or gameplay.
See [the exact scope](testing.md#startup-scene-construction-and-typed-settings-save).

The earlier GL-diagnostic source passes 47 Windows cases. Nine Core output cases
retain their separate earlier Windows test-header source. Clang 18 and GCC 13
each pass 53 selected cases: 41 CPU/protocol and twelve real-device cases,
with ASan, UBSan and leak detection. Fresh Windows normal and real-GL-error
startup profiles pass 103 and 109 checks. These results do not rerun the full
suite or qualify Linux default-directory/Session startup, original-JVM or
visual correspondence, World entry or gameplay. A separate Release headless
build links the region-inspection utility with client, sound, audio and
testing disabled. No utility runtime or server behavior was tested.

Published checkpoint `33c777c` has a [failed hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40589053675).
Windows stops on an older MSVC flexible-array warning. Each Linux compiler
passes 400 of 401 tests; the SDK transport test fails during leak checking.
Formatting passes. The corrections use a scoped MSVC compatibility guard and
ordinary Linux X11 linkage. The unchanged transport test passes all 37 checks in
a fresh sanitized Clang build. The corrected `f37a96f` hosted result above is green.

The later renderer/loading composition passes its eight selected Windows CPU
and protocol cases. Nine real Windows startup-window profiles pass 173,090
checks across twelve actual Apps, including shared-owner retirement and failure
limits. Nineteen loading-screen drawing profiles pass 888 checks. A combined
source also builds the client and passes 29 selected cases on each Linux
compiler with sanitizers. These scopes remain distinct from the hosted suite.

The earlier startup-logo callee passes five prepared-receiver Windows profiles,
414 checks, including actual drawing, swap and complete teardown. Normal
startup at that checkpoint stops at offset 324. The later Session startup
profile invokes the logo from its genuine caller and passes 92 checks through
the offset-383 frontier. No original visual parity is claimed from either
native profile.

The newer Windows source builds the full client. Its focused qualification
passes 26 CTests and two direct window profiles; the original build and later
strict-warning source refresh retain their separate scopes. Clang builds the
client and passes 56 selected CPU cases plus six real graphics cases with
ASan, UBSan and leak detection. After correcting a signed/unsigned trace
assignment, GCC builds the client and passes the same 56 CPU and six graphics
cases on the successor source. Minimal affected Clang checks and the Windows
92-check Session profile also pass on that successor. These selected cases
do not establish a full-suite result or a playable session.

The [source SaveHandler](world/save-handler.md) now constructs actual world
directories and session output, reads metadata through the real stream/NBT/Info
owners, and checks matching session tokens. Its constructor and later read
cohorts have selected native Windows/Clang/GCC results. World entry and complete
save/reload sessions remain unfinished.

The earlier local source builds ALL and the client on MSVC Debug, Clang 18 and
GCC 13. Windows passes 67 distinct selected CTests. Clang and GCC each pass 74 distinct
selected CTests across 81 executions.
Linux uses ASan, UBSan and leak detection. These focused results retain 774
constructor checks and 196,715 array/class checks; they are separate from the
preceding hosted matrix and do not establish a full local suite.

One real Windows startup process exited zero after 166 authored assertions,
including Font/color setup and retirement, and left only its three empty game
directories. The window qualification stopped afterward on its capacity guard.
Its reader also omitted two genuine constructor banners; the remaining six
window profiles did not run. No seven-profile result or original comparison is
claimed from that attempt. World entry, input/tick adoption, dynamic rendering
and gameplay remain unfinished.

## Resource observer source successor

A separate observer producer cohort builds ALL and passes six selected CTests
plus eleven native-only profiles on MSVC Debug and sanitized Clang 18/GCC 13.
It retains 774 constructor and 196,715 array/class checks. These finite profiles
preserve unfinished native frontiers and are separate from a complete local suite.

One retained Windows CPU default attempt now has independently reviewed origin,
capture, input, source and product admission. All seven supplied-original color
rows match, including full arrays and cache identity. The initial reader refusal
remains retained; exact added JDK class proof and a two-pattern log-tag correction
admit the same captured attempt without rerunning the game. ZIP, cold-static and
original texture/GL observations remain outside that result.

The later reader and metadata policy retain their pure parser controls and
read-only admission checks, separate from the earlier native producer. At
`37dc83f`, CI compiles both observer probes and passes four public pure checks
per compiler. Supplied-original profiles stay off; this hosted result does not
rerun the seven-row original attempt or the eleven-profile native producer.
See the [resource observation scope](client/render-engine.md#resource-lookup-observations).

## Preceding local cohort

The preceding Timer, Minecraft class/worker, RenderEngine, Biped and World/Player
cohort passed 1,163 local CTests: 393 Windows MSVC Debug cases and 385 cases on
each Clang 18/GCC 13 Debug build with ASan, UBSan and leak detection. Every
preceding 389/381/381 case remained enabled. None failed, skipped, was disabled
or remained unrun. All three profiles exercised real OpenGL startup, title and
fixed-camera World inspection.

The tested cohort contained 1,885 files and 1,643 non-Markdown inputs. All 936
C/header files passed clang-format 18.1.3. Source and configured products remained
unchanged through execution; all 283 Windows executables kept their default
1 MiB stack. Documentation updates preserve non-Markdown inputs and pinned
reference text.

[Ordered Timer construction](client/timer.md) passes 1,267 checks per compiler,
including retained clock-failure prefixes and independent render-partial storage.
[Minecraft class activation and the Timer hack worker](client/application-owner.md)
pass 225 checks and 70 controls in each normal/optimized Python run. Five original
class/worker projections retain their declared prepared-receiver scope. These
owners are startup prerequisites adopted by the newer source startup above;
World entry remains unfinished.

[RenderEngine construction](client/render-engine.md) passes 8,333 CPU and 223
device checks, with 55 separate texture-trace checks. Fresh original CPU runs
match all 4,096 ARGB pixels and ten glyph metrics on Windows and Linux; the
selected original GL profile has ten exact Windows rows. [Biped construction](client/biped-construction.md)
passes 4,381 checks and 435 allocation prefixes, with 838 fresh original records
per compiler. Normal application publication, animation and drawing remain
unfinished.

[LocalPlayer NBT](entity/player-nbt.md) now executes the actual Living, Player
and Inventory read/write hooks, first-Player World metadata save and saved
Info.Player attachment. Each compiler passes 490 NBT, 110 coordinate, nine
diagnostic and 169 publication checks. Thirty-three original cases match 1,302
normal tag nodes. Windows runs fresh originals; current Linux executables match
the retained Windows observations. The separate wake93 prefix has 37 records,
including 33 tag nodes. Full wake remains unfinished.

[Lava CPU animation](../tests/reference/client/lava_animation/README.md) passes
132,659 checks per compiler. Fresh Windows and Linux original CPU runs match all
348 records across 23 cases and 31 calls. That isolated profile did not include
application publication, texture registration or GPU upload.

The [singleplayer controller](client/singleplayer-controller.md),
[camera methods](client/first-person.md),
[World Entity updates](world/entity-update.md),
[Player spawn/reset](entity/player-spawn-reset.md) and
[weather method](world/weather.md) retain their specified native and original
comparisons in this combined matrix. These are finite method/owner results.
They do not establish a connected playable session. Full evidence and
reproduction options are in the [testing record](testing.md#current-public-runtime-integration).

The preceding published `4a6b805` [hosted checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40464703757)
passed 1,055 public/JDK cases: 345 Windows and 355 per Linux compiler. Its hosted
scope and the 1,163-case local cohort remain separate from the current results.

## First playable singleplayer session

The next end-to-end milestone is one World that can be loaded, explored with
Player controls, changed through block actions, saved and reloaded. It requires:

- Complete normal startup and World/Player/view publication in source order.
- Connect physical input, frame timing and World/Player ticks to the real owners.
- Update rendered terrain as Chunks and Blocks change, with the live Player camera.
- Complete targeting, mining, placement and the reached inventory interactions.
- Verify native Windows/Linux play and persistence across a complete session.

Existing terrain, population, lighting, storage, Player, inventory, audio and
rendering modules have separate qualified scenarios in the [parity matrix](parity.md).
Those results remain narrower than complete subsystem or gameplay parity.

## Remaining deliverables

Multiplayer interoperability, the standalone C dedicated server, extended play,
all dimensions and remaining presentation/gameplay behavior still require
implementation and verification. Account authentication is the only approved
gameplay exception. See the [roadmap](roadmap.md) and [original inventory](inventory.md).

Earlier checkpoints remain in the [client runtime history](history/client-runtime.md),
[population history](history/population.md) and [integration history](history.md).
