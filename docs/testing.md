# Test scope and reproduction

## Renderer attachment and image-worker qualification

The Windows MSVC Debug ALL build and all 482 selected CTests pass with zero
failures or skips in 209.59 seconds. There are 528 configured tests;
`-LE reference` excludes 46. All five native World-selection profiles and the
three optional application-constructor/ownership cases are included. The
independent headless Release ALL build also passes. clang-format18 checks
1,405 C/header/selected fragment files, and strict GCC13/Clang18 checks pass
for all 43 affected translation units, 86 checks total.

The new controls cover renderer World attachment and source DK bounding boxes,
logical list replacement, exact WorldAccess failure receipts, Player loading,
appearance and HTTP/ImageIO/RR workers. Full saved-World loading returns at
outer bytecode130; healthy null-screen focus returns before gameplay input337.
Foreign renderer and particle-manager occurrences retain the same World and
Player through BUSY cleanup. A separate current Release generation profile
also reaches that return and completes cleanup under a 300-second process
bound. That bound is not an elapsed-time benchmark.

GCC13 and Clang18 ASan/UBSan runs each pass 13 public headless cases and two
private CPU-wrapper cases, 30 total without failures or skips. Leak detection
is enabled. The wrappers compile the actual Engine/Font/RR source closure
with pinned dependencies and use no SDL/window/graphics context. They exercise
the original-grid pixel hash, concurrent image holds and genuine Engine CPU
construction. These results do not qualify a complete Linux graphical session.

The first complete Windows run exposed three stack overflows in Player action,
active tick and allocation controls. A real debugger trace follows Player
action/travel through movement to the World rain query. Complete nested error
records exhausted the existing 1 MiB native stack. Per-invocation checked heap
workspaces reduce the action/travel stack reservations to 64/288 bytes while
preserving full public diagnostics and source order. The same three unmodified
tests then pass on Windows and both sanitized Linux compilers. No stack-reserve
increase or removed assertion supplies the result.

The [image observer](../tests/reference/client/image_buffer/README.md) separately
matches 524,288 authored original-JAR pixels across two raster families.
Native image processing checks the canonical 262,144-pixel grid against its
original hash. The loopback image-factory control uses 14 GETs, 12 authored
routes and six release barriers. These finite checks do not establish original
HTTP timing, live-service behavior or terrain pixels.

## World selection in the default runtime

The genuine Windows native loop passes empty, populated, sixteen-save scrolling
and double-click save-directory
profiles. It opens the actual RQ from GuiMainMenu, obtains and sorts the real
SaveFormat List, draws and selects its rows, and returns to the exact parent
on Cancel. Held-button entry and trailing keyboard input keep the original
source receiver after synchronous screen publication. Select constructs the
actual `os` controller and enters the retained source World-load call. Version-zero
saves reach the conversion dependency at bytecode 52. Holding an old RQ blocks teardown until its external
occurrence is released; cleanup then completes.

The actual scrollbar drag reaches the lower clamp at 436 and returns to zero
without selecting a row. A second click within the real 250 ms window reaches
the controller/loader failure before LG's later index, clock and drag stores.
The startup owner refuses teardown while the runtime graph holds its lease.
A separate prepared native monitor-failure receipt checks retryable guard
cleanup, active-call refusal and idempotence. No host mutex failure was forced.

The profiles use isolated native APPDATA directories and authored NBT metadata,
including `level.dat_old` fallback. Save and imported-resource hashes remain
unchanged, with no new session lock in these menu/conversion profiles. Their empty and populated 854 by 480 captures
were inspected, along with the bottom-scroll capture. Create/Rename still stop at `NEW162`/`NEW189`; Delete stops at
preparation 21 without skipping its earlier source work. See the
[World-selection contract](client/world-selection.md).

The additional existing-save Windows profile supplies version 19132 metadata
and all 289 authored Chunks in real zlib region files. It verifies source
`CLIENT_LOAD_OR_CREATE`/`READY`, all 289 x-major progress-before-block samples,
light drains through false, the final message/maintenance, and the same-App
Player/input publication. Renderer and particle attachment, actual Player
insertion, and view publication now complete. World loading returns at 130;
healthy null-screen/focus handling follows, before the same tick stops at
gameplay input `Minecraft.k:337`. A stored block 57 marker distinguishes the real
file-backed Chunk from generated terrain. Actual loading creates its session
lock; every pre-existing save byte and imported resource remains unchanged.
Foreign retained Loading and SaveFormat failure controls verify that BUSY
cleanup preserves their original owner and child identities.

A separate `--mode generate` Release invocation keeps only the stored marker
Chunk and generates its surroundings through the same complete preparation.
The current rebuilt Release profile passes all 289 samples, renderer/effect
attachment, Player insertion, view publication, healthy null-screen focus and
cleanup with a 300-second process limit. It retains the same stored marker. The default five-profile integration selection
uses existing Chunks; the optional generation profile has a separate longer
default limit for Debug builds. Neither profile establishes rendered World
pixels, playability or save interoperability with the original game.

Seven focused headless source tests pass on each of GCC13.3 and Clang18.1.3:
handler construction/read/close, SavedData ordinary/close, source World
construction and regions. Both Linux Debug builds use strict C17 warnings,
ASan, UBSan and leak detection: 14 tests pass without skips. The World case
executes eight constructor scenarios; region controls include live Unix
descriptors and renamed paths, incremental compressed input and retained
malformed-reader cleanup. This is runtime qualification, separate from earlier
syntax checks and from the Windows Display integration.

Three further headless cases per compiler pass after the cache/gravel changes:
diagnostic layout (14 checks), Chunk finishing/cache (3,511) and falling (616).
These six tests also use ASan/UBSan and leak detection. All 24 selected
translation units pass strict GCC/Clang syntax checks, 48 checks total, with
pinned dependency headers and no extra diagnostic suppressions.

The generated-terrain profile exposed a native stack overflow during recursive
gravel updates. A 75-block authored column reproduces it in 0.63 seconds before
the fix. Checked heap diagnostic workspaces preserve synchronous notification
and immediate-tick order; the focused Windows case then passes 616 checks.
The [optional original observer](../tests/reference/world/falling/README.md)
uses the unchanged pinned client JAR and genuine World/SaveHandler constructors.
It independently confirms 38 surviving blocks at Y5..42, 150 scheduled-record
identities, unchanged F/E and unchanged World Random for that exact scenario.
This unusual result follows from source reentry and discarded setter booleans;
the test does not substitute a mass-conserving gravity algorithm.

Cache validation was also measured on the actual source function before its
change. For 289 entries, 5,000 validations improve by 18.5..31.5 times across
strict GCC/Clang O0/O2 probes. The bounded scratch index still validates every
mutable key, Chunk and insertion; it adds no allocation or cached validity flag.
Larger caches retain the existing exact fallback. These microbenchmarks do not
establish an overall client startup speedup.

DateFormat matches all 2,926 JDK17 timestamp vectors on Windows and Linux for
English/en_US, UTC/US Eastern and UTC years 1967..2099. Other locales, zones and
ranges return explicit unsupported errors. The
[observer and input provenance](../tests/reference/platform/date_format/README.md)
record the selected JDKs, historical Eastern transitions and exact UTF16 scope.
These observations qualify the formatter's UTF16 results. Original menu pixels
and gameplay remain unqualified.

The preceding source-loading Windows MSVC Debug ALL build and all 475 selected
CTests pass with
zero failures or skips in 208.25 seconds. This selection includes all five
native UI profiles, the source storage/World controls, the gravel regression
and all three optional application-constructor/ownership cases. There are 521
configured tests; `-LE reference` excludes 46. The independent headless Release
ALL build also passes. Formatting checks 1,379 C/header/selected fragment files.

The preceding World-selection Windows MSVC Debug ALL build passes, followed by all 468 selected CTests
without failures or skips in 197.43 seconds. The selection uses `-LE reference`
and excludes 46 configured reference-label profiles. The four native
World-selection profiles and the corrected startup fingerprint are included.
A separate headless Release ALL build passes with client, sound, audio and
testing disabled. Clang-format18.1.3 checks 1,356 files. Eleven affected
production translation units pass strict GCC13.3/Clang18.1.3 C17 syntax checks
under WSL; those checks are separate from a complete Linux build and suite.
Earlier local and hosted checkpoints below retain their source scopes.

The complete 468-case selection had the optional application-constructor
profiles disabled. Enabling them adds two constructor cases and an inventory
callback-ownership case. All three pass separately in 0.89 seconds after the
diagnostic-only change described below. That is not a complete 471-case rerun.

## Hosted World-selection checkpoint 1334db5

The [exact World-selection run](https://github.com/frankischilling/minecraft-c/actions/runs/40813601193)
is terminal with one failure. Windows passes 440/440 in 243.78 seconds and
Clang passes 461/461 in 370.74 seconds. GCC passes 460/461 in 193.30 seconds.
Formatting passes for 1,356 files. All unique test starts/results match, with
zero skips; every checkout tree matches `1334db5`. The fingerprint regression
passes on all three hosts; `client.startup` passes in both Linux selections.

GCC's sole failure is `client.application_construction_later_owners`: the third
profile reports `MC_CLIENT_DISPLAY_SDL_ERROR` while creating its real Display.
The first Display profile succeeds in that invocation, and refusal cleanup
completes. The log does not contain the captured SDL operation or detail, so
the underlying cause remains undiagnosed. A diagnostic-only follow-up prints
those existing error fields before cleanup, without changing the failure,
assertions or runtime behavior. Its two native Windows constructor profiles
and strict GCC/Clang syntax checks pass. The later hosted result follows below.

## SDL initialization and Xvfb lifetime

The [exact `d71bd94` run](https://github.com/frankischilling/minecraft-c/actions/runs/40815149868)
is terminal with one failure. Windows passes 440/440 in 278.67 seconds and
Clang passes 461/461 in 372.76 seconds. GCC passes 460/461 in 348.97 seconds.
Formatting passes for 1,356 files. Every unique test start has one result,
with zero skips and matching checkout trees. The test-name sets are unchanged
from `1334db5`; all three timing caches restore. The preceding diagnostic-only
run is cancelled by the configured supersession policy.

GCC's sole failure is now `client.display_sdk_transport`. Its native child
fails `SDL_InitSubSystem(SDL_INIT_VIDEO)` in `pending_failure`, before the
pending-event assertions. The supervisor correctly rejects its nonzero exit.
The later-owner case passes in both Linux selections. Neither hosted refusal
retains SDL's underlying error string, so their exact causes remain unrecorded.
The SDK test now prints `SDL_GetError` immediately on failed initialization,
preserving the single call, failure, checks and cleanup.

A bounded private WSL reproduction uses the pinned static SDL3.4.18 library
(`SDL-release-3.4.18-0-g829a65d76`, `SDL_X11_SHARED=OFF`) with the X11 driver and
the same default Xvfb screen. Four processes each repeat actual video init,
hidden-window creation/destruction and video quit up to 200 times. Across four
fresh default servers, three runs fail: six processes report
`x11 not available` at the real SDL init call and one exits on SIGPIPE. One
default run passes; it is retained in the result. Single-process repetition
passes 200 window cycles, and four processes without windows pass 800 init/quit
cycles.

Changing only the Xvfb server argument to `-noreset` passes all 2,400 window
cycles across three fresh servers and twelve processes. Each such server log
contains one keymap initialization block; default logs contain repeated blocks.
X.Org documents that
[`-noreset` prevents reset after the last client disconnects](https://cgit.freedesktop.org/xorg/xserver/tree/doc/Xserver.man.pre?id=ca5ebe3971d8ebcfed00c5ebcd026cdd0ce0c6ba).
These matched runs identify automatic reset as a cause of the reproduced
failure pattern. They do not recover the missing error strings from hosted CI.

Linux CI now keeps that owned Xvfb server alive between clients with
`-screen 0 1280x1024x24 -noreset`. It retains four CTest workers and the complete
suite. The actual SDK transport test passes all 37 checks in the private
sanitized Linux build; a real unsupported-driver refusal confirms the new
diagnostic and still exits with failure. The exact
[`9d83665` hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40819123976)
then passes all 1,362 selected tests: Windows 440, GCC 461 and Clang 461, with zero
skips and matching checkout trees. Formatting passes for 1,356 files. Both
earlier Linux refusal cases pass and every test name remains in the selection.
This green checkpoint precedes the source World-loading extension; it does not
recover the old missing SDL strings or establish a general CI speedup.

## Default source menu runtime

The preceding Windows MSVC Debug ALL build includes the default client runtime.
All 463 selected CTests pass without failures or skips; `-LE reference`
excludes 45 configured reference-label profiles. A separate headless Release
ALL build passes with client, sound, audio and testing disabled. Formatting
checks 1,341 files.

That 463-case run precedes the diagnostic-site transport correction described
in [status](status.md). The affected runtime translation unit also passes strict
GCC and Clang syntax checks under WSL, and three affected native Windows tests
pass after that correction. The later `de4290b` hosted result is recorded below.

`client.source_menu_loop` constructs the genuine App and startup graph, updates
all nine registered effects with 15 real atlas uploads, advances the Timer and
source menu counters, reaches the one-second FPS path, then dispatches SDL
mouse events to Quit and retires the graph. A prepared `V=5999` control checks
the approved omission of account-session validation at tick6000. The executable
also produces an 854 by 480 PNG through its default entry point. A separate
100-frame, 32-tick run and its capture were inspected. These Windows checks do
not qualify original visual correspondence or a playable session.

The authored-resource upload test covers duplicate effects, repeated tiles,
separate texture IDs, anaglyph, actual GPU readback, preserved GL errors and
unsafe unpack rejection. Four-level mipmap uploads remain unexecuted. Source
events cover mouse-before-keyboard draining and unread tails at a dependency.

Published predecessor `3ec9892` passes [all hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40796960416):
434 Windows, 452 GCC and 452 Clang tests, 1,338 total, without failures or skips.
Formatting passes for 1,325 files; all tested checkout trees match that head.
The runtime has its own later hosted results below. Full source run/PX/shutdown,
World loading, other menu destinations and gameplay remain unfinished.

## Hosted runtime checkpoint de4290b

The [exact `de4290b` run](https://github.com/frankischilling/minecraft-c/actions/runs/40802203908)
is terminal with one failing test. Windows passes 435/435 in 329.63 seconds and
Clang passes 454/454 in 189.45 seconds. GCC passes 453/454 in 339.00 seconds.
Formatting passes for 1,341 files. Each native job has matching unique test
starts/results and zero skips; all four checkout trees match the published head.
The effect-upload and source-event cases pass in both Linux selections.

GCC's sole failure is `client.startup`, before launching the client. The old
`raw_file` guard used one assertion for regular-file type and the 128 MiB bound.
Its log reports neither actual mode nor size. A larger linked artifact is the
size explanation inferred from the completed link and resolved executable path;
the original bytes were not measured. This failure supplies no SDL/GL result.

The current executable fingerprint uses a separate 1 GiB bound and 1 MiB streaming
buffers, checks identity and size across the open/read/close operation, and
retains the receipt's before/after equality assertion. General resource files
remain bounded at 128 MiB and captures at 16 MiB. Failure output now includes the
actual file type, size and bound.

`tools.startup_executable_fingerprint` creates an authored sparse regular file
of 134,217,985 bytes, computes its independent expected SHA256 including a nonzero
tail, and checks the complete hash with bounded Python allocation. It also
checks stable identity, directory and over-limit rejection, and unchanged
resource/PNG limits. The regression reproduces the old guard failure and passes
with the streaming fix on Windows and Linux, normally and under optimized
Python. Its `.exe` filename also reproduces Windows' path/handle execute-bit
difference: path stat infers execute bits from the extension, while descriptor
stat has no filename. Cross-API identity compares file type; full modes still
must remain unchanged within each API. The real Windows executable passes two
independent complete-hash comparisons. No large binary fixture is committed. This correction is included in
the World-selection increment; its later hosted result is recorded above.

## Normal startup through menu initialization

The preceding startup checkpoint's Windows MSVC Debug ALL build passes, including the client, followed
by 460 selected CTests with no failures or skips in 207.15 seconds. The selection
uses `-LE reference`; configured reference-label profiles remain outside it.
Normal and GL-error window cases reach GuiIngame publication, typed statistics
saving, current-screen publication and scaled menu initialization. Startup
returns at `749` and stops before `Minecraft.run` clock `32`. The executable's
default title/inspection loop was separate at that checkpoint.

Nine focused cases cover startup, RenderGlobal, source statistics, File.delete
and MouseHelper CPU behavior. Four additional MouseHelper window runs each pass
91 checks; CPU controls pass 4,225. The fixture transfers real SDL motion into
the retained Display transport before polling and checks exact deltas 7/9.
MouseHelper adds no event pump or FIFO removal. Cleanup controls retain external
Screen, GuiIngame and menu-particle descendants while checking App dependencies.

Formatting passes for 1,325 files. The separate Release ALL build with client,
sound, audio and testing disabled also passes. Current original menu/visual and
Linux startup qualification remain unfinished. The failed `8288290` hosted run
and its two test-source corrections are recorded in [status](status.md).

## Resource worker and source menu integration

The earlier resource-worker checkpoint builds ALL on Windows MSVC Debug and
passes 460 selected CTests without failures or skips:

```powershell
ctest --test-dir build-client-startup-20261027b -C Debug -LE reference --parallel 4 --output-on-failure
```

This selection excludes the configured reference-label profiles. It includes
actual startup windows through resource-thread construction, `U676`, worker
start and the `Post startup` GL diagnostic. GuiIngame caller allocation at `697`
remains unconnected. Both normal and injected GL-error paths pass, along with
RenderGlobal device behavior and retained startup failures. Windows startup
controls isolate APPDATA while preserving the real native home. The default
executable still uses its title/inspection loop.

The selection includes six [resource-worker](client/resource-download.md) cases,
including a real loopback HTTP peer, and seven [source-menu](client/source-screens.md)
cases. Four separately observed original constructor scenarios retain their
finite scope; they do not verify the complete original worker or menu behavior.
The formatter checks 1,319 C/header/fragment files. A separate Release ALL build
passes with client, sound, audio and testing disabled; no dedicated-server runtime
result follows from that build. Current Linux qualification remains separate.

CI retains four test workers and its complete existing suites. It caches CTest
timing estimates by platform/compiler so later runs can start long tests early.
GCC restores these estimates in the `3ec9892` hosted run and starts long cases
early. Its test step takes189.20 seconds versus253.61 seconds in the preceding
cold run; source and cloud differences prevent a causal speedup claim.

<a id="current-public-runtime-integration"></a>

## Current client runtime integration

Hosted baseline `5d61f89` passes [all four jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40746811719):
410 Windows, 427 GCC and 427 Clang cases, 1,264 total, without failure or skip.
The later focused startup results below retain a separate source scope.

Earlier `6249a64` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40692181754):
407 Windows, 423 GCC and 423 Clang cases, 1,253 total.

Earlier checkpoint `37dc83f` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40544932686).
The audited logs contain 1,127 unique Start/Passed pairs: 367 Windows, 380 GCC
and 380 Clang cases, with no skips or failures. All 1,115 cases from `e8fcb3a`
remain; each compiler adds four resource-observer checks. All four actual
checkouts use `5b671b26e8d5922b41e61423a569249c4868b76e`, whose complete tree is
`fdda932e6f3578c1ec37cde5d0762cbc2601a3ed`, equal to the named checkpoint.
Independent readback rehashes the raw logs and source archive and verifies all
2,027 archived blobs against that tree. Formatting covers 1,027 C/header files.
Controller mirrors and 556 positive/2,011 negative lifecycle controls pass on
every compiler platform.

The observer option compiles both probes and runs four asset-independent Python
protocol/admission tests. The existing constructor, array, SoundManager, loading,
source-rendering and ZIP controls remain; Linux also retains the font,
floating-point and later-owner graphics cases. These are hosted public/native
and enabled JDK checks. Supplied-original profiles stay opt-in, and later
startup source proposals are outside this checkpoint. The earlier native
producer and retained original result keep their separate scope below.

The startup/resource increment has separate focused local qualification: 48
logical checks per compiler, comprising 47 CTests and the direct later-owner
regression. Each passes 774 constructor checks and 196,715 array/class checks.
The Linux profiles use ASan, UBSan and leak detection. Windows precedes the
POSIX-only unused-parameter annotation; both Linux profiles use that correction.
This selected scope is not a new complete local suite or an original-game run.
Normal startup adoption, physical input, World entry and gameplay remain unfinished.

The preceding published `4a6b805` [hosted checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40464703757)
passed 1,055 public/JDK cases: 345 Windows and 355 per Linux compiler. All four
checkout trees matched `c3c7e45cb8b9b94d86078cea1dd8ac41289c0315`; its formatting
scope was 936 C/header files. The `37dc83f` hosted run retains every one of those cases.

## Normal startup owner qualification

### Startup scene construction and typed Settings save

The current local increment builds the native client and passes 24 selected
Windows CTests. Six cases belong to source snapshot `ff886a21`; eighteen belong
to `5f0bf5d0`. The latter changes only the RenderGlobal device control and four
current input maps. Production C/headers, CMake, flags and all other tests are
byte-identical. Earlier successful cases retain their original scope and were
not rerun to change labels. All 24 exact names/JUnits, process results and
current/historical evidence closures received independent read-only review.

The selection covers typed Writer and Settings save, normal and real-GL-error
Sessions, GuiIngame/RenderGlobal CPU construction, RenderGlobal device behavior,
four startup allocation bounds, Settings compatibility/actions/sound,
DataOutputStream/Stats and App ownership. Windows retains the default 1 MiB
stack, strict C17/floating-point settings, actual link maps and the unchanged
265-product SDL cohort, including 38 driver objects. Source, tools, original
inputs, imported resources and configured products remain fixed through each
admitted run.

| Actual native control | Checks and reached behavior |
| --- | --- |
| Typed FileWriter | 582; same FOS/File identity, source/native close distinction, quota/BUSY restoration and error precedence |
| Typed Settings save | 202; two saves, exact 25-line canonical-stream readback, shorter truncation and reload |
| Normal Session | 181; sound bind488, nine registrations, actual n625/g628/viewport641 and dn657/j660; next unexecuted NEWcz664 |
| RenderGlobal device | 16,723; actual triangle records/XYZ copies, sticky error, independent namespaces and full retryable cleanup |

Five genuine earlier build/runtime failures remain preserved: missing source
declarations/include path, device OpenGL linkage, a wrong physical-close count,
native-status loss at the new Writer adapter and wrong quad counts in the device
control. Corrections retain the core Writer and original authority. The final
device test checks six emitted triangle vertices per logical quad and the
original `[0,1,2,0,2,3]` copied-position order; disabled attribute slots are not
given invented equality.

Clang 18 passes 41 selected cases on production snapshot `ff886a21`: Writer and
Settings save first, 32 further CPU cases, then seven actual-device cases under
an owned authenticated Xvfb. ASan, UBSan and leak detection are enabled. Product,
SDK, source, tool, dependency, imported-resource and real-HOME guards close
without change. The later Windows device-test-only successor is outside this
selection. GCC qualification remains separate.

These selected cohorts are not a complete local/hosted suite. They execute no
original JVM/game, establish no original pixels or star/trigonometric
correspondence, and do not cover Linux default-directory/Session startup,
World entry, input/frame/tick scheduling or gameplay. The default executable
still uses title/fixed-camera inspection. The next ordinary startup child is
the resource download thread; GuiIngame caller publication and menus follow it.

### Startup tail through GL diagnostics

The earlier focused startup-tail source builds the client and passes 47 selected
Windows cases without failure or skip. Nine Core output cases retain their
separate preceding Windows test-header source. Clang 18 and GCC 13 each pass
53 selected cases: 41 CPU/protocol and twelve real-device cases, with ASan,
UBSan and leak detection. These are selected runs, not complete local suites.

Fresh Windows normal and real-GL-error Session profiles pass 103 and 109
checks. They reach Controllers.create383, diagnostic397, GL setup through460
and diagnostic466. The last App publication is C380 and the next unexecuted
operation is cx470. Output checks use the real System.out/PrintStream pipeline;
the nonzero diagnostic does not consume an extra GL error. Physical nonempty
Controller mapping remains the SDK Native12 frontier. Official SDL HIDAPI and
virtual joystick drivers are enabled for the actual driver-backed controls.

Source, tools, dependency inputs and configured products retain their recorded
before/after checks. Windows keeps the default 1 MiB executable stacks. Linux
device cases use owned Xvfb contexts. Linux default-directory/Session startup,
original/JVM observations, a complete local test suite and playable World entry
are not covered. A separate Release headless build completes all 372 build
steps and links `mc_region_inspect` with `MC_BUILD_CLIENT`,
`MC_BUILD_CLIENT_SOUND`, `MC_BUILD_CLIENT_AUDIO` and `BUILD_TESTING` disabled.
No runtime tests ran in that graph; utility linkage does not qualify server
behavior.

Published `6249a64` separately passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40692181754):
407 Windows, 423 GCC and 423 Clang cases, 1,253 total. That published suite does
not include this later startup-tail source.

### Earlier scoped results

The [source OutputStream encoder](reference/source-encoder.md) has a separate
target-only qualification: fifteen selected cases pass on each MSVC, Clang 18
and GCC 13 build, 45 executions total. Each new contract case passes 91 checks.
Actual inventories contain 354/356/356 cases; those inventory counts are not
pass counts. All selected cases run without failure or skip, Linux sanitizers
remain enabled, and link maps exclude a second Writer archive object. This
headless scope does not claim a full client or System.out implementation.

Checkpoint `671c003` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40639713371).
Actual job logs contain 398 Windows, 414 GCC and 414 Clang Start/Passed pairs:
1,226 total, with no failure, skip or disabled case. Formatting checks 1,164
files. Early input preflight checks 4,085 entries covering 2,209 distinct files
on each native job before configuration; the native tests remain enabled.
This checkpoint fixes the Font test's terminal lookup-metadata cleanup.

