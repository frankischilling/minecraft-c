# Earlier client runtime status

Earlier checkpoint records retain their original result and unfinished-work
scope. See [current status](../status.md) and [testing](../testing.md) for the
latest integrated matrix.

## RenderEngine/Biped matrix before ordered Timer and class/worker adoption

The combined RenderEngine, Biped, Lava animation, LocalPlayer NBT, singleplayer
controller, camera, World Entity update, spawn/reset and weather source passes
1,151 local CTests: 389 Windows MSVC Debug cases and 381 cases on each Clang18/
GCC13 Debug build with ASan, UBSan and leak detection. Every preceding
376/348/348 case remains enabled. None failed, skipped, was disabled or remained
unrun. Windows adds thirteen Renderer/Biped cases. Each Linux profile adds
twelve Renderer/Biped cases, twenty full-client public cases and the supported
original CPU rendering comparison to its earlier headless matrix.

The frozen cohort contains 1,867 files and 1,627 non-Markdown inputs. All 926
C/header files and the separate shared-model trace include pass clang-format
18.1.3. Before execution, 292 current source/helper fingerprints and twenty-one
original class/Code families verify. Source inventories, every configured
artifact, runtime library, symbol and compiler/link configuration remain
unchanged. Each Linux profile binds 1,830 product/configuration files and 334
actual targets, including extensionless executables and all built objects.
All 281 Windows primary executables retain the default 1 MiB stack reserve.
Subsequent documentation edits preserve non-Markdown inputs and pinned
reference text.

All three profiles enable the graphical client, display tests, sound and audio.
They exercise actual OpenGL startup, title and three World inspection captures.
Linux uses private authenticated abstract-local Xvfb displays, Mesa llvmpipe
OpenGL4.5 core, and the real production Display. Both owned servers terminate
cleanly. Actual recipes instrument all 42 FreeType compilation units and 593
strict project C17 recipes per Linux compiler. The 833 unmodified FreeType
source files match the pinned official archive before and after execution.

Separate fresh Windows production-client and headless ALL builds pass with
BUILD_TESTING disabled and Java/Python package discovery disabled. The headless
graph excludes font, display and audio dependency targets. The production-client
mode check disables optional sound/audio; the full matrix independently enables
both. Hosted public/JDK checks run separately.

## Earlier movement and runtime checkpoint

[LocalPlayer movement](../entity/player-movement.md) and the
[Entity environment helpers](../entity/environment.md) now use the real World,
Registry, Input and Entity owners. Player clipping, sneaking, walking, step
sounds and the current Block step-on family retain source order and reclaim
temporary sound Strings. Water/lava, stored-water, clearance and relative
acceleration retain their distinct actual calls and nullable-World prefixes.
Living travel, complete Player/World ticks and application adoption remain
unfinished; there is no playable client yet.

The complete combined source passes all 280 Windows MSVC cases and all 261
headless cases each on fresh sanitized Clang 18/GCC 13 builds: 802 completed
cases without failures, errors, disabled cases or skips. Each product passes
15,297 environment and 40,373 movement/step/sound checks, all 1,353 original
client environment records, 1,027 original server relative/null-World-prefix
records and 51 separately labelled native server World crosschecks. Movement
matches 21 unchanged-client records plus twelve separate JDK rounding records.
Successful original-server World environment execution is not claimed.

All 773 C/header files pass clang-format 18. The frozen 1,511-file source cohort
contains 1,303 non-Markdown inputs; readback verifies code, native/helper pins,
original class metadata, executable identities and complete retained captures.
Both new twelve-case capture-control suites pass normally and under optimized
Python. Controller mirrors and all 556 positive/2,011 negative lifecycle
interval controls pass. Windows includes real OpenGL/client checks; the local
Linux profiles are headless.

The notice checkpoint `04be54b0518e61ca70c48270eb95ab1f8d9ed696` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40251364177):
256 Windows, 262 GCC and 262 Clang tests, with 780 matched Start/Passed pairs.
Every checkout uses merge `79d9c8e09de20ed967b4057baf1eea890406b22a`, whose tree
equals that checkpoint. These hosted results precede the movement/environment
integration qualified locally above.

