# Build and tests

The genuine World-selection loop passes empty, populated, sixteen-save scrollbar
and real double-click Windows profiles:
real SaveFormat metadata and stable sorting, row selection during drawing,
same-parent Cancel, original-receiver trailing input and retained-old-RQ cleanup.
Select constructs the actual controller and stops at World-load `a(NULL World)`2.
Empty, populated and bottom-scroll 854 by 480 captures were inspected. Save and resource hashes remain unchanged,
without a new session lock. Create/Rename remain at `NEW162`/`NEW189`; Delete
stops at preparation 21 without skipping its prefix. The Windows MSVC Debug
ALL build and all 468 selected CTests pass without failures or skips in 197.43
seconds, excluding 46 configured reference-label profiles. The separate
headless Release ALL build and formatting for 1,356 files also pass. World
loading and gameplay remain unfinished.

The DateFormat owner matches 2,926 JDK17 vectors on Windows and Linux for
English/en_US, UTC/Eastern and UTC years 1967..2099. Other locales, zones and
ranges fail explicitly. The
[observations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/reference/platform/date_format/README.md)
record the selected JDKs and exact UTF16 scope. Native runtime requires no JVM.
The [World-selection contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-selection.md)
and [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#world-selection-in-the-default-runtime)
give the focused checks and remaining dependencies.

The [exact World-selection run at `1334db5`](https://github.com/frankischilling/minecraft-c/actions/runs/40813601193)
is terminal: Windows 440/440 pass in 243.78 seconds, Clang 461/461 pass in
370.74 seconds and GCC 460/461 pass in 193.30 seconds. Formatting passes for
1,356 files, with matching checkout trees and zero skips. Startup and its
fingerprint regression pass on all three hosts. GCC's sole failure is a real
Display creation refusal in the third later-owner constructor profile. Its
cleanup succeeds, but the captured SDL operation/detail was not printed and
the cause remains undiagnosed. A diagnostic-only follow-up prints those fields
without changing assertions or runtime behavior. Both optional constructor
profiles pass separately on Windows in 0.74 seconds; the complete 468-case run
had those two profiles disabled, so this is not a complete 470-case rerun.

The preceding default source menu passes four focused Windows MSVC Debug CTests:
GuiIngame, texture uploads, source screen events and the native menu loop.
Its 100-frame/32-tick run completes with cleanup and an inspected 854 by 480 capture.
That result does not qualify complete source run/PX/render/shutdown or gameplay.

The preceding startup/menu-initialization source passes all 460 selected CTests without
failures or skips in 207.15 seconds. The selection excludes configured
reference-label profiles. Nine focused cases also pass, including real normal
Session and GL-error startup through the source startup return, statistics and
source-file deletion, and MouseHelper CPU behavior. MouseHelper methods pass
4,225 CPU checks and 91 real-window checks; four consecutive window runs pass.
Formatting checks 1,325 files. A separate headless Release ALL build passes.
These results do not establish Linux qualification, original visual
correspondence, a running dedicated server or a playable session.

The preceding runtime passes all 463 selected Windows CTests, excluding 45 configured
reference-label profiles. Formatting checks 1,341 files and a separate headless
Release ALL build passes.

The first runtime [hosted run at `fab3769`](https://github.com/frankischilling/minecraft-c/actions/runs/40801330270)
stops both Linux builds on signed diagnostic-site conversion before tests.
The corrected transport preserves native `-1` and unsigned source sites.
Strict GCC/Clang syntax checks under WSL and three rebuilt Windows tests pass.

The later [exact `de4290b` run](https://github.com/frankischilling/minecraft-c/actions/runs/40802203908)
is terminal: Windows 435/435 pass in 329.63 seconds, Clang 454/454 pass in 189.45
seconds and GCC 453/454 pass in 339.00 seconds. Formatting passes for 1,341 files.
Unique Start/result pairs match every native selection, with zero skips; all
four checkout trees match that published head. Both Linux effect-upload and
source-event cases pass.

GCC's sole failure is the prelaunch `client.startup` fingerprint guard, whose
old assertion combines regular-file type and a 128 MiB bound. The log contains
no actual mode or size. A larger linked artifact is inferred from the completed
link and resolved path; the original artifact's size was not measured. The
current fix hashes complete executables in 1 MiB chunks with a separate 1 GiB bound,
retains identity/before-after checks, and reports actual type/size on rejection.
The public regression uses an authored sparse 134,217,985-byte regular file,
checks its full SHA256 including the nonzero tail and bounded memory, and
preserves the 128 MiB resource and 16 MiB PNG limits. It passes on Windows and Linux,
normally and under optimized Python. The fix is included in the current
increment; its later hosted result is recorded above.

Published predecessor `3ec9892` passes [all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40796960416):
434 Windows, 452 GCC and 452 Clang tests, 1,338 total, zero failures or skips.
Formatting checks 1,325 files and all tested checkout trees match that head.
The old Clang Display failure below remains undiagnosed despite passing later.

Published source `8288290` has a [failed hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40792366222).
GCC passes all 452 cases. Clang passes 451 of 452; `client.source_gl_startup_device`
fails during Display creation, and its log does not establish the underlying
cause. Windows stops during compilation on MSVC C4127 for a constant layout
condition. Formatting passes for 1,319 files. Both Linux jobs have 452 unique
Start/result pairs and zero skips; all four tested checkout trees match the
published source. The current source preserves the layout invariants with
compile-time assertions and adds actual Display error details without changing
the failing check. The later `3ec9892` hosted run verifies those test-source changes.

Earlier hosted baseline `5d61f89` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40746811719):
1,264 public/JDK cases, with 410 Windows, 427 GCC and 427 Clang cases. None failed
or skipped. Formatting checks 1,220 files. Native jobs check 4,445 current source
bindings across 2,283 distinct files before configuration and retain their
complete test suites. Audited logs contain matching unique Start/Passed pairs;
all four tested merge checkouts have the same complete tree as the published
head. Supplied-original comparisons keep their separate scopes.

That hosted checkpoint uses four CTest workers. A timing-dependent
owned-child fixture was corrected with a real live-inventory handshake; its
supervisor, validation assertions and timeout stay unchanged. The corrected
run retains every baseline case. Its test steps took 346/355/371 seconds on
Windows/GCC/Clang, compared with the earlier two-worker 432/417/273 seconds. These
separate hosted observations include cloud/build variance and do not establish
a general or causal speedup.

The [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
keeps this hosted baseline separate from the newer local results.
[Build instructions](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/building.md)
cover prerequisites, resource import and opt-in window/reference profiles.

CI caches CTest timing estimates by platform/compiler and retains four workers
and complete suites. Both Linux jobs in the `8288290` run had cold timing
caches; the successful GCC job saved its estimates. GCC restores estimates in the `3ec9892` run and starts long cases early. Its
test step takes189.20 seconds versus253.61 seconds in the earlier cold run;
source/cloud differences prevent a causal speedup claim.

## Earlier checkpoints

The preceding resource-startup increment passed 460 selected CTests without
failures or skips, including real startup windows, resource HTTP/XML and source
menu controls. The selection excludes configured reference-label profiles.
Formatting checked 1,319 files. A separate headless Release ALL build passed.
That local result did not establish Linux qualification.

An earlier startup-tail source has focused local coverage of 47 selected
Windows cases. Nine Core output cases retain their earlier Windows source and
test-header binding. Clang 18 and GCC 13 each pass 41 CPU/protocol and twelve
real-device cases, 53 per compiler, with ASan, UBSan and leak detection. Fresh
Windows normal Session and real-GL-error startup profiles pass 103 and 109
checks. At that source, startup reached the second GL diagnostic at 466 and
stopped before `cx` allocation at 470; the last App publication was MouseHelper
`C380`.

That earlier Release headless build completed all 372 build steps and linked the
actual `mc_region_inspect` utility, with `MC_BUILD_CLIENT`,
`MC_BUILD_CLIENT_SOUND`, `MC_BUILD_CLIENT_AUDIO` and `BUILD_TESTING` disabled.
No runtime tests ran in that graph. Utility linkage does not qualify server
behavior. The focused client cases do not establish a complete local suite,
Linux default-directory/Session startup, original-JVM or visual correspondence,
World entry or a playable client.

Earlier hosted checkpoint `671c003` passes [all four jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40639713371):
1,226 public/JDK cases, with 398 Windows and 414 per Linux compiler. Formatting
covers 1,164 files. The Font cleanup correction and preceding failures remain
in the testing record.

Earlier focused native coverage includes nineteen Windows loading-screen drawing
profiles with 888 checks and nine Windows startup profiles with 173,090 checks
across twelve actual Apps. At that source, startup reached EntityRenderer and
RenderManager construction, then stopped before statistics at offset 324. A combined
source snapshot also passes 29 selected CTests on both sanitized Clang and GCC
builds and links the native client. These selected runs do not establish a
playable session. The separate SaveHandler metadata/session cohort passes 27
selected tests per Linux compiler, with native Windows coverage recorded in
the [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md).

An earlier statistics/callback/logo/input composition reaches Controllers383
on the genuine Windows Session path (92 checks). Its Windows qualification
retains 26 actual CTests and two direct profiles over separately recorded
source phases. Clang and GCC each build the client and pass 56 selected CPU
plus six real graphics cases; the GCC source includes the later strict-warning
correction. A separate headless encoder scope passes fifteen selected cases
per compiler, including 91 new checks. These are focused native results;
World entry and a connected playable session remain unfinished.

An earlier normal-startup graph builds ALL on MSVC, Clang and GCC. Windows
passes 67 distinct selected CTests; Clang and GCC each pass 74 distinct cases
across 81 executions. None failed or skipped. This focused scope retains
774 constructor and 196,715 array/class checks and is separate from the
preceding hosted matrix. A real Windows startup process exited zero with
166 assertions, but its aggregate window qualification stopped afterward on
capacity and still has a reader grammar gap; six modes remain unrun.
[Client startup](Startup) links the exact scope and remaining work.

The earlier startup/resource cohort retains its separate 48 focused local
checks per compiler. Neither selected matrix establishes a playable session.

The preceding 1,885-file Timer, Minecraft class/worker, rendering and World/Player
cohort passed 1,163 local CTests: 393 Windows MSVC Debug cases and 385 cases on each
sanitized Clang/GCC Debug build with leak detection. Every preceding 389/381/381
case remained enabled, with no skip or failure. All profiles exercised real
OpenGL startup, title and World inspection. Formatting passed for 936 C/header
files; that cohort had 1,643 non-Markdown inputs.
Source and all products remained unchanged; all 283 Windows executables retained
their default 1 MiB stack. Documentation updates preserve pinned inputs.

The [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) separates the complete matrix from
individual operation profiles:

| Operation | Scoped evidence |
| --- | --- |
| Standalone App and array owners | 774 constructor and 196,715 array/class checks per compiler in the focused local scope; public constructor/array controls also run in hosted CI |
| Resource lookup observations | Three native compilers pass six selected CTests plus eleven profiles each; one retained Windows CPU default original attempt matches seven rows after independent read-only admission. The initial refusal remains retained. Hosted `37dc83f` compiles the probes and passes four pure checks per compiler; original ZIP/static/GL and normal startup remain outside these results |
| Ordered Timer | 1,267 constructor checks per compiler, existing arithmetic/clock controls; separate original eight-rate/ten-field qualification |
| Minecraft class and kg worker | 225 native checks and 70 normal/optimized controls per compiler; five original class/worker projections on prepared receivers, no instance-constructor claim |
| RenderEngine | 8,333 CPU/223 device/55 separate trace checks; 19 normal/optimized controls; fresh original 4,096-pixel/ten-glyph comparisons on both hosts and ten original GL rows on Windows |
| Biped construction | 4,381 checks/435 allocation prefixes; 128 normal/optimized driver controls and 838 fresh original records per compiler |
| LocalPlayer NBT and first-Player metadata | 490 NBT, 110 coordinate, nine diagnostic and 169 publication checks per compiler; 33 original cases and 1,302 normal tags. Fresh Windows originals, retained Windows observations for current Linux executables; separate 37-record wake prefix contains 33 tags, with full wake unfinished |
| Lava CPU animation | 132,659 native checks per compiler; fresh Windows/Linux originals match all 348 records across 23 cases and 31 calls |
| Singleplayer controller | 230 normal and 256 allocator checks per compiler; twenty original records per target, with fresh original execution on Windows |
| World Entity/TileEntity updates | 881 native checks and 54 exact original records per compiler |
| Weather method | 473 native checks, unchanged 8,565-check Info regression and 68 exact original records per compiler |
| Player spawn/reset | 270 native checks and 273 allocator checks; Windows compares 42 reset records and 36 position vectors |
| Selected camera | 438 native checks and 1,198 callback checks per profile; declared Windows original matrix comparison |
| Pickup particle and CPU manager | 4,571 native checks; 71 exact original records with 4,592 paired checks |
| Manager ownership | 12,095 native checks |
| Complete-App pickup/lifetime | 82 native checks |
| Active Player/Input/allocation | 678 active, 1,222 Input and 247 allocator checks |
| Active animation arithmetic | 8,676 JDK atan2 results |
| Water animation | 103,098 native checks and the separate exact original array/RGBA comparison |
| Item tick | 1,089 native checks and 70 original records: 37 states, 33 complete calls and four prefixes |

Earlier isolated profiles retain their own coherent products. The genuine
active-App original profile retains its Windows LWJGL scope. These component
comparisons do not establish a connected World gameplay loop or original visual
parity. Native startup and drawing results above keep their separate scopes. See [active Player ticking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-active-tick.md),
[pickup/effects](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/particles.md) and [Item ticking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/item-tick.md).

The preceding published `4a6b805`
[hosted validation](https://github.com/frankischilling/minecraft-c/actions/runs/40464703757)
passed all four jobs and 1,055 public/JDK cases: 345 Windows and 355 per Linux
compiler. All 1,043 preceding cases remained. All four checkout trees matched
the reviewed `c3c7e45cb8b9b94d86078cea1dd8ac41289c0315` tree. Formatting checks
936 C/header files, controller mirrors and 556 positive/2,011 negative lifecycle
controls passed per platform. Its original-archive comparisons retain their local
scope. Current owner and gameplay limits are in
[development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md).

At `4a6b805`, CI ran two CTests concurrently and allowed four parallel Windows
build jobs. In that run, both Linux jobs restored 11,318,434 cache bytes and downloaded no
additional package archives from the required 11.4 MB. Dependency installation
took eleven/twelve seconds; each package-index update still fetched 12.6 MB.
All compiler, sanitizer, graphics,
formatting and controller checks remained enabled. These observations do not
establish a controlled whole-run speedup. The earlier
[scheduling comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#continuous-integration-scheduling)
retains its separate measurements.

Build with C17, CMake 3.24+ and Python 3.10+. Client audio also needs a C++20
compiler for OpenAL Soft; project-owned runtime remains C17. Ordinary native
public tests require neither Java nor original archives. Follow
[building](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/building.md) for Windows/Linux tools, offline overrides and
separate build directories.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional reference tests require the locally supplied, verified archives and
the dependencies declared by each profile. Missing prerequisites do not count
as parity passes. The [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md) and
[development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md) describe the remaining acceptance work.

<a id="normal-generation-checks"></a>

The [former build page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md), including its
[normal-generation checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md#normal-generation-checks),
retains older commands, counts and their original scope. Current reproduction
commands and finite comparison limits are in the [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md).
