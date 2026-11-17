# Piston rendering

The client uses the original MovingPiston renderer, `HY`, on the real
TileEntity dispatcher. It emits geometry through the same mutable
`RenderBlocks` and Tessellator objects used by the World renderer. The
inspection mesher does not supply this path.

## Draw order

The renderer captures `Block.m`, reads the Tile's carried block ID, and tests
the captured array before interpolating progress. A missing Block returns.
Progress at least one, including NaN through the original comparison, also
returns before texture lookup or graphics state changes.

An entered draw captures the actual Tessellator, binds `/terrain.png` through
the inherited Tile renderer texture method, disables standard lighting, sets
the original blend function, enables blending, disables culling and selects
the shade model from the actual ambient-occlusion setting. Coordinates round
to binary32 before the original coordinate subtraction and movement offsets.
Each offset reads the Tile's defining Direction array and raw facing index;
it does not select a namespace from the Tile's mutable World.

The original branches remain distinct:

- An early moving head renders the shorter rod directly.
- A retracting base with `renderHead` sets the actual shared Head texture
  override, renders the head, resets that override, removes the movement
  offset and renders the shortened base.
- Other carried Blocks enter the ordinary block renderer.

The three void block-rendering wrappers set `RenderBlocks.f`, call their real
generic/base/head child, discard its boolean return, and clear `f` only after
normal return. Base/head geometry includes rotated UV fields and all four
rod quads. Normal completion resets rotations and performs the reached
original bound resets.
The HY tail resets Tessellator translation, draws the batch, then enables
standard lighting. There is no invented finally block.

## Retained failures

The retained draw owns its exact reached CV occurrence, texture attempt,
graphics occurrence and complete typed child diagnostics. Heavy CV/texture
diagnostic storage is allocated only after a nonculled Piston reaches it;
Spawner draws and culled Pistons allocate no such workspace.

Checked retirement releases native ownership. It never clears an unfinished
batch, resets a source Block bound or Head texture, changes progress, restores
GL state, or resumes a failed source body. The private CV release admits only
the exact inactive retained draw. Public CV operations still refuse while a
draw is pending. Consumed receipt views expire explicitly during partial
native retirement; the first source status and reached sites remain.

Moving-block bounds use the actual World or ChunkCache Tile lookup, the
defining Piston Class identity, the carried Block's virtual bounds, progress
and fresh Direction array reads. A failed query retains only an actual
supplied WorldAccess receipt. Native receipt-capture status remains separate
from the first source failure.

## Evidence and limits

The [original direct-draw packet](../../tests/reference/client/piston_draw/README.md)
contains four fresh original processes, 27 cases, 24 active batches and 25
live Code pins per passive run. It covers six directions, moving Stone,
retracting normal/sticky bases, progress boundaries, interpolation and AO.
Observers read actual active Tessellator words; stale suffix storage does not
establish a batch count.

The Windows native comparison uses genuine constructors, saved Worlds and
direct Piston ticks. It compares every active vertex word and declared
retained Block/CV/Tile/Tessellator/GL cell. Physical blend/cull/texture state
and the backend's legacy lighting/shade ledger are identified separately.
A separate native fault control
leaves the actual lighting buffer NULL, reaches the completed geometry draw
and the subsequent lighting NullPointerException, then checks unchanged
source state across native retirement. These are direct-body observations,
not original whole-frame or pixel equality.

The ordinary monitor-2 empty-front SDK scene also exercises returned World/HUD
frames, extension, retraction, save and reload. A later pushed-Stone capture
passes 948 checks through both moving Tiles, settled extension/retraction and
save/reselection after the [identity Set join](../reference/object-identity.md).
The earlier multiple-new-Tile failure remains in its historical receipt.
See [Piston activation](../world/pistons.md). These finite captures do not
establish sustained play, original pixel equality or FPS.

The [sanitized Windows record](../../tests/benchmarks/piston_windows.json)
preserves selected input/product/image hashes, finite observations and failed
attempt history. Its before/after maps describe each captured revision; later
documentation or admission-binding updates do not rewrite those receipts.
