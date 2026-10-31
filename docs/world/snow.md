# Snow support and melting

The [concrete snow module](../../src/world/runtime/mutation/snow.c) implements
the paired snow-layer support, neighbor, private survival and light-melt
methods. They use the actual World, Registry, Chunk and notifying setters.
Player harvesting, collision-box pooling, shape updates and rendering remain
separate operations. The [source map](../reference/snow.md) records the method
descriptors and reached bytecode sites.

## Support and synchronous removal

Support first reads the block at the wrapped coordinate `y-1`. Air returns
false before Block initialization or array access. Nonair captures the current
Block array, selects that ID and invokes its actual virtual opacity predicate.
The cached opacity array and the World solid-support method are different
operations. Leaf and slab receiver fields therefore remain live.

Only an opaque receiver reaches a second, independent World material query at
the same below coordinate. Its returned Material must receive `c()`. A null
Block or Material fails at its reached invocation; a nonopaque Block skips the
material query entirely. The first selected Block's material is not reused.

The neighbor method ignores its source-ID argument. Its private survival call
returns immediately when support is valid. Otherwise it reads current metadata,
executes the inherited final drop method and clears the position with the
notifying setter. Snow's actual quantity override returns zero. This route
creates no snowball, Stack or Item Entity and consumes no Random draws. The
player-harvest overload has a different spawn sequence.

The clear discards either ordinary setter boolean. WorldAccess callbacks and all reached
neighbors still execute in source order. Recursive snow removal can continue
up a stack; a later unsupported neighbor or native resource failure retains
earlier block, metadata, height, queue and dirty effects. There is no rollback,
implicit retry or extra gameplay recursion limit.

## Light tick

The tick first reads the actual shared Block-light type, then calls the World
saved-light query. Only a value greater than eleven reaches the metadata read,
inherited zero-quantity drop and notifying clear. Sky light, combined plant
light and a newly computed propagation result are not substituted. The tick
does not run the support predicate first and ignores its Random argument,
including null.

Errors retain distinct neighbor, private survival, support and tick sites,
full material and saved-light children, and the current/deepest concrete hook
projection. An explicitly returned saved-light population context propagates
with its actual child. This finite diagnostic representation is not a source
call-stack owner or evidence for arbitrary recursion resources.

## Verification

The native `world.snow` case uses both Registry sides and real GAME owners with
serialized input chunks. Its capture retains inputs and streams; saved regions
are fixture inputs, not final-state observations or original-game comparisons.
The focused case passes 380 checks on MSVC and sanitized GCC/Clang. It covers
live opacity fields, the independent material query, null dependencies, ignored
Random/source-ID arguments, zero-quantity ownership/RNG effects, recursive and
partially failed removals, remote melting, Block-light eleven versus twelve,
and wrapped/out-of-World coordinates. All 159 configured MSVC cases and 161
per sanitized GCC/Clang build pass in focused/complementary partitions. Fresh
qualification verifies actual source, dependencies, products, captures and the
Snow executable/process/stream joins. No original snow replay, player harvesting
or complete population claim follows.