The newer startup composition contains 2,247 files. Its initial full Windows
build completes 1,587 steps and links the client. New statistics admission and
null-Z controls pass 170 and 65 checks. The genuine Session window profile
passes 92 checks through statistics publication/callback, actual logo caller,
keyboard/mouse creation and App.C380, then stops at Controllers383. It starts
one real read worker and performs no startup join. The callback keeps its App
alive until the actual Registry-owned occurrence retires. This process belongs
to the initial source, before two later strict-warning corrections.

The first correction explicitly initializes the appended statistics-error
pointer, satisfying Clang's missing-field diagnostic. The Windows incremental
refresh recompiles one owner translation unit and relinks thirteen dependent
executables. The corrected source passes the foreign-ready admission profile
(78 checks), default/shared-Font failure profiles (82,137/82,142), five prepared
logo modes (419 total), nineteen selected CPU CTests and two cursor cases
(65/6,169). Across the two retained source phases, 26 actual CTests and two
direct window profiles pass. All 327 built executables retain the default
1 MiB stack; 758 owned C recipe groups retain strict C17 diagnostics/FP.

Clang 18 builds the client and passes 56 selected CPU CTests plus six real
graphics cases under ASan, UBSan and leak detection on that first correction.
The authenticated Xvfb display is verified by `xdpyinfo`; its supported abstract
Unix transport does not require a filesystem socket. Two WorldInfo controllers
copy C/header inputs for binding checks and execute the actual sanitized CMake
binaries. Those copied inputs are recorded separately from build products;
they are not standalone native compilations.

GCC 13 subsequently detects a signed/unsigned intermediate in the input trace
assignment. Splitting the two exact constant-380 stores preserves runtime and
ABI behavior. Its four-leaf source/metadata successor has current preflight
4,219 entries/2,246 distinct files. The same GCC graph completes its remaining
802 build steps, links the client and passes 56 CPU plus six authenticated
graphics cases. All 2,608 products remain unchanged through execution and
Xvfb retires normally. The minimal Clang refresh recompiles one input unit,
updates its archive and relinks two tests. All six affected startup, MouseHelper
and SDK queue/icon cases pass. A matching minimal Windows refresh passes the
genuine 92-check Session window profile again. These runs are justified by the
changed trace unit; the earlier full selections are not repeated. Earlier
passing products keep their original source
bindings. Windows default-directory/Session coverage does not credit a POSIX
home-directory or original visual-parity profile.

The corrected `bd8d7f1` [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40635329070)
passes Windows and formatting, and each Linux compiler passes 413 of 414 cases.
The Font child now executes but stops at its final Engine release assertion:
the test retained lookup policy metadata after retiring its pending call.
Its BUSY refusal is correct. Explicit terminal metadata cleanup fixes the test;
all 625 checks and eleven ordered rows pass on Clang/GCC with empty ASan/UBSan/
leak output. The original failed executable, assertion and leak report remain
retained. Production Font/Engine behavior is unchanged. Capture rejection now
includes bounded stdout/stderr tails from the same process. A current-input
preflight precedes native configuration in CI and leaves the full suite enabled.

The later `cd346b0` [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40632949338)
builds both Linux clients. Each compiler passes 406 of 414 cases; the eight
failures are current-source binding checks in App-owner, Biped and source
rendering controls. Windows stops at RenderManager's flexible-array declaration
under the older hosted MSVC C4200 diagnostic. Formatting passes. The scoped type
guard preserves the declaration and layout; all five affected C files compile
under installed MSVC with the existing C17, warning/error and strict-FP flags.
Refreshed bindings describe the current test inputs without relabeling older
native products or original captures. The later `671c003` result above passes.

Published `f37a96f` [passes all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40599463192):
386 Windows, 401 GCC and 401 Clang cases, 1,188 total, with no failures or skips.
The formatter checks 1,115 selected files. Exact-head checkout and complete
Start/finish inventories are verified from the actual job logs. The Linux
transport case passes on both compilers with sanitizer checks enabled.

The published `33c777c` [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40589053675)
fails separately from the focused local results below. Windows stops while
compiling the event allocator: older MSVC treats its C17 flexible array as warning
C4200 under `/WX`. Both Linux compilers pass 400 of 401 CTests, with only
`client.display_sdk_transport` failing. Formatting passes.

A fresh Clang 18 diagnostic reproduces 150,060 leaked bytes in 2,736 Xlib
allocations when SDL dynamically loads and unloads X11. An isolated final
`SDL_Quit` probe still reports those allocations. Using SDL's supported
`SDL_X11_SHARED=OFF` option instead makes X11 an ordinary linked dependency;
the unchanged original transport test exits zero after all 37 checks, with empty
stderr and ASan, UBSan and leak detection enabled. This is one focused Linux
result, not a complete corrected CI matrix. Three affected translation units
compile with the scoped flexible-array compatibility guard on installed MSVC
14.51; the later hosted VS2022 build and tests also pass. The SDK driver includes
bounded stdout/stderr tails when it rejects a failed child.

The later EntityRenderer/RenderManager/loading composition has eight selected
Windows CPU/protocol case results, including 1,673 RenderManager checks. Its
test successor preserves the actual cold Item-first initialization failure and
uses genuine Block-first and Statistics-first entry for successful construction.
Production initialization does not prewarm a Registry to avoid that failure.
The separate Minecraft textured-rectangle CPU helper passes 146 checks for
wrapped coordinates, binary32 UV conversion and retained failure prefixes.

Real Windows graphical coverage adds nineteen loading-draw modes (888 checks)
and nine normal-startup profiles (twelve actual Apps, 173,090 checks). Default
and shared-Font startup reach the explicit offset-324 boundary. The startup
fixture uses the import root for SDK resources and its real `jar/` child for
class resources, retaining both providers through their dependent owners.
One reader correction admits the already captured UTF16 directory trace; the
default native child is not rerun. Imported assets remain private. These runs
do not execute the original game or prove original visual parity.

The separate prepared startup-logo callee passes five real Windows modes:
success (102 checks), null Settings, null Engine, initial FP refusal and FP
refusal after the actual clear (78 each), 414 total. The real coordinator first
stops at offset 324. The control verifies two actual draws, resource/literal
identity, viewport/alpha/swap, reentry refusal and complete outer retirement.
The initial cleanup-harness failure remains retained; its correction invokes
the existing held-item retirement API before RenderManager class retirement.
The public driver's later source scan excludes private/build trees and uses
the existing default-directory opt-in. Its unchanged result admission reads
the same five complete captures without rerunning native children. Normal
startup adoption, Linux graphical execution and original visual parity remain
outside this result.

A combined 2,198-file source builds the actual client and passes 29 selected
cases on each Clang 18/GCC 13 profile, with strict C17/FP and ASan/UBSan/leaks.
Those tests combine twenty File/Logger/output/SaveHandler controls, the eight
renderer/startup CPU/protocol cases and the rectangle helper. A separate
headless configuration with testing disabled contains zero test targets.
This source snapshot predates the later metadata/session and logo additions;
its result is not a full-suite or current-window qualification.

The later [SaveHandler read cohort](world/save-handler.md) passes a 27-case scope
per compiler. Windows combines 26 original passing rows with the corrected
affected handler case; each Linux compiler runs all 27 on the final source.
Only the old assertions that metadata/session methods were unfinished changed.
The real new read/close controls pass 257/283 checks. Existing NBT/Info and
File/Writer/Store regressions remain enabled. These scopes do not establish
World entry, complete persistence or gameplay.

The earlier [normal startup owner](client/normal-startup.md) cohort contains
2,144 files. All three native compilers build ALL and the graphical
client. Windows MSVC Debug passes 25 selected CTests plus 42 disjoint regression
cases, for 67 distinct cases. Clang 18 passes 39 selected cases plus 42 regression
cases with seven deliberate overlaps, for 81 executions and 74 distinct cases.
GCC 13 passes the same 39 plus 42 cases, also with seven overlaps and 74 distinct
cases. None failed or skipped on any compiler. The full configured inventories contain 382 Windows and 384 Linux names;
those inventory sizes are not full-suite pass counts.

All 699 owned-C recipe variants compile on each Linux compiler. Linux uses the
existing C17 warnings, strict floating-point policy, ASan, UBSan and leak
detection. Windows retains its strict C17 warning policy and the default
1 MiB reserve/4 KiB commit in all 306 built executables. The Linux ELF stack
policy and 8 MiB process limit remain unchanged. Source, tools, dependency input
trees and complete configured product maps are checked through runtime.

This selected scope covers save-format ownership, streaming NBT input,
file attributes, Settings construction, SDK queues/icons, the CPU startup
boundary and retained constructor/resource regressions. The CPU startup case
stops before default-directory and device effects. Source assertions and named
failure frontiers remain separate from successful full application startup.

A later real Windows `window-default-directory` process exited zero with 166
authored checks. The supplied resource/SDK import and byte-identical resource
view passed their separate input checks. The process reached the declared
EntityRenderer frontier and completed its ownership assertions. The aggregate
runner stopped on its post-process capacity guard before admitting that mode;
its output grammar also omitted the real achievement and recipe banners.
Six further window modes remain unrun. The retained partial attempt is not a
qualified seven-mode matrix, an original run, World entry or gameplay evidence.


## Resource observer producer and later admission policy

The retained y producer snapshot has fresh Windows MSVC Debug, Clang 18 and
GCC 13 ALL builds. Each compiler passes six selected CTests and eleven fresh
native-only observations: seven color profiles and four real-Display texture
profiles. Clang/GCC use ASan, UBSan and leak detection; Windows executables keep
the default 1 MiB/4 KiB stack. Source, concrete inputs and complete configured
product maps remain unchanged through those runs. This is not a full local suite.

One actual Windows JDK17 CPU default attempt is separately admitted read-only
against the unchanged native producer. The seven water/cache/grass/foliage/
missing/clamp/blur rows match complete arrays, cache identity and selected state.
Independent review rechecks the actual original capture and helper/Code origins,
all 31 hosted JDK origins, source/input pins and closed native product gate.
The initial failed report remains a failure. Six byte-proved JDK class additions
and exact unpadded/three-space library tags resolve the reader's missing proof
and transport refusal; they do not rerun or replace the original producer.

The publication source map and Python/metadata policy are later than that native
producer. Their normal/-O parser controls require no JDK, JAR or private logs;
read-only default admission preserves the old producer and new policy roles.
Only the observer package's own provenance is rebound. Prior producer,
helper-compilation, failed-attempt and successful-review receipts stay immutable.

