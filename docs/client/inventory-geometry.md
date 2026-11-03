# Block Item geometry

The inventory renderer implements the five render types admitted by the
original 3D Item predicate: cubes (0), Cactus (13), Stairs (10), Fence (11)
and Piston bases (16). Held, dropped and GUI Items use the same production
CV body. This does not implement the separate World mesh paths for every
specialized Block.

| Item family | Source parts | Returned face batches | Retained bounds |
| --- | ---: | ---: | --- |
| Cube | 1 | 6 | Its actual inventory-bound hook result |
| Cactus | 1 | 6 | Existing bounds, including authored non-cube bounds |
| Stairs | 2 | 12 | The second part: `(0,0,.5,1,.5,1)` |
| Fence | 4 | 24 | Unit bounds, set only after all parts return |
| Piston base | 1 | 6 | Unit bounds; inventory metadata becomes 1 |

Every face retains its own actual NW begin, normal, texture lookup, four
vertices and draw. The original default quad conversion submits six packed
vertices. Cactus adds and reverses its side offsets through the genuine
`nw.c(FFF)V` body. Its zero operands remain positive zero, including undo.
Stairs and Fence use the side-only texture overload; metadata does not select
those face textures. Fence's rail bounds extend beyond the unit Z range and
use the existing face methods' original UV fallback.

Exceptions retain the reached state. A failed Cactus face keeps an already
applied offset. A failed Fence part keeps its current bounds; the final reset
is not cleanup. A nullable Stair source retains the open NW batch, first-part
bounds and translated modelview. Nested Stair sources keep distinct SS frames.
Malformed cyclic aliases reach the existing native 16-frame limit instead of
unbounded C recursion. That native refusal is not original StackOverflow
qualification.

## Original and native comparisons

The [original geometry packet](../../tests/reference/client/inventory_geometry)
executes 17 calls on actual registered Cactus, Wood Stairs, Cobblestone Stairs
and Fence owners. Sixteen return and the final nullable-source call fails.
Ordinary and read-only JDI processes agree on complete inputs and returned
state. A separate GL_COMPILE cohort agrees on all CPU data and preserves the
distinct deferred GL state.

The finite comparison covers 216 actual face batches, 864 source vertices,
1,296 packed vertices and 10,368 backing words. All 7,596 active words compare
exactly. Each VM separately checks its 2,772 inactive words against its own
prior backing. The original Font-used buffer and the native fresh buffer are
not made artificially identical.

The native fixture executes complete production calls in two fresh processes.
Its GL_COMPILE run inspects actual submitted bytes through passive Display
list records. It does not fabricate observations of native NW draw-entry
fields, bounds or PCs. Immediate mode independently checks returned fields,
matrices and known color state. The existing backend marks current color
unknown after direct color-array draws in the three authored AO profiles;
retained requested operands are checked separately and are not presented as
queried current attributes.

The [additive translation packet](../../tests/reference/client/nw_translation)
records 106 calls across 36 profiles. Two ordinary VMs and a third read-only
JDI target agree byte-for-byte. All 318 field stores occur in s/t/u order at
PC8/19/30. Native replay preserves raw float widening, double addition, signed
zero, NaN payloads, unrelated state and ownership. This CPU operation has no
source begin/draw guard.

The [Piston texture packet](../../tests/reference/client/piston_inventory_texture)
matches 34,612 rows, including 34,320 metadata-texture calls on both Piston
bases. Invalid directions and front faces bypass WJ initialization. Other
faces reach the real lazy Direction owner and its current opposite element.
The original comparisons treat NaN bounds as non-violating: they can fall
through to the live front texture. Native code preserves that behavior.

Native HUD controls draw all six specialized registered Items: Cactus, both
Stairs, Fence and both Piston bases. They retain the existing Dirt/Grass
controls, actual Stack lifetimes and failed-child Font refusal/retry checks.
Separate native controls exercise nested Stair sources and the bounded cyclic
alias refusal with actual Display/NW operations.

The [nested Stair getter packet](../../tests/reference/client/inventory_geometry/side_chains)
separately matches 30 original outcomes and 80 getter entries. Late Wood
textures pass through both actual Stair receivers; NULL retains both SS5
frames. This direct getter evidence does not add a nested enclosing-CV geometry
comparison. The native nested-success control checks returned faces and state,
without a separate packed-UV comparison.

Run the asset-independent packet, CPU and hidden-display comparisons with:

```sh
ctest --test-dir build-windows --output-on-failure --no-tests=error -R '^(client\.(nw_translation|inventory_geometry|piston_inventory_texture|gui_ingame_draw)|tools\.(nw_translation_packet|inventory_geometry_packet|inventory_side_chains_packet|piston_inventory_texture_packet))$'
```

Display tests require `MC_ENABLE_CLIENT_DISPLAY_TESTS=ON`. These finite
component and HUD checks do not establish original raster pixels, arbitrary
current attributes, complete original Piston geometry, source-input acquisition
of every Item, unrestricted play, audio or an FPS improvement. Save/reload,
other World render families and full client playability retain their own
qualification scopes.
