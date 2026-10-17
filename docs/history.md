# Prior integration evidence

These checkpoints retain earlier results and context. The current branch,
verification and next actions are in [development status](status.md).

Latest integrated implementation: fresh chunk arrays, packed nibble access,
heightmaps and direct skylight at
`16ceea3c254b636cd6d24bb56f0bfa2dd88d8e01` on `main`. Runtime/reference source
`67c928db7c0d87e599f16228833b0234fba4bc24` passes all 30 local CTests on native
MSVC and WSL GCC/Clang without skips. Both original comparisons pass on every
build, including all five fresh/repeated chunk cases in three independent
pairings. The six manifests pass 660 fingerprint checks; public fixtures match
800 complete arrays from the original-only capture and three fresh runs.
Clang-format 18 passes for 61 C/header files. See [chunk evidence](reference/chunk.md)
for exact hashes and [the C contract](world/chunk.md) for ownership and scope.

The published branch and resulting main checks pass all 28 public/numerical
CTests per compiler plus formatting. The synthetic nibble fixture also covers
temporary-path aliases; all 18 guards pass on native Python 3.14 and WSL
Python 3.12. The five published wiki pages match their committed source blobs.
Resume from `main` and inspect the live worktree before making changes.
Next implementation work covers world-height/cache queries, ordered light
queues and dirty effects during chunk finishing; propagation remains separate.
The graphical client, dedicated server and complete rewrite remain unfinished.

