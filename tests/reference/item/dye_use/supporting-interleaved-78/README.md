# Original Dye block-use cohort

Two fresh ordinary JVMs execute 78 declared calls against the unchanged Beta
1.7.3 client archive SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Their complete stdout bytes match. There are 39 direct `km.a` calls and
39 separately prepared `iz.a(Player,World,IIII)Z` wrappers. All original game
classes and methods remain unchanged. The authored helper supplies inputs,
reads state and installs a real WorldAccess observer. It performs no debugger
invocations or source writes.

The 78 calls share one genuine App, saved World, four canonical Chunks, Player
and statistics writer in each VM. Before each call the helper directly resets
the five Chunk byte arrays and height fields, prepares the declared floor,
target and metadata, restores the original Item Random cell, seeds that actual
owner and actual World Random, and allocates a new Stack. It does not reconstruct
the World or clear/drain lighting, scheduled or other residual World state
between profiles. These are ordered shared-World cohorts, not independent
fresh-World cases or complete World-state oracles.

`setup-contract.json` states the exact reset operations. The sky nibble array is
filled with byte0x0f, so even-Y light is15 and odd-Y light is0. It is not uniform
light15. The complete Block/metadata map and sparse differences across Blocks,
metadata, Sky light, Block light and height arrays are measured for each call.
Those full byte changes are relative to that row's declared direct preparation.
Unreported World queues remain outside this projection.

The matrix covers signed non15 metadata with a NULL World argument, unrecognized
Stone and Mushroom targets, remote Sapling/Crop/Grass, immature and mature Crops,
negative and wrapped counts, a declared false-setter alias fixture, five finite
Sapling growth cases, a rejected tree restore, Grass's full128 attempts on
plane/isolated/occupied scenes, all three Grass plant choices, out-of-height
false writes, callback failures/count stores, NULL operands and late wrapper
NULL-Player failures. Original Grass plane output includes Tall Grass31,
yellow Flower37 and red Flower38. The false Crop setter case temporarily sets
the actual Crop receiver's bn field to0 so an out-of-height Air read selects
the Crop branch. That is a declared reflective fixture, not ordinary gameplay.

Both Random identities are retained in the observations. Fields named
`item_random_before`, `item_random_after` and each callback's `item_random`
always describe the original saved GM.b Random owner. They do not follow the
live GM.b cell after aliasing. `world_random_*` describes actual World.r.
`gm_b_is_world_after` records the live alias before restoration. Observer mode2
aliases the mutable GM.b field to the existing World.r owner at the first real
cell callback. The original Item owner then stops consuming draws while World.r
advances; the resulting plant map differs from the baseline. This provides an
executed live-Random-reload case without a Random subclass or method replacement.
All callback inputs and their order remain explicit in each row.

There are 64 ordinary returns (50 true,14 false) and14 expected exception
outcomes. Six declared observer failures throw `IllegalStateException`; eight
NULL-reference calls throw `NullPointerException`. These outcomes remain in
the complete successful capture. They are not discarded failures or native
parity claims. `source-outcomes.json` identifies every exceptional row and keeps
native exception-class equivalence unqualified. Source callback failure and
a future native observer refusal require separate outcome/cause checks.

The actual IZ wrapper awards the real used statistic only after a true body
result. Remote recognized targets therefore increment it without consuming or
advancing either Random. NULL-Player wrappers fail after the completed body
effects. Mature Crop use still notifies and consumes. A count99 callback ends
with Crop98 because consumption is late, but Grass99 because its decrement
already happened. No statistic entry or writer is replaced by NULL.

`source-pins.json` records whole-class and method Code hashes and sizes only.
It includes the full380-byte KM method. Static bytecode anchors remain distinct
from runtime ordering observations: no read-only JDI trace was captured here.
Tall Grass invokes its notifying metadata setter at KM316; PC315 pushes1.
The existing C implementation, graphics, audio, save/reload, server behavior,
unrestricted gameplay and frame rate have not been compared by this packet.

`DyeObservation.java`, profiles, declared inputs and source pins retain the exact
successful frozen authored bytes. `original-capture-driver.py` retains that
run's exact driver. Both `.stdout` files preserve captured bytes with packet
attributes disabling newline conversion. `selected.jsonl` is an explicitly
declared LF projection of DYE-prefixed lines. Portable provenance replaces
absolute input/command paths with artifact roles, retains raw private receipt
hashes and per-VM unchanged save manifests, and represents private warning logs
by their hashes. It does not label projected metadata as raw evidence.

Run the Java-free self-audit and corruption controls with:

```text
python -B packet.py
python -B -m unittest discover -s . -p test_packet.py
```

The optional `capture_reference.py` copies the exact frozen driver/inputs into
a new workspace `.local` task directory before an explicitly requested replay.
It is not run by the public audit and requires the locally supplied archive,
pinned legacy libraries and qualified JDK17 environment.

The [native replay contract](native-replay-contract.md) plans39 genuine IZ
wrapper projections and explicitly separates the39 direct original rows.
No native result is supplied or implied by this candidate packet.
