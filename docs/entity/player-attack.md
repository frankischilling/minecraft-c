# Player attack and animal kill callbacks

The client Entity-target left-click branch now calls the actual inherited
`ob.b(gs,sn)` controller body and `gs.d(sn)` Player attack body. The ordinary
input owner retains a failed invocation for checked cleanup. Empty-hand
animal attacks have an earlier 747-check component snapshot matching five selected unchanged-original
rows on Windows and Linux with sanitizers. A bounded Windows SDK Sheep encounter
also returns through kill, visible Wool drawing and checked cleanup. This is
not complete combat or playable-client qualification.

## Source order

The attack reads Inventory damage through `ix.a(sn)` and its actual selected
main/armor array choice. Empty hand returns damage 1. If that damage is
positive, a negative Player Y velocity adds 1 before target hurt. The body then
calls the actual target damage override and discards its returned Boolean.
A resisted hit therefore still continues through the later reads and statistic
work. Health, hurt timers, knockback, sound and fatal drops use the existing
animal damage/death owners.

After damage returns, Player reads its current held Stack again. This fresh
read is separate from the initial Inventory damage read. The
[held Item bodies](../item/combat.md) now resolve all 38 actual Item Classes to
GM, DQ, QD or SA behavior. Item-array captures, Stack ID reads, durability and
statistics preserve their separate source order. A Living target then receives
an alive check. Only a still-living target enters the attack ally-alert call.
The damage-dealt statistic follows for Living targets, including resisted and
fatal hits. Fatal animal damage first executes the Player score and kill
callbacks inside the victim's ordinary death sequence.

The score callback adds the death score using Java integer wrapping, then adds
the appropriate kill statistic. The animal kill callback returns before the
Monster achievement branch. That branch remains an explicit dependency;
monster kills are not qualified by animal results.

## Owned Wolves

The actual `gs.a(ls,boolean)` attack alert excludes Creeper and Ghast targets,
a tame target Wolf owned by this Player, and Player targets under the original
false Player-attack permission. It captures the Player's World before creating
the pooled position-to-position-plus-one box and expanding it by (16,4,16).
The World query returns real Wolf occurrences in its source order.

The loop checks each Wolf's tame flag, current target, exact owner String and
sitting flag. Eligible nonsitting owned Wolves with no target receive the real
target through retained Creature fields. Watcher reads and the setter preserve
their actual source order and dirty-state behavior. The query list, World,
pooled boxes and target references remain owned until native retirement.

## Picking and Wool drawing dependencies

All eleven canonical Overworld Mob classes now use their inherited LivingEntity
pickability and Entity border bodies. The pickability body reads only the
removed flag; zero health and RAW constructor state do not add hidden gates.
Native controls exercise every class before and after setting that flag and
check the unchanged 0.1 border. Original reflection resolves each concrete
class to `ls.h_()` and `sn.m_()`; their method bytes and class identities are
pinned in the packet. This is separate from the five attack-state rows.

Wool now enters its inherited white inventory-color and no-op shape hooks.
Native controls cover metadata 0 through 15, exact literal texture indices on
all six faces and unchanged nondefault bounds. Original reflection resolves
Wool to `ee` and both hooks to `uu`; sixteen actual original color calls return
white. The reached Wool Item renderer also preserves BI icon selection through
`(~damage) & 15`; sixteen actual original icon calls and the inherited GM leaves
are pinned separately. These controls do not establish original Wool pixels.

## Failure ownership

Every allocated attack invocation survives success or failure until checked
retirement. Player, target, Registry and held Stack occurrences stay live for
their reached prefix. Failures after target damage keep that victim's pending
death work, including when the caller omitted the error output. Retirement
uses the same victim. Active work or refused child cleanup preserves the call
for retry; it never repeats source damage, scores, statistics or publication.
Completed health changes, Wolf targets and published Items are not rolled back.

The source NULL/array/Class exceptions and native ownership/allocation failures
remain distinct. C bounds checks preserve defined host behavior while recording
the reached Java exception category. Normal gameplay does not need Java.

## Evidence and remaining work

The [original packet](../../tests/reference/entity/player_attack/finite.json)
records five calls on genuine original App, World and Player owners. Normal
Cow hurt leaves health 9 and damage-dealt 1; negative Y velocity leaves health 8
and damage-dealt 2. The resisted Cow keeps health 10 while damage-dealt still
increments by 1. Fatal Sheep hurt sets the dying flag, publishes one wool Item,
and changes Score from 3 to 10 with one Mob kill. An owned nearby Wolf acquires
the surviving Cow as its target.

These rows compare selected deterministic integers and Booleans. Original
constructor and shared Math entropy remain independent; no velocity-word,
complete Random, full World graph or pixel correspondence is claimed.

Earlier Windows and Linux Clang ASan/UBSan snapshots each complete 747 authored checks and match
the five original rows. The historical final held-Stack control verified the reached
`iz.a(sn):9` frontier before any target hurt or statistic change, then checked
cleanup. Windows completes the component in 0.20 seconds and Linux in 0.28
seconds; their enclosing tests take 0.23 and 0.30 seconds. Earlier 743-check
snapshots precede this diagnostic/frontier control. Controls cover RAW/removed
Mob picking, source ordering,
null and held Item frontiers, owned-Wolf filtering and checked retirement. Six
packet tests reject semantic and packet-shape corruptions. Broader final
qualification retains its own snapshot scope.

An earlier bounded Windows SDK encounter passes 507 checks in 10.37 seconds. Its authored
Sheep has health 1, death score 7 and color 0 before real World publication.
Normal source picking finds that Sheep; actual SDK left click executes fatal
damage, score/kill statistics and one Wool Item publication. Three World/HUD
frames return with visible Wool, then checked cleanup closes both windows.
The run retains 1,393 selected runtime/harness inputs and 413 resources unchanged;
other Python helpers, toolchain and desktop state are outside that inventory.

