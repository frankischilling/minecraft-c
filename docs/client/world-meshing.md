# World meshing

The client can build CPU terrain meshes from an actual READY GAME World and
draw a fixed inspection scene. It uses real Chunk objects, the shared Registry
and the dimension lighting table. Player simulation, input-driven movement,
block interactions and the gameplay save loop remain separate work.

[`mc_client_world_mesher_build`](../../src/client/world/mesher.h) builds one
aligned 16×16×16 section. It first acquires the padded nine-column rectangle
in the original ChunkCache order: X outer, Z inner, both inclusive. Store-owned
Chunk identities stay leased throughout geometry and climate work. Subsequent
face reads use that cache. Failed acquisition stops the build rather than
publishing missing terrain as air.

The output owns two triangle arrays, one per Block render pass. Vertices hold
section-relative float XYZ, float UV and ARGB. Positions narrow after the double
World-to-section translation. Each original quad uses `[0,1,2,0,2,3]`; this
diagonal also controls AO interpolation. The caller uploads the arrays through
the World renderer and releases them with `mc_client_world_cpu_mesh_free`.
CPU arrays retain no World, Chunk or Registry pointer.

## Block behavior

The Registry rendering interface reads actual subtype state and mutates actual
shared Block bounds. It handles side textures, metadata, opacity, culling,
biome color and the leaves graphics setting. The mesher uses original atlas
insets, six-face order, flat directional shading, default AO corner samples,
grass overlays and dimension float brightness. Fluids use corner heights,
flow direction and still/flowing UVs.

| Render type | Implemented geometry |
|---:|---|
| 0 | Common faces, including state-dependent partial bounds |
| 1 | Crossed plants, tall-grass offsets and sapling variants |
| 4 | Fluids |
| 6 | Crops |
| 8 | Ladders |
| 10 | Stairs |
| 11 | Fences |
| 13 | Cactus |

Types 2, 3, 5, 7, 9, 12, 14, 15, 16 and 17 still require specialized geometry:
torches, fire, redstone, doors, rails, levers, beds, repeaters and both piston
families. Negative render types require the separate TileEntity draw path.
Unsupported geometry returns its Block ID, render type, coordinates and
dependency. The build discards partial vertex arrays. Earlier acquisition,
population and Block-bound effects remain; this is not a World rollback.

Callers supply actual imported grass/foliage maps as borrowed 65,536-entry ARGB
arrays and an initialized Minecraft trig table. Climate queries use the World's
WorldChunkManager. Entity/TileEntity drawing, terrain animation, sky/weather,
full render scheduling and all family/state combinations remain outside the
current comparison coverage.

## Reproduce the original comparisons

The [public harness](../../tests/reference/client/rendering/README.md) accepts
explicit archive and library paths:

```powershell
$clientJar = 'C:\minecraft-reference\b1.7.3.jar'
$libraries = 'C:\minecraft-reference\libraries'
python tests/reference/client/rendering/prepare_libraries.py --output-dir "$libraries"
python tests/reference/client/rendering/compare.py --client-jar "$clientJar" --library-dir "$libraries"
```

The client archive SHA-256 must be
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Dependency JARs, class bytes, method Code bytes, authored probes and normalized
original records also have pinned hashes. Standalone mode freezes current
native sources and headers, then compiles strict-FP GCC C17 probes. Prebuilt
mode accepts the configured build's three native executables and needs no GCC.
Its caller supplies build provenance. Both modes capture commands, tool versions,
executable identities, source snapshots and exact comparison results.

The Windows x64 rerun on Temurin 17.0.17 and GCC 15.2 matches every compared bit:
108 face vertices across unit, inset and extended bounds; 108 AO colors across
blocked, transparent and mixed material flags; and 534 family vertices with 16
counts. Family scenes cover still/flowing/falling water, a water layer, plants,
crops, cactus, two stair orientations, a fence and all ladder attachments.
[`validation.json`](../../tests/reference/client/rendering/validation.json)
records the frozen source and observation hashes.

The AO probe calls the original smooth-light body directly with nonuniform
light. Family fixtures select flat shading and uniform brightness. These
IBlockAccess observations do not cover the GameSettings-to-AO join, real World
acquisition/population order, matrices, driver pixels, bootstrap history or
general failure equivalence. Class/Code pins identify the input binary; they
do not extend case coverage. The separate `client.world_mesher` test passes
493 authored checks for culling, winding, UVs, coordinate narrowing, AO corners,
live bounds, textures and color-map lookup.

## Fixed World inspection

The frontend accepts `--inspect-world FRESH_OR_COPIED_DIRECTORY --world-seed N`
alongside its imported assets, finite frame count and capture options. It uses
the public session owner to construct a real client GAME World, then draws
sections through the CPU mesher and OpenGL renderer. The selected directory is
opened for read/write and receives the session lock; supply a fresh directory
or an owned World copy. Terminal session retirement is separate from saving.

The inspection displays generated terrain, trees, snow and water through a
fixed diagnostic camera. It establishes a graphical World path. A playable
Player loop and original/native framebuffer equality remain unverified.
