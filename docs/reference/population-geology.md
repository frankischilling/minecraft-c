# Ore, clay, lake and spring source map

The verified [client and server archives](binaries.md) provide the paired
feature bodies. Complete saved bytecode, including constructors and inherited
generator scale, establishes their fields, ordering and side mappings.

| Feature | Client / base-server | Selected source boundaries |
| --- | --- | --- |
| Ore | `fl` / `dr` | Constructor target6/size11; float1; candidate World query496; live stone499/502; quiet write519 |
| Clay | `ms` / `ib` | Constructor clay5/8/11 then size16; material6/water9 before float18; candidate487; sand494/497; quiet write514 |
| Lake | `dj` / `cd` | Air17/descent23; mask35; count41/double59; validation568-629; fill727; dirt822/Sky846/grass875; lava selection897-911; rim1152-1203 |
| Spring | `xo` / `pc` | Admission8-75; stone/air counts87-233; notifying write264; a=true270; m273/target277/element280; tick288; a=false293 |

The generator bases `pg` / `jm` retain no fields and their inherited scale body
only returns. Ore/clay placement has 326/322 instructions per side; complete
paired base bodies add four. Lake and spring have 668/162 instructions per side.
The comparisons retain PCs, operands, constants and branch order after explicit
class/member aliases and constant-pool indices are normalized.

World ID access is `fd.a(III)I` / `dj.a(III)I`: coordinate rejection precedes
World.c at48, whose provider receiver is read before invocation6 even on a
cached hit. Quiet and notifying setters remain distinct. Springs delegate to
the real shared tick dispatcher rather than introducing a feature-only fluid
backend.

For recursive springs, scheduler `fd.c(IIIII)V` / `dj.c(IIIII)V` reads the current
ID at96 and skips a mismatched request at108, before Block.m116 and World.r139.
Still-to-flow conversion requests its flowing ID at59. A nested added tick can
already have restored the still ID before that later request. This ordering
explains the tested unchanged Random and normal null-Random mismatch path.

The [ore/clay contract](../world/population/ore-clay.md) and
[lake/spring contract](../world/population/lakes-springs.md) document ownership,
prepared native scenarios and remaining descendants. The native tests derive
invocation budgets and selected exact layouts from these source boundaries;
they supply no original output replay or complete population qualification.
