# Storage and original references

Storage provides raw NBT, gzip/zlib memory compression and read-only local
region-record inspection. Global chunk-to-file mapping, world schemas, writable
world compatibility and copied-world round trips remain unimplemented.

The raw NBT codec handles the original tag IDs 0..10 with named roots, lists,
compounds, byte arrays and lossless UTF-16 string units. Decoded documents own
retained data, including application fields the caller does not interpret.
Duplicate compound names retain the last value; resource limits bound memory,
bytes, tags, elements and nesting. See the
[NBT interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/storage/nbt.md).

Compression accepts an explicit gzip/zlib format and borrowed input bytes,
returning an owned output buffer. It validates complete stream checksums and
trailers. Gzip concatenates adjacent recognized members, while zlib stops after
one stream. Nonmagic compressed tails remain unconsumed. Limits bound input,
output, live allocation and member count. See the
[compression interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/storage/compression.md).

The original gzip NBT wrapper can finish its root before reading a missing or
corrupt trailer. The C module rejects malformed containers through full member
validation; reserved flags and recognized invalid later headers are also
documented departures from the selected modern Java readers. Valid container
payloads compare exactly. Compressed blocks need not match across compressors.

Reference comparisons invoke untouched classes from separately supplied,
hash-verified client/server JARs in isolated JVM processes. They record source,
binary, runtime and input fingerprints plus raw records in ignored local
reports. The C runtime has no JVM dependency. Missing prerequisites remain
unavailable results. Rewritten components agreeing with each other alone do not
establish original compatibility.

[Float lookup/floor evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/math.md)
records the complete initialized table and raw-bit scalar calls. These pure
numerical observations need no world or cave setup. They establish the tested
math prerequisite separately from cave geometry and complete chunks.

