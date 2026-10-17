# Test scope and reproduction

CTest separates public unit/tool tests from explicitly requested numerical
reference comparisons. Public tests do not require Minecraft files, Java, a
display, assets, or an account. Missing private references are unavailable results.
The Java oracle is a standard-library probe and can run in public CI without
original game files.

| Check | Coverage | Limits |
| --- | --- | --- |
| `core.java` | 146 fixed arithmetic/cast/Random edge and captured-oracle checks | No game simulation or game random-consumption ordering |
| `tools.import_assets` | 16 generated-fixture tests for copy/manifest, missing/hash/path/link/size/overwrite checks | No original assets in fixtures; canonical sound contents unverified |
| `reference.java_semantics` (opt-in) | 135768 exact C versus Java records, including float bit patterns | Current installed JVM primitives; not original-client/server game-state parity |
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

Use `ctest --test-dir build -L public --output-on-failure` for public tests.
Use `ctest --test-dir build -L reference --output-on-failure` for enabled Java
checks. For multi-configuration Windows builds add `-C Debug`.
`BUILD_TESTING=OFF` builds the C library without Python or Java discovery.

## Local environments

Observed on 2026-10-17, Intel Core i9-10900F, x86-64:

| Environment | Toolchain and configuration | Result before integration |
| --- | --- | --- |
| Native Windows 10 Home 10.0.19045 | Visual Studio 18 Community, MSVC19.51.36260.0, CMake4.2.3, Ninja, Debug, `/W4 /WX /fp:strict`, Python3.14.3 | All three CTests passed |
| Ubuntu WSL2, Linux6.6.87.2, Ubuntu24.04 userspace | GCC13.3.0, CMake3.28.3, Ninja, Debug, ASan/UBSan, strict diagnostics, Python3.12.3 | All three CTests passed |
| Same Linux environment | Clang18.1.3, same diagnostics/sanitizers | All three CTests passed |

Windows CMake selected an installed Oracle JDK21.0.10; Linux selected Ubuntu
OpenJDK21.0.12. A separate explicit Windows Temurin17.0.17 comparison also passed
135768 records with vector SHA-256
`6e267821b006c8219a806e69ac82f8d46bd208c24ebdfcaf02af89d8d6139c82`.
This agreement does not substitute for historical game execution.
Native GitHub runner results and the exact integrated commit are recorded in
[status.md](status.md). WSL is reported separately from native Linux CI.

`.github/workflows/ci.yml` builds/tests with GCC and Clang on Ubuntu24.04 and MSVC
on Windows2022, plus a format job. The workflow fetches no private JARs/assets or
launcher data. It has read-only repository permissions and produces no releases.
There are currently no runtime dependencies to bundle.

## Private reference checks

Follow [binary setup](reference/binaries.md), [resource import](reference/resources.md),
and [launch smoke](reference/launch.md). Keep complete disassemblies, generated
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
