# Storage and original references

## Singleton state-shape checkpoint

All 15 Block overrides and the base state-shape body are implemented. MovingPiston
preserves captured owner identities, late getters and partial singleton stores.
The expanded geometry contract passes 386 constructed native checks per compiler;
original-runtime state-shape equivalence remains unverified.

Fresh builds pass all 169 MSVC and all 171 tests on each GCC/Clang build, with zero skips.
The
[testing notes](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
record exact-copy Linux validation and retained mounted-source timeouts.

At source `eab4533`, MSVC, GCC and Clang tick replays pass complete protocol
qualification for 114 caller inputs and 228 snapshots per side on each compiler.
Against the same qualified original family, all inputs and binary action
classifications match, as do all 5,016 comparisons per compiler of eleven
primitives: carried ID, metadata, facing, extending/render flags, raw
current/previous progress bits and the actual Tile's inherited x/y/z/invalid.

| Host | Full native protocol | Finite Piston projection |
| --- | --- | --- |
| MSVC | Qualified | 5,016 primitive comparisons match |
| GCC | Qualified | 5,016 primitive comparisons match |
| Clang | Qualified | 5,016 primitive comparisons match |

The earlier malformed MSVC streams and intact-record diagnostic remain retained
unchanged. Caller Context coordinates, World references or aliases, World/Entity/
Block graphs, complete error correspondence, constructor history and the original
state-shape body remain outside this comparison. It establishes no whole-engine
parity.

The following tick/bounds checkpoint predates state-shape integration.

## Piston tick and standalone bounds checkpoint

The [Piston contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles/piston.md#tick-and-entity-pushing)
implements actual Tile tick/private push ordering, real supported Entity movement
and shared owning-List history. The
[bounds contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/bounds.md)
adds genuine standalone raw-coordinate allocation and native lifetime handling.

Fresh focused MSVC and sanitized GCC/Clang builds pass nine cases each: bounds
4,751, tick 575, finalization 460, Tile 296, geometry 144, mutation 218, collision
732, movement 544 and World dependencies 202. Independent qualification checks
all 940 public/798 native/254 dependency inputs, exact working-source differences,
fresh compile/product/graph/command joins, full streams and recorded closures.
The audited configurations register 167/169/169 cases; only nine run per host.
No current full-suite, hosted or complete original/C tick result is inferred.

The original tick family independently admits 114 ordered inputs and 228
snapshots per side, including five genuine fresh seed23 Worlds and 567 actual
workspace quiet-air setters per World. Each side retains 24 genuine action
throws; setup/snapshot/observer/close failures are zero. Source/tool/JAR/compiled
helper seals, full gzip hashes/CRC/EOF, input/control records and admitted passive
graphs qualify. Complete native replay comparison remains pending. Natural
constructor entropy, non-atomic snapshots, unadmitted statics and the helper/JVM
trust limits described in the
[source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/piston.md#active-tick-input-family)
remain explicit.

| Retained evidence | Bytes | SHA256 |
| --- | ---: | --- |
| Working-source capsule | 558606 | `dbecd83baffa22952e5a68b80b21969fc6b55e6592b11522a06d5834f212a2b6` |
| Native focused audit | 97475 | `17a18dd765be099c2b66d4b0966c7ad554566fa542586e9fe2df86a7dd2ce866` |
| Original tick qualification | 4076833 | `599ccf1d9e5fabd26313523c94fe40ffc6826b23297b81717527df656b472822` |

These are retained working-source observations. Commit and hosted-check bindings
remain separate; documentation changes do not extend their runtime scope.

The following finalization/neighbor checkpoint predates tick and standalone-factory integration.

## Piston finalization and neighbor observations

The [paired Piston source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/piston.md)
includes explicit finalization, MovingPiston/container removal and the actual
MovingPiston neighbor body. The neighbor's World B guard precedes its real
acquiring Tile getter. Discarding the return preserves acquisition, the added
hook, lazy invalid-Map cleanup and reached typed failures. Mappings
provide navigation names; separate client/server JAR descriptors and observed
behavior remain the authority. That checkpoint predates the tick/push addition
above; the separate state-shape update and wider gameplay remain unfinished.

Seven fresh local contracts pass on MSVC and sanitized GCC/Clang, with 460
finalization, 296 Piston Tile, 144 geometry, 218 mutation, 732 collision, 544
movement and 202 World dependency checks per host. The 3,579-byte focused build
receipt checks reports, actual child closure, current binary pins and input-map
equality. The separate full native qualification performs 4,809 before/after
file-pin joins over the recorded capsule's 936 public files, including 794
native inputs, plus 254 dependency files. Their bytes, build tools/products/
graphs, complete streams, process receipts, unchanged probe, resources and all
57 caller inputs are hashed and joined. Neither establishes a full local or
hosted suite for the callback. The earlier green `85441fe` 168/170 hosted checkpoint
remains historical to its own source. Current full local suites remain unrun.

| Retained report | Bytes | SHA256 | Scope |
| --- | ---: | --- | --- |
| Focused build qualification | 3579 | `1a84e726504240a369693db2bf1e2e114b4f16a9b6408537eeb2508815b722a7` | Report/closure/current binary and map-equality joins |
| Native qualification | 47097624 | `a1b82230abb7f35c9e16d8bd3f047eddff940fbb4f9d6d94a163a32182e1489c` | Complete enumerated capture and actual file/process audits |
| Finite state comparison | 18084016 | `39e0ee4786dff14fa373f7394e42f430a1bf61c5121be2d02343e20b67243bb4` | Source-mapped finite projections and action-prefix joins |
| Comparison findings | 9415 | `63935947e6ec718f68b5b31fcd68c21ee72263f58ada70bfa8b40baaa17eeec1` | Dynamic metrics, source rules and material omissions |

The original family preserves all 57 ordered active inputs and declared
projections per side. Fresh native captures execute them once on each of six
side/compiler processes, with natural-zero exits, reaping, full lossless streams
and cleanup. Independent comparison reports
`finite_projection_matches_enumerated_observations`: all 342 rows, 684 snapshots,
2,100 full cached-array images and 1,050 complete changed-offset/value delta lists
match. No input/control, completion-prefix, finite-projection, delta, action
Random/scalar-preservation or scope-gap mismatch was found.

Each side/compiler joins 42 normal void completions, three true Boolean results,
nine actual null-World failure prefixes, two full signed-ID -1 prefixes and one
null-Block255 prefix. The prior `same_id_changed_data`, `hook_piston_next_down`
and `installed_negative_inf` completion gaps and invalid Chest Map discrepancy
are absent on all six runs. The old Chest key `(11,96,5)` is removed after
`installed_negative_inf` and remains absent in later snapshots. Native full
status/error records and Java full Throwable records remain separate. Failure
joins use the pinned complete source bodies and retain completed effects; they
do not equate every native descendant with a Java Throwable model.
The signed-ID prefix joins retain byte/metadata writes before the full-ID
lookup. A before/after dirty=true image does not prove that later PC299 ran.

Persistent bidirectional owner joins cover current/other Tiles, logical Tile
Map entries, ordered c/G occurrences, light owners/types/bounds/order, cached
Chunks and Nibble/array owners, World/Dimension/coordinator and Random owners.
Cached arrays compare complete bytes and every changed offset/value. Reused
contexts retain prior identities, coordinates and progress; source-unused
constructor columns are not reapplied.

The comparison is limited to the enumerated source-mapped projection:

- Registry Block, Material, Sound and subclass state is omitted except for all
  four Block Randoms. Tile registration Maps compare counts, without their
  entries or class-loader graph.
- World SavedData, Store, save-handler, paths, sessions, JDK graphs and `World.P`
  active-set contents are omitted. WorldInfo String alias identity and
  unprojected Player contents are not established by names or null values.
  Generic nonempty World Lists have no coverage in this family.
- Generation noise, biome and work arrays and the fallback BlankChunk are
  omitted. Actual cached Chunk arrays compare complete bytes and all deltas.
- Native holds, phases, allocations and Store counters are outside Java field
  equality. Map key-wrapper/bucket identities and native ChunkState allocations
  are not equated with Java Chunk/key objects.
- Native PFN1 omits Chest slots-array and insertion-List owner identities;
  physical slot contents and ordered insertion contents are joined. The Chest
  slots observed here are empty. Java AtomicLong seed-owner identity has no
  asserted native counterpart; String alias identity remains unproved.
- World `g`, `I`, `Q` and default World/Block Random initial values remain constructor
  entropy. Every action must preserve its own actual owners and state. Generator
  and cave Random values compare exactly; native `terrain_constructor_random` is
  an extra native-only role, without a fabricated Java owner.
- Early native World/Chunk static images are not equated where original images
  are absent. Constructor and generation history and whole-graph replay parity
  are not qualified by these finite action comparisons.

Earlier interpolation and geometry replays retain only their stated helper
scopes. Tick, pushing, the separate state-shape update and wider gameplay remain
unfinished; finite projection agreement closes no broader acceptance gate.

The [movement source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-movement.md) records complete paired Item/Falling base bodies
and separately qualified original observations. All 59,654 client records and
exactly 24,109 intact server records qualify; the server subset has interior gaps
and corruption. Disk exhaustion left the original final/server receipts empty,
with the server exit code unknown. Historical JAR/tool/class before-pin objects
did not survive; independently checked current after pins remain distinct.
The failed receipts and raw streams are retained, and no original process was
restarted. Native replay now passes all 59,654 client and 24,109 intact server
rows on MSVC and sanitized GCC/Clang, including exact failure prefixes and full
pool tails. Server execution includes 11,639 state-only rows to preserve history,
without claiming their missing original outputs. New captures are losslessly
retained as gzip with raw/encoded hashes and CRC checks. Probe objects relink
unchanged qualified core archives; the later Python-only correction has a
separate unchanged-binary binding. Complete original server/nonnull-World
movement and gameplay acceptance remain open.

The following Piston checkpoint predates Entity movement integration.

The [Piston source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/piston.md)
records paired Tile, collision and independent Direction bodies. Retained
original captures contain 19,140 float/constructor/NBT records and 11,026 direct
geometry records per side. Native replay selects explicit helper projections;
it does not compare original nonnull-World metadata or Tile traversal. The
geometry producer's retained final pin-bookkeeping failure and separate
qualification remain documented alongside the completed subject streams.

The following collision checkpoint predates Piston integration.

The [collision source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/collision-boxes.md)
records complete World/Entity virtual ordering and the selected 61 paired Block
bodies. Original capture runs the full actual registry and 2,200 scenarios per
side. Native replay qualifies 623 source-unused-World getter records, keeping
other original results separate. Metadata/Tile traversal and complete gameplay
still need original comparisons.

The following nearby checkpoint predates collision integration.

The [nearby source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/nearby-entities.md)
records complete paired World query, normal/Blank gather, floor, presence,
acquiring getter and strict intersection bodies. Public reflection tools use
untouched original JAR classes, with real null-World constructors and explicit
slice/bounds research inputs. Native replay uses real GAME/store-owned objects.
Original/native constructor or World traversal equality is not claimed by those
Chunk projections. Raw rows, bootstrap streams and native failure prefixes
remain distinct retained evidence; malformed collection states stay explicit.

The following bounds checkpoint predates nearby integration.

The [normal population source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/normal-population.md)
records the complete paired controller, shared-Random mixing, late Biome/static
reads, tree constructor/height order and current-w snow operations. Native
all-air cache scenarios inspect all eight final saved Chunk slots. These
prepared contracts provide no original whole-controller continuation or natural
terrain qualification.

The [prepared Dungeon placement reference](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/dungeon-placement.md)
retains 48 genuine complete original calls on independently copied saved Worlds.
All 144 native comparisons pass across MSVC and sanitized GCC/Clang. Actual
cache acquisition precedes each notifying placement; exact selected Chunk bytes,
Tile/Stack values, collection order, queued lighting and Random continuation
match. These direct prepared calls do not establish natural population or
original save/unload/reload. The separate native persistence tests keep their
own finite coverage.

The [ordinary GAME Item persistence reference](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-game-storage.md)
records complete paired load/save bytecode order, actual factory/Chunk/Boolean
joins, current Entity/Stack provenance and retained failure prefixes. Its
constructed native disk tests pass on MSVC and sanitized GCC/Clang. Separate
untouched-original shared-schema and direct-region calls run ten cases per side,
twice; exact known Item tag lists match all native hosts. Unknown native records
and provenance metadata follow an explicit preservation policy, while original
fresh writers discard them. Other constructors, natural dedicated startup,
broader original/native routes and full population remain unfinished.

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
