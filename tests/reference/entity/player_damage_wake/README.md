# Sleeping Player damage

This packet records 32 calls to unchanged client `gs.a(sn,int):boolean` on a
genuine `dc` Player. Two fresh JDK 17 interpreter VMs each complete 25 normal
returns and seven first `NullPointerException`s. The selected raw-bit state
agrees exactly between the VMs. No original class is replaced or subclassed.

Preparation uses actual App, statistics writer, World, normal Chunk and Player
constructors. The Player is published through the real World insertion body
and enters sleep through the actual `gs.b(III):cw`. The observer then applies
the declared component inputs in `profiles.tsv`; these are not elapsed ticks.
All damage sources are null. Healthy nonzero profiles use damage four and
remain nonfatal. The World time, normal nighttime admission and Chunk planes
are fixture inputs; the packet does not qualify natural sleep progression.

Each transport row contains:

```text
case direction health amount mode
```

| Mode | Prepared input after actual sleep |
| --- | --- |
| 0 | Valid occupied Bed |
| 1 | Remote World |
| 2 | Null Player.World |
| 3 | Null bounds |
| 4 | Null bed coordinate |
| 5 | Both Bed blocks replaced with air |
| 6 | Null Player.App |
| 7 | Awake Player with null World |
| 8 | Missing Bed and null bounds |
| 9 | Null bed coordinate and null bounds |
| 10 | Remote World and null bounds |
| 11 | Null Player.Inventory |
| 12 | Null bed coordinate and null World |
| 13 | Awake Player |

The first eight profiles cover all four Bed directions at damage zero and four.
Other profiles cover zero/negative health, the bypasses above and failures
after a completed wake. Every row retains position, motion, standing/sleep
dimensions, timer, coordinates and identity relations, current/original bounds,
health and hurt fields, World sleep/list state, complete central Chunk-plane
hashes, all three reached Random states, statistics and sound arguments.

`source-pins.json` pins 29 complete original classes and their 843 method
declarations, including Code fingerprints where present. This inventory does
not mean all those methods execute. Actual class bindings are checked against
the supplied archive in each VM. The source archive SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`provenance.json` records executed observer/reader/support inputs, helper and
binary hashes, bounded process receipts and unchanged-input checks. Raw
stdout, stderr, classes and isolated statistics files remain private.

Capture requires the supplied JAR, JDK 17 and the locally imported original
SDK libraries:

```powershell
python tests/reference/entity/player_damage_wake/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/player-damage-wake-new-attempt
```

The output must be a new child of `.local`. The observer permits statistics
writes only inside its isolated work directory. Both accepted VMs complete
with zero attempted network access, native-library loading, process execution
or rejected outside-directory writes. No graphical window is created.

The Java-free native replay uses the real damage API and reconstructs each
complete Chunk plane from bounded, nonzero byte runs before hashing it:

```powershell
python -B tests/reference/entity/player_damage_wake/run_native.py `
  --executable build-windows/mc_test_player_damage_wake_original.exe `
  --work-dir build-windows/player-damage-wake-original
python -B tests/reference/entity/player_damage_wake/test_packet.py
python -B -O tests/reference/entity/player_damage_wake/test_packet.py
```

The shared comparison has 4,748 exact scalar positions. The original packet
also retains its Java row schema and actual Java exception-stack names; the
native effect API has no Java stack, so these 46 positions are explicitly
unpaired. First-null categories and selected reached state are compared.
The native fixture attaches a genuinely constructed App as a prepared input
before observation. This comparison does not establish equal constructor
sequences or complete App/World object graphs.

Windows MSVC Release replay passes all 32 profiles and 4,748 shared scalar
comparisons, with 537,253 native fixture checks. The process exits zero with
empty stderr, complete bounded capture and unchanged replay inputs. The first
MSVC compilation stopped at the test fixture's `fopen` CRT diagnostic; the
fixture now declares the same local CRT policy as the existing hurt observer.
No diagnostic setting or original expected output is weakened. Linux native
execution remains pending at this checkpoint.

The reader/control test rejects 307 authored corruptions in both ordinary and
optimized Python. These are admission and comparison
controls, not additional original-game calls. Neither these controls nor this
packet qualifies hostile attackers, Arrow relationships, fatal damage,
natural night completion, ticking, rendered pixels, audio-device output or FPS.
