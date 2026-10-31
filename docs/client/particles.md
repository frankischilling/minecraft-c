# Pickup and Item particles

The client implements the actual `em` pickup and `pb` Item particles, their
`xw`/`sn` constructors, the `dn` effect manager, and the selected complete-App
pickup and RenderGlobal particle notifications.
The [particle draw entries](particle-rendering.md) now run on these same owners.
Reached Entity drawing and the complete game frontend remain unfinished.

`mc_pickup_particle_allocate` creates a real RAW client-only Entity. Construction
reads the picked actor's six position/velocity arguments before entering its
ancestors. It uses the existing shared Math owner for five `Math.random` calls
and its newly constructed ordinary Entity Random for four `nextFloat` calls.
Size, bounds, velocities, colors, lifetime and retained actor fields follow
their original store order. Null picked actors fail at actual `em3` before an
Entity ID or constructor entropy is consumed. Reached failures keep their
completed fields and cannot retry.

Actors are held READY ordinary non-particle identities from the same namespace,
matching the actual pickup pipeline. This native admission rejects RAW cycles
and nested particle reference graphs before source construction. It keeps
retirement depth constant. Picked and target aliases own two distinct holds.
The actual pickup override increments its wrapping age, compares the lifetime and
sets inherited `be` when equal. It does not call the ordinary particle tick.

The effect manager allocates its List array and constructs its own ordinary
Random, then stores the supplied nullable World and borrowed RenderEngine.
It next constructs four distinct ArrayLists in that array.
Each List occurrence retains its actual Entity separately. Adding at 4000 removes
the first occurrence before appending. Tick reads each current size, calls the
real override and removes dead occurrences without skipping the next element.
World change stores the new World and then clears all Lists; their identities
and capacities stay stable. Typed errors preserve source PCs and native
allocation/hold prefixes. Native capture holds keep a previously loaded manager
alive through constructor argument evaluation.

`mc_item_particle_construct` enters the genuine PB allocation. Its ancestor
consumes five shared Math draws, followed by four floats from the newly created
Entity Random. PB then reads the supplied Item icon at17, reads the actual
Snowball static Item at39 and its icon at42, halves the inherited size at55,
and returns at58. These are ordered field reads, including both null failure
positions; the constructor does not retain either Item. The ordinary XW tick
stores previous coordinates, postincrements age, performs the reached death
check, applies gravity and enters real Entity movement. Damping and the grounded
velocity adjustment follow movement even when the age check already marked the
particle dead.

RenderGlobal's actual Item callback captures the current App effect manager
at683, allocates PB at686, reloads its own World at691, reads the selected Item
at699, constructs at702 and enqueues through the captured manager at705. The
initial null-manager guard returns at27. A later failure keeps the RAW or partial
candidate and the exact App, World, Registry, manager and reached Math owners
until checked cleanup. Math ownership begins at the reached XW active use after
the five shared draws. It is not acquired before constructor argument evaluation.

DN dispatches PB's layer2, tick and draw bodies on those same Entities. A failed
PB tick keeps its full movement error in manager-owned pending storage; the
compact ordinary particle error remains suitable for the existing pickup path.
Mutation, World changes and final manager cleanup refuse while a pending child
still owns its resources. Terminal World admission counts every real List
occurrence, including duplicates, and one World binding for each distinct PB.
An external particle owner prevents retirement before source fields change.

`mc_local_player_pickup_notification` follows actual `dc.b(sn,int)`: capture
Minecraft.j at4, allocate `em` at7, reload Minecraft.f at15, construct at22,
then invoke the captured manager at25. A real null manager is checked after
construction. The World comes from the actual application field independently
of the Player's construction World. Amount is genuinely unused. Failed anonymous
candidates retire without reversing shared IDs or Random effects.

The application has explicit f/p/j storage. Its selected startup operation
allocates `dn` at645, reads f650/p654, constructs657 and publishes j660 only on
success. A failed candidate stays privately owned until terminal retirement.
The genuine normal startup caller now reaches this operation after RenderGlobal
and viewport setup. The Windows Session constructs all four Lists and publishes
j660 before stopping at resource-thread allocation664. This does not execute a
World or particle frame. Native shutdown releases views, particles and World
owners before the Session class namespace, because their Players retain Sessions.
It adds no source save or unload operation.

The normal Item pickup body now returns from the real notification before
reloading its current Stack and reaching inherited Entity.K. The queued
particle keeps a picked Item alive after the caller releases it; the third
effect tick retires that occurrence and its actor holds. Failed notifications
retain prior insertion, achievement, sound and Item Random effects.

[Qualification](particles-qualification.md) separates the 71 exact original
NULL-World CPU records from the actual-App native fixture. The latter checks
real Session/Writer/Registry/World/Player owners, independent application World,
null-App and null-manager ordering, failed candidates and complete retirement.
The bounded controller checks the complete case order, whole original class/Code
metadata, actual runtime JAR origins, helper inventory and input/product hashes.
Observed constructor U/nanoTime inputs replay only through the private test
binding; the game API accepts no seed, clock sample or supplied random output.

The [PB original observer](../../tests/reference/client/item_particle/README.md)
separately records real nonnull-World construction, Slime landing and DN drawing,
along with the PB17/PB42 and Entity movement/brightness null failures. Its warm
Registry prerequisite is declared. A cold first-failure experiment is retained
separately and earns no native cold-initialization correspondence.

Fresh Windows and Linux comparisons each pass 1,863 native checks across five
processes and 67,444 selected original comparisons, with 180 identity pairs,
45 independent stream-progress checks and four actual null failures. Four
corrupted manifests and eight comparator corruptions fail for their intended
reason. Eleven direct native constructor receipts have 77 separate scalar
checks; the twenty original constructor intervals retain their own scalar
proofs. Neither set supplies entropy to execution.

The comparison checks the actual PB/Entity fields, bounds, watcher record and
typed value, DN occurrence order, shared origins, pool cursors and selected NW
state. Raw constructor velocity/jitter/size/lifetime fields and landing x/z
coordinates from Slime's independent private stream are not equated across VMs.
Concrete JDK buffer classes, List capacity/layout, boxed-Byte and flattened
String identity retain separate limits. The declared input/helper manifest has
28 inputs and four compiled helpers; a separate post-capture audit checks 234
current output files before and after execution. It does not claim those output
hashes were recorded at capture start.

Original nonnull-App pickup correspondence and complete pickup rendering remain
unfinished. PB's finite constructor, tick and draw evidence does not establish
all particle kinds, arbitrary World updates or a playable client.
