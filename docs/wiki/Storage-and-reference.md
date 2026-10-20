# Storage and original references

The [Item Entity NBT observations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-nbt.md)
retain twenty prepared calls per side into the untouched archives. Actual
MSVC/GCC/Clang probes match 2,422 comparison rows per side, including exact
selected field bits, partial reads, owner relationships and semantic saved
tags. Complete original stdout is preserved in a bounded public fixture;
private archives, logs, commands and environment values remain excluded. This
does not establish serialized byte order, private Random state or ordinary
GAME Entity disk round trips.

The [live Tile and staged GAME path](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles.md)
imports supported Tile records into the same Store-owned Chunk before block
normalization. Failed imports retain their source prefix under that owner and
disable further direct source use. Pending and failed imports require joint
container retirement before individual disposal. Successfully committed chunks
retain ordinary save, unload and retirement behavior. The
[live GAME save coordinator](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md)
writes current supported Tiles and explicit empty Entities over that same
owner, preserving reached q effects and complete typed failures. Constructed
native coordinator tests save, unload, retire and reload on a continuing World.
Natural dedicated startup, GAME Entity load/save integration, whole World
metadata and original/native Tile replay remain unfinished.

The [original World query observations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/world-queries.md)
retain 44 calls, 112 snapshots and 170 raw Context records per side. Complete
array images, typed fields and namespace bridges preserve actual World/Chunk
identities. A cold unpublished Chunk prelude and two same-class Worlds expose
Light initialization and the shared positive-sky flag. Prepared input writes
are explicit. The separate native query/plant contracts preserve six region
baselines and check two exact empty provider-capacity regions. They do not
replay the originals or qualify genuine dedicated construction,
full population or gameplay persistence.
A separate offline audit checks the retained typed fields, complete images,
identity bridges, ordered preparations and raw Contexts. Both positive side
checks and twenty semantic corruption refusals pass with unchanged inputs.

The [original Entity publication observations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-publication.md)
retain 21 actual calls per side, full Chunk array bytes, ordered duplicate lists,
typed payloads and actual cross-World aliases. The separate [native owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/entities.md)
retains each occurrence and refuses nonempty saves. Terminal disposal prevents
later serialization even after lists are cleared. Original/native replay,
Entity save interoperability, player/dedicated descendants and complete
population acceptance remain unfinished.

