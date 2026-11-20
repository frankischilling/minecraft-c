# Animal death CPU observations

This packet executes unchanged Beta 1.7.3 client methods for fatal damage,
Living death, concrete animal drops, Item construction and World publication.
The supplied client archive has SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The measured Windows x64 JDK17 capture passes 43 calls in 42 profiles. It
constructs 38 actual Items and inserts them into the original World and Chunk
collections. `provenance.json` records the tools, outputs and separate scope
of the ordinary and read-only JDI executions. Native replay has its own result.

Every profile uses actual constructors: `fd(new wy(), name, xa.a(0), 23L)`,
one actual empty `lm` at Chunk `(0,0)`, an animal and a Wolf attacker. The
authored scene installs that Chunk in the original `ok` provider's cache and
loaded list. Animal and attacker are not published; only dropped Items enter
the World and Chunk lists. Their actual provider, World and Chunk methods
execute during insertion. There is no substitute publication result, game
subclass, uninitialized allocation or replaced game method.

Most profiles call actual fatal `a(sn,int)` on Sheep, Pig, Cow or Chicken.
Direct inherited `ls.b(sn)` calls exercise that method's own behavior,
including a Wolf receiver whose actual drop ID is negative. They do not
represent Wolf's separate damage override. One profile invokes protected
`gi.a(sn,float)` through reflection and reaches actual fatal Sheep damage.
The capture helper prepares declared inputs, observes calls and reads state;
it does not implement damage, death or drop results.

## Profiles

`profiles.txt` has fourteen integer columns:

| Column | Meaning |
| --- | --- |
| 0 | Receiver: Sheep 0, Pig 1, Cow 2, Chicken 3, Wolf 4 |
| 1 | Call: fatal damage 0, direct inherited death 1, actual Wolf bite 2 |
| 2–4 | Wool color, sheared flag and fire timer bv |
| 5 | Receiver private Random seed |
| 6 | World mode: ordinary 0, remote 1, NULL receiver World 2 |
| 7 | Source operand: actual Wolf 0, NULL 1 |
| 8 | Observer mode, described below |
| 9 | Number of direct calls on the same receiver, one or two |
| 10 | Score field T |
| 11–12 | Declared NULL private Random and NULL watcher flags |
| 13 | Fixed pose selector, always zero in this packet |

The finite inventory is:

| Profiles | Calls |
| --- | --- |
| 0–15 | Fatal unsheared Sheep, every wool color |
| 16 | Fatal sheared Sheep, no drop |
| 17–19 | Pig quantity 0, 1 and 2 |
| 20–21 | Positive and negative Pig fire timers, cooked and raw pork |
| 22–24 | Cow quantity 0, 1 and 2 |
| 25–27 | Chicken quantity 0, 1 and 2 |
| 28–30 | Direct Wolf death, remote Pig death and NULL-World Sheep death |
| 31 | Fatal Sheep with a NULL attacker |
| 32 | Two actual direct Sheep death calls, with two distinct Stack/Item results |
| 33–34 | Observer throws after first Sheep or second Pig insertion |
| 35 | Item-added observer clears the receiver World; final death status fails at 54 |
| 36–39 | Negative T, NULL source, NULL quantity Random and NULL Sheep watcher |
| 40 | Actual untamed Wolf close bite kills an unsheared Sheep |
| 41 | Death-sound observer clears the receiver World; death's fresh B read fails at 38 |

Observer mode 0 records normal callbacks. Modes 1/2 throw an authored
`IllegalStateException` on the first/second Item-added callback, after genuine
World and Chunk insertion. Mode 3 changes the actual victim World field to
NULL inside the first Item-added callback. Mode 4 makes that change inside
the actual fatal sound callback. These are declared adversarial callback
inputs; no World method or attacker callback is replaced. Their records
preserve the completed mutations and published Items.

Receiver position is `(8.5,64.5,8.5)`, yaw 30, velocity `(.25,-.375,-.125)`,
Y offset bf `.125f` and bounds `(8,64,8,9,65,9)`. Its health is 4 before an
amount-4 hit, or 2 before the actual amount-2 Wolf bite. Direct death starts
with health 4 and leaves it unchanged. E is 20, Z is 17, by/au are zero,
av is 17, aa/ab are 3/4, ad is 7, ac is 99 and al is .25. Attacked, dying
and dead flags start false. T, fire, color and sheared state come from the
table; the actual Sheep setters prepare watcher state.

Wolf position is `(11.5,64.5,12.5)`, yaw 15, velocity `(-.5,.75,1.25)`,
cooldown 123 and grounded true. Its bounds are `(11,64,12,12,65,13)`, and
dead/attacked flags start false. The actual tame setter receives false. The
Wolf's private seed is 12345 and the World's private seed is 2468. Receiver
private seeds come from the table after ordinary construction. For full fatal
Pig/Cow/Chicken calls, seeds 3/0/2 select quantities 0/1/2 after the two real
sound draws. Direct death seeds apply before its quantity call without sound.

No shared Math seed is supplied. Constructors retain their actual private
clock entropy, and each run's shared Math generator retains its actual state.
No constructor result, clock, field or Random state is repaired after a call.
Each profile has a new World/Chunk/actor graph; repeated profile 32 retains
the same graph and stream through both calls.

## Records and entropy

Each row records its case/profile/step, complete numeric input, actual result,
exception category and named scope: `FULL`, `SOURCE_FAILURE` or
`PUBLICATION_CALLBACK`. Direct death and Wolf attack are void methods, so a
normal result is NULL. The state includes health/timers
`[Y,Z,E,by,au,av,aa,ab,ad,T]`, ac/al, velocity, pose/bounds, attacked/dying/dead
flags, watcher byte, World role, receiver/attacker/World Random states,
source cooldown, sounds, ordered callbacks and actual Item memberships.
Direct death sets ah without setting be or resetting ad. Delayed death
particles and removal are separate methods outside these calls.

