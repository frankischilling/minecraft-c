# Ladder neighbor changes

The client neighbor dispatcher now reaches the actual Ladder body rather than
the previous dependency refusal. Its authority is client `dp.b(fd,IIII)V`,
139 Code bytes with SHA-256
`d861b7c1ad99cb7816bd130ae59ce56aa6a0894439504e923dcf0ff25ac2f443`
in the verified original archive. The server `cg` arm remains separate.

The first World metadata read occurs at bytecode 5. The method compares the
full metadata value before any support query; it does not mask orientation bits.

| Metadata | Actual support coordinate | World.h call |
|---|---|---|
| 2 | x, y, z + 1 | 26 |
| 3 | x, y, z - 1 | 48 |
| 4 | x + 1, y, z | 70 |
| 5 | x - 1, y, z | 92 |

Only the matching row reaches a support query. Every other metadata value is
unsupported even when all four walls are present. Coordinate arithmetic wraps
as Java integers. The support operation keeps its existing Block.m capture
before the acquiring World ID read.

Support loss reaches the inherited drop at 114 before the notifying air setter
at 123. The drop keeps the Ladder's actual constant quantity one, ordinary drop
selection, Item construction and World/Chunk publication. World B skips that
inherited drop body; it does not skip the Ladder's metadata/support checks or
air setter. The setter's ordinary Boolean is discarded at 126. The final
invokespecial at 135 reaches the base neighbor method's literal return.

Failures preserve completed effects. A drop can remain published when the later
setter or notification fails. The flat mutation diagnostic keeps `dp`, the
neighbor descriptor and its reached call site alongside the actual lower
query, drop or mutation cause. A null World reaches the metadata invocation
at 5 and sets `ladder_source_null`. Reached child scratch uses the existing
heap diagnostic pattern rather than accumulating large recursive stack frames.

The [original collector](../../tests/reference/world/ladder_neighbor/README.md)
executes twenty independent cached-World calls with unchanged game classes:
all sixteen metadata values with four stone walls, then each missing matching
wall. Its World B fixture excludes Item construction and compares return,
block/metadata, dirty state and unchanged World Random. Notification counts
remain separate because prepared lighting differs between the two fixtures.

The native `world.ladder` fixture also exercises ordinary Item publication,
drop-before-removal callbacks, failure after the air write, actual null metadata
and null World ordering. These are finite contracts. The complete recursive
mutation execution join and unrestricted Ladder interactions remain separate
work. An owned mutation caller still needs that join through these descendants.
