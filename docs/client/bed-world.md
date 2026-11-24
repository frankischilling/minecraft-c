# Bed rendering

Bed Block 26 renders through the original client render type 14. The current
World renderer enters `CV.b(Block,III)`, captures render type at bytecode 1,
updates the same Block's bounds at 15, then calls `CV.q(Block,III)` at 265 and
returns at 268. The Bed's
`VE.a(IBlockAccess,III)` update calls `VE.r`, setting bounds
`(0,0,0)..(1,0.5625,1)` on that actual Block.

The body is in [bed.c](../../src/client/render/blocks/bed.c). It captures the
Tessellator at bytecode 0, reads metadata at 13 and executes the Bed's direction
and head predicates at 22 and 29. Direction is `metadata & 3`; bit 8 selects
the head half. The lower plane uses `y + minY + 0.1875`, and the upper plane
uses `y + maxY`. Both planes always emit. The upper UV coordinates follow the
original four direction branches and vertex order.

After those two quads, `JJ.a[direction]` selects the internal half-face to
exclude. The head branch instead uses `JJ.a[JJ.b[direction]]`. It captures
the outer `a` array at bytecode 681 before capturing `b` at 684 and loading
its element at 689. The remaining sides use the actual Block visibility and
brightness children. The shade factors are 0.5 below, 1 above, 0.8 on the two
Z sides and 0.6 on the two X sides.

`VE.a(side,metadata)` supplies the textures. Side 0 reads the current Planks
texture through `UU.y`; other sides index `JJ.c[direction][side]` before
selecting the head/foot texture offset. The two direct planes retain these
texture calls when an override is active. Ordinary side-face children apply
the override. They also retain the current AO flag and per-vertex colors.
`CV.q` leaves that AO state intact and clears its flip field `e` only at the
final source store 1228. Its source return is 1232.

[bed_render_tables.c](../../src/block/registry/bed_render_tables.c) implements
the independent client `JJ` class. Its initializer allocates two four-element
arrays and a four-row, six-column texture table in source order. `a` publishes
at 19, `b` at 41 and `c` at 172; the initializer returns at 175. These are
Registry-owned arrays with captured identities. Reads preserve null fields,
null rows, signed index failures and later changes to array contents.
Unknown native backing is refused before dereference. A native allocation
refusal at 23 keeps the already published `a` array and latches the failed
initializer; raising the budget does not replay it.

A failed renderer child retains earlier bounds, flip stores and appended
Tessellator words with its source frame and typed child error. Native quotas
and strict floating-point prerequisites remain explicit native policies.
The caller owns the begun batch and its cleanup; the Bed body does not begin,
draw or reset it.

The [reference packet](../../tests/reference/client/bed_world/provenance.json)
contains 192 rows from each of two fresh original VMs:

| Inputs | Coverage |
| --- | --- |
| Metadata | All 16 values |
| Texture override | Default and texture 17 |
| Six profiles | Air; opaque neighbors; forced faces; retained AO colors; changed `JJ.a` contents; changed `JJ.c` top mapping |

Each row compares the returned CV fields, final bounds and every appended
Tessellator word. The Tessellator converts each source quad to six encoded
triangle vertices. The profiles therefore append 12, 30 or 36 vertices,
with eight words per vertex. The original observations use actual prepared
World, ChunkCache, Block and Tessellator owners without an OpenGL context.
Archive, class/Code and helper hashes accompany the packet. Both original
VM guards report zero network, native-library link and process-execution attempts.

Native replay passes all 192 rows and 1,066 checks on Windows MSVC Release
and Linux Clang 18 Debug with ASan/UBSan and leak detection. Its authored
failure controls also check captured arrays after field replacement,
null/foreign tables, retained geometry and flip state, and the failed JJ
initializer prefix. Packet corruption controls pass normally and under
Python `-O`. Public replay needs no original archive or assets:

```sh
ctest --test-dir build --output-on-failure -R '^(client[.]bed_world|tools[.]bed_world_packet(_optimized)?)$'
```

Separate native Windows scenes exercise the rendered Bed during saved-Bed
respawn. The retained valid run completes two cycles and 1,013 checks; the
missing-Bed run completes one cycle and 977 checks, including the translated
notice at age 9. Both use the real Death-screen button, direct fatal-body
input, a direct World save and common cleanup on monitor 2. Their captures
were visually reviewed, and the 5,408 guarded source files and imported
resources stayed unchanged. [Player coordinates](../entity/player-coordinates.md)
records the separate original component readback of C-written save records.

These CPU rows establish the declared rendering calls. Original whole-frame
pixels, audio, arbitrary terrain and FPS remain separate qualifications.
The older [mesher](../../src/client/world/mesher.c) still reports its own type
14 dependency; the loaded `CV` renderer is a separate pipeline. Saved-Bed
validation and exit search are described in [Saved-Bed spawn](../world/bed-spawn.md).