Reproduction and exact bytecode anchors live in
[NBT evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/nbt.md)
and [compression evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/compression.md).
The [testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md)
and [development status](https://github.com/frankischilling/minecraft-c/blob/main/docs/status.md)
record current validation scope and outstanding work.

[Normal density evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/density.md)
also documents genuine original world construction for numerical observations.
Each run owns fresh ignored directories; normal server construction can create
a session lock and directories. Tests never open an existing personal world.
These density/base comparisons do not establish copied-world save compatibility
or complete generation.

[Surface evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/surface.md)
separates immutable constructor state from live Random and current material
attributes. Genuine server constructor spawn may generate complete chunks
before a fresh measured generator is created; the later observed pass calls
base/surface directly. These observations do not establish complete chunks,
caves, finishing, population, lighting or save compatibility.

[Normal cave evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/caves.md)
separates ten genuine natural contexts from 44 protected room/path controls.
The natural pass starts after actual base/surface generation and retains all
generator, climate, material and Random state. Focused controls construct no
worlds and observe full buffers, interior/shell water, lava at stored Y10,
grass repair and ineligible bytes. Branching is inferred by unique Random
continuation matching, with the inference recorded explicitly. Cave comparisons
do not establish chunk finishing, population, lighting or save compatibility.

[Chunk evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/chunk.md)
covers fresh voxel arrays, packed nibble access, column heights and direct sky.
Original full finishing calls run in separately owned genuine worlds. Complete
local arrays compare independently between C/client, C/server and client/server;
world cache membership and ordered queued-light bounds remain separate records.
The local C component preserves the original Y127 height exclusion, Y0 sky
exclusion and retained sky values across repeated passes. It does not implement
world-height queries, lighting propagation or chunk lifecycle.

All five fresh/repeated local chunk cases and the complete packed-access
observations match on MSVC/GCC/Clang in all three pairings. Public fixtures also
match the complete original bytes in a separate expansion audit. The chunk
evidence page records exact source, manifest fingerprints, reproduction inputs
and observation limits. Queue and cache diagnostics remain evidence for the
next finishing stage.

[Light-region evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lighting.md)
adds actual untouched merge and world-queue calls. It retains each real setup
baseline and compares complete ordered queues with C after observed admission.
Forty-eight cases contain 72 merge requests; each side also runs 43 world
requests and a separate actual million-entry overflow experiment. The C module
implements storage after admission. Full finishing has separate evidence below;
propagation remains open.

[Full finishing evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/finishing.md)
adds actual JDI entry/exit traces of all 1024 height queries per pass, dirty state
and complete ordered queues. Each fixed case runs twice in the same genuine
world and matches a separate untouched control across complete snapshots.
Suspended marker inventories record thread roles and original-loader frames;
they establish finite headless conditions rather than general scheduling.
The C cache supports lookup, explicit map/list publication and a synchronous
acquisition coordinator. Separate callback observations below cover successful
loading and natural population; full persistence and light propagation remain open.

## Normal generation checkpoint

[Direct normal generation evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/generation.md)
records original stage handoffs and genuine setup qualification for seeds 23,
24, 26 and 51. The C composition is implemented through full finishing with
fresh unpublished chunks, a borrowed world biome manager, separate biome
output preserving manager `d`, and all 13 current material pairs supplied
explicitly. Its public contract unit passes 99073 checks on MSVC/GCC/Clang,
including sanitizer checks. Four-seed original capture and lossless public C
replay pass eight inputs, 72 complete chunks and 73728 exact height queries.
At exact source `a08c12056a06261ed01d9f66999459f7ce9543b4`, all 41 selected
local tests pass on native MSVC and WSL GCC/Clang without skips, with ASan/UBSan
on Linux. All three pairings pass independently. Each build retains 1224 stage
events, 144 complete marker inventories and eight byte-equal untouched controls.
Independent audit verifies 47 exact-commit project sources, 57 stable attributed
inputs, eight unchanged helper classes and all record/control/world-file hashes.
The evidence page records all three manifest hashes and the audit fingerprint.

The `reference.normal_generation` comparison observes actual direct
generator calls in fresh worlds and requires complete byte-equal untouched
controls. It retains complete local/world state, current materials and opacity,
climate/scratch, all 82 layers, separate Random continuations and 1024 ordered
height queries per call. Repeated, negative, packed-key-alias, world-border and
finite cave-domain endpoint calls exercise one retained generator. Public
`tools.normal_reference` and `tools.normal_interop` checks provide synthetic
guards and lossless retained-fixture replay; see
[build commands](Build-and-tests#normal-generation-checks). Generation remains
separate from cache acquisition/publication, population, propagation, other
dimensions and save compatibility.

The measured contexts use base worlds `fd`/`dj` and caches `ok`/`jb`. The actual
`MinecraftServer` constructs `dp` with `he`; its cache lifecycle is not covered
by the base-world generation observation. The suspended inventories
establish finite headless conditions, not general concurrency behavior. Hosted
[a08 checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass 36 public/numerical tests per compiler and formatting for 79 C/header files.

## Cache and region checks

[Cache acquisition evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache.md)
records actual persistent client and dedicated-owner constructors with retained
providers. Three original-only contexts retain 20 operations, five generations,
5120 ordered queries, 252 lifecycle events, 40 inventories and three complete
byte-equal controls. At exact source `9a50ac67997082c89dbc43cf4274fba1427c1565`,
all 52 selected tests pass without skips on MSVC and WSL GCC/Clang with Linux
sanitizers and independent audit. Hosted checks pass 45 tests per compiler and
formatting for 88 C/header files.

[Callback boundary evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache-callbacks.md)
adds four original contexts for successful loading and naturally triggered
population: 12 calls, 394 lifecycle events, 24 inventories and four complete
byte-equal controls. Public replay imports the complete observed external body
effects while C computes coordinator transitions. Final exact-source platform
acceptance is pending. No running-server or terrain-feature parity follows.

[Read-only region evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/region.md)
at exact 092301ae passes all 49 queries per original side, with byte-equal
controls and independent source/artifact audit, on MSVC/GCC/Clang. Eight cases
are compatible and 41 retain explicit diagnostic departures. Hosted checks pass
42 tests per compiler and formatting for 87 C/header files. This establishes
finite local read inspection, not a world schema, global addressing or writes.

Original binaries, assets, raw decompiled source, launcher files, credentials,
recordings and personal worlds remain outside public source and releases. Copy
worlds before any writable reference experiment. Project code uses MIT; zlib
retains its own license, and neither license covers original Minecraft material.
