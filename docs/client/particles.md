# Pickup particles and effect ownership

The client implements the actual `em` pickup particle, its `xw`/`sn` constructors,
the CPU `dn` effect manager, and the selected complete-App pickup notification.
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
The actual override increments its wrapping age, compares the lifetime and
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

Original nonnull-App pickup correspondence, actual RenderEngine construction,
complete particle drawing, frame scheduling, full World/Entity updates and the playable
client remain unfinished. Finite CPU evidence does not establish those behaviors.
