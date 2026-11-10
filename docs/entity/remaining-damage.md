# Wolf, Slime, Squid and Ghast damage

The client damage dispatcher now reaches the original hurt bodies for all four
of these admitted concrete types. They previously stopped at an unsupported
hurt declaration before changing health. Zombie Pigman remains a Class literal
without an admitted native Mob constructor; this change does not add one.

Slime (`uw`), Squid (`xt -> ar -> ii`) and Ghast (`bp -> wq`) inherit the Living
damage body directly. Wolf has its complete [own wrapper](wolf-damage.md),
including sitting, amount adjustment, retaliation, pack alerts and owner checks.
It shares the same actual Living parent without publishing the Wolf return
before that wrapper finishes.

| Receiver | Hurt/death sound | Volume | Death drops |
| --- | --- | --- | --- |
| Wolf | `mob.wolf.hurt` / `mob.wolf.death` | 0.4 | None from inherited `j() == -1` |
| Slime | `mob.slime` for both | 0.6 | Slimeballs only when the actual signed watcher size is 1 |
| Squid | NULL for both | 0.4 | Ink Sacs, with the separate Squid quantity and Stack loop |
| Ghast | `mob.ghast.scream` / `mob.ghast.death` | 10.0 | Gunpowder |

Squid's NULL sound does not skip the original sound body. Both private Random
draws still run, and the World forwards the nullable name to its listeners.
The String-returning leaf needs no Registry String lookup in this case.
The [drop contract](remaining-death.md) records the reached quantity, watcher,
allocation and callback order for Slime, Squid and Ghast.

## Owner comparison

Wolf's tamed-owner branch uses the actual Player name as the nullable String
receiver, then obtains the owner argument before invoking `equalsIgnoreCase`.
The native [counted UTF16 helper](../../tests/reference/core/string_case/README.md)
matches the qualified JDK17 char/codepoint comparison order, including paired
and unpaired surrogates. It uses no JVM or locale at runtime. Its reference
runner compares case keys for every code point value and a finite String corpus;
the public test also checks the complete key-stream fingerprint.

## Verification

The [original collector](../../tests/reference/entity/remaining_damage/README.md)
executes ten unchanged-original calls with real cached Chunks, entities and
published Items. Each target starts at health 20 with a declared private Random
seed of 38 after ordinary construction. Slime sizes 1 and 4, Squid and Ghast
receive damage 4 and 20. Wolf receives damage 4 and 40 from a Cow, preserving
its Java-wrapped `(amount + 1) / 2` adjustment. Constructor and Item entropy
remain independent.

Native return, health/timers, raw angle and velocity words, private Random
progress, nullable sound name, raw pitch/volume and ordered Item records compare
exactly. The public native runner checks the same finite records without Java
or original assets. Separate authored controls exercise populated Wolf packs,
Player/Arrow sources, ownership branches, callback changes and failure cleanup;
the ten-call original comparison's Wolf pack is empty.

These are component qualifications. The separate delayed Slime `K()` splitting
body, complete execution-owned Arrow impact composition, sustained combat,
original audiovisual equality and FPS remain unfinished or unqualified.