At `37dc83f`, `MC_ENABLE_RESOURCE_LOOKUP_OBSERVERS=ON` compiles both probes and
passes four public tests on each compiler: `tools.lookup_observation_normal`,
`tools.lookup_observation_optimized`, `tools.lookup_observation_admission_normal`
and `tools.lookup_observation_admission_optimized`. Existing public JDK checks
stay enabled; supplied-original options stay off and no private resources are
required by those four checks. This hosted run is not a new complete local
producer matrix or original comparison. It adds no original ZIP/static/GL,
normal startup, World entry or gameplay result.
See [resource lookup observations](client/render-engine.md#resource-lookup-observations).

## Preceding combined local cohort

The preceding ordered Timer, Minecraft class/worker, RenderEngine, Biped and
World/Player cohort passed 1,163 local CTests: 393 Windows MSVC Debug cases and
385 cases on each Clang18/GCC13 Debug build with ASan, UBSan and leak detection.
Every preceding 389/381/381 case remained enabled. None failed, skipped, was
disabled or remained unrun. Four new public cases per compiler cover ordered
Timer construction and native application-owner contracts/driver controls.

The frozen cohort contained 1,885 files and 1,643 non-Markdown inputs. All 936
C/header files and the separate shared-model trace include file passed clang-format
18.1.3. Before execution, 388 current source/helper fingerprints and twenty-three
original class/Code families verified. Source, configured artifacts, runtime
libraries, symbols and compiler/link configuration remained unchanged. Each Linux
profile bound 1,844 product/configuration files and 338 targets, including actual
extensionless executables and built objects. All 283 Windows primary executables
retained their default 1 MiB stack. Subsequent documentation updates preserve
non-Markdown inputs and pinned reference text.

All three profiles enabled the graphical client, display tests, sound and audio.
They exercised real OpenGL startup, title and World inspection. Each Linux
compiler also closed separate imported-resource title and fresh seed23 captures.
Four authenticated abstract-local Xvfb servers terminate cleanly. Actual Display
identity is Mesa llvmpipe OpenGL4.5 core. Compilation recipes instrument all 42
FreeType units and 599 strict project C17 units per Linux compiler. The 833
FreeType source files match the pinned official archive before and after use.

Fresh Windows production-client and headless ALL builds also passed with tests
and Java/Python package discovery disabled. The headless graph excludes font,
display and audio dependency targets. Its production-client companion disables
optional sound/audio; the complete test matrix independently enables both.
Those local checks are separate from both hosted checkpoints above.

Independent readback verifies 230 ordinary and 256 allocation checks for the
singleplayer controller on each compiler. Each native target matches twenty
unchanged-original records. Windows runs a fresh original comparison through
the public wrapper; Linux native comparisons retain their Windows-original
observation scope. The existing World Entity, weather, spawn/reset and camera
capture audits also pass without changing their declared scenarios.

Each preceding combined compiler product passes 132,659 Lava checks. Fresh unchanged-original
CPU execution on Windows and Linux matches all 348 array, RGBA and Math records
across 23 cases and 31 animation calls. This exercises the CPU constructor and
animation body; that profile did not include application publication, texture
registration or GPU upload.

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

## Ordered Timer and Minecraft class/worker integration

The preceding combined local products pass 1,267 Timer-constructor checks, preserving the
existing supplied-clock API and arithmetic regression. Actual render partial
`c` remains distinct from accumulator `e`. Source wall25/g28 precedes native
nanotime32/h39, including its late floating-point prerequisite. A failed
candidate remains inspectable and cannot retry. Earlier isolated qualification
retains eight fresh original constructors/all ten fields on each host, with
independent clock windows rather than an invented exact raw nanoTime sample.
See [Timer](client/timer.md).

Each compiler passes 225 Minecraft class/worker contracts and 70 driver controls
normally and with Python optimization. The source initializer owns its real
10 MiB byte array and distinct 512-entry long arrays. Actual J/o use one atomic
location each. A genuine joinable native kg worker captures that same App and
preserves its long sleep, interrupt and source-stop behavior. Native cancellation
joins without storing J=false. Each current Linux product matches five fresh
unchanged-original class/worker rows. Windows retains the qualified original
observations and matching current native five-row profile.

The original observer prepares the actual concrete kq with Unsafe, executes the
real Minecraft initializer and creates genuine kg workers. It runs neither the
Minecraft nor kq instance constructor. Its original abstract-Minecraft allocation
failure is retained; the observer-only correction selects the actual concrete
subclass and binds its complete class/Code and origin. This changed no C body.
See [class activation and worker ownership](client/application-owner.md).

The combined freeze initially found one stale transitive Timer-header fingerprint
in the new application reference metadata. Only that current leaf was updated
before compilation. All native suites and post-suite semantic readers then
passed without a runtime correction. The separate older RenderEngine/Biped
qualification failures below retain their original checkpoint scope.

## RenderEngine and Biped integration

The combined products each pass 8,333 RenderEngine CPU checks, 223 real-context
texture checks and 55 separate source/device trace checks. Nineteen controller
controls pass normally and with Python optimization. Fresh unchanged-original
CPU execution matches every one of the 4,096 ARGB pixels and ten glyph metrics
on each actual host. Windows additionally matches ten original compatibility-GL
generation, parameter, cache, device and transfer rows. Linux runs no original
GL profile. See [RenderEngine](client/render-engine.md) for build options,
ownership and remaining pack/String/mipmap/GL_CLAMP boundaries.

Each Biped product passes 4,381 checks and 435 allocation prefixes. Its bounded
driver passes 128 controls normally and with Python optimization. Fresh original
execution matches all 838 constructor/alias/continuation records on each host.
The shared model graph is compiled once; the standalone Biped is distinct from
Notice's model. The existing Notice APIs and default-Random continuation retain
their earlier behaviour. See [Biped construction](client/biped-construction.md).

The preceding RenderEngine/Biped suites passed without a runtime correction. A Windows audit
stopped before tests because it counted EXE plus PDB as two primary executables;
the corrected audit selects one EXE and retains both artifacts. The same graph
then completed, without reconfiguration or rebuilding. Raw post-suite audits
also retain their two obsolete receipt-layout failures: one used host mirror
copy time, another expected an old combined phase list. Both now read the actual
terminal phase receipts. No source, native capture, original record or game
field was rewritten to make an audit pass.

Those local suites did not cover App.p/App.x publication or normal bootstrap.
The later standalone-constructor checks include App.x publication; App.p adoption,
normal bootstrap, model animation and gameplay remain unfinished. These finite
methods and actual frontend scenes do not establish a connected playable session.

## Lava animation and LocalPlayer NBT integration

The corrected native contracts assert all four Living Short getter sites: 4, 29, 39 and 49 in `ls.a(Lnu;)V`. Review found that DeathTime/AttackTime wrong-tag diagnostics pointed at the later field stores 42/52. A fresh native reproduction failed on those two sites before correction and passes afterward. Four native assertions and three added comparator controls retain the correction; the comparator now passes 31 controls normally and under Python optimization. This changes diagnostic locations, without moving source field stores or changing tag values.

`client.lava_animation` runs the native constructor and CPU animation contracts.
Its driver controls run normally and under Python optimization.
`MC_ENABLE_LAVA_ANIMATION_REFERENCE_TESTS` adds `reference.lava_animation` with
the supplied client JAR and explicit matching JDK17 tools. The actual original
profile is CPU-only and checks its class origins and initialization boundaries.
See the [reference package](../tests/reference/client/lava_animation/README.md).

`entity.player_nbt`, `entity.player_nbt_coordinates` and
`entity.player_nbt_errors` cover the concrete read/write hooks, coordinate
ownership and complete selected child diagnostics. Their native, original-driver
and comparator controls run normally and under Python optimization without
requiring a supplied archive.

`MC_ENABLE_PLAYER_NBT_REFERENCE_TESTS` adds the Windows-only
`reference.player_nbt`. It starts fresh native and original observations and
compares complete captured receipts. Configure the supplied client JAR, the
pinned support-library directory and native-library directory through the
existing `MC_REFERENCE_CLIENT_*` options. Linux comparisons use explicit
exact-hash mappings for retained original input locations; native executable
paths and raw captures remain unchanged. See [LocalPlayer NBT](entity/player-nbt.md)
and its [reproduction guide](../tests/reference/entity/player_nbt/README.md).

## Singleplayer controller integration

`client.singleplayer_controller` exercises the canonical constructor and selected
World-entry methods on actual App, World and LocalPlayer owners. The allocation
variant checks retained failures, aliases and allocation-free clear/retirement.
Together they pass 230 and 256 checks. The parser/process controls run normally
and with Python optimization; twenty-one controls reject incomplete or changed
process evidence and malformed records.

`MC_ENABLE_SINGLEPLAYER_CONTROLLER_REFERENCE_TESTS` adds the Windows-only
`reference.singleplayer_controller`. It starts a fresh native observer, requires
its complete process receipt and unchanged executable, then compares its twenty
records with actual unchanged-original execution. The original cohort contains
twenty classes, 736 methods and 731 Code bodies. Its records cover fourteen
completed observations and six interrupted factory prefixes. Original archive,
class origins, runtime/helper inputs and generated helper bytes are checked
before and after execution. The original does not return failed allocated
Players; their retained native prefixes are checked separately.

The controller reloads the current Session for each Player factory call,
allocates LocalPlayer before argument reads and stores only yaw during its
preparation hook. The two empty hooks are actual reference bodies. Application
publication, controller updates, mining and item use remain unfinished. See the
[controller contract](client/singleplayer-controller.md).

## Player spawn/reset integration

`entity.player_spawn_reset` runs genuine LocalPlayer spawn/reset and inherited
base-body cases. `entity.player_spawn_allocation` exercises retained failure
prefixes with the real descendants. `entity.position` executes the 36 raw-input
position vectors and requires native completion; its default route does not
supply an original numerical oracle. The reset and position parser/process
controls run normally and with Python optimization.

The isolated coherent MSVC and sanitized Clang/GCC qualifications pass 270 reset
and 273 allocation checks, 42 reset records and 36 position vectors. The old
position body differs from the unchanged original in eight two-NaN cases on
MSVC and Clang; the shared arithmetic correction matches all 36 on each compiler.
Linux native comparisons use the retained Windows-original observations and
do not claim execution of the original application on Linux.

On Windows, `MC_ENABLE_PLAYER_SPAWN_REFERENCE_TESTS` and
`MC_ENABLE_POSITION_REFERENCE_TESTS` add `reference.player_spawn_reset` and
`reference.entity_position`. Each starts a fresh native process, validates its
exit, bounded streams, executable fingerprints and positive marker, then passes
that exact stdout and process receipt to the unchanged-original comparison.
The reset comparison covers 34 completed calls and eight interrupted prefixes.
Both modes verify original inputs and helper products before/after execution.
See [spawn/reset](entity/player-spawn-reset.md) and the
[reference guide](../tests/reference/entity/spawn_reset/README.md).

## World Entity update integration

`world.entity_tick` exercises the actual client Entity/TileEntity pass on real
World, Store, Registry and Chunk owners. The isolated MSVC Debug and sanitized
Clang/GCC qualification passes 881 native checks per compiler, including 32
World-to-Player updates, Item updates, live removal callbacks, iterator failures,
Piston/Spawner state and retained riding actors. `world.entity_tick_abi` checks
the public layout contract. The normal and optimized controller controls require
completed native execution and retain capture failures.

`MC_ENABLE_WORLD_ENTITY_TICK_REFERENCE_TESTS` enables
`reference.world_entity_tick`. The untouched original CPU harness compares 54
ordered records: 37 states and 17 events. This covers the declared CLIENT GAME
owners and source prefixes. Server-owner admission, mob construction, the later
World time/block scheduler and application ticking have separate limits.
These isolated results do not establish a newer full combined matrix. See
[World Entity updates](world/entity-update.md) and the
[reference driver](../tests/reference/world/entity_tick/run.py).

## Weather method integration

`world.weather` exercises the client weather descendant with real World,
WorldInfo, provider and Random owners. Its isolated MSVC Debug and sanitized
Clang/GCC runs pass 473 native checks and the unchanged 8,565-check WorldInfo
regression. Eight controller controls run normally and under Python optimization;
the existing statistics capture controls cover the shared subprocess helper.

`MC_ENABLE_WEATHER_REFERENCE_TESTS` adds `reference.world_weather`. Its 68 exact
original records contain four completed Overworld initialization states and
64 weather observations: 58 method returns and six interrupted prefixes. Three
returning no-sky cases use the genuine failed native Nether constructor prefix;
they establish the weather early return while leaving construction failed at
PC403. They do not establish completed Nether generation. The enclosing World
tick and dedicated-server weather entry remain unfinished. See
[weather](world/weather.md) for the full source and finite qualification scope.

## First-person camera integration

The subsequent camera integration passes 34 selected Windows Debug CTests and
26 cases on each fresh sanitized Clang/GCC Debug build with leak detection.
Its tested cohort has 1,713 files and 863 formatted C/header files. All matching
targets rebuild; the selected Windows cases include the actual client
executable, startup, three World inspection captures, GPU drawing and the
unchanged-original GL comparison. Source inventories and configured products
stay unchanged across each selected suite. These 86 selected cases are separate
from the earlier full matrix below.

The native camera has 438 checks; sixteen genuine saved-Item callback cases have
1,198 checks. Late App/Settings/World reads, reached FP failures and hold limits
retain their source prefixes. The original Windows profile compares 42 baseline
matrices and ten callback prefix matrices under its declared `2e-6` coefficient
policy. It does not qualify full EntityRenderer construction, source fog/clear,
portal clock, third-person, normal App h/i publication or a playable frontend.
See [the camera contract](client/first-person.md) and its
[reproduction guide](../tests/reference/client/camera/README.md).

## Earlier water, Item and Player checkpoint

At published checkpoint `100adbe`, the combined water animation, Item pickup/tick,
active Player and pickup-particle source passes 952 local CTests: 332 Windows
MSVC Debug cases and 310 cases on each
fresh Clang 18/GCC 13 Debug build with ASan, UBSan and leak detection. Every case
from the previous 307/286/286 matrix remains enabled. No case failed, skipped,
was disabled or remained unrun. Windows includes actual OpenGL startup and World
inspection; the local Linux profiles are headless. Hosted public/JDK results are
tracked separately from these local checks.

The frozen build cohort contains 1,700 files and 1,476 non-Markdown inputs. All
859 project C/header files pass clang-format 18.1.3. Before execution, 157 current
native/helper fingerprints and eleven original class/Code metadata families
verify. Source inventories and bytes match before/after each suite. Actual Debug
target artifacts, copied runtime libraries, PDBs and compiler/link configuration
are also unchanged across each suite. All configured Windows executables retain
their default 1 MiB stack. Retained native captures and the new water, particle
and Item original records verify independently after execution.

The earlier particle integration passed 37 selected Windows tests before Item
tick import. Its 854-file format check and the isolated module qualifications
below remain dated evidence; they are not additional full-suite cases.

The first combined build stopped at an old Minecart enum-count assertion. The
existing selectors 22..26 retain their values; client xw/em append 27/28 and COUNT
is 29. The corrected assertion and class tests pass their two focused cases,
followed by the complete 37-case selected pass. This compiler assertion failure
is retained separately from runtime test results.

The preintegration module qualifications use their own coherent source and
products. Their counts do not add up to a new full-suite total:

| Operation | Scoped evidence |
| --- | --- |
| Pickup particle constructors/tick and CPU effect manager | 4,571 native checks; the paired original run passes 4,592 checks and 71 exact records |
| Effect-manager ownership and Lists | 12,095 native checks |
| Complete-App pickup notification and Item lifetime | 82 native checks |
| Active Player bodies and allocation lifetime | 678 active checks and 247 allocator checks |
| Input arithmetic/admission | 1,222 native checks |
| Active animation atan 2 | 8,676 separate JDK results |
| Water CPU animation | 103,098 native checks, with the separate exact original array/RGBA comparison |
| Item tick | 1,089 native checks and 70 exact original records across 37 states: 33 complete calls and four prefixes |

The CPU particle and water comparisons run on the qualified Windows and Linux
products. The original active-App profile retains its Windows LWJGL scope.
Particle drawing, normal RenderEngine startup, fog, the World Entity loop and
a playable client remain unfinished. [Item ticking](entity/item-tick.md) now
executes its concrete body; its three isolated profiles predate the current
combined matrix.

`entity.pickup_particles`, `client.effect_renderer` and
`client.player_pickup_particles` use authored native owners. The optional
`MC_ENABLE_PARTICLE_REFERENCE_TESTS` enables `reference.pickup_particles` with
the verified client JAR, pinned support archives and matching JDK17 tools.
Its controller checks the full 71-record order, class/Code pins, actual runtime
origins, helper inventory and before/after inputs/products. Native and original
captures require exit 0, empty stderr, bounded output and positive final markers.
See [particle ownership](client/particles.md) and its
[finite CPU qualification](client/particles-qualification.md).

`client.water_animation` and its normal/optimized driver controls need no JDK
or original assets. `MC_ENABLE_WATER_ANIMATION_REFERENCE_TESTS` enables
`reference.water_animation`; the [reproduction guide](../tests/reference/client/water_animation/README.md)
describes the 23 declared array/state cases, holder continuation and source
failure prefixes. The runtime uses its shared Math owner; the finite comparison
explicitly seeds that existing owner and supplies no draw outputs.

`entity.item_tick` and its two controller tests use authored native owners.
`MC_ENABLE_ITEM_TICK_REFERENCE_TESTS` enables `reference.item_tick` with the
verified client JAR, pinned libraries and matching JDK17 tools. Its original
profile compares 37 states and 33 events on constructed Item/World/Stack owners;
33 calls return and four retain their reached source prefixes.

[Active Player ticking](entity/player-active-tick.md) composes the real Player,
Living, Inventory, Input, travel, nearby and statistic bodies.
[Item pickup](entity/item-pickup.md) completes notification before the later
Stack/death check. These method-level results leave World scheduling, Map
tracking/terrain updates, later Living hurt, GUI closure and drawing as separate
work.

## Player runtime and application statistics

At the earlier `9da869d` checkpoint, the combined source passed all 307 Windows
MSVC Debug cases and all 286
headless cases each on fresh Clang 18/GCC13 Debug builds with ASan, UBSan and
leak detection: 879 completed cases without failures, errors, disabled cases or
skips. Its separately retained hosted public/JDK matrix passed 855 cases. These
counts date that earlier checkpoint; they do not describe the current combined
source. Every case from the preceding movement checkpoint remains enabled.
Windows includes real OpenGL startup and terrain captures. The frozen cohort
contains1,617 files and 1,400 non-Markdown inputs; all 822 C/header files pass
clang-format18. Current native/helper provenance and seven original class/Code
cohorts verify before execution; source and retained capture/product readback
are checked after execution.

New public tests cover real statistics files/workers, genuine application/Session/
writer ownership, actual statistic callsites, travel, base environment ticking,
inventory updates and Player/Living action bodies. Each product passes 601
application checks, 79 separately instrumented clock-fault checks, 115 connected
callsite checks, 195 travel checks, 1,109 base-tick checks, 33,158 inventory checks
and 523 action checks. These tests use authored resources and real native owners;
they require neither the original archives nor a JVM.

Optional reference switches are `MC_ENABLE_CLIENT_STATS_REFERENCE_TESTS`,
`MC_ENABLE_PLAYER_TRAVEL_REFERENCE_TESTS`,
`MC_ENABLE_PLAYER_BASE_TICK_REFERENCE_TESTS`,
`MC_ENABLE_INVENTORY_TICK_REFERENCE_TESTS`,
`MC_ENABLE_PLAYER_ACTION_REFERENCE_TESTS` and
`MC_ENABLE_PLAYER_STATISTICS_REFERENCE_TESTS`. Supply the original client JAR,
pinned client libraries and matching explicit JDK17 executables. The genuine
Minecraft/xi travel and direct-statistic profiles additionally need the pinned
Windows x64 native library and currently qualify Windows only. Other new original
profiles run on the qualified Windows and Linux products. JDK math component
records, completed original calls and source failure prefixes stay separate.

The initial combined Debug travel test exposed a stack overflow in nested
passive diagnostic initialization. Pointer initializers preserve typed defaults
and layouts without increasing the default 1 MiB stack or hiding the failure
with optimization. Its original failing products and default-stack fix remain
retained. The first full runtime cohort then exposed three failures on both
Windows and Clang: two older predicates still expected the newly supported
Minecart candidate-box method to fail, and the environment comparison repeated
that native failure. Actual client yl.f/server pr.e_ return null; the null-moving
pair call is later at260. The corrected exact assertions retain cases, rows,
cleanup and the separate moving-Minecart guard. The failing full runs, tight red
reproductions and subsequent four-case green run remain distinct from the final
complete qualification.

[Player statistics](client/player-statistics.md), [travel](entity/living-travel.md),
[base tick](entity/base-environment-tick.md), [inventory](entity/inventory-tick.md)
and [action](entity/player-action.md) describe the finite source boundaries.
The later active Player and Item pickup bodies have their own current entries
above. World Entity scheduling, Map tracking/terrain updates, Living hurt,
Container close and complete gameplay remain unfinished.

## LocalPlayer movement and walking effects

`entity.player_movement` constructs the real Player, Input, Session, Registry,
World and loaded Chunk owners using authored resources. It covers movement,
walking effects, block step-on dispatch, reclaimable sound identities and
explicit unsupported descendants. The existing Item/Falling regression stays
separate. The two `tools.player_movement_controller` variants check twelve
capture faults normally and under optimized Python. These public tests need
neither Java nor original assets.

The complete combined source passes 280 Windows MSVC tests and 261 headless
tests each on fresh Clang 18/GCC13 builds with ASan, UBSan and leak detection:
802 completed cases, no failures, errors, disabled cases or skips. Each product
passes 40,373 movement/step/sound and 15,297 environment checks. Existing
Item/Falling, Player/input/publication, crafting, material, lighting, persistence
and notice checks pass in the same cohort. Windows includes real OpenGL/client
tests; these local Linux builds are headless.

All 773 C/header files pass clang-format18. The 1,511-file frozen snapshot
contains1,303 non-Markdown inputs. Readback checks source, executable and capture
hashes and all actual rows, including 21 original movement records and twelve
separately labelled JDK rounding records per product. Both new twelve-case
controller suites pass normally and with Python optimization. Controller mirrors
and all 556 positive/2,011 negative lifecycle interval controls also pass.

Enable `MC_ENABLE_PLAYER_MOVEMENT_REFERENCE_TESTS` with the supplied client JAR,
`MC_REFERENCE_CLIENT_LIBRARY_DIR` and matching JDK17 runtime/compiler for
`reference.player_movement`. The controller compares 21 unchanged-client records
on actual constructed owners and twelve separate JDK rounding records. It checks
the exact executable, original class metadata and source/helper hashes before and
after capture. `MC_REFERENCE_TESTED_COMMIT` identifies the tested source; an
uncommitted snapshot must retain its separate complete source manifest.
These finite operations do not execute a Player tick or establish gameplay.

## Entity environment helpers

`entity.environment` runs the real dual-side World, Registry, Chunk and Entity
owners without Java or original assets. The two
`tools.entity_environment_controller` variants run the same twelve process and
capture-fault controls normally and with optimized Python. The integrated
Windows preflight passes all four selected cases, including the optional
unchanged-original comparison. The complete combined qualification above then
passes on MSVC and both sanitized Linux compilers.

Enable `MC_ENABLE_ENTITY_ENVIRONMENT_REFERENCE_TESTS` and supply
`MC_REFERENCE_CLIENT_JAR`, `MC_REFERENCE_SERVER_JAR`,
`MC_REFERENCE_CLIENT_LIBRARY_DIR` and matching JDK17 runtime/compiler paths for
`reference.entity_environment`. This portable comparison checks 1,353 original
client records and 1,027 original server records. The server observations cover
relative acceleration and null-World exception prefixes. Its 51 successful
native server World rows use client observations and paired bytecode evidence;
they are not successful original-server World observations. See the
[runtime contract](entity/environment.md) for exact scope and remaining ticks.

## Achievement notice and model construction

The final combined source passes all 272 Windows MSVC tests and all 253 tests
each on fresh headless Clang 18/GCC13 builds with ASan, UBSan and leak checks.
All 778 cases run without failures, errors, disabled tests or skips. The frozen
1,484-file cohort contains1,279 non-Markdown inputs; all 765 C/header files pass
clang-format18. Windows includes real GL captures and the unchanged-original
notice comparison; local Linux notice coverage uses authored CPU resources.
The original comparison matches140 geometry and 151 callback records plus
source diagnostics in order, including 1000 actual reminders. Both capture
controller modes pass nine controls on all three native products.

The initial Clang four-way suite retained a 120.02-second timeout in the unchanged
Java-free Python `tools.region_write_reference` controller. No child remained.
The same test passed under its existing deadline in isolation in 28.67 seconds.
Final Clang/GCC suites use CI's bounded parallelism two and pass in full; no
assertion, timeout or production algorithm was relaxed. The initial failure
does not count as a completed pass.

`client.achievement_notice` creates authored localization/map files in a fresh
directory and executes real Registry, Settings, Entity/Vec3, model, notice and
complete application owners. It requires neither original assets nor a JDK,
SDL, window or audio device. The two `tools.achievement_notice_capture` tests
run nine subprocess controls normally and under optimized Python. Native cases
cover aliases, mirrored geometry, borrowed lifetimes, 16 allocation prefixes,
default-Random failure order, null/failed prefixes and 1000 reminders without
permanent Registry growth.

The optional `reference.achievement_notice` currently qualifies Windows x86-64.
Enable `MC_ENABLE_NOTICE_REFERENCE_TESTS` and supply `MC_REFERENCE_CLIENT_JAR`,
`MC_REFERENCE_CLIENT_LIBRARY_DIR`, `MC_REFERENCE_CLIENT_NATIVE_DIR` and
`MC_REFERENCE_CLIENT_RESOURCE_DIR`, plus matching explicit JDK17 tools. The
resource root contains the imported JAR's `lang` and `achievement` directories.
The pinned LWJGL DLL profile is Windows-specific; it does not qualify original
execution on Linux. Missing private prerequisites return77 and count as skipped,
never a parity pass. See the [reference commands](../tests/reference/client/notice/README.md)
and [constructor/state scope](client/achievement-notice.md).

## Continuous integration scheduling

The hosted workflow runs four CTest cases concurrently and allows four parallel
Windows build jobs. The existing compiler, sanitizer, graphics, formatting and
controller checks stay enabled. New revisions cancel superseded review checks;
default-branch runs are allowed to finish. Linux build jobs cache downloaded APT
package archives and still run package installation and every existing check.
Measured cold/warm cache results are pending; no new hosted speed claim follows
from the cache configuration. The grouping follows the documented
[workflow concurrency rules](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency).

The [serial material run](https://github.com/frankischilling/minecraft-c/actions/runs/40244894912)
and [parallel scheduling run](https://github.com/frankischilling/minecraft-c/actions/runs/40245956977)
both pass all four jobs and the same 771 complete Start/Passed pairs. Every
checkout tree matches its corresponding code head. The observed test-step
durations were:

| Compiler | Serial | Parallel |
| --- | ---: | ---: |
| Windows MSVC | 690 seconds | 402 seconds |
| Linux GCC | 550 seconds | 218 seconds |
| Linux Clang | 667 seconds | 375 seconds |

The Windows build also falls from 372 to 314 seconds. These consecutive hosted
runs include ordinary runner and network variation; they establish these measured
results, not a fixed performance guarantee.

The client Display header now forwards only the SDL Window type. Source files
that call SDL video functions include the SDL header directly. This lets the
actual application state compile in CPU-only notice tests without SDL headers.
A complete MSVC rebuild and all six affected client tests pass with CTest
parallelism two; the full application-access source also passes strict Clang
syntax checking without an SDL include path. These focused checks are separate
from the earlier complete material cohort below.

## Material immersion and acceleration

The complete combined cohort passes all 268 Windows MSVC tests and all 250
headless tests each on fresh Clang 18/GCC13 builds with ASan, UBSan and leak
checks. All 768 cases run without failures, errors, disabled tests or skips.
The 1,464 frozen inputs contain 1,261 non-Markdown files; all 754 C/header files
pass clang-format18. Each product passes 1,944 material/Vec3 checks and all
300 fresh original-client records plus172 shared native server crosschecks.
Both controller suites also pass with Python optimization enabled on Windows
and Linux. Earlier World/Player/input/crafting/lighting comparisons pass again
in the same source cohort. Windows includes real GL captures; these local
Linux profiles are headless.

`world.material_movement` runs the actual dual-side World/Registry/Chunk/Entity
contracts without a JVM or imported assets. It covers identity and liquid scans,
fluid acceleration, eye immersion, the shared Vec3 pool, ownership, allocation
failures and checked work limits. `tools.material_movement_controller` runs twelve
subprocess controls, including real log-write, short-write and close failures.
The separate maintenance controller now has the same twelve controls. Capture
success requires an explicit empty `reader_errors` dictionary as well as complete
pipes, bounded output, zero exit, empty stderr and a positive final marker.

Enable `MC_ENABLE_MATERIAL_MOVEMENT_REFERENCE_TESTS` for the optional unchanged
client comparison. Supply `MC_REFERENCE_CLIENT_JAR`,
`MC_REFERENCE_CLIENT_LIBRARY_DIR` and matching JDK17 Runtime/Development tools.
The controller creates fresh native and original runs, verifies their completed
reports and compares all 300 client records plus172 shared native server rows.
The server evidence is paired bytecode and a native crosscheck; the comparison
does not run an original server process. See the [contract](world/material-movement.md)
for prepared inputs and remaining movement scope.

## World lighting, Player publication and input

The complete integrated suite passes all 265 Windows tests and all 247 headless
tests each on fresh sanitized Clang/GCC builds. Every case runs; there are no
failures, errors, disabled tests or skips. The same 1,446 source inputs cover all
three products, with all 745 C/header files formatted by clang-format18.
Linux coverage here is headless; the Windows suite includes actual GL captures.

The integrated contracts use real World/Store/Registry/Player/input owners.
`world.light_propagation` covers saved-light writes, live callback mutation,
failed popped records, 499-record limits and 50 actual nested drains. The
inspection test acquires its full footprint and drains before final meshes.
Its separate 60-second process deadline covers the measured strict Debug work;
pixel relations and exit/stderr checks remain exact.

`world.player_publication` covers duplicate Player occurrences, sleeping scans,
Chunk/World membership, callback reentry/failure, saved-tag prefixes and terminal
ownership. `entity.player_input` covers actual field attachment, shared/replaced
holds, source key/reset/sneaking forwarding and RAW/failed receiver prefixes.
`world.provider_maintenance` covers natural empty/cached state and declared
pending-Set inputs, including real Item unload/save and session failure.
The public controller/reader controls need neither original files nor a JDK.

Optional original comparisons use the supplied JARs and matching explicit JDK17
tools. Configure `MC_ENABLE_PLAYER_PUBLICATION_REFERENCE_TESTS`,
`MC_ENABLE_PLAYER_INPUT_REFERENCE_TESTS`, `MC_ENABLE_MAINTENANCE_REFERENCE_TESTS`
and `MC_ENABLE_PROPAGATION_REFERENCE_TESTS` to run their finite comparisons.
Maintenance also requires the pinned client library directory. The propagation
default compares newly produced 25-Chunk arrays, all ordered light regions,
scheduler ownership/order and boundary Random states. It supplies an explicit
post-constructor World.r seed on both sides; production default entropy stays
unchanged. Full 289-point fresh/warm profiles require the separate expensive
`MC_ENABLE_STARTUP_GRID_REFERENCE_TESTS` option. They are not default test coverage.
See [propagation reproduction](../tests/reference/world/propagation/README.md)
and [current results](status.md) for exact cohort qualifications.

## Local Player construction

The earlier [constructor contract](client/player-construction.md) passes a complete
Windows MSVC ABI rebuild and all 254 tests. Complete headless Clang 18/GCC13
rebuilds each pass all 236 tests with ASan, UBSan and leak checks. Every recorded
case runs without failures, disabled tests or skips. All 730 C/header files pass
clang-format18. The Linux profiles build with `MC_BUILD_CLIENT=OFF`; they do not
qualify a new Linux desktop or audio-device run.

Six focused tests per compiler include the new constructor, its 32 authored
probe controls, the unchanged-original comparison and existing Entity/Class/
Minecart regressions. Native constructor tests pass 736 checks. Fresh JDK17
normal and read-only JDI runs verify 187 class origins and 1,862 Code bodies,
six completed constructors, two actual failed receivers and 71 checkpoints.
The [reference README](../tests/reference/entity/player/README.md) separates
deterministic fields, aliases, bounds and Slot comparisons from scalar replay
of observed Math returns. Production constructors use their independent real
application Math profile.

Enable `MC_ENABLE_PLAYER_REFERENCE_TESTS` with `MC_REFERENCE_CLIENT_JAR` and
explicit JDK17 tools for the local original comparison. The public constructor
and probe tests use authored resources and require no original archive or JVM.
Full Player/World gameplay remains unfinished.

## Crafting inventories and containers

The [crafting contracts](inventory/crafting.md) pass three integrated tests on
Windows MSVC19.51 and sanitized Linux Clang 18/GCC13. Native tests cover all
151 recipes and pass 254 outer inventory/container assertions per side on
Windows and 253 on Linux; native allocation-budget branches depend on layout.
Each product passes 48 strict protocol controls and fresh JDK17 comparisons
against both supplied original archives: 1,047 authored cases and 1,153 exact
rows per side. Class/Code pins, actual class origins and initialization
boundaries are checked. The final published input/pin files use LF endings.

The complete frozen Windows suite passes all 251 cases without failures,
disabled tests or skips. All 723 C/header files pass clang-format18.

Use `MC_ENABLE_CRAFTING_REFERENCE_TESTS=ON` with both `MC_REFERENCE_CLIENT_JAR`
and `MC_REFERENCE_SERVER_JAR` for the opt-in original comparison. The public
`inventory.crafting` and `tools.crafting_probe_inputs` tests use authored
resources and need no original archive or JVM. Reproduction and finite limits
are in the [reference README](../tests/reference/inventory/crafting/README.md).
Output pickup and gameplay remain unfinished. The actual Player constructor
has the separate coverage above.

## Native client frontend

Checkpoint `5143b58fbba506bec1d9753080e5fcb3a70906a3` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40218311052):
240 Windows, 246 GCC and 246 Clang tests. All 732 Start/Passed pairs are present.
Every checkout uses merge `1d012bd644dd57b0b767de78b747aebcd9307016`, whose tree
equals that code head. The later crafting changes have separate validation.

Checkpoint `3d26621d5e66741c4a4383336fd12e729fe7439b` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40213051132):
237 Windows, 243 GCC and 243 Clang tests. All 723 Start/Passed pairs are present.
Every checkout uses merge `17ad0d82d141591bc123d4450a2ed52e49fa98a6`, whose tree
equals that code head. These hosted profiles omit the locally supplied original
archives and Windows desktop-only cases; they do not replace those local checks.

The next input/turn increment passes six selected integrated tests per compiler
on MSVC and sanitized Ubuntu Clang/GCC. They include 1,214 native MovementInput
checks, 137 Entity turn checks, 857 exact original input records and 3,330 exact
turn records. The backend's injected ledger-allocation regression fails against
the earlier branch and passes after preserving `ALLOCATION_FAILED`. It uses a
leaf allocation override with a real decoded cache and OpenAL context; the
normal backend has no runtime fault-injection option.
The complete frozen suites for this increment pass 248 Windows MSVC tests and
250 tests each on Clang 18/GCC13 with ASan/UBSan/leak checks. All tests run,
without failures or skips. Formatting covers all 707 C/header files.

The frozen World/audio increment passes all 243 Windows MSVC tests and all
245 tests each on Ubuntu Clang 18 and GCC13 with ASan/UBSan and leak checks.
No test fails or skips. These runs include public/JDK tests and both optional
unchanged-original WAV/CPU rendering comparisons. All 697 C/header files pass
clang-format18. Controller mirrors and 556 positive/2,011 negative lifecycle
controls pass. A final first-color-map-error preservation fix additionally
passes `client.world_session`, `client.world_renderer` and
`client.world_inspection` on all three compilers.

GPU tests use Mesa through Xvfb on Linux; audio uses real OpenAL loopback and
workers. `client.world_inspection` runs the actual executable with authored
resources, three fresh Worlds and two native pixel relations. Native Windows
and sanitized Linux/Mesa imported-resource seed23 runs also capture generated
scenes. Their finite scope and unresolved cross-driver pixels are recorded in
[World rendering](client/world-renderer.md). These results do not establish
playability or original whole-frame/device correspondence.

The SoundManager checkpoint `2c311612d78028e8ab17e454925e1d0106d4f124`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40203404635).
That earlier hosted result predates the World renderer, WAV and OpenAL backend.

