# Animal damage

The client Sheep, Pig, Cow and Chicken now enter the actual inherited
LivingEntity damage body for NULL and non-NULL attackers. This connects the
damage call reached by a [Wolf bite](wolf-attack.md). The receiver keeps its own
health, movement, Random and sound declarations throughout the call.

Sheep's `dl.a(Entity,int)` invokes the symbolic Animal method `bg.a` at bytecode
3. That reference resolves directly to `ls.a`; neither Animal nor Creature
declares an intervening damage body. Sheep returns the inherited Boolean at
bytecode 6. Pig, Cow and Chicken inherit `ls.a` without the Sheep wrapper. The
[reference packet](../../tests/reference/entity/living_damage/README.md) records
the selected original identities, declarations and resolved references.

## Health, direction and knockback

The shared body preserves the remote-World and dead-Entity returns, hurt
resistance, full-hit health and timer stores, and stronger repeated damage.
A weaker repeated hit returns false. A stronger repeated hit subtracts only
the additional damage and skips fresh knockback and sound. These early branches
leave the attacker and MathHelper arguments unread.

A full non-NULL-attacker hit reads the attacker's X and Z positions only after
the health, hurt-timer, status and attacked-flag prefix. The damage direction
uses the original binary64 arithmetic and `Math.atan2`, then narrows and
subtracts the receiver's yaw in binary32. Nearly coincident positions enter
the original retry loop: each attempt consumes four shared `Math.random`
draws and forms two differences scaled by 0.01. The squared-distance test and
Java unordered comparison remain intact. Diagnostic counters saturate without
limiting retries or changing Random consumption.

The inherited knockback method uses the caller's actual MathHelper class
namespace at its reached square-root call. It halves the existing velocity,
applies the horizontal impulse, adds the original upward impulse and caps Y in
source order. Earlier health, timer and direction changes survive a failure at
this call. A NULL attacker instead uses the original shared-random direction
branch and does not enter knockback.

`mc_entity_effect_damage_with_math` accepts the explicit Registry and MathHelper
owners. Both remain borrowed and exclusive through the synchronous call.
MathHelper initialization occurs only when knockback reaches it; the method
does not derive another namespace from World fields or a detached trig table.
The older context entry supplies NULL MathHelper and retains its reached
failure when a hit needs that namespace.

## Sounds and lifetime

The sound call captures its World receiver before evaluating the virtual name,
volume and two private Random draws. The animal declarations are:

| Animal | Hurt sound | Fatal sound | Volume |
| --- | --- | --- | --- |
| Sheep | `mob.sheep` | `mob.sheep` | 1.0 |
| Pig | `mob.pig` | `mob.pigdeath` | 1.0 |
| Cow | `mob.cowhurt` | `mob.cowhurt` | 0.4 |
| Chicken | `mob.chickenhurt` | `mob.chickenhurt` | 1.0 |

The effect entry retains the receiver before source execution. A non-NULL
attacker gains a separate native hold only at the first reached position read,
`ls.a:144`, and keeps that hold through later sound callbacks. The call releases
it on success or failure. A self-attacker therefore has two independent holds
while that branch runs. Early returns neither validate nor retain a skipped
attacker. Callers must keep the actual owner graph live; recorded pointer
identities do not own objects.

Errors retain the reached wrapper, LivingEntity and child locations, Random
progress, source-hold receipts and complete typed causes. Failure preserves
completed stores and leaves the Boolean output unchanged. Native allocation,
ownership and floating-point refusals remain distinct from an evidenced source
exception. No failed call is replayed or rolled back during cleanup.

A fatal hit reaches the actual death/drop invocation at `ls.a:316`, after the
fatal sound when the full-hit branch requires it. That body remains unfinished.
The preserved health and sound prefix does not establish completed death,
Entity removal or an Item drop. Other Mob damage overrides, Player combat and
complete combat remain outside this animal scope. Existing
[Zombie](living-damage.md) and [Skeleton](skeleton-damage.md) NULL-attacker
qualifications retain their own receiver and branch limits.

## Verification scope

Windows MSVC Release and Linux Clang ASan/UBSan each pass 145 authored checks
and 27 original comparison rows within the scopes below.
The authored component uses real animal, Wolf, World, Registry, MathHelper and
WorldAccess owners. It checks health and velocity words, repeated hits, NULL
and self-attacker paths, retry Random progress, concrete sounds, late holds,
callback refusal and reached failure prefixes. The Wolf continuation checks the
ordinary bite caller rather than supplying a damage result.

The unchanged-original CPU capture records 25 declared profiles and 27 calls.
It constructs actual Sheep and Wolf objects with an ordinary original World,
then observes the real damage and sound calls. The final profile makes three
successive hits on the same Sheep; two others enter the actual untamed and
tamed Wolf bite method. Actors remain component operands outside loaded
Chunks. Their constructor entropy and ordinary World scheduling are outside
the comparison.

The fatal profile compares the read-only sound-callback checkpoint before
`ls.a:316`. The original then completes its sheared-Sheep death; the native
body retains its explicit death dependency. The throwing callback profile
likewise keeps the original `IllegalStateException` and the native adapter's
I/O failure as distinct outcomes, while comparing their completed damage and
sound state.

Build `mc_test_living_damage`, then run:

```sh
ctest --test-dir build -R '^entity\.living_damage$' --output-on-failure
```

The public native controls and replay need no original archive, assets or JVM.
Static source identities, original CPU observations and native replay retain
their separate scopes. They do not establish a complete graphical Wolf
encounter, native death/drop behavior or extended-play stability. Pig, Cow and
Chicken use authored native sound controls; the original CPU packet's animal
receiver is Sheep.
