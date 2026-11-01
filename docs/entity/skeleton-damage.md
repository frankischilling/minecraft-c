# Skeleton damage without an attacker

The client Skeleton now uses its actual inherited Monster and LivingEntity
damage methods when the attacker is NULL. This connects the periodic fire
damage reached by ordinary World ticking. The receiver remains a Skeleton;
its hurt and fatal sound prefixes resolve the actual Skeleton declarations.

The [source packet](../../tests/reference/entity/skeleton_null_damage/README.md)
pins seven complete class inventories and the selected method identities and
references from the original archive. Skeleton `fr` has no damage override.
Its inherited `gz.a(Entity,int)` invokes the symbolic Creature method at3,
which resolves to LivingEntity `ls`. Both Skeleton sound methods return
`mob.skeletonhurt`; volume inherits `ls.k()`.

The existing [Zombie NULL-attacker body](living-damage.md) supplies the shared
source order and arithmetic. Skeleton now enters that same body through its
own dispatch. Health, repeated-hit resistance, hurt timers, attacked state,
direction, shared and private Random calls, captured sound World and actual
WorldAccess callbacks retain their original order. Zombie's receiver and sound
declarations remain distinct.

Nonnull-attacker damage remains a reached dependency. Fatal NULL-attacker
damage returns from the reached Skeleton sound, then stops at the actual
LivingEntity death/drop invocation316. No completed death or drop is credited.

## Regression and scope

A native SDK run recorded `fd.a(Entity,true):121` entering Skeleton's
`sn.U():501` fire-damage call and stopping at the missing concrete hurt body.
The failed actor's complete state was not retained, so this is not an exact
saved-actor replay.

A separate authored component constructs a genuine Skeleton, GAME World,
25 generated air Chunks and normal World insertion. Declared Fire20 reaches
the same World→Mob→environment→damage chain. The pre-fix public regression
failed at that exact frontier. Fire0 and an omitted-neighbor control separately
distinguish a real completed tick from a vacuous outer World return.

`entity.skeleton_null_damage` checks returned health/timer effects and the actual
sound callback before the fire decrement. Further controls cover fresh,
repeated and stronger damage, an existing target identity, remote/dead and
nullable inputs, the reached Skeleton sound declarations, callback refusal,
active-owner cleanup refusal, and fatal/nonnull-attacker prefixes. Inputs are
declared component state; no SDK actor, kind, source flag or damage result is
repaired.

The test requires no original archive, assets or JVM. Static matching and
authored native controls are separate from original runtime comparisons. The
existing Zombie observations retain their original receiver and scope. This
bounded continuation does not establish general combat or extended play.

The new regression passes on Windows MSVC Debug and Linux Clang ASan/UBSan.
The original fast red probe also returns normally in all five runs after the
fix; the earlier red executable and observations remain preserved separately.
