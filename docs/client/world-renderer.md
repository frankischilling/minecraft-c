# Native World rendering

The client can construct an actual client GAME World, generate/populate its
chunks, mesh a fixed area and draw it through OpenGL 3.3 core. This is a diagnostic
scene. It has no Player, world tick, targeting, mining, placement or inventory.
In normal startup, choose Singleplayer to open World selection.
The `--inspect-world` option opens the separate fixed-camera diagnostic scene.

After [asset import](../reference/resources.md), run:

```sh
./build/minecraft-client --assets /absolute/imported-assets \
  --game-directory /absolute/fresh-game-directory \
  --inspect-world /absolute/fresh-world-directory --world-seed 23
```

Use `build/Debug/minecraft-client.exe` with a Visual Studio build. Escape or window
close exits inspection. The camera is fixed. `--frames 2 --hidden --capture world.png`
captures a finite run without a visible window. The imported terrain atlas and
grass/foliage maps remain local resources; no Java or original classes run.

The selected World path is read/write: opening writes `session.lock`, and normal
chunk reads may create region headers. Tests must use a fresh path or an owned
world copy. Inspection does not save chunks or metadata implicitly. It must not
be used as an editor for a personal world.

## Ownership and construction

The application owns one Random profile, material/Registry classes, Entity
classes, World classes and initialized trigonometric table. The session borrows
those classes and owns its region cache, store, counted UTF-16 name and actual
World. It initializes Blocks/Biomes, derives the store block map from registered
IDs, samples the native session clock and executes `CLIENT_LOAD_OR_CREATE`.
Constructor failures retain their real prefix; the same owner cannot retry.

Inspection first acquires the full padded ChunkCache footprint around the actual
spawn and [drains World lighting](../world/light-propagation.md). It then builds
all eight vertical sections in each of nine columns. Each [CPU mesh](world-meshing.md) retains independent two-pass
arrays after its source ChunkCache leases end. Unknown shape families stop with
the block, render type and coordinates. The renderer copies the supplied
vertices into owned GPU buffers; the CPU arrays are then freed. The completed
view retains no World or Chunk pointer.

Teardown releases GPU geometry before Display. The session terminally retires
typed Entity/TileEntity containers, cache/pending references, World contexts,
store chunks and the World. A busy owner remains available for retirement retry;
it cannot resume gameplay. No save, source unload or arbitrary count reset is
substituted. World classes retire before Registry; Registry retires before Entity
classes. Their shared profile and material resources outlive them.

## Loaded Chunks at Far distance

Far creates a grid 26 Chunk columns wide. The renderer's ChunkCache reads
neighboring Blocks across section edges, so one stationary grid can need up
to 28×28 columns. The graphical client now gives both its Provider and the
SaveHandler room for 1,024 loaded or reserved Chunks. The Provider and Store
still enforce their respective bounds. Other Store callers use their configured
limit or the 512-Chunk default.

A native Windows test loaded a prepopulated save spanning Chunk coordinates
−18 through 18 on each axis. At 854×480 on DISPLAY2 with Far selected, three
of four earlier runs stopped at 512 live cache entries during a renderer
ChunkCache get. The three stopped runs completed 191–193 World ticks of the
timed interval. With the higher bound, all four runs completed 200 World ticks
and closed normally, with 517–518 cache entries at the end. Two runs used the
ordinary Random constructor clock and two used fixed constructor inputs in
the benchmark. The two ordinary runs measured 53.38 and 54.56 FPS. The
comparison exercises a stationary saved world over a ten-second interval.
The Provider reported 230,424 bytes of direct storage with the higher bound,
up from 211,992 bytes. Chunk payloads still allocate on demand.

The storage control checks both the default count limit and a configured limit
above it. It holds every reservation until the next request fails, checks that
the failure preserves the output and held count, then cancels every reservation.
The Windows World-selection control checks the Provider limit after opening
the save through the graphical client's normal load path.

## Dirty Chunk compilation at Far distance

Two heavy cave runs on DISPLAY2 at 854×480 spent a median 6.97–6.99 ms
in dirty Chunk updates on frames with zero World ticks. Mesh compilation
accounted for 6.77–6.80 ms. Dirty queue classification and compaction
together took about 0.22–0.24 ms. The trace samples individual block and
light query timings inside the compile call.

