# Mob tick diagnostics

This finite native CPU workload measures the actual public `FD.g` entity loop.
The retained Windows cohort compares two diagnostic changes on the same common
runtime: the successful World return writes the complete empty output directly,
and the World Mob dispatcher uses its own working error plus an optional child
workspace. Source simulation, getters, constructors and owning occurrences stay
in the measured path.

The [lossless producer report](../../tests/benchmarks/mob_tick_diagnostics_windows.json.gz)
contains every producer field and every interval row. Its decompressed bytes are
the exact 14,168,041-byte `run.json` from the terminal passing cohort
`mob-tick-diagnostics-ud0ju4w1`, with SHA-256
`59a15ea68a16e36c68080f9cfbd4063389550c97e396643b8e78a75df1223c0d`.
The [companion manifest](../../tests/benchmarks/mob_tick_diagnostics_windows.manifest.json)
records the compressed size/hash, all sixteen actual raw JSONL hashes and all
32 stdout/stderr hashes. Export verified every referenced file and its producer
byte counts. Gzip uses level 9, zero mtime and no filename field; two compressions
were identical and decompression matched the original bytes exactly.

Four rotated rounds ran sixteen serial processes on Windows x64 with an Intel
Core i9-10900F and 20 logical processors. The retained build uses MSVC 14.51
Release `/O2 /Ob2`, C17 and strict floating point. Each interval contains sixteen
completed `mc_world_runtime_tick_entities` calls, measured with QPC at
10,000,000 ticks/second. Constructors, loading, publication, report writing,
state inspection and checked retirement are outside each interval. The actual
World output finalization remains inside the timed API calls. All four products
link one identical compiled workload object, one complete World adapter and one
complete dispatch adapter, followed by identical ordered common archives.

| Variant | World successful return | Mob dispatch |
| --- | --- | --- |
| legacy | Previous clear/copy algorithm | Public independent Mob diagnostic |
| tail | Direct empty output plus retained observation | Public independent Mob diagnostic |
| workspace | Previous clear/copy algorithm | World working output and optional child workspace |
| both | Direct empty output plus retained observation | World working output and optional child workspace |

The legacy adapter restores the former diagnostic algorithms from
`2d9ea870` on the final common core. These measurements do not compare historical
whole products. They provide no Minecraft frame, FPS, rendering or original-JVM
timing result.

Each process runs seven cells: empty, then 1, 8 or 64 Pigs and 1, 8 or 64 Zombies. Every
cell has two warmups and 21 measured intervals, each with a fresh genuine
World/App namespace and 25 stored normal Chunks at x/z -2..2. Ordinary region
records contain a Stone plane at y=63, zero metadata/light planes, HeightMap bytes of 64,
and empty saved Entity/TileEntity lists. Actual World getters load and activate
the Chunks; actual canonical Mob constructors and `World.b(Entity)` publication
create the live actors. A genuine fixture Player remains unpublished. Normal
nearest-Player calls therefore return null, and no attack/hurt result is claimed.

Declared workload inputs select night/time 13,000, sky subtraction 4, difficulty 2,
World `g=17`/`Q=6001` and timestamp `1234567890000 + sample`. Actor positions are
`x=5+(i%8), y=64, z=5+(i/8)`, with zero yaw/pitch. Independent literal seeds
select each actor's workload Random, the World Random, the unpublished Player
Random and the real shared Math owner after genuine constructors finish. The
producer records the complete formulas. No captured constructor tape or inferred
seed is supplied. Actual constructor `living.F/G/aj` values remain independent
between processes; they are retained in raw and constructor hashes. Only those
three fields are excluded from the paired semantic projection. All downstream
fields remain paired. Their before/after constructor hashes happened to remain
equal in every completed interval; the reader permits real source evolution.

