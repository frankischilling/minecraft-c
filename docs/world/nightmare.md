# Sleeping-Player nightmare spawning

The nighttime branch is the full 654-byte unchanged-original `cq.a(fd, List)Z` method. `fd.l()` reaches it after the sleeping-player check when natural hostile spawning is enabled and difficulty is at least one. A true result suppresses that tick's dawn alignment and weather clearing; normal tick continuation still follows.

CQ constructs one direct-World `fw` before reading the passed List iterator. It captures the current final three-element Class array for each Player. The default elements are Spider (`cn`), Zombie (`uz`) and Skeleton (`fr`). For each Player it tries up to 20 candidates. Each attempt draws X and Z as differences of two `nextInt(32)` calls, Y as a difference of two `nextInt(16)` calls, clamps Y to 1–128, then draws the class index. The selected array element is read later, after descending to supported terrain and scanning for the shared land-spawn predicate.

The packet pins these original call boundaries and arguments:

| CQ bytecode position | Reached original operation |
| --- | --- |
| 7 / 12 | `fw(xp)` with the World once, then `List.iterator()` |
| 92, 102, 122, 132, 152, 162, 200 | World Random bounds 32, 32, 32, 32, 16, 16, array length |
| 224 / 246 | World support test, then shared `cq.a(lk,fd,int,int,int)Z` land predicate |
| 336 / 347 | selected Class public `(fd)` constructor lookup / reflective construction |
| 382 / 389 / 394 | yaw `nextFloat()`, `sn.c(double,double,double,float,float)`, Mob eligibility |
| 410 / 431 | reusable `fw.a(sn,sn,float)` with radius 32, then last PathPoint |
| 528 | `ve.f(fd,int,int,int,int)` with Player floors and skip 1 |
| 585 / 591 | final Mob pose, then `fd.b(sn)Z` publication |
| 622 | shared `cq.a(ls,fd,float,float,float)` postspawn helper |
| 630 / 635 | Player wake `(true,false,false)`, then `ls.T()` ambient |
| 638 / 653 | success assignment / complete method return |

Actual reflection obtains a public `(fd)` constructor, constructs the Mob, consumes the World yaw draw and applies the Mob pose. The Mob's real spawn predicate runs before direct-World pathfinding. CQ accepts only a non-null Path with more than one point whose final X, Z and Y distances from the Player are each strictly less than 1.5. One finder retains and clears its actual heap/map backing storage between searches and Players.

The real Bed exit search skips one candidate. A missing exit falls back to the sampled candidate one block higher. CQ applies the final pose, publishes the Mob, runs the shared postspawn helper, wakes the Player with `(true,false,false)`, emits the Mob's ambient sound, then records success. The primary publication boolean is ignored. Spider postspawn may construct, publish and mount a Skeleton using the actual World `nextInt(100)` draw. Wake leaves the saved spawn and World sleeping flag unchanged for the outer tick to handle.

The [reference packet](../../tests/reference/world/nightmare/README.md) executes complete real original calls on prepared normal Chunks and sleeping Players. It includes full no-spawn loops, reusable finder searches, all Bed directions, two-player iteration and callback mutation, real publication failure, Zombie/Spider constructors and successful jockey publication. It compares selected actual Player/Mob/World/List/Block state, all Chunk planes, World and Math streams, final finder counts and backing capacities, and observer ordering. Original per-search PathPoint journals are also retained; the native CQ comparison does not serialize every private point. Private constructor entropy remains independent and is validated arithmetically rather than replaced.

Each original VM executes 28 calls: 23 return (13 true and 10 false), and five throw the actual uncaught source exception. The corpus executes 180 attempts and 77 direct-World searches. The blocked Player completes all 20 searches; each mixed two-player scene completes 21 searches using the same finder.

The four original VMs agree on their selected deterministic readback. Normal and optimized Python admission checks each pass 410 constructor arithmetic checks and 1,198 corruption refusals. Native comparison checks 39,908 selected scalars, including 21 native whole returns, five source-exception outcomes and two explicitly pending caught-printer prefixes.

Null List/World and changed-iterator exceptions retain their source prefix. The original reflection catch prints a stack trace and returns the prior result; native CQ362 printing remains an explicit frontier. These cases do not claim a native whole false return. The final static Class-array reference cannot be replaced by ordinary JDK 17 reflection, so null/empty array-reference corruption controls remain separate native-only evidence.

This packet qualifies the prepared CPU component. Natural loaded sleep progression, complete tick behavior, second-monitor rendering and performance require their own evidence.

The [loaded Bed sleep scene](../client/bed-sleep.md) separately records the qualified natural-night, dawn, presentation and save path. Its receipt does not broaden this component packet's scope.
