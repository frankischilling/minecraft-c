# LocalPlayer NBT and World metadata

The [Entity NBT API](../../include/mc/entity/nbt/nbt.h) reads and writes the
actual canonical client LocalPlayer. Its read dispatch is `sn.e` followed by
`dc.a -> gs.a -> ls.a`; its direct writer uses `sn.d` and
`dc.b -> gs.b -> ls.b`. These calls reach the real Player Inventory. They do
not construct a Player, reset it, attach input or run a tick.

## APIs and field order

| Entry | Contract |
| --- | --- |
| `mc_entity_read_nbt_in_context` | Read into the held Entity with an explicit borrowed code Registry and NBT limits. |
| `mc_entity_read_nbt` | The same operation with NULL Registry/default limits; an Inventory Item lookup can therefore reach a missing-Registry failure. |
| `mc_entity_build_nbt` | Build an independent unnamed Compound from current semantic fields. |
| `mc_inventory_player_read_nbt` | Execute the actual `ix.b(sp)` array replacements and ordered List reads. |
| `mc_inventory_player_build_nbt` | Build the Inventory List in main-then-armor order, using actual Stack fields. |

The read first completes the base pose/motion/rotation prefix, then reads
Living `Health`, `HurtTime`, `DeathTime` and `AttackTime`. Missing Health reaches
the source's default 10 after its initial zero store. Player reads Inventory,
Dimension, Sleeping and SleepTimer next. A complete three-tag spawn coordinate
is read in X/Y/Z order; an incomplete triplet leaves the prior spawn reference
unchanged. Score follows in the LocalPlayer hook. Missing scalar
tags use their source defaults. Missing Lists become empty getter results,
whose later indexed reads can still fail. A wrong later tag preserves every
earlier field store and array publication.

Sleeping=true allocates and publishes the bed coordinate from the current
position, then reports the unimplemented wake invocation at `gs.a` bytecode 93.
It does not proceed to spawn fields, Score, Info.Player clear or publication.
Its prior Living/Inventory/Player stores remain visible. Native allocation and
strict floating-environment failures retain their actual earlier prefixes.

Inventory replaces its main array of 36 slots before its armor array of 4 slots,
before reading List entries. Each record reads Slot, constructs a real Stack,
then performs the reached Item namespace lookup. Duplicate slots replace the
earlier occurrence; main slots 0 through 35 and armor slots 100 through 103 are
selected after unsigned Byte interpretation. An out-of-range slot still reaches the preceding Stack
construction and Item lookup. The read does not change selected slot, cursor or
the changed flag, send notifications, preserve unknown tags or drop items.

The direct Entity writer emits the seven base fields plus four Living Shorts,
Inventory, Dimension, Sleeping, SleepTimer and Score: 16 top-level fields, or 19
when the spawn coordinate exists. Inventory records contain Byte Slot, Short
id, Byte Count and Short Damage, with source narrowing and main-then-armor
order. RAW or failed live Stack fields remain writer inputs; there is no
invented READY, ID-validity or count-clamping check. The canonical LocalPlayer
remains unnamed in the Boolean Entity-save wrapper; World Player metadata uses
the direct writer and supplies no `id`.

## Ownership and failure results

Keep owners, the explicit Registry and reached immutable input spans live and
exclusive through return. Output, error and limits storage must be mutually
disjoint and must not overlap owners, input views or borrowed tree storage.
Reads use balanced Entity/Registry/Stack holds and
preserve completed effects on failure. They do not add a blanket Entity READY
gate or infer the code Registry from an Entity's World. Compound names must be
unique; this is a typed immutable-view API, not a pointer probe or eager
whole-tree validator.

Inventory array views expire at their corresponding source replacement. An
input view borrowing an old Stack needs a separately held owner to keep that
view live; replacing an Inventory array releases its old occurrence holds.
Player coordinate values have native ownership nodes. Replacing a coordinate
retires a node only when none of that Player's coordinate references still
uses it; this is not an unbounded coordinate arena.

Writers return independent trees, leave `out` unchanged on failure and never
mutate a caller-supplied Compound/List. Free successful results with
`mc_nbt_tree_free`. Their limits charge live scratch, intermediate trees and
the final tree/validation peak. Borrowed owners and allocator overhead remain
separate. Inventory array allocations charge their old/new overlap. Its reached
iteration ceiling fails before the pending List.get; it never reports truncated
success. Native budgeting and fresh-tree output do not reproduce Java temporary
allocation/OOME timing or HashMap order.