Typed little-endian FNV-1a guards include represented Entity/base/Living fields,
watcher payloads, Path arenas/Points, source pools, World fields/Info/lists,
Random states and Gaussian caches, owning ledgers, and actual Chunk bytes and
slice occurrences. Reference identities use deterministic typed traversal ranks;
addresses and struct padding are never digest input. Every actor advances its
actual source tick word by sixteen. All source/state/RNG/Chunk/ownership guards
match across variants for the same declared input, while raw constructor entropy
is retained independently.

The cohort completed 2,352 measured intervals and 224 warmups: 41,216 actual FD.g
returns and 859,648 actual Mob dispatches. A common passive adapter observation
counts each completed Mob invocation in all four variants. The workspace and
both products prove 429,824 total dispatches with a nonnull reservation; legacy
and tail prove zero workspace uses. Every process completes 148,672 native
checks and checked teardown, with empty stderr, complete bounded pipes and a
reaped child. Source, product and prepared-input guards remain unchanged.

The following are observed pooled medians in milliseconds for a sixteen-call
interval, with 84 measured rows per cell/variant. Four process medians per
variant are the independent process-level units; the 84 rows must not be treated
as 84 independent processes. Sample ordinals select the declared literal seeds;
matching ordinals controls workload input without creating more process replicas.

| Cell | legacy ms | tail ms | workspace ms | both ms |
| --- | ---: | ---: | ---: | ---: |
| Empty | 0.69575 | 0.46470 | 0.72155 | 0.45385 |
| Pig1 | 8.32905 | 8.03165 | 7.73175 | 7.46010 |
| Pig8 | 57.20590 | 57.65880 | 56.38905 | 55.72460 |
| Pig64 | 453.08890 | 454.51470 | 439.12655 | 440.58610 |
| Zombie1 | 8.61090 | 8.29265 | 8.55235 | 8.27385 |
| Zombie8 | 64.63710 | 64.94230 | 63.88440 | 63.86380 |
| Zombie64 | 509.91300 | 512.40825 | 501.50225 | 501.15020 |

The empty legacy/workspace-only cell executes the same path, yet its pooled
medians differ by 3.7%. Workspace-only is slower in all four empty process pairs,
by 1.5-8.1%. This control exposes process/order variation at the scale of several
occupied-cell differences. It diagnoses process/layout sensitivity, rather than
providing a universal busy-cell error bar or a correction to subtract. Empty tail versus legacy is lower in all four round
median pairs, by 30.7-35.5%. The small busy-cell gaps are observations, not
established wins or a uniform improvement claim. Zombie 8 and Zombie 64 each have
one round where both is slower than legacy. No busy-frame or FPS conclusion
follows from these component intervals.

These actual MSVC x64 sizes explain the diagnostic scope; they are not an
allocation or source-heap equivalence claim.

| Actual type | Bytes |
| --- | ---: |
| World entity error | 347,376 |
| Mob error | 346,624 |
| Effect error | 33,552 |
| Environment error | 106,096 |
| Action error | 296,600 |
| World scope | 347,448 |
| Mob scope | 96 |
| Mob child workspace | 296,600 |

The World workspace is one optional reservation per entered outer invocation,
disjoint from its working Mob error. Environment and action diagnostics share
the union sequentially. Each new Mob attempt initializes its working output;
selected failures copy into the parent's active child before scratch reuse.
Native owner holds, first failures, borrowed child diagnostics and checked
retirement remain authoritative. An absent reservation uses the existing child
allocation path without a new source failure or changed source budget. Nested
World calls own separate scopes. Separate functional controls enter the exact
reserve-bypass state; they model fallback and do not claim an injected allocator
failure. Timed workspace rows require a real nonnull reservation.

The first CPU opportunity, `mob-tick-diagnostics-p4h48cwu`, failed before interval 0
and contributes no accepted timing. Its prepared MCR planes were correct. The
native guard incorrectly expected minimum height 64 from serialized HeightMap bytes of 64.
A tight probe on copied retained input, `ground-probe-7oi1odb5`, showed every
earlier plane check passing and actual minimum height 0. Original `lm` construction
leaves nonserialized field i at its primitive zero default. `to.a(fd,nu)` stores
valid HeightMap/SkyLight and skips rebuilding them; the later `ld272 -> lm.i`
normalizes block IDs through `vi.a` without deriving a new minimum. Only that
expectation changed to 0. All voxel/metadata/light/HeightMap guards and first
difference diagnostics remain. The failed opportunity, probe and original input
hashes are retained; no runtime or failed input was repaired to obtain a pass.

