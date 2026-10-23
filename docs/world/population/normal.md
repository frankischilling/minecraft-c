# Normal population controller

The normal provider's complete population schedule lives in
[`normal.c`](../../../src/world/runtime/population/normal.c), with its typed
interface in [`normal.h`](../../../include/mc/world/runtime/population/normal.h).
The installed GAME cache adapter calls it on the actual current provider.
COMPOSITION keeps its separate callback interface. The cache's existing
candidate order, leases and population flags are unchanged.

The [paired source map](../../reference/normal-population.md) identifies the
original controller and its reached operations. Implementing this controller
does not close every feature descendant. [Snow support and melting](../snow.md)
now execute their complete concrete methods. Later [FallingSand Entity ticks](../falling.md),
nonempty observers and other unsupported operations remain explicit failures.
The [fresh World comparison](../../reference/natural-population.md) now covers
the seed-23 origin neighborhood, full retained Chunk arrays, ordered light
regions, provider numerical state and copied Random continuations on both
sides. Other natural terrain and arbitrary recursive resource behavior retain
separate acceptance requirements.

## Schedule and shared state

The controller uses the provider's actual World, terrain-noise objects and
embedded Random. It reads the current Dimension manager for the initial Biome
point, retains that actual nullable identity, then performs the source seed
mixing. The two odd longs use signed truncating division by two followed by
wrapped multiplication and addition. Both World seed reads remain separate.
There is no copied Random, alternate generator or restored continuation.

The full schedule includes optional water/lava lakes, eight Dungeons, ten clay
attempts, all eight ore groups, Biome-dependent trees/flowers/grass/dead bushes,
optional flowers/mushrooms/pumpkins, reeds/cacti, both spring loops and snow.
Every feature receives the same provider Random. Ordinary feature false
continues; a native failure stops with the reached prefix intact. Blocks and
their full signed IDs are loaded at each reached static/field operation.
Biome comparisons load the actual current statics independently.

Tree selection dispatches through the actual concrete Biome kind, including
the source's inherited base methods. Each selected big oak is constructed
fresh before scaling and the separate acquiring height query. Its actual
default-Random/profile effects precede height reentry. The owner is released
when that iteration exits, preserving its completed effects.

## Falling flag and temperature ownership

[`falling.c`](../../../src/block/registry/falling.c) accesses the existing
Registry `gk_a` cell through actual declaring-class initialization. The controller
writes true first and false only on ordinary completion. Failure has no finally
or restoration. A successful nested controller can therefore leave false while
its outer controller continues. Accessing this flag does not implement falling
physics or its Entity constructor.

The actual provider starts with nullable `w` and an independent lazy
[temperature allocation owner](../temperature.md). It captures the current
manager and `w` at the reached temperature call. Only a successful return moves
the returned array hold into `w`; the preceding field hold is released afterward.
Reused-array writes survive a later failure. External aliases survive field
replacement and final provider release through their own holds.

Each snow top-solid/liquid query precedes a fresh read of current `w`. Adjusted
temperature is evaluated before height guards. The first material query invokes
the actual material predicate; the second is a separate fresh query before the
late ice-static comparison. Snow uses the notifying setter. No can-place
preflight or quiet-write substitute is installed.

Provider reservation counts its actual added fields. Temperature defaults
separately bound direct owner/handle requests to 1MiB and buffer requests to
64MiB. Manager buffers, feature budgets, pending history and World allocations
retain their own bounds. These do not form a combined Java heap or C-stack bound.

## Failures and cache effects

The cache marks `terrain_populated` before calling the provider, and marks the
target dirty on ordinary success. Feature writes can mark chunks dirty before a
later failure. Neither layer rolls back flags, bytes, RNG, arrays or callbacks.
A later cache hit does not rerun an already marked candidate.

Direct controller errors retain common source locations and one selected typed
child in a union. Only the tag's selected child is valid. The acyclic
[`population/context.h`](../../../include/mc/world/runtime/population/context.h)
projection carries the current controller and deepest explicitly propagated
controller site, with the actual storage/Registry cause. Each synchronous
acquisition owns a fresh local diagnostic and restores its predecessor on return.
Mutation, query, height and light paths carry that explicit value when reached.

Status-only block/material interfaces retain their finite storage causes and
omit typed population context. The block getter now selects its cause from its
actual acquisition attempt. It does not recover a missing child from ambient
World diagnostics. Intermediate recursive frames and discarded children are not
reconstructed. The diagnostic representation imposes no gameplay recursion cap;
arbitrary recursive C-stack/resource closure remains unqualified.

## Verification scope

`reference.natural_population` constructs fresh client/base-server Worlds with
the actual supplied resources and calls the real acquisition route. Its
[finite evidence record](../../reference/natural-population.md) distinguishes
that complete boundary comparison from the prepared native contracts below.

All 156 configured MSVC cases and 158 on each sanitized GCC/Clang build pass,
with zero skips and local JDK comparisons. The controller case passes 2,219
checks per host; the shared flag case passes 408.

`block.registry_falling` exercises both Registry sides, real cold initialization,
shared warm access, cached first failures and actual entered-resource BUSY calls.
`world.normal_population` uses genuine client/base-server load-or-create Worlds
with four serialized all-air chunks, all initially unpopulated. The fourth cache
publication reaches the real controller. It checks full chunk images, target
flags, actual `w` and ledger identity, stable cache hits, cold resource failures,
leases and retained temperature aliases after owner retirement. Its retained
driver independently inspects all eight final saved Chunk slots.

At the preceding controller checkpoint, generated-terrain activation reached
gravel placement at controller PC664 and the unfinished falling-addition hook.
The falling Block bodies now implement that hook and its complete tick dispatch.
The [snow support and neighbor bodies](../snow.md) continue the prepared
generated-terrain regression through the whole controller. It now requires
ordinary successful acquisition, actual snow cells and the final falling-flag
reset. Cache and explicit populate hits preserve the marked candidate without
another generator or cave Random advance. See [test scope](../../testing.md)
for the exact source and qualified platforms.

These prepared native scenarios do not establish natural terrain population,
all Biome selectors/gates, exact whole-controller RNG continuation, nested field
replacement, original save replay or whole-game acceptance.
