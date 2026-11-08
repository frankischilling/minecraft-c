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
