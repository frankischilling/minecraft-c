# Client Mob spawning

The client implements spawn predicates for the eleven default Mob kinds:
Spider, Zombie, Skeleton, Creeper, Slime, Pig, Sheep, Cow, Chicken, Wolf and
Squid. Each call uses the actual Mob, its instance Random, its current bounds
and its captured World. The caller supplies the application's actual
MathHelper and World class namespaces. No substitute World or spawn answer is
installed.

## Predicates

The four Monsters test saved sky light against `nextInt(32)`, read block light,
and test it against `nextInt(8)`. Thunder temporarily sets the actual sky
subtraction to 10 for a second light query. The original restores the previous
value only after that query returns; a failure retains 10. Creature eligibility
then calls Living's entity, block-collision and liquid checks before evaluating
the virtual path weight. Monster path weight performs another brightness query.

Animals first require actual grass below and plant light greater than 8, then
use the same Creature/Living checks. Their path weight is 10 over grass;
otherwise it reads brightness and subtracts 0.5. Wolf inherits this behavior.
Squid only tests Entity collision; it does not borrow the land predicate.

Slime acquires the real Chunk before checking watcher size or difficulty. It
uses its own `nextInt(10)`, a fresh Random from that Chunk's seed method with
salt 987234911, and finally the current height below 16. Normal and Blank
Chunks use their own stored World and coordinates in the source's wrapping
integer/long formula. The installed Blank fallback has real coordinates 0,0;
requested cache coordinates never replace them.

## World loop and ownership

The World loop constructs the selected Mob, draws fresh World Random yaw and
sets its pose before invoking eligibility. A false result continues the real
three-by-four attempt loop. A true result increments the per-Chunk group count
before World publication, ignores publication's returned boolean, runs postspawn
work and tests the virtual group limit. The source's cumulative count addition
and its group-limit jump remain distinct.

Sheep postspawn draws its color from World Random and updates the actual typed
watcher Byte. Spider postspawn makes its one-percent jockey draw. A selected
jockey executes real Skeleton construction, pose and publication before the
unfinished mount body stops at helper bytecode 56. Other kinds reach the
postspawn return without an extra Random draw.

The retained scheduler owns its current candidate and any reached jockey.
World and Chunk publications own separate occurrences. Source effects survive
later failure; retirement releases native ownership without undoing source
fields or replaying an attempt.

## Evidence boundaries

The [original predicate package](../../tests/reference/entity/mob_spawning/README.md)
keeps ordinary and read-only JDI observations separate from native controls.
Controlled original subclass failures are labelled separately from calls through
the actual World methods. Native qualification does not manufacture a second
light-query failure, a provider with absent generator/loader, or arbitrary Blank
Chunk coordinates when those concrete ownership seams do not exist.

These predicates and publication branches do not establish Mob AI, Mob ticking,
Entity drawing, a complete ordinary World tick or playable single-player.

## Qualified native controls

All 68 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 77 Windows tests pass in 105.78 seconds and all 73 Linux tests in
141.36 seconds, without failures, skips or sanitizer findings. All 2,387 C,
helper, build and reference inputs remain unchanged through both builds and tests.
Formatting checks 1,665 files; source bindings cover 7,766 entries and 3,324 files.

Each host passes 1,667 constructor checks over 33 actual constructors, with 8,244
independent original field comparisons and 165 separately labelled scalar checks.
Spawning adds 6,451 checks over 115 genuine predicates and 18 Chunk Random leaves;
136 selected predicate/Chunk/thunder rows match the original exactly. Original
constructor capture checks 25 live methods and 561 checkpoints. Predicate capture
checks 19 live methods and 2,138 checkpoints. Independent JVM/native constructor
entropy is not equated.

The nine integrated World controls retain eleven preceding original prefix
comparisons and use authored stored terrain for the later native loop. Positive
Wolf group-limit8 and publication-false postspawn are not integrated reference
cases. The original protected BG path-weight entries, controlled second/third
light-query failures and unavailable Blank/provider construction seams remain
separate from native parity. Mob NBT loading/saving also remains unfinished.
