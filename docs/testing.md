# Test scope and reproduction

CTest separates public unit/tool tests from explicitly requested reference
comparisons. Public tests do not require Minecraft files, Java, a
display, assets, or an account. Missing private references are unavailable results.
The numerical Java oracle is a standard-library probe and can run in public CI
without original game files. Original-class math, storage, noise, climate and terrain
comparisons remain local.

| Check | Coverage | Limits |
| --- | --- | --- |
| `world.normal_generator` | 99074 public checks for composed generation, fresh lifecycle flags, borrowed ownership, unpublished identities, mutable materials, repeated results, coordinate/resource preflight and allocation-failure preservation | Finite ordinary normal generator; no cache acquisition, population or propagation |
| `world.chunk_acquire` | 838 constructed checks for load/generate/fallback, dedicated gates, pending membership, publication, nine activation callbacks, four live population gates, deliberate recursion and partial failures | Callback coordinator; injected effects are separate from genuine gameplay reachability |
| `tools.acquire_reference` | 23 original-protocol and execution guards covering complete events, object ownership, intermediate cache contents, query order, Random states and unavailable/failure classification | Java-free fixed three-context protocol corpus |
| `tools.acquire_fixture` | Five lossless corpus, provenance, compression, MCA1 import and original-only export guards | Retained original expectations; no original execution |
| `tools.acquire_interop` | Three continuous C inputs, 20 operations, five generated chunks and 5120 exact queries; malformed binary rejection | Actual retained-provider composition with fresh-world load misses; no population body or disk decoder |
| `reference.chunk_acquire` (local opt-in) | At 9a50ac6, genuine persistent client and dedicated-owner constructors, retained providers, loader misses, 20 operations, 5120 query pairs, 252 lifecycle events, 40 inventories and three complete untouched controls pass on MSVC/GCC/Clang | Finite seed23 constructor/API corpus; successful-load and natural population callback comparisons remain separate |
| `tools.callbacks_reference` | 24 Java-free execution and original-protocol guards, including setup-only unavailable proofs, full events/state/owners, loaded-time and population boundaries | Retained four-context original evidence; no original execution |
| `tools.callbacks_fixture` | Ten bounded fixture, provenance, preparation, complete MCC1/expected projection and strict JSON guards | Original-only expectations; fingerprint-only public method proofs |
| `tools.callbacks_interop` | Four genuine continuous inputs, 12 operations and 16 external callbacks, plus four constructed cases and 28 malformed/state/owner guards | C computes coordinator transitions; observed external effects within the cache/world boundary are explicitly replayed |
| `reference.chunk_callbacks` (local opt-in) | Genuine successful loads and natural population in persistent base worlds; 394 lifecycle events, 24 inventories, preserved saved baselines and four complete untouched controls | At 28a55b5, all three local toolchains and independent acceptance audit pass; no C decoder or terrain-feature body implementation |
| `storage.region`, `tools.region_inspect`, `tools.region_capture`, `tools.region_live`, `tools.region_probe` | Read-only storage/CLI, historical replay guards, 17 live-protocol guards and real Unicode stdin paths | Local record inspection; no world schema, global addressing or writes |
| `reference.region` (local opt-in) | All 49 queries per side with fresh copies, exact controls, complete metadata and bounded diagnostic differences; exact 092301ae MSVC/GCC/Clang checks and independent audit pass | Eight compatible cases and 41 explicit malformed/diagnostic departures; no original error-parity claim |
| `tools.normal_reference` | Production helper schema, source/provenance, prerequisite and preservation guards | Synthetic checks; no original methods run |
| `tools.normal_interop` | Lossless retained original baselines and complete sequential C generation replay | Fixed four-seed corpus; eight continuous nine-call inputs pass |
| `reference.normal_generation` (local opt-in) | All four seeds and nine calls per side match in three pairings on MSVC/GCC/Clang at a08c120; complete local/world state, 73728 ordered query pairs, 1224 stage events, 144 inventories and eight untouched controls per build | Fresh finite headless base-world contexts; unpublished normal entry within the finite cave-scheduler domain; no dedicated-server cache lifecycle or population |
| `world.chunk_finish` | 3259 constructed checks for packed cache aliases, publication visibility, unsigned heights, original admission, query order, dirty effects and rejected-call preservation | Valid nonnull cache state; no loader, generator or population lifecycle |
| `tools.finish_reference` | Corpus, packed-key, wrapped-query, boundary and fixture omission guards | Synthetic helper checks; no original methods run |
| `tools.finish_interop` | Twelve captured original inputs, 24576 exact query records and 48 complete data/queue stages | Six fixed fresh-world cases with two passes; no propagation or complete cache lifecycle |
| `reference.chunk_finish` (local opt-in) | Actual original entry/exit traces, complete retained local/cache/queue snapshots and untouched controls, independently compared in three pairings | Finite headless marker inventories; normal finishing on the fixed corpus only |
| `world.light_queue` | 72 observed merges, complete live queues across 43 requests per original side, storage/alias/failure contracts and the million-entry abort | C queue storage after admission; no C world admission or propagation |
| `tools.light_reference` | Five parser, ordering, rejected-request preservation and old-JDK prerequisite guards | No original methods run |
| `reference.light_queue` (local opt-in) | Actual original merge methods and world queue overloads, complete ordered queues, unchanged world/cache state and actual million-entry abort | Replays each observed baseline/admission result; full world finishing remains open |
| `core.java` | 146 fixed arithmetic/cast/Random edge and captured-oracle checks | No game simulation or game random-consumption ordering |
| `core.math` | 903 checks: all observed lookup/floor cases, complete-table fingerprint, 37 exact entries, repeated initialization and immutable reads | Float table and cast/compare floor only; sampled scalar inputs, measured native backends |
| `core.mutf8` | 596848 checks, including every UTF-16 unit, accepted byte forms, malformed input and unchanged outputs | Payload codec only; NBT and packet framing are distinct |
| `storage.nbt` | 394978 checks for all original tag types, boundaries, retained state, ownership, budgets, errors and deterministic mutations | Raw tags only; no world schema or compression |
| `storage.compression` | 2351 checks for gzip/zlib round trips, headers/trailers, ownership, byte/allocation/member limits and mutations, including failed large-output growth | Memory containers only; no region/world files |
| `storage.level` | 1055 checks for exact metadata types/defaults, UTF-16 ownership, explicit clocks, opaque fields and bounded failure preservation | Metadata codec; filesystem behavior is separate |
| `storage.region_writer` | 433 checks for owned image allocation/reuse, exact-boundary sector counts, retained bytes, growth and failure atomicity | Memory images only; no native file replacement or clock source |
| `world.stored_chunk` | 67681 checks for stable state/array ownership, fixed schema shape, transient defaults, normalization and opaque edit guards | No entity/tile factories, registration or subtype simulation |
| `world.noise` | 4850 checks: seeded permutation/state, independently traced original scalar/bulk/octave observations, zero sizes/layers and unchanged invalid-call outputs | Perlin/octave primitives only; no complete terrain or simplex biome noise |
| `world.simplex` | 5005 checks: observed constants/state/grids, mixed constructor ordering, original numerical boundaries, independent/default octave factors and unchanged rejected-call state/output | 2D simplex primitives only; no biome climate/classification or complete terrain |
| `world.biome` | 33155 checks: all 4096 observed table cells, 212 float and 79 double cases, climate vectors, buffer growth/suffixes, staged allocation rollback and unchanged rejected-call state | Climate/classification module; no world startup, terrain or complete biome behavior |
| `world.terrain` | 535745 checks: three full original-captured density/base fixtures, constructor state, retained suffixes, interpolation ordering and unchanged rejected-call output | Density and base blocks only; complete terrain requires later passes |
| `world.surface` | 1114102 checks: seven complete paired-original fixtures, direct/reseed contexts, independent Random schedules, sea/material/bedrock/sandstone controls and failure preservation | Normal surface pass only; constructed controls are separate from original fields |
| `world.caves` | 2021631 checks: ten complete paired-original fixtures and 30 captured passes, reseeding, owner isolation, suffix/table preservation and C argument/coordinate controls | Normal cave pass; protected geometry controls have separate evidence |
| `world.caves_geometry` | 2917405 checks: 44 paired-original protected cases with full buffers and Random continuations, raw bits, draw-count inference, suffixes, canaries and independent owners | Private valid room/path domain only; no public scheduler coverage |
| `world.chunk_data` | 101534 checks for fresh/repeated initialization, unchanged rejected outputs, capacity suffixes, packed-coordinate bounds and extreme-opacity preservation | C ownership/argument contracts; original local-state comparisons are separate |
| `world.chunk_nibbles` | 414 checks against 16 observed setters, complete retained packed bytes and two capacity variants | Packed-array access and preservation; no chunk lifecycle |
| `world.chunk_reference_data` | 335 checks across five paired-original fixtures, 20 stages, 100 complete arrays and ten rebuilds | Local voxel state only; full finishing world effects are separate |
| `tools.noise_reference` | 4 generated prerequisite/report/path/symlink/hard-link regressions | No original classes run; filesystem link cases skip only if unsupported |
| `tools.simplex_reference` | 5 generated prerequisite/hash/report/path/link regressions | No original classes run; link cases skip only if unsupported |
| `tools.biome_reference` | 5 generated prerequisite/hash/report/path/link regressions | No original classes run; link cases skip only if unsupported |
| `tools.density_reference` | 6 generated prerequisite/hash/report/path/link and mismatch-before-unavailable regressions | No original classes run; link cases skip only if unsupported |
| `tools.surface_reference` | 6 generated prerequisite/hash/report/path/link and failure-precedence regressions | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.math_reference` | 10 generated prerequisite/hash/report/path/alias/link, parser and failure-precedence regressions | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.caves_reference` | 18 generated schema/table/report/path/link and failure-precedence guards, including ordinary-file and hardlink inventories and renamed-script rejection | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.caves_geometry_reference` | 18 schema/pattern/continuation/table/report/path/link and failure-precedence guards, including renamed-script rejection | No JVM/C/original process runs; link cases skip only if unsupported |
| `tools.nibbles_reference` | 18 source/protocol/array/path/prerequisite and failure-precedence guards, including aliased temporary directories | Synthetic protocol checks; original methods are not executed |
| `tools.chunk_data_reference` | 12 schema, array, cache/queue growth, preservation, path, source-attribution, compiler-option and completed-measurement guards | Synthetic protocol checks; original methods are not executed |
| `tools.compression_interop` | 127 exact payload/framing records between C and Python gzip/zlib, including all optional header field combinations | Container interoperability, not original gameplay |
| `tools.level_reference` | 26 guard methods, 80 original-derived codec comparisons and 6 separate C preservation-policy checks | Frozen original observations; fresh handler/native-world acceptance is separate |
| `tools.region_write_reference` / `tools.region_write_interop` | 23 guard methods and four complete original-derived image profiles with 34 write attempts | Raw images and explicit observed clock inputs; no original GC timing or failed-I/O parity |
| `tools.chunk_storage_reference` | 28 guard methods and 268 C records: 196 original-derived comparisons/departures plus 72 preservation/refusal checks | Fixed-shape supported records; opaque retained data is not live entity simulation |
| `tools.regions_reference` / `tools.regions_interop` | Four guard methods and seven native filesystem scenarios, including 16 malformed/bounded protocol controls | Nine global mappings are original-derived; remaining scenarios check constructed C contracts |
| `tools.world_store_interop` | Seven scenario groups using 52 paired-original metadata inputs, 56 paired chunk imports and constructed session/rollover/acquisition controls | Development integration; both-direction copied-world and exact-source full acceptance pending |
| `tools.import_assets` | 16 generated-fixture tests for copy/manifest, missing/hash/path/link/size/overwrite checks | No original assets in fixtures; canonical sound contents unverified |
| `reference.java_semantics` (opt-in) | 135768 exact C versus Java records, including float bit patterns | Current installed JVM primitives; not original-client/server game-state parity |
| `reference.math` (local opt-in) | All 65536 raw table entries and 366 scalar calls in three independent pairings; table preservation controls | Original math methods only; no world setup, cave geometry or full input-space claim |
| `reference.nbt` (local opt-in) | Per original side: 194 reads, 40 original writes, 12 packet controls, 157 original-to-C and 435 original re-reads | Selected JDK; 4 malformed negative-list departures and 20 order-only differences recorded separately |
| `reference.compression` (local opt-in) | 80 containers, 22 C encodes; per side 181 baseline records, 65 original gzip reads, 8 original writes, 35 original-to-C and 38 C-to-reference transfers | 36 explicit malformed-container departure records; no copied-world compatibility claim |
| `reference.noise` (local opt-in) | Per side 783 requests, 140218 exact double values, 399872 permutation entries and 3132 following Random longs; three separate pair comparisons | Scoped procedural noise; no chunks, population or lighting |
| `reference.simplex` (local opt-in) | Per side 812 requests, 204923 exact double values, 348160 permutation entries and 3248 following Random longs; actual short/extended overload and factor/suffix controls | Scoped 2D numerical methods; consumer-like inputs do not establish biome results |
| `reference.biome` (local opt-in) | Actual catalogue/table, direct float and quantized double selectors, processed temperature and biome grids with stateful buffer sequences | Explicit-seed numerical fixture; no executed world constructors or terrain |
| `reference.density` (local opt-in) | 150 exact snapshots per side, 82 original density/scratch recomputation checks; all constructor, climate, density and base state across three pairings | Normal dimension only; no surface, caves, population or complete chunk parity |
| `reference.surface` (local opt-in) | 290 exact snapshots per side, 105 original state/material/density/suffix controls and 20 apply-field controls | Measured base/surface contexts; no complete chunk/cave/finishing compatibility claim |
| `reference.caves` (local opt-in) | Ten complete cases, 30 calls and 50 snapshots per participant; all state, blocks and Random observations in three pairings | Three pairings pass on MSVC/GCC/Clang at the recorded source; no complete chunk claim |
| `reference.caves_geometry` (local opt-in) | 44 separately labeled room/path controls, full input/output buffers, Random continuations, table and suffix controls; shared-draw consumption inferred by unique continuation matching | Three pairings pass on MSVC/GCC/Clang at the recorded source; valid private geometry domain only |
| `reference.nibbles` (local opt-in) | Original constructors, two complete getter sweeps and 16 setter snapshots compared independently in three pairings | Valid local packed-array access; explicit C invalid-argument behavior has separate tests |
| `reference.chunk_data` (local opt-in) | Five fresh/repeated input pairs, complete local arrays/heights, opacity and retained-object controls | Genuine original cache/queue diagnostics are retained separately; no C world lifecycle or propagation claim |
| `reference.level` (local opt-in) | Actual original info codecs and metadata handlers, exact types, fallback, clocks, rotation and session observations | Native world-owner acceptance is separate; current results are development evidence |
| `reference.region_write` (local opt-in) | Actual original region image writes, full images, allocation/growth and independently bounded timestamp fields | Global filename/cache observations do not prove original global read/write local-slot mapping |
| `reference.chunk_storage` (local opt-in) | Actual original raw chunk codecs, region loaders and serializers in genuine worlds, with complete arrays and typed opaque data | C activation of nonempty entity/tile data remains refused |
| Format check | `.clang-format` applied to current project C source and headers | Style only |
| Original JAR inspector (local) | SHA-1/SHA-256, bounded archive/class-header/CRC metadata | Not cryptographic signature trust or JVM verification |
| Original server smoke (local) | Fresh-world loopback offline startup/save/stop | No client joins or C server implementation |

Enable the Java check at configuration with `-DMC_ENABLE_JAVA_REFERENCE_TESTS=ON`.
If requested and no JDK is discoverable, configuration fails clearly. If a
prerequisite disappears before execution, the probe reports `UNAVAILABLE`, exit77,
which CTest marks skipped. Inspect skips separately from passing comparisons.
The standalone helper also returns77 for missing tools or its C executable, and
returns1 for execution errors or mismatches. It only normalizes platform text
newlines; numeric records and floating bit patterns must agree exactly.

NBT comparisons require JDK 11 or newer and both pinned supplied JARs. Enable
`MC_ENABLE_NBT_REFERENCE_TESTS` and set `MC_REFERENCE_CLIENT_JAR`,
`MC_REFERENCE_SERVER_JAR` and `MC_REFERENCE_TESTED_COMMIT` to the exact source
commit used to build the C probe. Missing, mismatched or unsupported prerequisites
produce exit 77 and skipped status, not parity success. The manifest records
an omitted commit as unrecorded. Comparison execution errors return 1. Reports
replace old running/unavailable/fail results; output paths containing inputs or
sources are refused. See [the original-class procedure](reference/nbt.md).

Compression comparisons use the same supplied-JAR paths, JDK11 requirement and
commit field. Enable `MC_ENABLE_COMPRESSION_REFERENCE_TESTS` separately. The
[compression procedure](reference/compression.md) records original wrapper
behavior and distinguishes NBT success from full container validation. Public
cross-reader tests use only generated fixtures and Python's standard library.

Noise comparisons use `MC_ENABLE_NOISE_REFERENCE_TESTS`, both pinned JARs and
the same exact-source commit/JDK settings. Requests exercise actual original
sampler methods through a project-owned reflective probe. The helper compares
finite double bits and state without tolerances; missing references return77.
See [noise evidence](reference/noise.md) for method anchors and remaining scope.

Simplex comparisons separately use `MC_ENABLE_SIMPLEX_REFERENCE_TESTS` with the
same supplied-JAR/JDK/commit settings. They invoke actual original grid overloads,
record initialized constants and compare exact state/results without a finite-bit
tolerance. Mixed Perlin/simplex prefixes test subsequent Random consumption.
See [simplex evidence](reference/simplex.md) and its [interface](world/simplex.md).

Climate/biome comparisons use `MC_ENABLE_BIOME_REFERENCE_TESTS` and the same
supplied-JAR/JDK/commit settings. The fixture invokes actual original protected
manager constructors and original seeded octave objects, then untouched valid
sampling methods. It records exact numerical arrays, object labels and buffer
retention/alias state. Class initialization diagnostics are retained separately
from protocol output. Client-only raw temperature is compared separately from
the shared client/server cases. See [biome evidence](reference/biome.md) and
[ownership and limits](world/biome.md).

Normal density/base comparisons use `MC_ENABLE_DENSITY_REFERENCE_TESTS` with
those same supplied-JAR/JDK/commit settings. The helper constructs real untouched
original world contexts in fresh ignored directories, then fresh normal generators.
It refreshes each original manager's 16?16 processed climate before sampling.
Full constructor state, retained spans, exact double bits and base bytes must
agree between C/client, C/server and client/server. The internal original base
pass density is checked against the separately observed density. See
[density evidence](reference/density.md) and the [terrain interface](world/terrain.md).

Normal surface comparisons use `MC_ENABLE_SURFACE_REFERENCE_TESTS` with the
same private inputs and source settings. They distinguish actual constructor
continuation from mutable live Random, compare prepared chunk-reseed contexts,
and check current material attributes, complete fields/blocks and suffixes.
Genuine server constructor setup can itself generate chunks; measured calls
use a fresh generator afterward. See [surface evidence](reference/surface.md)
and [ownership/operations](world/surface.md).

Normal cave comparisons use `MC_ENABLE_CAVES_REFERENCE_TESTS` with both supplied
JARs, a JDK containing `java`, `javac` and `javap`, and the exact-source commit
field. The natural helper prepares genuine climate/base/surface inputs and
invokes the actual public scheduler three times per case. The C probe emits
its complete table bits; the helper measures their digest before comparing
snapshots. Missing setup is unavailable, while differences, incomplete controls
and measured-stage timeouts fail. See [cave evidence](reference/caves.md) and
the [interface contract](world/caves.md).

The same option enables `reference.caves_geometry`, which creates no worlds and
tests protected room/path calls on separately labeled owned buffer patterns.
These cover eligible and retained block bytes, interior/shell water, lava and
addressed-Y boundaries, grass repair, rooms and inferred branching. Complete
buffers and observed Random continuations compare independently in all three
pairings. Public helper tests reject renamed executing scripts before report
creation so recorded source attribution identifies the helper actually run.

Packed-array and chunk comparisons use `MC_ENABLE_NIBBLES_REFERENCE_TESTS`
and `MC_ENABLE_CHUNK_REFERENCE_TESTS` respectively, with both supplied JARs,
JDK11+ and the exact source commit used for the C probes. The chunk corpus
includes air/stone transitions, asymmetric boundary/material controls and
two frozen original post-cave buffers. It records complete world-height/cache
and queued-light diagnostics without draining lighting. Read the additional
input requirements and commands in [chunk reproduction](reference/chunk.md).

Use `ctest --test-dir build -L public --output-on-failure` for public tests.
Use `ctest --test-dir build -L reference --output-on-failure` for enabled Java
checks. For multi-configuration Windows builds add `-C Debug`.
`BUILD_TESTING=OFF` builds the C libraries and pinned zlib without Python or Java discovery.

`MC_ENABLE_STORAGE_REFERENCE_TESTS` enables the three persistence comparisons
above with JDK17+, both pinned supplied JARs and the exact-source commit field.
The public storage checks replay original-only fixtures and constructed C
policies without Java. Read [storage evidence](reference/storage.md) for raw
producer attribution, full-state comparison boundaries and deliberate malformed
input restrictions. Fresh reference worlds and copied-world experiments stay
under owned ignored directories; no personal world is used as writable input.

Full normal finishing uses `MC_ENABLE_FINISH_REFERENCE_TESTS`, JDK17+ with
`jdk.jdi`, both supplied JARs and the exact source commit field. The controller
observes actual world-height entries and matching return values during two
consecutive passes. Four suspended marker inventories check thread roles and
original-loader frames; these are finite headless observations. Each traced
run must match a separate untouched control byte-for-byte across complete
snapshots. See [finishing evidence](reference/finishing.md) for the corpus,
loopback debugger, captured state and remaining cache/propagation work.

Complete unpublished normal generation uses `MC_ENABLE_GENERATION_REFERENCE_TESTS`
and the same JDK17+/`jdk.jdi`, supplied-JAR and exact-source settings. Its C target
is `mc_normal_generation_probe`; the opt-in test is `reference.normal_generation`.
The verified corpus retains one fresh generator through nine ordered calls for
each seed 23, 24, 26 and 51, including repeated, negative, packed-key-alias,
world-border and accepted cave-domain endpoint coordinates. It compares complete
data, queues, climate/scratch, all 82 terrain layers and separate Random
continuations, plus 1024 ordered height queries per call. Each original traced
run needs a byte-equal untouched control. See [generation evidence](reference/generation.md)
and the [generation contract](world/generation.md) for exact capture hashes,
source attribution and observation bounds. The measured server context is base
world `dj`/cache `jb`; actual `MinecraftServer` construction uses `dp`/`he`.
The current results do not cover a running dedicated server's cache lifecycle.

At source `a08c12056a06261ed01d9f66999459f7ce9543b4`, the suite contains 35
public tests, or 36 with the numerical Java oracle. The selected original
finishing, light queue, chunk, nibble and normal generation comparisons bring
that configuration to 41 tests. These are exact-checkpoint counts; later
cache/region additions on the active branch need their own validation.

## Local environments

Expanded source `28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56
selected tests without skips on native MSVC19.51 and WSL GCC13.3/Clang18.1,
with Linux ASan/UBSan. The suite has 47 public tests, the numerical Java oracle
and eight original comparisons. Independent audit verifies 168 actual passed
rows, complete callback projections/controls, exact callback source objects,
source attribution for all 24 original manifests and the archived logs.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39089309334)
pass 48 tests per compiler and formatting for 89 C/header files. The
[callback evidence](reference/cache-callbacks.md#exact-source-platform-checkpoint)
records manifest/audit hashes, timings and the excluded initial Windows
setup-unavailable attempt before its complete zero-skip retry.

Expanded acquisition source `9a50ac67997082c89dbc43cf4274fba1427c1565` passes
all 52 selected tests without skips on native MSVC19.51 and WSL
GCC13.3/Clang18.1, with Linux ASan/UBSan. Each suite runs 44 public tests, the
numerical Java oracle and seven selected original comparisons. Independent audit
verifies all 53 project sources, 63 stable inputs, nine helper classes, complete
records and all 156 passed test rows across the three builds.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39085300876)
pass 45 tests per compiler and formatting for 88 C/header files. See the
[cache evidence](reference/cache.md#exact-source-platform-checkpoint) for exact
manifest hashes and observation bounds. The separate
[callback expansion](reference/cache-callbacks.md) adds three public tests and
one original comparison, yielding the separately accepted 56-test checkpoint above.

Normal composition source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes
all 41 configured tests without skips on native MSVC19.51 and WSL
GCC13.3/Clang18.1, with ASan/UBSan on both Linux builds. The generator unit
passes 99073 checks; all nine public schema/continuation/stage/preservation
guards and retained-fixture replay pass. Each build compares all four seeds in
C/client, C/server and client/server pairings, retaining eight byte-equal
untouched controls, 72 generations, 73728 queries, 1224 stages and 144 complete
marker inventories. Windows uses Temurin17.0.17 and WSL OpenJDK21.0.12.

Independent audit verifies all 47 project source files against the exact Git
objects, 57 unchanged attributed inputs, eight unchanged helper classes and
every retained record/control/world-file hash per build. Actual 41-test logs,
zero CTest exits and all selected original pass manifests establish the
no-skip result. [Generation evidence](reference/generation.md#exact-source-platform-checkpoint)
records the three manifest hashes and audit fingerprint.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass all 36 public/numerical tests per compiler and formatting for 79 C/header
files. These results apply to a08; later branch changes require separate evidence.

Finishing source `a18615f0d9d90758f0ea95b0218cec97733d2adf` passes all 37
configured tests on native MSVC19.51 and WSL GCC13.3/Clang18.1 without skips.
The Linux builds enable ASan/UBSan. Each suite runs 32 public tests, the numerical
Java oracle and four actual-original comparisons: finishing, queues, chunks and
nibbles. Original finishing uses Temurin17 on Windows and OpenJDK21 on WSL.
All six cases match in three independent pairings; all untouched controls and
marker inventories pass. See [exact finishing results](reference/finishing.md)
for source audits and manifest hashes. Native public CI passes all 33 configured
tests per compiler plus formatting for 75 C/header files on the final branch
source and runtime integration.

Chunk source `67c928db7c0d87e599f16228833b0234fba4bc24` passes all 30 configured
tests on native Windows/MSVC and WSL GCC/Clang without skips: 27 public tests,
the standard Java numerical oracle and two original chunk comparisons. Both
WSL builds enable ASan/UBSan. All six original manifests identify that source,
pass with empty error/unavailable lists and retain matching file fingerprints.
Fresh observations use Temurin17 on both platforms.

All five fresh/repeated chunk cases and complete packed-array observations
match separately between C/client, C/server and client/server. The fixture
audit compares 800 complete arrays to retained original records, including all
three fresh runs. The report audit verifies 660 fingerprints and the tested
source snapshot. See [chunk evidence](reference/chunk.md) for hashes and scope.

Chunk contract, nibble and full-fixture units pass 101534, 414 and 335 checks.
The chunk-data helper has 12 public guards, including renamed-source and
injected-compiler-option regressions. The nibble helper has 18 after adding a
temporary-directory alias regression. Canonicalizing the synthetic fixture
root fixes the Windows short-path mismatch; both successful and unavailable
captures preserve their expected results. All 18 guards pass on native
Python 3.14 and WSL Python 3.12, with Windows short paths also checked directly.
The C and comparison sources are unchanged from the recorded original runs.
Clang-format18 checks 61 C/header files. Public CI runs 28 tests per compiler,
without private JARs or world captures. Integration results belong in
[status](status.md).

Previous normal cave increment:

Normal cave source `a507f59d756acf2d035ac7d4ea2da7171f60ad2f` passes all 33
configured tests on native MSVC and WSL GCC/Clang without skips: 22 public
tests, the standard Java numerical comparison and ten original comparisons.
All 30 original manifests identify that source and pass with empty errors.
Natural ten-case and protected 44-case records match independently between
C/client, C/server and client/server, including full arrays, Random continuation,
measured tables, preserved generator state, classes, inputs and artifacts.
Independent review also verifies the source Git objects and 634 current file
hashes across the six new reports. See [cave evidence](reference/caves.md).

The natural/protected units pass 2021631/2917405 checks and both helper suites
pass 18 guards per toolchain. Isolated native MSVC AddressSanitizer runs also
pass. Clang-format18 checks 50 C/header files; the native testing-disabled build
produces the three libraries without Python or Java discovery. Public CI runs
23 tests per compiler; integration results belong in [status](status.md).

Previous float lookup/floor increment:

Float lookup/floor source `d6665e7f3e1b57c16e9e12c14983abd2ba227441` passes all 27
configured tests on native Windows/MSVC and WSL GCC/Clang without skips:
eighteen public tests, the numerical Java oracle and eight original comparisons.
All 24 original manifests identify this source and pass with empty errors;
every probe hash matches. Fresh math comparisons match all 65536 table entries
and 366 scalar calls independently for C/client, C/server and client/server,
including actual initialized original tables on Temurin17 and OpenJDK21.

Public math tests pass 903 checks and all ten helper guards run without skips.
The alias regression reaches the completed comparison through a directory
alias, covering the fixture-path failure exposed by Windows CI. Runtime code
and fixed numerical data are unchanged; all 27 tests were repeated at this source.
The complete-table fingerprint, 37 selected entries, 132 float pairs and 102
floor cases were independently traced to both original-only captures. Source
Git-blob, executable, class, artifact and input-preservation checks agree.
Clang-format18 checks 41 C/header files; all three libraries build with testing
off and no Python/Java discovery. Public CI runs nineteen tests per compiler
without private files; exact-head and integration results belong in
[status](status.md). These results cover the documented numerical prerequisite.

Previous surface increment:

Normal surface source `f7ac19ebed8a14c499bd233c2dcf3fb0f2502360` passes all 24
configured CTests on native Windows/MSVC and WSL GCC/Clang without skips:
sixteen public tests, the numerical Java oracle and seven original comparisons.
All 21 original manifests identify this source and pass with empty errors;
every current probe hash matches its report. Surface's 290 snapshots match
independently in all three pairings, including complete fields, blocks,
materials, climate, constructor state and live Random continuation. All
equivalence, preservation, suffix and retained-apply controls agree.

Public surface tests pass 1114102 checks and all six helper guards, including
filesystem links and completed-mismatch precedence, run without skips. Seven
fixed fixtures are traced to paired untouched originals; constructed C controls
separately exercise state-machine boundaries. Clang-format18 checks 36 C/header
files. The three libraries build with testing off and no Python/Java discovery.
Public CI runs seventeen tests per compiler without private files; current
results and integration state are recorded in [status](status.md).

Previous density/base increment:

Normal density/base implementation source `1817417e8891e93ed5dbc205c9d1f6d8e6436a8d` passed
all 21 configured tests on native Windows/MSVC and WSL GCC/Clang without skips:
fourteen public checks, the numerical Java oracle and six original comparisons.
All eighteen original manifests identify that source, pass status and empty
errors, with matching executable hashes. Density's 150 snapshots match in all
three pairings, including 341732 binary64 values, 1427456 permutation entries,
600 continuation longs, 4358904 block bytes and 31744 biome identities per side.
All state/equivalence/preservation/suffix controls and 82 original q/scratch
checks agree. Public tests include 535745 terrain checks and six density helper
regressions; filesystem link cases ran without skips. Public CI runs fifteen
checks per compiler without private files; exact-head results are recorded in
[status](status.md). The three runtime libraries also build with testing off.

Previous climate/biome increment:

Climate/biome implementation source `6607b3650410bac2707276dd07769cda85215a6a`
passed all 18 configured tests on native Windows/MSVC and WSL GCC/Clang without
skips: twelve public checks, the numerical Java oracle and five original
storage/noise/climate comparisons. All fifteen original manifests identify that
source and pass with empty errors. Biome checks match 2879 common snapshots in
three pairings and 32 separate client-only snapshots; all table, field-preservation,
repeat, temperature-path and point/cache controls agree. Public helper link
cases ran without skips on all three toolchains. Native public CI runs thirteen
tests without private references. The three libraries also build with testing off.

Previous simplex increment:
Simplex implementation source `ec92cbf3774b4be154d6c2894324b6fe33ebceac` passed
all 15 configured tests on the same native Windows/MSVC and WSL GCC/Clang
toolchains below, without skips: ten public checks, the numerical Java oracle
and four original storage/noise comparisons. All twelve original manifests pin
that source and pass with empty errors. Five simplex helper regressions ran on
each platform without skips. That increment's public CI ran eleven tests without private
references. The three C libraries also build with testing off.

Previous Perlin implementation source `1188da0f126db8d623a45d801638d645df054c76` passed all
12 configured CTests without skips on the same three toolchains below: eight
public checks, the numerical Java comparison and three original NBT/compression/
noise comparisons. All nine original-comparison manifests record that commit,
pass status and empty errors. Public CI runs nine tests without private game
files; see [status](status.md) for CI and integration results at the tested commits.
All four helper regression tests ran on each local platform without skips.
The three runtime libraries also build with testing off and no Python/Java discovery.

Observed on 2026-10-17, Intel Core i9-10900F, x86-64:

| Environment | Toolchain and configuration | Result before integration |
| --- | --- | --- |
| Native Windows 10 Home 10.0.19045 | Visual Studio 18 Community, MSVC19.51.36260.0, CMake4.2.3, Ninja, Debug, `/W4 /WX /fp:strict`, Python3.14.3 | All 33 CTests pass at the cave source, with explicit Temurin17 |
| Ubuntu WSL2, Linux6.6.87.2, Ubuntu24.04 userspace | GCC13.3.0, CMake3.28.3, Ninja, Debug, ASan/UBSan, strict diagnostics, Python3.12.3 | All 33 CTests pass at the cave source with JDK21 |
| Same Linux environment | Clang18.1.3, same diagnostics/sanitizers | All 33 CTests pass at the cave source with JDK21 |

Current exact-source Windows checks explicitly select Temurin17.0.17; Linux
selects Ubuntu OpenJDK21.0.12. The numerical foundation also passed with the
installed Windows Oracle JDK21.0.10. The Temurin17 numerical comparison passes
135768 records with vector SHA-256
`6e267821b006c8219a806e69ac82f8d46bd208c24ebdfcaf02af89d8d6139c82`.
This agreement does not substitute for historical game execution.
Native GitHub runner results and the exact integrated commit are recorded in
[status.md](status.md). WSL is reported separately from native Linux CI.

`.github/workflows/ci.yml` builds/tests with GCC and Clang on Ubuntu24.04 and MSVC
on Windows2022, plus a format job. The workflow fetches no private JARs/assets or
launcher data. It has read-only repository permissions and produces no releases.
The actions use verified official release commit IDs. Checkout credentials are
not persisted. Storage statically links zlib; retain its
[license notice](dependencies.md) with future packages.

## Private reference checks

Follow [binary setup](reference/binaries.md), [resource import](reference/resources.md),
and [launch smoke](reference/launch.md), plus [NBT comparisons](reference/nbt.md).
Compression comparisons are described [separately](reference/compression.md).
Keep complete disassemblies, generated
vectors, logs, imported assets and worlds in ignored directories. Record
`git rev-parse HEAD` alongside local results. A local result without a commit
identifier must not be treated as a result for a later source revision.

Copy reference worlds before writable tests. The smoke helper creates a fresh
directory and refuses existing ones. Retain its generated world as a baseline,
make a separate copy for each later scenario, and restore from the untouched
baseline. Original-created/rewrite-created world round trips, corruption recovery,
dimensions, inventory/entity/tile state and restart persistence are not implemented.

No matched graphics/audio comparison, interactive rewrite play, extended session,
four-way multiplayer matrix, state-drift or performance measurement has occurred.
No performance target or additional platform support follows from these tests.
