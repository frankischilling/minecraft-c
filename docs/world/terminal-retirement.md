# World terminal admission

`mc_world_runtime_terminal_admit` checks whether a native World graph has only
the owning references that its terminal coordinator plans to release. It does
not release them, mark the World terminal, invoke source methods or save files.
Success is an inventory for the current exclusive graph, not a cached permit
for later destruction. The caller must keep that graph unchanged until cleanup.

This first scope accepts a READY client GAME World with its existing Registry,
Entity classes, Random profile, Store and bound WorldProvider. Entered calls,
leases, WorldAccess registrations and extra provider holds refuse admission.
The dedicated Entity index, particle subobjects and Entity riding relationships
remain outside this scope. Unsupported graphs remain live.

## Count actual owners

The audit reads the canonical owning ledgers for World `b/D/d/e`, nearby `R`
and permanent collision `K`. It visits every actual Store-owned Chunk, its
eight typed Entity slices and its embedded TileEntity Map, independently of the
source cache and replaceable Map projection. World `c/G` supply their native
TileEntity owning slots. Nonempty Chunk slices must use `mc_entity_release` as
their genuine finalizer before any payload is inspected.

Each distinct object must have exactly as many native holds as discovered
owning occurrences. Duplicate list entries count separately; aliases of the
same source collection do not add ownership. Each distinct World-bound Entity
or TileEntity contributes one direct World binding, regardless of its hold
count. The actual provider `a` contributes one more. Excess holds or unexplained
World bindings return BUSY. Inconsistent ownership metadata refuses admission.

The public function grants no exemptions for client owners. The private
[selected-load adapter](../client/terminal-retirement.md) supplies typed
addresses of actual owned Entity and World slots in its intact successful-load
scope. Mirrors, borrowed fields and guessed hold allowances are not anchors.
Failed/partial load admission and complete Format/App/service external-root
admission remain unfinished. The normal menu-run coordinator now uses this
selected-load gate after its genuine prefix and dispatcher retirement; an
independent passive classification proves when storage was never acquired.

## Paths and Point arenas

Each actual Creature Path hold contributes one occurrence. Aliased Mob fields
share the same Path identity and cursor. A distinct Path contributes one arena
occurrence, regardless of its own hold count. The audit checks exact Path and
arena holds as well as their defining Entity namespace. Extra Path or arena
owners refuse strict admission before mutation.

Returned Paths have no World/Entity/Chunk backedge. The broader native container
retirement operation preserves a surviving externally held Mob's Path and
cursor. Final Mob destruction releases its Path occurrence; a separately held
Path can outlive the World and keeps class retirement busy until its arena ends.
Active Mob calls and pending path/ray cleanup refuse before the joint commit.
Source target cycles are still pinned and cleared only after the World's
terminal admission latch. Acyclic Path fields are not cleared to make an
externally owned Mob pass retirement.

The preliminary Mob scan uses the existing same-World Store traversal scope.
Another World's Chunks in that Store remain independent owners. Actual closed
Mob graph and strict whole-World audits retain their single-World restrictions.

## Store registrations and unfinished work

Admission validates the complete Store registration, reservation and Chunk
chains before selected-World traversal. It permits only the actual World cache
and the address of that World's acquisition fallback slot. Another valid empty
cache registration still returns BUSY; it is not cleared or treated as owned.
Any reservation, entry lease, coordinator lease, open save or unfinished import
also returns BUSY, including work on generic stored entries.

Every actual runtime Chunk must name the selected World. A coherent inactive
generic stored entry with no runtime header can remain if it has no unfinished
work. Its unrelated decoded storage does not become a World Entity owner.
Final Store release keeps its existing stricter requirement that registrations
and containers have actually ended; this preflight does not perform that work.

## Selected source Handler owners

The client adapter additionally inspects the intact source World and counts
four actual Handler slots: the Load allocation, the World construction sidecar,
the source SavedData manager's `source_a`, and the source Chunk loader's `store`.
World `w`, options and callback transport fields are borrowed aliases. A direct
Handler retain, independent SavedData manager or second loader adds an
unexplained Handler occurrence and refuses admission.

Retaining the same loader increases only that loader's holds. The World source
inspector therefore checks its actual single loader-owner slot separately.
SavedData has no retain counter to invent. Raw, failed and incomplete source
Worlds remain unsupported by this selected-entry join.

The owner hooks inspect real File, stream, DataInput, compression and inflater
states without source calls or diagnostic mutation. The loader's DataInput
port borrows its compression owner; it contributes no second compression hold.
That owner must have its one actual loader occurrence before the borrowed
record bytes can later be freed. A completed RAM input is not falsely reported
as source-closed. File-backed Handler prefixes require their completed native
ownership and source-close state in this scope.

Safe independent File, Info and document retains are not extra World roots.
Shared File, logging, regions and class namespaces are kept live rather than
closed by this audit. Later native descriptor close can still fail; checked
cleanup retains its retry contract. Format and complete App/service admission
remain separate work.

## Static Piston roots

The class-owned Piston Entity List supplies its real owning occurrences. A
nonempty List additionally requires the sole live World in that class namespace
and discovery of every live TileEntity with exact holds. An unlisted RAW
TileEntity, even with a null World, blocks that proof.

Successful inventory records `piston_discard_required`. It does not discard the
List. The coordinator must first end all TileEntity owners, then use the
existing checked class discard before freeing the remaining World graph.
Multiple live Worlds remain BUSY for this nonempty static-root scope.

## Bounded, passive inspection

The default scratch limits are 65,536 distinct object, owning-slot and Chunk
records and 16 MiB of peak allocation. Growth counts both old and replacement
tables. Zero limits are literal limits. Overflow, quota and allocation failures
preserve the graph and leave the caller's inventory output unchanged. A null
limits pointer selects defaults; the error output is optional.

Inspection neither initializes classes nor adopts source collection changes.
It changes only disjoint caller outputs and temporary scratch memory. It must
run with exclusive access to live canonical owners, like the existing native
retirement APIs. It does not validate arbitrary pointers supplied by a caller.

## Native controls and remaining integration

`world.terminal_inventory` constructs a real client World and loaded Chunk on a
fresh owned Unicode path. It exercises duplicate Entity owners, real provider
and Entity retains, Chunk and Class-query leases, TileEntity Map and `c/G`
occurrences, a static Piston reference and a second real World. Each admission
attempt checks retained snapshots for mutation and preserves inventory output
on refusal. A separately retained, unlisted TileEntity exercises the all-Tiles
proof. Valid extra empty cache registration, reservation and generic-entry lease
controls cover the full Store preflight, with positive retries after actual
unregister/cancel/release operations. The test passes 1,047 native checks on each
qualified host. Cleanup uses checked native operations and discards static references
only after all TileEntity owners end.

The test's voxel and `World.L` branch values are declared source inputs. Its
Piston List append is a native ownership control, not an original push or tick
comparison. No native phase, hold count or readiness marker is assigned to make
the graph pass. The test has no original-runtime equality claim.

The [client terminal prefix](../client/terminal-retirement.md) now has explicit
selected-load controller/effect/Input checks and actual dispatcher consumer
ending with passive LL discovery and checked LL/TH retirement, plus the actual
source Handler and loader inventory described above. Full service admission
remains unfinished. World admission alone does not authorize
SaveHandler or App destruction. Normal menu-run shutdown composes the existing
checked owner cleanup for its admitted scope. Deferred old-World ownership,
partial loads and live N44/N80 adoption remain unfinished.