The earlier fixture expected damage-dealt 1 and failed. A read-only death-sound
callback now samples the Player operand while the attack is active. Its exact
Y-velocity word `bfb41205c28f5c29` is negative, so the original downward bonus
requires damage 2. The actual health becomes -1 and damage-dealt becomes 2.
The fixture derives this expectation from that one sample without repairing
Player fields. Earlier graphical dependency failures remain separate prefixes.
A preceding Windows SDK snapshot passes 1,230 checks in 13.90 seconds. It
waits under the ordinary Timer for twenty distinct [GY explosion particles](explosion-particle.md),
actual World removal at death age 21 and three further World/HUD frames. It
also reaches the real period-40 autosave after removal. A test-only passive
observer captures source entry/exit: old time 39, next time 40, 24 eligible
Chunks and save return before the time store. Independent NBT/region/disk
verification passes, including saved time 39 and Player Score 14. The observer
is compiled out of the normal client. Its phase/maps/counts audit preserves
380,628 cold EntityList snapshot bytes before and after observation. The
observer cannot initialize that Registry; actual source active uses retain
their original order. The strict region gate protects
abandoned payloads and reused tail padding outside each new record. Declared
execution inputs/resources stay
unchanged, and both native windows close through checked cleanup. The paired
autosave corruption test passes sixteen cases in 0.91 seconds; both tests pass in 14.85 seconds.

The earlier frozen thirteen-test Windows cohort failed at missing GY construction
after twelve tests passed in 101.48 seconds. A later 602-check native death
continuation returned, then failed the old immutable-disk predicate on a real
autosave. Both retain their historical scope. The pre-GY Linux build and sixteen
focused sanitized tests passed in 27.76 seconds; six later particle regressions
passed in 0.63 seconds. The broad Linux snapshot passes its full build and all
597 public tests in 166.82 seconds under Xvfb/software OpenGL with display tests,
Clang ASan/UBSan, leak detection and halt-on-error. Its source scope precedes the
later 747-check component diagnostic/frontier control; the Linux SDK encounter
remains unqualified.

The historical Windows full run passed 593/602 in 215.03 seconds with 3,293 selected
inputs and 823 resources unchanged. Slime/Cow failures record actual foreign
Remote Desktop foreground activation; seven source-binding gates reject metadata
stale after diagnostic-fixture changes. Their declarations are now refreshed;
the exact seven tests pass separately in 13.29 seconds with comparisons and
guards unchanged. The historical 593/602 result remains failed; that focused
rerun did not itself establish a full passing result.
A preceding 26-test run passes 24 in 188.02 seconds, including local attack in
12.65 seconds, and has instrumented Slime/Wolf `msrdc.exe` focus failures.
Whole Windows focus stability remains unqualified. Earlier uninstrumented
owners remain unknown. The [qualification record](../testing.md#current-targeted-and-public-qualification)
retains the older broad failures, isolated passes, exact owner receipts and
unchanged byte predicates. Hosted evidence keeps its exact tested-commit scope.

Those earlier bounded encounters establish no original graphical equality, Wool
pickup, save/reload or extended combat result. The
[packet provenance](../../tests/reference/entity/player_attack/provenance.json)
records source pins and the original observation scope.

The newer [held Item packet](../../tests/reference/entity/held_item_attack/README.md)
adds 422 exact selected rows and three independently audited lethal Cow rows
without changing those five unarmed observations. The Windows component
selection passes eight tests in 17.75 seconds, including 425 genuine attack
calls and 37,416 checks. Source damage, DQ/QD durability, SA Saddle behavior,
late Stack/Item-array reads and real Inventory clearing are implemented. Ten
attack-packet and eight geometry-packet corruption tests pass. Twelve Windows callback/order
controls also pass with the five unarmed rows unchanged. The reached
[damaged-Item overlay](../client/gui-ingame.md#damaged-item-overlay) now has a
passing selected CPU comparison: nine profiles, 24 entered rectangle prefixes
and 576 active words, with each process's inactive backing validated separately.
The original completes 24 draws; the native CPU prefixes complete zero. Eight
overlay packet controls and the three-test selection pass in 0.76 seconds.
The same frozen runtime passes the Windows SDK Dirt profile in 14.20 seconds
with 1,446 checks and the Iron Sword break profile in 16.74 seconds with 3,631
checks. The isolated unarmed profile passes in 13.22 seconds with 2,106 checks.
Each observes three hand frames before and after the actual click, twenty GY
particles, removal at death age 21, a real post-death autosave and cleanup.
Independent decoding matches all 40 live Inventory slots to the NBT at every
actual save entry and in the final disk image. These latest three runs publish
Wool without picking it up. A separate earlier 13.38-second Sword pass proves
actual Wool pickup and its slot-0 clone; that result is not combined with the
latest run's counts.

The frozen runtime passes all 611 Windows public tests in 280.15 seconds and
all 606 sanitized Linux public tests in 145.58 seconds. Earlier GUI-color,
Dirt-hand, maximum-durability, focus/Pause and unconsumed-click failures remain
historical failures. A failed all-World conservation check counted an unrelated
distant Seed Item, ID 295; the observer now follows only the actual active
`GS.d` Sheep producer. Sixteen public Inventory corruption controls run in CI.
These passing finite scenarios do not establish global focus stability or a
Linux saved-World SDK pass. Hosted checks apply to their exact tested commit.

Remaining target hurt/death overrides, Monster
kill achievements, Player damage/death/respawn, right-button Entity interaction,
Inventory UI and extended natural encounters remain unfinished or unqualified.
The qualified native encounters do not establish unrestricted combat or original
whole-frame/pixel equality.
