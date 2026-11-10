# Combat and projectiles

Spider, Zombie, Skeleton and Creeper now take nonnull-attacker damage through
their actual inherited Living damage body. Successful hits apply knockback and
the concrete hurt sound, then preserve mount/self targets or retain the attacker
as the new Creature target. A sound callback can change the mount before that
retaliation decision. Stronger repeated hits still reach retaliation even when
they skip knockback and sound.

Four fresh original nonfatal calls match the native return records exactly for
the recorded health/timer fields, raw direction and velocity bits, sound name,
attacked flag and target alias. Constructor entropy and sound pitch retain
their separate scope. The [contract and native controls](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/monster-retaliation.md)
describe ownership, failure ordering and reproduction.

Arrow water controls cover wet flight, both air/water transitions, callback
failure and checked cleanup. They preserve the original small-bounds quirk:
an eight-tick prepared water scene has wet ticks 1–4, dry ticks 5–7, then another
wet tick with a new splash. NULL particle listeners now retain the actual Source
null-reference classification. The [water reference collector](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/reference/entity/arrow_water/README.md)
records complete finite state/event pairs and every independent-entropy
difference.

Fatal Monster damage now completes ordinary drops, Skeleton's ordered Arrow/Bone
loops, Creeper's late Skeleton-kill record and Player's achievement callback.
Twenty-two finite records compare exactly with fresh original calls, including
ordered Items and selected Player statistics. Native failure controls retain
published drops through checked cleanup. See the
[death contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/monster-death.md)
for source ordering and the comparison's excluded independent entropy.

Full Arrow victim damage, pickup/riders,
unrestricted combat, original audiovisual equality and FPS remain unfinished
or unqualified. These component checks do not establish complete playability.
