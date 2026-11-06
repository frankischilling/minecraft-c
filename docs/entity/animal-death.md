# Animal death and drops

Fatal client Sheep, Pig, Cow and Chicken damage now enters the inherited
LivingEntity death body at `ls.a:316`. Direct inherited death calls also admit
Wolf. The Wolf damage override remains a separate unfinished body; accepting
its inherited death method does not supply that override.

## Death order

`ls.b(Entity)` first tests the victim's score field. A nonnegative score and a
nonnull attacker invoke the attacker's score callback, including when the
score is zero. A second independent nonnull test invokes its kill callback.
The admitted animal and Wolf attackers inherit both empty Entity declarations.
Other concrete attacker overrides remain explicit dependencies.

The method then stores the dying flag `ah`, captures the current World and
reads its remote flag. Only the drop call is skipped in a remote World. After
drops, a fresh World capture receives status byte 3. The actual client World
status declaration returns immediately. A null World at either reached read
preserves every earlier callback, flag, drop and Random operation.

Death does not set the removal flag, reset health or death time, or award
experience. A direct repeated death invocation can repeat drops. Ordinary
damage instead has its existing nonpositive-health return before this body.

## Concrete drops

| Receiver | Drop |
| --- | --- |
| Unsheared Sheep | One wool Stack, count 1, with the independently read current color |
| Sheared Sheep | None; no Block Registry use or Stack allocation |
| Pig | Zero to two distinct singleton raw pork Items; positive fire time selects cooked pork |
| Cow | Zero to two distinct singleton leather Items |
| Chicken | Zero to two distinct singleton feather Items |
| Wolf | None; its negative drop ID skips the quantity draw |

Sheep allocates its RAW Stack before the actual wool Block static read. Its
sheared and color getters make separate DataWatcher reads. Pig, Cow and
Chicken resolve their actual Item statics before one private `nextInt(3)`
quantity draw. A quantity of two creates two Stacks and two Items, each through
the source loop. No aggregate Stack substitutes for those invocations.

## Shared Item construction

The retained Entity drop module implements the inherited numeric and Stack
drop methods for these four animals and Falling Sand. Numeric entry allocates
a RAW Stack and constructs the supplied ID and count with metadata zero.
Sheep passes its actual color-bearing Stack to the Stack entry.

Stack entry allocates the actual RAW Item, captures the constructor World
before its coordinate arguments, and evaluates Y plus the widened float
offset, including an offset of positive zero. The canonical Item constructor
alone supplies its constructor state and entropy. The caller stores pickup
delay 10, independently captures the publication World, and invokes ordinary
World insertion. A false insertion result is discarded and the Item still
returns. A callback failure after insertion preserves actual World and Chunk
membership. This method adds no inventory statistic or launch-velocity rewrite.

## Failure ownership

`mc_entity_effect_death_in_context` and fatal damage leave failed death work
on the actual victim. That work holds the victim, reached attacker occurrence,
Registry, Worlds, Stack, Item and publication producer as required. An optional
or copied effect diagnostic owns none of those resources. Its typed receiver
and child views borrow the actor's pending work and expire together when
cleanup succeeds.

`mc_entity_effect_has_pending` and `mc_entity_effect_cleanup` let callers find
and drain this work even when the original call had no error output. Checked
cleanup refuses during an active callback and preserves remaining owner slots
for retry. It neither replays source work nor removes published Items. A new
damage or Mob tick refuses while that actor has pending death work. World
terminal admission also refuses before changing its graph.

Mob and World error retirement follow the actual failed effect's typed victim
view. This matters when a Wolf bites a target retained only by a relationship,
outside the World's loaded Entity list. Cleaning the Wolf alone would miss
the victim's pending work.

Delayed removal remains the existing LivingEntity tick behavior. Death time
increments until it becomes greater than 20, then invokes the inherited empty
closure and sets the removal flag before twenty explode particles. This later
path does not run drops again.

## Evidence and limits

Windows MSVC Release and Linux Clang18 ASan/UBSan each pass 2,709 animal-death
checks and 69 shared Entity-drop checks. Native replay matches 43 original
calls in 42 profiles,
including all wool colors, quantity zero/one/two, Pig fire selection, repeated
direct death and a real fatal Wolf bite. Each process independently audits 38
new Item constructor occurrences and 153 shared Math draws. The original
read-only JDI run validates all 234 reached Random returns and 54 live method
pins. Eight original-packet controls pass with the supplied archive.

Authored controls additionally exercise active cleanup refusal, RAW Stack
allocation before a later Registry failure, a fresh second watcher read,
NULL error output, stronger repeated fatal damage and a victim outside the
World's loaded list. A separate retirement-interface control transfers the
actual failed Wolf diagnostic into a fresh World Mob child; it does not claim
an executed World tick. World terminal admission rejects that pending target
without changing memberships, bindings or callback progress.

The complete Windows and Linux builds pass. Seventeen selected Windows tests
pass in 25.03 seconds; sixteen Linux tests pass in 12.84 seconds, followed by
the separate terminal-inventory regression in 0.52 seconds. Linux uses leak
detection and sanitizer halt-on-error. These are component and ownership checks;
the full Windows creation/save/reselection lifecycle has its own native receipt.

The [animal death packet](../../tests/reference/entity/animal_death/README.md)
records static source identities and unchanged-original CPU observations.
The [damage packet](../../tests/reference/entity/living_damage/README.md)
separately covers health, repeated hits, knockback and sounds. Public native
controls use actual Registry, World, Chunk, animal, Wolf, Stack and Item owners.
Executed-original comparisons, authored failure controls and graphical session
checks retain their separate scopes.

Player death, other Mob drop overrides, Player attacks, inventory interaction,
respawn and complete combat remain unfinished. These finite checks do not
establish unrestricted play or original graphical and audio equivalence.