## Collision reads during Mob travel

Two instrumented Far dusk runs broke down Mob travel in the native client. In
the busiest sampled frame (510.53 ms with tracing enabled), collision-box queries
used 66.44 ms in Zombie travel and 65.59 ms in Spider travel. Path requests used
about 2.9 ms in that frame. These are inclusive times and overlap with their
parent travel and action timers.

The ordinary collision-box scan now writes block-read diagnostics into the
enclosing call's private frame. Block and nearby Entity queries request the
Chunk acquisition diagnostic on failure. The collision scan captures `Block.m`
before the Chunk provider runs and indexes that same array afterward. The
public result and retained-execution diagnostics still use their original
publication points.

A Windows Release comparison changed only the three collision translation units
relative to `edd355cd`. It used the fixture above with eight and 64 Zombies and
the same current World and dispatcher adapters in both builds. Each count had
two warmups and seven timed samples per process, with two processes per build in
baseline, candidate, candidate, baseline order. Processes ran on logical CPU 4.
Each interval completed 16 actual World entity ticks. Matching sample seeds
produced equal actor tick words and semantic, RNG, Chunk, and ownership
digests. All 14 matched measurements at each count were faster.

| Zombies | Baseline median ms per World entity tick | Candidate median ms per World entity tick | Reduction |
| ---: | ---: | ---: | ---: |
| 8 | 3.399 | 3.213 | 5.5% |
| 64 | 26.232 | 24.567 | 6.3% |

The first uninstrumented Far dusk comparison lost foreground focus in two of
six runs. In the full repeat, every run stayed focused on DISPLAY2 with the
same fixed camera, 854×480 resolution, seed 123, and saved World time 12,420.
The median of the three run FPS values fell from 25.58 to 17.86. The median
run p99 rose from 82.11 to 190.08 ms. The six runs completed 197–200 World
ticks each and allowed natural Mob spawning.

Six separately instrumented runs counted the entity dispatches. The build with
fewer calls in each pair had higher FPS. The three baseline/candidate call
counts were 413/1,637, 903/272, and 817/1,248, with 199–201 World ticks per run.
In the first pair, Creeper dispatches alone rose from 132 to 1,167. The fixed
Zombie workload shows a lower tick cost with matched inputs. A controlled
gameplay FPS result still needs matched changing Mob work.

### Matching natural Mob work

Fixed constructor clock inputs alone were insufficient in a 17×17 saved
Chunk square. Two runs of the same build agreed on World RNG, block-coordinate
LCG and Entity dispatches through the start of World time 12,430. The first
difference appeared inside the random-block tick at 12,430, before the next
spawning pass. The World tick scans 19×19 Chunks, so the original fixture left
parts of the active area to live loading and generation.

