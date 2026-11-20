# Redstone Torch lifecycle

Client Redstone Torches 75 and 76 execute their addition, removal, neighbor and
scheduled-tick bodies through the genuine World services. Their private power
and burnout helpers preserve the original call order. The implementation is in
`src/world/runtime/mutation/redstone_torch/`; the defining Registry owns the
shared burnout List in `src/block/registry/redstone_torch_list.c`.

## Power, scheduling and burnout

The power helper reads metadata once, then checks attachments in the original
order: floor, north, south, west, east. It invokes at most one directional World
power query. Addition runs the inherited Torch orientation helper only for
metadata zero, then reads the live lit flag. A lit receiver notifies six
neighbors, reading its ID separately for each call. Removal reads the lit flag
before using World. Neighbor handling runs the inherited support-loss body,
then schedules the original receiver with delay two, even when support loss
normally removed the target.

A scheduled tick checks power before pruning burnout history. Each pruning
iteration reads the current World time before fetching the first current List
entry. It removes that entry only when the wrapped Java long difference exceeds
100. An age of exactly 100 remains. The List belongs to the defining class and
has no World or dimension key; Worlds sharing that class see the same records.

A lit, powered Torch captures the live unlit registration, reads its ID, fetches
current metadata and invokes the notifying ID/metadata setter. Its ordinary
boolean result is discarded before recording burnout. An unlit, unpowered
Torch checks the same history before capturing the live lit registration and
performing the corresponding setter.

Recording captures the List receiver before allocating XU and reading World
time. Construction stores the supplied coordinates and time, then appends to
that earlier captured List. Counting reloads the current List for size and get;
the eighth matching coordinate returns true. These reads retain their original
nullable-receiver and failure order.

Burnout emits one `random.fizz` event and five smoke events. Sound coordinates
use int-to-float conversion and float addition before widening. Its pitch uses
two fresh reads of World Random. Smoke consumes three doubles per event from
the supplied Random, preserving aliasing with World Random and a reached NULL
argument. Native floating-environment admission occurs at the reached floating
operations and leaves earlier setter/List effects intact.

## Storage and failure prefixes

The typed List retains size, capacity, modification count, record aliases and
backing identity under the selected JDK 17 reference contract. Growth charges
the overlapping old/new arrays. A refused growth retains the already stored
modification count and existing elements. Removal shifts entries, clears the
last slot and transfers the removed occurrence until the ignored result is
released. Records and replaced arrays use reclaimable storage.

Native snapshots hold their actual records independently. Outstanding external
occurrences make Registry retirement busy before any collection is retired.
Cleanup covers owned Lists even after the static field is rebound. Source
NULL/index failures and native allocation policies remain separate typed
causes. Current/deepest method sites are flat diagnostics; they impose no
gameplay recursion bound.

## Texture metadata

The [scalar packet](../../tests/reference/client/redstone_torch_texture/README.md)
covers 160 calls per ordinary VM. DB side one captures live Dust 55 and invokes
its metadata texture method. That receiver selects SM's direct `bm` read;
other sides resolve the inherited UU metadata body and its virtual side call.
The native API now preserves both branches, including signed operands and live
instance texture inputs.

## Verification scope

The [lifecycle packet](../../tests/reference/world/redstone_torch/README.md)
declares 203 calls across lifecycle, power, burnout and aging cohorts, each
observed in two fresh original VMs. Six saved inputs supply time zero, 100, 101,
`LONG_MAX - 50`, `LONG_MIN + 49` and `LONG_MIN + 50`. Only declared Block bytes,
metadata bytes and remote flags are authored between calls. Other arrays,
clocks, Random owners, phases, queues, Sets and burnout history continue.

Windows and Linux native observations match all 203 deterministic case records.
The comparison covers five arrays in four canonical Chunks per World, ordered
byte deltas, callback/failure prefixes, DB List state and aliases, World flags,
lighting order, scheduled records/deadlines, membership, and independently seeded
particle operands. Each process's own Random/Math continuations and exact
constructor counters are audited separately. Linux uses Clang 18.1.3 Debug with
ASan, UBSan, leak detection and halt-on-error; its four processes have empty
stderr. List ownership controls pass 1,946 checks on each host. The integrated
Windows client builds and eleven neighboring gameplay tests pass.

A Windows SDK placement run consumed one of two held Item76s. Passive frame
observations captured lit76 metadata 4 at World time 7 with its actual E/F
deadline at time 8, then off75 metadata 4 at time 8. Three further World/HUD/held
frames returned. Normal Save and Quit wrote time 13; a distinct World and Player
reloaded off75 and the remaining Item76 count of one. Independent NBT readback
confirmed the Torch, wall, pressed Plate and unpowered Dust. All 3,784 frozen
project, build, header and asset inputs stayed unchanged. The prelaunch pressed
Plate is a declared transient input; the run does not establish sustained
Entity occupancy or a complete circuit.

The separately built public fixture passed its first opportunity with 1,232
checks. It observed the same time 7 lit placement and time 8 off transition,
saved at time 14 and reloaded at time 15. All 3,792 frozen inputs stayed
unchanged. Its C body and scene ledger are byte-identical to the first fixture;
the public Python projection changes paths and file guards only. Both runs and
their distinct native products remain retained. Enable
`MC_ENABLE_CLIENT_REDSTONE_TORCH_PLACEMENT_TESTS` to register the optional Windows
scene; its five pure methods and 26 corruption cases run without launching it.

Four extreme-coordinate lifecycle calls construct a dropped Item before World
publication rejects it. Separate, tightly gated accounting reconciles four
World float steps, eight Math steps and one default Random construction per
call. It preserves the neighbor cases' real scheduler effects. The original
failed accounting reports remain unchanged beside the successor admissions.
Unpublished Item identity, fields, Random state and constructor clock operands
remain unobserved; no callback or object snapshot is invented.

Physical Java collection topology outside the DB List contract and extra
acquired Chunks remain outside the stated comparison. Dedicated-server Torch
dispatch, unrestricted circuits, extended play, original pixels/audio and FPS
retain separate qualification. Live placement and save/reselection results are
independent native observations, without an original GUI or pixel comparison.
