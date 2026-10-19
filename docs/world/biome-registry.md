# Shared Biomes and reference sampling

The [Biome registry interface](../../include/mc/world/biome/registry.h) implements
the shared objects constructed by the original `kd`/`gs` class initializer. It
uses the application's existing `mc_registry_classes` owner, Material owner,
Random profile and resource source. Worlds using these APIs must share that
owner. A per-World numerical climate manager does not own another catalogue.
The [reference note](../reference/biome-registry.md) records the paired source
and finite original observations.

The [numerical Biome APIs](biome.md) remain available with their existing
explicit-seed, finite-input and preflight contracts. Their `mc_biome_id` values
encode labels; the interfaces here return actual shared `mc_biome` pointers.

## Objects and fields

Initialization constructs thirteen distinct Biomes in `a..m` order. Eight actual
classes supply their constructors, including the base class. Repeated classes
still produce separate objects. The ten declaring-class fields are exposed as
a borrowed readonly view:

| Field | Actual stored value |
| --- | --- |
| `n` | Nullable shared UTF16 String identity |
| `o` | Integer stored by `b(I)` |
| `p`, `q` | Signed Java-byte top/filler fields |
| `r` | Independent integer, initially 5169201 |
| `s`, `t`, `u` | Three distinct spawn-list identities |
| `v`, `w` | Independent boolean fields |

Names, integer setters and flags follow each original fluent chain. The base
constructor reads live `Block.v.bn` before `Block.w.bn`, narrowing them into
`p` and `q`.
These are real Block active uses: a cold registry executes its existing Item,
statistics, achievement, recipe and resource dependencies. The Biome bodies
construct no Random or World themselves; reached Block work retains its own
profile, resource and console effects.

There are 39 distinct list objects. The source constructs 135 spawn records;
Hell and Sky clear their original list contents, leaving 115 live records.
Each record holds an actual shared Class handle and an integer weight. Thirteen
Class identities recur across the Biomes, while records remain distinct.
Resolving a Class literal does not construct an entity or run its initializer.
The native arena retains cleared records until class-owner retirement; these
counts do not describe Java heap or garbage-collection timing.

After the thirteen named publications, initialization allocates one actual
4096-reference table. Its entries alias ten named Biomes; Ice Desert, Hell and
Sky still exist but are absent from this climate table. Only after filling the
table does the builder assign the actual sand byte to Desert and Ice Desert,
writing `q` before `p` for each. Ordinary objects of their same class retain the
base top/filler values unless a reached operation changes them.

## Active use and lookup

`mc_registry_classes_initialize_biomes` performs real class initialization.
`mc_biome_static_get` performs active use before loading a named cell.
`mc_biome_registry_inspect` instead reads the reached storage without starting
initialization. Inspection remains useful after failure, but a failed-prefix
view is not a successfully initialized gameplay object.

`mc_biome_registry_classify_float` first multiplies rainfall by temperature in
binary32, then executes the original ordered comparisons. It preserves Java's
NaN branches and accepts inputs outside the numerical API's finite unit
interval. For example, a NaN temperature reaches Rainforest; it is not clamped
or replaced with a default climate.

`mc_biome_registry_lookup` multiplies each double by 63, applies Java `d2i`
semantics, then uses wrapping int multiplication/addition for the table index.
NaN converts to zero; out-of-range conversion saturates. There is no floor or
unit-interval clamp. The reached array reference and index are checked, and a
valid null cell remains a null result. Native null/index statuses are distinct
from Java Throwable identities.

`mc_biome_table_rebuild` rewrites the same table and repeats the late Desert/Ice
Desert field stores. It creates no replacement catalogue or table. A cold
external call first completes class initialization and then executes the
requested builder body.

These operations require the documented strict x86-64 binary32/binary64
environment: nearest-even rounding, gradual underflow and masked traps. Native
FP admission does not move ahead of the real constructor/Block prefix.

## Direct sampling of references

The [typed sampler](../../include/mc/world/biome/sampling.h),
`mc_biome_sample_references`, joins the existing climate stages to actual Biome
active use. It uses an initialized numerical manager and returns a retained
array of shared pointers, with this original direct-method order:

1. Allocate a null or short output replacement at source operation 19, before
   Biome initialization. A sufficiently long input retains its identity,
   length and unvisited suffix.
2. Fill temperature, rainfall and auxiliary channels at 49, 81 and 113, retaining
   each completed channel publication.
3. Process each cell and store the manager's actual `a` temperature at 294 and
   `b` rainfall at 303.
4. Capture the output array/index, increment the local index at 307, perform the
   real double lookup at 314, then store that shared reference at 317.

If cold class initialization fails at the first lookup, the sampled channels,
processed first cell and transitive registry effects remain. The current
reference cell has not been installed. The result output stays unchanged;
`mc_biome_sampling_error` separately exposes the reached local array, completed
cell count, next index and nested causes. This path does not translate an
already completed enum grid into pointers or restore a detached manager copy.

The verified shape is square, with size and its product fitting `INT32_MAX` and
the native spans. Zero size materializes arrays without a Biome active use.
The numerical manager's `d` buffer is untouched. Its `a/b/c` buffers use their
existing numerical memory owner; reference arrays use the Registry arena.
The numerical square API still preflights replacements and preserves prior
state on failure. It does not acquire this sampler's partial-publication policy.

## Lifetime and unfinished joins

All Biomes, names, Class handles, lists, records and reference arrays borrow the
same Registry lifetime. Retain the Registry and its Material/profile/resource
dependencies, and the sampler's manager/memory owner, through every call and
view. Reference arrays have no separate caller-owned free operation. A saved
borrowed pointer does not itself prevent Registry destruction.

Calls require exclusive access. Ordinary Registry reentry and destruction are
BUSY for the entire typed sample, including synchronous resource callbacks.
Callbacks must also respect the manager and dependency lifetime/mutation
contracts. Native allocation bounds account for retained successful and failed
prefixes; they do not reproduce Java allocation exceptions or heap limits.
An initialization failure is cached and cannot be retried on that owner.
An ordinary later constructor failure does not poison a READY class.

These modules establish shared storage and the direct reference-sampling seam.
The remaining joins include sharing these references through World/Dimension
and fixed-manager construction, generator/provider sampling, live surface
`p/q` reads, spawning, feature placement and persistence. The bootstrap fixture
does not qualify those paths or complete game startup. See
[World construction](construction.md) and [World registry access](registry.md)
for the current surrounding interfaces.