The follow-up uses the [larger Far fixture](../client/world-renderer.md#loaded-chunks-at-far-distance)
and fixed constructor clock inputs in a separate benchmark build. It authors
six layers of dark air at Y=47–52 in Chunks −6 through 6 on both axes. The
ordinary spawning and entity loops run over that saved terrain. Both collision
variants use the same higher Chunk capacity. In all four focused DISPLAY2
runs, the common 38 timed World ticks repeated the same 3,154 dispatches,
including 1,140 Creeper, 760 Skeleton, 304 Spider and 912 Zombie dispatches.
The per-tick hash of ordered entity kinds, positions and actor RNG readings
matched. World RNG, block-coordinate LCG and recorded scheduler work also
matched at each observed stage over the common ticks.

The two baseline runs measured 2.51 and 2.72 FPS. The two candidate runs
measured 3.14 and 3.12 FPS. The timer grouped the matched ticks into different
frames, and the four full timed windows completed 38–44 World ticks. The
three frames with exactly matching tick spans in the same pair had combined
host frame times of 1,745.02 ms before and 1,751.79 ms after. The live scene
reproduces the same Mob work with long frames. It leaves the whole-frame speed
effect of the collision change unresolved.

### Reusing collision scan diagnostics

Two instrumented runs of the 82-Mob cave scene spent a median 249.72–263.61 ms
in the World entity loop. Per-tick travel took 174.55–183.69 ms, including
80.86–85.55 ms in collision-box queries. Environment checks took 27.93–30.12 ms.
These stage timings include work in their called functions.

The collision scan initializes its private nearby diagnostic before the scan.
The column-presence and Block reads restore just the fields they write on
success. Block contributions use the same private frame directly. A failure
still publishes the reached coordinates, stage and cause. The collision-box
control shortens a live Chunk's backing extent to test a failure after a valid
air read and after a solid Block contribution. It checks the failing read's
World, Block and Chunk diagnostics and the caller's output sentinel.

The pinned Windows Release workload repeated the same eight- and 64-Zombie
cases, with 14 matched measurements at each count. Each interval completed 16
World entity ticks. State, actor tick words, RNG, Chunk and ownership digests
matched for each seed.

| Zombies | Baseline median ms per tick | Candidate median ms per tick | Faster pairs |
| ---: | ---: | ---: | ---: |
| 8 | 3.170 | 3.143 | 13 of 14 |
| 64 | 24.513 | 24.077 | 14 of 14 |

Six instrumented Far runs on DISPLAY2 repeated 2,905 collision-box calls and
entity dispatches in the same 35 World ticks, with matching per-tick state
and call counts. One of the three pairs had lower median collision-box and
World entity loop times. Median collision-box time per tick for each pair was
84.74 to 78.37 ms, 83.97 to 86.22 ms, and 72.51 to 77.01 ms, respectively.

A smaller cave in four Chunks (X=3–4, Z=−1–0) produced 2,881 matching
entity dispatches over the 169 ticks common to six further DISPLAY2 runs.
The complete intervals returned 169–177 World ticks. At 854×480, the median
run FPS changed from 9.94 to 10.04 and median run p99 from 767.54 to 765.81 ms.
Two pairs had higher FPS and one had lower FPS. The fixed-work tick savings
are reproducible in this sample. The whole-frame benefit remains unresolved.

### Bounds ownership during collision and clipping

The heavy cave trace attributed most of the collision and clipping cost to
walking the list of native Bounds owners. A class namespace now builds an
address index once its pool reaches 16 entries. It grows at half occupancy
and indexes every live owner, including standalone and detached held Bounds.
The list still governs ownership and retirement. A normal pool reset retains
the index, and client clear drops it before retiring the unheld owners.

The index has a separate 1 MiB peak limit across both tables during growth.
A growth attempt also checks the remaining direct allocation budget. If the
attempt fails, lookups use the owner list until client clear. The pool header
adds 16 bytes on x64. Tests cover client and server namespaces, foreign
addresses, the return-hold failure after publishing a pooled entry, growth
with 20,000 live Bounds, and lookup after an optional allocation refusal.

At 854×480 on DISPLAY2, the heavy and lighter Far scenes both had higher FPS
and lower p99 frame time in every pair. These whole-frame runs used fixed
constructor clock inputs and the ordinary World and Mob loops. On the shared
timed ticks, ordered entity state, actor and World RNG, and the observed
scheduler work matched. The timing windows included different numbers of
World ticks because the timer grouped the work differently. The heavy runs
completed 42–43 before indexing and 200–201 after it. The lighter runs
completed 174–175 before indexing and 200 after it.

| Scene | Run pairs | Common timed ticks | Dispatches per run over common ticks | Median FPS, baseline → indexed | Median p99 frame ms, baseline → indexed |
| --- | ---: | ---: | ---: | ---: | ---: |
| Heavy cave | 3 | 19 | 1,577 | 2.78 → 22.60 | 1,487.69 → 57.23 |
| Lighter cave | 2 | 173 | 2,997 | 11.64 → 57.34 | 724.43 → 31.15 |

Two additional heavy pairs recorded nested stage timings over 24 common
World ticks. Each run repeated 1,991 entity dispatches and collision-box
queries, 10,200 Block contributions, and 5,973 clip calls. The table reports
the median of each variant's per-run tick medians.

| Stage | Baseline ms per World tick | Indexed ms per World tick |
| --- | ---: | ---: |
| World entity loop | 234.314 | 27.466 |
| Entity movement | 156.812 | 4.964 |
| Collision-box queries | 77.122 | 0.409 |
| Block contributions | 52.611 | 0.079 |
| Axis clipping | 54.637 | 0.036 |

At the end of each indexed heavy run, the table held 14,875 live owners in
32,768 slots and occupied 262,160 bytes. The comparison isolates the index
by building both variants with the same pool header layout. The results
apply to these fixed-camera scenes and workloads.

The same pinned Windows Release workload had 14 measured intervals per
variant and Zombie count. The baseline and candidate used the same pool header
layout for the allocation and ownership digests. Those digests, the actor
tick words, simulation state, World RNG, and Chunk digests matched. The median
tick cost went from 3.150 to 3.194 ms with eight Zombies and from 24.043 to
24.040 ms with 64 Zombies.

### Living action diagnostics after Bounds indexing

Two heavy and two lighter Far runs on DISPLAY2 timed the indexed build with
per-frame and per-World-tick counters. The counters record in memory and emit
their results after capture. Both launches in each scene matched ordered actor
state and observed scheduler work over 200 timed World ticks. In the heavy
scene, action diagnostic initialization took 3.16–3.19 ms per World tick.
Lava queries across all callers took 6.54–6.61 ms, including their child
work. Monster brightness queries took 0.37–0.38 ms.

On heavy frames with zero World ticks, median render time was 16.89–16.92 ms.
Dirty Chunk updates took 6.86–7.05 ms, opaque terrain submission 4.05–4.40 ms,
and clouds 2.22–2.23 ms. Median buffer swap was 0.02–0.03 ms. These are CPU
wall-clock spans around the actual calls, including their synchronous work.

Living action and travel now clear the storage outside their largest child
and let that child's initializer clear its own record. Mob travel reuses the
record prepared by action. The grounded-jump control reaches a movement limit
after the jump and checks the reached travel method, action and movement
positions, vertical velocity, and retained failure. Ordinary environment
success initializes the caller's diagnostic directly. A failure still copies
the completed local diagnostic after releasing its borrows.

The pinned Windows Release workload matched actor tick words and published
simulation, RNG, Chunk, and ownership digests for 14 intervals per variant at
each size. Both variants used the portable Bounds index layout.

| Zombies | Baseline median ms per World entity tick | Candidate median ms per World entity tick |
| ---: | ---: | ---: |
| 8 | 3.058 | 2.789 |
| 64 | 23.482 | 20.854 |

In two heavy Far pairs at 854×480 on DISPLAY2, median FPS rose from 24.28
to 26.91, and median p99 frame time fell from 53.28 to 50.79 ms. Each launch
completed 200 World ticks. Ordered actor state and observed World work matched
across 199 common timed ticks and 16,342 entity dispatches per run.

Two additional instrumented pairs matched the same timed work. Travel
initializations fell from 33,082 to 16,939 per run, one fewer per Mob action.
Median World entity loop time fell from 27.19 to 23.96 ms per tick. Median
action time fell from 21.08 to 17.89 ms per tick. These are medians of the
per-run tick medians.

The Bounds index stores its slots after its header in the same allocation,
using the Store index layout. This also compiles with the strict MSVC C17
warning settings used by Windows CI.