The ChunkCache opacity and normal-cube queries now read directly from the
captured Block array when the class is already initialized and idle. They
capture that array before reading the Block ID. Cold classes still run the
reached initializer, and failed reads publish the Registry diagnostic. Light
queries only attach a Registry record when that child fails. Chunk compilation
binds the fields of its fresh, zeroed RenderBlocks context in place.

The pinned Windows Release getter benchmark matched each result and its
World, Chunk, Random, and ownership receipts. Each variant ran 28 samples
of 65,536 calls per operation against nine leased Chunks.

| Query | Baseline median ns per call | Candidate median ns per call |
| --- | ---: | ---: |
| Brightness | 60.05 | 58.43 |
| Minimum brightness | 60.59 | 58.35 |
| Opaque | 26.90 | 23.02 |
| Normal cube | 31.21 | 25.24 |

Material queries now read a ready, idle Block registry directly after
reading a non-air Block ID. The ordinary lookup handles initialization and
read failures. In six alternating Windows Release pairs, the material
query median fell from 39.19 to 17.96 ns per call across 42 samples per
build. Query results and the World, Chunk, Random, and ownership receipts
matched. These timings cover the getter loop.

Brightness queries retain the Dimension and capture its live light table
together. Packed light reads attach child diagnostics on failure, and the
usual in-range skylight subtraction uses native arithmetic. The wrapped
result is preserved at the signed limit. Eight alternating Windows Release
pairs measured brightness at 59.45 to 53.04 ns per call and minimum
brightness at 59.70 to 53.17 ns per call. The same nine-Chunk fixture
matched query results, ownership receipts, and skylight side effects.

Section compilation now scans consecutive air cells along each X row in
one ChunkCache call. It reads the same cells in order and returns at the
next non-air block or failed read. Eight alternating Windows Release
pairs compiled an authored 16×16×16 section repeatedly. An empty
section fell from 49.7 to 26.5 µs at the median. A section with 64 blocks
fell from 262 to 239 µs. Each populated section produced matching
vertex bytes and display list counts. The measurement covers compilation
and retirement of the compile call.

Section compilation also avoids preparing an EntityRenderer diagnostic
when the caller provides no output. An initialized application reads the
live ambient-occlusion setting under one lock. Eight alternating Windows
Release pairs with 512 authored blocks measured 1.106 to 1.010 ms for flat
shading and 2.167 to 2.047 ms with ambient occlusion. Display list counts
and vertex bytes matched in both modes.

Quad conversion checks the span for both duplicate vertices before
copying their position and enabled texture or color words. The checked
path still preserves the partial writes of a failed copy. Eight
alternating Windows Release pairs with 512 authored blocks measured
0.997 to 0.944 ms for flat shading and 2.088 to 1.987 ms with ambient
occlusion. Display list counts and vertex bytes matched.

The tessellator now runs context release only when a binding exists
and its resource counts are zero. In eight alternating Windows
Release pairs, 512-block rebuilds fell from 0.939 to 0.872 ms with flat
shading and from 1.979 to 1.909 ms with ambient occlusion. The scene
retained its context through each rebuild and released it at retirement.
Display list counts and vertex bytes matched.

Separate instrumented Far runs matched 411 mesh groups by section position
and counts of cell reads, rendered Blocks, minimum-light queries, opacity
queries, and ambient occlusion flags. Each group had at least two samples per
variant. The median of the group medians fell from 6.69 to 6.51 ms, and 373
groups compiled faster.

Two uninstrumented pairs per scene matched actor state and observed World
work on shared ticks. The heavy cave went from 26.47 to 26.74 median FPS and
52.35 to 51.40 ms median p99 frame time. The lighter cave went from 57.46 to
56.90 median FPS and 30.10 to 29.61 ms median p99. Whole-frame benefit remains
unresolved across these scenes.

### Texture-coordinate conversion

The Tessellator converts both texture coordinates together once the
vertex span and floating-point settings pass their checks. The scalar
path retains partial writes when an element access fails.

In ten paired CPU runs, the median textured write took 5.11 ns with
Clang 18 on WSL2, down from 5.45 ns. On native Windows with MSVC Release
it took 4.71 ns, down from 5.10 ns. On each compiler, 200,000 control
cases matched every written word, rejected-write diagnostics and MXCSR.

Six paired runs of the authored textured RTX 3060 scene rebuilt 384
sections per frame. Framebuffer hashes and submission order matched.

