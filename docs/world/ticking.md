# Ordered client World ticks

The source client invokes `fd.l()V` after renderer and entity ticking. The new
retained scheduler executes reached children in their original order. The
ordinary loaded Player path completes weather, gathers 289 mob-spawn candidate
Chunks, selects a real Biome entry and reaches concrete Mob construction at
`cq.a:773`.
A complete ordinary tick and gameplay remain unfinished.

The scheduler calls weather, tests sleeping Players, runs spawning, maintains
the actual provider, updates sky subtraction and dispatches the live WorldAccess
sky-change list. It then computes the next World time, performs a due autosave,
stores time, drains scheduled ticks and enters random chunk work. Every missing
descendant stops the call at its reached source location. No later phase runs
after a failure. Empty candidate/player branches can return at `fd.l:198`.

The candidate Sets retain canonical World/class ownership. The modeled JDK17
traversal is bounded to its qualified integer-hash domain; collisions or native
capacity limits report an explicit frontier rather than changing iteration order
or discarding candidates. Actual s/t/u spawn Lists, wrapping weight totals,
weighted World Random selection, candidate/count gates, initial point draws,
all three-by-four trial loops, material/support checks, nearest-Player scan and
float spawn-distance gate run before 773. Both `nextInt(1)` calls consume Random.
Selected Entity class/entry, candidate progress and Random state remain visible
in retained diagnostics. General Mob construction, sleeping wake transitions,
and nonempty random Chunk work remain unfinished.

The [eleven original observations](../../tests/reference/world/tick/README.md)
include one new selected-Pig prefix: seed 529, ticket 18 of 40, initial point
(-40,64,-36), trial(-39,64,-37), ten bounded draws and state `98aeeda4389e`.
The original invokes the actual Pig constructor and stops at authored insertion;
the compared snapshot precedes yaw. Native execution reaches 773 on a real
stored flat Chunk, preserves region bytes and leases, and performs no insertion.
This qualifies the prefix, not a successful spawn or complete ordinary tick.

Scheduled drain implements actual `fd.a(Z)Z`. It checks the ordered/membership
queue sizes, processes at most 1,000 entries, honors due times unless forced,
removes from the ordered queue before the membership set, checks nonloading
radius-eight presence, and calls the matching positive Block's tick with the
same World Random. Each selected record stays held across removals and
synchronous callback reentry. Earlier removals survive later failures. The
result indicates whether the ordered queue remains nonempty.

The scheduler binds its World through checked native retirement. Sky callbacks
iterate the live list, allowing self-removal and nested ticks. Retained failure
receipts and source fields survive failure; cleanup does not replay source work.
The client runtime keeps these diagnostics under its terminal tick record.
After a completed scheduler, ambience `fd.q(III)` remains an explicit dependency.

Native component tests cover no-Player returns, weather/time order, the 1,000
record bound, force/future entries, size mismatch, removals before provider
failure, sky callback mutation/reentry and retained ownership. Their finite
scope does not establish complete spawning, random blocks or server ticks.
