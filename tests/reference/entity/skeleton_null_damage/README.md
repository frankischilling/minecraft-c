# Skeleton NULL-attacker damage source pins

This packet records offline class/Code hashes and selected resolved references
from the unchanged original client JAR. It contains no JAR, classfile, copied
game body, native capture or original runtime observation. Run the authored
verifier with a separately supplied private archive:

```text
python -B tests/reference/entity/skeleton_null_damage/verify_source.py --jar ORIGINAL_CLIENT_JAR
```

Missing archive input reports `UNAVAILABLE` with exit77. Invalid or changed
identities report `FAIL` with exit1.

The complete inventories establish that FR has no `a(Entity,int)` damage
declaration and inherits GZ. GZ invokes the symbolic II method at3; II has no
such declaration, so that call resolves to LS. The existing NULL-attacker
Living/Monster body is therefore shared by genuine Zombie and Skeleton
receivers. Their native concrete kinds and identities remain distinct.

FR declares both `j_()` and `i()` as three-byte String-returning bodies, each
returning `mob.skeletonhurt`. This literal is used for both reached hurt and
fatal sound prefixes. Volume `k()` is inherited from LS. SN's actual environment
damage call at501 and LS's SN environment call at9 are pinned separately from
the GZ/LS damage bodies and World Entity-update/sound callers.

The authored native component is
`tests/entity/mob/test_skeleton_null_damage.c`. It constructs genuine Registry,
Entity/Math namespaces, GAME World and25 distinct generated air Chunks, then
normally inserts a canonical Skeleton and calls actual World update with
declared Fire20. It checks returned health/timer effects and the real
WorldAccess sound callback before the fire decrement. Further controls cover
fresh/repeated/delta damage, actual target identity, remote/dead/nullable
operands, reached FR sound methods, native callback refusal, and fatal and
nonnull-attacker frontiers. Callback retirement attempts use actual active
owners; no busy field, damage result, Random stream or source return is forged.

The component is an authored contract, not a replay of a captured SDK actor.
It does not make original/native runtime comparison claims. The existing
Zombie observations retain their original receiver and scope. Nonnull attacker
damage remains unsupported at its real dispatch, and fatal NULL damage stops
at actual LS death/drop invocation316 after the reached Skeleton sound. These
prefixes do not credit a completed death, drop, or full damage return.
