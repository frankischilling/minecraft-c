# Wolf damage

The canonical client Wolf now executes its complete `gi.a(Lsn;I)Z` wrapper around
the existing Living damage body. Its first operation clears sitting, even when
the later remote-World, dead-health or hurt-resistance branch rejects the hit.
A nonnull source that is neither a Player nor an Arrow changes the amount to
Java-wrapped `(amount + 1) / 2`. The original Arrow call passes its Living shooter as
the damage source, so a Skeleton's amount 4 reaches this parent as 2.

After a successful parent, the wrapper reads the current tame and angry flags.
An untamed, calm Wolf hit by a Player becomes angry and targets that Player.
A direct Arrow argument may then resolve its current shooter through the two
original field reads. For a Living source, the wrapper constructs the original
position-to-position-plus-one box, expands it by `(16,4,16)`, and queries the
actual Wolf Class. Every untamed Wolf without a target receives that source;
Player sources also make those Wolves angry. This loop does not clear the
other Wolves' sitting flag. Already targeted, tamed and outside Wolves retain
their previous state.

The tame/angry branch skips null and self sources. A tamed Wolf keeps its target
when the Player's name matches the owner through `String.equalsIgnoreCase`.
That comparison uses the project's counted UTF16 helper and pinned JDK17 simple
case mappings, including supplementary and malformed-surrogate cases. Other
eligible sources replace the actual owning Creature target occurrence.

## Source evidence

The unchanged client archive SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its Wolf class SHA-256 is
`a9515317c9159edbdff01f97242b3ebfdb8ee735b3e839965f93c48692eec906`.
The hurt method's 285 Code bytes have SHA-256
`51890f2399c649ca8c19cff5954257bb1dcc2c30c60c9e1f43aab901067436b1`.
These identities include distinct class/constant-pool authority; a Code hash
alone does not identify a String literal's value.

Relevant bytecode sites are:

- Sitting setter at 2, wrapped addition at 25, division at 27 and direct
  inherited Living call at 32.
- Tame/angry getters at 39 and 46, own anger store at 61 and target store at 66.
- Direct-Arrow shooter reads at 80 and 90, captured query World at 101,
  pooled box at 137, expansion at 149 and actual Class query at 152.
- Queried tame getter at 195, target read at 203, target store at 212 and
  anger setter at 225.
- Late tame getter at 244, owner comparison at 268, target store at 278,
  and actual return instructions at 275, 282 and 284.

The inherited sound leaves are `mob.wolf.hurt` and `mob.wolf.death`, both at
volume `0.4f`. Wolf's drop ID method returns `-1`; fatal damage still executes
the complete Living death callbacks, dying flag and status continuation.

## Ownership and verification

One actor-owned pending call retains its actual receiver and current source
local, the captured query World, both pooled box occurrences, the actual class
result List and any exact failed query receipts. Its parent error is stable
private storage rather than a pointer into the public facade's temporary frame.
Only a newly acquired parent death is included in cleanup. Earlier pending
work is rejected, and neither cleanup nor retry replays damage or group stores.
Typed diagnostic pointers expire when checked cleanup consumes the call.

The [shared damage execution entry](damage-execution.md) also enters this same
wrapper from Arrow741. It uses the caller's actual defining runtime, Registry and
shared Math context. World101 is captured before the Class and bounds operands;
the actual class query keeps its stable canonical result through protected capture
and recorded native close. Copying a diagnostic does not acquire a receipt or
close authority, and unqualified query failures remain explicitly unresolved.

`entity.wolf_damage` uses real World, Chunk, Wolf, Player, Cow, Arrow, Item, watcher,
String and result-List owners. Authored controls cover group membership and
order, existing targets, tame/angry/self/nullable branches, direct Arrow shooter
resolution, case-insensitive ownership, wrapped/zero amounts, remote/resisted
hits and fatal death. Callbacks change flags, watcher type or the Wolf's World
after the captured hurt sound. Failure controls check partial effects, active
reentry refusal, retained parent death, native refusal while retiring bounds, provenance
failure and checked cleanup without source-field repair.

The existing public damage facade and the owned Arrow impact route share this
wrapper. Their authored group, owner and cleanup controls retain their separate
finite scopes; the original component captures are not retroactively expanded by
the join. Complete Player wake, delayed Slime splitting and arbitrary cold/foreign
graphs remain outside the complete owned combat path. These results do not
establish unrestricted Wolf behavior, original audiovisual equality or FPS parity.