Each Item records its actual Stack `[ID,count,metadata,animation]`, position,
previous position, velocity, yaw/pitch/previous angles/hover phase, size and
bounds. It also records Item age/pickup-delay/health counters, Entity timers,
flags, Chunk coordinates, actual private Random state, World role, World
membership, Chunk occurrence count and distinct Stack identity predicate.
Every nonzero quantity creates separate singleton Items. Stack count is never
substituted for the Random quantity. Snapshots are read after the actual call
or exception, including Items already inserted before callback failure.

Shared Math state and the full-width default-Random uniquifier are recorded
before and after every call. Each namespace independently checks the actual
four Math draws per Item against its hover, yaw and launch velocity words,
the complete shared LCG progression and one uniquifier advance per new Item.
The NULL-attacker full hit also checks its own direction draw and ac word.
All computations retain the source float/double narrowing and operation order.
These are consistency checks on captured original results, not a replacement
original constructor or an entropy input to native execution.

Read-only JDI separately records actual original entry/return frames, caller
bytecode positions, live Code hashes, default Random uniquifier/nanotime/seed
events, every reached Random result and each actual Item constructor return.
It verifies 38 default constructor seed formulas, 153 Math returns and all
234 Random returns. The 54 live original method pins span 17 reached classes.
Item constructor return records precede pickup delay 10 and publication.
The observer does not write fields, invoke game methods or force returns.
VM termination is used only to abort a failed observation run.

Native and original raw Item clock seeds and shared Math states are retained
as independent contexts. They are not equated, injected or normalized. The
comparator pairs all remaining typed fields after each process passes its own
Math/formula/uniquifier checks. Native constructor clock semantics remain the
native profile's separately qualified platform behavior; this packet does not
claim identical JVM and native clocks. The source-preservation predicate covers
the declared Wolf geometry, flags, health, private stream and World/Random/
bounds identities. Its separately emitted cooldown changes on the real bite.

## Boundaries and provenance

Original publication observers throw `IllegalStateException`; native adapters
return an authored I/O error. Their outcomes remain `CALLBACK` and `IO` under
the same `PUBLICATION_CALLBACK` scope. Only the already-completed source state
and memberships are paired. No Throwable implementation equivalence is claimed.
NULL World, quantity Random and Sheep watcher failures retain actual source
categories and their earlier health/ah/publication effects. Cleanup must not
repair those source fields or remove Items to manufacture a match.

This packet covers these finite CPU bodies and owners. Other damage overrides,
Player score/kill behavior, delayed death, Item ticking/pickup, save/reload,
ordinary Mob behavior, unrestricted combat, audio, rendering and original pixels are
outside its comparison. Native lifetime/checked-retirement controls belong to
the native component fixture; matching Java list contents alone does not prove
native reference ownership. Static pins, executed original capture and native
replay remain separate evidence.

The capture pins the entire client JAR, 22 relevant class hashes and 66 selected
Code bodies, including LS death, all concrete drop dispatch, generic SN drop,
Stack/Item constructors and real World/Chunk insertion. Selected classes must
resolve from that archive. Every JDI-reached Code body is checked against the
unchanged archive. Source, matching JDK17 tools, JDK modules/JVM, four SDK
metadata JARs and compiled helper hashes are unchanged before/after execution.
Only authored observer/controller/JSON helper classes may appear in output.

The original target denies and counts network connections/listens, native
loading, writes and deletes. JDI uses its own startup debugger transport before
that target guard is installed. Java tool-option injection variables are cleared.
Complete bounded subprocess receipts retain pipe/timeout/reader/output-limit
evidence. The JDI journal and target pipes have separate bounds. Raw game
classes, disassembly and assets are local inputs and are not published.
`finite.json` binds all capture source inputs. The actual JDI journal and
constructor contexts remain in the private capture directory, with hashes in
`provenance.json`.

Eight controls reject row identity/type/category changes, missing rows,
one-bit Item formula/Math/uniquifier changes, altered deterministic fields,
membership/identity failures, changed repeat state and corrupted source pins.
They also check that independently valid raw clock contexts remain separate.
The local run with the original JAR passes all eight with zero skips.

## Run

Original capture requires Windows x64 matching JDK17 tools and a new private
output directory:

```powershell
$jdk = 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin'
$libraries = "$env:APPDATA/.minecraft/libraries"
python -B tests/reference/entity/animal_death/capture_reference.py `
  --client-jar b1.7.3.jar --library-root $libraries `
  --java "$jdk/java.exe" --javac "$jdk/javac.exe" `
  --output .local/animal-death-original-repeat
python -B tests/reference/entity/animal_death/test_packet.py --client-jar b1.7.3.jar
```

Review original completion, JDI and unchanged-input receipts before replacing
the finite observations. Native replay requires no original JAR or Java:

```powershell
cmake --build build-windows --target mc_test_animal_death
python -B tests/reference/entity/animal_death/run_native.py `
  --executable build-windows/mc_test_animal_death.exe `
  --work-dir build-windows/animal-death-reference
```

For a Visual Studio build, add `--config Debug` and use its `Debug/` executable.
The runner invokes `EXE assets worlds --reference native.ndjson`, retains both
unmodified entropy contexts, audits them independently, compares the declared
fields and writes a PASS/FAIL receipt with before/after source/product hashes.
