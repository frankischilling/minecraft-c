# Creeper damage without an attacker

Client Creeper `GB` inherits `GZ.a(Entity,int)` through the actual
`GB -> GZ -> II -> LS -> SN` hierarchy. A null attacker now enters the existing
Monster and LivingEntity damage body with the same Creeper receiver. The
concrete hurt sound is `mob.creeper`; the fatal sound is `mob.creeperdeath`.
The symbolic death invoke in `GB.b(Entity)` at bytecode 2 resolves to `LS`
through its superclasses. The later Skeleton-record branch remains outside
this qualification.

The reached native SDK failure came from a naturally spawned Creeper during
nonpausing crafting. Its actual World update entered `LS.U` suffocation damage
at bytecode 66 and stopped at the missing concrete dispatch. The failed run is
retained; it is not an exact replay of that actor's complete state.

The new `entity.creeper_null_damage` control independently constructs a genuine
Creeper and GAME World, inserts it normally and places opaque Stone at its eye
height. The real World tick reaches the same environmental damage call. The
sound callback observes health and hurt stores before later movement writes
the nonburning fire value from Java negation of the live `bu` field.
The initial test incorrectly expected that final field to remain zero; only
that assertion changed after a passive measurement and original `SN.b` audit.

Further controls cover independently derived Random progress/pitch, repeated
and resisted damage, an existing target identity, remote/dead/nullable inputs,
sound callback refusal and active-owner cleanup refusal. Nonnull attackers
remain an explicit dependency. Fatal null-attacker damage preserves the real
health, timers and death-sound prefix, then stops at the still-unimplemented
death/drop child. It does not claim a completed Creeper death.

These are authored native component controls. Original method identities and
inheritance have [static archive evidence](../../tests/reference/entity/creeper_null_damage/README.md),
with 23 passing corruption controls. No executed-original Creeper damage,
general combat or extended-play comparison is claimed.