The [diagnostic contract](../../include/mc/entity/nbt/errors.h) keeps common
wrapper/base, construction, Stack, Registry and codec causes as complete values.
Its late-hook union has explicit NONE, CART and PLAYER tags. Inspect
`living_bytecode`/`player_inventory` only for PLAYER; inspect the Cart arm for
CART or the documented fresh NONE defaults. Initialize with
`mc_entity_nbt_error_init`; passive selection helpers initialize the complete
selected arm, preserve common causes and preserve an already selected arm on
repeated selection. Fresh indices use SIZE_MAX; a fresh Player Inventory slot
is -1. A reported slot is distinct from the List occurrence index.

This changes the diagnostic ABI and requires coherent rebuilding of every
consumer. The measured Windows x64 Entity NBT error shrank from 744 to 544 bytes
by sharing mutually exclusive complete late-hook data. In-place lower error
initialization also removes passive value-return temporaries. This error-storage
correction changes no actor layout and uses no child diagnostic truncation,
shared mutable scratch or stack-limit increase. The ordinary Windows Debug
chain passes with the unchanged 1 MiB reserve;
one deeper measured checkpoint left 64,384 bytes of reserved headroom, 60,288
after one 4 KiB guard page. That observation is not a global peak or arbitrary
callback/reentry stack bound.

## Save and attachment

[Full detailed World saving](../../include/mc/world/runtime/saving/saving.h)
checks the session and sets the actual Info version to 19132 before selecting
only the first live Player occurrence. It holds that Player while building its
direct NBT and the independent metadata record. Empty Player Lists and a first
null occurrence omit Player from that record without clearing the retained
Info.Player. Later Players are not serialized. Metadata uses the real clock,
precedes SavedData/Chunk saving and precedes later SizeOnDisk growth. Completed
version changes survive a later failure; native checked transport does not
claim original caught-file-error behavior.

[`mc_world_runtime_attach_player`](../../include/mc/world/runtime/players.h)
captures the actual Info.Player. A nonnull tag is held across the inherited
base/Living/Inventory/Player load. Only complete success permits clearing
Info.Player and then entering ordinary Player publication. Malformed input or
the wake93 frontier leaves that tag and all earlier effects intact, with no
later publication. The real map-cache provider does not become a centered-grid
provider by policy inference. Complete reached NBT and publication errors remain
available; the native API reports explicit failures rather than emulating the
source's Throwable printing.

## Qualified scope

The [reference package](../../tests/reference/entity/player_nbt/README.md)
compares 33 genuine original cases: two direct writes and 31 reads, of which 11
complete and 20 interrupt at their declared source failures. The comparison
checks 1,302 normal tag nodes in 40 trees and separately brackets three LastPlayed
values. The wake93 comparison is 37 records: 33 tags plus state, Random, site and
attachment records. The original resumes beyond that checkpoint and completes
wake/read/clear/publication; native full-wake parity is not claimed.

The earlier isolated Windows Debug/default 1 MiB and sanitized Clang18.1.3/GCC13.3.0 products each pass
486 NBT, 110 coordinate, 9 passive error, 169 publication, 678 active-tick and
247 allocation checks, plus the 12 selected regressions. Each Linux profile
also closes 17 native target dependencies and 28 comparator controls normally
and under optimized Python. Original execution is Windows JDK17; Linux-native results compare the
retained Windows observations using exact-hash input-location mapping.

The subsequent combined source passes all 1,072 local cases: 376 Windows and
348 per sanitized Linux compiler. Current binaries pass 490 NBT checks, including all four Living getter failure sites, plus the same coordinate, diagnostic, publication and active-tick checks. Fresh Windows
original execution and both current Linux-native receipt comparisons pass with
the same declared tag/clock/wake scope. The [testing record](../testing.md#current-public-runtime-integration)
describes the complete build cohort and retained-source checks.

This is a finite client method/save/attachment closure. Full wake, complete
frontend startup and World/Player gameplay, server gameplay, all concrete
Entity/Chunk save descendants, unobserved floating-remainder cases, original
mutable-tree/OOME/file-exception parity and global stack bounds remain unfinished.
