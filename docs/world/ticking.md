# Ordered client World ticks

The source client invokes `fd.l()V` after renderer and entity ticking. The new
retained scheduler executes reached children in their original order. The
ordinary loaded Player path now completes weather and gathers 289 mob-spawn
candidate chunks before stopping at Biome spawn-list selection in `cq.a:290`.
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
or discarding candidates. General mob construction, sleeping wake transitions,
and nonempty random Chunk work remain unfinished.

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
