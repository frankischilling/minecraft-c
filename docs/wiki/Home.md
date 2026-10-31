# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. The runtime needs no JVM after local resource import.

The client now draws eight default Mob kinds: Sheep, Pig, Cow, Chicken,
Spider, Slime, Squid and Creeper. [Mob drawing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/mob-drawing.md) uses the actual TH/GV, concrete
renderer/model, PS and NW owners. Four independent loaded Windows sessions
for Spider/Slime/Squid/Creeper each return four World/HUD frames: delay,
unticked undrawn traversal, first draw and reuse, then checked cleanup.
The new families draw 22/5/9/6 parts and 18/2/18/8 shadow quads across their
two drawn frames. Existing-player statistics are declared inputs; these
controls do not qualify fresh-player tutorials or serialized Mob loading.

The creature comparison passes 3,722 native checks and 366,155 selected
original checks per host: 341 queries, 42 lists, 252 faces and twelve rejected
corruptions. Linux ordinary/read-only JDI and the audited matching-device
NVIDIA ordinary supplement keep separate scopes. Native global invocation
order, dynamic Block classes, original/native pixels and whole-frame equality
remain unpaired. Prepared matrices alone allow four float ULP or opposite-zero
numeric equality, with raw differences retained.

[PB particles](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/particles.md) now follow the actual Slime landing through N, eight distinct PBs,
DN tick, real World movement and layer 2 sprite drawing. Both hosts pass 1,863
native checks across five fresh processes and 67,444 selected original checks
over seventeen operations, including four actual null failures, 180 identity
pairs, 45 stream-progress relations and complete 48/48/384-word batches.
Four manifest and eight targeted comparator corruptions are rejected.
Eleven direct native constructor receipts have 77 separate scalar checks;
twenty original constructor intervals retain their own proofs. Constructor
entropy and raw landing x/z remain independent. Post-array current color,
concrete JDK buffer/List layout, boxed-Byte and flattened String identity,
full texture-state graphs and original/native pixels retain explicit limits.

[Mob ticks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-active-tick.md) include the actual Creeper attack/unseen bodies. Each host passes 615
native checks and 864 selected original comparisons: nine returns, the actual
null-World failure and two explosion-entry prefixes. The real FD/QX explosion
remains unfinished; later death/attacking stores receive no completion credit.
The sound fixture's empty WorldAccess List gives no audio-device coverage.

Single-player remains pre-playable. Wolf, Zombie and Skeleton with its genuine
held Bow remain unfinished. Active achievement/tutorial notices, real Canvas
focus/resize, Mob NBT and source-region writes, save/detach/reload, populated
item/HUD interactions, mining/placement, combat, further Block overrides and
TileEntities still require work. Server work follows single-player. Account
authentication is the sole approved gameplay exclusion.

At `c88e648`, all 98 selected targets build on Windows MSVC Debug and Linux
Clang18 ASan/UBSan. All 129 Windows and 122 Linux tests pass without
failures, skips or sanitizer findings. CTest takes 457.67/460.55 seconds;
complete test phases including following input audits take 458.08/501.85 seconds.
The same 2,610 compiled/helper/build/reference inputs remain unchanged
through both builds and tests and match the committed Git blobs.
All 26 fresh original-comparison wrappers pass, thirteen per host. A separate
current audit verifies 4,614 resolved paths, merging Windows separators
and `/mnt/c` aliases of the same file. Formatting checks 1,757 C files.
Six current binding tables pass with historical metadata preserved. Hosted
results keep their exact published-commit scope in
[Draft PR52](https://github.com/frankischilling/minecraft-c/pull/52).

The full local selection above was recorded at `c88e648`. A subsequent
test-only GCC repair makes the existing cosine-count assertion's `&&` grouping
explicit within `||`, preserving C precedence and every expected count. GCC13
with the actual target flags rejects the original and accepts the correction.
Rebuilt Windows and Clang18 creature comparisons each retain 3,722 native checks,
366,155 selected original checks and twelve rejected comparator corruptions.
Production and build configuration are unchanged; hosted results keep their
separate commit scope in [draft PR #52](https://github.com/frankischilling/minecraft-c/pull/52).

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