[Achievement notice/model construction](../client/achievement-notice.md) now uses
the complete application storage and the real Entity/Vec3, Registry and Settings
owners. Final combined validation passes all 272 Windows MSVC tests and all
253 headless tests each on sanitized Clang 18/GCC 13: 778 completed cases,
no failures or skips. Windows original comparisons match all 140 geometry and
151 callback records plus ordered diagnostics. Each native profile checks ten
authored notification cases, 16 allocation prefixes, failure/alias lifetimes and
1000 reminders without permanent Registry growth. Nine capture controls pass
normally and under optimized Python. Drawing, complete startup and Player ticks
remain unfinished; there is no playable client yet.

All 765 C/header files pass clang-format 18. The 1,484-file frozen cohort has
1,279 non-Markdown inputs, with original class/Code and native/helper pins
verified. An initial four-way Clang run timed out in the unchanged Python
region-write controller at 120 seconds. It passed unchanged in isolation in
28.67 seconds, then both complete Linux suites passed at CI's parallelism two.
The original failure is retained; its deadline and assertions stay unchanged.

The CI scheduling/header checkpoint `4aa603872a29048fc73122a9aa03c5fcc929fd1d`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40245956977),
with 771 complete test pairs. All checkout trees equal that head. Bounded
parallelism reduces the observed test steps from 690 to 402 seconds on Windows,
550 to 218 on GCC and 667 to 375 on Clang. These are consecutive-run measurements;
all checks remain enabled. The [testing notes](../testing.md) record their scope.

The material checkpoint `989176653d3a4672ddecd3364e532c80ea1d75be` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40244894912):
253 Windows, 259 GCC and 259 Clang tests, with 771 complete Start/Passed pairs.
All checkouts use merge `29ca9414f3ae0a238d0401eddc00f2c48d88926d`; its tree
equals that code head. The later CI scheduling/header change has separate checks.

[Material immersion and acceleration](../world/material-movement.md) now use actual
World/Entity/Block/Material owners and a shared Vec3 pool. The combined source
passes all 268 Windows MSVC tests and all 250 headless tests each on fresh
sanitized Clang 18/GCC 13 builds: 768 completed cases, no failures or skips.
Each product passes 1,944 new native checks and matches 300 unchanged-client
records plus 172 shared native server rows. Original server evidence remains
paired bytecode and a native crosscheck. Living movement, active Player ticks
and application gameplay remain unfinished; there is no playable client yet.

All 754 C/header files pass clang-format 18. The frozen 1,464-file cohort has
1,261 non-Markdown inputs; source and original class/Code pins pass readback.
Both material and maintenance controllers pass twelve subprocess controls,
normally and under Python optimization. Reader I/O failures and short writes
now fail capture explicitly, and legacy reports lacking reader-error metadata
are rejected. The native runtime and completed clean earlier comparisons retain
their documented scope.

The World/Player checkpoint `a59b168af91d64a72d9de072b6ecccb9747c684a`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40240886139):
251 Windows, 257 GCC and 257 Clang tests, with 765 complete Start/Passed pairs.
All checkouts use merge `53ddb2cd57f16f3b04ffd21383b3cb4a4a81c6bd`; its tree
equals that code head. These checks precede the new log-write failure control.

The integrated World/Player source passes all 265 Windows MSVC tests and all
247 headless tests each on fresh Clang 18/GCC 13 builds with ASan, UBSan and leak
checks. All 759 cases run without failures, disabled tests or skips. The
1,446-file input snapshot stays unchanged through verification; all 745 C/header
files pass clang-format 18. Controller mirror identities and the 556 positive /
2,011 negative lifecycle interval controls also pass.

