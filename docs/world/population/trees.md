# Tree population features

[`population/trees.h`](../../../include/mc/world/runtime/population/trees.h)
provides the complete oak, birch, two taiga and big-oak feature bodies. Each
uses the actual acquiring World, shared Registry and quiet mutation operations.
The [Overworld controller](overworld.md) now selects these bodies through actual Biome
dispatch. Full original continuation, reentry and descendant closure remain
unfinished.

| Tree | Initial height and shape draws | Placement behavior |
| --- | --- | --- |
| Oak | `nextInt(3) + 4` | Broadleaf clearance, live soil/statics, corner draws, leaves and trunk |
| Birch | `nextInt(3) + 5` | Distinct broadleaf body with species metadata 2 |
| Tall taiga | `nextInt(5) + 7`, `nextInt(2)`, then a foliage-dependent radius bound | Descending canopy and metadata 1 |
| Dense taiga | `nextInt(4) + 6`, `nextInt(2) + 1`, `nextInt(2) + 2` | Radius state transitions, later radius and trunk-shortening draws, metadata 1 |
| Big oak | Actual default Random constructor; placement reseeds its private Random from supplied `nextLong` | Retained height, node planning, leaf discs, trunk and branches |

The four small entries borrow the supplied Random directly. Initial draws
precede height rejection. Their soil comparisons and dirt writes perform
separate late static reads. Leaf candidates capture the actual opacity array
before the World query and use the captured reference afterward. Ordinary
false quiet writes are discarded; a later native failure retains earlier
Random, class, cache, block, metadata, hook and lighting effects.

Big oak is a retained owner with separate create, scale, place and dispose
operations. Create uses the constructor World's actual application Random
profile. Callers must create and scale before the separate population height
query. Placement retains its source World field before the supplied `nextLong`,
including rejected and failed placements. Rebinding retains the new World
before releasing the old one; disposal releases the last binding. The caller
must keep the constructor class/profile owner alive even before first placement.

Scale preserves an already selected height. A height scale greater than 0.5
sets the retained leaf distance to five; later smaller scales do not reset it.
Placement reseeds the actual private Random and retains the original phase
order: location admission, node planning, leaves, trunk, branches. The actual
private production definition is shared with internal inspections; the public
owner remains opaque.

The default native big-oak limit is 1 MiB. It accounts for the owner, old
published rows, temporary candidate rows and replacement rows while they are
simultaneously live. An explicit zero limit permits no allocation. Native
allocation failures retain earlier source effects and old owned rows. This
policy does not reproduce Java heap sizes or Java allocation-failure graphs.

Errors separate native admission from reached source failures and preserve
the feature method/PC, outer big-oak phase and fresh lower Registry/mutation
cause. No rollback, retry, fallback Random or substitute World is supplied.
Missing lower callback bodies remain explicit failures.

Native MSVC and sanitized GCC/Clang runs pass the combined 6,110 query,
predicate, plant, tree and geology checks on actual client/base-server GAME
fixtures. Separate membership/ordered cases pass 370/984 checks. They cover the
four small forms, big-oak geometry and reuse, constructor/global Random
prefixes, primed supplied Gaussian caches, actual private Random tails and
retained node identities, native budgets, World binding lifetimes and late
partial writes. These are source-derived native contracts.
They do not establish original/native tree equivalence or natural biome
reachability. Further original comparison and complete population qualification
are pending.

The complete configured public suites also pass on those compilers:
142/144/144 cases with zero skips. Separate original seed0 API captures execute
all five forms on both sides and return true at actual height64. Those captures
are retained observations; original/native output equivalence remains pending.

Tree arithmetic requires the documented x86-64 binary32/binary64 profile and
strict compiler settings. Big-oak `pow`, `sqrt`, `sin` and `cos` still require
separate qualification against the selected original runtime. Representation
checks and a supported floating environment do not prove that equivalence.
Complete population, synchronous callback closure and populated persistence
remain unfinished. See the [source map](../../reference/population-trees.md).