The nonzero sound checkpoint `aa39f8f663d250052d6d2b9cae9c691cfa7136f3`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40190426923):
229 Windows, 233 GCC and 233 Clang tests, with 695 complete Start/Passed pairs
and no failures, skips or missing cases. Formatting covers all 658 C/header
files; controller mirrors and 556 positive/2,011 negative lifecycle controls
pass. Every checkout uses synthetic merge `839f22e2f21181a27d9b27f2381976bb0c412b51`,
whose complete tree equals that head. Independent readback reconstructs 331
trees/1,566 entries, consumes every one of the 1,236 literal source bodies and
joins all 1,056 Windows-tested non-Markdown inputs. The full console supplies
the test chronology; no hosted JUnit or artifact archive was retained.

The earlier sound checkpoint `e10a5e2708e81c81ea2cc9b721c060a2d2f056d7`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40171237434):
228 Windows, 232 GCC and 232 Clang tests, or 692 complete Start/Passed pairs,
with no failures, skips or missing cases. Formatting covers 657 C/header files;
controller mirrors and all 556 positive/2,011 negative lifecycle controls pass.
All checkouts use synthetic merge `ec63077a8db7674a197219458a9343fbedcd35cc`,
whose complete tree equals that head. Independent readback reconstructs 331
trees/1,564 entries and joins all 1,234 literal published files and 1,054
Windows-tested non-Markdown inputs. The console supplies the full chronology;
no hosted JUnit or artifact archive was retained.

