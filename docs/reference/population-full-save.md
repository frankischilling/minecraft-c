# Held World full-save comparison

The `full-world-save` profile constructs each implementation's own client or
base-server World, runs the unchanged seed23 origin population schedule, sets
World time to 1234 and calls its actual full save twice. It passes on the MSVC
development build against both supplied original JARs and through public replay.
Exact committed-source and other compiler qualification are pending.

The actual Player ArrayList and SavedData named cache, ordered list and ID map
are distinct, empty constructor-owned objects throughout this profile. The
observer retains and checks those owners; it does not derive their emptiness
from Info.Player or the ID count. Native SavedData uses a copy-owned storage
binding, reported separately from the World Store role. No fabricated Player
list, auxiliary callback, prepared Chunk or imported population effect supplies
the native result.

The original helper composes the four existing observer sources without
changing them. It selects and checks the actual list-taking World/handler/Info
methods and SavedData fields from the pinned JAR. Its only gameplay operations
after population are the original public time setter and two
`World.a(true, null)` calls. It never calls the chunk-only persistence observer's
observation routine before those full saves. Reflection writes only the
project-owned measurement flags; it does not edit game fields or Random seeds.

The comparison retains five origin and seven full-save boundaries per side.
Each full-save boundary contains the complete cache, held Info, provider and
shared state, actual collection graph and a complete ordinary disk inventory.
Across all twelve numerical boundaries it checks all arrays, lengths, aliases,
three Random states and the eight copied nextLong values derived from each
captured 48-bit seed. World.r remains its separately measured per-run input.

It checks all thirteen typed metadata tags and all eleven saved Chunk Level
tags, including the five complete byte arrays. Both metadata LastPlayed values
must fall inside their own measured save intervals. Gzip lengths and bytes are
verified independently for each implementation; decoded metadata comparisons
exclude only that separately checked clock value. The first metadata file
writes SizeOnDisk zero before Chunk growth reaches live Info at 24576. The
second writes 24576. Live version becomes 19132, while live last_played remains
zero. The second level.dat_old must equal the first level.dat byte for byte;
the entire region and session files must remain unchanged by the second save.
Actual final directories are reread and checked against their final captures.

`world_save_fixtures.json` contains lossless original-only observations and
rederived expected projections. Only the two archive-URL maps are replaced by
side-specific logical archive identities. The package pins all five Java source
files and rejects a stored expected projection that does not derive from its
original record. Its initial capture honestly records a dirty development
checkout and no tested commit. Native outputs are not inputs to its expected
values. The package is 3,146,302 bytes with SHA256
`803f6d14a7152ef74c9ff8bde3dc18f9b960b1612db1db373fd0f835eac50d17`.

Public replay executes a fresh native probe using the same project-authored
minimal translation files as the older replays. It requires no Java process or
JAR and imports no game state. Those resources retain their separate attribution
from fresh comparisons using exact JAR resources. Eleven corruption tests cover
collection identities and contents, complete boundaries, live Info changes,
metadata tags and clock bounds, complete gzip streams, exact rotation, all five
saved arrays, Random state and fixture source/projection pins.

Run the public checks with:

```text
ctest --test-dir build -C Debug --output-on-failure -R "^(tools.population_world_save|world.population_full_save_replay)$"
```

The optional `reference.population_full_save` test uses
`MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`, the two existing JAR path options and
Java 17. The capture driver records source/tool fingerprints, process commands,
complete output streams, file inventories and the tested commit only when it
matches clean source. Each capture uses a new owned directory and preserves
the supplied JARs.

This is finite successful held-World evidence. It does not establish nonempty
Player or named SavedData behavior, source I/O-failure semantics, unload,
restart, dedicated cache behavior or full population parity. The
[full-save contract](../world/full-save.md) records those unfinished paths;
the older [Chunk save/readback profile](population-persistence.md) retains its
independent direct-load evidence.
