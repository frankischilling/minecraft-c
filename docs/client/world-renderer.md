# Native World rendering

The client can construct an actual client GAME World, generate/populate its
chunks, mesh a fixed area and draw it through OpenGL 3.3 core. This is a diagnostic
scene. It has no Player, world tick, targeting, mining, placement or inventory.
The Singleplayer button still requires its world-selection and gameplay owners.

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

## GPU contract

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
