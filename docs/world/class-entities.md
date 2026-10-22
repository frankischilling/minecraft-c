# Class-filtered entity queries

`mc_world_runtime_class_entities` implements the paired `fd/dj.a(Class,AABB)`
overload. It reads and floors the query bounds before constructing a fresh List,
then scans inclusive X/Z coordinates using the real nonloading presence test,
acquiring getter and normal or Blank Chunk dispatch. It never clears, returns
or appends to World.R. Empty successful queries also return a fresh List.

Each normal Chunk selects its current Y slices, reads each current List size
and occurrence, obtains the candidate's actual Class and checks assignability
before reading its bounds. A rejected Class therefore does not read nullable
candidate bounds. A null candidate fails at getClass113 before a null target
can fail at isAssignableFrom116. A matching candidate with null bounds fails
at intersection invocation128; reading the nullable field124 itself succeeds.
Blank dispatch returns before inspecting nullable arguments. Duplicates and
source order remain intact. There is no dead, Player or construction-World
filter and no traversal cap.

The passive Class namespace has 26 canonical cells per loaded owner. The first
24 retain EntityList registration order; query-only Entity and Player follow
Boat. EntityList still registers exactly 24 classes, and only the existing
Item/Falling constructors are available. Assignability follows the audited
superclass chains within one owner. Equal names in another owner do not match.
Literal lookup and assignability perform no class initialization, allocation,
ID update or entropy draw. Exact-kind inspection retains its existing meaning.

Each result owns a stable source List descriptor, separate backing arrays and
one Entity hold per occurrence. A successful result holds one local World lease,
including when empty. Release balances those holds and charges; World and
container retirement stay busy while a result is live. Later Class or R queries
cannot change an earlier result. Header and overlapping growth allocations
charge the World's direct allocation budget. These native resource limits do
not reproduce Java ArrayList allocation or out-of-memory timing.

Failure preserves the caller's output and reached World/Chunk effects. Only the
failed query's private result and occurrence holds are disposed. Diagnostics
retain separate World, provider and Chunk sites and detailed acquisition causes.
The [pressure helper](pressure-plates.md) uses these real results for Living and
Player categories, resolving each target from the Block's defining namespace
before its bounds factory. Its previous selected result remains held until the
next query returns successfully.

The `world.class_entities` case passes 373 checks per host; the Class identity
and ancestry case passes 13,819. Full suites pass 173 MSVC and 175 tests on each
sanitized GCC/Clang build without skips. The Class fixture uses both sides,
eight real Worlds and eight explicit loaded Chunk records. Its retained regions
are inputs, not final query state or original observations. Tests include result
identity/lifetime, R independence, nullable ordering, partial appends, failed
public cleanup, overlapping growth budgets and native retain/lease overflow.

The source authority is the paired bytecode identified in
[the pressure source map](../reference/pressure-plates.md). Native contract tests
exercise supported real Item/Falling actors; they do not construct surrogate
Living, Player or Minecart instances. Executed-original Class-query equality,
other constructors, detector-rail integration and a running game loop remain
unfinished.
