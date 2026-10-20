# Dimension temperature arrays

`mc_simulation` implements the concrete normal and fixed temperature methods in
`src/world/dimension/temperature.c`, with the interface in
`include/mc/world/dimension/temperature.h`. The receiver is an actual retained
Dimension biome manager. Its embedded noise, fixed value and auxiliary buffer
remain the same owners used by the other Dimension methods. The module creates
no World, Registry, Random, biome classifier or alternate terrain backend.

The [climate reference](../reference/biome.md) records the paired normal bodies.
The fixed overrides are client `hd.a([DIIII)[D` and server `et.a([DIIII)[D`.
The [normal population controller](population/normal.md) uses this staged
operation for its actual `w` publication and snow loop. Full natural population
and all reached descendants remain unfinished.

## Ordered effects

`mc_temperature_sample` borrows a nullable input handle and returns one owned
reference on success. Sufficient capacity returns the same handle and storage.
Null or short input creates an exact-length private array. A nonnull allocated
empty array remains distinct from null. Failure preserves the ordinary result
slot and every completed effect on existing arrays.

| Body | Reached operations |
| --- | --- |
| Normal `xv/ph.a([DIIII)[D` | Allocate output if needed at19; fill raw temperature44; fill auxiliary74; publish manager `c`77; process and store temperature192; return209 |
| Fixed `hd/et.a([DIIII)[D` | Allocate output if needed at19; read actual fixed `f`30; fill requested prefix33; return37 |

Normal raw noise clears the whole retained output span, including a larger
suffix. Only afterward does the method inspect and grow the current manager's
auxiliary `c`. If that later request fails, a reused output keeps its completed
raw writes. Normalization has not run. A newly allocated output remains private
and is released on failure. A replacement auxiliary buffer is filled before it
moves into `c`; its preceding allocation stays charged through publication.
Normal sampling leaves manager `a`, `b` and `d` unchanged.

Fixed sampling copies the actual stored value into the requested prefix and
preserves a larger suffix. It neither samples noise nor changes `a/b/c/d`.
It performs no floating-environment check because this body only copies values.

The lower numerical `mc_biome_temperatures` retains its separate atomic
preflight contract: it prepares both replacements before writing anything and
requires output storage from the current manager's allocator. Normal population
needs the staged method because its array can outlive a manager replacement.

## Array and allocator lifetime

An opaque `mc_temperature_owner` contains the output allocation ledger. Each
opaque `mc_temperature_array` owns one genuine lower Biome double buffer and one
hold on that ledger. Aliases retain the same array handle. They never copy an
owning buffer descriptor or mutable manager.

An output can be reused with a different normal or fixed manager. Its allocator
membership stays with its original output owner. Releasing a provider/root
owner does not invalidate a retained array: the last array release frees its
storage and releases the final ledger hold. Arrays retain no World, Dimension
or manager, so this ownership adds no cycle.

Passive views expose the real address, length, holds and allocation totals.
They retain nothing. Contents can be inspected or prefilled with exclusive
access, while descriptors remain private. Callers must retain an array before
keeping it across replacement or releasing its supplying root.

The caller supplies a separate local result slot. Successful reuse reserves
the result hold before writing, so it cannot fail a final retain after effects.
The population controller moves that returned hold into its actual `w` field
only at successful source store2684, then releases the preceding field hold.
A snow height query can reenter population; the subsequent source read reloads
the current field rather than retaining an earlier temperature view.

## Native bounds and errors

Dimensions and their product must fit `INT32_MAX` and the native byte span.
The normal method requires the established x86-64 binary32/binary64 floating
profile at its reached raw call44, after any output allocation. It preserves
the paired operation association without floating contraction.

Two independent retained/peak limits apply. The default direct limit is 1MiB
for the owner and live array handles. The default buffer limit is 64MiB for the
existing Biome allocator headers and double payloads. Old and new requests stay
charged while both are live. The current manager's `c` uses that manager's
existing separate budget. Stack, allocator overhead, borrowed owners and
unrelated World storage are excluded. Zero limits are valid native policy.

Errors retain the selected normal/fixed body, reached bytecode, lower raw
allocation point when attempted, native cause and completed progress. They do
not manufacture a Registry cause or Java exception. Hold/budget/FP refusals are
native controls; they do not claim Java OOME timing, GC lifetime, unrestricted
Java dimensions, arbitrary reflected array aliases or complete VM behavior.

`world.temperature_runtime` exercises this interface. Its results must be
reported separately from full controller and whole-game qualification.

## Executed verification

All 154 configured MSVC and 156 GCC/Clang cases pass with zero skips and local
JDK comparisons. Linux runs use ASan/UBSan/leak checks and fresh byte-verified
source/dependency copies. `world.temperature_runtime` passes 371 checks per host,
retaining its actual source, process streams and owned Unicode World root.

Four existing untouched-original temperature9/auxiliary6 vector pairs test exact
successful values at ordinary, negative and extreme coordinates/seeds. Fresh
normal allocations, short allocated-empty growth and larger reuse use those
original values. Fixed methods use their actual canonical 1.0/0.5 fields and
preserve supplied suffix bits. No new original JVM experiment was run here.

Native controls independently test old/new peak quotas, exact raw writes before
a later c refusal, unpublished-output cleanup, receiver/root retirement, alias
lifetime and accounting. The raw failure image comes from a separate call to
the existing lower raw-fill interface: it tests native sequencing and is not an
original parity oracle. FP controls restore the host environment after each
call. These checks do not qualify Java GC/OOME, arbitrary reflected aliases,
provider w publication, the full controller, snow reentry or natural population.
