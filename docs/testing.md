# Test scope and reproduction

CTest separates public unit/tool tests from explicitly requested reference
comparisons. Public tests do not require Minecraft files, Java, a
display, assets, or an account. Missing private references are unavailable results.
The numerical Java oracle is a standard-library probe and can run in public CI
without original game files. Original-class storage, noise, climate and terrain
comparisons remain local.

| Check | Coverage | Limits |
| --- | --- | --- |
| `core.java` | 146 fixed arithmetic/cast/Random edge and captured-oracle checks | No game simulation or game random-consumption ordering |
| `core.mutf8` | 596848 checks, including every UTF-16 unit, accepted byte forms, malformed input and unchanged outputs | Payload codec only; NBT and packet framing are distinct |
| `storage.nbt` | 394978 checks for all original tag types, boundaries, retained state, ownership, budgets, errors and deterministic mutations | Raw tags only; no world schema or compression |
| `storage.compression` | 2351 checks for gzip/zlib round trips, headers/trailers, ownership, byte/allocation/member limits and mutations, including failed large-output growth | Memory containers only; no region/world files |
| `world.noise` | 4850 checks: seeded permutation/state, independently traced original scalar/bulk/octave observations, zero sizes/layers and unchanged invalid-call outputs | Perlin/octave primitives only; no complete terrain or simplex biome noise |
| `world.simplex` | 5005 checks: observed constants/state/grids, mixed constructor ordering, original numerical boundaries, independent/default octave factors and unchanged rejected-call state/output | 2D simplex primitives only; no biome climate/classification or complete terrain |
| `world.biome` | 33155 checks: all 4096 observed table cells, 212 float and 79 double cases, climate vectors, buffer growth/suffixes, staged allocation rollback and unchanged rejected-call state | Climate/classification module; no world startup, terrain or complete biome behavior |
| `world.terrain` | 535746 checks: three full original-captured density/base fixtures, constructor state, retained suffixes, interpolation ordering and unchanged rejected-call output | Density and base blocks only; complete terrain requires later passes |
| `tools.noise_reference` | 4 generated prerequisite/report/path/symlink/hard-link regressions | No original classes run; filesystem link cases skip only if unsupported |
| `tools.simplex_reference` | 5 generated prerequisite/hash/report/path/link regressions | No original classes run; link cases skip only if unsupported |
| `tools.biome_reference` | 5 generated prerequisite/hash/report/path/link regressions | No original classes run; link cases skip only if unsupported |
| `tools.density_reference` | 6 generated prerequisite/hash/report/path/link and mismatch-before-unavailable regressions | No original classes run; link cases skip only if unsupported |
| `tools.compression_interop` | 127 exact payload/framing records between C and Python gzip/zlib, including all optional header field combinations | Container interoperability, not original gameplay |
| `tools.import_assets` | 16 generated-fixture tests for copy/manifest, missing/hash/path/link/size/overwrite checks | No original assets in fixtures; canonical sound contents unverified |
| `reference.java_semantics` (opt-in) | 135768 exact C versus Java records, including float bit patterns | Current installed JVM primitives; not original-client/server game-state parity |
| `reference.nbt` (local opt-in) | Per original side: 194 reads, 40 original writes, 12 packet controls, 157 original-to-C and 435 original re-reads | Selected JDK; 4 malformed negative-list departures and 20 order-only differences recorded separately |
| `reference.compression` (local opt-in) | 80 containers, 22 C encodes; per side 181 baseline records, 65 original gzip reads, 8 original writes, 35 original-to-C and 38 C-to-reference transfers | 36 explicit malformed-container departure records; no copied-world compatibility claim |
| `reference.noise` (local opt-in) | Per side 783 requests, 140218 exact double values, 399872 permutation entries and 3132 following Random longs; three separate pair comparisons | Scoped procedural noise; no chunks, population or lighting |
| `reference.simplex` (local opt-in) | Per side 812 requests, 204923 exact double values, 348160 permutation entries and 3248 following Random longs; actual short/extended overload and factor/suffix controls | Scoped 2D numerical methods; consumer-like inputs do not establish biome results |
| `reference.biome` (local opt-in) | Actual catalogue/table, direct float and quantized double selectors, processed temperature and biome grids with stateful buffer sequences | Explicit-seed numerical fixture; no executed world constructors or terrain |
| `reference.density` (local opt-in) | 150 exact snapshots per side, 82 original density/scratch recomputation checks; all constructor, climate, density and base state across three pairings | Normal dimension only; no surface, caves, population or complete chunk parity |
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

Use `ctest --test-dir build -L public --output-on-failure` for public tests.
Use `ctest --test-dir build -L reference --output-on-failure` for enabled Java
checks. For multi-configuration Windows builds add `-C Debug`.
`BUILD_TESTING=OFF` builds the C libraries and pinned zlib without Python or Java discovery.

## Local environments

Normal density/base implementation source `02e18641dc5910a8d3e6e419dfea3f6757467707` passed
all 21 configured tests on native Windows/MSVC and WSL GCC/Clang without skips:
fourteen public checks, the numerical Java oracle and six original comparisons.
All eighteen original manifests identify that source, pass status and empty
errors, with matching executable hashes. Density's 150 snapshots match in all
three pairings, including 341732 binary64 values, 1427456 permutation entries,
600 continuation longs, 4358904 block bytes and 31744 biome identities per side.
All state/equivalence/preservation/suffix controls and 82 original q/scratch
checks agree. Public tests include 535746 terrain checks and six density helper
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
files; see [status](status.md) and the linked issue for exact-head CI/integration.
All four helper regression tests ran on each local platform without skips.
The three runtime libraries also build with testing off and no Python/Java discovery.

Observed on 2026-10-17, Intel Core i9-10900F, x86-64:

| Environment | Toolchain and configuration | Result before integration |
| --- | --- | --- |
| Native Windows 10 Home 10.0.19045 | Visual Studio 18 Community, MSVC19.51.36260.0, CMake4.2.3, Ninja, Debug, `/W4 /WX /fp:strict`, Python3.14.3 | All nine CTests passed, including original NBT/compression with explicit Temurin17 |
| Ubuntu WSL2, Linux6.6.87.2, Ubuntu24.04 userspace | GCC13.3.0, CMake3.28.3, Ninja, Debug, ASan/UBSan, strict diagnostics, Python3.12.3 | All nine CTests passed with JDK21 |
| Same Linux environment | Clang18.1.3, same diagnostics/sanitizers | All nine CTests passed with JDK21 |

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
