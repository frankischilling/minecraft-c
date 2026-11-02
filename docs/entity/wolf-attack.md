# Wolf attack and pounce

The actual Creature attack caller now enters the Wolf's `gi.a(sn,float)` body.
It preserves the original range checks, private Random call, airborne branch,
pounce motion, vertical overlap tests, tame getter and target hurt invocation.
This method has no existing attack-cooldown gate.

For a distance strictly between 2 and 6, the method draws `nextInt(10)` before
checking on-ground state. A zero draw while airborne returns directly. A zero
draw on ground reads the target X/Z, computes the original double sum and
square root narrowed once to float, then updates horizontal velocities and
stores vertical velocity `0.4000000059604645`. It introduces no jump callback.
Other outcomes continue to the distance below 1.5 bite test.

A bite requires strict overlap of the target and Wolf bounds. The method
stores attack cooldown 20 before reading tame state through the actual watcher
Byte getter. Untamed/tamed Wolves pass damage 2/4 and their own identity to the
canonical target hurt dispatcher. The returned Boolean is discarded. A failed
tame getter or target hurt body retains the cooldown store and exact operands.

The unchanged-original [CPU packet](../../tests/reference/entity/wolf_attack/README.md)
contains 24 real calls on GI/JU objects with NULL Worlds and declared inputs.
It compares raw velocity and bounds bits, Random progress, cooldown, NULL
prefixes and the target's inherited attacked flag. These calls use the actual
original game methods without replacing or instrumenting them. Constructors
and their entropy are outside the comparison.

The native component passes 75 authored checks on Windows and Linux Clang
ASan/UBSan. It covers boundary
and NaN skips, Random hit/miss, airborne return, exact pounce words, zero-XZ
NaNs, late NULL/Math/FP/bounds failures, actual Item damage despite a preexisting
cooldown, the tame Byte cast and actual nonfatal Sheep damage. The 24
unchanged-original CPU rows also match on both hosts.

The original Wolf method now reaches the real [Sheep damage body](animal-damage.md)
after its cooldown store. Nonfatal bites apply health, hurt timers, knockback
and sound through the ordinary effect dispatcher. Fatal damage still stops
at the actual LivingEntity death/drop invocation after its reached sound.
These finite checks do not establish complete Wolf combat, death/drops, taming,
original World behavior or extended-play stability.