[Lighting propagation](../world/light-propagation.md) writes actual Chunk nibbles
and preserves live callbacks, detached queue records and 50-level reentry.
The inspection view drains after footprint acquisition and before final meshes.
Each compiler's fresh original comparison matches all five arrays, all 49 drain
calls, ordered regions, scheduler records/aliases and boundary Random states
for the controlled 25-Chunk view. The post-constructor World.r seed is an
explicit fixture input; default production entropy is unchanged. Full 289-point
profiles retain the separate older Release/transport qualifications recorded in
the [reference package](../../tests/reference/world/propagation/README.md).

[Player publication](../world/player-publication.md) passes 163 native checks and
three genuine original projections per compiler. [Input attachment](../client/player-input.md)
passes 193 native checks and 21 new original projections; the underlying
MovementInput still matches all 857 prior records and passes 1,222 native
checks. [Provider maintenance](../world/provider-maintenance.md) passes 949 native
checks and six original cases with 30 matching state groups/104 Chunk vectors
per compiler. The eight maintenance-controller and seven propagation-reader
controls pass. These operations still require application event routing,
Player/Living/World ticks, controller actions and gameplay integration.

The earlier Player constructor checkpoint `42b91699b9a7313f5af3946496e6c599005419be`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40232088249):
244 Windows, 250 GCC and 250 Clang tests, with 744 complete Start/Passed pairs.
Every checkout uses merge `2e0c2c94c913e81a0ff2b997532b6e0dddd9ebb7`; its tree
equals that code head. These checks precede the lighting integration.

[Local Player construction](../client/player-construction.md) now executes the
actual `dc -> gs -> ls -> sn` constructor chain, including the early watcher
hook, four shared Math calls, Inventory/Container aliases, World spawn and
Session/filter ownership. Full ABI rebuilds pass all 254 Windows tests and
all 236 headless tests each on sanitized Linux Clang/GCC. The new constructor,
protocol and original comparisons pass on all three products. Source-supported
null failures retain their actual constructed prefixes. Separate
[World Player attachment and publication](../world/player-publication.md) now
preserve the lowercase Player ledger, sleeping scan, Chunk/World membership and
live WorldAccess callbacks. [Player input attachment](../client/player-input.md)
now retains the actual MovementInput and forwards key/reset/sneaking operations.
Application event routing, ticks, movement and Player rendering remain unfinished.

The crafting checkpoint `d113527bfb4c306bfa94b5b56bb62277b9b392db` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40224136061):
242 Windows, 248 GCC and 248 Clang tests, with 738 complete Start/Passed pairs.
All checkouts use merge `ba83182df0d9bf222f21273151f54f8eb1265113`; its tree
equals that code head. These checks precede the Player constructor increment.

The input/turn checkpoint `5143b58fbba506bec1d9753080e5fcb3a70906a3`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40218311052):
240 Windows, 246 GCC and 246 Clang tests, with 732 complete Start/Passed pairs.
Every checkout uses merge `1d012bd644dd57b0b767de78b747aebcd9307016`; its tree
equals that code head. These checks cover input, turning and the backend
allocation regression, before the crafting increment below.

[Crafting inventories and containers](../inventory/crafting.md) now construct
InventoryPlayer and the actual Player/Workbench containers, notify grid changes
synchronously and match all 151 recipes in source order. A bounded probe scanner
replaces a NUL-truncating reader. The final native, protocol and unchanged-original
tests pass on MSVC and sanitized Clang/GCC. Each product matches 1,153 original
rows per side and passes 48 protocol checks. Crafting-output effects and gameplay
remain unfinished; Player construction has its separate scope above.
The complete frozen Windows suite passes all 251 tests without failures or
skips. Linux verification of this increment covers the three selected native,
protocol and original tests per compiler with ASan, UBSan and leak checks.

The renderer/audio checkpoint `3d26621d5e66741c4a4383336fd12e729fe7439b`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40213051132):
237 Windows, 243 GCC and 243 Clang tests, with 723 complete Start/Passed pairs.
All four checkouts use merge `17ad0d82`; its tree equals that code head.

