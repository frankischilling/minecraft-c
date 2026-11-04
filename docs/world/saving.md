# Live GAME Chunk saving

The source client Store writes metadata through its
[SaveHandler](save-handler.md#metadata-output) and Chunks through the same
World's loader and [shared region cache](region-output.md). The actual World
save callback follows this route. Native tests save authored loaded Worlds,
retire their owners, then reconstruct new source Worlds from those files.
[Windows Tiny menu Save and quit](../client/world-saving.md) has separate
normal-leave and session-failure/cleanup observations. Linux visible save/title
and complete gameplay retain their separate requirements.

[`saving/saving.h`](../../include/mc/world/runtime/saving/saving.h) writes one
actual committed Chunk from its owning GAME World. It uses the same Store
entry, header and arrays. Source saves create fresh root, Level, Entity,
TileEntity and Stack records from current semantic fields. Legacy native saves
keep their separate provenance and opaque-record preservation rules.

The runtime save and maintenance callbacks use this path. Legacy composition
and passive opaque builders keep their separate behavior. The [full save
route](full-save.md) also handles metadata and the actual empty named SavedData
list. Broader Player, named subtype, natural restart and complete
population/persistence qualification remain unfinished.

The [populated Chunk comparison](../reference/population-persistence.md) exercises
the legacy GAME Store path after genuine origin population, then directly reloads the four saved
records. It checks complete typed NBT and raw disk preservation in the same held
World. Full World metadata save and restart retain their separate scope.

## Source loader and output

The first session check precedes the loader's catch. The loader opens its
actual region stream at LD16 before constructing root24, Level33 and attaching
Level44. Serializer51 performs the second session check, snapshots current
fields and resets q105, then visits Entities and TileEntities. Plain NBT57
precedes explicit outer close61. Only afterward does the loader reload Info65,
read SizeOnDisk74, consume signed region growth89 and store its wrapped long
sum94. Source QJ samples its own timestamp during commit; the callback skips
the legacy transport's earlier clock sample.

Source Entity occurrences invoke the actual Boolean `SN.c` wrapper. Supported
Item, FallingSand, Minecart and all eleven client Overworld [Mob writers](../entity/mob-nbt.md)
use current semantic fields. The Mob hooks retain their Living/species order,
including Slime health reset and Wolf owner retention. Source Chest, Spawner
and Piston output uses direct subtype fields; Chest Stack values do not pass
through the legacy storage overlay.

The Mob native fixture performs actual nonnullWorld factory/import, two full
source saves and two reloads. Retained provenance remains separate from these
fresh fields. The independent legacy Mob overlay entry refuses after semantic
save rather than restoring inactive keys such as Creeper `powered`. Original
NULLWorld hook observations and native World admission have separate scope.

Unconsumed opaque Entity or TileEntity records refuse source saving before
commit. This native safety policy prevents silent data loss while those
records lack semantic writers. It preserves the already opened stream and
completed source effects. Legacy opaque preservation does not substitute for
fresh source serialization.

There is no serializer finally-close. The loader retains partial output and
owning record trees after a failure. Native cleanup ends the codec before
freeing input it may still borrow, without emitting pending bytes. A failed
attempt blocks further loader reads and saves while permitting passive
terminal inspection and native retirement. Reached exception-printing calls
remain explicit diagnostic dependencies.

A full-save session mismatch precedes metadata and Chunk writes. The known
`fm.b:42` native exception-allocation prefix can reach checked terminal
retirement while its actual FileInputStream remains open and source-unclosed.
The [Handler contract](save-handler.md#retained-session-mismatch-and-terminal-retirement)
limits admission to that exact inactive graph with matching File/namespace
identities and one stream hold. Extra FileInputStream or Handler holds return
BUSY without consuming the retained prefix. Native retirement does not invent
`us`, a source finally-close or a resumed save. Both hosts pass the actual
external-hold/release/retry control and preserve metadata, regions and Info size.

## Source order and effects

The paired Region save invokes the World session method before calling the
shared Chunk serializer. That serializer invokes the session method again,
reads the current World time and Chunk fields, then resets q at105. It walks
the actual Entity slice array and lists before reaching TileEntity Map values224.
The native coordinator performs both session checks, reads the reached nibble
descriptors and follows that q/Entity/TileEntity order.

Empty live Entity slices produce an explicit empty Entities list. Every actual
occurrence sets q=true at167 before its virtual Boolean serializer183. Class
name resolution precedes the dead test. False skips append192 and continues;
duplicates call the serializer again and emit independent records. The order is
ascending current slice index followed by each selected List's iterator order.
q can remain true when every result is false and the saved list is empty. It is
never inferred from retained NBT or treated as serialized-record count. A failed
NULL Entity insertion can leave q=true with empty slices; a later save resets it.

Null e/f/g descriptors fail before q changes. Null source m or l fails after
the reset at its reached receiver. Null byte-array projections remain available
to the later native tree/codec validation instead of being replaced with the
canonical owned arrays. Nonnull spans must remain readable for their fixed
native extents throughout the exclusive call.

The TileEntity builder serializes actual Map occurrences and physical Chest slots,
including shared values and slot35 in a freshly constructed 36-slot Chest.
Loaded Chests retain their source 27-slot read behavior. Spawner name precedes
Delay; a null name fails at its concrete save12. Earlier q changes survive a
later TileEntity, budget, codec or transport error. See [live TileEntity owners](tile-entities.md).

Successful transport publishes actual region growth and adds its Java signed
int value to the selected World Info's SizeOnDisk with wrapped long addition.
It does not update another World's Info or substitute convenience Store metadata
for a GAME binding. Lifecycle still owns last-save-time and dirty transitions.
No failed save restores those earlier effects or reports successful growth.

## Source progress ownership

The concrete client Source full-save join uses the private
`mc_world_runtime_save_stages_detailed` entry. It passes only the actual callback
owner and LEVEL/CHUNKS stage to the existing LoadingScreen callback; that
callback never consumed a mutable save context. A synchronous stack bridge
copies only the per-call progress binding and shares the unchanged full-save
lifecycle. Session, metadata, SavedData, Chunk work, loading calls and failure
prefixes keep their order. NULL disables progress for this call, including any
configured callback; it does not select a fallback.

This trusted entry accepts GAME Worlds. COMPOSITION remains excluded because
its auxiliary callback can receive the context. The public progress API and
configured-options bridge still export the actual mutable cache before calling
external code. A public raw borrow from inside a private stage callback also
discards the optional owned index permanently; retained aliases stay mutable
and later calls retain generic validation.

Nested calls retain their own stack bridge and diagnostic frame, then restore
the outer frame, active holds and mutation scope. No runtime context or option
is rebound. Callback data and result remain live through return; the result
can be a subobject of its owner, as in the actual Source save record. The
runtime and result remain disjoint. Existing Source success/failure ownership
of the captured LoadingScreen and terminal cleanup is unchanged.

Six frozen Windows native profiles pass 378 checks: completed full save, LEVEL
and CHUNKS failure, NULL callback override, nested save/read restoration and an
actual raw export with retained-alias mutation. The first completed save has
63 native checks; its original decoder failure is retained. A separate audit
of those same bytes requires exact authored Compound10 empty Entity/TileEntity
lists before writing and canonical Byte1 empty lists afterwards. All nine other
checked Chunk fields and unwritten record bytes stay exact. The remaining five
profiles pass 61/63/60/65/66 checks. The public projection now separately
passes Windows and the current Linux sanitizer focused cohort. Current Windows
selection, farming, Food and Dye loops also pass their bounded save cycles.
A separate actual seed777 Create World SDK run passes 2,616 checks, including
initial save, ordinary leave, independent NBT readback and distinct-epoch
reselection. Its guard and monitor record zero changes. Full broad qualification
remains pending; these controls add no original whole-save or FPS result.

## Ownership, composition and diagnostics

One embedded Store save scope owns one real exact-identity Chunk lease. Nested
begin on that entry is BUSY. Scope cleanup releases only that lease and performs
no source validation, Map clearing, source disabling or terminal retirement.
Failed imports and terminally retired containers remain ineligible for saving.
An inactive completed Chunk can still retire after an ordinary unload while
its World continues.

The legacy lower layer composes explicit live Entities and TileEntities over the
same stored baseline, preserving supported unknown root/Level children. It
never calls simulation code. The actual TileEntity builder preserves the documented
TileEntity/Stack provenance and consumed-record merge; an owned count-zero Map must
still use that builder. Deleted recognized records cannot be resurrected by
copying the old list. Generic opaque editing restrictions remain intact.

Each detailed lifecycle call installs a fresh stack diagnostic frame. Nested
acquisition and lifecycle calls suspend and restore their predecessor's frame.
A save error carries the complete Entity or TileEntity/Stack/NBT child, separate Region/shared
serializer positions, and complete lower composition/encoding diagnostics.
Entity slice/occurrence indices, native Entity-list assembly and lower codec
causes remain distinct. Native owner/buffer limits do not invent a codec failure. Success clears the
whole returned result; no error is reconstructed from ambient last_error.

## Limits and validation

The Entity collector charges its live record trees/nodes and final List scratch
while copying the independent Entity list. Each storage serializer also charges
its current Entity/Stack/provenance graph during that call. The TileEntity builder
deducts the already live Entity tree before accounting for its Chunk/TileEntity/Stack
graph, planning scratch and independent TileEntity tree. The Store codec scope then
charges both retained list trees, the independent complete output tree and all
overlapping pointer/codec/raw buffers. Retained baseline documents, journals,
World/class owners and region/compression
owners keep their separately documented budgets. This is not a process-wide
or whole-graph heap guarantee.

The legacy independent result and buffered checked replacement do not reproduce the
original mutable compound allocation failures, early stream/file setup or
caught-Exception/printStackTrace behavior. The legacy runtime retains its real
wall-clock sampling position; the legacy direct API accepts an explicit transport
timestamp. These limits are distinct from source field/List order and successful
supported-domain values. Original/native whole-save replay, arbitrary array
lengths, other Entity/TileEntity descendants and complete
populated save/unload/reload qualification remain open.

`world.saving` uses four real client/base-server Worlds to cover fresh 36-slot and imported
27-slot Chests, aliases, Stack narrowing, Spawner fields, complete saved arrays,
unknown root/Level/TileEntity/Stack fields, session failure, nullable source receivers,
late composition depth failure, error freshness and count-zero Map/journal saves.
An independent bounded Python decoder checks all four final region records.
`world.game_entities` adds both-side constructed load/save/reload, slice ordering,
duplicate occurrences, false/dead results, current Stack replacement, RAW Stack
fields, unknown records and retained import/save failures. It passes 468 checks
per host. All 151 configured MSVC cases and 153 cases on each sanitized GCC/Clang
build pass with zero skips and local JDK comparisons enabled. The overlay adds
255 separate checks. Each host retains 710 selected unchanged inputs, 254 zlib
pins, 185 products and 35 captures. The independent decoder checks all twelve
final Entity-test regions. Whole-Chunk original calls are observed separately;
complete known two-Item tag lists match all three native hosts in 24 comparisons.
See the [source and observation boundaries](../reference/entity-game-storage.md).

Both installed GAME constructors currently bind BASE acquisition policy. The
same-World unload/reload checks therefore use explicit dedicated-policy native
coordinator descriptors with the actual World callbacks, collections and
references; they do not mutate the World's policy or establish a natural
dedicated-World startup route. They perform real save, detach, unpublication,
inactive Store retirement and reload without terminalizing the continuing World.
The save fixture uses identity block normalization. The separate TileEntity import
fixture's deliberate Chest/Spawner-to-air map remains an import-order control.

The default NBT depth limit is 64. A retained 61-node unknown child fits TileEntity
import and its independent TileEntity result, then fails when nested under the outer
Level. This exercises a genuine input-based composition error after q resets;
it is not an allocator failure injection or original Java OOM comparison.

Run `ctest --test-dir <build> -R '^world.saving$' --output-on-failure`.