| Quads per list by pass | Recording p50 | Frame p50 | Frame p95 |
| --- | ---: | ---: | ---: |
| 16/3 | 2.847 → 2.835 ms | 4.878 → 4.881 ms | 6.371 → 5.849 ms |
| 64/48 | 14.456 → 14.440 ms | 19.317 → 19.380 ms | 22.889 → 22.741 ms |

## GPU contract

### Terrain selection

Terrain submission inlines the renderer array and receiver checks and
source program-counter updates. Statistics wrap from `INT32_MAX` to
`INT32_MIN`. The CPU controls check those wraps and the counters left
behind when a renderer from another owner interrupts selection.

Eight alternating Windows MSVC Release pairs ran the production selection
loop over authored section arrays. Every run matched the visit counters,
ordered selections and final out-of-range diagnostic.

| Sections | Workload | Before (ns/section) | After (ns/section) |
| --- | --- | ---: | ---: |
| 200 | Mixed | 10.76 | 7.21 |
| 5,408 | All empty | 9.65 | 5.44 |
| 5,408 | Mixed, 338 selected | 11.05 | 7.66 |

The benchmark stops after selection, before batching and draw submission.

Six alternating Windows Release pairs then ran `mc_benchmark_terrain_frames`
on an RTX 3060. Both profiles visited 8,712 section slots and submitted
450 lists per pass. Each run measured 160 frames through GPU completion.
Framebuffer, section order and work hashes matched in every pair.

| Profile | Median frame, before → after | p95, before → after |
| --- | ---: | ---: |
| Stationary, 16/3 quads per list | 0.642 → 0.571 ms | 0.750 → 0.669 ms |
| Alternating camera height, 3/3 quads per list | 1.046 → 0.922 ms | 1.572 → 1.080 ms |

The medians use the per-run percentiles. The alternating-height profile
also exercises section recentering on this authored scene.

### Display-list draw cost

Two heavy Far cave traces at 854×480 on DISPLAY2 averaged about 360
unlit draws and six lit draws per frame. The unlit draw bridge spent
3.33–3.35 ms per frame binding and uploading vertex storage, 1.22–1.24 ms
converting vertices, and about 0.065 ms uploading lighting uniforms. These
are CPU wall-clock spans, including time spent waiting inside driver calls.

The native draw bridge converts the four packed color channels together
with SSE2. It uses the same profile-specific division or reciprocal
multiplication as the original scalar path. Unlit draws upload the lighting
enable and flat-shading settings. Lit draws still upload their current
normal, light, and material settings. The hidden GL control checks all 256
byte values against each scalar formula, unlit flat list pixels between two
lit draws, and a material change made while lighting is off.

Two further heavy pairs timed both builds. Mean unlit conversion fell from
1.25 to 0.82 ms per frame. Unlit lighting uniform calls took 0.007–0.008 ms
per frame, down from 0.065 ms. The builds submitted about 360–364 unlit
draws and six lit draws per frame.

Separate runs without stage timing measured both cave scenes at Far on
DISPLAY2. Both FPS pairs improved per scene. The heavy runs matched 16,342
entity dispatches across 199 common World ticks. The lighter runs matched
3,780 dispatches across 200 ticks. Ordered actor state and observed scheduler
work matched in each scene.

| Scene | Median FPS, baseline → candidate | Median p99 frame ms, baseline → candidate |
| --- | ---: | ---: |
| Heavy cave | 26.73 → 27.57 | 52.48 → 51.69 |
| Lighter cave | 54.27 → 56.06 | 29.45 → 27.82 |

An experimental build also replaced vertex buffer storage on every nonempty
draw. It measured 26.94 median FPS in the heavy scene and 56.27 in the
lighter scene, with median p99 of 50.99 and 30.04 ms. The results did not show
a consistent benefit across the two scenes.

Three subsequent 10-second DISPLAY2 traces timed individual GL calls on the
same build. In the heavy cave, the upload span averaged 3.56–3.60 ms per
frame. The data transfer call accounted for 0.48–0.49 ms, and the following
GL error check accounted for 3.02–3.07 ms. The active texture query took
1.14–1.19 ms, and the post-draw error check took 1.23–1.24 ms. In the
lighter cave, the transfer call took 1.17 ms and its error check took
0.15 ms. These CPU wall-clock timings include driver waits, which can move
between calls as the GPU queue fills.