The next Player dependencies implement [MovementInput](../client/movement-input.md)
and inherited [Entity turning](../entity/turn.md). Integrated native and original
comparisons pass on MSVC and sanitized Clang/GCC. They preserve live key bindings,
duplicate precedence, double sneak/turn arithmetic, pitch clamps and previous
angles. They do not construct or tick a Player. Concrete crafting owners now
provide the inventory/container graph and actual recipe scan. Living/Player
ticks, Player publication and World lifecycle integration remain unfinished.
The combined input/turn and allocation-error increment passes all 248 Windows
tests and all 250 tests each on sanitized Clang/GCC, with no failures or skips.
All 707 C/header files pass clang-format 18. The allocation case is verified
against both the failing earlier branch and the corrected branch.

The client now renders a fixed area of a fresh actual GAME World through its
[CPU mesher](../client/world-meshing.md) and [OpenGL renderer](../client/world-renderer.md).
A native Windows seed23 run and sanitized Clang Linux/Mesa run both exit
successfully after two frames. Player controls, world ticking, interactions,
inventory and child menus remain unfinished. There is no playable client yet.

The [native OpenAL backend](../client/audio-backend.md) implements real loopback,
device channels, decoded/device caches, streaming and separate command/stream
workers. Review fixed displaced-buffer cleanup, worker wake synchronization and
first-fault retention. The updated native loopback and default-device tests
pass with empty stderr. The application has not yet
adopted it. The [WAV codec](../client/sound/wav-codec.md) preserves source frame and
buffer policy rather than forcing all input to PCM16.

The SoundManager checkpoint `2c311612d78028e8ab17e454925e1d0106d4f124`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40203404635).
Those checks cover the earlier manager/Vorbis increment, not the new renderer,
WAV and concrete backend work described above.

The frozen World/audio increment passes all 243 Windows MSVC tests and all
245 tests each on Ubuntu Clang 18 and GCC 13 with ASan/UBSan and leak checks.
There are no failures or skips. The runs include the unchanged-original WAV and
CPU mesh comparisons, the actual client World capture test and real loopback
workers. All 697 C/header files pass clang-format 18; controller mirrors and
556 positive/2,011 negative lifecycle controls pass. The final color-map error
preservation change additionally passes all three focused World tests per
compiler. These local results retain the [documented scope](../testing.md).

The [SoundManager](../client/sound-manager.md) now owns shared setup, real nested
settings saves, effects, streaming, music and listener requests at the backend
call boundary. Windows MSVC passes 20,066 manager checks and 62 separate settings
integration checks. Fresh sanitized Ubuntu Clang/GCC profiles each pass all 11
selected sound/settings/reference tests without failures or skips. Original
device correspondence and real client adoption remain unfinished.

The [source-policy Vorbis codec](../client/vorbis-codec.md) passes 14 fresh
original/native page and buffer comparisons, including tiny buffers, saved-tail
trimming and fresh readAll thresholds. Stereo PCM matches all 21,248 bytes.
Short and long mono retain exact synthesis mismatches. No tolerance is applied.
The focused public codec test passes; broader validation of these changes is
recorded separately from the earlier checkpoints below.

The nonzero sound checkpoint `aa39f8f663d250052d6d2b9cae9c691cfa7136f3`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40190426923):
229 Windows, 233 GCC and 233 Clang tests, with 695 complete Start/Passed pairs
and no failures, skips or missing cases. All 658 C/header files, controller
mirrors and 556 positive/2,011 negative lifecycle checks pass. The four checkouts
use synthetic merge `839f22e`, whose complete tree equals that head. Independent
readback reconstructs 331 trees/1,566 entries and joins all 1,236 literal
published files and 1,056 Windows-tested non-Markdown inputs.

The earlier sound checkpoint `e10a5e2708e81c81ea2cc9b721c060a2d2f056d7`
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