The earlier option-action checkpoint `2afd39a70935a3ae167f7cb7fad6452392828c6d`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40154472178):
225 MSVC, 229 GCC and 229 Clang cases, 683 complete Start/Passed pairs, with zero
failures, skips or missing runs. Formatting covers 645 C/header files; mirrors
and lifecycle guards pass. Every host runs `client.game_settings_actions`.
All four actual checkouts use synthetic merge `c37ce7331a8bf643587081c7ae07e5e2a117797b`,
whose complete tree equals that head. Independent readback reconstructs 328
trees/1,542 entries and joins all 1,215 literal current committed bodies and
1,037 non-Markdown tested inputs. Nine later result-documentation changes retain
separate pins. This hosted result does not execute the new sound foundation.

The preceding code checkpoint `ea1aefc952739fd6e7fba023646039856324e721`
passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841):
224 MSVC, 228 GCC and 228 Clang cases, 680 complete Start/Passed pairs, with
zero failures, skips or missing runs. Formatting covers 644 C/header files;
controller mirrors and all 556 positive/2,011 negative lifecycle checks pass.
The audit rejoins all 1,213 literal Git source bodies and 1,035 non-Markdown
tested inputs. All four actual checkouts use synthetic merge
`2be1dc660f3bddbba4c1788c612cad18e963a3db`, with complete tree
`f7c2f270c059b83f10c31500160dd2e9c07861dd` equal to the head. The full tree has
1,540 entries and 328 trees. No hosted JUnit or artifact archive was retained;
raw console Start/Passed records supply the complete test chronology.
This includes GameDirectory on all hosts and startup graphics tests on Linux.
Linux CI requests software OpenGL under Xvfb; no actual vendor was measured.
The new float/discrete option actions below are outside this checkpoint.

