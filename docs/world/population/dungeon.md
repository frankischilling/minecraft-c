# Dungeons

[`population/dungeon.h`](../../../include/mc/world/runtime/population/dungeon.h)
provides the complete stateless placement body and direct calls to its two
private-helper implementations. The [paired source map](../../reference/population-dungeon.md)
records the original descriptors and source positions. Native integration
and the finite original helper comparisons below pass. Natural population
and arbitrary-World closure remain unfinished.

The [prepared whole-placement procedure](../../reference/dungeon-placement.md)
records a separate complete-call observation and exact comparison boundary.

Placement uses the actual GAME World, bound Registry, acquiring Chunk paths,
supplied Random, notifying mutation methods and concrete TileEntity owners. It does
not construct a second terrain backend or suppress unavailable descendants.
Validation rejection publishes false. Completed placement publishes true,
including ordinary false setter results. Native failure leaves the result
untouched and retains all completed World, class, Random and container effects.

The feature holds its World through synchronous calls. Chest lookup publishes
a hold on the actual nullable TileEntity; the feature keeps that identity throughout
all eight loot calls. Each new Stack starts RAW before its source arguments are
evaluated. Successful slot assignment retains that same Stack, releases the old
slot hold and performs actual dirty behavior. A late failure retains slot and
dirty prefixes; only feature-local holds are released. No copied loot owner or
new provenance is introduced. The Spawner path similarly retains its TileEntity and
the actual shared Registry-backed name handle until assignment returns.

The optional per-call native budget counts successful fresh loot Stack
allocation requests, even when those objects are later released or replaced.
Admission occurs before each source NEW and before its Item/count arguments.
Default limits admit all sixteen possible allocations. Zero still permits
validation rejection and nonallocating NULL-loot branches. This is a separate
feature-local request bound: existing World/Chunk/TileEntity/Stack/name graphs,
Registry initialization, synchronous descendants and allocator overhead are
excluded. It is not a combined retained-World bound or Java heap equivalence.

Errors preserve separate placement, loot, mob, air-wrapper and Item constructor
positions, the active Chest group/trial/fill, and complete typed causes from the
material, mutation, TileEntity, Registry and Stack interfaces. Local NULL Random or
Material failures do not fabricate an unreached Registry constructor error.
Ordinary success clears the attempt's error; partial source effects remain.

## Executed checks

All 153 configured MSVC cases and 155 cases on each Linux GCC/Clang build pass,
with zero skips and local JDK comparisons enabled. Linux uses ASan/UBSan and
leak checks on fresh byte-verified source/dependency copies. The Dungeon case
passes 3,655 checks per host; ordinary Item runtime access passes 292. Each
host retains 723 selected inputs, 254 zlib pins, 191 products and 36 captures.

The Dungeon suite constructs 18 independent epochs with 72 real World/Store
owners. It covers all four horizontal radius combinations, opening admission
and floor/roof rejection, mixed masonry, unchanged roof, real Chest replacement,
Spawner names, Gaussian-primed Random continuation, nullable/type/late-failure
prefixes and the separate cumulative Stack request budget. The successful
case saves, unloads, retires its Store Chunk and reloads through actual GAME
loading. Its driver independently reads all 36 regions and both saved Dungeon
records. These are prepared native scenarios, not naturally generated rooms.

Six untouched-original helper captures complete on the selected Windows JDK17:
one cold context and two explicit Block-initialized contexts per side. Each
executes 77 helper calls. Cold initialization failures remain negative
observations. Each prepared context has 75 normal returns and two NULL-Random
controls. Across all three native hosts, 924 comparisons match complete Stack
fields, exact UTF16 mob names and all five subsequent Random draws against
both original repeats. Helper comparisons exclude class-context/World identity,
wall-clock/default-Random effects, whole placement and native/JVM error text.

The separate [whole-placement comparison](../../reference/dungeon-placement.md)
captures all 24 prepared room cases per original side. Each side has eight
true placements and sixteen validation rejections. All 144 matching native
comparisons pass across MSVC and sanitized GCC/Clang, including complete arrays,
independently captured actual Map keys, all physical Chest slots, ordered Map/
World TileEntity/light queues, Spawner values and five consuming Random draws.
The passive `mc_live_tile_entities_map_entry_at` inspector exposes actual immutable
local-coordinate keys separately from mutable TileEntity positions without retaining
or changing the borrowed Map value.

The [Overworld population controller](overworld.md) now invokes the actual Dungeon
body in its complete schedule. Concrete WorldAccess receivers, several old-block/neighbor hooks and other explicitly unsupported
descendants still prevent arbitrary-World closure. Prepared solid-room native
tests and direct untouched-original helper experiments retain that scope.
They do not establish natural cache-triggered population, dedicated
startup, full populated persistence or whole-game parity.
