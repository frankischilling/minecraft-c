# Plant population features

[`population/plants.h`](../../../include/mc/world/runtime/population/plants.h)
exposes complete placement bodies for flowers and mushrooms, tall grass and
ferns, dead bushes, reeds, pumpkins and cacti. The implementation uses the
actual World, shared Registry and supplied Java Random. It calls concrete
[plant predicates](../plants.md) and quiet World setters.

| Feature | Attempts | Placement behavior |
| --- | ---: | --- |
| Flowers and mushrooms | 64 | Air, actual captured `Block.m` element and flower-family cast, then survival and quiet ID write |
| Tall grass and ferns | 128 | Descend through air/leaves, then the same selection and survival, with late ID and metadata fields |
| Dead bushes | 4 | Descend through air/leaves, then actual selected survival and quiet ID write |
| Reeds | 20 | Air and adjacent shared water material, height `2 + nextInt(nextInt(3) + 1)`, then survival per layer |
| Pumpkins | 64 | Air, grass and actual can-place; orientation `nextInt(4)` only after acceptance and late write ID |
| Cacti | 10 | Air, height `1 + nextInt(nextInt(3) + 1)`, then survival per layer |

Each candidate axis consumes two bounded draws and uses Java wrapping integer
arithmetic. Coordinate-only budgets are 384, 768, 24, 80, 384 and 60 draws in
the table's order. Height, orientation and reached lower World operations can
consume additional draws. Bounds of one still consume their source draw.
Tall grass and dead bushes perform descent before their first Random call.

Parameterized features retain their actual constructor integer fields. Every
reached field, named static and array element remains late, including a new
read of the requested ID after survival. The selected receiver can differ from
that later ID. Cactus and reed layers continue after ordinary false predicates
or setters. All writes are quiet, and each feature discards an ordinary setter
Boolean. Normal completion returns true even when nothing changes.

Native failure stops at the first reached operation, preserves the caller's
result and supplied Random continuation, and retains earlier class, cache,
Chunk, metadata, lighting and hook effects. Errors identify the feature PC and
its nested query, predicate, Registry or mutation cause. A NULL Random fails
at its first reached invocation without fabricating a Registry error. Missing
dependencies and unfinished lower hook bodies remain explicit failures.

All 142 configured public tests pass on MSVC and all 144 on each sanitized
GCC/Clang build. Combined query, predicate, plant, tree and geology contracts
perform 6,110 checks per host using real loaded GAME Chunks on both sides. The
plant cases exercise all
nine predicate kinds, negative and wrapped coordinates, height/orientation
branches, late nullable inputs, ordinary false writes, and failures after a
completed lower layer or a stored block byte. Separate reed/cactus trials
require two rejected lower layers followed by replacement of a nonair upper
block. Wrapped success controls preserve an actual primed Gaussian cache as
well as the Random state. All six prepared region files
must remain byte-equal after terminal disposal. The two additional files from
real provider-capacity controls must contain exactly an empty region header.
The wrapper checks the complete eight-file inventory and eight occupied slots.

These are source-derived native contracts. They do not establish original/native
plant equivalence. Reentrant changes to feature fields/statics and the complete
native admission matrix need further controls.
The [paired source map](../../reference/population-plants.md) is separate from
the pending complete normal controller, other features, callbacks and save
qualification.
