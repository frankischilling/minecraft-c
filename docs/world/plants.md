# Plant placement, survival and growth

[`plants.h`](../../include/mc/world/runtime/plants.h) exposes the actual Block
receiver's can-place and survival methods for flowers, saplings, crops, tall
grass, dead bushes, mushrooms, reeds, cactus and pumpkins. Dispatch follows the concrete receiver
kind and proved inheritance. It does not select behavior from a numeric ID or
a detached material/property table. Other receiver kinds fail explicitly.

| Receiver | Can-place | Survival |
| --- | --- | --- |
| Flower, sapling and tall grass | Base target replaceability, then grass/dirt/farmland substrate | Plant light at least8 or visible sky, then the same substrate |
| Crops | Base target replaceability, then farmland substrate | Inherited flower light/sky test, then farmland substrate |
| Dead bush | Inherited flower placement with sand-only substrate | Inherited flower light/sky test, then sand-only substrate |
| Mushroom | Inherited flower placement with the live Block.o substrate | Y0..127, plant light below13, then live Block.o substrate |
| Reeds | Same receiver ID below, or grass/dirt beside actual shared water material | Calls its actual can-place method |
| Cactus | Base target replaceability, then its survival method | Test neighbor materials in -X,+X,-Z,+Z order; reject the first solid one, then require cactus or sand below |
| Pumpkin | Target replaceability, then actual solid support below | Proved inherited base literaltrue body |

Placement and survival are distinct methods. For example, mushroom placement
does not add survival's light bound. Reeds do not accept sand in this source
body. Flower survival does not invent a target-air or Y-bounds gate. Each source
static field is loaded only when its branch reaches it; Registry aliases and
nullable values keep their actual short-circuit behavior.

The implementation invokes the concrete [World queries](queries.md), material
getter and solid-support method. Material failures carry a fresh nested Registry
frame rather than copying an unrelated earlier World diagnostic. Mushroom
substrate captures the actual opacity-array reference before its separate
element read. This same captured-array operation is needed by tree leaf checks.

The World and its bound Registry remain live throughout each call. Ordinary
Boolean results and native failures are separate. Failures preserve the caller's
result and earlier source effects, including acquisition and the shared positive
sky-light flag. Errors identify the deepest predicate, its immediate predicate
caller, and fresh World-query, material, support or Registry causes.

The source mapping is [documented separately](../reference/plant-predicates.md).
The [six plant feature families](population/plants.md) invoke these predicates
and actual quiet World setters. Native predicate trials cover all nine receiver
kinds, placement/survival branches, each reed/cactus neighbor direction and late
nullable predicates. Their earlier verification scope is separate from the
lifecycle work below.

## Synchronous callbacks and ticks

The [plant callback subsystem](../../src/world/runtime/mutation/plants) implements
the inherited flower neighbor/survival body, inherited flower/tall-grass/dead-bush
ticks, and the sapling, mushroom, crop, reed and cactus overrides. Generic Block
neighbor and tick dispatch invoke these actual bodies.

Unsupported plants read current metadata, execute their concrete drops, then
remove themselves through the notifying setter. No current-ID preflight is
added. Cactus and reed columns can therefore remove recursively across Chunk
borders. Drop, setter, notification and acquisition failures retain earlier
changes; a failed drop leaves the plant in place.

Reeds and cactus preserve age increments, the height-three cap, upper-air
checks and the two notifying writes at age15. A false upper write still reaches
the age reset. Neither tick uses its supplied Random or runs survival first.
Mushrooms retain both candidate checks and the two extra Random draws whose
updated source locals are otherwise unused.

Crops run inherited survival before the subclass growth logic, even when that
helper removed the crop. Growth uses the distinct sky-subtracted
[block-light query](queries.md), fresh eight-neighbor reads, the source-ordered
farmland scan and binary32 growth arithmetic. Saplings alone check the remote
World flag before survival. Their stage bit, species selection and selected
tree bodies use the same supplied Random. Each big oak receives a fresh owner
with constructor leaf distance4; no population scale call runs. A rejected
tree restores the sapling quietly. A native failure retains its partial state
without that restore.

Farmland neighbors test the actual above Material and convert covered soil to
dirt. Its tick scans for water, then checks actual World rain, updates moisture,
and preserves dry soil beneath a crop. The separate step-on body consumes World
Random and ignores its Entity parameter. Existing Entity movement does not yet
dispatch walking effects; inside contact remains its distinct empty overload.

The [lifecycle source map and validation](../reference/plant-lifecycle.md) separate
native GAME contracts from original dependency-fixture comparisons. Complete
natural population, general tick scheduling, Player harvesting and whole-game
acceptance remain unfinished.
