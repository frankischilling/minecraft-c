# Test scope and reproduction

CTest separates public unit/tool tests from explicitly requested reference
comparisons. Public tests do not require Minecraft files, Java, a
display, assets, or an account. Missing private references are unavailable results.
The numerical Java oracle is a standard-library probe and can run in public CI
without original game files. Original-class NBT/compression/noise comparisons remain local.

| Check | Coverage | Limits |
| --- | --- | --- |
| `core.java` | 146 fixed arithmetic/cast/Random edge and captured-oracle checks | No game simulation or game random-consumption ordering |
| `core.mutf8` | 596848 checks, including every UTF-16 unit, accepted byte forms, malformed input and unchanged outputs | Payload codec only; NBT and packet framing are distinct |
| `storage.nbt` | 394978 checks for all original tag types, boundaries, retained state, ownership, budgets, errors and deterministic mutations | Raw tags only; no world schema or compression |
| `storage.compression` | 2351 checks for gzip/zlib round trips, headers/trailers, ownership, byte/allocation/member limits and mutations, including failed large-output growth | Memory containers only; no region/world files |
| `world.noise` | Seeded permutation/state, fixed original numerical observations, scalar/bulk/octave behavior and bounded caller spans | Perlin/octave primitives only; no complete terrain or simplex biome noise |
| `tools.compression_interop` | 127 exact payload/framing records between C and Python gzip/zlib, including all optional header field combinations | Container interoperability, not original gameplay |
| `tools.import_assets` | 16 generated-fixture tests for copy/manifest, missing/hash/path/link/size/overwrite checks | No original assets in fixtures; canonical sound contents unverified |
| `reference.java_semantics` (opt-in) | 135768 exact C versus Java records, including float bit patterns | Current installed JVM primitives; not original-client/server game-state parity |
| `reference.nbt` (local opt-in) | Per original side: 194 reads, 40 original writes, 12 packet controls, 157 original-to-C and 435 original re-reads | Selected JDK; 4 malformed negative-list departures and 20 order-only differences recorded separately |
| `reference.compression` (local opt-in) | 80 containers, 22 C encodes; per side 181 baseline records, 65 original gzip reads, 8 original writes, 35 original-to-C and 38 C-to-reference transfers | 36 explicit malformed-container departure records; no copied-world compatibility claim |
| `reference.noise` (local opt-in) | Untouched client/server numerical methods; exact double bits, offsets, permutations and Random continuation | Scoped procedural noise; no chunks, population or lighting |
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

Use `ctest --test-dir build -L public --output-on-failure` for public tests.
Use `ctest --test-dir build -L reference --output-on-failure` for enabled Java
checks. For multi-configuration Windows builds add `-C Debug`.
`BUILD_TESTING=OFF` builds the C libraries and pinned zlib without Python or Java discovery.

## Local environments

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
