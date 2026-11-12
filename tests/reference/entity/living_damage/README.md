# Living damage CPU observations

This packet records 27 calls to unchanged Beta 1.7.3 client damage methods.
The locally supplied client archive has SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The original JDK17 capture passed with complete bounded stdout/stderr capture
and no attempted network access, native-library loading or filesystem writes.
`provenance.json` contains the measured tool, helper and output fingerprints.
Native comparison runs separately through `run_native.py`.

The observer constructs the actual original World with
`new fd(new wy(), "LivingDamageReference", xa.a(0), 23L)`, then constructs
actual Sheep and Wolf objects with that World. These are ordinary original
constructors, with no uninitialized allocation, subclassed game objects or
replaced methods. The real World sound body calls an authored `pm` observer.
The actors are component operands and are not published into loaded Chunks.
No World query, terrain, sound device or whole-frame behavior is compared.

Most rows invoke actual `dl.a(sn,int)`. Its inherited call resolves directly
to `ls.a(sn,int)`; neither Animal BG nor Creature II contributes another
damage body. Two rows invoke the actual protected Wolf `gi.a(sn,float)` by
reflection, reaching Sheep damage with untamed and tamed bite amounts.
The observer prepares inputs and reads results; it contains no damage,
direction, knockback or Random implementation.

## Inputs and records

`profiles.txt` declares 25 profiles. Profile 24 performs three successive
damage calls, with amounts 4, 3 and 7, on the same Sheep. Each other profile
performs one call. Its sixteen columns are:

| Column | Meaning |
| --- | --- |
| 0 | Path: direct Sheep damage 0, actual Wolf bite 1, three-call sequence 2 |
| 1 | Damage amount; the sequence uses 4, 3 and 7 |
| 2–4 | Initial Sheep health Y, immunity timer by and previous damage au |
| 5 | World mode: ordinary 0, remote 1, NULL Sheep World 2 |
| 6 | Attacker operand: actual Wolf 0, NULL 1 |
| 7 | Sheep private Random: constructor object 0, declared NULL 1 |
| 8 | Sheep bounds: constructor object 0, declared NULL 1 |
| 9 | Attacker horizontal offset index |
| 10–11 | Sheep yaw as raw float bits and initial vertical velocity as raw double bits |
| 12–13 | Shared Math Random seed and Sheep private Random seed |
| 14 | Wolf tame flag |
| 15 | Ordinary sound observer 0, throwing sound observer 1 |

Sheep position is `(8.5,64.5,8.5)`. The Wolf has the same Y and an X/Z offset
of `(3,4)`, `(-2.8,3.3)`, `(-3,-4)`, `(0,0)`, `(.005,.005)` or `(.01,0)`.
Positions are prepared by those actual double additions. For the last offset,
subtracting 8.5 from the rounded Wolf X gives slightly less than .01, so the
original small-distance retry loop does run. The capture retains that result;
it does not replace the authored positions with an idealized distance.

Sheep velocity starts at `(.25, profile Y, -.125)`, bounds at
`(8,64,8,9,65,9)`, Y offset bf at `.125f`, E at 20, Z at 17, av at 17,
aa/ab at 3/4, ac at 99, al at .25, bb false and ah false. Its actual sheared
setter receives true, which makes the original fatal control avoid wool
drops. Wolf yaw is 15, velocity `(-.5,.75,1.25)`, cooldown ae 123, grounded
true, bb/be false, and bounds extend .5 around its X/Z with Y bounds 64–65.
Wolf bite distance is the declared float 1. These are component inputs,
independent of the ordinary geometric distance between these operands.

The actual private Random objects receive seeds after construction and tame
preparation. Wolf private seed is 12345, World private Random seed is 2468, and Sheep and shared Math
seeds come from the table. The shared seed belongs to the JDK's actual
`Math.random` generator. Constructors retain their ordinary entropy;
constructor values are outside this damage comparison. No state is repaired
after a call, and calls are not retried. NULL fields are declared fault
inputs prepared through reflection, including the final bounds reference.

Rows contain typed case/profile/step identities and the complete input row.
The state records health and timers `[Y,Z,E,by,au,av,aa,ab]`, ac/al float
words, all three velocity double words, attacked and dying flags, position,
yaw, visible bounds, actual 48-bit Sheep/Wolf/World/Math Random states,
Wolf cooldown and actual sound arguments. Sounds are cumulative during the
three-call sequence. NULL Random and bounds remain NULL in the records.

`source_unchanged` observes Wolf position, velocity, bounds contents, yaw,
grounded/dead/attacked flags, health and private Random state, plus World,
Random and bounds object identities. Its cooldown is excluded from that
predicate and emitted separately because actual Wolf bites set ae to 20.
The native fixture separately checks its genuine owner holds and cleanup;
matching a boolean does not establish JVM/native object identity equality.

