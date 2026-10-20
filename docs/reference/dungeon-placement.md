# Prepared Dungeon placement observations

[`DungeonPlacementObservation.java`](../../tests/reference/world/DungeonPlacementObservation.java)
invokes the untouched original complete Dungeon placement body on a copied
saved World. [`dungeon_placement_probe.c`](../../tests/world/runtime/population/dungeon_placement_probe.c)
invokes the production C entry. Neither helper predicts an expected floor,
Chest location, loot stack or Spawner name.

This experiment is separate from the [private-helper comparisons](population-dungeon.md).
It covers a direct placement call on a declared room. Natural terrain, the
population schedule, dedicated startup, ticks, arbitrary observers and
save/unload/reload remain outside this experiment.

## Inputs and acquisition

Each case uses a fresh process and independently copied owned World root. The
original root must contain a complete genuine saved baseline, `world/level.dat`
and the exact `.chunk-storage-owned` marker `fresh chunk storage reference\n`.
The existing `CacheCallbacksObservation` prepare mode creates a genuine saved
baseline; copy it before each Dungeon call. The observer rejects linked paths,
oversized roots and existing outputs. Preserve input/tool/archive hashes,
complete process streams, stage files, class-load logs and thread receipts.

Original setup constructs actual World, provider, cache and storage owners in
a prepared class context, then calls the real World time setter to 1234. The
native probe applies the ordinary World Info time setter before acquisition.
Setup active uses and unrelated metadata differ between these fixtures; these
calls do not establish cold-context or complete-World equality.

Input chunk `(0,0)` is full actual named stone, with zero metadata/light arrays,
unsigned height bytes of 128, empty Entity/Tile lists and the populated flag set.
The original NBT writer and region stream install those declared input bytes.
Actual World/provider/cache acquisition decodes, normalizes and activates the
Chunk. The C fixture uses its real encoded region/acquisition route. Neither
observer inserts a decoded Chunk into a fabricated cache.

The 24 cases combine all four radius pairs `(2,2)`, `(2,3)`, `(3,2)`, `(3,3)` with
six variants: one opening, five openings, zero openings, six openings, floor
rejection and roof rejection. Center is `(8,64,8)`. Openings are two cells high;
rejection removes the first validation-corner floor or roof cell. All room
coordinates stay inside the selected Chunk.

An independent ordinary JDK Random selects the first nonnegative seed below
1,000,000 for each pair after one `nextGaussian()` and two `nextInt(2)` draws.
This selects only input radii. The actual supplied Random is separately created
with that seed and genuinely Gaussian-primed before placement. An optional
original seed override must reproduce the requested radius prefix. Native calls
receive the exact captured seed without another expected-output algorithm.

## Interfaces

Compile these Java sources together with JDK17: `ChunkObservation.java`,
`ChunkStorageObservation.java` and `DungeonPlacementObservation.java`. Keep the
game JAR off the compiler classpath. Add `CacheCallbacksObservation.java` when
also compiling its separate baseline-preparation entry point.

```text
java -Xint -Xmx512m -cp CLASSES DungeonPlacementObservation JAR client|server CASE COPIED_ROOT OUTPUT_JSON [SEED_HEX16]
mc_dungeon_placement_probe dungeon-placement client|server CASE SEED_HEX16 ROOT_PATH_UTF8_HEX
python scripts/reference/world/compare_dungeon.py --original ORIGINAL_JSON --native NATIVE_JSON --output NEW_COMPARISON_JSON
```

Native Registry setup emits its ordinary four status lines before the complete
JSON record on stdout. Retain the full raw stream; pass that final unchanged
JSON record to the comparison command. Diagnostics go to stderr. Retain both
streams and exit status. A normal false result differs from native
failure. Output is published after checked genuine teardown. Use a new root and
output for every attempt. The comparison command reads retained observations
and refuses existing output.

## Exact comparison boundary

Select the ordinary original Chunk by actual cache Map identity. Coordinates
alone cannot select it: the fallback also has `(0,0)`. Compare all block,
metadata, sky-light, block-light and height bytes, minimum height, live flags
and last-save time. Each side retains its own same Chunk, World, nibble wrappers
and array owners across placement. Identity tokens are local to each process.

Compare actual Chunk Tile Map iteration order separately from ordered World
Tile lists. Join keys, positions and actual same-owner list references. Every
fresh Chest has 36 physical slots despite logical size 27: compare all 36, nulls,
four Stack fields and the observed Stack alias graph. Compare Spawner nullable
exact UTF16 name, delay and both rotation bit patterns. Original String value
capture does not expose String identity.

Compare the full ordered pending light queue, kind/default and six bounds per
entry without sorting or draining. Compare enqueue/drain depth, the normal
Chunk static flag, empty scheduling and all eight empty Entity slices. Original
scheduling class state/counters and arbitrary World reference graphs remain
outside this shared observation. The original fallback header graph and Tile
namespace HashMap iteration also have no corresponding captured native object
layout; they remain explicit exclusions, not fabricated equivalent owners.

The same supplied Random consumes, in order, `nextGaussian()`, `nextInt(11)`,
`nextInt()`, `nextLong()` and `nextDouble()`. Compare raw floating and long bit
patterns as well as integers. These draws consume the continuation; no
nondestructive state-copy claim applies.

Original startup/prepared/acquired/before/call/after receipts stay separate. A
minimal call receipt preserves an actual outcome if later image serialization
fails. Full comparison requires complete after-image, continuation and passing
close/thread guards. Capture limits refuse rather than truncate. Native
request budgets, JVM allocation failures, process-tree closure, fallback state
and output persistence are not equated by this check.

## Executed results

All 48 original observations complete on the selected Windows JDK17, with
24 independent processes and copied Worlds per side. Each side returns true
for eight rooms and false for sixteen validation controls. No actual placement
throw occurred in this corpus. Original archives, observer sources, compiled
classes, baselines, complete streams and all stage/thread/class-load receipts
are retained with before/after hashes and bounded file inventories.

MSVC and sanitized Linux GCC/Clang each run the same 48 captured input cases.
All 144 comparisons pass with zero differences in the exact projection above,
including original validation returns and full after-images. Linux runs use
ASan/UBSan/leak checks and fresh native filesystem roots. The configured native
suites also pass all 153 Windows and 155 tests per Linux compiler with zero
skips; each retains 723 selected inputs, 254 zlib pins, 191 products and
36 existing integration captures. These results establish this finite prepared
placement comparison, while the population and whole-game limits above remain.
