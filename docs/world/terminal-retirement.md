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

The public function grants no exemptions for client owners. A private adapter
can supply typed addresses of actual owned Entity and World slots. Mirrors,
borrowed fields and guessed hold allowances are not anchors. That client
adapter and the complete outer coordinator remain unfinished.

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
proof. Cleanup uses checked native operations and discards static references
only after all TileEntity owners end.

The test's voxel and `World.L` branch values are declared source inputs. Its
Piston List append is a native ownership control, not an original push or tick
comparison. No native phase, hold count or readiness marker is assigned to make
the graph pass. The test has no original-runtime equality claim.

The [client terminal prefix](../client/terminal-retirement.md) still needs its
actual owner adapter, effect/controller/service admission, dispatcher consumer
release and passive LL/TH retirement. World admission alone does not authorize
SaveHandler or App destruction. Normal shutdown, deferred old-World ownership
and live N44/N80 adoption remain unfinished.
