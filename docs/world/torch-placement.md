# Ordinary Torch placement and support

Client Torch 50 now runs its original support, orientation, addition, neighbor
and tick bodies. The existing BlockItem path places it through the ordinary
World setter, consumes the Stack, updates the statistic and reaches saving.
Redstone Torch lifecycle and the dedicated-server counterpart remain separate.

## Source behavior

Can-place queries solid support at -X, +X, -Z and +Z, returning on the first
true result. Otherwise the private floor helper checks below. It first calls
World solid support; only false reaches a fresh ID query and the live Fence
singleton's ID. It does not call the base target-replaceability predicate or
replace the live Fence read with ID 85. Coordinates retain Java wrapping.

On addition, the first supported horizontal direction selects metadata 1..4;
floor support selects 5. The private validation body then runs even when no
orientation was selected. Without any support it reads metadata, drops through
the same receiver and calls notifying set-air. The setter's ordinary false
result is discarded. These operations preserve preceding effects on failure.

The clicked-face hook independently reads current metadata and checks the
selected support. Unsupported or unmatched faces preserve that captured value.
It always calls notifying metadata-set, including equal values. Chunk addition
runs before this clicked-face override; the implementation retains both writes
and their notifications.

Neighbor change first validates general support. If supported, it reads
metadata, queries all four walls and the floor in order, then removes a Torch
whose actual attachment is gone. An earlier mismatch does not skip later
support queries. Removal uses another fresh metadata read. Tick invokes the
empty base tick and reads metadata; only zero calls onAdded. It never draws
the formal Random, including when that argument is NULL.

The retained placement API keeps the invoking code Registry independent of
the World's Registry. Five reached support frames, separate ID/metadata frames
and the notifying mutation child retain their failure causes and callback
producers until checked retirement. The shared Fence comparison performs only
the reached live static-field operations. Mutation callbacks preserve current
and deepest concrete sites without imposing a source recursion limit.

## Original comparison and component checks

Two fresh ordinary VMs execute 193 calls covering the seven unchanged Torch
methods, the inherited face wrapper and the World placement wrapper. Both
produce the same 193 literal effect records and terminal line. The
[packet](../../tests/reference/world/torch_placement/README.md) retains all
array changes, ordered lighting queues, notifications, published Item fields
and nine reached NULL-World exceptions.

Raw entropy differs between processes and is retained separately. Each
process's own observed stream validates 34 drops, 136 World float steps and
272 Math LCG steps. No stream is reseeded or replaced. The first Math prestate
is unobserved; its poststate supports a formula audit, not a cold-phase claim.

Windows MSVC Release and Linux Clang Debug ASan/UBSan match every literal
record and independently pass the same entropy audits. Packet tests reject 75
corruptions; the native comparator rejects 49. Native controls cover live
Fence rebinding/NULL, support short circuits, independent Registries, typed
source failures, BUSY reuse and checked retirement. Real RAW RenderGlobal
failures and native notification refusals preserve the metadata write and
retained callback graph. Ten focused tests pass in 17.66 Windows and 27.77
Linux CTest seconds, with leak detection and halt-on-error on Linux.

The retained pre-change Linux probe fails at the missing Torch can-place body.
Its 1,553 selected inputs remain unchanged. This is a semantic baseline, not a
timing comparison.

## Windows input, lighting and persistence

The final two fresh-process SDK profiles pass 3,242 placement checks and 5,724
support-removal checks in 6.66 and 7.87 CTest seconds. Each preserves 1,999
selected inputs and 413 assets. Saved input contains Dirt at (3,64,3), Air
above, and a selected Torch Stack of two. One actual RMB tap places Torch 50,
selects attachment metadata 5, plays the sound, leaves one Item and increments
the use statistic. The held Item, HUD and terrain continue through ordinary
frames. Hook order is inferred from source control flow and the returned
setter/use suffix; there is no direct hook-order observer.

The actual type-2 section changes from 60 to 90 vertices, including 30 Torch
vertices with checked positions, UVs and emission-forced white. Normal frames
drain lighting to an empty queue: Torch light is 14, adjacent air is 13 and
the two-step samples are 12. The opaque support remains zero.

The separate removal profile sends one relative mouse event (24,0), with saved
sensitivity 0.5, and requires the actual Dirt top-face ray before holding LMB.
Ordinary mining removes Dirt, neighbor handling drops the Torch, and both
cells become Air. It observes one genuine Dirt Item and one Torch Item, a
30-vertex remaining section and nine zero BlockLight samples after normal
frame work. No pose, progress, queue, clock or Random field is repaired.

Both profiles complete Save and Quit and reselection into distinct World and
Player owners. Independent Region/Player NBT checks verify the Block,
metadata, remaining Stack, lighting and both removal-profile Item bodies.
All thirteen saved files retain their bytes through reselection except the
actual eight-byte session token. Unrelated saved data stays intact, and
terminal checks verify that both native windows disappear.

## Fixture correction and retained failures

The first saved scene supplied 289 Chunks, covering [-8,8] on both axes.
Original random ticks visit [-9,9]; Grass and spawn probes can reach one
further Chunk. Passive traces showed newly generated terrain and Lava/Sky
work outside the saved area, starving the queued Torch update. Three lighting
failures remain retained, including a predeclared two-sample diagnostic pair.

The corrected prelaunch builder appends 152 flat, populated Chunks, supplying
exactly 441 coordinates in [-10,10]. It independently verifies every appended
array and preserves all 289 original records, compressed payloads, NBT wires,
slots and timestamps byte-for-byte. Player, local terrain, settings,
assertions, deadlines and frame bounds stay unchanged. The first corrected
placement run passes; diagnostic logging is then removed before the final
two-mode cohort. There is no runtime lighting or work-budget change.

Two other attempts stop before placement on the existing native focus
mismatch: Windows reports canvas focus while SDL reports no keyboard-focus
window. Both remain retained. The final cohort passes without a focus change;
these results do not establish general focus reliability.

These are finite method and Windows gameplay results. Redstone Torch
lifecycle, original loaded pixels/audio, Linux loaded play, unrestricted play
and FPS require their own evidence. No performance improvement is claimed
from changing the fixture's saved terrain coverage.