The [concrete World lighting source](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lighting.md#concrete-world-source)
records paired height, enqueue, Light and region-constructor order. Native
client-World tests use actual store-owned restored chunks, real allocation
limits and explicitly controlled shared depth. They supply no new original
World observation. Complete live generation, propagation and population remain
unfinished; see the [runtime contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/runtime-lighting.md).

The [Biome capture](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/biome-registry.md)
records natural ordinary initialization on both originals in three JVM modes,
all Biome/list/record/Class aliases, six snapshots and 3,824 raw-bit queries per
side. Complete public protocols exclude original assets. Native comparison
distinguishes its producer policy and projects exception kinds, without
claiming Java Throwable identity or full World startup.

The [Dimension capture](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/dimension.md)
records actual original constructors, retained arrays, standalone light tables
and 48,739 celestial calls per side. Its public projection compares exact bits
and identities without requiring Java or original archives. The separate
[native World contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/construction.md)
documents normal constructor completion and the remaining spawn, registry,
population, subclass and persistence dependencies.

Storage provides raw NBT, gzip/zlib memory compression, metadata codecs,
region inspection and writing, stable stored chunks, native region transport
and world-store ownership through `mc_persistence`. The finite storage foundation
passes exact-source native MSVC/GCC/Clang acceptance and independent audits at
`9ad690d83592f127b9bb39d7233e8eef06fd0029`; hosted checks pass as well.
See the [storage checkpoint](#storage-foundation-checkpoint) below for its scope.

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
loading and natural population. The storage foundation below adds native
persistence operations; full persistence acceptance and light propagation remain open.

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
byte-equal controls. Public replay imports observed cache/world effects while C
computes coordinator transitions. At exact source
`28a55b52c001c4c114a11ca7df844fc1f286d328`, all 56 selected tests pass without
skips on MSVC/GCC/Clang, with Linux sanitizers and independent audit. Hosted
checks pass 48 tests per compiler and formatting for 89 C/header files.
No running-server or terrain-feature parity follows.

[Read-only region evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/region.md)
at exact 092301ae passes all 49 queries per original side, with byte-equal
controls and independent source/artifact audit, on MSVC/GCC/Clang. Eight cases
are compatible and 41 retain explicit diagnostic departures. Hosted checks pass
42 tests per compiler and formatting for 87 C/header files. That checkpoint
establishes finite local read inspection; later schema, global mapping and write
work has separate evidence below.

## Storage foundation checkpoint

The [storage evidence overview](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md)
records the implemented foundation and its acceptance limits. The
[metadata codec](https://github.com/frankischilling/minecraft-c/blob/main/docs/storage/level.md)
interprets exact known types/defaults and builds independent NBT with an explicit
save clock. The
[region image writer](https://github.com/frankischilling/minecraft-c/blob/main/docs/storage/region-writer.md)
owns allocation, retained bytes and growth in memory. The
[world storage owner](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/storage.md)
adds stable chunk lifetimes, native files and cache ownership, metadata fallback,
session checks and rotation, dimension paths, accounting and the real
acquisition load adapter.

Original metadata and chunk observations support the public codec/schema
replays. Unknown root/metadata fields, opaque Player data and entity/tile
compounds have a separate C preservation policy. Original serializers can drop
such fields or reconstruct objects through factories. Opaque retention and
edit refusal do not establish player/entity simulation, subtype persistence or
complete gameplay save behavior. Fixed array/type restrictions also remain
explicit C malformed-input departures.

The region-write fixture retains 34 memory-image write attempts across four
profiles. Its allocation/growth and complete images are separate from native
filesystem replacement. Only independently bounded original timestamp fields
become explicit C clock inputs. Separate original global-cache observations
retain 341 actions per run and nine filename/open mappings. Those nine global
mappings are original-derived; C local-slot mask cases and the remaining native
transport scenarios are constructed contracts. These checks do not establish
original GC timing, general concurrent mutation or failed-I/O durability.

At exact source `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71 selected
tests pass without skips on native MSVC and WSL GCC/Clang, with Linux ASan/UBSan.
Times are 744.00, 1255.43 and 1256.05 seconds. Hosted checks pass 59
public/numerical tests per compiler and formatting for 111 C/header files.
Independent platform audits bind all 290 tracked files and every original
manifest to the source checkpoint, with tools, build inputs and executable
bytes preserved. Separate source/provenance and semantic audits pass for all
new profiles. Copied-world tests cover original-created and C-created
containers in both directions, explicit roots 0/-1, signed storage boundaries
and opaque Boat/Sign transport. All 74 world-harness guards pass.

The [exact storage checkpoint](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md#exact-source-storage-checkpoint)
records twelve new capture hashes, audit hashes, full-state/control/clock
checks, unchanged original-only fixtures and excluded earlier attempts.
These are finite seed23/API contexts. Entity/player and every subtype body,
gameplay save/unload scheduling, dimension-transition sharing, running-server
persistence, crash recovery and full game integration remain open. Earlier
checkpoint counts above describe only their recorded sources.

## Lifecycle qualification

The [lifecycle evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lifecycle.md)
adds finite seed23 client, base-server and dedicated-owner constructor/API
schedules for incremental/forced and metadata-first saving, actual far-drop,
100/101 unloads, detach, reload and session failure. Dedicated construction does
not start the server tick loop. Original APIs supply expected values; C never
does. Full arrays/headers, metadata, pending/insertion history, supported typed
files, untouched bytes, clocks and measured input/class/process provenance are
separate required projections.

At source `46546d8322de52beb6c3a51f30e827c0260d2b30`, complete Windows v10,
GCC v9 and Clang v9 suites each passed 92 tests without failures or skips.
Independent source/semantic audits, storage rechecks and complete capture
preservation also pass, with Linux ASan/UBSan and stable measured inputs. The
[canonical ledger](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lifecycle.md#exact-source-lifecycle-checkpoint)
retains exact result/manifest hashes, the three audit-v3 report sets and
excluded incomplete attempts. Each platform preserves all 22 original-profile
roots; no successful Windows v9 row is reused.
The profile-specific 600/660-second dedicated101 allowance changes no captured
state, graph scope or proof requirement.

The current-source Windows v9 attempt is excluded in full: 91 passed, 1
skipped and none failed. Original chunk-data setup returned unavailable before
measurement when its queue contained 42964 entries against the 16384 bound.
The fresh Windows v10 suite uses unchanged source and bounds; every test, capture
and audit passes independently. Its rows are never supplemented from Windows
v9. See the
[excluded Windows v9 history](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lifecycle.md#excluded-windows-v9-attempt).

The preceding v7 source `e5dc551a901a84fcec1568fac9e0d62fc5a6ee14` remains
excluded: Windows passed 92 tests, GCC passed 91 and failed 1, and Clang passed 90,
failed 1 and skipped 1. Both Linux dedicated101 captures failed trace identity
validation before C comparison; Clang's chunk-data skip occurred during unseeded
original-constructor setup. The Windows result remains historical to that source.

The v8 source `96bd35b4a0138383478a936465a9c69be99a2547` is also excluded:
Windows passed 92 tests, while GCC and Clang each passed 91 and failed 1, with
zero skips. Linux dedicated101 Java execution completed, then helper epoch
validation failed before C comparison. Retained full semantic and native
`--development` comparisons pass as diagnostics only; the failed manifests
remain unchanged. Exact times and scope are in the
[v8 history](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lifecycle.md#excluded-v8-attempt).

The new controller retains bounded mirrors from each first serialized ID. The
old-controller Linux marker control proved128 live objects received new IDs after
debugger disposal. The corrected standalone regression passed both marker and
pre-label cases on Windows JDK17 and Linux JDK21, with 128 disposed controls,
stable anchors and capacity/duplicate-label checks. These tool checks do not
qualify those earlier failed cohorts. The accepted Windows v10/Linux v9
selection uses fresh complete suites and independent audits. The current helper interval
policy permits 600000ms only for observed dedicated101 operations at both
validator call sites. Preparation, constructors, controls and other profiles
retain 180000ms, and events remain bracketed by their own interval. The actual
call-site regression went red before this fix and passed 556 positive/2011
negative checks on native Windows and Linux afterward.

Keep four evidence domains separate: static original mappings, JDK-only
collection histories, constructed C reentry/error/ownership contracts, and
genuine original game observations. Nonempty Boat/Sign replay observes external
feature bodies; native runtime activation refuses those records and must
preserve copied file bytes. Native empty-runtime serialization is implemented;
entity/player bodies, general deferred cleanup, terrain population, propagation,
world ticking, gameplay dimension transitions, running-server persistence and
crash recovery remain required. Explicit C retirement does not reproduce Java
GC lifetime, and pinned modern-JDK order makes no Java6 claim.

Original binaries, assets, raw decompiled source, launcher files, credentials,
recordings and personal worlds remain outside public source and releases. Copy
worlds before any writable reference experiment. Project code uses MIT; zlib
retains its own license, and neither license covers original Minecraft material.
