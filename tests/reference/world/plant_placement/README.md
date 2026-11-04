# Original WB plant placement predicates

Two fresh ordinary OpenJDK 17 VMs executed 864 unchanged original predicate
calls for eight registered Blocks: Sapling 6, Tall Grass 31, Dead Bush 32,
flowers 37/38, mushrooms 39/40, and Crops 59. These are eight receivers and
six runtime classes. Both complete raw stdout streams are byte-identical.
All methods, receivers, World, Chunks, and materials come from the pinned
Beta 1.7.3 JAR; no original method or class is replaced or recompiled.

The 288 ordered profiles in `profiles.tsv` contain name, ID, x, y, z, face,
target ID, below ID, and NULL-World flag. The matrix has 64 substrate profiles,
96 target-replaceability profiles, 64 signed-y profiles, 56 face profiles,
and eight NULL-World profiles. Each runs these independent channels in order:

1. Actual registered Block `a(Lfd;III)Z`, resolving to WB.
2. Actual inherited face wrapper `a(Lfd;IIII)Z`, resolving to UU then WB.
3. Actual World `a(IIIIZI)Z`, with `ignore=false` and the declared face.

No equality between different channels is assumed. The World wrapper has its
own collision/replacement prefix. All 24 reached NULL-World exceptions remain
literal `UNRETURNED`/`java.lang.NullPointerException` outcomes. Signed y=128 is
not forced false: the original can observe air at the out-of-height target and
a real substrate at y=127. These predicates do not establish setter legality.

All eight inherit the UU face wrapper and WB base. WB first invokes the UU
target-replaceability body; only success reaches the wrapped Java `y - 1`
read and virtual substrate `d(I)Z`. Sapling/Tall Grass/flowers inherit WB's
live Grass/Dirt/Farmland comparisons. Dead Bush resolves to JB's live Sand
comparison; Crops to NI's live Farmland comparison; mushrooms to TL's actual
opaque boolean-array lookup. Survival `g(World,...)`, light, growth, planting
mutation, Items, Players, and statistics are outside this replay.

The helper uses the existing Torch real-World setup: a genuine saved fd World
and four canonical Chunks, with the seed-23 authored Region/level inputs from
the existing picking fixture. It creates no App, Player, or Timer worker.
One World and the same four Chunks are shared across the entire ordered
cohort. Before **each channel**, the declared direct array setup resets all
four Chunks: Blocks/Data/BlockLight/HeightMap zero, SkyLight byte `0x0f`,
`i=0`, and `n=true`. It writes below at Java `y-1`, then target at y, only
when that signed y is in 0..127. It makes no World setter call or coordinate
clamp. Lighting/scheduled queues are not reset or drained; residual provider/
World state is shared cohort history, not independent fresh-World cases.

Every call records the count of changed bytes across all five arrays of all
four Chunks; all 864 observed counts are zero. Supporting raw guards compare
shallow World/Chunk primitive bits and reference slots before/after each call,
plus the actual World Random seed, Gaussian flag, and raw Gaussian value;
all remain unchanged. These are passive snapshots, not a deep World-graph or
cross-VM random-entropy oracle. `--add-opens java.base/java.util=ALL-UNNAMED`
permits those read-only JDK Random-field observations. The helper never draws
from or reseeds Random and changes no Source clock, time, or input state.

The exact selected protocol uses tabs:

```text
CLASS id runtimeClass faceOwner baseOwner substrateOwner
PLANT index name channel id x y z face target below result exceptionClass changedArrayBytes
COMPLETE 8 288 864
```

There are eight CLASS rows first, then 864 profile-major/channel-major PLANT
rows, then completion: all 873 selected lines. `original.stdout` is the
declared ASCII prefix selection with LF endings. Each `ordinary-*.stdout`
preserves exact captured CRLF bytes, including genuine bootstrap and the
supporting guard. `-text` and `cr-at-eol` preserve raw bytes while retaining
trailing-space/tab checks. All authored capture inputs were LF before freeze;
the declared JSON input also has explicit `-text` protection.

`source-pins.json` records complete selected class/method identities for the
plant, material, and World/setup children and binds the existing public static
placement authority. `capture-provenance.json` uses portable artifact roles
for exact tool/JAR/SDK/helper/input pins. Both authored saved Worlds were frozen
before either execution; helper classes were frozen before both VMs. Ordinary
source session-lock file changes are retained separately. All other saved
input bytes remain unchanged. The guard records three allowed private-World
write checks, with zero outside writes/network/native-load/process attempts.

Compilation and both captures succeeded on the first authored attempt. The
only run stderr is the JDK SecurityManager deprecation warning; exact private
raw hashes preserve it without publishing its private helper path. Complete
private argv/freeze, classes, World inputs/outputs, stdout/stderr, and outcomes
remain retained. No original Code bytes, classes, JAR, or assets are published.

Audit and finite parser/integrity controls:

```text
python tests/reference/world/plant_placement/packet.py
python -m unittest discover -s tests/reference/world/plant_placement -p test_packet.py
```

`audit()` returns the finite inventory/effect summary. `reference_lines()`
audits then returns every literal selected line. To repeat with private inputs:

```text
python tests/reference/world/plant_placement/capture_reference.py --jar <original.jar> --libraries <libraries> --jdk <jdk-home> --output <new-private-directory>
```

This packet alone establishes no native equality, unrestricted placement,
mutable-static alias behavior, failed acquisition ownership, whole-World/save
correspondence, gameplay, pixels/audio, or FPS result.
