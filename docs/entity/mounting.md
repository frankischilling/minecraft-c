# Entity mounting and Spider jockeys

Natural Spider jockey spawning connects the actual Skeleton and Spider through
the inherited Entity mount method. Mounted Skeletons execute their riding tick,
including the active Skeleton body, passenger positioning and the LivingEntity
animation tail. Other unfinished riding and ranged-combat paths remain explicit.

## Original behavior

The authority is the original client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The original `cq.a(Lls;Lfd;FFF)V` postspawn helper invokes `fr.i(sn)` at56.
Skeleton and LivingEntity declare no override; `sn.i(sn)` owns this body.

Both angular accumulators become positive zero before branching. A fresh
mount detaches the rider's old passenger link, detaches the target's previous
rider, then stores rider-to-mount and mount-to-passenger in that order.
A NULL target positions the rider before clearing either link. Selecting the
same mount clears both links before positioning. These distinct failure
prefixes remain observable when source bounds are NULL.

Dismount positioning follows `sn.c(DDDFF)`: the mount's x/z and lower bounds plus
height become the rider's position, with the rider's own offset added by that
body. Yaw and pitch retain their actual current values.

Spider's `cn.m()D` passenger offset is `(double)height * .75 - .5`.
Skeleton inherits `ls.s_()V`, which calls `sn.s_()` before storing `J=K` and
`K=+0`. The stores run after a returned dead-mount short path too. A failed
active tick or positioning call preserves its completed prefix.

## Lifetime and cleanup

Source `aH` and `aG` fields retain separate native occurrences. These holds keep
relationships valid across removal from a World container. Supplied borrowed
relationships need an independently live caller scope before dereference;
unproved raw pointers are refused. Mounting introduces no Random draw, World
insertion or extra tick.

Ordinary dismount, replacement and dead-link cleanup release the corresponding
holds at their actual stores. Checked World retirement inventories relationship
occurrences alongside existing Entity and Mob roots, pins the complete graph,
then breaks internal cycles. An external owner of live relationship fields
still refuses terminal mutation. Isolated callers must dismount before dropping
their last external roots.

The public `entity.mount` fixture uses genuine Mob, World and Chunk constructors.
It covers fresh, NULL, same-target, replacement and self mounting; positive-zero
accumulators; unchanged Random state; exact occurrence counts; source-null
failure order; borrowed-link admission; mounted World ticks; and terminal cycle
retirement. These authored controls are distinct from an executed original
jockey, original save correspondence or extended natural play.

The final component completes 130 checks on Windows MSVC Release and Linux
Clang with AddressSanitizer and UndefinedBehaviorSanitizer. The public World
entry clears its success diagnostic; a separate actual update-body call
observes the one passenger continuation before that reset. The full Windows
build passes. The focused fifteen-suite Windows and fourteen-suite Linux
regressions cover existing Entity updates, publication, terminal ownership,
Player ticks, damage, drop and placement alongside mounting.
