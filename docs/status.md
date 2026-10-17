# Resume note

Active branch: `feature/compression-codec`, based on tested `main` commit
`aa4640231cac9e8b356de07715a97d2260bd155d`. Active issue:
[#11](https://github.com/frankischilling/minecraft-c/issues/11), under M2.
The raw codec and modified UTF-8 increment is merged. Bounded memory gzip/zlib
compression, public interoperability and original-class comparisons are implemented.
Exact-source local checks pass; inspect the PR's final-head checks before integration.
Inspect the live worktree, PRs and exact-head checks before relying on this note.

## Integrated foundation

[PR #6](https://github.com/frankischilling/minecraft-c/pull/6) added C17 Java
numerical/cast/Random helpers, local resource import, pinned reference tooling,
the initial full-scope inventory, formatting and subsystem folders.
[PR #8](https://github.com/frankischilling/minecraft-c/pull/8) applied the owner's
repository text preferences and updated CI to pinned current official actions.
[PR #10](https://github.com/frankischilling/minecraft-c/pull/10) added raw NBT,
lossless modified UTF-8 and original-class comparisons, closing issue #9.
All three are merged; their branches were removed. The first scoped milestone is
complete. The full parity inventory in issue #4 remains open; there is no playable
C client/server, writable save support, or successful client interoperability test.

Exact tested default-branch commit: `aa4640231cac9e8b356de07715a97d2260bd155d`,
[run39023237616](https://github.com/frankischilling/minecraft-c/actions/runs/39023237616).
All four jobs passed: format, native Windows MSVC, native Linux GCC and Clang.
Each compiler passed all five CTests with no reference skips. Coverage includes
146 numerical, 596848 text and 394978 NBT checks, 16 generated asset tests,
and 135768 exact Java/C records.
GCC/Clang use ASan/UBSan. A public-only Linux build also passed without Java discovery.

Local implementation commit `8de9d81d317847c9bf25b58790e6baf261c8a199` passed
native MSVC and GCC/Clang WSL tests, supplied-JAR inspection, a real 409-resource
import with every hash reread, and original offline loopback seed 17 startup/save/stop.
Results remain under `.local/reference/`, `.local/assets-validation-8de9d81/`
and `.local/server-smoke-8de9d81/`. These results do not establish gameplay parity.

## Next work

Exact tested codec commit: `453fc56d80cb0e7f74b6a0a75e0849e78420195a`.
All six checks passed on native Windows/MSVC with Temurin17, and the five
public/numerical checks plus original NBT check passed under GCC/Clang WSL
ASan/UBSan with JDK21. The Linux original-check invocation initially had truncated
JAR arguments and correctly reported unavailable; corrected explicit paths
passed. No skipped result is counted as original parity. Reports under each
`build-*/reference-nbt/manifest.json` record this exact commit, source/JAR/tool/C
binary hashes, raw records and scope. Both C libraries also built with
`BUILD_TESTING=OFF`, without Python or Java discovery. Format18 checks pass.

The merged C implementation
preserves IDs 0..10, named/unnamed boundaries, UTF-16 units, empty-list
normalization, last-wins duplicate names and retained unknown application fields.
Public checks: 596848 text and 394978 NBT assertions. Native MSVC and WSL GCC/Clang
sanitizer suites pass. Original comparisons execute untouched JAR classes in
isolated processes: per side 194 reads, 40 writes, 12 packet controls, 157 transfers
and 435 re-reads. Four negative-list departures and 20 order-only byte differences
are explicit. Review corrected MSVC alignment, Java11 prerequisites, alias
preconditions, stale report handling and overly broad compound normalization.
The current increment is issue #11: a bounded gzip/zlib memory module with pinned
static zlib, public cross-reader checks and opt-in original wrapper comparisons.
Original research produced 183 records per side/runtime on JDK17 and JDK21;
all four sets agree. A whole-member decoder must record its deliberate rejection
of corrupt or missing trailers that the original root-early wrapper may leave
unread. Original region selectors are researched, but region files and world
write compatibility remain separate work. Full inventory issue #4 stays open.

Exact compression implementation commit: `50e5b7f5cd84091d05b9e5d373308a9c422414c2`.
Native Windows/MSVC with Temurin17 and WSL GCC/Clang ASan/UBSan with JDK21
passed all nine CTests, including both actual original-class NBT and compression
comparisons, with no skips. All six local original-comparison manifests under
`build-compression-*/reference-{nbt,compression}/manifest.json` record this commit,
status pass and empty errors. The 2351 compression C checks
include exact and failing allocation-growth budgets; Python cross-reader checks
pass 127 records on Windows zlib-ng and Linux zlib. The original comparison
passes 80 containers and 22 C encodes; each side executes 181 baseline records,
65 original gzip reads, 8 original writes, 35 original-to-C and 38 C-to-reference
transfers. It records 36 explicit malformed-container departures. Both libraries
also build with testing off, without Python/Java discovery; upstream zlib passed
13 Windows and 16 Linux tests separately. Reviews found no remaining issues.
The README now contains a short overview/quick start; detailed setup is in
`docs/building.md`, with matching wiki sources in `docs/wiki/`.

## Access limits and decisions

Ordinary Minecraft Launcher capture timed out twice; no Play action or account
file access occurred. Retry when capture works. The original-client/server
baseline and all four interoperability pairings remain untested.

The wiki is now initialized and its Git endpoint is accessible. The clone at
`.local/wiki/` contains the existing welcome page. Publish the reviewed
`docs/wiki/` pages after this increment is integrated, then verify the hosted
commit and rendered pages; publication has not yet been claimed.

MIT is owner-approved. Account authentication is the only approved gameplay
exception. Canonical external audio, unavailable legacy services and dormant
Sky-provider reachability need further research. No owner answer currently
blocks coding; ask about exceptions only when established unavailability affects
completion.
