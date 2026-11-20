# Original Block harvest observations

This packet records unchanged client Leaves (`bk`), Snow-layer (`jr`) and Ice
(`nk`) harvest bodies. It contains 92 prepared invocations: 63 complete
singleplayer `os.b(IIII)Z` calls and 29 direct virtual harvest calls. The public
native comparison uses the 63 controller profiles, producing 208 selected
rows. The 29 direct profiles are supplemental original observations; they do
not establish a separate native direct-body replay.

The authority is the supplied client JAR with SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`source-pins.json` records complete declaration inventories, class hashes and
method Code hashes for 33 reached client classes. It also verifies that `nr`
and `jp` declare no harvest override, so their superclass calls resolve to
`uu`. No original method bytes, extracted assets or game binaries are included.

## Preparation and observations

`BlockHarvestObservation.java` constructs real original App, Stats, World,
Chunk, Player and Stack owners. Each profile prepares the actual Chunk arrays,
metadata, held cell, remote flag and below Block before the observed call. The
target is `(3,64,5)` in Chunk `(0,0)`. A below Snow layer has Stone at `y=62`
so its support is valid. These are arranged method invocations, rather than an
ordinary full client startup or a saved-world session.

Immediately before the call, the helper applies the declared seeds to the
actual World Random and JDK Math Random owner. It observes their real 48-bit
states before and after. Item positions, motion, yaw, HoverStart, payload,
pickup delay and age remain exact raw IEEE-754 or integer values. Every
published Item must belong to both the actual World List and a real Chunk
Entity List. The new Entity's private default-Random constructor entropy and
identity are outside this projection.

The listener implements the original `pm` interface. It records actual Item
insertion and target-cell notifications with the current Block, statistics,
Random states and membership aliases. Its other methods return normally; no
original game class or method is replaced. Callback reads are passive queries
of retained owners. The listener projection does not compare every sound,
particle or range notification.

The profiles distinguish fresh held-cell reads after Shears wear, last-Item
clearing, a remaining Item after a break, count zero and negative counts,
metadata type/high bits, sapling/no-drop branches, Snow hand/Spade behavior,
remote handling and Ice support by air, solids and liquids. Flowing Water is
ID 8. Ice above Lava also reaches the real fluid interaction and preserves its
additional World/Math Random consumption.

Prepared failure controls use an actual NULL statistic cell, NULL App writer,
or NULL Player argument. Retained owners observe prior publication, air writes
and statistics after the real exception. Snow's direct trace records Item
publication, then air, then the statistic; Ice's Water notification sees the
already completed mined statistic. Native admission, allocation and retained
callback-failure controls have their separate native-test scope.

## Reproduction and transport

The qualified capture uses Temurin 17.0.17+10, `-Xint`, `-Xverify:all`, headless
AWT and a 512 MB heap. Two fresh VMs agree on all selected rows and their whole
stdout hashes. All 14 supplied archives, JDK/runtime files and capture helpers
remain unchanged. Both runs make zero source network or native-library
attempts. Actual connect, listen and native-link operations are then denied
as separate guard controls. The real Timer worker is interrupted and joined.

```powershell
python -B tests/reference/block/harvest/capture_reference.py --jar b1.7.3.jar --libraries "$env:APPDATA/.minecraft/libraries" --output .local/block-harvest-capture
python -B -m unittest discover -s tests/reference/block/harvest -p test_packet.py
ctest --test-dir build-windows -R '^(client\.block_harvest|tools\.block_harvest_packet)$' --output-on-failure --no-tests=error
```

Each capture requires a new output directory and preserves failures there.
`--profile-limit` is a private diagnosis prefix and never produces a complete
qualified packet. No Java or original files are needed to validate or replay
the published packet.

`profiles.tsv` declares inputs. `observations.tsv` groups each `PROFILE` with
one `RESULT`, its `ITEM` rows, then `EVENT` rows in callback order.
`schema.json` defines every column and sentinel. `guards.tsv` records the five
source/denial checks. `provenance.json` binds their exact bytes. All packet
files deliberately use LF, matching the repository attributes; do not convert
their line endings without recapturing or explicitly requalifying the bytes.
The validator checks hashes, class/method identity, shapes, ranges, nullable
roles, Random preparation, publication and the selected source-order rules.
Its 32 tests include 31 rejection controls.

These results do not qualify cold class-initializer failures, unrestricted
native storage, another JDK or original-era VM, all listener arguments,
original pixels/audio, persistence, extended play, server behavior or full
game parity.