Previous integrated implementation: normal caves, integrated at
`49f87ef9c803909ca8d89c8efe67a32da648cca1`. Exact source
`a507f59d756acf2d035ac7d4ea2da7171f60ad2f` passes all 33 local CTests on native
MSVC and WSL GCC/Clang without skips. All ten natural cases and 44 protected
room/path controls match in C/client, C/server and client/server comparisons.
All 30 original-reference manifests pass; independent review checks source,
executables, complete records, table, classes, inputs and inventories.
Branch [public CI](https://github.com/frankischilling/minecraft-c/actions/runs/39043911065)
passes 23 tests per compiler and formatting for 50 C/header files. The
[resulting-main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39044349578)
also pass. Its documentation checkpoint is integrated at
`0145f6d83ee25179204f812bb895df8f1b04319a`, with passing
[main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39044963945).

Previous integrated implementation: float lookup/floor, integrated at
`077300380a81596da82fbf67ea1cb625e3215436`. Exact source
`d6665e7f3e1b57c16e9e12c14983abd2ba227441` passes all 27 local CTests on native
Windows/MSVC and WSL GCC/Clang without skips. All 65536 table entries and 366
scalar calls match independently between C/client, C/server and client/server,
including fresh Temurin17/OpenJDK21 original executions. Public CI passes
nineteen tests per compiler plus formatting on the exact branch commit and resulting
main. This documentation checkpoint uses
`docs/math-integration`; inspect the live worktree before resuming.

Previous integrated implementation: normal surface replacement and chunk seeding,
integrated at
`1394928cebbcbe31700db3d280d0faa071ea6a34`. Exact source
`f7ac19ebed8a14c499bd233c2dcf3fb0f2502360` passes all 24 local CTests on native
Windows/MSVC and WSL GCC/Clang without skips. All 290 surface snapshots match
in C/client, C/server and client/server comparisons. Public CI passes seventeen
tests per compiler plus formatting on the exact branch commit and resulting main.
Its documentation checkpoint is integrated at
`d8e6fca665a9e881a5ab1711413b9b84e9e20116`; the resulting
[main CI](https://github.com/frankischilling/minecraft-c/actions/runs/39036383364)
also passes. Inspect the live worktree before resuming.

Previous integrated implementation: normal-dimension density and base-block interpolation,
integrated at
`6a98ab37457e9df342d366e9e336805e111b3c0d` on `main`. Exact source
`1817417e8891e93ed5dbc205c9d1f6d8e6436a8d` passes all 21 local CTests on
native Windows/MSVC and WSL GCC/Clang without skips. Public CI passes all fifteen
tests per compiler plus formatting on the exact branch commit and resulting main.
Its documentation checkpoint is integrated at
`de9722abe0449e6ba32e1da78bd2e4582cd556a1`. Caves, population, lighting,
save compatibility and playable client/server operation remain open.

Previous integrated implementation: original climate blending and biome classification,
integrated at
`b62de3e8742d1b2099b0f4f4d09a0b4d7cb98620` on `main`. Exact source
`6607b3650410bac2707276dd07769cda85215a6a` passed all 18 configured CTests on
native Windows/MSVC and WSL GCC/Clang without skips. CI for the tested branch
commit and resulting runtime integration passed all thirteen public tests on
each compiler plus formatting. The documentation checkpoint is integrated at
`7349e8334207f12cc1ee719cf489377e631a96d7`; its exact head and resulting main CI
passed. Inspect the live worktree and recorded evidence before resuming. The
numerical fixture establishes neither original world startup nor full terrain
generation.

Previous integrated implementation: original 2D simplex/octave noise,
integrated at
`00aabbd522f78a29ec55ee42ce8d92bfc0400f8b` on `main`.
Original constant bits were captured on both sides before selecting C literals.
Exact source `ec92cbf3774b4be154d6c2894324b6fe33ebceac` passed all 15 configured
tests on native Windows/MSVC and WSL GCC/Clang, without skips. Public CI passed
on exact branch commit `0626eaffffa328ba575c4325243865638e16be0f` and the resulting
runtime integration. This documentation checkpoint uses `docs/simplex-integration`.
Inspect the live worktree and exact-head checks before resuming.

Previous implementation increment: original Perlin/octave noise,
integrated at
`c6cac5d01e2750461f54482f66ea8d5b27d8a56b` on `main`.
Implementation commit `1188da0f126db8d623a45d801638d645df054c76` passed all 12
configured CTests on native Windows/MSVC and WSL GCC/Clang, without skips.
Public CI passed on exact branch commit `05592a347d7ac8d237c201337c61f2649f963666`
and the resulting runtime integration.

Read-only region inspection remains a draft implementation
at `8ab5f5e8f26a1dc12443dffe1f9f68993d9dd7d6`. All ten public CI tests passed
on Windows/Linux GCC/Clang, plus formatting, in
[run39026570248](https://github.com/frankischilling/minecraft-c/actions/runs/39026570248).
Its 32 recorded observations do not satisfy the pending live original-region
comparison. Preserve that branch; live validation remains pending.

Raw NBT, modified UTF-8 and bounded gzip/zlib compression are integrated. The
README is a short overview and quick start; detailed setup lives in
[building.md](building.md), with published [wiki pages](https://github.com/frankischilling/minecraft-c/wiki).
Inspect the live worktree and checks for the tested commits before resuming.

There is no playable C client/server, writable-world support or successful
client interoperability test. Full parity and milestones M2 through M5
remain incomplete.

## Integrated work

| Increment | Result |
| --- | --- |
| Deterministic core and tooling | C17 numerical/Random helpers, local asset import, reference tooling, initial inventory, formatting and subsystem folders |
| Repository conventions and CI | Repository text preferences and pinned current official CI actions |
| Raw NBT and modified UTF-8 | Owned raw NBT, lossless modified UTF-8 and original-class comparisons |
| Memory compression | Bounded gzip/zlib memory module, pinned static zlib, public/original interoperability and detailed documentation |
| Perlin and octave noise | Caller-owned seeded Perlin/octave primitives, original bulk behavior, exact both-side numerical comparisons and public regressions |
| 2D simplex and octave noise | Caller-owned 2D simplex/octave primitives, observed original constants, exact both-side numerical comparisons and public regressions |
| Climate and biome lookup | Seeded climate blending, float-built biome lookup, bounded retained arrays and exact both-side numerical/state comparisons |
| Density and base terrain | Allocation-free normal density/base terrain, all 82 seeded layers, exact original numerical/state comparisons and complete fixed fixtures |
| Surface replacement | Allocation-free normal surface replacement, explicit chunk/live Random contexts, current materials and exact state/block comparisons |
| Float lookup and floor | Caller-owned float trig table and exact lookup/floor helpers, full-table original comparisons and public path-alias regressions |
| Normal cave scheduling and carving | Complete natural and protected original comparisons, explicit Random/state preservation and public full-buffer fixtures |
| Local chunk data and packed access | Caller-owned voxel/nibble arrays, exact height/direct-skylight behavior, complete original comparisons and public full-array fixtures |

All twelve listed increments are integrated and their feature branches removed. M1 is complete.
The implementation uses system/subsystem folders with matching headers/tests,
plus repository clang-format 18 checks. No original binaries, assets, decompiled
source, launcher data or worlds entered the commits.

Latest tested runtime integration: local chunk data at
`16ceea3c254b636cd6d24bb56f0bfa2dd88d8e01`, with passing
[main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39059389712).
Exact branch commit `dc1320db05161f87c4e3b8733e1e9262f9beb477` passes
[branch checks](https://github.com/frankischilling/minecraft-c/actions/runs/39059210013).
Both runs pass all 28 public/numerical CTests on native Windows MSVC and Linux
GCC/Clang, plus formatting. The integrated tree matches the tested branch tree.

The first Windows run exposed a synthetic fixture comparing unresolved
temporary paths against canonical file records. The correction resolves the
fixture root once and adds a portable alias regression. Direct Windows short-path
and `child/..` checks reproduce the old failure and pass after correction,
including both successful and unavailable observations. C runtime, comparison
drivers, original observers and fixed arrays are unchanged from the recorded
30-test source; the later changes are Markdown and this synthetic fixture.

Previous tested cave runtime integration: `49f87ef9c803909ca8d89c8efe67a32da648cca1`,
with passing [main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39044349578).
All 23 public/numerical tests pass per compiler, along with formatting.

Previous tested math runtime integration: `077300380a81596da82fbf67ea1cb625e3215436`,
[main run39038169561](https://github.com/frankischilling/minecraft-c/actions/runs/39038169561),
passes all nineteen tests on native Windows MSVC/Linux GCC/Clang, plus
formatting. Exact branch commit
`1a217c3b79156d9f5fa28d9bd371ef856a40938e` passes
[run39037988919](https://github.com/frankischilling/minecraft-c/actions/runs/39037988919):
all nineteen tests on native Windows MSVC/Linux GCC/Clang, plus formatting.
The initial Windows head exposed an aliased generated source path in a guard;
the corrected fixture and explicit directory-alias regression pass. Runtime
code and numerical data are unchanged. All 27 local tests were repeated at the
corrected source. The locally tested source and integration differ only in Markdown.

Previous tested surface runtime integration: `1394928cebbcbe31700db3d280d0faa071ea6a34`,
[main run39036083441](https://github.com/frankischilling/minecraft-c/actions/runs/39036083441),
passes all seventeen tests on native Windows MSVC/Linux GCC/Clang, plus
formatting. Exact branch commit
`36d82aaa284b755ada3be5ab0d0c789a5d340868` passes
[run39035989117](https://github.com/frankischilling/minecraft-c/actions/runs/39035989117):
all seventeen tests on native Windows MSVC/Linux GCC/Clang, plus formatting.
The locally tested source and runtime integration differ only in Markdown.

Previous tested density/base runtime integration: `6a98ab37457e9df342d366e9e336805e111b3c0d`,
[main run39034545973](https://github.com/frankischilling/minecraft-c/actions/runs/39034545973).
All fifteen tests pass on native Windows MSVC/Linux GCC/Clang, plus formatting.
Exact branch commit `ebee25a9e64aa09cbe00a99b225d58a805fc1713` passes
[run39034316595](https://github.com/frankischilling/minecraft-c/actions/runs/39034316595).
The first head exposed an older-MSVC constant-condition warning in a test;
the assertion is now checked at compile time. Runtime code and fixed vectors
were unchanged. All 21 local tests were repeated at the corrected exact source.
The locally tested source and runtime integration differ only in Markdown.

Previous tested climate/biome integration: `b62de3e8742d1b2099b0f4f4d09a0b4d7cb98620`,
[main run39031285945](https://github.com/frankischilling/minecraft-c/actions/runs/39031285945).
All thirteen tests passed on native Windows MSVC/Linux GCC/Clang, plus formatting.
Exact branch commit `b1d28d89fdcec3950c31830625b0401df746c86c` passed
[run39031194746](https://github.com/frankischilling/minecraft-c/actions/runs/39031194746).
The locally tested source and runtime integration differ only in Markdown.

Previous tested simplex integration: `00aabbd522f78a29ec55ee42ce8d92bfc0400f8b`,
[main run39029339629](https://github.com/frankischilling/minecraft-c/actions/runs/39029339629).
All eleven tests passed on native Windows MSVC/Linux GCC/Clang, plus formatting.
The first Windows configure rejected a dependency download with a checksum mismatch.
A fresh official archive matched the existing pin; only the failed job was
rerun, and attempt 2 passed without changing source, pin or assertions. The exact
branch commit passed [run39029195752](https://github.com/frankischilling/minecraft-c/actions/runs/39029195752).
The tested implementation source and this runtime integration differ only in Markdown.

Previous tested Perlin integration: `c6cac5d01e2750461f54482f66ea8d5b27d8a56b`,
[main run39027900248](https://github.com/frankischilling/minecraft-c/actions/runs/39027900248).
All nine tests passed on native Windows MSVC/Linux GCC/Clang, plus formatting.
The first Clang configure rejected a dependency download with a checksum mismatch.
A fresh official download matched the existing pin; only the failed job was
rerun, and attempt 2 passed without changing source, pin or assertions. The exact
branch commit passed [run39027782875](https://github.com/frankischilling/minecraft-c/actions/runs/39027782875).

Earlier tested storage integration: `b4b0af1b1a54af8a7abb7c494992a853fa41b64e`,
[default-branch run39024379065](https://github.com/frankischilling/minecraft-c/actions/runs/39024379065).
Formatting and native Windows MSVC/Linux GCC/Clang jobs passed. Each compiler
passed all seven asset-independent CTests, with no skips; GCC/Clang use ASan/UBSan.
The tested branch commit also passed before integration.

## Local acceptance

Chunk source: `67c928db7c0d87e599f16228833b0234fba4bc24`. All three toolchains
pass 27 public tests, the numerical Java oracle and two original comparisons.
No tests are skipped. Complete local arrays, opacity preservation, packed
getter sweeps and setter snapshots match independently in all three pairings.
The [chunk evidence](reference/chunk.md) records six manifest hashes, source
and artifact verification, paired fixture provenance and actual observation
budgets. Runtime review covers C ownership, bounds and defined arithmetic;
direct source/fixture audits cover the observer fixes and complete captured
bytes.
World-height/cache queries, lighting queues and propagation remain separate.

Previous float lookup/floor increment:

Float lookup/floor source: `d6665e7f3e1b57c16e9e12c14983abd2ba227441`. All three
toolchains pass eighteen public tests, the numerical Java oracle and eight
original comparisons. All 24 manifests under
`build-math-*/reference-*/manifest.json` record this source, pass status and empty
errors, with matching current probe hashes. Each math report compares every one
of 65536 initialized float entries and 366 scalar calls in all three pairings.
The table's big-endian raw-bit SHA-256 is
`87a67a39fff59636acb04edcda4ef622831037291f11fb61586c57596771dac2`
for all nine participants. Original table identity/content and C table content
remain unchanged across calls.

Fresh Windows comparisons use Temurin17; WSL comparisons use OpenJDK21. Public
math tests pass 903 checks. All ten helper guards, including parser controls,
filesystem aliases/links and completed-mismatch precedence, run without skips. Reports
verify source/executable/JAR/tool/runtime inputs before/after, compiled classes
and published artifacts. Source hashes also match the tested Git blobs.
Independent runtime, fixture and protocol reviews found no outstanding defects.
The initial Windows CI head exposed an aliased path in a generated test
fixture. Its paths are now canonicalized, with an explicit alias regression;
runtime code and numerical data are unchanged. All 27 local tests were repeated
at the corrected exact source.
Clang-format18 checks all 41 C/header files; all three libraries build with
testing off and no Python/Java discovery. These numerical results establish the
documented math prerequisite, with sampled scalar inputs and measured backends.

Previous surface increment:

Normal surface source: `f7ac19ebed8a14c499bd233c2dcf3fb0f2502360`. All three
toolchains pass sixteen public tests, the numerical Java oracle and seven
original comparisons. All 21 manifests under
`build-surface-*/reference-*/manifest.json` record this source, pass status and
empty errors; current executable hashes match every recorded C probe.

Surface's 290 snapshots match all three pairings, including 554948 binary64
values, 1511424 permutation entries, 1160 live and 1160 constructor continuation
longs, 8914866 block bytes, 67072 biome identities and 134144 material bytes per
participant. Each participant passes 50 equivalence, 1861 preserved-field,
105 live-Random advance and 114 live-Random preservation controls. Each original
also completes 105 internal surface-state and 20 retained-apply field checks.
The ten fresh client setup roots per platform contain no files; ten server
roots contain only eight-byte session locks plus directories. Genuine server
setup can generate chunks before the fresh measured generator is created.

Public surface tests pass 1114102 checks; existing terrain tests pass 535745.
All six surface helper guards run without skips, including completed-mismatch
precedence. Independent reviews traced the seven complete fixed fixtures and
full corpus to both originals and checked runtime, adapters and reports.
Clang-format18 checks all 36 C/header files. The three runtime libraries build
with testing off and no Python/Java discovery. See the
[surface evidence](reference/surface.md) for scope and reproducibility.

Previous density/base increment:

Normal density/base source: `1817417e8891e93ed5dbc205c9d1f6d8e6436a8d`. All three toolchains
passed fourteen public tests, the numerical Java oracle and six original
NBT/compression/Perlin/simplex/biome/density comparisons. All eighteen manifests
under `build-density-*/reference-*/manifest.json` record this source, pass status
and empty errors; current executable hashes match every recorded C probe.

All 150 density snapshots match C/client, C/server and client/server, including
341732 binary64 values, 1427456 permutation entries, 600 continuation longs,
4358904 block bytes and 31744 biome identities per side. Each original completes
82 internal-density/full-scratch recomputation checks. Every side also passes
49 equivalence, 346 preserved-field, 141 unchanged-Random and 25 constructor-state
controls, plus exact suffix, retention and block-domain checks. Each platform's
nine client setup roots contain no files; nine server roots contain only
8-byte session locks plus directories. These are numerical generation contexts,
not complete game/server startup or save compatibility.

Public terrain tests pass 535745 checks. All six density helper regressions,
including completed-mismatch versus later-unavailable precedence and filesystem
links, run without skips on all three toolchains. Independent reviews traced all
three complete fixed fixtures to both untouched originals and checked runtime,
Java bindings, controls, reporting and source attribution. Clang-format18 checks
all 30 C files; the three runtime libraries build with testing off and no
Python/Java discovery. Exact-head public CI and runtime integration pass as linked above.

Previous climate/biome increment:

Climate/biome source: `6607b3650410bac2707276dd07769cda85215a6a`. All three
toolchains passed twelve public tests, the numerical Java oracle and five
original NBT/compression/Perlin/simplex/biome comparisons. All fifteen original
manifests under `build-biome-*/reference-{nbt,compression,noise,simplex,biome}/manifest.json`
identify that source, pass status and empty errors. Executable hashes match the
tested probes. All 2879 common snapshots matched C/client, C/server and
client/server; all 32 client-only snapshots matched C/client. The 4096-entry
initialized table, thirteen names, exact array bits, lengths and all identity
masks agree on every platform.

Each common side passed 72 equivalence, one selector distinction, 1512 preserved
field, 36 temperature-path and 144 point/cache controls. Each client-only side
passed four equivalence, 76 preserved-field and four point/cache controls.
Public tests passed 33155 biome checks and five helper guards, including link
cases without skips. Independent reviews traced all 4096 fixed table entries,
212 float cases, 79 double cases and four seeded climate sequences to both
original captures, and checked source, original bindings and comparison controls.
The probe captures integer addresses before replacement for retention checks.
Clang-format 18 checked all 26 C files; all three native libraries build with
testing off and no Python/Java discovery. Complete biome gameplay, terrain,
storage round trips and playable client/server operation remain open.

Previous simplex increment:
Simplex source: `ec92cbf3774b4be154d6c2894324b6fe33ebceac`. All three toolchains
passed ten public tests, the numerical Java oracle and four original
NBT/compression/Perlin/simplex comparisons. All twelve original manifests under
`build-simplex-*/reference-{nbt,compression,noise,simplex}/manifest.json` identify
that source, pass status and empty errors. Public tests passed 5005 simplex
checks and five helper guards, including filesystem link cases without skips.
All 812 requests matched in the three simplex pairings: 204923 double values,
348160 permutation entries and 3248 continuation longs per side. Constants and
all equivalence/factor/suffix/zero-layer controls agree. Clang-format 18 checked
all 22 C source/header files; native testing-off builds include all three libraries.

Independent reviews checked source against both-side bytecode, the reflective
helper/controls, and fixed-vector provenance plus public test/probe coverage.
One protocol discrepancy was corrected before this source commit: constants
requests require zero seed in both probes. The built C probe's rejection was
checked separately. The original-only fourteen-case capture is ignored under
`.local/reference/simplex-fixed-vectors/`; no original source/assets entered commits.
Complete biome gameplay, density combination,
terrain, population, lighting and playable client/server runtimes remain open.

Previous Perlin/octave increment:
Noise source: `1188da0f126db8d623a45d801638d645df054c76`. All three toolchains
passed eight public checks, the numerical Java oracle and three original
NBT/compression/noise comparisons. The nine manifests under
`build-noise-*/reference-{nbt,compression,noise}/manifest.json` identify that
commit, pass status and empty errors. The noise comparison matched all 783
requests in each of its client/C, server/C and client/server comparisons:
140218 double values, 399872 permutation entries and 3132 continuation longs
per side. Public tests passed 4850 noise checks and four helper regressions;
symlink/hard-link cases ran without skips. All 18 C source/header files pass
clang-format 18. Native testing-off builds include all three runtime libraries.

Independent reviews checked the source against bytecode, the reflective
comparison and input/report guards, and the public test/probe coverage. Every
fixed numerical vector was traced to both originals; the 14-case original-only
capture is ignored under `.local/reference/noise-fixed-vectors/`. Full terrain,
simplex biomes, density/surfaces, caves, population, lighting and game runtimes
remain open. Continue those requirements rather than expanding primitive-noise
results into a full-world claim.

The following results describe the previously integrated compression increment.
Exact runtime implementation source: `50e5b7f5cd84091d05b9e5d373308a9c422414c2`.
Native Windows/MSVC 19.51 with Temurin 17 and Ubuntu WSL GCC 13.3/Clang 18.1 with
JDK 21 passed all nine CTests, including actual original-class NBT and compression
comparisons. All six reports under
`build-compression-*/reference-{nbt,compression}/manifest.json` record this commit,
pass status and empty errors. WSL results are separate from native Linux CI.

Coverage includes 146 numerical, 596848 text, 394978 NBT and 2351 compression
checks; 135768 exact Java/C numerical records; 16 generated asset tests; and
127 public C/Python compression records. Compression tests include failed
large-output allocation growth, exact budgets and unchanged caller results.
Both C libraries also build with testing off and no Python/Java discovery.
The normal verified zlib download and offline source override were exercised.
Upstream zlib separately passed 13 Windows and 16 Linux tests.

Raw NBT comparisons per side execute 194 reads, 40 original writes, 12 packet
controls, 157 original-to-C parses and 435 original re-reads. Four negative-list
departures and 20 compound-order-only byte differences remain explicit.
Compression checks execute 80 containers and 22 C encodes; per side 181 baseline
records, 65 original gzip reads, 8 original writes, 35 original-to-C and 38
C-to-reference transfers. There are 36 explicit malformed-container departure
records, including complete trailer validation and strict header policies.
Payload/canonical bytes remain exact; compressed block identity is not claimed.

Reviews resolved MSVC alignment, Java prerequisites, alias contracts, stale
reports, broad compound normalization and missing allocation-growth coverage.
Prerequisite/version/output-path guards passed; missing references remain
unavailable results. See [testing](testing.md) and the [reference notes](reference/compression.md).

## Next work and access

Read-only region inspection remains a draft implementation;
its live original comparison is still pending. Preserve raw NBT and compression
as separate interfaces, use explicit limits and retain unknown application fields.
Normal density/base terrain, surface replacement and chunk seeding are integrated.
The float lookup/floor prerequisite and normal cave runtime are integrated.
[Fresh chunk data, heightmaps and direct skylight](world/chunk.md) are integrated
for the local array contract. Continue with original world-height/cache queries,
chunk visibility, ordered light queues and dirty effects. Record genuine setup
state because the server's spawn Random makes the cache/queue baseline variable.
Full propagation, storage schemas and both-direction copied-world round trips
still need separate comparisons. Client/server runtimes and the full remaining
inventory stay open.

The original server passed a fresh seed 17 offline loopback startup/save/stop
smoke during foundation work. It did not involve a client or C server. Ordinary
Minecraft Launcher capture timed out twice; no Play action or account-file access
occurred. The original-client/server baseline and all four interoperability
pairings remain untested. Continue headless work while graphical observation is
unavailable; see [launch notes](reference/launch.md).

The [wiki](https://github.com/frankischilling/minecraft-c/wiki) is published at
`7b149b37b6518e86bc91c491bcd235fcea86d000`. The remote head is verified and all
five page blobs match the versioned sources at
`16ceea3c254b636cd6d24bb56f0bfa2dd88d8e01`. Updated home, build/tests,
architecture and storage/reference pages describe chunk ownership, packed
access, height/skylight comparisons and the remaining world-finishing scope.
The clone remains at `.local/wiki/`.

MIT is owner-approved. Account authentication is the only approved gameplay
exception. Canonical external audio, unavailable legacy services and dormant
Sky-provider reachability still need research. No unanswered owner question
currently blocks coding; ask about exceptions only when established
unavailability affects completion.