The cases exercise ordinary and asymmetric knockback, yaw subtraction, the
vertical velocity cap, repeated weaker/equal/stronger hits, the exact by=10
full-hit boundary, remote and nonpositive-health returns, NULL World and
private Random failures, NULL attackers, unused NULL bounds, coincident
and near-coincident attacker loops, negative/zero damage and actual Wolf
bites. Comparison uses exact raw words and typed values, with no tolerance
or inferred bytecode positions from source stack lines.

## Explicit comparison limits

Profile 19 is a fatal full hit. The original observer takes a read-only
checkpoint inside the actual fatal sound callback, after both private Random
draws and all sound arguments. The original then returns from World sound,
enters `ls.b(sn)` at caller position 316 and completes the actual sheared
Sheep death. Its result and complete after-state remain in `completion`.
The native implementation now completes that same sheared death and returns
true from damage. Its ordinary `RETURN` row contains the actual terminal
state. Replay compares every terminal primitive, Random state, owner predicate
and sound argument against the original `completion.state`, including the
dying flag set by death. The historical sound checkpoint remains unchanged;
it is not presented as a native checkpoint. The original capture inputs and
their pinned hashes also remain unchanged. The native runner validates its
current terminal-row format separately from the pinned original capture reader.

Profile 20 declares a NULL private Random during a fatal hit. The original
throws at the first fatal sound Random invocation, before entering death.
Profile 21 instead throws an authored `IllegalStateException` from the real
World sound observer. Its native adapter returns an authored I/O failure.
Those distinct outcome categories remain `CALLBACK` and `IO`. Comparison
covers their already-completed damage and sound state at that boundary;
it does not claim matching Throwable or callback implementations.

These are finite Sheep and Wolf CPU observations. Other Living subclasses,
Player armor, revenge/target selection, ordinary AI and World scheduling,
Item drops, delayed death ticks, complete combat, audio, GL and original pixels are outside
the packet. Static pins, original execution and native replay are separate
evidence. A passing original capture alone establishes no native result.

## Guards and execution

The packet pins the complete original archive, twelve relevant class hashes
and 33 selected Code bodies, including Living damage/knockback, IN sqrt,
concrete Sheep sounds, actual World sound/status and the Wolf bite route.
Selected classes must resolve from the original archive. Only the authored
observer, security guard and sound observer may appear in the helper output.
Published files contain authored inputs and capture code, decoded primitive
observations, hashes and provenance. Original classes, raw disassembly and
assets stay local.

Capture requires matching JDK17 tools and four pinned LWJGL/JInput/JUtils
metadata JARs. Java tool-option injection variables are removed. Network,
native loading, writes and deletes are denied and counted. The two
`--add-opens` options permit observation of actual Random states and access
to the actual shared Math generator for declared seed preparation. Tools,
source, archive, SDK JARs and compiled helpers are checked before and after
execution. Complete bounded subprocess receipts retain exit status, timeouts,
reader errors, output limits and hashes. The finite packet binds its capture
inputs; changing them requires a new original capture.

Seven control tests reject missing/reordered rows, type coercion, duplicate
keys, changed inputs, one-bit primitive/Random/sound changes, owner predicate
changes, one-bit fatal terminal-state changes, invalid fatal/callback scope
substitutions and corrupted Code pins.
The measured local run with the original archive passes all seven without
skips. Without an archive, only the archive identity control explicitly skips.

Use a new private output directory for each original capture:

```powershell
$jdk = 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin'
$libraries = "$env:APPDATA/.minecraft/libraries"
python -B tests/reference/entity/living_damage/capture_reference.py `
  --client-jar b1.7.3.jar --library-root $libraries `
  --java "$jdk/java.exe" --javac "$jdk/javac.exe" `
  --output .local/living-damage-original-repeat
python -B tests/reference/entity/living_damage/test_packet.py --client-jar b1.7.3.jar
```

Native replay needs the focused native executable and no original archive:

```powershell
cmake --build build-windows --target mc_test_living_damage
python -B tests/reference/entity/living_damage/run_native.py `
  --executable build-windows/mc_test_living_damage.exe `
  --work-dir build-windows/living-damage-reference
```

These paths assume a single-configuration Windows build. With Visual Studio,
add `--config Debug` and use the executable under `Debug/`. The runner creates
declared native fixture resources and a separate World directory, invokes
`EXE assets worlds --reference native.ndjson`, and retains the native rows,
stdout/stderr, comparison receipt and immutable input/product hashes.
The fatal completion comparison and distinct callback outcome pair are recorded
explicitly in that receipt.
