# Lever placement and switching

Client Lever Block 69 now implements its two support predicates, clicked-face
placement, activation, neighbor support checks and removal. World mutation
dispatch reaches the actual Lever bodies. Rendering has its separate
[geometry comparison](../client/lever-world.md).

The face predicate accepts only the top and four wall faces. It checks the
selected attachment without checking whether the destination is replaceable.
The general predicate checks west, east, north, south and below in order,
stopping at the first solid support. Unlike an ordinary Torch, a Lever cannot
use a Fence as floor support. World placement still performs its enclosing
collision and occupancy checks before calling the face predicate.

Clicked-face placement retains the old powered bit. A supported floor consumes
one actual World Random `nextInt(2)` call to choose orientation 5 or 6. A wall
uses orientation 1 through 4. An unsupported face freshly reads metadata,
drops the Lever and calls the notifying air setter, ignoring its Boolean result.

Activation returns true immediately in a remote World. Otherwise it toggles
bit 8, writes notifying metadata, marks the single-cell render range, plays
`random.click` and notifies neighbors around both the Lever and its attachment.
Volume is 0.3; pitch is 0.6 when switched on and 0.5 when switched off. The
Player argument is unused. Each notification freshly reads the captured Block's
ID, so synchronous callback changes remain observable. Failure keeps all
earlier effects and the reached diagnostic until checked retirement.

The single-player mining controller's initial left-click hook also invokes
activation and discards its Boolean result, matching the original wrapper.
Its retained activation child survives until the mining call is retired. A
separate native controller test checks two ordinary mining-start toggles and
a callback failure after the metadata write. This does not turn the omitted
original wrapper rows into an original controller comparison.

Neighbor changes first run the general support helper. If some support remains,
the body still performs all six attachment checks, including two separate
floor queries for orientations 5 and 6. Losing the selected attachment drops
the Lever before removing it. Removing a powered Lever notifies its own cell
and attachment before reaching the empty base removal body. These bodies have
no remote guard; the drop and notification children retain their own guards.

## Evidence and limits

The [original packet](../../tests/reference/world/lever_placement/README.md)
executes 244 calls in each of two fresh ordinary VMs against the verified client
archive. It retains ordered callbacks, changed Chunk arrays, lighting queues,
published Items and each process's own Random states. Floor orientations are
audited against each process's own stream rather than normalized between VMs.

The native replay selects 211 entered bodies: 209 complete literal effect rows
and two floor-placement rows checked against the native process's own Random
stream. The 33 original left-click wrapper rows remain outside that comparison.
Separate native controls cover support ordering, invalid faces with a null
World, all metadata values, a null floor Random, callback order, sound refusal
after the metadata write and refusal to retire an active call.

Windows MSVC Release and Linux Clang 18 Debug with ASan/UBSan pass the native
replay and focused controls. The Linux run enables leak detection and
halt-on-error and preserves all 4,895 guarded non-document inputs. Both hosts
also pass the 32-row rendering comparison. The full enabled Windows suite
passes 652 tests in 238.64 seconds; Linux's seven-test selection passes in
5.85 seconds. This is not a full Linux suite result.

These results describe the component checkpoint. The saved circuit below adds
bounded native input and persistence coverage. Circuit timing across arbitrary
devices, original whole-frame pixels/audio, extended survival and FPS require
separate checks. The server Lever lifecycle remains unimplemented.

```sh
ctest --test-dir build --output-on-failure -R '^(world[.]lever|world[.]lever_placement_reference|tools[.]lever_placement_packet|client[.]placement_world)$'
```

## Saved circuit

The [Windows SDK scene](../../tests/reference/client/lever_placement/README.md)
places a wall Lever beside Redstone Dust through ordinary SDL input. Right
click turns it on, left click turns it off through the actual mining-start
hook, and a final right click powers it again. Lever metadata follows 4/12/4/12;
Dust follows 0/15/0/15. One Item is consumed and used69 increments once. The
unchanged camera ray selects the actual Lever outline.

Default AO remains true. Six actual terrain batches match the active position,
UV and color arrays from the separate 160-call original scene packet. Native
outlines match 18 independently derived positions per phase. Ordinary
Save/Quit and reselection preserve the powered circuit, health 5 and Item69
count 1 across distinct World/Player owners. Independent NBT readback checks
all 441 saved Chunks and zero Item Entities. Only the actual eight-byte session
token changes on disk during reselection.

The post-fix opportunity passes 1,945 native checks and 19 evidence-corruption
controls with 2,143 selected source/product inputs and 413 assets unchanged.
Natural Redstone particles appear in the actual DN Lists during completed
frames. A prior failed opportunity reached the missing concrete particle
allocation. The [Redstone particle implementation](../entity/redstone-particle.md)
resolves that dependency; retained failures and the unchanged scene limits
remain documented. This finite Windows scene does not qualify unrestricted
circuits, general focus reliability, Linux loaded play or FPS.