An offline replay counted reuse by recorded command lifetime, native
context generation, array state, attribute profile, and the exact current
color and normal when their arrays were disabled. For unlit recorded draws
of at least 96 vertices, a 16 MiB least-recently-used model reused 96.4% of
all recorded upload bytes in both heavy runs and 79.8% in the lighter run.
The 32 MiB model reused 96.4% and 97.4%, respectively. The model counted
bytes from cold start within each measured interval and removed entries on
command retirement.

The initial Display cache retained converted vertices for eligible unlit
recorded draws of at least 96 vertices and capped retained GL buffer
storage at 32 MiB. Retiring or replacing a recorded
command invalidates its entry. Replay still performs native texture and
floating-point admission, applies current uniforms, issues each draw in
order, and publishes the final vertex attributes according to the driver
profile. The native tests read back the real buffer and attribute layout
across reuse, ambient attribute changes, intervening direct draws, and
list replacement and retirement. A separate control introduces a real
binding error on a cache hit and verifies that replay stops before drawing.

Three paired runs per scene compared the cache with the preceding build
at 854×480 on DISPLAY2. Each run captured 10 seconds of fixed-pose Far
gameplay. Ordered actor state and scheduler work matched over 198 common
World ticks and 16,259 dispatches in the heavy cave, and 199 ticks and
3,779 dispatches in the lighter cave.

| Scene | Median FPS, baseline → cache | Median p99 frame ms, baseline → cache |
| --- | ---: | ---: |
| Heavy cave | 26.78 → 32.48 | 51.58 → 46.69 |
| Lighter cave | 57.49 → 55.26 | 31.38 → 29.95 |

The heavy cave gained about 21% FPS. The lighter cave lost about 4% FPS.
The p99 frame time improved in every pair in both scenes.

Later paired traces measured the cache saving CPU time in both scenes.
Mean unlit draw time fell from 7.56 to 4.18 ms per frame in the heavy cave
and from 3.80–4.08 to 2.33–2.58 ms in the lighter cave. Native binds for
cached vertices took 0.06–0.07 ms per frame in the lighter cave. These spans
include driver waits. The instrumented lighter trials also spent more time
in the native state queries and the GL check after drawing with the cache.

Two fresh uninstrumented pairs per scene put the current cache ahead of
streaming at 32.54 versus 26.71 median FPS in the heavy cave and at 59.46
versus 57.41 in the lighter cave. The lighter result varied between pairs
and reversed the earlier median. A shared-buffer prototype placed behind
the current cache in both scenes at 32.13 and 57.10 median FPS.

Within one list invocation, replay keeps the current source program and
the last bound vertex objects. Unlit draws reuse unchanged non-matrix
uniforms after verifying the current texture clamps. Every draw still
uploads its current matrices and goes through native texture, error, and
floating-point checks. The first draw of the next invocation sets its own
bindings and uniforms. Native controls forward and count the actual GL
calls, check both matrices and color pixels, and verify binding and uniform
restoration after another native operation between invocations.

