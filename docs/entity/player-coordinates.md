# Player saved coordinates

The client implements the original `GS.Q()` saved-spawn getter,
`GS.a(BR)` copy setter and inherited `SN.c(DDDFF)` placement child. Saved
coordinates retain the existing `mc_player_coordinates` type and Player
field types. The public ownership API is in
[coordinates.h](../../include/mc/entity/player/coordinates.h).

`GS.Q()` reads the saved field at bytecode 1 and returns that same nullable
object at 4. `mc_player_saved_spawn_get` performs that read once. A null field
returns a null pin successfully without allocating or retaining a Player.
A nonnull result is available through `mc_player_coordinate_pin_value`; it
preserves the original coordinate identity.

A pin owns one native Player occurrence and protects its coordinate node in
the existing NBT ledger. The collector keeps nodes referenced by `v`, saved
spawn, the private `bN` coordinate field, or any active pin. A callback can replace saved
spawn and run the collector while an earlier source local still reads its
original object. Releasing the pin collects unreferenced nodes and then
releases its Player occurrence. This also permits the pin to be the last
native owner of a RAW Player. Release performs no source field store or
World callback.

The getter's output slot must initially be null. A foreign saved-field pointer
is refused after the source read without dereferencing it. Native pin/Player
hold overflow and the Entity allocation ceiling are explicit failures. These
are native resource policies rather than evidence about JVM allocation-failure
timing. The [implementation](../../src/entity/player/coordinates.c) admits
canonical LocalPlayers in RAW, constructed and failed constructor states.

The setter creates a distinct coordinate for every nonnull argument: `NEW` at
5, the actual BR copy constructor at 10, then saved-field publication at 13.
The copy constructor reads and stores X, Y and Z in order. A null argument
stores null at 21; both branches return at 24. The native setter accepts a
live coordinate from another Player or an owned World result and copies its
values. Allocation failure preserves the previous saved field. Existing
aliases and pins keep their original nodes across successful replacement.

Construction and respawn share `mc_entity_source_position_rotation`, declared
in [construction_internal.h](../../src/entity/construction_internal.h). This
is the inherited `SN.c(DDDFF)` body:

| Source point | Reached behavior |
| --- | --- |
| 5, 9, 12 | Store X and both X history fields |
| 24 onward | Add the current float offset to Y, then store Y/history and Z/history |
| 55, 61 | Store supplied yaw and pitch; previous-angle fields stay unchanged |
| 77 | Invoke the actual `SN.e(DDD)` child on the same bounds |
| 80 | Return |

The placement error retains separate parent and position-child bytecodes.
An unsupported native floating-point environment stops at 24 after the X
stores. A null bounds receiver fails at `SN.e` bytecode 86 after the complete
pose/history/angle prefix. Bounds arguments use the original float width/2,
double arithmetic and existing bounds identity. The helper introduces no
World query, Random draw or READY-state requirement.

In `Minecraft.a(ZI)`, an existing Player and false force flag reach Q at 50.
Its pinned result remains live through the saved-Bed helper at 63 and later
World callbacks, including Player replacement at 206 and old Input cleanup
at 321. A valid Bed skips the fallback World spawn getter at 91 while still
executing spawn repair at 146, cleanup at 153, replacement and spawn reset
at 229. The new Player receives a distinct copy of the original saved Bed
at 242, before placement at the separately selected exit.

The caller captures Player `h` at 248 before evaluating all three placement
arguments. Each coordinate converts to float and adds its float offset before
widening to double:

```text
(double)((float)exit.x + 0.5f)
(double)((float)exit.y + 0.1f)
(double)((float)exit.z + 0.5f)
```

Yaw and pitch are zero at the `SN.c` invocation at 281. The retained respawn
call releases its saved-coordinate pin after checked child retirement,
including any failed Bed helper's World occurrences. The surrounding
[Saved-Bed spawn](../world/bed-spawn.md) and
[death/respawn](../client/death-respawn.md) guides cover the remaining caller work.

The [original packet](../../tests/reference/entity/player_coordinates/provenance.json)
contains five rows from each of two fresh VMs using unchanged original
classes. They cover getter identity, distinct copies and null, three complete
pose/history/bounds vectors, and the actual null-bounds exception prefix.
The vectors include coordinates beyond float's exact integer range at 2^24
and both signed integer extremes. Native replay compares every selected
double word with the original packet.

Windows MSVC Release and Linux Clang 18 Debug with ASan/UBSan and leak
detection each pass all five rows and 144 native fixture checks. The authored
native cases additionally cover simultaneous pins, real NBT replacement and
collection, self-copy, alias retention, caller mutation, resource refusals,
foreign-pointer rejection and pin-only Player lifetime. Packet corruption
controls pass normally and under Python `-O`. Public checks require no
original archive or assets:

```sh
ctest --test-dir build --output-on-failure -R '^(entity[.]player_(coordinates|nbt_coordinates)|tools[.]player_coordinates_packet(_optimized)?)$'
```

A separate private compatibility probe reads copies of C-written metadata
from the first valid and missing-Bed loaded scenes. Two fresh JDK 17 VMs use
the original AS gzip reader and a constructed LocalPlayer's full
`SN.e → DC.a → GS.a → LS.a/IX.b` read, followed by Q. Both saves restore health
20 and empty 36-slot main/four-slot armor arrays. Their encoded empty
Inventory List has element type Byte 1, which the original reader accepts.
The valid save returns Q `(3,64,4)` and position
`(2.5,65.62000000476837,3.5)`; the missing-Bed save returns null and position
`(0.5,65.62000000476837,0.5)`. Position, Motion, Rotation and both history
triplets match the saved bits. Player and World Random states stay unchanged
during each read.

That probe's private receipt is
`.local/bed-save-readback-original-2/report.json`. Its two VMs agree on the
selected rows. Guards record zero network, native-library link, file-write/delete
and process-execution attempts. Hashes of the original files, read-only copies,
archive, SDK and helpers stay unchanged. It qualifies original
component readback of these records; it does not establish a full original
World restart or GUI session.

The first loaded native flows completed 1,034 and 1,049 checks, but their
Python reports failed an incorrect expectation that an empty Inventory List
must declare Compound type 10. Original `SP.a(DataOutput)` writes Byte 1 for
an empty list at bytecodes 35 through 45. The strict decoder was corrected,
and the failed receipts remain retained. Fresh complete scene runs pass
1,013 and 977 checks; their direct World saves contain health 20, empty
Inventory, and respectively saved coordinates `(3,64,4)` or omitted spawn
fields. These scene checks use direct fatal-body and World-save calls plus
the real Death-screen button; save-menu/reload input remains separate.
