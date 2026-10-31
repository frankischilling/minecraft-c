# Falling blocks and FallingSand ownership

The [concrete Block bodies](../../src/world/runtime/mutation/falling.c) implement
the inherited sand and gravel addition, neighbor and tick methods. The
[Entity constructors](../../include/mc/entity/entity.h) allocate actual
FallingSand instances. Later Entity movement, collision, landing and dropping
remain unfinished; executing a Block tick does not establish those behaviors.

## Source order

Addition and neighbor hooks read the receiver's current block ID, invoke its
inherited delay of three, then call the actual [World scheduler](scheduling.md).
The neighbor's supplied source ID is unused. The scheduler consumes a record
sequence even for a duplicate or absent request. Its immediate mode invokes the
concrete tick; deferred mode retains its separate membership and ordered-list
effects. The shared falling flag and the World immediate flag are distinct.

The tick ignores its Random argument, including NULL. It queries the block
below before checking whether Y is negative. Its predicate accepts air first,
then the actual live fire Block identity, then the selected Block's Material
against the actual water and lava identities. A NULL Material ordinarily
compares unequal; a NULL Block fails at its reached field read.

With the shared flag true, or a missing chunk in the nonloading radius of 32,
the tick clears the original position through the notifying setter. It queries
below the current Y before each positive-Y test, then decrements Y. A positive
final Y receives the receiver's late block ID through another notifying setter.
At Y zero, clearing can complete without placing. Ordinary setter booleans are
discarded; a reached native failure preserves earlier writes and notifications.

When the whole region is present, allocation precedes the constructor argument
arithmetic. Each integer coordinate converts to Float, adds `0.5f`, then widens
to Double. The real full constructor receives those coordinates and the late
receiver ID. [World publication](entities.md) inserts that same Entity; its
ordinary boolean is discarded. This branch does not clear the source block.

## Concrete Entity and persistence

FallingSand has its own carried-ID and age integers, independent of Item fields
and Stack ownership. Both constructors call the base constructor once, using
its actual ID, private default Random, watcher, bounds and counted World
binding. They consume no Item Math draws. The world-only overload resets age
and retains base geometry. The full overload sets size to `0.98f`, derives
offset from the actual height, positions the same bounds, clears velocities
and stores previous coordinates.

An immutable canonical Class literal selects constructor and NBT dispatch.
Base Entity equality still compares the same class owner and ID, without a
concrete-subtype equality gate. Item-specific APIs reject FallingSand.

The [semantic NBT methods](../entity/nbt.md) write the seven common base fields
and `Tile`, narrowed to Byte. Reading masks `Tile` with 255. Age is not saved;
the factory's real world-only constructor resets it before semantic read.
The named wrapper uses `id=FallingSand`. The independent provenance owner
preserves unknown Entity tags without requiring or constructing a Stack.

## Diagnostics and remaining work

Errors retain separate tick, private-fall and predicate PCs, whole constructor
and publication children, and the current/deepest explicitly carried hook.
A passive scheduler prefix keeps scheduler status, source sites, queue causes
and Registry diagnostics beside its actual tick child. This finite projection
does not retain every recursive frame or impose a gameplay recursion limit.
Native allocation ceilings and strict floating environment admission remain
distinct from original JVM failure timing.

The native contracts are [Entity construction and NBT](../../tests/entity/test_falling.c)
and [Block/runtime ownership](../../tests/world/runtime/test_falling.c).
They use genuine owners and explicitly prepared native inputs. They do not
establish original-game replay, arbitrary recursion, full natural population,
FallingSand Entity ticking or complete save interoperability.
