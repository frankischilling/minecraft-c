# Development status

In progress: normal-dimension density composition and base-block interpolation,
[issue #24](https://github.com/frankischilling/minecraft-c/issues/24), on
`feature/normal-terrain`. Genuine original client/server contexts now produce
matching constructor, climate, density and base-block observations, including
warm water and cold-sea ice. Fixed regressions and the repeatable comparison
suite are being completed. Exact-source acceptance and integration remain pending.
These observations do not establish complete chunks or playable game operation.

Latest implementation: original climate blending and biome classification, issue
[#21](https://github.com/frankischilling/minecraft-c/issues/21), merged in
[PR #23](https://github.com/frankischilling/minecraft-c/pull/23) at
`b62de3e8742d1b2099b0f4f4d09a0b4d7cb98620` on `main`. Exact source
`6607b3650410bac2707276dd07769cda85215a6a` passed all 18 configured CTests on
native Windows/MSVC and WSL GCC/Clang without skips. Exact PR and resulting
runtime CI passed all thirteen public tests on each compiler plus formatting.
Documentation checkpoint [PR #25](https://github.com/frankischilling/minecraft-c/pull/25)
is integrated at `7349e8334207f12cc1ee719cf489377e631a96d7`; its exact head and
resulting main CI passed. Inspect the live worktree and linked issue before resuming. The numerical fixture establishes
neither original world startup nor full terrain generation.

Previous integrated implementation: original 2D simplex/octave noise, issue
[#19](https://github.com/frankischilling/minecraft-c/issues/19), merged in
[PR #20](https://github.com/frankischilling/minecraft-c/pull/20) at
`00aabbd522f78a29ec55ee42ce8d92bfc0400f8b` on `main`.
Original constant bits were captured on both sides before selecting C literals.
Exact source `ec92cbf3774b4be154d6c2894324b6fe33ebceac` passed all 15 configured
tests on native Windows/MSVC and WSL GCC/Clang, without skips. Public CI passed
on exact PR head `0626eaffffa328ba575c4325243865638e16be0f` and the resulting
runtime integration. This documentation checkpoint uses `docs/simplex-integration`.
Inspect the live worktree and exact-head checks before resuming.

Previous implementation increment: original Perlin/octave noise, issue
[#16](https://github.com/frankischilling/minecraft-c/issues/16), merged in
[PR #17](https://github.com/frankischilling/minecraft-c/pull/17) at
`c6cac5d01e2750461f54482f66ea8d5b27d8a56b` on `main`.
Implementation commit `1188da0f126db8d623a45d801638d645df054c76` passed all 12
configured CTests on native Windows/MSVC and WSL GCC/Clang, without skips.
Public CI passed on exact PR head `05592a347d7ac8d237c201337c61f2649f963666`
and the resulting runtime integration.

Read-only region inspection remains in [draft PR #15](https://github.com/frankischilling/minecraft-c/pull/15)
at `8ab5f5e8f26a1dc12443dffe1f9f68993d9dd7d6`. All ten public CI tests passed
on Windows/Linux GCC/Clang, plus formatting, in
[run39026570248](https://github.com/frankischilling/minecraft-c/actions/runs/39026570248).
Its 32 recorded observations do not satisfy the pending live original-region
comparison. Preserve that branch and keep issue #14 open.

Raw NBT, modified UTF-8 and bounded gzip/zlib compression are integrated. The
README is a short overview and quick start; detailed setup lives in
[building.md](building.md), with published [wiki pages](https://github.com/frankischilling/minecraft-c/wiki).
Inspect the live worktree, issues, PRs and exact-head checks before resuming.

There is no playable C client/server, writable-world support or successful
client interoperability test. Full parity issue [#4](https://github.com/frankischilling/minecraft-c/issues/4)
and milestones M2 through M5 remain open.

## Integrated work

| Increment | Result |
| --- | --- |
| [PR #6](https://github.com/frankischilling/minecraft-c/pull/6) | C17 numerical/Random helpers, local asset import, reference tooling, initial inventory, formatting and subsystem folders |
| [PR #8](https://github.com/frankischilling/minecraft-c/pull/8) | Repository text preferences and pinned current official CI actions |
| [PR #10](https://github.com/frankischilling/minecraft-c/pull/10) | Owned raw NBT, lossless modified UTF-8 and original-class comparisons; closes #9 |
| [PR #12](https://github.com/frankischilling/minecraft-c/pull/12) | Bounded gzip/zlib memory module, pinned static zlib, public/original interoperability and detailed documentation; closes #11 |
| [PR #17](https://github.com/frankischilling/minecraft-c/pull/17) | Caller-owned seeded Perlin/octave primitives, original bulk behavior, exact both-side numerical comparisons and public regressions; closes #16 |
| [PR #20](https://github.com/frankischilling/minecraft-c/pull/20) | Caller-owned 2D simplex/octave primitives, observed original constants, exact both-side numerical comparisons and public regressions; closes #19 |
| [PR #23](https://github.com/frankischilling/minecraft-c/pull/23) | Seeded climate blending, float-built biome lookup, bounded retained arrays and exact both-side numerical/state comparisons; closes #21 |

All seven listed PRs are merged and their feature branches removed. M1 is complete.
The implementation uses system/subsystem folders with matching headers/tests,
plus repository clang-format 18 checks. No original binaries, assets, decompiled
source, launcher data or worlds entered the commits.

Latest tested runtime integration: `b62de3e8742d1b2099b0f4f4d09a0b4d7cb98620`,
[main run39031285945](https://github.com/frankischilling/minecraft-c/actions/runs/39031285945).
All thirteen tests passed on native Windows MSVC/Linux GCC/Clang, plus formatting.
Exact PR head `b1d28d89fdcec3950c31830625b0401df746c86c` passed
[run39031194746](https://github.com/frankischilling/minecraft-c/actions/runs/39031194746).
The locally tested source and runtime integration differ only in Markdown.

Previous tested simplex integration: `00aabbd522f78a29ec55ee42ce8d92bfc0400f8b`,
[main run39029339629](https://github.com/frankischilling/minecraft-c/actions/runs/39029339629).
All eleven tests passed on native Windows MSVC/Linux GCC/Clang, plus formatting.
The first Windows configure rejected a dependency download with a checksum mismatch.
A fresh official archive matched the existing pin; only the failed job was
rerun, and attempt 2 passed without changing source, pin or assertions. The exact
PR head passed [run39029195752](https://github.com/frankischilling/minecraft-c/actions/runs/39029195752).
The tested implementation source and this runtime integration differ only in Markdown.

Previous tested Perlin integration: `c6cac5d01e2750461f54482f66ea8d5b27d8a56b`,
[main run39027900248](https://github.com/frankischilling/minecraft-c/actions/runs/39027900248).
All nine tests passed on native Windows MSVC/Linux GCC/Clang, plus formatting.
The first Clang configure rejected a dependency download with a checksum mismatch.
A fresh official download matched the existing pin; only the failed job was
rerun, and attempt 2 passed without changing source, pin or assertions. The exact
PR head passed [run39027782875](https://github.com/frankischilling/minecraft-c/actions/runs/39027782875).

Earlier tested storage integration: `b4b0af1b1a54af8a7abb7c494992a853fa41b64e`,
[default-branch run39024379065](https://github.com/frankischilling/minecraft-c/actions/runs/39024379065).
Formatting and native Windows MSVC/Linux GCC/Clang jobs passed. Each compiler
passed all seven asset-independent CTests, with no skips; GCC/Clang use ASan/UBSan.
The exact PR head also passed before merge.

## Local acceptance

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

Read-only region inspection is implemented in draft PR #15 under issue #14;
its live original comparison is still pending. Preserve raw NBT and compression
as separate interfaces, use explicit limits and retain unknown application fields.
Continue normal-dimension density and base terrain under
[#24](https://github.com/frankischilling/minecraft-c/issues/24), then surface replacement
and chunk seeding under [#26](https://github.com/frankischilling/minecraft-c/issues/26),
and exact chunk comparisons, alongside storage schemas
and both-direction copied-world round trips. Client/server runtimes and the
full remaining inventory stay open.

The original server passed a fresh seed 17 offline loopback startup/save/stop
smoke during foundation work. It did not involve a client or C server. Ordinary
Minecraft Launcher capture timed out twice; no Play action or account-file access
occurred. The original-client/server baseline and all four interoperability
pairings remain untested. Continue headless work while graphical observation is
unavailable; see [launch notes](reference/launch.md).

The [wiki](https://github.com/frankischilling/minecraft-c/wiki) is published at
`3c70516044bf66a339274b4565c95c227a08fd06`. All five page blobs match the versioned
`docs/wiki/` sources. Home, build/tests, architecture and storage/reference each
returned HTTP200 with expected content. Updated pages include climate/biome
classification and its comparison option. The clone remains at `.local/wiki/`.

MIT is owner-approved. Account authentication is the only approved gameplay
exception. Canonical external audio, unavailable legacy services and dormant
Sky-provider reachability still need research. No unanswered owner question
currently blocks coding; ask about exceptions only when established
unavailability affects completion.
