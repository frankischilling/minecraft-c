# Development status

The C runtime now composes chunk acquisition, saving, pending unloads and explicit
owner retirement. It preserves live callback order, a stable world clock,
metadata-first accounting and retained identities through reentry. Empty native
entity/tile collections are supported; nonempty Boat/Sign behavior remains an
external callback seam.

The next state layer adds [ItemStack ownership and NBT provenance](item/stack.md)
and [scheduled records and membership](world/scheduling.md). Its two CTest cases
pass 422 checks on native Linux GCC with sanitizers and Windows MSVC without
sanitizers. These are API contracts;
original item/container comparisons, ordered scheduling and bound-world behavior
remain unverified.

Exact-source lifecycle qualification passes at
`46546d8322de52beb6c3a51f30e827c0260d2b30`. Complete Windows v10, GCC v9 and
Clang v9 suites each passed 92 tests without failures or skips: 1940.77s,
2969.23s and 2961.75s respectively. Independent source/provenance and lifecycle
semantic audits, unchanged storage rechecks and all 22 capture roots per platform
also pass, with Linux ASan/UBSan and stable measured inputs. The controller retains bounded
JDI mirrors from their first recorded IDs; its separate debugger regression
passes on Windows JDK17 and Linux JDK21. The helper interval correction
permits 600 seconds only for observed dedicated101 operations; its separate
Python regression passes on both native hosts. The preceding v7 and v8 matrices
remain excluded. Windows v9 is also excluded after a premeasurement original
setup refusal; its successful rows do not count toward the fresh Windows v10
suite. See the
[lifecycle checkpoint](reference/lifecycle.md#exact-source-lifecycle-checkpoint)
for its evidence ledger and retained failed attempts.

The persistence foundation passes exact-source acceptance at
`9ad690d83592f127b9bb39d7233e8eef06fd0029`. All 71 selected local tests pass
without skips on native MSVC and WSL GCC/Clang, with Linux ASan/UBSan and
independent platform, provenance and copied-world semantic audits. The complete
rewrite remains unfinished. See the [storage checkpoint](reference/storage.md#exact-source-storage-checkpoint).

The preceding integrated main checkpoint `b7b33d4b66e027a6de18d04018cd0ba34dd87f0b`
combines normal
generation, synchronous cache acquisition and read-only region inspection.
Its tree matches final documentation checkpoint
`d0dde15a095424a2103dd755db546a9be2ab3c30`; implementation acceptance is at
`28a55b52c001c4c114a11ca7df844fc1f286d328`.
All 56 selected local tests pass without skips on native MSVC and WSL GCC/Clang,
with Linux ASan/UBSan and independent acceptance audit. Hosted checks pass 48
public/numerical tests per compiler and formatting for 89 C/header files.
Inspect the live worktree before continuing. The complete rewrite remains unfinished.

The [resulting-main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39091121154)
pass all four jobs. The initial Clang configuration rejected a dependency
download with the wrong hash, before compilation. Three fresh local downloads
matched the unchanged official checksum; the failed job passed on an unchanged
source retry. The failing response bytes were unavailable, so the precise
delivery failure is unknown. The published five-page wiki checkpoint is
`c4ecb344bffae2d5bd293f2678063b6bcc985025`, with all five remote page blobs
verified against `docs/wiki/`.

## Persistence foundation checkpoint

The [region image writer](storage/region-writer.md), [level metadata codec](storage/level.md),
stable stored chunks and [native world storage](world/storage.md) now have
finite original-to-C-to-original and C-to-original-to-C acceptance. Metadata
fallback/rotation, session checks, both explicit dimension roots, shared region
ownership and the actual acquisition loader are covered by original observations,
paired inputs and separately identified C failure policies.

At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, MSVC/GCC/Clang each pass 71
selected tests without skips: 744.00, 1255.43 and 1256.05 seconds respectively.
Independent audits bind all 290 tracked files, native tools/executables/build
inputs and all 36 original manifests. Separate audits check deep new-profile
provenance and complete supported world-state/control/preservation projections.
The three original-only fixtures remain unchanged. The
[hosted code checks](https://github.com/frankischilling/minecraft-c/actions/runs/39104921389)
pass 59 public/numerical tests per compiler and formatting for 111 C/header files.

The [evidence](reference/storage.md#exact-source-storage-checkpoint) retains
exact capture/report hashes, clocks, limitations and excluded earlier attempts.
These results cover seed23, eight storage slots and explicit dimensions 0/-1.
Opaque transport does not implement entity/player simulation or every subtype
serializer. Gameplay save/unload scheduling, dimension transitions,
running-server persistence and crash recovery remain open.

## Verified combined checkpoint

The coordinator composes retained C generation and finishing with pending
removal, ordered publication, activation and live population gates. The
retained-provider corpus has 20 calls, five generations and 5120 ordered query
pairs. A separate callback corpus adds successful original loads with live
time stamping and natural population, replaying observed cache/world effects
while C computes coordinator transitions. It retains 12 calls, 16 external
callbacks, 394 lifecycle events, 24 inventories and four untouched controls
per build. Decoding and terrain-feature bodies remain external.

Exact-source suite times are 529.51 seconds on MSVC19.51, 1044.42 on WSL GCC13.3
and 1044.86 on WSL Clang18.1. Each runs 47 public tests, the numerical Java oracle
and eight original comparisons. Independent audit verifies all 55 callback
source files against Git objects, unchanged inputs/classes, complete callback
projections and controls, source attribution for all 24 original manifests,
168 actual passed CTest rows and exact-head hosted checks. Its report SHA-256 is
`ef597d13ffa27582e223bd95f692cc890e4ac48f6d4ce61d8f95c77313932b2e`.
The initial Windows setup-unavailable attempt is preserved and excluded; the
accepted complete fresh-world retry has zero skips. See
[callback evidence](reference/cache-callbacks.md#exact-source-platform-checkpoint)
for all manifests, qualification limits and the unchanged observation cap.

[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39089309334)
pass at that exact source. Loading and population callback profiles each pass
C/client, C/server and client/server independently. The read-only region comparison also reruns
successfully within these suites, preserving its eight compatible cases and 41
explicit diagnostic departures.

## Verified normal composition checkpoint

Source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes all 41 configured
CTests on native MSVC19.51 and WSL GCC13.3/Clang18.1 without skips. Both Linux
builds use ASan/UBSan. Each exact-source suite runs 35 public tests, the numerical
Java oracle and original finishing, queue, chunk, nibble and generation
comparisons. These counts describe this checkpoint, not later branch additions.
The normal generator unit passes 99073 constructed contract checks.

The production generation corpus uses seeds 23, 24, 26 and 51, both original
sides and nine sequential calls per retained generator. All three pairings
pass independently per build. Each build retains 72 complete generations,
73728 ordered height-query pairs, 1224 stage events, 144 marker inventories and
eight complete byte-equal untouched controls. The lossless public fixture also
replays all eight continuous nine-call inputs on each toolchain.

Each generation manifest retains 57 unchanged attributed inputs and eight
unchanged compiled helper classes. Independent audit verifies all 47 project
source files against the exact commit, all complete records and world-file
inventories, actual 41-test logs and zero CTest exits, with no skips or audit
errors. The audit report SHA-256 is
`9acbf7d9865b4f9817bf1125d33078895df592d7e0bae5a973f88ff62ec80843`.
See [generation evidence](reference/generation.md#exact-source-platform-checkpoint)
for all three capture paths, manifest hashes and finite observation conditions.

[Hosted checks at a08c120](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass 36 public/numerical tests per compiler on native Windows MSVC and Linux
GCC/Clang, plus clang-format18 for 79 C/header files. Hosted checks use no
private originals. The [API contract](world/generation.md) documents borrowed
ownership, current materials, finite coordinates and failure preservation.

## Prior finishing integration

Normal finishing connects local heights/direct sky, packed cached-world
queries, original light admission, ordered queues and dirty state. Its runtime
integration is `fa17e3d8885632ca0baecf71bdf9551e47be68eb`; the latest integrated
finishing evidence checkpoint is `491962cfc0cce03e2572432b4ff3a00b5f2fc905`.

The original-only six-case capture passes at
`.local/reference/finish-fixtures/capture-ca34ded514654e0eb6891161b3feaa50/manifest.json`.
Each case has two consecutive full passes per original side and a fresh
untouched control. All complete traced/control snapshot files match byte for
byte. Public replay covers 12 original inputs, 24576 exact height-query records
and 48 complete local/queue stages.

Finishing source `a18615f0d9d90758f0ea95b0218cec97733d2adf` passes all 37
configured local CTests per compiler without skips, with Linux ASan/UBSan.
All six finishing cases match in three pairings. The twelve untouched controls
and 48 marker inventories pass per build. Each manifest retains 54 unchanged
inputs and six unchanged helper classes; all 44 project source files match the
exact commit. Constructed finishing tests pass 3259 checks; seven helper guards
pass. See [finishing evidence](reference/finishing.md) for manifest hashes.

Final finishing branch source `7c82b4a861e742e717b6ea8fd9646c5611211dba` passes
all 33 hosted public/numerical tests per compiler and formatting for 75 C/header
files in [run39076093215](https://github.com/frankischilling/minecraft-c/actions/runs/39076093215).
Its integration tree matches the tested tree. The
[resulting-main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39076208781)
also pass. Earlier [run39075690802](https://github.com/frankischilling/minecraft-c/actions/runs/39075690802)
passes the same hosted test and format counts. That finishing implementation
branch was removed; the normal-generation and cache checkpoints followed it.

The finishing wiki checkpoint was `4e4a9f3853ac8db012bca9aaf179d956e288a686`,
with remote head and five page blobs verified. Current wiki content is versioned
under `docs/wiki/`. README remains unchanged.

## Scope and next work

The normal entry uses the same world's borrowed biome manager, a separate
biome output preserving manager `d`, all 13 current material pairs, and finishing
before unpublished return. Both coordinates stay within `INT32_MIN + 8` through
`INT32_MAX - 9`. Measurements cover fresh finite headless worlds and complete
controls; they do not establish concurrent simulation or general scheduling.

Production observations cover persistent client `fd`/`ok`, base server `dj`/`jb`
and actual `MinecraftServer` construction with `dp`/`he`. Successful reloads
use genuine `mx`/`ie` save handlers and `ld`/`hb` loaders in the base-world
contexts. The dedicated constructor/API corpus covers force gates separately
from a running server. Pending removal, load/fallback selection, publication,
ordered insertion, activation and natural population remain distinct effects;
constructed recursion checks do not establish gameplay reentrancy.
The C cache now also has a [synchronous acquisition coordinator](world/cache.md).
The extended suite contains 3945 constructed checks recorded in development;
its exact-source qualification is recorded above. Independent
review matches all 231 acquisition instructions across `ok`/`jb`/`he`.
The combined retained-provider corpus now has three original-only contexts,
20 operations, five generated chunks, 5120 exact query pairs, 252 lifecycle
events, 40 inventories and three byte-equal untouched controls. Lossless public
replay and 28 Java-free guards pass on MSVC/GCC/Clang. A separate four-context
[callback comparison](reference/cache-callbacks.md) now passes original controls
and exact-source C comparisons for successful loads and natural population. Its
public fixture retains 12 calls, 394 lifecycle events and 24 inventories. Its
expanded exact-source acceptance is recorded above. Earlier source
`9a50ac67997082c89dbc43cf4274fba1427c1565` passes all 52 selected tests without
skips on MSVC/GCC/Clang, including seven original comparisons and Linux
sanitizers. Independent audit verifies all 53 source objects, 63 stable inputs,
nine helper classes and complete records per build; hosted checks pass 45 tests
per compiler and formatting for 88 C/header files. See the
[cache checkpoint](reference/cache.md#exact-source-platform-checkpoint).
These finite checks
do not establish a running server or complete cache lifecycle parity.

Successful original reloads return saved arrays unchanged while the loaded
minimum-height field remains zero. Their callback comparison preserves that
result without implementing a C disk decoder.
The [read-only region inspector](reference/region.md) has a guarded development
comparison covering all 49 queries per original side, with byte-equal controls,
eight compatible cases and 41 explicit diagnostic departures. Exact source
`092301ae8282d50a734df9018f791925aa9be99b` passes the original comparison on all
three platforms; independent audit and hosted 42-test/87-file checks pass.
See [region evidence](reference/region.md#verified-source-checkpoint).
No region result follows from a08 alone; its acceptance uses the separately
recorded exact source.

Terrain-feature population bodies, light propagation, other dimensions,
general tick-driven saving/unloading, gameplay restart/recovery and protocol
remain open. The synchronous lifecycle above does not complete these systems. See the
[roadmap](roadmap.md), [testing](testing.md) and [parity matrix](parity.md).
Earlier integrations remain in [history](history.md). M1 is complete; M2 through
M5 remain open.

There is no playable C client/server or supported gameplay world-loading
workflow. Native library writes have only the finite scope recorded above. All four
multiplayer pairings, native graphical play, controls and presentation/audio
comparisons remain required. Original offline loopback server startup/save/stop
passed without a client. Graphical Launcher capture remains unavailable.
Account authentication is the only approved gameplay exception. No unanswered
owner question blocks current work.
