# Player ally alerts

Both Player combat routes use the client `gs.a(Lls;Z)V` body. Outgoing attack
calls it at GS93 with `true` after a living target survives damage. Incoming
Living damage calls it at GS144 with `false`, before the damage statistic and
the shared health operation. Null and non-Living incoming attackers skip that
call. The caller's source site remains separate from the helper's own site
and deepest reached child.

The helper first excludes Creeper and Ghast, the Player's own tame Wolf, and a
Player whose canonical `F()` returns false. These paths return before World,
bounds or nearby-entity work. Otherwise it captures the current World at 65,
creates the actual pooled unit bounds at 100, expands by 16/4/16 at 112 and calls
the real Class-filtered Wolf query at 115. A null World therefore fails after
the two bounds operations, preserving that completed source prefix.

The fresh result List retains source order and duplicate occurrences. Each
Wolf must be tame, have no current target and belong to this Player. Attack
mode then reads the sitting flag and skips a sitting Wolf. Damage mode skips
that getter. Both modes call the actual unsit setter before assigning the
owning target field. Earlier Wolf changes survive a later getter or target
failure; no rollback is performed.

Watcher records retain the declared serialization type independently of their
actual payload kind. A null boxed Byte reaches null unboxing; an Integer used
as a Byte reaches ClassCastException even if the record still declares type 0.
The runtime uses typed inline payload storage with an explicit tag. It does
not allocate Java box objects or treat an arbitrary pointer as a payload.

## Native ownership

`mc_player_ally_alert_begin` publishes an inspectable invocation owner for a
reached source failure, including a null receiver. Native admission failures
leave the fresh output null. The owner retains each reached Player, target,
World, bounds, query List and diagnostic receipt occurrence. Its error keeps
the first source outcome; cleanup has its own status and operation.

Incoming damage stores the failed invocation directly on the Player, including
when no diagnostic output is requested. Another damage, death or Player tick
refuses while that child is pending. World terminal checks also refuse it.
Cleanup retires only native ownership: it never reruns a query, watcher store
or target assignment. A refused cleanup leaves the invocation available for
retry. Successful cleanup clears the Player backedge before the final Entity
holds can be released, and clears borrowed diagnostic views immediately.

The damage error retains a compact first-outcome view and borrows the detailed
helper error while its owner lives. Error retirement handles a child already
consumed through another cleanup sink. A current native BUSY or cleanup refusal
does not inherit the retained child's earlier source exception classification.
Loader and callback classification require the current status and operation to
match the reached diagnostic cause.

Retirement validates the full multiplicity of held List occurrences before
releasing any of them. Small Lists keep the allocation-free scan. Larger Lists
count pointer identities in optional scratch storage capped at 1 MiB; a workspace
extent or allocation refusal falls back to the same scan. The workspace adds
no new cleanup failure and does not change World allocation budgets. Source
List order, duplicate occurrences and target/Player aliases remain intact.

## Evidence and remaining scope

The [source map](../reference/player-ally-alert.md) and
[original packet](../../tests/reference/entity/player_ally_alert/README.md)
describe the finite inputs and comparison. The native incoming-Skeleton
regression uses the genuine World, full application, session and statistics
writer. Its first run stopped at the missing GS144 body with health 20; the
implemented route reaches health 16 and records damage 4. Separate controls
exercise null diagnostic output, pending ownership, refused cleanup, retry and
a copied error view after cleanup through another sink.

Four original VMs each complete 40 prepared calls: 19 normal returns, 17 null
exceptions and four class-cast exceptions. Windows and sanitized Linux each
match 57,580 shared scalar values. Another 111 original exception-message and
stack-frame fields remain unpaired. Constructor entropy is independent;
each native call's own stream consumption and sound arithmetic are checked
separately. The [retirement measurements](../performance/player-ally-retire.md)
retain their separate CPU-only source and product scope.

The natural Monster attack parent still stops at `GZ.a(Entity,float):61`,
after committing cooldown and damage operands but before invoking target hurt.
Joining that caller is required before a naturally attacking Zombie can use
this completed damage component in loaded play.

The overlay's entered Byte getter uses a compact diagnostic. Its successful
path writes only the selected scalar output; failure materializes the full
effect record from that same completed read. Eleven watcher controls and the
separate hidden hand-rendering profiles cover this diagnostic adapter without
replaying a getter or adding a large stack record.

These components do not establish loaded hostile encounters, Arrow shooter
resolution, unrestricted combat, original audiovisual output or sustained FPS.
Ghast construction has its [separate scope](ghast.md); Ghast flight and attacks
remain unfinished.
