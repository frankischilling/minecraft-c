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
position, then executes the actual `wake(true,true,false)` invocation at
`gs.a` bytecode 93. After that child returns and its native work is retired,
the same reader continues through Spawn and Score. A malformed Spawn tag
therefore preserves the completed wake and the old spawn reference. A malformed
Score preserves the completed wake and any newly published spawn reference.
Native allocation and strict floating-environment failures retain their
actual earlier prefixes.

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

A reached wake can leave an owned child in `error.sleep_work`. That result is
noncopyable until `mc_entity_nbt_error_retire` succeeds. Keep its Entity, World
and Registry dependencies live; retire it before initializing, assigning or
reusing the error. `sleep` and `sleep_cleanup` borrow this stable work. Checked
retirement can refuse and be retried after the native obstruction ends. It
does not repeat the read, wake or any World callback.

The entry Registry occurrence is independent of the wake child. The reader
keeps it through later Spawn/Score reads and releases it after the child ends.
A later tag error can consequently have `sleep == NULL` while still owning
`sleep_work`. Consuming the child ends its borrowed lower diagnostics even if
the later Registry release refuses. The first source cause and caller PCs
remain available; cleanup reports its separate current cause.

NULL error is supported on ordinary read paths. At a reached wake93, it
preserves the preceding stores and refuses before creating a child without
a lasting ownership destination. The legacy compact World attachment result
keeps that same prefix contract. Use the detailed World loader for the complete
sleeping read and its owning diagnostic.

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

The diagnostic ABI requires coherent rebuilding of every consumer. The common
payload occupies 544 bytes on x86-64 Windows and Linux; three independent
pointer cells for the sleep owner and borrowed views extend the complete error
to 568 bytes. The earlier reduction from 744 bytes shares mutually exclusive
complete late-hook data; no full Sleep error is added to every union arm.
In-place lower error
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

The [detailed World loader](../../include/mc/world/runtime/loading/player.h)
captures the actual Info.Player. A nonnull tag is held across the inherited
base/Living/Inventory/Player load. Only complete success permits clearing
Info.Player and then entering ordinary Player publication. A failed wake or
malformed input leaves that tag and all earlier effects intact, with no later
publication. Its owning result must pass `mc_world_player_load_error_retire`
before graph retirement. Client Load and Respawn retire that actual child
before releasing their Player, World or Load lease. Source Exception, source
Error and a current native cleanup failure retain separate classifications.
The real map-cache provider does not become a centered-grid
provider by policy inference. Complete reached NBT and publication errors remain
available; the native API reports explicit failures rather than emulating the
source's Throwable printing.

## Qualified scope

The [sleeping-reader packet](../../tests/reference/entity/player_sleep_nbt_parent/README.md)
contains forty unchanged-original component cases, repeated in two fresh VMs.
Each VM completes 29 parent returns and eleven genuine first exceptions,
recording 5,393 original calls and 13,062 ordered events. Thirty wake calls
are reached from the real GS93 site; 29 return. The four FD roots return at91,
including a genuine absent-Info.Player path with no nested NBT read. A separate
case publishes the Player and enters sleep through actual calls before reading
the saved tag; neither fixture assigns the World's sleeping result to prepare it.

Earlier Windows MSVC Release and Linux Clang Debug ASan/UBSan products each match all forty
rows, 9,909 typed scalar comparisons and 58 active source positions. The native
fixtures complete 670,991 and 670,904 checks respectively. They also check 7,600
unselected native cells within their actual calls. Coverage includes four Bed
directions, missing/non-Bed cells, blocked exits, remote Worlds, signed timers,
Health zero, incomplete and malformed Spawn tags, absent/wrong Score, actual
World/bounds null failures and retained cleanup refusal followed by repair.

The current fixture selects the same direct-metadata World constructor as
the sealed producer. Its previous load-or-create selection could run initial
spawn search and publish Items before the declared-empty setup. A regression
first rejects retained native BEFORE `FD.s=true` against original `fd.s=false`,
then the fixture changes only its constructor enum. Current Windows and
sanitized Linux each match 9,949 scalars across all forty rows, including the
new constructor discriminator. All 58 active-site and 7,600 unselected-cell
checks remain. Pure controls reject a consistent wrong-constructor field
across before/after/retired snapshots, alongside every earlier control.

The observer pins entry coordinates and retains old Stack occurrences.
Native replaced Inventory backing arrays end at replacement: their old slot
values are captured while live, and the held Stacks are inspected again.
The comparison preserves old/current Stack aliases without reading freed
arrays or claiming their native array identities survive. Constructor entropy,
unselected fields and both Random objects are checked within each invocation.
The no-reader case's constructor history stays in the full raw evidence and
within-call check; it is excluded only from cross-VM absolute comparison.

Native diagnostics pair actual NBT parent positions and the retained wake's
typed cause. The original null-bounds leaf `SN.e(DDD):86` remains source-only:
native exposes its null-source result at base294/Pos, with no invented child86
receipt. Ordinary wrong-tag pairing covers the declared named-getter controls;
it does not claim native NU execution or Java Throwable implementation.
The original's two console prefix lines have fixed static formatter origins
and physical positions; initializer execution extent remains unobserved.
The packet does not qualify original drawing, natural sleep or a whole restart.

The [reference package](../../tests/reference/entity/player_nbt/README.md)
compares 33 genuine original cases: two direct writes and 31 reads, of which 11
complete and 20 interrupt at their declared source failures. The comparison
checks 1,302 normal tag nodes in 40 trees and separately brackets three LastPlayed
values. The wake93 comparison is 37 records: 33 tags plus state, Random, site and
attachment records. Those historical packets retain their original prefix
scope; they do not qualify the subsequent complete sleeping-reader work.

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

This is a finite client method/save/attachment closure. Complete
frontend startup and World/Player gameplay, server gameplay, all concrete
Entity/Chunk save descendants, unobserved floating-remainder cases, original
mutable-tree/OOME/file-exception parity and global stack bounds remain unfinished.