The [sound resource foundation](client/sound.md#verified-native-scope) passes one
Windows MSVC19.51.36260.0 x64 Debug headless profile: 18,654 SoundPool, 581 file,
579 Vorbis, 43 integer-parser and 191 Random-profile checks. All five CTests,
seven supervised stages and eleven final audits pass. Independent file-only
readback consumes14,410 files and rejoins all 1,404 physical source bodies,
including 1,234 public members and 1,054 non-Markdown inputs. Formatting covers
657 C/header files. Retained decoder inputs are authored zero PCM, including a
mono-to-stereo chain and a real missing-page rejection. Four earlier local failed
attempts remain preserved, with their fixes and scope in the sound note. These
tests initialize no audio device; original PCM correspondence remains
unqualified. The 683-case hosted result above predates these modules.

Nonzero vectors exercise the actual binary32-times-32767/Java-conversion
helper and planar suffix interleaving. Encoded fixtures remain silence. The
[sanitized sound CI](https://github.com/frankischilling/minecraft-c/actions/runs/40169607338)
passes all 228 Windows and 232 GCC cases, plus 657-file formatting. Clang passes
231 of 232 tests; only decoder checking fails. Its native child exits 0 but
emits a UBSan null-pointer-offset report in libogg. The driver's strict stderr
assertion rejects that outcome. An exact-commit Ubuntu Clang 18 build reproduces
the same path; the private Ogg copy now guards the null page pointer.

Earlier bounded Ubuntu GCC ASan/UBSan failures locate libvorbis's negative Bark
shift, last-band interpolation read and signed decode search-word shift. The
corrected public driver passes 563 GCC checks, seven inputs and four exact
zero-PCM outputs. Independent file readback verifies its1,234-member copied
source, captures and closed PIDFD ledger. These focused diagnostics do not
qualify the full platform. The later 692-case hosted result above includes the
Ogg correction on all three compilers. A fresh corrected Ubuntu Clang ASan/UBSan
build passes all 563 decoder checks and independent file readback with empty
child stderr. Empty stderr,
successful status, exact footers, bounds and cleanup remain required.

The separate `client.vorbis_nonzero` regression encodes mono, distinct-channel
stereo, a format-changing chain and a longer stream from authored nonzero
rational PCM. It first reproduces a negative floor-deviation shift in
libvorbis. An exact private-copy multiplication correction passes a fresh
Clang 18 ASan/UBSan run: 5,469 nonzero checks plus the existing 563 decoder
checks. Independent readback verifies all 1,236 source members, four encoded
inputs, four nonzero PCM outputs, exact chain joins and complete dependency
copies. A fresh Windows MSVC19.51.36260.0 x64 Debug headless run passes all six
selected tests: the existing five counts remain unchanged, and the nonzero
case passes 5,480 checks. All seven supervised stages and eleven final audits
pass. Independent readback consumes14,431 files and joins all 1,406 physical
source files, including 1,236 public members and 1,056 non-Markdown inputs.
All 658 C/header files pass clang-format18.1.3. The terminal report SHA-256 is
`1a2a9ae0aa7e1fc4e20b9e8c764d1776b28faf10dabb4375a46b37ec334665cf`.
Later result-documentation changes remain separate from this tested snapshot.
Original PCM is not compared.

The separate [arranged original codec control](reference/client-sound.md#arranged-original-codec-observation)
completes its eight-operation plan and eleven final audits. It records a
null/EOS result for the authored mono stream and a later SoundBuffer with null
data. The generic native decoder emits 6,400 bytes from the same encoded input.
That difference remains explicit; observation completion is not a PCM match.

The [nonzero original mono comparison](reference/client-sound.md#nonzero-original-pcm-observation)
reuses the unchanged compiled helper and supplies only the actual authored
encoded input. Both decoders return 8,192 bytes. Complete comparison finds
four one-step differences among 4,096 signed16 samples, with no unmatched
suffix and no tolerance. Independent evidence readback passes all 2,368 final
file/tree checks; the comparison exits1 for failed exact equality. It does not
capture conversion-entry floats or locate the numerical cause.

Earlier code checkpoint `adb573dbc6998dcd5fabf72f81e2342e0feb147c` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40129932174):
221 MSVC, 224 GCC and 224 Clang tests. All 669 cases have complete Start/Passed
records, with zero failures, skips or missing runs. Formatting covers all 635
C/header files; controller mirrors and lifecycle interval checks pass. The
actual synthetic checkout has the same complete tree as that head, and all
1,199 literal source bodies match the published snapshot. The separate
original/native persistence comparisons below retain their own finite scope.

The actual checkout
`e6f5a84b8c322fdea4b7842f0da814f71091e3c1` has complete tree
`5250b9b78d8b76f6c1c11b56fb073122ef3814c0`, equal to the head tree. These hosted
tests do not run the supplied-original settings comparison.

Code checkpoint `389b57361d9454b447c0f956cc39c5a1d32245b3` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40117293002):
218 MSVC, 221 GCC and 221 Clang tests, with 660 actual Passed records and no
missing cases or skips. Formatting covers 630 C/header files. Controller
mirrors and lifecycle interval checks pass. The synthetic checkout's complete
tree equals the head tree. The preceding Windows build rejected the Font test's
constant condition as C4127 under MSVC19.44. A C17 compile-time assertion fixes
that condition without relaxing warnings. The earlier image allocation-layout
fix also remains covered.

The [startup contract](client/startup.md) records six frontend tests: image,
ScaledResolution, FontRenderer, assets, Timer and actual OpenGL display checks.
All six pass on MSVC and sanitized GCC/Clang. The same profiles complete
bounded 200-frame runs; Windows additionally passes window-close, Quit and
positive-size resize/Quit controls. Fullscreen and zero-drawable controls
remain unverified. The display test is opt-in with
`MC_ENABLE_CLIENT_DISPLAY_TESTS=ON` and fails if it cannot create a context.
Linux CI runs it under Xvfb with software Mesa.

The later shared [GuiMainMenu/probe checkpoint](client/gui-main-menu.md) passes
eight focused cases per compiler, including 100 display checks and 53 scene
admission controls. Ordered matrix replay makes the Windows conditional image
match every original RGBA pixel; Linux still differs at one pixel against that
Windows/NVIDIA baseline. The failed Linux comparisons remain failed. The
ordered FontRenderer interface passes 262,377
additional checks while the original 220 glyph checks remain. Complete startup
still needs qualification.

The preceding full source snapshot passed 211 MSVC and 213 Clang cases.
GCC passed 212 of 213 initially. Its named-data replay rejected an observed
1,253 ms backward wall-clock step; one unchanged-code retry with fresh OS
samples passed. The failed capture remains failed, and the strict comparison
was preserved. A later minimized-window adjustment passed the focused
frontend tests and captures on all three compilers. These results do not
qualify subsequent source changes or matched original rendering.

The [original title note](reference/client-title.md) records the independently
observed method and clock, including retained controller failures. The
[float parser](core/java-float.md) has separate primitive checks; it does not
complete options-file persistence.

The [legacy formatter](core/java-float-format.md) passes 55,240 external records
per compiler through the public shared core, retaining the separate 5,708
observed-JDK and 49,532 model-derived groups. The
[integer parser](core/java-integer.md) passes 43 unit checks and the complete
65,536-unit alphabet comparison per compiler. Its original controller reports
retain their footer-parser failure; separate file-only audits verify the actual
tests and rows and complete read-only binary checks without native reruns.

<a id="startup-settings-and-game-directory"></a>

## Startup settings and GameDirectory

The App now loads one real GameSettings owner after Display and uses its GUI
scale and initial anaglyph for the title. Explicit `--gui-scale0` overrides a
loaded value; absence of that flag preserves the loaded value. Fresh Windows
MSVC x64 Debug headless and client profiles pass these six selected CTests:

| Profile | Test | Actual result |
| --- | --- | --- |
| Headless | `client.game_directory` | 57 native checks and six independent file checks |
| Headless | `client.game_settings` | 548 native checks and six independent file checks |
| Headless | `client.game_option` | 883 metadata/query checks |
| Client | `client.display` | 100 actual OpenGL checks |
| Client | `tools.client_main_menu_probe` | 19 admitted and 34 rejected scenes |
| Client | `client.startup` | Nine App cases and eight exact decoded-pixel relations |

Both profiles complete all seven stages, identical before/after test inventories
and eleven independent final audits. The frozen family has1,213 members,
including 1,035 non-Markdown files and 644 C/header files. Source, SDK/tool,
dependency, product, stream and capture-tree pins remain joined. Configure/build
retain the explicit byte-pinned compiler-helper cleanup allowance; test trees
close naturally with no residual owned processes. A later file-only readback
rehashes 5,603 retained members, including raw/JUnit records, complete capture
trees and products. It repeats neither a native run nor pixel decoding.

An earlier B build remains failed: strict MSVC reported C4310 for a narrowing
cast in the malformed UTF-8 test fixture. Expressing the same authored bytes
as a C string literal fixed that compile failure without changing production
behavior or relaxing diagnostics. The 644-file formatter check passed before
that fixture edit; the corrected file then passed a dry-run check and diff
validation. The later 680-case hosted result above passes GameDirectory on
all three compilers and startup on Linux. It adds no original rendering or
ordinary-default-directory comparison.

`client.game_directory` runs the real native component in fresh authored
Unicode paths. It covers existing directories and accepted regular-file final
components, nested creation, canonical creation through a missing-prefix
`..` path, failure with a regular-file ancestor, independent owner lifetime,
and null/invalid/empty/encoding/length controls. Python independently checks
the exact created directory family and untouched sentinels after retirement.
No directory is deleted. Its Windows default case uses and checks a fresh
child-process APPDATA parent. Linux native-account default creation remains
excluded from the driver; no HOME override substitutes for that policy.

`client.startup` requires `MC_BUILD_CLIENT=ON`,
`MC_ENABLE_CLIENT_DISPLAY_TESTS=ON` and an actual OpenGL context. It creates
eight authored PNG/font/language/splash resources, then runs the real App nine
times with separate fresh game directories. Eight decoded-pixel relations
check loaded scale, explicit 1/0/negative overrides, initial anaglyph, malformed
line continuation, caught directory-load failure and absent-directory
creation. Ordinary options bytes, including an unknown key, must remain
unchanged; no run may save options automatically.

One splash, fixed splash time, identical button texture rows and empty button
labels remove the intended Random/time/hover variation without supplying an
expected image to the App. The local Windows runs use an NVIDIA RTX 3060 core
context; the hosted Linux jobs request software OpenGL under Xvfb without
recording a graphics vendor. The actual physical capture profile is 854 by 480;
other high-DPI outcomes fail that finite profile rather than changing the
expected scale. The PNG decoder checks framing, CRCs, legal chunk names,
noninterlaced RGB8/RGBA8, filters and a bounded complete deflate stream; palette
and transparency-key chunks are refused for this capture profile.

Each driver retains the complete raw streams, files, captures and receipts,
with binary and resource/input pins and failure cleanup. Forced direct-child
cleanup cannot pass. The outer process owner must supervise the complete
tree. These tests establish no original filesystem, unmodified default-home
or original rendered correspondence. [GameDirectory's reference contract](reference/game-directory.md)
records the remaining File, locale and startup requirements.

## Java output writer and GameSettings

The [shared output writer](core/java-writer.md) passes 110,708 stream checks,
19 real-file checks and five independent file checks per MSVC/GCC/Clang build.
All 15 selected headless cases pass on each compiler, including the renamed
temperature, LivingEntity and Dungeon regressions. Linux enables ASan/UBSan.
The complete retained source matches checkpoint `389b573`'s1,014 non-Markdown
Git blobs; all 630 C/header files pass clang-format18.

The MSVC/GCC CTest commands returned zero and every test passed. Their
controllers then rejected CTest's truncated successful Dungeon footer in the
JUnit file. Those reports remain failed. Independent file audits check the
complete raw output, actual Start IDs, Passed records and source footers.
Later metadata-only commands confirm unchanged test inventories and direct
imports for the two writer executables, Server constructor and Dungeon probe.
They rerun no tests. Clang's full controller passes with an explicit 256 KiB
bound for each successful test's retained output.

Run the public writer cases without Java, original archives or graphics:

```sh
ctest --test-dir build --output-on-failure -R '^core[.]java_writer(_file)?$'
```

The [GameSettings reference](reference/game-settings.md) separately records
16 original persistence cases on each of three charset/platform profiles.
The later writer observer passes 25 transport controls on each compiler.
Its finite Windows original/native comparison matches13,391 fields in 68
scenarios, with all 14 corruption controls rejected. The
[writer contract](core/java-writer.md#finite-original-comparison) defines the
matched projection and the unpaired Thread/private-state evidence.

The [native GameSettings owner](client/game-settings.md) adds headless defaults,
load/save and a public-state observer. Its two new tests are
`client.game_settings` and `tools.client_game_settings_probe`. They keep their
authored files, complete output and receipts below the build directory.
At the earlier persistence-only checkpoint, both pass on MSVC and sanitized
GCC/Clang: six actual CTest cases, with 481
native interface checks, six independent file checks and 26 observer transport
controls per compiler. All 635 C/header files pass clang-format18. The
1,199-file tested cohort contains1,024 non-Markdown files. A later independent
file audit rehashes1,706 artifacts, including all retained output trees and
the actual products. These focused native results and the later 669-case
hosted checkpoint retain their separate source and execution records.

The two Windows original/native comparisons each cover 16 cases and 64 paired
stages: 2,304 instance-role pairs, 11,389 matching fields and 2,877 logical
reference joins. Windows-1252/CRLF and explicit UTF-8/CRLF remain distinct
profiles. The separate GCC Linux UTF-8/LF comparison matches the same declared
projection; Clang Linux comparison remains pending. Exact plans are joined to
actual original input controls before a native target runs; no expected state
or saved output enters the producer. Independent Windows file audits verify
complete output trees, raw records, aliases and unchanged inputs.

Fourteen file-only controls on the Windows-1252 baseline reject the intended
changes: twelve full comparisons and two explicitly labeled admission-helper
checks. Full evaluations pass their consumed-file after-checks; an unrelated
pin error cannot count as an intended rejection. They run no native or JVM
target. The [reference record](reference/game-settings.md#finite-native-comparisons)
defines the finite projection. Static labels, Thread and stream-internal
correspondence and options screens remain unfinished. The new startup
connection is outside these older persistence projections and has only the
finite Windows and hosted Linux validation above.

## GameOption queries and labels

The earlier local [GameOption and label increment](client/game-options.md)
passes six actual Windows MSVC x64 Debug CTest cases in separate headless/client builds:
883 metadata/query checks, 1,241 label checks, 463 Assets checks, 4,404
Translation checks, 481 persistence checks plus six independent file checks,
and 26 probe controls. No test failed or skipped. The headless validation
controller stopped after the tests because its broad failure-word search
matched `0 tests failed`; its failed report is retained. An independent file-only
audit checks the actual raw/JUnit records, complete output trees, file bytes,
all controls, source/tool/SDK inputs and products. It does not repeat a test or
invent the unrun after-test inventory. The corrected client controller completes
all seven stages. Compile trees retain the declared qualified helper cleanup;
all test trees close naturally. Formatting passes for all 641 C/header files
with version 18.1.3. The later 680-case hosted run above also passes these native
contracts on Linux GCC and Clang with sanitizers. Original-label comparison and
interactive input remain pending. App startup has the finite Windows and
hosted Linux tests above; options screens remain unfinished.

## Float and discrete option actions

One fresh Windows MSVC x64 Debug headless profile passes these three CTests:

| Test | Actual result |
| --- | --- |
| `client.game_settings_actions` | 702 native checks and eight independent file-check categories |
| `client.game_settings` | 548 native checks and six independent file checks |
| `client.game_option` | 883 metadata/query checks |

The action target is `mc_test_game_settings_actions`. Its driver exercises the
real `mc_game_settings_set_option_float`/`mc_game_settings_set_option` APIs with
authored values and real files. The single profile closes all seven stages and
eleven independent final audits in 85.887 seconds. Tests exit 0 naturally and
are reaped; configure/build retain their qualified byte-pinned vctip cleanup.
There is no new local client-mode or Linux action profile and no original-action
comparison. The later hosted `2afd39a` checkpoint passes these native actions on
MSVC and sanitized GCC/Clang within all 683 cases, with 645-file formatting.
Earlier labels and App results retain their separate source and scene scopes.

The actual tested source contains1,215 public members, 1,037 non-Markdown
inputs and 645 C/header files. All 645 pass formatting before target execution
with unchanged source. A later file-only readback rehashes 2,788 retained members,
eight products, two capture trees, raw/JUnit records, identical test inventories
and source/tool/product pins. All seven consumed-file pins join the complete
actual output inventory. The retained input archive is a directory containing
the original authored files. Later result-documentation changes have separate
Markdown byte pins.

Before execution, review corrected bounded reads and joins between consumed
bytes and retained file hashes. The corrected driver and validation helper
were used for the first native action run, which passed.

The [implementation contract](client/game-options.md#float-and-discrete-actions)
covers all 13 descriptors plus null; foreign/BUSY report preservation;
float no-save behavior; completed no-effect discrete saves; signed arithmetic;
retained assignments at all six missing receiver fields; real completed-path
save output and reload; aliases, partial file prefixes and diagnostic reentry.
No fake SoundManager/RenderEngine/RenderGlobal callback can satisfy those
dependencies. Source PCs are static provenance, not measured action execution.

The concrete [SoundManager](client/sound-manager.md) now supplies the audio
option dependency. `client.sound_manager` records authored backend call traces,
while `client.settings_sound` checks genuine nested settings saves and guarded
diagnostic reentry. These tests do not qualify original SoundSystem or devices.

`client.vorbis_codec` exercises the low-level source-policy owner with frozen
authored Ogg bytes. Enable `MC_BUILD_CLIENT_SOUND=ON` for headless codec tests.
`MC_ENABLE_SOUND_REFERENCE_TESTS=ON` adds `reference.vorbis_policy`, requiring
JDK17 and `MC_REFERENCE_CLIENT_JAR`. It compares complete declared page/buffer
traces and reports exact PCM equality separately. Current mono synthesis
differences remain failed PCM comparisons; policy success never covers them.
The direct tool's `--require-pcm` makes those differences fail the command.

## Binding-code action

The new [binding-code action](client/game-settings.md#binding-code-action)
extends `client.game_settings` without changing its existing file driver or
the six independent file checks. Fresh Windows MSVC x64 Debug headless and
client builds pass all six selected CTests: 548 native settings checks plus six
independent file checks, 26 probe controls, 883 option checks, 1,241 label checks,
463 Assets checks and 4,404 Translation checks. Each profile completes all seven
stages, including byte-identical before/after test inventories, and all ten
independent final audits. Both use the same 1,206 public files and 1,376 physical
source inputs. All 641 C/header files pass clang-format18.1.3.

Configure and build retain the admitted pinned compiler-helper cleanup
allowance; all test trees exit naturally. Complete streams, JUnit, file receipts
and products are rehashed after execution. These results qualify the specified
native Windows contracts. The later `fce1539` hosted run also covers the action
on Linux GCC/Clang. The earlier 481-check and `2e02a61` results retain their
historical scopes.

The added authored cases target current-array aliases, signed int32 codes,
same-code calls that still rewrite the real file, and the noarg null-File save.
They also check a retained file prefix when a later null binding is caught by
save, directory/missing-parent failures, source null/index precedence before
save, unchanged file sentinels and diagnostic reentry/retirement refusal.
The production action uses the same admitted save body, preserving code and
file effects on later failure. No original binding-action comparison or
Controls UI execution has been added.

## Canonical Minecraft names

The renamed source passes all 619 public/JDK tests: 205 MSVC, 207 GCC and
207 Clang, with no failures or skips. Linux enables ASan/UBSan. All 587 C/header
files pass clang-format18; controller mirrors and 556 positive/2,011 negative
lifecycle interval controls pass on both platforms. The retained 1,130-file
source copy matches the naming checkpoint's non-Markdown inputs.

Eight additional file-only controls check complete historical/current module
families and WorldProvider recorder provenance. Fixed JSON/TSV fixtures keep
their exact bytes. The initial naming runs rejected historical paths and the
old recorder binding; explicit compatibility checks resolved those failures
before the full passing reruns. The [naming reference](reference/minecraft-names.md)
records the current roles, source aliases and preserved transport labels.

## WorldAccess and redstone constructor inputs

At checkpoint `ef1434015420678559cfa3d3b2f21018c5b733ce`, the predecessor of the
current `world.world_access` case passed 295 native checks through actual GAME World methods and
typed callback receivers. It covers all six implemented slots, raw numerical
arguments, live append/removal/replacement, duplicate and NULL registrations,
missing callbacks, partial failures, Entity sound coordinate changes, bounded
allocation and terminal holds. Callback-driven redstone updates reenter another
World sharing the same Registry. Those controls passed on MSVC and sanitized
GCC/Clang; original nonempty WorldAccess correspondence remains unverified.

The current `world.redstone_probe_inputs` entry names 52 native subprocess controls
on authored copied Worlds, verified at that checkpoint under their former name.
They check the unpaired CLI and declared constructor tapes, including
signed int64 boundary samples, actual discovered consumption, exhausted/unused
inputs, early witness rejection, canonical-format/size limits and existing-output
protection. Full operation prefixes and file preservation are checked separately
from consumption. They do not establish original tape provenance or clock parity.

```sh
ctest --test-dir build --output-on-failure -R '^world[.](world_access|redstone_probe_inputs|redstone_lifecycle|redstone_multichunk)$'
```

Add `-C Debug` for a Visual Studio build. These tests need no original archives.

At `ef1434015420678559cfa3d3b2f21018c5b733ce`, the complete local public/JDK suites
passed 205 MSVC, 207 GCC and 207 Clang tests, 619 in total, with no failures or
skips. Linux builds enabled ASan and UBSan. All 587 C/header files passed
clang-format18; controller mirrors and all 556 positive/2,011 negative interval
controls passed on Windows and Linux. The retained project copy matched that
checkpoint's sources. Its [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40052114598)
also passed all four jobs, all 619 tests and 587-file formatting. These results
precede the canonical naming migration and do not qualify the renamed sources
or schema-2 redstone producer. The [naming reference](reference/minecraft-names.md)
records the unchanged compatibility labels and migration boundary.

## Dedicated construction and synchronous fluid recursion

Public `server.construction`, `world.dedicated_entity_index` and
`world.dedicated_construction` cover native constructor prefixes, field shadows,
Entity publication, duplicate-key behavior and ownership. `world.fluids` includes
a stored enclosed water column whose seven downward cells execute synchronous
tick reentry on both sides. It checks every resulting ID and metadata value,
34 schedule requests, unchanged Random state and balanced World calls.
`core.java_random_profile_inputs` checks input progression, completeness,
fresh-profile admission and failure preservation without installing Random state.

```sh
ctest --test-dir build --output-on-failure -R '^(core[.]java_random_profile_inputs|server[.]construction|world[.](dedicated_entity_index|dedicated_construction|fluids|scheduling))$'
```

Add `-C Debug` for a multi-configuration build. These constructor tests use
project-authored inputs and need no original archives.

`tools.population_dedicated` runs eighteen fixture and original-semantic tests.
`world.population_dedicated_replay` runs the production native probe against the
lossless original-only fixture, then checks the complete comparison, actual
final disk inventory and eight native corruption controls. Both run without
Java or original archives. Replay supplies minimal resources and nine measured
constructor inputs to the native process; expected game state never enters it.

```sh
ctest --test-dir build --output-on-failure -R '^(tools[.]population_dedicated|world[.]population_dedicated_replay)$'
python scripts/reference/world/population/dedicated/replay.py \
  --executable build/mc_natural_population_probe \
  --work-dir build/population-dedicated-replay
```

For a Visual Studio build, use
`build/Debug/mc_natural_population_probe.exe` as `--executable`.

`MC_ENABLE_POPULATION_REFERENCE_TESTS=ON` also enables
`reference.population_dedicated`. Configure both supplied archive paths and JDK
17 tools using the existing reference settings. This workflow captures a fresh
original run, independently audits its source and Random observations, derives
the input tape, extracts exact original resource files and runs the native
comparison. A blank tested-commit setting retains null attribution; a named
commit must match the checked source context. Build and platform qualification
remain separate from capture completion.

```sh
ctest --test-dir build --output-on-failure -R '^reference[.]population_dedicated$'
```

The native child has a 120-second deadline, separate 4 MiB stream bounds and a
watched 64 MiB JSON bound. Public replay has a300-second CTest limit; the optional
fresh reference test has a600-second limit. Capture receipts record child
completion, termination, reaping and retained files.

The [dedicated evidence](reference/dedicated-world-construction.md) distinguishes
the inherited schema-1 original fixture from fresh schema-2 observations and
requires native schema2 throughout. Actual held Store tokens match the session
files at all eighteen boundaries on both sides. The eighteen Python tests and
eight actual-native corruption controls pass.

The [committed checkpoint](reference/dedicated-world-construction.md#committed-checkpoint)
at `9c87827` passes all 622 local executions: 206 MSVC, 208 GCC and 208 Clang,
with zero failures, errors or skips. All 1,099 source files match their literal
Git blobs; source, selected tools and products remain unchanged across testing.
Each compiler passes a fresh schema-2 original audit and native comparison,
including 18 original and 18 native actual Store-token boundaries. Linux enables
ASan, UBSan and leak checks. Independent retained audits repeat those checks.

The [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40000554912)
passes 601 public/JDK executions: 199 Windows, 201 GCC and 201 Clang. Its actual
merge checkout has the same entire tree as the code checkpoint. Formatting
checks all 569 C/header files; mirrors and 556 positive/2,011 negative interval
controls pass on every local and hosted build. The seven supplied-archive tests
per compiler remain local. Earlier development results, including 613 executions
and the 82.515-second fresh comparison, retain their dirty source and null tested
commits. The reference record corrects their historical byte-equality claim and
preserves the finite profile limits.

## Fresh World population

`reference.natural_population` and `reference.natural_population_ring` compare
fresh original and production GAME client/base-server Worlds for seed23. The
origin profile makes four acquisitions, retains four normal Chunks and populates
one, with 1,777 final ordered light regions. The ten-call ring profile reaches
negative coordinates, retains nine normal Chunks and populates four, with 6,968
final ordered light regions.

Both profiles compare complete retained Chunk arrays and headers, cache state,
ordered light regions, provider/manager numerical buffers, array aliases and
provider/cave Random state with copied continuations. Each run separately checks
its clock-seeded `World.r` remains unchanged. The
[evidence and commands](reference/natural-population.md) define the schedules,
resource binding, capture bounds and remaining acceptance work. Other seeds,
all feature selectors and arbitrary recursive schedules remain outside this
finite result.

Enable `MC_ENABLE_POPULATION_REFERENCE_TESTS=ON` with the supplied archive and
JDK paths, then run both fresh comparisons:

```sh
ctest --test-dir build --output-on-failure -R '^reference\.natural_population(_ring)?$'
```

Public tests need no Java or archives:

- `tools.natural_population` checks complete-state rejection, strict Random
  records, aliases and the derivation and integrity of the public ring fixture.
- `world.natural_population_replay` compares the real native origin profile
  with the older original-derived cache fixture. Numerical records receive
  transport and repeated-hit checks; this replay has no original numerical
  oracle.
- `world.natural_population_ring_replay` compares the real native ring profile
  with the public original-derived cache and numerical golden records. Its
  eleven boundaries include complete provider/manager arrays, aliases, Random
  state and copied continuations, plus shared state. The final cache hit must
  preserve the complete provider/shared projection, including array identities.

Both native replays use four project-authored translation strings and two empty
resource files. This minimal binding differs from the exact original resources
used by the fresh comparisons. The older [MCC1 callback replay](reference/cache-callbacks.md)
retains its separate imported-effect scope.

```sh
ctest --test-dir build --output-on-failure -R '^world\.natural_population(_ring)?_replay$'
python tests/tools/world/replay_natural_population.py \
  --executable build/mc_natural_population_probe --profile ring \
  --work-dir build/natural-population-ring-replay
```

Both population drivers accept `--profile origin` or `--profile ring`; origin is
the default. Use `--config Debug` and `-C Debug` for a multi-configuration build,
and the corresponding executable path on Windows.

### Earlier origin/ring qualification

Code checkpoint `4414dfd` passes complete local suites of 188 MSVC, 190 GCC and
190 Clang tests, 568 total, with no failures, errors or skips. Every compiler
runs both fresh original population profiles, both public native replays and
the prepared plant comparisons. Linux enables ASan, UBSan and leak checks.
All 550 C/header files pass clang-format18; all three hosts pass controller
mirrors and 556 positive/2,011 negative lifecycle-interval controls.

The independent [source and capture audit](reference/natural-population.md#committed-checkpoint)
verifies all 1,041 exported source files and all twelve fresh/public population
captures. Both resource bindings produce the same complete retained native
projection for every compiler, side and profile, excluding independently seeded,
unchanged `World.r` inputs. The [hosted code run](https://github.com/frankischilling/minecraft-c/actions/runs/39935917481)
passes all 559 public/JDK tests: 185 MSVC, 187 GCC and 187 Clang. Formatting and
all mirror/interval checks pass; all four actual checkout trees match the code
commit. The nine optional supplied-archive population/plant comparisons are local.

## Populated Chunk save/readback

`reference.population_save_readback` follows the unchanged origin schedule with
time 1234, two forced cache saves and four direct region loads. It uses both
supplied originals and exact extracted resources. The same population reference
flag, JAR paths and Java paths enable this test.

Public `world.population_save_readback_replay` needs no Java or archive. It runs
the actual native producer with the declared minimal resources and lossless
original-only fixtures. Both comparisons check all twenty cache/numerical
boundaries, fifteen Info/disk inventories, complete saved typed NBT and fresh
normalized readback arrays. Session and region clocks remain measured per-run
inputs. `tools.population_persistence` rejects corrupted observations and fixture
attribution. The [scope and commands](reference/population-persistence.md) retain
the independent full World save profile, unload and restart as separate work.

```sh
ctest --test-dir build --output-on-failure -R 'population_(save_readback|persistence)'
```

Code checkpoint `eee7fb0` passes all 577 local test executions: 191 MSVC,
193 GCC and 193 Clang, without failures, errors or skips. Every compiler includes
all three fresh original population profiles, all three public replays and the
prepared plant comparison. Linux enables ASan, UBSan and leak checks. The
[committed checkpoint](reference/population-persistence.md#committed-checkpoint)
records the complete 1,048-file source audit, eighteen fresh/public captures,
eighteen resource-variant pairings and formatting/mirror/interval checks.
The [hosted code run](https://github.com/frankischilling/minecraft-c/actions/runs/39945110219)
passes 565 public/JDK executions: 187 MSVC, 189 GCC and 189 Clang, plus formatting
and every mirror/interval check. All four actual checkout trees match the code
commit. Four archive-dependent tests per compiler retain their local scope.

## Held World full saves

`reference.population_full_save` uses the same optional population-reference
flag and JAR/Java paths. After the genuine origin prefix it calls the actual
full save twice. Public `world.population_full_save_replay` needs no Java or
archive; `tools.population_world_save` has eleven corruption tests.

The [full-save record](reference/population-full-save.md) defines all twelve
numerical boundaries, actual empty Player/SavedData collection owners, complete
typed metadata and Chunk NBT, and exact within-run metadata rotation. It
preserves the distinction between original resource files and the public
replay's minimal inputs. Nonempty serialization lies outside this empty-list
profile. The separate [named MapData profile](reference/population-named-data.md)
compares one default value through full saves and a separate manager load.
Nonempty Players, trackers, broader named subtypes, source I/O failures, unload
and restart remain acceptance work.

```sh
ctest --test-dir build --output-on-failure -R 'population_(full_save|world_save)'
```

Code checkpoint `cd41262` passes all 586 local executions: 194 MSVC, 196 GCC
and 196 Clang, without failures, errors or skips. Linux sanitizers and leak
checks are enabled. All four fresh population profiles, all four public replays,
plant comparisons, formatting and mirror/interval controls pass. The
[independent audit](reference/population-full-save.md#committed-checkpoint)
verifies1,057 source files, 24 fresh/public captures and 24 resource-variant
pairings. Hosted verification is tracked in its [separate run](https://github.com/frankischilling/minecraft-c/actions/runs/39951520244).
Five supplied-archive comparisons per compiler retain their local scope.

## Held World named MapData

`reference.population_named_data` uses the same optional population-reference
flag, supplied JARs and Java paths. After the unchanged seed23 origin prefix,
the World puts one dirty default MapData under a key different from its name,
saves twice, and a separate manager loads the named file and performs a cache
hit. The original World manager stays attached. This finite profile retains
nineteen post-origin observations and twenty-four cache/numerical boundaries
per side, including 72 copied Random records and 360 array descriptors.

Public `world.population_named_data_replay` runs the actual native producer
with declared minimal resources and lossless original-only fixtures. It needs
no Java or archives. `tools.population_named_data` runs 24 Python tests with
132 rejection controls. Separate `world.saved_data_map_value` and
`world.saved_data_named` tests exercise the constructed ownership, type,
aliasing, mutation-prefix and resource-limit contracts. Nondefault maps and
failure cases in those tests retain native scope.

```sh
ctest --test-dir build --output-on-failure -R '^(tools[.]population_named_data|world[.]population_named_data_replay|world[.]saved_data_(map_value|named))$'
ctest --test-dir build --output-on-failure -R '^reference[.]population_named_data$'
```

The [committed checkpoint](reference/population-named-data.md#committed-checkpoint)
passes all 601 local executions on MSVC/GCC/Clang, with Linux sanitizers and
leak checks, 559-file formatting and all mirror/interval controls. The
independent audit verifies1,073 source files, 30 fresh/public captures and
30 resource-variant pairings. Hosted public/JDK checks pass 583 executions;
the eighteen supplied-archive comparisons retain their local scope.
Map updates, trackers and packets, nonempty Players, other subtypes, original
filename/stream/error behavior, unrestricted ordering, dedicated runtime,
unload and restart remain unfinished or unqualified.

## Plant lifecycle validation

The finite original/native comparison matches all 59 prepared cases per side
with zero mismatches. It compares every block/metadata cell in X/Z0..31 and
Y0..127, four continuation fields for each supplied and World Random stream,
and ordered Item stack, pickup-delay and position fields. The original fixture
executes unchanged Block callbacks and selected tree bodies with prepared World
services. These results do not establish original World/Chunk, WorldAccess,
neighbor or light-propagation parity. The [evidence record](reference/plant-lifecycle.md)
defines the projection and its limits.

`world.plant_lifecycle` exercises native GAME Worlds and their actual Chunk,
Registry, mutation and publication services. The separate
`world.plant_lifecycle_replay` compares those native projections with public
fixtures derived from original captures. Replay covers all 59 cases and twelve
semantic corruption guards plus154 malformed-input guards per side, including
altered maps, drops, every Random-tail field, request order and strict field types. Both tests run without Java or original archives:

```sh
cmake --build build
ctest --test-dir build --output-on-failure -R '^world\.plant_lifecycle(_replay)?$'
```

Use `--config Debug` for a multi-configuration build and add `-C Debug` to CTest.
To recapture and compare both supplied originals, configure a separate build
with a full JDK17+ and the verified archives:

```sh
cmake -S . -B build-plant-reference \
  -DMC_ENABLE_PLANT_REFERENCE_TESTS=ON \
  -DMC_REFERENCE_CLIENT_JAR=/path/to/b1.7.3.jar \
  -DMC_REFERENCE_SERVER_JAR=/path/to/b1.7.3_server.jar \
  -DJava_JAVA_EXECUTABLE=/path/to/jdk/bin/java \
  -DJava_JAVAC_EXECUTABLE=/path/to/jdk/bin/javac \
  -DMC_REFERENCE_TESTED_COMMIT=SOURCE_COMMIT
cmake --build build-plant-reference
ctest --test-dir build-plant-reference --output-on-failure -R '^reference\.plant_lifecycle$'
```

Replace the paths and `SOURCE_COMMIT` with the actual tools, supplied archives
and tested source. On Windows, use the corresponding executable paths and
multi-configuration options from [building](building.md). The comparison creates
fresh test directories and retains its report under the build's
`reference-plant-lifecycle` directory. See the [plant contract](world/plants.md)
for implemented callbacks and remaining gameplay work.

At the plant checkpoint, complete local suites pass183 MSVC, 185 GCC and 185
Clang tests, 553 executions with zero failures, errors or skips. Each suite includes the optional original
plant comparison; normal hosted builds omit those three archive-dependent tests.
Native plant contracts pass17,595 checks per compiler, including two adjacent
127-block columns removed recursively. Linux enables ASan, UBSan and leak
checks. All three hosts pass controller mirrors and 556 positive/2,011 negative
lifecycle-interval controls. Clang-format18 checks all 549 C/header files.
Earlier checkpoints below retain their original source and comparison scope.

### Committed plant checkpoint

At `2e9c6e0`, complete committed-source local suites and fresh original plant
comparisons pass with the results above. The separate
[hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/39918948340)
passes 182 MSVC, 184 GCC and 184 Clang tests, 550 total, without failures or
skips. All four checkout trees match the committed tree. The
[source-bound evidence](reference/plant-lifecycle.md#committed-checkpoint) records
tool versions, retained input/stream checks and the local-original distinction.

## Earlier Minecart inventory, NBT and storage validation

Full local suites pass all 180 MSVC, 182 GCC and 182 Clang tests, 544 total,
with zero failures, errors or skips. The C17 profiles use MSVC19.51.36260.0,
GCC13.3 and Clang18.1.3; Linux enables ASan, UBSan and leak checks. Each host
also passes controller mirrors and lifecycle intervals, including 556 positive
and 2,011 negative interval checks with zero failures. All 540 C/header files
pass clang-format18.

| Native case | Checks per host | Source cases |
| --- | ---: | ---: |
| `entity.minecart_inventory` | 598 | 40 |
| `entity.minecart_nbt` | 558 | 40 |
| `entity.minecart_storage` | 335 | 14 |
| `entity.minecart` | 7,270 | 164 |
| `world.minecart_detector` | 208 | 24 |

The storage driver separately passes 314 disk checks over six region files and
eight metadata files. The [inventory contract](entity/minecart-inventory.md) and
[native validation record](reference/minecart-inventory-nbt.md#native-validation)
separate semantic NBT, native preservation, genuine GAME reload and the staged
native failure control. A separate [original/MSVC finite comparison](reference/minecart-inventory-nbt.md#finite-inventory-and-semantic-nbt-comparison)
matches the complete declared inventory/semantic NBT projections on both sides
with zero mismatches: 24 traces, 155 actions, 334 snapshots and 23 planned source
faults per view. All 22 finite rules pass after four-view admission. This is
positioned null-World coverage; GCC/Clang comparison, optional native storage
policy and GAME disk equality remain outside this result. The historical
constructor comparison at `39edf12` retains its original inputs and projection.

### Earlier CMake D probe

The full suites retain thirteen tree inventories and use a 1,009-file source copy
based on public `3fb0881`. CMake D changed only the storage test's
Python REALPATH registration. A separate GCC one-test probe selects the real
interpreter through `/usr/bin/python3`; all 354 compile commands and 182 test
registrations match after source/build-root substitution. That probe is
separate from the three full-suite runs and adds no new committed or hosted
validation result.

### Hosted checkpoint and CMake E probes

Hosted `5ddb4f0` run `39855800472` passed 543 of 544 CTests. Windows passed
179/180, with the storage driver's redirected Python3.12.10 `python3.exe`
refused before native launch; GCC and Clang passed 182 each. All 540-file
formatting and Linux mirror/interval checks passed. Windows mirror/interval
checks were skipped after the failure.

CMake E has separate passing one-test probes: MSVC through a directory-junction
path to `C:\Python314\python.exe`, and sanitized GCC through the
`/usr/bin/python3` file symlink to `/usr/bin/python3.12`. Each passes 335 native
checks/14 cases plus 314 disk checks over six regions and eight metadata/session
owners. Windows joins all 354 compile commands and 180 test names/properties to
B after root substitution. GCC checks 354 configured commands, including 339
strict project commands, and 182 retained B test names/properties; its historical
B/C compile-command files were unavailable. Both probes retain stable 1,011-file
source/public-parent maps and fifteen frozen input trees. CMake E alone differs.

The [detailed record](reference/minecart-inventory-nbt.md#hosted-checkpoint-and-cmake-e-probes)
keeps those probes separate from the full B suites. The Windows junction probe
does not cover the hosted file-alias case. The CMake-only fix is committed at
`dd560492`. Neither probe establishes original Minecart equality.

### Corrected hosted checkpoint

The [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/39859975602)
at `dd560492` passes all 544 CTest executions: 180 MSVC19.44.35229.0,
182 GCC13.3.0 and 182 Clang18.1.3, with zero failures, errors or skips.
Storage passes on every host. Formatting checks all 540 C/header files;
all three hosts pass mirrors and 556 positive/2,011 negative lifecycle
interval controls. All four actual checkouts share the merge tree matching
that code commit, with all 1,011 source/blob pins stable. The
[closed checkpoint record](reference/minecart-inventory-nbt.md#corrected-hosted-checkpoint)
retains the full log/audit identities. This adds no new original-runtime
inventory, NBT or storage equality.

## Earlier power and rail checkpoint

Full native suites pass all 175 MSVC and all 177 tests on each GCC/Clang build, with zero skips.
Java standard-library numerical comparisons are enabled;
original-JAR comparison options remain off. Linux enables ASan, UBSan and leak
checks. The same per-test deadlines and output policies remain in force.

The [power case](world/power.md) passes 7,593 checks per host using two real GAME
Worlds and two loaded Chunk records. It covers all 69 kinds, metadata/side
matrices, ordered neighbors, conduction, complete redstone output/connectivity,
late namespace reads, guarded getters and nested failure frames. The separate
horizontal-direction case passes 359 checks, including all seven allocation
failure cuts and retained-array captures. Existing public solid support passes
4,359 checks. At the earlier `9f9d3a2` source checkpoint, a separate
executed-original comparison matches all 5,365 literal inputs per side against
the native MSVC observer: 75,110 whole snapshot-field comparisons, 5,365
completion classifications and 5,164 normally returned booleans per side.
The 201 target failures per side remain recorded with their booleans excluded;
Throwable/native errors are not equated. The
[finite evidence record](reference/power.md#validation-boundary) defines the
seven fields, source binding, hashes and remaining exclusions.

The [rail case](world/rails.md) passes 814 checks across144 literal constructed
cases per host. Two real GAME Worlds and two loaded Chunks exercise topology,
search, reached lifecycle callers, namespace order and retained partial writes.
Exact allocation checks account for actual persistent lighting records and
pointer capacity while requiring temporary rail ownership to retire. The
separate error-transport case passes 1,444 checks across 94 constructed cases,
covering explicit selections, current/deepest causes, PC0, unentered calls,
aliases and refused plans. The Snow regression passes 392 checks, including
a real null-Random rail-drop failure after both preceding Snow removals.
These are native contracts. Positive Minecart construction and detection now
have the separate validation above; ordinary positive rail drops and
executed-original rail equality remain unverified.

The [Class-query case](world/class-entities.md) passes 373 checks per host using
eight real Worlds and eight loaded Chunk records. It checks fresh result
identities, duplicate holds, R independence, class-before-bounds ordering,
nullable failure sites, partial cleanup, overlapping growth budgets and native
retain/lease overflow. The passive Class identity/ancestry case passes 13,819
checks while preserving the 24 registrations.

The [pressure-plate case](world/pressure-plates.md) passes 561 constructed checks
per host. Both Registry sides use eight actual GAME Worlds and four loaded
chunks each. The cases cover real Item/Falling membership and duplicates,
activation/release guards, float/int wrapping, independent category comparisons,
support loss, detailed acquisition failures and partial metadata/F/E effects.
The movement regression now passes 564 checks. No original pressure/Class runtime
comparison or running tick-loop acceptance follows.

The state-shape contract exercises all declared virtual bodies through actual
World/Chunk/Registry owners. Its cases include setter tuples, all six directions,
object identity versus numeric ID, late progress, partial stores, raw NaN bits,
lazy invalid-TileEntity cleanup and hold/pool preservation.

| Case | Checks per host |
| --- | ---: |
| `world.power` | 7,593 |
| `world.rails` | 814 across144 cases |
| `world.rail_errors` | 1,444 across 94 cases |
| `world.snow` | 392 |
| `block.horizontal_directions` | 359 |
| `world.solid_support` | 4,359 |
| `world.class_entities` | 373 |
| `entity.classes` | 13,819 |
| `world.pressure_plate` | 561 |
| `entity.bounds` | 4,751 |
| `world.piston_tick` | 575 |
| `world.piston_finalization` | 460 |
| `world.piston_tile_entities` | 296 |
| `world.piston_geometry` | 386 |
| `world.mutations` | 218 |
| `world.collision_boxes` | 732 |
| `entity.movement` | 564 |
| `world.movement_dependencies` | 202 |

Reports retain complete configured/executed CTest names, zero-skip JUnit results,
command arguments, actual child PIDs/exits, full stdout/stderr and before/after
source maps. Controller mirrors and lifecycle interval checks also pass. The
Windows run uses a sealed source copy; GCC and Clang use the same bytes on
the native Linux filesystem. All 977 tested files match their source map before
and after execution. The prior 961 public files remain unchanged throughout
validation; the 29-file code/test/driver overlay is then promoted byte-for-byte.
The tested source-map SHA-256 is
`1c6bf379c6f4d4cdaa742caf9ca12e7d4d494c1a7b478574e60b50318d7d55b4`.
This is a canonical file-map digest, not a Git commit or tree ID. Windows and
native Linux source bytes were also independently rehashed after closure.
Validation prose is added afterward; implementation, test and driver bytes
remain unchanged. All 515 C files and headers pass clang-format18. These are
public regression results, not complete original acceptance.

Earlier Linux runs using `/mnt/c` sources each hit the existing 180-second
deadlines in `tools.world_storage_reference`, `tools.chunk_storage_reference`,
`tools.level_reference` and `tools.callbacks_reference`. The exact copied-source storage guard completes
all 76 cases in 15.9 seconds. Fresh Linux source/build trees pass without changing
test limits. Failed mounted-source attempts and their complete streams remain
retained; they are not relabeled as successful runs.

The separate original tick family qualifies114 inputs and 228 snapshots per
side. At source `eab4533`, MSVC, GCC and Clang replays pass complete native
protocol qualification against the same original family. All 114 literal inputs
and normal-return versus failure classifications match per side on each compiler.
The eleven-field projection matches 5,016 primitive values per compiler across
228 snapshots per side. It includes the Piston's seven own fields and inherited
TileEntity x/y/z/invalid, with exact current/previous progress bits.

The earlier malformed MSVC streams, failed protocol audit and intact-record
diagnostic remain retained unchanged. The
[original observations](reference/piston.md#active-tick-input-family) keep their separate qualification scope.
The finite comparison excludes caller Context coordinates, World references or
aliases, complete Java/native error equivalence, World/Entity/Block graph equality,
original-runtime state-shape equivalence and whole-engine parity.

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
space. Hosted CI runs the configured167 MSVC and 169 per Linux compiler cases;
that full-suite scope is distinct from these focused local results.

The [movement source map](reference/entity-movement.md) records native reproduction
commands and a separate original family: all 59,654 client records and only
24,109 intact, non-prefix server records qualify after disk exhaustion. Failed
receipts and missing historical before-pin objects remain explicit.

The [retained-original native replay](reference/entity-movement.md#retained-original-native-replay)
passes all 83,763 selected rows on each of MSVC and sanitized GCC/Clang: 59,654
client and 24,109 intact server rows, including 10,874/4,741 exact failure
prefixes. Each server process also executes11,639 unobserved state-only inputs
to preserve pool history. New captures retain every raw byte through gzip,
with raw/encoded hashes, decompression/CRC and natural-zero ownership closure.
Probe objects relink unchanged qualified core archives; a subsequent Python-only
fix has a separate unchanged-binary input binding. These results add no full
current local-suite run, complete original server/normal movement or whole-game
acceptance.

The following Piston checkpoint predates Entity movement integration.

The [Piston contracts](world/tile-entities/piston.md) pass on native Windows MSVC and
sanitized GCC/Clang. All 165/167/167 configured cases pass in focused and
complementary partitions, without skips and with numerical Java checks.
The new cases pass 295 Direction, 296 TileEntity and 144 geometry checks per host.
They exercise real class owners, ordered NBT, GAME save/readback, all six faces,
installed extending/retracting TileEntities, late failures and live pooled aliases.

The [interpolation replay](../tests/tools/world/tile_entities/compare_piston_interpolation.py)
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

The [Overworld controller checks](world/population/overworld.md) pass all 156 configured
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

The preceding [live TileEntity contracts](world/tile-entities.md) add real Chest/Spawner owners, actual
NBT imports before normalization, Map/List holds, item drops, detailed acquisition
failures and joint native retirement. All 143 configured cases pass on Windows
MSVC and all 145 on each Ubuntu GCC/Clang build, with zero skips and local JDK
comparisons enabled. Linux uses ASan/UBSan and leak checks. Each host passes
468 TileEntity checks and 98,868 provider-activation checks, retaining 672 unchanged
selected source inputs, 254 external zlib pins, 161 products and 31 captures.
Linux source/dependency copies match before and after execution. Clang-format
18 checks all 383 C sources and headers. These native contracts do not complete
live Entity/TileEntity saving or original/native feature replay.

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
checks do not complete live TileEntity ownership, factories, Map/List joins or saving.

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
complete 18 calls and retain39 full contexts; all 18 framing controls reject per
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
pass177 height and 2,000,382 queue checks within38 selected cases on MSVC,
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

The [WorldChunkManager contracts](world/biome-registry.md#worldprovider-and-worldchunkmanager)
pass 304 noise-backed WorldChunkManager and 386 fixed-manager checks within all 28 selected cases
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
The array unit now reports11,312,060 checks: the added native borrow counter
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
each explicit Linux locale regression reports14,565 statistic checks.
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
allocation-budget publication boundaries. Block has102,659 checks, including
the retained class/PC of a nested failure after another class fails.
The finite original comparisons match all
[33,064 Item rows](reference/item-registry.md) and
[3,950 Block rows](reference/block-registry.md) per side. These are selected
runs; the complete local matrix below remains bound to its recorded source.
Full bootstrap and broader original equivalence remain unfinished.

The [quiet metadata addition](world/blocks.md) passes six selected cases on
MSVC, GCC and Clang, with Linux ASan/UBSan and leak
checks. These cover World construction/GAME, Overworld ChunkProvider construction,
chunk acquisition, packed nibbles and stored chunks. The GAME case now has
4,397 native checks, including actual stored and blank identities, bounds,
packed-key aliases, equal writes, retained acquisition failures and an
in-memory typed codec round trip. All 377 code/tool/fixture inputs remain
unchanged across these runs. No fresh complete-suite or new executed-original
claim is attached to this selected addition.

A following MSVC19.44 compatibility change scopes C4200 suppression to the
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
unchanged across the runs and retained copies. clang-format18 passes all
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
clang-format18 passes all 176 C/header files. The declared public inventory
was then 91; that selected run made no fresh complete-suite or hosted-CI
claim. Original disk captures and native disk contracts have distinct limits.

The preceding [WorldInfo binding checkpoint](reference/world-info-runtime.md)
passes all 89 public cases on native MSVC (126.50s), Linux GCC (274.28s) and
Linux Clang (272.83s). Linux uses ASan/UBSan and leak checks; Windows uses strict
Debug without sanitizers. Its new `world.runtime_info_bindings` case passes
173 checks, initially selected and again in each complete run. All 339
code/tool/fixture files stay unchanged across the runs. clang-format18 passes
all 172 C/header files. This matrix refreshes the existing Info unit and graph
replay, without adding a new original-game capture or hosted-CI result.

At the Entity construction checkpoint, all 85 public CTest cases passed on
native MSVC (114.62s), WSL GCC (244.09s) and WSL Clang (246.16s). Linux used
ASan/UBSan and leak checks; Windows used strict Debug without sanitizers.
After that matrix, test field macros were adjusted for stable clang-format18
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
| `world.saved_data_ids` | 196,823 native checks per compiler: complete signed-Short cycle, copied nullable UTF-16 keys, stable counters, raw load/write, independent owners, stale sessions, caught errors, reached partial files and resource bounds | Constructed contracts; original HashMap disk order, forced allocator/stdio failures and actual World startup remain unfinished. Named values have separate tests below |
| `world.saved_data_map_value` | 800 constructed checks per compiler: actual side class, name/dirty/scalar cells, shared colors and record lifetime, exact tag types, signed scale, resize spills, partial failures and bounds | Native contracts; nondefault maps and failure cases have no fresh-original qualification |
| `world.saved_data_named` | 33,144 constructed checks per compiler: side-bound put/get, nullable String keys, aliases and first-identity removal, ordered dirty saves, caught partial loads and stable retained owners | Native manager contracts; source filename/stream/error prefixes, printed diagnostics, unrestricted ordering and Java allocation timing remain unqualified |
| `tools.population_named_data` | 24 Python tests with 132 rejection controls for complete original/native observations, fixture digests/source attribution, typed payloads and owner trajectories | Public validation controls; launches no game or native producer |
| `world.population_named_data_replay` | Fresh native seed23 World with one default MapData, two full saves, separate manager load/cache hit; complete original-derived cache, Info, owner, numerical, NBT and file projection | Minimal project resources; one default value and seed, no Java/JAR execution, nonempty Players, map updates/trackers, source I/O failures or restart |
| `reference.population_named_data` (local opt-in) | Fresh original/native default-map profile on both sides and all three local compilers; 24 boundaries, 72 copied Random records, 360 array descriptors, typed metadata/Chunk/MapData NBT and final disk rereads per side | Same held World and exact extracted resources; excludes broader manager/subtype behavior, nondefault maps, source failure policies, dedicated runtime and gameplay restart |
| `tools.population_dedicated` | Eighteen tests cover lossless packaging, provenance, compression, complete graph, used Random, dedicated fields, roles, paths, queue order, NBT and constructor inputs | Original-only offline invariants; source/classfile qualification remains inherited |
| `world.population_dedicated_replay` | Production dedicated probe, complete original correspondence, actual final file inventory and eight native corruption controls | One seed and seventeen-call schedule; minimal project-authored resources; no Java or archive execution |
| `reference.population_dedicated` (local opt-in) | Fresh original/JDI capture, archive-dependent source audit, measured constructor inputs, exact extracted resources and native comparison | Passes in the [clean MSVC/GCC/Clang checkpoint](reference/dedicated-world-construction.md#committed-checkpoint); one seed, 17 calls and 18 boundaries, with empty Entity/TileEntity/named-data/index collections |
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
| `world.runtime_profile-and-entity-bindings` | 104 checks per compiler using real stores/profiles/runtimes; nullable binding, typed diagnostics, RAW profile mismatch and READY/failed persistent world holds | Existing loaded-world foundation; no original World constructor order, collection insertion, registry or TileEntity behavior |
| `world.scheduled_membership` | 329 native full-key collision, duplicate identity, held-alias, time-wrap, growth-budget and output-preservation checks; Linux GCC/Clang with sanitizers and Windows MSVC without sanitizers | Membership layer only; no world scheduler, original comparison or forced allocator failure |
| `world.scheduling` | 424 checks using actual client Worlds, shared sequence owners, full keys, immediate ticks, RAW Info, separate F/E failure effects and twelve actual maintenance saves; MSVC/GCC/Clang with Linux sanitizers | Client native contracts; separate original observations have no native replay; tick draining, nonempty tick descendants and scheduled persistence remain unfinished |
| `world.scheduled_ordered` | 940 native identity, successor-hold, deletion-history, mutable-key, limit and output-preservation checks on GCC/Clang/MSVC | Finite collection contracts; no world drain/reentry or forced allocator/hold failure |
| `world.scheduled_ordered_replay` | All 13,584 captured JDK public-API steps match GCC/Clang/MSVC; immutable expected trace and provenance are checked | Captured Temurin17 profile with project-owned comparable records; no original game classes, Java6 or whole-world qualification |
| `tools.scheduled_ordered_protocol` | 101 constructed native protocol cases pass on GCC/Clang/MSVC, covering numeric/ID bounds, line endings, control bytes, the 160-byte line limit and exact failure output | Probe boundary only; no JDK/assets, operation-limit stress or world behavior |
| `core.java_gaussian_replay` | All 14,013 immutable JDK result/full-owner-state events match GCC/Clang with sanitizers and native MSVC; includes seeded Gaussian histories and positive-finite strict log/sqrt | Captured Windows Temurin17 profile; no constructor entropy, clocks, original game classes or exhaustive input claim |
| `core.java_gaussian` | 85 captured-result/cache contracts pass on GCC/Clang/MSVC, including stale bits after reseed, signed zero, no-draw hits, owner copies and strict-math boundaries | Explicit seeds and admitted FP environment only; no constructor entropy or arbitrary math-input claim |
| `tools.java_gaussian_protocol` | 157 exact native cases pass on GCC/Clang/MSVC: seed/ID/bound limits, malformed math, line endings/control bytes, 160/161-byte boundaries, full cache snapshots and exact rejection prefixes | No operation-limit stress, changed-FP experiment, JDK or original-game execution |
| `platform.nanotime` | 172 Windows / 173 Linux checks pass on MSVC/GCC/Clang: exact conversions, native OS brackets, unchanged outputs/clock storage and controlled unsupported MXCSR states | Native clock contracts; no original nanoTime/QPC conversion or timing-performance proof |
| `platform.millis` | 251 checks pass on MSVC/GCC/Clang: signed epoch/unit conversions, checked ranges, output guards, six FP-control variants and four ordered real OS brackets per compiler | Native wall-clock contracts and pinned upstream source; no forced OS failure, installed-library bridge or World-constructor qualification |
| `platform.nanotime_conversion` | 17,769 checks over 4,866 actual Java scalar vectors pass on all three compilers | Java operator formula and explicit native range policy; no clock sampled by the Java helper |
| `core.java_random_profile` | 191 native ownership, budget, failure-latch, constructor-commit, cold-holder contention, atomic Math and FP checks pass on all three compilers | Native contracts; no original-game startup or cross-language concurrent scheduling proof |
| `core.java_random_constructor_observation` | 18 checks pass on MSVC/GCC/Clang for recorded constructor inputs through the production tail and plain Random continuations | No production clock sample, shared-uniquifier advance or atomic Math-holder replay |
| `world.overworld_generator` | 99082 public checks for composed generation, fresh lifecycle flags, borrowed ownership, unpublished identities, mutable materials, repeated results, populated-cache resets, coordinate/resource preflight and allocation-failure preservation | Finite ordinary Overworld generator; no cache acquisition, population or propagation |
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
| `tools.overworld_reference` | Production helper schema, source/provenance, prerequisite and preservation guards | Synthetic checks; no original methods run |
| `tools.overworld_interop` | Lossless retained original baselines and complete sequential C generation replay | Fixed four-seed corpus; eight continuous nine-call inputs pass |
| `reference.overworld_generation` (local opt-in) | All four seeds and nine calls per side match in three pairings on MSVC/GCC/Clang at a08c120; complete local/world state, 73728 ordered query pairs, 1224 stage events, 144 inventories and eight untouched controls per build | Fresh finite headless base-world contexts; unpublished normal entry within the finite cave-scheduler domain; no dedicated-server cache lifecycle or population |
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
| `world.stored_chunk` | 67681 checks for stable state/array ownership, fixed schema shape, transient defaults, normalization and opaque edit guards | No entity/TileEntity factories, registration or subtype simulation |
| `world.noise` | 4850 checks: seeded permutation/state, independently traced original scalar/bulk/octave observations, zero sizes/layers and unchanged invalid-call outputs | Perlin/octave primitives only; no complete terrain or simplex biome noise |
| `world.simplex` | 5005 checks: observed constants/state/grids, mixed constructor ordering, original numerical boundaries, independent/default octave factors and unchanged rejected-call state/output | 2D simplex primitives only; no biome climate/classification or complete terrain |
| `world.biome` | 33155 checks: all 4096 observed table cells, 212 float and 79 double cases, climate vectors, buffer growth/suffixes, staged allocation rollback and unchanged rejected-call state | Climate/classification module; no world startup, terrain or complete biome behavior |
| `world.biome_registry` | Actual shared objects, field/list/Class identities, literal selectors, cold/warm and retained failure contracts | Class bootstrap and ordinary helpers; no World binding or gameplay |
| `world.biome_reference_sampling` | Cold, warm, partial channel/class failure, array reuse and whole-call lifetime contracts | Direct square sampler with native bounds; no World generation or surface join |
| `world.biome_runtime` | 304 checks: constructed Worlds, same-registry binding, actual cache and pointer results, callback replacement, retained failures, all three client constructor cause frames and manager lifetime | Native integration and existing climate fixtures; no new original World observation or generation |
| `world.fixed_biome_runtime` | 386 checks: both sides and fixed kinds, cold l/m setup, rectangle/empty/suffix effects, natural allocation/fill failures, aliases and registry retirement | Native/source contracts; no fabricated recursive null-e state or Java OOM/GC timing |
| `world.temperature_runtime` | 371 checks: retained original temperature/auxiliary bits, actual noise-backed/fixed owners, null/empty/reuse/growth, suffixes, manager/root retirement, real late c/output quota effects and FP ordering | Concrete method and native ownership controls; no fresh original OOME, provider w/controller, snow or natural population qualification |
| `world.overworld_population` | 2,219 prepared native checks: real complete cache-triggered schedule on both class sides, full all-air Chunk images and eight saved slots, w/ledger identity, cold resource failure, cache hits, leases and aliases after provider retirement | Native controller/ownership scenarios; full original RNG continuation, all selectors/reentry, falling/snow descendants and natural terrain remain unqualified |
| `block.registry_falling` | 408 checks over actual declaring-class initialization, shared flag access, complete first/cached causes and genuine entered-resource BUSY calls on both sides | Shared flag contract; later FallingSand Entity ticks and physics remain unfinished |
| `world.biome_registry_observation` | Complete original graph/query streams, explicit byte/table controls and corruption guards | Finite Biome bootstrap and methods; no Java Throwable identity or full World startup |
| `world.terrain` | 535745 checks: three full original-captured density/base fixtures, constructor state, retained suffixes, interpolation ordering and unchanged rejected-call output | Density and base blocks only; complete terrain requires later passes |
| `world.surface` | 1114102 checks: seven complete paired-original fixtures, direct/reseed contexts, independent Random schedules, sea/material/bedrock/sandstone controls and failure preservation | Overworld surface pass only; constructed controls are separate from original fields |
| `world.caves` | 2021832 checks: ten complete paired-original fixtures and 30 captured passes, reseeding, fresh cache initialization, owner isolation, suffix/table preservation and C argument/coordinate controls | Overworld cave pass; protected geometry controls have separate evidence |
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
| `reference.density` (local opt-in) | 150 exact snapshots per side, 82 original density/scratch recomputation checks; all constructor, climate, density and base state across three pairings | Overworld only; no surface, caves, population or complete chunk parity |
| `reference.surface` (local opt-in) | 290 exact snapshots per side, 105 original state/material/density/suffix controls and 20 apply-field controls | Measured base/surface contexts; no complete chunk/cave/finishing compatibility claim |
| `reference.caves` (local opt-in) | Ten complete cases, 30 calls and 50 snapshots per participant; all state, blocks and Random observations in three pairings | Three pairings pass on MSVC/GCC/Clang at the recorded source; no complete chunk claim |
| `reference.caves_geometry` (local opt-in) | 44 separately labeled room/path controls, full input/output buffers, Random continuations, table and suffix controls; shared-draw consumption inferred by unique continuation matching | Three pairings pass on MSVC/GCC/Clang at the recorded source; valid private geometry domain only |
| `reference.nibbles` (local opt-in) | Original constructors, two complete getter sweeps and 16 setter snapshots compared independently in three pairings | Valid local packed-array access; explicit C invalid-argument behavior has separate tests |
| `reference.chunk_data` (local opt-in) | Five fresh/repeated input pairs, complete local arrays/heights, opacity and retained-object controls | Genuine original cache/queue diagnostics are retained separately; no C world lifecycle or propagation claim |
| `reference.level` (local opt-in) | Actual original info codecs and metadata handlers, exact types, fallback, clocks, rotation and session observations | Pure codec comparison passes at 9ad690d; native world-owner acceptance is separately recorded in storage evidence |
| `reference.region_write` (local opt-in) | Actual original region image writes, full images, allocation/growth and independently bounded timestamp fields | Global filename/cache observations do not prove original global read/write local-slot mapping |
| `reference.chunk_storage` (local opt-in) | Actual original raw chunk codecs, region loaders and serializers in genuine worlds, with complete arrays and typed opaque data | C activation of nonempty entity/TileEntity data remains refused |
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
role and profile bindings. The observed dedicated101-chunk profile permits a
600-second helper interval, matching its controller budget. Other profiles and
control, preparation and constructor intervals retain their 180-second bound.
The check also rejects events outside their own sampled interval. CI runs it
separately from CTest; full original capture validation remains required.

NBT comparisons require JDK11 or newer and both pinned supplied JARs. Enable
`MC_ENABLE_NBT_REFERENCE_TESTS` and set `MC_REFERENCE_CLIENT_JAR`,
`MC_REFERENCE_SERVER_JAR` and `MC_REFERENCE_TESTED_COMMIT` to the exact source
commit used to build the C probe. Missing, mismatched or unsupported prerequisites
produce exit77 and skipped status, not parity success. The manifest records
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

Overworld density/base comparisons use `MC_ENABLE_DENSITY_REFERENCE_TESTS` with
those same supplied-JAR/JDK/commit settings. The helper constructs real untouched
original world contexts in fresh ignored directories, then fresh Overworld generators.
It refreshes each original manager's16?16 processed climate before sampling.
Full constructor state, retained spans, exact double bits and base bytes must
agree between C/client, C/server and client/server. The internal original base
pass density is checked against the separately observed density. See
[density evidence](reference/density.md) and the [terrain interface](world/terrain.md).

Overworld surface comparisons use `MC_ENABLE_SURFACE_REFERENCE_TESTS` with the
same private inputs and source settings. They distinguish actual constructor
continuation from mutable live Random, compare prepared chunk-reseed contexts,
and check current material attributes, complete fields/blocks and suffixes.
Genuine server constructor setup can itself generate chunks; measured calls
use a fresh generator afterward. See [surface evidence](reference/surface.md)
and [ownership/operations](world/surface.md).

Overworld cave comparisons use `MC_ENABLE_CAVES_REFERENCE_TESTS` with both supplied
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

Complete unpublished Overworld generation uses `MC_ENABLE_GENERATION_REFERENCE_TESTS`
and the same JDK17+/`jdk.jdi`, supplied-JAR and exact-source settings. Its C target
is `mc_overworld_generation_probe`; the opt-in test is `reference.overworld_generation`.
The verified corpus retains one fresh generator through nine ordered calls for
each seed23, 24, 26 and 51, including repeated, negative, packed-key-alias,
world-border and accepted cave-domain endpoint coordinates. It compares complete
data, queues, climate/scratch, all 82 terrain layers and separate Random
continuations, plus1024 ordered height queries per call. Each original traced
run needs a byte-equal untouched control. See [generation evidence](reference/generation.md)
and the [generation contract](world/generation.md) for exact capture hashes,
source attribution and observation bounds. The measured server context is base
world `dj`/cache `jb`; actual `MinecraftServer` construction uses `dp`/`he`.
The current results do not cover a running dedicated server's cache lifecycle.

At source `a08c12056a06261ed01d9f66999459f7ce9543b4`, the suite contains 35
public tests, or 36 with the numerical Java oracle. The selected original
finishing, light queue, chunk, nibble and Overworld generation comparisons bring
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

Both capture scripts require the pinned Windows Temurin17.0.17 installation
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

Dedicated `unload101` alone has a600-second controller/660-second exporter
allowance. Other observed profiles retain300/360 seconds, with unchanged
180-second control/preparation process limits. Complete evidence and every other
bound remain required. A timeout is failed observation, never native parity.

The controller now retains each serialized JDI object mirror through the session,
with a32768-mirror bound. The standalone regression above passed both marker and
pre-label observation cases on Windows JDK17 and Linux JDK21, including proved
disposal of128 separate control mirrors and capacity/duplicate-label checks.
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
and eight original comparisons. Independent audit verifies168 actual passed
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

Overworld composition source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes
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

Chunk contract, nibble and full-fixture units pass101534, 414 and 335 checks.
The chunk-data helper has12 public guards, including renamed-source and
injected-compiler-option regressions. The nibble helper has18 after adding a
temporary-directory alias regression. Canonicalizing the synthetic fixture
root fixes the Windows short-path mismatch; both successful and unavailable
captures preserve their expected results. All 18 guards pass on native
Python3.14 and WSL Python3.12, with Windows short paths also checked directly.
The C and comparison sources are unchanged from the recorded original runs.
Clang-format18 checks 61 C/header files. Public CI runs 28 tests per compiler,
without private JARs or world captures. Integration results belong in
[status](status.md).

Previous Overworld cave increment:

Overworld cave source `a507f59d756acf2d035ac7d4ea2da7171f60ad2f` passes all 33
configured tests on native MSVC and WSL GCC/Clang without skips: 22 public
tests, the standard Java numerical comparison and ten original comparisons.
All 30 original manifests identify that source and pass with empty errors.
Natural ten-case and protected 44-case records match independently between
C/client, C/server and client/server, including full arrays, Random continuation,
measured tables, preserved generator state, classes, inputs and artifacts.
Independent review also verifies the source Git objects and 634 current file
hashes across the six new reports. See [cave evidence](reference/caves.md).

The natural/protected units pass 2021631/2917405 checks and both helper suites
pass18 guards per toolchain. Isolated native MSVC AddressSanitizer runs also
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

Overworld surface source `f7ac19ebed8a14c499bd233c2dcf3fb0f2502360` passes all 24
configured CTests on native Windows/MSVC and WSL GCC/Clang without skips:
sixteen public tests, the numerical Java oracle and seven original comparisons.
All 21 original manifests identify this source and pass with empty errors;
every current probe hash matches its report. Surface's 290 snapshots match
independently in all three pairings, including complete fields, blocks,
materials, climate, constructor state and live Random continuation. All
equivalence, preservation, suffix and retained-apply controls agree.

Public surface tests pass1114102 checks and all six helper guards, including
filesystem links and completed-mismatch precedence, run without skips. Seven
fixed fixtures are traced to paired untouched originals; constructed C controls
separately exercise state-machine boundaries. Clang-format18 checks 36 C/header
files. The three libraries build with testing off and no Python/Java discovery.
Public CI runs seventeen tests per compiler without private files; current
results and integration state are recorded in [status](status.md).

Previous density/base increment:

Overworld density/base implementation source `1817417e8891e93ed5dbc205c9d1f6d8e6436a8d` passed
all 21 configured tests on native Windows/MSVC and WSL GCC/Clang without skips:
fourteen public checks, the numerical Java oracle and six original comparisons.
All eighteen original manifests identify that source, pass status and empty
errors, with matching executable hashes. Density's150 snapshots match in all
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
| Native Windows10 Home 10.0.19045 | Visual Studio 18 Community, MSVC19.51.36260.0, CMake4.2.3, Ninja, Debug, `/W4 /WX /fp:strict`, Python3.14.3 | All 33 CTests pass at the cave source, with explicit Temurin17 |
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
TileEntity simulation and recovery beyond checked host errors remain unimplemented.

No matched graphics/audio comparison, interactive rewrite play, extended session,
four-way multiplayer matrix, state-drift or performance measurement has occurred.
No performance target or additional platform support follows from these tests.
