# Populated Chunk save and direct readback

The `save-readback` profile follows the genuine seed-23
[origin population schedule](natural-population.md) with two forced cache saves
and four direct region loads. The original and native implementations each
construct, populate, save and read their own World. Expected arrays and NBT come
only from the original observations.

Both original sides use the persistent base-World constructor. This profile
covers a held World and its populated Chunks. Full World metadata saving,
unloading and restarting remain separate work.

## Actual calls and boundaries

[`PopulationPersistenceObservation.java`](../../tests/reference/world/population/PopulationPersistenceObservation.java)
composes the unchanged `NaturalPopulationObservation`,
`CacheCallbacksObservation` and `ChunkObservation` helpers. It calls the existing
origin observer once, then keeps the actual World, Info, cache, generator, region
loader and collections alive. Read-only reflection checks their bindings and
retained identities. No original game field is assigned.

| Operation | Client original | Base-server original | Native call |
| --- | --- | --- | --- |
| Set World time to 1234 | `fd.a(J)V` | `dj.a(J)V` | `mc_world_info_time_set` on the actual held Info |
| Force cache save, twice | `ok.a(ZLyb;)Z` | `jb.a(ZLpj;)Z` | `mc_world_runtime_save_chunks_detailed` |
| Load `(0,0)`, `(1,0)`, `(0,1)`, `(1,1)` | `ld.a(Lfd;II)Llm;` | `hb.a(Ldj;II)Lhi;` | `mc_world_runtime_load_game`, also used by GAME acquisition |

The unchanged population prefix has five boundaries. Persistence adds fifteen:
its initial state, before/after the time setter, before/after each save and
before/after each load. Every boundary includes complete cache and Chunk state,
all fifteen Info fields, complete numerical state and a recursive inventory of
every file and directory. Files retain all raw bytes; original files also carry
their SHA-256 digests.

The native probe uses the real Registry `m` table to derive its post-load block
map before opening the Store. Slot 0 maps to air; each nonnull registered slot
from 1 through 255 retains its byte ID. The original map is first reflected after
the first genuine loader return, so observation does not initialize its class
early. Both actual maps contain bytes 0 through 96 followed by 159 zero bytes.

## Compared effects

Setting time changes the held Info and World time to 1234. It preserves all
Chunk arrays, numerical state and disk bytes. The first save stamps the four
normal Chunks with 1234 and clears dirty and Entity-presence flags. It preserves
their identities, all five arrays, minimum heights, cache membership and ordered
light regions. Both forced saves return true. The second pass preserves the
complete compared state and every file byte.

The initial region file already has an 8,192-byte header. After the first save it
contains 24,576 bytes and four occupied slots: 0, 1, 32 and 33. Live Info
`SizeOnDisk` becomes 24,576, including the region owner's pending header-growth
credit; the physical file grows by 16,384. Cache-only saving leaves Info
version 0 and creates no `level.dat`.

Each record contains one root Compound and a `Level` Compound with all eleven
expected tags. The comparison checks exact tag types, coordinates, last-update
time, population flag and every byte of Blocks, Data, SkyLight, BlockLight and
HeightMap against the actual saved Chunk. Both empty Entity/Tile lists have
on-disk element type 1. Original List writing chooses 1 for an empty list; the
native NBT codec already implements that rule.

Direct loads return four fresh owners outside the cache. Each belongs to the
same actual World and requested coordinates. Its persisted population flag and
five complete arrays match the saved record after the actual block-map
normalization. Active, dirty, never-save and Entity-presence flags are false;
last-save time and minimum height are zero. All direct reads preserve the
published cache, held Info, numerical state and disk bytes.

Complete provider/manager arrays, aliases, provider/cave Random state and copied
continuations remain compared across the population/persistence join. Each run
keeps its independently clock-seeded `World.r` unchanged. No original Random is
replaced or reseeded.

Session tokens must match each run's measured construction interval and actual
`session.lock` bytes. Occupied region timestamps must fall within that run's
measured first-save interval. These clock inputs are checked separately across
runs. Complete typed NBT and sector allocation are compared after decompression;
Compound entry order and compressed payload bytes are not cross-language
equality claims. Within each run, the second save and every load must preserve
all raw files exactly, including headers, timestamps, payloads and padding.

## Public replay and reproduction

The public
[`persistence_fixtures.json`](../../tests/reference/world/population/persistence_fixtures.json)
retains lossless original observations for both sides. Only the archive URLs in
the two class-origin maps become logical `supplied:client` or `supplied:server`
labels. Loading checks all four Java source fingerprints, decompresses bounded
records, revalidates the original schedule and rederives every stored expected
projection. Native output supplies no expected value. The capture attribution
records its dirty development source honestly and claims no exact tested commit.

`world.population_save_readback_replay` constructs fresh native Worlds with
the separately declared project-authored minimal resources. It needs no JAR or
Java process. `reference.population_save_readback` instead reruns both actual
JARs and extracts their exact required resources. Both use the same complete
comparison. `tools.population_persistence` exercises corruption controls.

```sh
ctest --test-dir build --output-on-failure -R 'population_(save_readback|persistence)'
```

The optional fresh test uses `MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`, both
`MC_REFERENCE_CLIENT_JAR` and `MC_REFERENCE_SERVER_JAR`, and explicit Java/Javac
paths. Set `MC_REFERENCE_TESTED_COMMIT` only for a matching clean source commit.
Use `-C Debug` with a multi-configuration build. Captures retain complete JSON,
disk bytes, process streams, class/tool/source fingerprints and reports.

Inventories admit at most 256 entries, 4 MiB per file, 8 MiB per boundary and 8 MiB
of cumulative retained file bytes. Each complete observation is bounded to 64 MiB;
the public package is bounded to 16 MiB. The native inventory admits ASCII game
filenames and strict UTF-8 roots, including Windows UTF-16 conversion. Captures
require exclusive access to their fresh owned directory. These observation
bounds do not impose operating-system filesystem quotas.

This is one seed, four cached normal Chunks, two cache passes and four matching
direct records. Nonempty Entity/Tile data, malformed/relocated records, failure
paths, eviction, dedicated-server behavior, saved metadata construction and
gameplay restart retain their separate acceptance requirements. Terminal
retirement closes observation resources; it is not a source unload operation.
