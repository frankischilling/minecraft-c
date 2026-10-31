# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. The native runtime needs no JVM after local resource import.
The dedicated server remains unfinished.

The client constructs all eleven default [Mob kinds](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-construction.md),
runs their [spawn predicates](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-spawning.md), and dispatches their actual
[active tick chains](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-active-tick.md) through genuine Living fields.
Shared environment, action, movement and travel work uses real World queries;
sight uses the existing World ray tracer. Finite returning and failure-prefix
scenarios are qualified independently of original constructor entropy.

The [Sheep, Pig, Cow and Chicken drawing checkpoint](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/mob-drawing.md)
returns through the actual TH/GV, model, PS and NW owners. It includes unsheared
Sheep fur, Pig's false saddle predicate after all four saddle texture lookups,
Cow's virtual pose and nine-part model, Chicken's sine/flap pose and all four
terrain shadows. The remaining seven default renderer families, including
Skeleton's real held Bow, are unfinished.

Focused Windows and Linux comparisons each pass 3,350 native checks, comprising
1,556 main checks and 452/328/344/339/331 checks in five fresh component
processes. The paired draw comparison passes 206,656 typed checks, 17,012
expanded graph records and 5,107 identity pairs. It covers 316 ordered direct
queries, 35 lists, 210 model faces, eight shadows with 2,688 words, 18 direct
String texture calls, eight animation returns, ten model-work records and ten
comparator negatives. Constructor entropy stays independent.

Linux uses the full ordinary/read-only JDI capture. Windows uses its graph,
list and query authority plus a separately audited same-device ordinary NVIDIA
GL supplement: 36 inputs, four helpers, 45 artifacts and four manifest
negatives. The strict Windows-to-Mesa current-normal mismatch remains failed
evidence. Only prepared matrices allow four float ULP and numeric equality of
opposite signed zeros. Original/native pixels, live GL call order and whole-frame
equality remain unqualified.

Three separate loaded Windows profiles publish a genuine Pig, Cow or Chicken
through the ordinary Entity APIs. Each returns four later World/HUD frames:
one delay, one traversal that leaves the unticked actor undrawn, then first
compilation and reuse of that exact actor. Pig compiles six parts, Cow nine and
Chicken eight; reuse compiles none. Their shadow totals are 18, 18 and two quads.
Returns include `Minecraft.k:1328`, `N569`, `PX1013` and `HUD2101`, followed
by checked cleanup. The existing-player inventory statistic is a declared input;
Player pose and World Random are not forced, and ordinary subsequent ticks run.
These controls do not qualify fresh-player tutorial completion or serialized
Mob loading.

Single-player remains pre-playable. Spider, Wolf, Creeper, Skeleton, Zombie,
Slime and Squid rendering remain unfinished; Skeleton's held Bow is mandatory.
Active VU achievement/tutorial notices, Canvas/focus transitions, nonempty items,
save transport, health/combat, further Block overrides and nonempty TileEntity
draws still need implementation and verification.

The continuation also completes 361 random Chunk coordinates and 28,880 samples,
then all 1,000 random-display samples and 6,000 World Random draws. Actual
Living/Creature movement returns; terminal cleanup handles Path aliases, target
cycles and Piston-only roots. A selected Spider jockey retains its separate
unfinished mount. Separate legacy-clamp controls retain 46,802 checks and 3,872
exact matching-SDK samples per host.

The current local cohort builds all 93 selected targets on both hosts. All 121
Windows tests and 115 Linux tests pass, without failures, skips or sanitizer
findings. The same 2,559 inputs remain unchanged through all four build/test
phases. All twenty fresh original-comparison wrappers pass, ten per host; the
final audit covers 5,510 distinct paths. [Build and tests](Build-and-tests#current-local-qualification)
records the separate CTest and post-run-audit timings, formatting and bindings.
Hosted results retain their published-commit scope in [Draft PR #52](https://github.com/frankischilling/minecraft-c/pull/52).

The preceding path checkpoint retains its own results below.

Focused Windows MSVC Debug and Linux Clang18 ASan/UBSan runs pass the actual
World path wrappers, shared ChunkCache, PathFinder and shared-cursor Path.
Each host passes 16,995 native checks over 64 paired scenarios: 143,871 typed
graph/state checks, six separate failed-Path constructor checks, 12,464 ordered
query attempts and 87,246 query words. Seven corrupted comparisons are rejected.
Eight native ceilings and actual shared-Path/outliving-World controls pass.

Mob following adds four full returns to the previous source-prefix cohort.
Each host passes 113,936 native checks, 19,832 typed field checks, 14,558
identity relations and all 72 events over 62 scenarios. The split is 52 full
returns, two callback failures, four source-null failures and four remaining
drop/hurt prefixes. All thirteen native controls and six reference negatives
pass. Constructor entropy remains independent.

At `f510367`, all 90 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 117 Windows tests pass in 220.67 seconds and all 112 Linux tests in
264.30 seconds, without failures, skips or sanitizer findings. The same 2,508
C, helper, build and reference inputs remain unchanged through both builds and tests.
All eight original-comparison suites pass on each host. Formatting checks 1,722
files; source bindings cover 8,107 entries across 3,450 current files.

Detailed source order,
ownership and limits are in [client pathfinding](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/pathfinding.md) and
[Mob active ticks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-active-tick.md). These scoped results do not
establish a playable client.

- [Startup and current client paths](Startup)
- [Build instructions, test results and scope](Build-and-tests)
- [World simulation](World-simulation)
- [Storage and original-reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)

## Integration qualification

At `a303b86`, all 92 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 119 Windows tests pass in 227.09 seconds and all 114 Linux tests in
321.27 seconds, without failures, skips or sanitizer findings. The same 2,543
C, helper, build and reference inputs remain unchanged through both builds and tests.
All nine original-comparison suites pass on each host. Formatting checks 1,742
files; six source-binding tables preserve their historical metadata. These scoped
results do not establish playability or whole-frame original/native equality.

The subsequent clamp-registration test repair changes no production code.
It replaces an obsolete GL_CLAMP failure expectation with actual successful
upload, logical/physical sampler, image ownership and cleanup checks. The
other five failure cases and four original trace rows remain unchanged.
The direct test and both Python controller modes reproduce the old failure,
then pass on Windows and Linux with unchanged inputs. At `dbd1a83`, [all four hosted jobs pass](https://github.com/frankischilling/minecraft-c/actions/runs/41196374086) for the repaired
commit. The failed parent run is preserved. That qualification predates the
four-animal increment.
