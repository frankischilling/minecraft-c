# Development status

Implementation branch: `feature/simplex-noise` from
`706f4cafcda04071fd53c8a30f18e8f594f5cb05`, issue
[#19](https://github.com/frankischilling/minecraft-c/issues/19). The C module,
public tests and live original numerical comparison are implemented.
Original constant bits were captured on both sides before selecting C literals.
Exact source `ec92cbf3774b4be154d6c2894324b6fe33ebceac` passed all 15 configured
tests on native Windows/MSVC and WSL GCC/Clang, without skips. Inspect exact-head
public CI and integration state in the linked issue before resuming.

Latest implementation increment: original Perlin/octave noise, issue
[#16](https://github.com/frankischilling/minecraft-c/issues/16), merged in
[PR #17](https://github.com/frankischilling/minecraft-c/pull/17) at
`c6cac5d01e2750461f54482f66ea8d5b27d8a56b` on `main`.
Implementation commit `1188da0f126db8d623a45d801638d645df054c76` passed all 12
configured CTests on native Windows/MSVC and WSL GCC/Clang, without skips.
Public CI passed on exact PR head `05592a347d7ac8d237c201337c61f2649f963666`
and the resulting runtime integration. Inspect the live worktree and remote
state before resuming. This documentation checkpoint uses `docs/noise-integration`.

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

All five PRs are merged and their feature branches removed. M1 is complete.
The implementation uses system/subsystem folders with matching headers/tests,
plus repository clang-format 18 checks. No original binaries, assets, decompiled
source, launcher data or worlds entered the commits.

Latest tested runtime integration: `c6cac5d01e2750461f54482f66ea8d5b27d8a56b`,
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
Complete biome climate/classification, consumer array quirks, density combination,
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
Continue simplex biome noise, terrain density/surfaces and exact chunk comparisons,
alongside storage schemas and both-direction copied-world round trips. Client/server
runtimes and the full remaining inventory stay open.

The original server passed a fresh seed 17 offline loopback startup/save/stop
smoke during foundation work. It did not involve a client or C server. Ordinary
Minecraft Launcher capture timed out twice; no Play action or account-file access
occurred. The original-client/server baseline and all four interoperability
pairings remain untested. Continue headless work while graphical observation is
unavailable; see [launch notes](reference/launch.md).

The [wiki](https://github.com/frankischilling/minecraft-c/wiki) is published at
`15aba6335f35178260dde0a97fa15412ec83dbaf`. All five page blobs match the versioned
`docs/wiki/` sources. Home, build/tests, architecture and storage/reference each
returned HTTP200 with the expected content; updated pages include the noise
module and comparison option. The clone remains at `.local/wiki/`.

MIT is owner-approved. Account authentication is the only approved gameplay
exception. Canonical external audio, unavailable legacy services and dormant
Sky-provider reachability still need research. No unanswered owner question
currently blocks coding; ask about exceptions only when established
unavailability affects completion.
