# Client pathfinding

Client World path requests now enter the original ChunkCache, PathFinder and
Path bodies. Creature behavior stores the returned Path and follows its shared
cursor. The implementation uses genuine Mob and World objects, the existing
Block registry and the application's Math namespace.

Focused Windows MSVC Debug and Linux Clang18 ASan/UBSan runs each pass 16,995
native checks over 64 scenarios. Their original comparison checks 143,871 typed
fields and graph relations, 12,464 ordered query attempts and 273 extent
constructor checks, plus six checks of the actual failed DH constructor object.
The broader selection passes 117 Windows and 112 Linux tests, including Mob
following and loaded-client controls. Detailed results are in [test scope](../testing.md).
These results do not establish a playable client.

## Source order

The authority is the unchanged Beta 1.7.3 client JAR, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [original package](../../tests/reference/world/path/README.md) pins the
classes, Code bodies, authored inputs and observation helpers.

| Original body | Native owner |
|---|---|
| `fd.a(Entity,Entity,float)` and `fd.a(Entity,int,int,int,float)` | Retained World path call. |
| `ew` | The same ChunkCache constructor and BlockAccess methods used by rendering. |
| `fw` | PathFinder, neighbor construction, clearance and reconstruction. |
| `d` | Stable PathPoint objects with source coordinates, costs, predecessor and heap index. |
| `lf` | Path heap with the original tie and index rules. |
| `jx` and `wm` | Integer-key cache and its entry chains. |
| `dh` | Path array, length and shared cursor. |
| `ii.E`, `ii.f_`, `ii.a(Path)` and `ii.F` | Creature requests, following, assignment and presence test. |

Both World overloads floor source X, Y and Z separately. They use position Y
for the cache center. The target overload adds 16 to its range; the coordinate
overload adds 8. The exact float addition, Java conversion and wrapping endpoint
arithmetic precede the ChunkCache constructor. PathFinder construction follows
that complete constructor. Only then does the target overload read its target.
A null target therefore retains the earlier cache acquisitions.

The finder clears its heap and integer cache before reading the source bounds.
It constructs all four neighbor candidates before filtering them, in the order
positive Z, negative X, positive X, negative Z. Radius tests measure distance
from each candidate to the goal. Ordinary exhaustion can return the closest
partial path; a failed query or native ceiling remains a failure.

Clearance reads the real cached BlockAccess methods. Doors reach their metadata
and original open test. Other blocks reach the actual Block and Material
classification before Water and Lava checks. The step allowance, three allowed
clear drops, fourth-drop rejection and Y-zero point quirk remain distinct.

The integer cache deliberately uses the original lossy packed key. A hit can
return an existing Point with different coordinates. Point equality still checks
its coordinates and key. Heap priority ties, right-child selection, cache growth
and the surviving array contents after clear follow their original bodies.

## Creature following

Wandering retains the winning coordinates from ten evaluated candidates. A
successful World request stores its actual nullable Path at the reached source
assignment. Failure leaves that assignment pending. A returned null Path clears
the old field only at that assignment.

Following captures foot Y and both liquid results before clearing pitch. A null
Path or the selected `nextInt(100)` branch completes Living behavior before
clearing the Path. Other branches obtain a genuine pooled waypoint, advance the
same Path cursor, preserve the original distance and turn calculations, then
perform target facing and jump decisions. The final private float draw occurs
before testing the earlier liquid results.

Concrete attack, damage, drop, owner-follow and selected-Item descendants remain
separate reached dependencies where their bodies are unfinished. Pathfinding
does not bypass an earlier child.

## Ownership and cleanup

The retained World call holds its World, nonnull Entity operands, defining
Registry and Math through callbacks and diagnosis. Its ChunkCache owns the
actual acquired Chunk occurrences. Native budgets cover the complete live
request, including overlapping storage during growth and passive query
receipts. World acquisition storage keeps its existing independent limits.

A returned Path owns a stable Point arena. It has no World, Entity or Chunk
backedge. Two Mobs can hold the same Path and observe the same cursor. Source
Path fields have separate native hold cells; replacement retains the new Path
before publishing it and releasing the old occurrence.

Strict terminal inventory counts distinct Paths, each Mob Path occurrence and
each distinct Path's arena occurrence. It rejects unexplained external Path or
arena holds. Broader native container retirement preserves an externally held
Mob's acyclic Path and cursor. Final generic destruction drops that occurrence;
the class namespace remains busy while a surviving arena exists.

Failure diagnostics copy the selected child values before retiring working
storage. Actual producer receipts keep their documented borrowed lifetimes.
If checked cleanup refuses, the Mob retains that exact path call with a
transferred self-hold. Cleanup retries native retirement without repeating
search, assignment, Random calls or source effects. A completed source store
remains completed even if later native cleanup fails.

## Native policy and remaining validation

Limits bound request bytes and allocations, cache cells, Points, expansions,
queries, predecessor traversal and work. Explicit zero limits remain zero.
Limits do not turn failure into an ordinary null result or truncate a path.
The qualified numerical baseline is x86-64 with strict binary32/binary64
operations and Java integer conversions.

The original controls use real Worlds, authored cached Chunks, ordinary Cow objects and
the production cache. Two extreme-range inputs explicitly prepare the genuine
provider fallback alias before observation. They qualify that cached input;
unprepared extreme-range terrain generation is a different case. Constructor
entropy is observed independently, and no existing stream is rebound after
setup.

The paired comparison expands 3,810 actual objects and separately checks 24
debugger-observed FD internal graphs. It preserves classes, array slots,
predecessors, aliases and identities across before, step and after records.
The 12,464 attempts include failed calls; 12,462 calls return. Comparison covers
87,246 query words. Seven deliberately corrupted graph, class, alias, temporal,
query and scalar observations are rejected.

Independent native controls check eight request ceilings, a failed new Path
hold without source assignment, two genuine Mob owners sharing a cursor,
external-Path refusal, broader container retirement, a Path outliving its World,
and class retirement remaining busy until the final arena release. Scalar
oracles cover doors, liquids, extent boundaries, drops, the Y-zero quirk, lossy
keys, distance, heap ties, cache growth and waypoint conversion.

The cached fixture does not naturally reach WorldAccess callback failure or
partial path-call retirement failure. Those cases remain unqualified. Mob
following comparisons and loaded-client controls retain their separate scopes
in [Mob active ticks](../entity/mob-active-tick.md) and [the menu runtime](../client/menu-runtime.md).
