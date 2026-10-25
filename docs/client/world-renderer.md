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

## GPU contract

Vertices contain section-local float XYZ, UV and ARGB. The upload converts color
to normalized RGBA bytes, checks finite geometry and enforces mesh/vertex/byte
limits. Display owns the nearest-sampled atlas texture. The terrain pipeline
uses back-face culling, depth `LEQUAL`, optional alpha rejection at `alpha<=.1`,
source-alpha blending and an explicit depth-write flag.

Projection preserves the pinned LWJGL GLU float operation order. Mesh origins
preserve the original 1024-block high/clip split, float camera subtraction and
`1.000001f` section dilation. The camera interface provides ordinary pitch/yaw
and first-person translation. Hurt, bobbing, portal, third-person and debug-camera
paths are unfinished. Linear and exponential fog have explicit shader inputs;
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