Two final DISPLAY2 rounds per scene compared the preceding cache build,
list replay reuse, and the combination with the
[stored-water tick change](../entity/environment.md#stored-water-queries-during-a-world-tick).
Ordered actor and scheduler receipts matched across 199 shared World ticks
per scene, with 16,342 heavy-cave dispatches and 3,779 lighter-cave dispatches.

| Scene | Median FPS, prior → replay → combined | Median p99 frame ms, prior → replay → combined |
| --- | ---: | ---: |
| Heavy cave | 32.73 → 34.02 → 35.38 | 48.26 → 47.38 → 46.04 |
| Lighter cave | 59.15 → 59.16 → 59.41 | 28.15 → 29.28 → 28.50 |

The combined build improved FPS and p99 in both heavy-cave rounds.
The lighter scene varied between rounds and had nearly the same median FPS.

### Draw material snapshots

An unlit list draw snapshots its current color and normal. Direct draws,
lit draws, and replay with pending Mesa material changes use the full
snapshot and publish it at the existing draw boundary.

Ten alternating Windows MSVC Release pairs per profile ran the authored
stationary terrain benchmark on an RTX 3060. Both profiles submitted
450 lists per pass. The framebuffer, section order and work hashes
matched. Measurements span frames through GPU completion.

| Quads per list, pass 0/1 | Median, before → after | p95, before → after |
| --- | ---: | ---: |
| 16/3 | 0.574 → 0.541 ms | 0.675 → 0.680 ms |
| 3/3 | 0.567 → 0.534 ms | 0.684 → 0.634 ms |

### Lit display-list uniforms

A list replay reuses the texture, alpha and fog uniforms while its sampled
texture clamps agree. Every later lit draw with the same clamp values skips
12 uniform calls and still uploads its normal matrix and lighting material.
Each new replay starts with a fresh upload.

The startup device test submits two lit lists twice. It checks the upload
count, the normal matrices after distinct modelview scales, and the
resulting pixels.

The Windows MSVC Release benchmark used an RTX 3060 and two direct
replays of 450 textured lists per frame on an authored scene. Each profile
ran eight alternating pairs with 40 warmup frames and 160 measured frames
per run. Times include `glFinish` and are medians of the run percentiles.
Framebuffer hashes, covered pixels and list order matched in every pair.

| Quads per list by pass | Frame p50 | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: |
| 3/3 | 9.21 → 9.11 ms | 11.85 → 14.82 ms | 15.26 → 15.43 ms |
| 16/3 | 19.65 → 19.37 ms | 29.88 → 30.50 ms | 38.05 → 36.32 ms |

### Display-list recording and cache lookup

Recording initializes the command header and copies its vertex payload
directly into the allocated storage. A recorded draw remembers a cache
slot after confirming the payload identity. Replay checks the payload,
generation and attributes before reusing its buffer. A stale hint falls
back to the normal lookup and replacement policy. The hint fits in the
existing 56-byte command.

Ten WSL2 Clang 18 `-O3` pairs ran two million lookups per profile with
simulated uploads. Selection order, hit counts and final cache-state
hashes matched in every pair.

| CPU workload | Median before | Median after |
| --- | ---: | ---: |
| Resident replay | 18.87 ns | 13.20 ns |
| Changing current normal | 14.54 ns | 11.44 ns |
| Collision churn | 26.80 ns | 27.25 ns |

Eight paired Windows MSVC Release runs per profile used the RTX 3060 and
900 draws per frame on an authored scene. Each run measured 160 frames
through GPU completion. The rebuilt profiles replace two lists for each
of 384 visible sections per frame. Framebuffer hashes, section order and
submitted work matched in every pair.

| Profile | Frame p50 | Frame p95 |
| --- | ---: | ---: |
| Warm, 16/3 quads per list | 0.549 → 0.541 ms | 0.659 → 0.649 ms |
| Rebuilt, 3/3 quads per list | 2.782 → 2.782 ms | 3.519 → 3.320 ms |
| Rebuilt, 16/3 quads per list | 4.726 → 4.747 ms | 5.795 → 5.555 ms |

In a separate recording-only comparison, the 64/48 profile's median
recording phase fell from 14.33 to 14.13 ms. Its median whole frame
moved from 19.06 to 19.04 ms.

### Cache slot publication and retirement

A successful vertex upload records its slot on the draw command.
Replay and retirement verify the payload in that slot. If a later draw
has reclaimed the slot, replay chooses a replacement in the hashed set
and retirement leaves the new resident intact. The device test checks
slot publication on the first upload and invalidation on retirement.
The pressure test checks failed uploads and retries.

Ten WSL2 Clang 18 `-O3` pairs ran two million lookups per profile with
simulated uploads. Each pair matched the slot choices, hit and miss
counts, and final cache-state hashes.

| CPU workload | Median before | Median after |
| --- | ---: | ---: |
| Resident replay | 9.52 ns | 9.19 ns |
| Changing current normal | 12.60 ns | 12.57 ns |
| Collision churn | 30.75 ns | 31.63 ns |
| Retire and rebuild | 26.53 ns | 24.48 ns |

Eight paired Windows MSVC Release runs per profile used an authored
textured scene with 900 draws per frame on the RTX 3060. Each run measured
160 frames through GPU completion. List rebuilding occurred before
timing each frame. Framebuffer hashes and submission order matched.

| Profile | Frame p50 | Frame p95 |
| --- | ---: | ---: |
| Warm, 16/3 quads per list | 0.587 → 0.579 ms | 0.693 → 0.700 ms |
| 384 sections rebuilt, 3/3 | 1.642 → 1.631 ms | 2.278 → 2.251 ms |
| 384 sections rebuilt, 16/3 | 2.044 → 2.056 ms | 2.924 → 2.864 ms |
| 128 sections rebuilt, 128/4 | 2.366 → 2.397 ms | 3.377 → 3.133 ms |

### Vertex cache evictions

The cache admits unlit recorded batches of at least six vertices, enough
for one expanded quad. Batches of 6–17 vertices can reuse an unused slot
or another entry of that size in the same four-way set. They use the spare
bytes in the 64 MiB budget. Batches of 18–95 vertices can also reclaim
storage from 6–17 vertex batches. Live entries of 96 vertices or more
retain priority in the set.

The 4,096-slot cache retains an eviction candidate in each group of 64.
Under pressure it reclaims 6–17 vertex storage first, then compares age
and slot index within each tier. A slot being resized is excluded.
Hits, uploads, and invalidation mark affected groups for rescan. A binary
tree combines the group candidates. Eviction refreshes the dirty groups
and their ancestors. When the root candidate is excluded, only its group
is rescanned and compared with the remaining subtrees. Groups holding
at most 32 buffers visit occupied slots in ascending order. Denser
groups use sequential reads of all 64 slots. The index and occupancy
records occupy 1,608 bytes per cache on x86-64.

Ten alternating Windows Release pairs per profile used 450 lists per pass
on an RTX 3060. Framebuffer, section order, and work hashes matched.
The times span frames through GPU completion.

| Quads per list, pass 0/1 | Median, before → after | p95, before → after |
| --- | ---: | ---: |
| 1/2 | 3.637 → 0.522 ms | 4.299 → 0.628 ms |
| 16/1 | 1.966 → 0.539 ms | 2.482 → 0.631 ms |
| 16/3 | 0.534 → 0.543 ms | 0.646 → 0.649 ms |

Twelve paired WSL2 runs used GCC 15.2 Release and stubbed GL calls to
measure cache bookkeeping near the 64 MiB limit. The regular workload
performed 98,304 uploads and 98,392 evictions. The mixed workload
performed 103,156 uploads and 104,265 evictions. Selection, upload,
fallback and deletion counts, deletion order, and final cache hashes
matched across the builds being compared.

| Workload | Group scan median | Tree median |
| --- | ---: | ---: |
| Regular batches | 41.20 ms | 34.11 ms |
| Regular and 6–12 vertex batches | 39.61 ms | 33.66 ms |

The earlier cache with an 18-vertex minimum took 37.51 ms for the same
regular workload. These measurements cover the CPU cache path.

Twelve further GCC Release pairs on WSL2 compared the tree with and
without occupancy tracking. The workloads used the same order of
requests and the same synthetic buffer charges, with stubbed GL calls.
Hit, upload, fallback, deletion and driver-check counts matched, as did
ordered deletions and final cache hashes.

| Workload | Before | After |
| --- | ---: | ---: |
| Regular pressure, one buffer per set at start | 37.10 ms | 18.63 ms |
| Mixed pressure, two buffers per set at start | 32.25 ms | 18.69 ms |
| Dense pressure, all 4,096 slots occupied at start | 32.52 ms | 32.35 ms |
| 4,194,304 hits | 36.84 ms | 36.88 ms |

An earlier fixed CPU workload measured the introduction of grouped eviction
with stubbed GL calls. All 12 WSL2 pairs matched hit and miss counts, eviction
counts, driver check counts, and ordered deletion and final-state checksums.
[Raw pairs and build identities](../../tests/benchmarks/vertex_cache_eviction_linux.json)

| Workload | Baseline median | Candidate median | Change |
| --- | ---: | ---: | ---: |
| 4,194,304 hits | 9.164 ms | 10.805 ms | +0.39 ns per hit |
| 31,744 evictions | 177.646 ms | 10.945 ms | -93.8% |
| 525,176 hits and 31,937 evictions | 186.368 ms | 38.824 ms | -79.2% |

Each process starts with 1,024 entries at the 64 MiB limit. The mixed case
touches recent entries between uploads and varies the upload size. These
timings cover cache bookkeeping with no GPU or game scene. The
`client.source_vertex_cache_cpu` test checks tie order, exclusion, timestamp
wrap, invalidation, refill under pressure, and retry after an injected driver
error. `client.source_lists_device` covers real OpenGL draws and readback.

For a whole render submission measurement on Windows, enable
`MC_BUILD_BENCHMARKS` and build `mc_benchmark_terrain_frames`. Run
`mc_benchmark_terrain_frames 33 0 7 16 3` for a stationary 33×33×8 section
grid with 450 visible sections and 16 then 3 quads per draw in the two
passes. Change `0` to `1` to move the camera vertically. The benchmark
reports milliseconds through GPU completion and hashes the framebuffer,
section order, and submitted work. It uses generated geometry.

#### Age updates under eviction pressure

A touch marks a clean group for rescan only if it changes the group's
current victim or moves a timestamp backward. The wrap control moves a
non-victim from a larger timestamp to zero and checks the new victim.

Eight WSL2 Clang 18 `-O3` pairs exercised 1.6 million touches per profile.
The mixed profiles selected and refreshed a simulated victim every 16
touches. Ordered victim choices and final timestamps matched in every pair.

| CPU workload | Median before | Median after |
| --- | ---: | ---: |
| Warm touches | 2.55 ns/step | 2.66 ns/step |
| Mixed touches and victim selection | 119.42 ns/step | 15.00 ns/step |
| Mixed sequence crossing timestamp wrap | 118.94 ns/step | 15.78 ns/step |

### Rendering and context ownership

Vertices contain section-local float XYZ, UV and ARGB. The upload converts color
to normalized RGBA bytes, checks finite geometry and enforces mesh/vertex/byte
limits. Display owns the nearest-sampled atlas texture. The terrain pipeline
uses back-face culling, depth `LEQUAL`, optional alpha rejection at `alpha<=.1`,
source-alpha blending and an explicit depth-write flag.

Projection preserves the pinned LWJGL GLU float operation order. Mesh origins
preserve the original 1024-block high/clip split, float camera subtraction and
`1.000001f` section dilation. The [first-person component](first-person.md) adds
source-ordered FOV, hurt/death, bobbing, sleep/debug and anaglyph operations over
actual Player and App owners. Normal frontend adoption, positive portal and
third-person paths remain unfinished. Linear and exponential fog have explicit shader inputs;
the fixed diagnostic uses a constant clear color without fog. This does not
implement the original sky, weather or complete EntityRenderer.

Display tracks scene and GUI frames separately. Scene begin establishes an
actual context, viewport and color/depth clear before capture is admitted. GUI
commands still require their own GUI begin. Context loss, invalid owners,
budgets and resource retirement retain the Display error policy.

## Verification

`client.world_renderer` uses a real hidden GL context with authored textures and
triangles. Its 87 checks cover depth order, culling, alpha threshold, blend
results, nearest sampling, fog endpoints, large-coordinate float behavior,
GUI-after-scene state, finite inputs, budgets and retirement. Enable it with
`MC_ENABLE_CLIENT_DISPLAY_TESTS=ON`; it needs neither original assets nor Java.

`client.world_session` uses authored language/statistic resources and fresh
directories. Its 34 checks cover real construction/acquisition, seed/live state,
single-attempt admission, held-binding cleanup retry and failed constructor
retirement. Native Windows and sanitized Clang Linux/Mesa seed23 inspections
also exit successfully after two frames and capture 854-by-480 generated scenes.
The NVIDIA/MSVC and Mesa/Clang captures differ in 25,964 of 1,639,680 RGBA bytes.
That observed cross-driver difference remains unqualified; no equality claim
or widened tolerance is applied.

`client.world_inspection` runs the actual executable three times with authored
title/terrain/color-map resources and fresh Worlds. It checks same-seed native
pixel repeatability, a changed anaglyph result, unchanged resource/options
inputs and only the expected session-token/empty-region side effects. Its two
pixel relations compare native runs on one driver; they are not original images.
Each inspection has a 60-second observer deadline; the enclosing test has 210
seconds for three captures. Real generation, population, propagation and meshing
take 25.3–25.5 seconds per strict Debug capture in the measured loaded Windows
profile. The title-only startup test retains its separate 20-second limit.

These checks establish the specified native contracts. Original mesh helper
comparisons have their [separate scope](world-meshing.md). Matched whole-frame
pixel comparison, broader lighting acceptance, dynamic section scheduling,
chunk streaming, sky/clouds/weather, entities and remaining block families are
unfinished. A generated scene does not establish client playability.
