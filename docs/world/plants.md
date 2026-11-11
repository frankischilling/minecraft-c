# Plant placement, survival and growth

[`plants.h`](../../include/mc/world/runtime/plants.h) exposes the actual Block
receiver's can-place and survival methods for flowers, saplings, crops, tall
grass, dead bushes, mushrooms, reeds, cactus and pumpkins. Dispatch follows the concrete receiver
kind and proved inheritance. It does not select behavior from a numeric ID or
a detached material/property table. Other receiver kinds fail explicitly.

[Ordinary client placement](plant-placement.md) now connects the eight
flower-family Blocks through the retained Block and World placement entries.
It shares the substrate body with this runtime while preserving independent
invoking-Registry identity and three reached World-query diagnostics. Two
Windows Sapling/Flower scenes separately check ordinary input and save/reload.

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

## Owned Flower neighbor during Player wake

The retained wake execution now reaches the same ordinary Flower neighbor and
final survival helper. The empty base neighbor returns before `wb.b:17` enters
`wb.h`. That helper calls virtual survival at 6, reads current metadata at 23,
publishes the inherited drop at 26, and invokes notifying removal at 35.
The air setter's ordinary false return is discarded. Successful support does
not draw Random or publish an Item.

The shared survival body keeps plant light `fd.m` at `wb.g:5`, conditional
sky visibility at 18, the below-cell ID at 32 and live virtual substrate at 35.
Plant light passes literal subtraction zero to the actual Chunk body; World
field `f` is recorded separately as sky subtraction. Positive real sky light
sets the defining Chunk class's shared flag. No target-air or Y gate is added
to Flower survival.

Owned query and plant frames borrow the exact evaluated World token, defining
classes, code and canonical result. Every reached acquisition keeps its actual
lower diagnostics and raw return independently of native continuation. A
native refusal stops before payload/output or the next Source operation. The
mutation call retains failed frames through canonical drainage and checked
close. A legitimate Source failure can retain a FAILED execution phase; it
does not request the OPEN-only continuation lease again. Successful lower
returns still require exact token/lease checks before another operation.
Nested callers preserve a genuine lower World cause when their own projected
detail remains empty.

The [original packet](../../tests/reference/entity/flower_wake/README.md) records
supported Grass, unsupported Air and an Item-observer exception. Native
comparison selects 34, 57 and 49 values respectively, including actual light,
height, floor and final Chunk flag. Wider Chunk planes, constructor entropy,
Gaussian caches and original stacks remain unpaired. Direct native controls
also cover independent scope, missing Math, later cell-observer failure,
low-light visible sky and low-light hidden sky. The hidden case checks the
real range notification before the height store. Separate actual query
controls retain cold plant-light and cold sky-visible failures, including
Source exceptions and outstanding descendants. Real Arrow cases preserve
different publication/removal prefixes through all five native unwind phases.

Other owned plant kinds, growth and addition/tick callers retain their separate
frontiers. These prepared calls do not establish natural growth, graphical
sleeping-Player damage, original audiovisual equality or an FPS change.

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
