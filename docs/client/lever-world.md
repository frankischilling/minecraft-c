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
per call. The comparison uses the flat base path; it does not qualify the
ambient occlusion path, loaded gameplay, original pixels/audio or FPS.

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
