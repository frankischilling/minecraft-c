# Development status

Active increment: `feature/region-inspector`, based on
`2cd7c04db5cc231c3ba865d5dc4e174a400248ca`, issue
[#14](https://github.com/frankischilling/minecraft-c/issues/14). The read-only
region module, CLI, public tests and offline captured-observation comparison
are implemented. Exact-source acceptance and draft PR checks are being recorded.
Live original-region validation remains pending; keep #14 open and do not merge
this increment on the strength of captured observations alone.

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

All four PRs are merged and their feature branches removed. M1 is complete.
The implementation uses system/subsystem folders with matching headers/tests,
plus repository clang-format 18 checks. No original binaries, assets, decompiled
source, launcher data or worlds entered the commits.

Latest tested runtime integration: `b4b0af1b1a54af8a7abb7c494992a853fa41b64e`,
[default-branch run39024379065](https://github.com/frankischilling/minecraft-c/actions/runs/39024379065).
Formatting and native Windows MSVC/Linux GCC/Clang jobs passed. Each compiler
passed all seven asset-independent CTests, with no skips; GCC/Clang use ASan/UBSan.
The exact PR head also passed before merge.

## Local acceptance

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

Read-only original region inspection is implemented on the active branch, with
49771 C checks, 19 CLI tests, 5 capture-helper regressions and 32 recorded
observations. Current local Windows/MSVC and WSL GCC/Clang sanitizer checks pass.
The captured check starts only the C probe, retains mode `recorded_observations`
and leaves live original-region checking pending. It does not invoke Java or
the capture producer. Public/private validation counts must stay distinct.
Preserve raw NBT
and compression as separate interfaces, use explicit limits and retain unknown
application fields. Region files, schemas, writable-world round trips, generation,
client/server runtimes and all remaining inventory behavior stay open.

The original server passed a fresh seed 17 offline loopback startup/save/stop
smoke during foundation work. It did not involve a client or C server. Ordinary
Minecraft Launcher capture timed out twice; no Play action or account-file access
occurred. The original-client/server baseline and all four interoperability
pairings remain untested. Continue headless work while graphical observation is
unavailable; see [launch notes](reference/launch.md).

The [wiki](https://github.com/frankischilling/minecraft-c/wiki) is published at
commit `1a45ac218c49d2abb899e4591ce401b9b0d7df17`, from the versioned `docs/wiki/`
sources. Home, build/tests, architecture and storage/reference pages were checked
through public HTTP and each returned the expected content. The clone remains
at `.local/wiki/`; preserve source/wiki synchronization when editing them.

MIT is owner-approved. Account authentication is the only approved gameplay
exception. Canonical external audio, unavailable legacy services and dormant
Sky-provider reachability still need research. No unanswered owner question
currently blocks coding; ask about exceptions only when established
unavailability affects completion.