The [sound resource foundation](../client/sound.md) owns SoundPool entries, resolved
URLs, raw/MUS streams and sequential Vorbis decoding. A Windows MSVC x64 Debug
headless run passes 18,654 pool, 581 file, 579 decoder and 5,480 nonzero decoder
checks, alongside 43 integer-parser and 191 Random-profile checks. All seven
stages and eleven audits pass; independent readback consumes 14,431 retained
files and joins all 1,236 public members. Formatting covers all
658 C/header files. These authored contracts initialize no audio device and
make no original PCM comparison. At that checkpoint, SoundManager, WAV, backend playback, real
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
PCM bytes through the generic native decoder. The [reference note](../reference/client-sound.md#arranged-original-codec-observation)
retains this output-policy difference; it supplies no floating-synthesis match.

A separate nonzero mono control returns complete 8,192-byte PCM on both sides,
but four of 4,096 samples differ by one signed16 step. Independent original
and native capture readback confirms the difference without trimming or
tolerance. The [nonzero observation](../reference/client-sound.md#nonzero-original-pcm-observation)
retains all four values and full-output hashes. The numerical cause remains
unresolved; original PCM equality is not established.

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

The newer [float/discrete option actions](../client/game-options.md#float-and-discrete-actions)
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
before font/menu consumers. [GameDirectory](../client/game-directory.md) performs
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

The [native client](../client/startup.md) draws the title screen on Windows with an
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
[GuiMainMenu renderer](../client/gui-main-menu.md) supports a matched conditional
scene with ordered binary32 modelview operations. Windows now matches all
409,920 original RGBA pixels in that scene. Linux/Mesa retains one differing
pixel against the original Windows/NVIDIA capture; the cause remains unresolved.
An original title draw and exact clock witness have
been observed in an arranged receiver; the first helper's teardown failed.
The second helper completed with natural target/observer exits; its controller
failed a JDI version-property check. A separate corrected file-only audit
passed without a redraw. The [reference note](../reference/client-title.md)
records that lineage. These results do not qualify complete original startup
or rendering equality.

The [counted float parser](../core/java-float.md) constructs binary32 values with
bounded integer arithmetic. Standalone builds on MSVC/GCC/Clang each pass 100
unit checks and 5,632 probe records. Public client-disabled integration also
passes 13 selected tests and the complete corpus per compiler, without graphics
targets or direct imports. The [legacy float formatter](../core/java-float-format.md)
also matches 5,708 retained JDK rows and 49,532 separately derived model cases
per compiler. The [integer parser](../core/java-integer.md) also passes 43 unit
checks and all 65,536 one-unit comparisons per compiler through the public
client-disabled core. The new GameSettings owner builds on those primitives;
its verification has the separate scope below.

The [output writer](../core/java-writer.md) adds counted UTF-16 encoding and
PrintWriter policy without graphics dependencies. Its public native builds
pass all 15 selected cases per compiler: 110,708 stream checks, 19 real-file
checks and five independent file checks, with Linux sanitizers. Independent
raw-output audits and later metadata inspections retain two controller footer
failures separately from the successful tests. A later input-driven observer
passes 25 transport controls per compiler. Its Windows comparison matches all
13,391 declared fields across 68 original/native scenarios; all 14 file
corruption controls reject the intended changes. Thread controls and private
state remain unpaired. See the writer contract for the finite projection.

The [GameSettings reference](../reference/game-settings.md) records all 40 declared
fields and three original persistence profiles. The
[native persistence owner](../client/game-settings.md) now implements defaults,
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

The [GameOption and label module](../client/game-options.md) now implements option
metadata, value queries, counted UTF-16 labels and the retained legacy key-name
table. Direct Translation initialization is separate from wrapper use. Native
Windows builds pass 883 GameOption, 1,241 label and 463 Assets checks, with
persistence and Translation regressions. A file-only audit qualifies the
headless tests after their controller's summary-parser failure; its failed
report remains unchanged. The later 675-case hosted run also passes the native
contracts on sanitized Linux GCC and Clang. Original label correspondence and
options screens remain separate. The new App startup connection has only the
finite Windows validation above, with Linux startup execution still pending.

The [binding-code action](../client/game-settings.md#binding-code-action) now
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

The [WorldAccess interface](../world/world-access.md) now dispatches typed
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
precede the [canonical naming migration](../reference/minecraft-names.md).

The renamed code also passes all 619 local tests and 587-file formatting,
including the schema-2 redstone producer's native controls. Controller mirrors
and lifecycle interval checks pass on Windows and Linux. Recorded fixtures keep
their source pins, with explicit historical/current module families and a
separate immutable WorldProvider recorder. These checks cover the represented
native contracts and finite replays; original nonempty WorldAccess and redstone
constructor-input correspondence remain unfinished.

The [synchronous redstone hooks](../world/redstone.md) now use actual World mutations,
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

The [redstone position Set](../reference/redstone-position-set.md) has reclaimable
Registry storage, full coordinate equality, bucket/tree ordering and snapshots
that survive clear/refill. Finite original comparisons pass conditionally on
measured identity hashes on MSVC/GCC/Clang. Windows development tests pass;
five Linux mounted-source timeouts pass separately from an identical native
source copy. The reference record preserves the failed full-suite results and
development-source attribution. Its [committed collection checkpoint](../reference/redstone-position-set.md#committed-collection-checkpoint)
separately passes all 607 hosted public/JDK tests and predates redstone integration.

The [dedicated checkpoint](../reference/dedicated-world-construction.md#committed-checkpoint)
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
[reference record](../reference/dedicated-world-construction.md) corrects the earlier
byte-equality claim. The eighteen public Python checks and eight actual-native
corruption controls also pass in the current checkpoint. Earlier named-data
results below retain their own qualified source and scope.

The production GAME World matches the original seed-23 origin and ring
population profiles on client and base-server sides. The ten-call ring retains
nine normal Chunks, populates four and ends with 6,968 ordered light regions.
The comparisons include complete retained arrays, cache state, provider
numerical state, array aliases and Random continuations. The
[fresh World evidence](../reference/natural-population.md) defines the finite
schedules and their limits. Broader population acceptance remains open.

The public ring replay checks full original-derived numerical and cache golden
records. The public origin replay keeps its cache-only oracle. Both run the real
native probe with separately declared minimal resource files.

The [populated Chunk persistence comparison](../reference/population-persistence.md)
adds two real forced cache saves and four direct region loads after the origin
prefix. It checks saved typed NBT, five loaded arrays, held Info,
cache/numerical state and raw disk preservation. The
[held World full-save profile](../reference/population-full-save.md) adds metadata
and empty named SavedData lists. The separate
[named MapData profile](../reference/population-named-data.md) publishes one dirty
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
lifecycle-interval controls. The [source and capture audit](../reference/population-named-data.md#committed-checkpoint)
verifies 1,073 clean-source files, 30 fresh/public captures and 30 resource-variant
pairings. [Hosted verification](https://github.com/frankischilling/minecraft-c/actions/runs/39964552445)
passes all 583 public/JDK executions and formatting/mirror/interval checks;
all four actual checkout trees match the code checkpoint. The eighteen
supplied-archive comparisons retain their local scope.

Plant lifecycle callbacks, farmland behavior and World block-light queries are
implemented.
The [plant](../world/plants.md) and [World-query](../world/queries.md) contracts describe
the implemented behavior and its limits.

Original client and server projections match all 59 prepared cases per side
on each compiler. The comparison covers block/metadata maps, both Random
continuations and ordered deterministic Item fields. These fixtures execute
unchanged original Block and selected tree bodies with prepared World services;
original World/Chunk construction, general notification and natural population
retain separate acceptance requirements. See the [evidence](../reference/plant-lifecycle.md).

Other implemented modules have separate contracts and verification scopes:

- [Minecart construction](../entity/minecart.md) and
  [inventory, semantic NBT and storage](../entity/minecart-inventory.md).
- [Rails](../world/rails.md), [World power queries](../world/power.md),
  [pressure plates](../world/pressure-plates.md) and
  [Piston TileEntities and geometry](../world/tile-entities/piston.md).
- [Item and FallingSand movement](../entity/movement.md), including collision and
  reached World effects.
- [Overworld population](../world/population/overworld.md), its feature bodies and the
  [live save path](../world/saving.md).

The [finite Minecart constructor comparison](../reference/minecart-construction.md#constructor-observations-and-qualification)
matches ten null-World inputs per side against Windows MSVC. A separate
[inventory and semantic NBT comparison](../reference/minecart-inventory-nbt.md#finite-inventory-and-semantic-nbt-comparison)
matches its complete declared projections on both sides. GCC/Clang inventory
comparisons remain pending. These results do not establish Minecart movement,
Player interaction or rail physics.

Whole-game behavior remains unfinished. Active Minecart ticks and rail travel,
complete redstone propagation, population and persistence acceptance, all four
multiplayer pairings, native graphical play, controls and audio remain required.
There is no playable C client or dedicated server. Account authentication is the
only approved gameplay exception.

See [testing](../testing.md), the [parity matrix](../parity.md) and
[roadmap](../roadmap.md) for current requirements. Detailed earlier results remain
in the [population and runtime history](../history/population.md) and
[earlier integration history](../history.md).


## LocalPlayer NBT and Lava checkpoint

At `0a330b2`, before RenderEngine/Biped integration:

The combined Lava animation, LocalPlayer NBT, singleplayer controller, camera,
World Entity update, spawn/reset and weather source passes 1,072 local CTests:
376 Windows MSVC Debug cases and 348 cases on each Clang 18/GCC 13 Debug build
with ASan, UBSan and leak detection. Every case from the preceding 362/335/335
matrix remains enabled.
No case failed, skipped, was disabled or remained unrun.

The frozen build cohort contains 1,822 files and 1,586 non-Markdown inputs.
All 910 C/header files pass clang-format 18.1.3. Before execution, 236 current
native/helper fingerprints and nineteen original class/Code metadata families
verify. Source inventories and actual configured artifacts, runtime libraries,
symbols and compiler/link configuration remain unchanged across each full suite.
All 277 configured Windows executables retain the default 1 MiB stack reserve.
The subsequent documentation update changes none of those non-Markdown inputs
or the reference README files pinned by their comparison packages.

Windows includes real OpenGL startup, three executable World inspection captures,
GPU camera drawing and the original camera comparison. Local Linux coverage is
headless; the CPU controller and camera closures have no SDL, OpenGL or FreeType
dependency. Hosted public/JDK checks run separately.

Independent readback verifies 230 ordinary and 256 allocation checks for the
singleplayer controller on each compiler. Each native target matches twenty
unchanged-original records. Windows runs a fresh original comparison through
the public wrapper; Linux native comparisons retain their Windows-original
observation scope. The existing World Entity, weather, spawn/reset and camera
capture audits also pass without changing their declared scenarios.

Each combined compiler product passes 132,659 Lava checks. Fresh unchanged-original
CPU execution on Windows and Linux matches all 348 array, RGBA and Math records
across 23 cases and 31 animation calls. This exercises the CPU constructor and
animation body; application publication, texture registration and GPU upload
remain unfinished.

LocalPlayer NBT passes 490 checks, coordinate ownership passes 110, the selected
diagnostic union passes nine and World Player publication passes 169. A fresh
Windows original run matches 33 cases and 1,302 normal tag nodes. Both current
Linux executables match those unchanged retained Windows observations through
the strict receipt comparator. Three LastPlayed values have separate clock
brackets. The distinct wake93 prefix contains 37 records, including 33 tag nodes;
native full-wake parity remains unfinished. These results include the actual
first-Player metadata save and saved Info.Player load/publication paths. They do
not establish a gameplay save/reload session.

Normal application Player/view publication, complete controller/World entry,
World time/block scheduling, physical input, dynamic rendering and the playable
frontend remain unfinished. The scoped method comparisons below describe their
supported operations and remaining descendants.
