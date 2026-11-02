# Explosion particles after LivingEntity death

The client now constructs, ticks and draws the actual GY explosion particles
emitted by a dying LivingEntity. Windows and Linux component comparisons and
a bounded Windows SDK Sheep death/removal/autosave scenario pass. The Linux
full build and 597 public tests also pass. Five corrected Windows focus profiles
pass; later parallel and serial graphical cohorts retain actual-focus failures.
The separate Linux SDK encounter remains unqualified. Hosted validation keeps
its exact tested-commit scope. Explosion damage and block
destruction retain their separate implementation requirements.

## Source bodies

An ordinary dying Sheep advances through `ls.U()` under the actual Timer. At
death age 21, it emits twenty `explode` callbacks. Each callback consumes three
Gaussian velocity samples and three float position samples in source order.
RenderGlobal constructs a fresh GY through its actual `n.a:296` branch and
queues it in EffectRenderer layer 0. No Random, clock or death-age repair
supplies the outcome.

GY preserves inherited XW construction, its own constructor Random/Math
operations, custom age/texture/movement tick and inherited billboard draw.
The tick stores old position, applies the postincrement lifetime check,
selects texture with Java integer arithmetic, adds upward motion, calls the
actual mover, then applies damping and the ground branch. The expired/dead
prefix still follows the remaining source operations. Zero lifetime retains
the reached Java division exception. Constructor, movement, draw and queue
failure prefixes keep their actual owners for checked cleanup.

The RenderGlobal distance branch also preserves Java `dcmpl` behavior. NaN
distance continues to the source particle-name comparison at 107. It does not
acquire an added finite-distance rejection. The two existing RenderGlobal-access
and sky-change controls pass in 0.35 seconds after that correction.

## Windows validation

The native GY component passes 193 checks and matches seven unchanged-original
tick profiles in 0.17 seconds. The CPU draw component passes 115 checks and
matches six original draws with 48 exact words per draw in 0.14 seconds.
Eight packet corruption tests pass in 0.18 seconds. All three tests pass
together in 0.23 seconds. Constructor and shared Math entropy retain their
independent scope; the tick/draw comparisons do not equate constructor streams.
The [original packet](../../tests/reference/entity/explosion_particle/README.md)
records the observation and comparator limits.

A preceding SDK snapshot passes 1,230 checks in 13.90 seconds. Actual left
click kills the authored Sheep, and the normal Timer reaches twenty callbacks,
twenty distinct GY owners and real World removal at death age 21. Three further
World/HUD frames return before checked cleanup closes both windows. Declared
execution inputs and imported resources remain unchanged.

## Autosave ordering and disk verification

After removal, the same scenario reaches the ordinary period-40 autosave. Its
passive test observer records old World time 39 and next time 40, with 24
selected eligible Chunks. Saving returns while live time remains 39; the source
time store has not entered. Independent NBT/region decoding validates the
actual entry/exit disk snapshots, including saved time 39 and Player Score 14.
The normal client compiles this observer out; it is enabled only in the test
target. Autosave remains enabled and no World time field is repaired. The entry
observer preserves cold EntityList phase, maps and counts across 380,628 bytes
before and after its own work; selected-slice calls remain zero at source entry.
Direct NBT traversal and observer-owned IDs avoid initializing that Registry.
Only the actual source save can warm it.

The region audit permits selected header cells and exactly four length bytes
plus the declared record length at each new selected-record offset. Abandoned
payloads and reused tail padding remain protected. Sixteen corruption controls
exercise these bounds and cold-Registry observation.

The autosave corruption suite passes sixteen cases in 0.91 seconds alongside the SDK test;
both pass in 14.85 seconds. This strict disk gate accepts only the source saves
it observes rather than requiring a World session to remain immutable.

## Earlier failures and remaining limits

The frozen earlier Windows cohort passed twelve of thirteen selected tests in
101.48 seconds, then failed at the missing GY construction path. Its 2,669
selected execution inputs and 410 selected resources remained unchanged. The
earlier 507-check, 10.37-second encounter covered a shorter lifecycle. After
particle implementation, a 602-check native continuation returned through all
twenty callbacks and removal, but its old immutable-disk gate rejected the real
autosave. These historical prefixes remain distinct from the current returning
source-save audit.

The pre-GY Linux build and sixteen focused tests passed in 27.76 seconds.
Six subsequent particle/renderer/packet regressions pass in 0.63 seconds. The
broad Linux full build and 597 public tests pass in 166.82 seconds with display
tests under Xvfb/software OpenGL, Clang ASan/UBSan, leak detection and halt-on-error.
The later 747-check Player frontier control has separate Windows/Linux coverage.
The asset-dependent Linux SDK death/autosave encounter remains unqualified.

The latest Windows full run passes 593/602 in 215.03 seconds, with 3,293
selected inputs and 823 resources unchanged. Actual foreign Remote Desktop
activation causes the instrumented Slime/Cow focus failures; seven source-binding
gates separately reject metadata stale after diagnostic-fixture changes.
Refreshed declarations now pass the exact seven-test rerun in 13.29 seconds
with comparisons and guards unchanged. The historical 593/602 run remains
failed; no new full passing result follows. The preceding 26-test
selection passes 24 in 188.02 seconds, including local attack in 12.65 seconds,
and records actual `msrdc.exe` owners for Slime/Wolf focus failures. Whole
Windows graphical focus stability remains unqualified. Older uninstrumented
foreground owners remain unknown; byte predicates and focus behavior remain
unchanged. [Testing](../testing.md#current-targeted-and-public-qualification)
preserves all earlier failure/pass scopes and the separate 1,900-check repeat.

Hosted evidence applies to its exact tested commit. Original full-frame/pixel
equality, Wool pickup, save restart interoperability and extended natural
encounters remain unqualified. No FPS,
frame-time or hosted CI speed result is claimed.
