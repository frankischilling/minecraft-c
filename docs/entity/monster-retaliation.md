# Monster damage and retaliation

Client Spider, Zombie, Skeleton and Creeper now accept nonnull attackers through
their inherited Living damage body. A successful hit applies the existing
health, resistance, hurt timers, direction, knockback and concrete sound logic,
then runs the Monster retaliation wrapper. The original damage source remains
the argument throughout; the wrapper does not substitute an Arrow's shooter.

The unchanged client `gz.a(Lsn;I)Z` has 41 Code bytes with SHA-256
`3a564dbff0d8aafe6a9adcb734753a4b334a763954343aba45255c4114e8c7fb`.
Its inherited Living call resolves directly from the symbolic Creature method.
After a false result it returns false without reading mounts or the target.
After a true result it compares the current passenger, then the current vehicle,
with the attacker. Either match preserves the old target. Otherwise self damage
also preserves the target; another attacker becomes the actual Creature target.
These comparisons occur after damage callbacks, so a mount changed by a sound
observer affects the subsequent wrapper branch.

The Creature target owns its own Entity occurrence. Replacement retains the new
target before publishing it and releasing the old occurrence. A stronger hit
during resistance can replace that target even though it skips direction,
knockback and sound. Failure at the owning store preserves earlier health
changes and leaves the caller's Boolean unwritten. A skipped store does not
validate the target's native ownership cells.

## Verification

`entity.monster_damage` constructs all four concrete monsters, a Cow attacker,
an old Pig target and a genuine World. Its controls cover an ordinary hit, weak
and stronger repeated hits, self damage, both mount directions, a mount changed
inside sound, callback failure, remote World rejection and reached native target
provenance failure. Checked retirement releases the target and mount occurrences.
The existing Zombie, Skeleton and Creeper controls also exercise their complete
nonnull continuations.

The [reference collector](../../tests/reference/entity/monster_retaliation/README.md)
compares four ordinary nonfatal hits with the unchanged original. Each uses
damage 4, health 20, a Cow at offset `(3, 0, 4)`, yaw 30 and initial velocity
`(0.25, -0.375, -0.125)`. Raw angle and velocity bits, health and hurt fields,
returned Boolean, sound declaration and target alias are compared exactly.
Constructor entropy stays independent. Sound pitch, complete heap state and
whole-method equivalence are outside that comparison.

The [fatal death/drop continuation](monster-death.md) now has separate native
and original controls. The full Arrow victim call, unrestricted combat sessions
and original audiovisual or FPS parity remain unfinished or unqualified.
