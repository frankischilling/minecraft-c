# Lever rendering

The client now renders Lever Block 69 through its original render type 12.
The base follows ordinary Block shading and uses the current Cobblestone
texture when no override is active. The handle uses the Lever texture,
original pooled Vec3 objects and MathHelper rotations. Switching metadata bit
8 changes the handle angle; metadata 1 through 4 attaches the base to a wall,
and 5 or 6 places it on the floor.

The renderer preserves the actual bound-update call before type dispatch,
the captured texture-override decision, ignored base render result, late
brightness/emission reads and final source fields. Metadata 0 and 7 preserve
the original fallback behavior. A failed base child retains its already
stored bounds and texture override. Native holds release on success and
failure without resetting the shared Vec3 pool.

The [original packet](../../tests/reference/client/lever_world/README.md)
contains 32 complete calls in each of two fresh ordinary VMs. It covers every
metadata value and default/override textures. Native replay compares final
bounds, override/AO state, eight pool acquisitions and all 576 appended words
per call. This packet uses the flat base path. A separate scene packet covers
ambient occlusion and ordinary face visibility for the declared Lever/Dust circuit.

The native replay and failure controls pass on Windows MSVC Release and Linux
Clang 18 Debug with ASan/UBSan and leak detection. Linux's enclosing seven-test
run preserves all 4,895 guarded non-document inputs. These platform results
extend the native qualification without changing the frozen original packet.
Separate native allocation admission checks a failed fourth Vec3 factory:
the completed base geometry and three pool acquisitions remain, with no
retained native holds or allocation growth.

Public checks require no original archive or assets:

```sh
ctest --test-dir build --output-on-failure -R '^(client[.]lever_world|tools[.]lever_world_packet)$'
```

## Circuit scene geometry

The [scene packet](../../tests/reference/client/lever_scene_geometry/README.md)
contains 160 calls in each of two further original VMs. It covers skylight
subtraction 0 through 15, AO off/on, and the circuit before placement, off and
on. The genuine dispatcher renders the wall Lever and adjacent Redstone Dust
from declared synthetic Chunk arrays. Its translation `(0,-64,0)` occurs before
float narrowing. The attached Stone hides one base face: the Lever contributes
66 encoded triangle vertices and Dust contributes 12.

Native CPU replay matches all eight words per vertex, final bounds, override,
AO flag and Vec3 pool advancement. The loaded-scene comparator uses the same
literal original rows for the active position, UV and color arrays: six words
per vertex. Normal and padding words remain inactive in that submitted format.
It independently checks 150 surrounding Block, metadata and light cells.
Neither path translates or rounds observed geometry after rendering.

An earlier capture remains preserved as failed evidence. Its Java helper used
integer arguments and selected the normal setter `nw.b(FFF)V` instead of the
translation `nw.b(DDD)V`. The corrected helper verifies its compiled descriptor
and actual fields before two fresh captures. The runtime renderer did not
change to accommodate those invalid coordinates. Public controls reject that
predecessor and changed or ambiguous selected geometry.

These finite calls establish the specified scene geometry. Original whole-frame
pixels, audio, arbitrary terrain and FPS remain separate qualifications.
