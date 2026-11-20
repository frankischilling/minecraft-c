# Wolf attack CPU observations

This packet executes the unchanged protected `gi.a(sn,float)` method from the
locally supplied Beta 1.7.3 client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The JDK17 original capture passes 24 primitive rows with complete bounded pipe
capture and zero network, native-library or filesystem-write attempts.
`provenance.json` records the measured tool versions and output hashes.
Native replay has its own result through `run_native.py`.

The observer constructs actual `gi(NULL World)` and `ju(NULL World)` objects,
prepares declared source fields, and invokes the original protected method
through reflection. It does not replace game methods, rewrite bytecode or
calculate attack results. The class origin guard requires GI, JU, SN, LS,
MathHelper IN and EQ to come from the pinned archive. The complete archive,
original classes, disassembly and assets remain local inputs. Published files
contain authored observation/preparation code, numeric inputs and results,
class/Code hashes and capture metadata.

## Profiles and observed values

`profiles.txt` declares eight values per row: actor private Random seed,
distance as eight hexadecimal float bits, grounded flag, position index,
tame flag, target mode, bounds mode and initial attack cooldown. Seeds are
applied with actual `Random.setSeed` after construction and the tame setter.
The target's independent private Random seed is 12345. Constructor entropy
is outside the attack comparison and is not repaired or substituted.

Actor position is `(8.5,64.5,8.5)`, velocity `(.25,-.375,-.125)` and bounds
`(8,64,8,9,65,9)`. Target position is actor position plus one of these declared
offsets: `(3,0,4)`, `(3.3,0,-2.8)`, `(0,3,0)`, `(0,0,0)`, `(0,2,0)`,
`(-5.25,-1,2.125)`, `(16777215.75,0,-256.75)`. Its bounds extend .5 units
around its position on each axis. Target velocity is `(-.5,.75,1.25)`, block
ID 12, age 7, dead flag false and attacked flag false. These bounds and scalar
fields are explicit component inputs, independent of constructor geometry.

The 24 cases cover exact distance boundaries 1.5, 2 and 6; positive infinity
and a quiet NaN; a Random miss; a successful Random draw while airborne;
ordinary, asymmetric, large-coordinate and zero-horizontal-length pounces;
the float values immediately above 2 and below 6; tame and untamed bites;
strict vertical overlap boundaries; negative distance; and a disjoint target.
Four cases select a NULL target or NULL target/actor bounds. The pounce with
NULL target bounds proves that the successful pounce branch never reads
those bounds. NULL bounds are declared fault inputs prepared through
reflection, including the original final SN bounds reference. They are not
ordinary constructor outcomes.

Every row records raw double words for all actor velocities, both positions
and both visible bounds, plus the cooldown, actual 48-bit Random state after
the call, exception category, target attacked flag and a target-preservation
observation. The target-preservation check covers its declared position,
velocity, bounds, block ID, age, dead flag, private Random state and object
identities. Successful bites legitimately set inherited `sn.bb` through
`sn.a -> ai`, then return the actual false damage result. The separate
`target_attacked` field captures that store. Other declared target fields
remain unchanged.

NaN velocity words from a zero-horizontal-length pounce remain exact bits in
this measured JDK17/x64 profile. The replay does not normalize NaNs or use a
tolerance. It compares the complete ordered row inventory and each typed
field, including Random progress and failure prefixes. Java source bytecode
positions are not inferred from exception stack lines.

## Scope and guards

The source GI method has 200 Code bytes, ending at return 199. The static
packet pins complete GI/JU/SN/LS/IN class hashes and selected constructor,
attack, tame, position, square-root and inherited damage/attacked Code hashes.
Those metadata checks are distinct from the actual original attack execution.
The helper contains no reconstructed attack or Random algorithm.

FallingSand provides an actual target whose inherited SN damage method
returns false after the attacked store. Full Living damage, Wolf revenge,
ordinary AI selection/ticking, World queries and scheduling, GL and pixels
remain outside this packet. The native authored unit controls also exercise
actual Item damage and an explicit Living hurt dependency; this reference
does not turn that unsupported Living child into a successful attack.

Capture requires matching JDK17 tools, the pinned original archive and four
pinned LWJGL/JInput/JUtils metadata JARs. It clears Java tool-option injection
variables, verifies that compilation produced only the observer and guard,
and checks class origin. Network, native linking, writes and deletes are
denied and counted. `--add-opens=java.base/java.util=ALL-UNNAMED` permits a
read of the actual private Random seed for observation. All source, tool,
archive and helper hashes must match after execution. The shared bounded
capture helper retains stdout/stderr, exit status, timeout, reader errors,
output limits and hashes. `finite.json` binds its observation-source inputs;
changes require a new original capture.

Six negative-control tests reject independent row removal/reordering,
changed exception/effect/type fields, numeric/boolean coercion, one-bit
velocity/Random/position/bounds changes, duplicate keys, changed capture
inputs and corrupted original method pins. The local run with the original
archive passes all six without skips and rechecks the unchanged baseline.
Without the archive, the original-identity test reports one explicit skip.

## Run

Use a new private output directory for an original capture:

```powershell
$jdk = 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin'
$libraries = "$env:APPDATA/.minecraft/libraries"
python -B tests/reference/entity/wolf_attack/capture_reference.py `
  --client-jar b1.7.3.jar --library-root $libraries `
  --java "$jdk/java.exe" --javac "$jdk/javac.exe" `
  --output .local/wolf-attack-original-repeat
python -B tests/reference/entity/wolf_attack/test_packet.py --client-jar b1.7.3.jar
```

Review the original completion and unchanged-input receipts before replacing
the packet's `finite.json` with a new capture. Native replay uses the focused
test's reference mode:

```powershell
cmake --build build-windows --target mc_test_wolf_attack
python -B tests/reference/entity/wolf_attack/run_native.py `
  --executable build-windows/mc_test_wolf_attack.exe `
  --work-dir build-windows/wolf-attack-reference
```

These paths use a single-configuration Windows build. For Visual Studio, add
`--config Debug` and select its `Debug/` executable. Native replay needs no
original archive or Java. It creates the existing native fixture's declared
statistics resources and separate World directory for class setup; its attack
actors are independently constructed with NULL Worlds and never rebound.
Each replay retains its native NDJSON, stdout/stderr, comparison result and
before/after source/resource/product hashes in a new artifact directory.
