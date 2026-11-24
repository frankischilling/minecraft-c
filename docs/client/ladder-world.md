# Ladder World rendering

RenderBlocks now dispatches type8 to the original `cv.h(Block,III)` path.
It captures Tessellator, reads the actual side0 texture before applying an
override, evaluates inherited Block brightness, sets gray color and reads the
World metadata. The path uses the existing packed Tessellator and section-list
pipeline without a separate geometry allocation.

Metadata2 draws a plane at `float(z+1)-.05f`, facing north; metadata3 draws at
`float(z)+.05f`, facing south. Metadata4 draws at `float(x+1)-.05f`, facing west;
metadata5 draws at `float(x)+.05f`, facing east. Vertex winding, UV assignment,
the `.05f` offset and each integer-add/float conversion follow the original
bytecode. The other twelve nibble values still read texture/brightness/metadata,
set the Tessellator color and return true, without emitting vertices.

The geometry retains the Block bounds and all RenderBlocks scratch fields.
Ladder's concrete `dp` class inherits the empty `uu.a(xp,III)` bound-update
method reached by the enclosing RenderBlocks dispatcher. Ray/collision bounds
belong to separate methods and are not derived from this drawing plane.

The [original packet](../../tests/reference/client/ladder_world/README.md)
records 35 calls across three fresh original VMs. Its 1,133 read-only JDI events
cover the actual texture/brightness/metadata order, 160 EW queries, packed
vertices and three null prefixes. Twenty visible calls append 960 words;
invalid metadata and null calls preserve their distinct effects. The packet
also covers translated section and loaded-World coordinates.

The native replay compares every selected input and complete outer observation
row. It separately checks own-prior inactive backing, retained buffer identities,
four type8 dispatcher calls, strict floating-point admission and cleanup.
The Windows MSVC Release run passes 1,820 checks and all 70 selected rows in
0.74 seconds. Its separate inactive-word audit checks 73,399,360 words; all
sixteen packet corruption controls pass.

## Compiled sections and loaded World

`client.chunk_compile` projects the four original section profiles into the
real compiler. Each orientation publishes six vertices, matches every active
position/UV/color word, retains its own inactive backing words and reuses the
immutable list. A WorldAccess light notification marks the section dirty and
the ordinary update path recompiles it. The same control retains its eight
original fluid profiles and the unsupported Fire failure prefix.

The Windows `client.world_selection_ladder` scene independently decodes a saved
metadata2 Ladder at (3,65,3), a Stone backing at (3,65,4), Sand below and a Stone
standing platform. The saved Player has feet at y65, yaw0 and pitch15. Ordinary
NBT loading supplies that same Player/camera; the test does not reposition it
after loading. Actual picking selects the Ladder and returns its outline.

The opaque section contains 102 vertices: 30 for each ground-supported cube,
six for the Ladder and 36 for its backing. All six Ladder position/UV words
match the original loaded-coordinate profile. The qualified capture also
matches all six colors; raw colors and match counts remain recorded because
lighting runs normally. Four returned World/HUD frames include two reuse
attempts and one light-notification recompile. The immutable definition is
reused, then replaced while its list ID remains stable. Ray/picking bounds
remain unchanged through drawing. Native cleanup completes 1,454 checks in
8.35 seconds. Protected save/resource bytes stay unchanged; only the actual
session lock is created. The capture visibly shows the mounted Ladder, Sand,
Stone, selection and HUD.

These finite rendering checks do not establish original whole-frame pixels,
extended gameplay, all Ladder interactions or an FPS improvement.
