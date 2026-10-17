# Resume note

Active branch: `feature/nbt-codec`, based on tested `main` commit
`293ed9923ad7797bc5b97f473a08c5c62d68c6ff`. Active issue:
[#9](https://github.com/frankischilling/minecraft-c/issues/9), under M2.
No PR is open for this increment yet. The raw codec, modified UTF-8 helper,
public tests and actual original-class differential are implemented. The
current worktree checks pass; an exact committed result is being recorded
before PR integration.
Inspect the live worktree, PRs and exact-head checks before relying on this note.

## Integrated foundation

[PR #6](https://github.com/frankischilling/minecraft-c/pull/6) added C17 Java
numerical/cast/Random helpers, local resource import, pinned reference tooling,
the initial full-scope inventory, formatting and subsystem folders.
[PR #8](https://github.com/frankischilling/minecraft-c/pull/8) applied the owner's
repository text preferences and updated CI to pinned current official actions.
Both are merged; their branches were removed. The first scoped milestone is
complete. The full parity inventory in issue #4 remains open; there is no playable
C client/server, writable save support, or successful client interoperability test.

Exact tested default-branch commit: `293ed9923ad7797bc5b97f473a08c5c62d68c6ff`,
[run39021606705](https://github.com/frankischilling/minecraft-c/actions/runs/39021606705).
All four jobs passed: format, native Windows MSVC, native Linux GCC and Clang.
Each compiler passed all three CTests with no reference skips. Coverage is 146
public numerical checks, 16 generated asset tests and 135768 exact Java/C records.
GCC/Clang use ASan/UBSan. A public-only Linux build also passed without Java discovery.

Local implementation commit `8de9d81d317847c9bf25b58790e6baf261c8a199` passed
native MSVC and GCC/Clang WSL tests, supplied-JAR inspection, a real 409-resource
import with every hash reread, and original offline loopback seed 17 startup/save/stop.
Results remain under `.local/reference/`, `.local/assets-validation-8de9d81/`
and `.local/server-smoke-8de9d81/`. These results do not establish gameplay parity.

## Next work

Finish issue #9's exact-commit checks and PR lifecycle. The C implementation
preserves IDs 0..10, named/unnamed boundaries, UTF-16 units, empty-list
normalization, last-wins duplicate names and retained unknown application fields.
Public checks: 596848 text and 394978 NBT assertions. Native MSVC and WSL GCC/Clang
sanitizer suites pass. Original comparisons execute untouched JAR classes in
isolated processes: per side 194 reads, 40 writes, 12 packet controls, 157 transfers
and 435 re-reads. Four negative-list departures and 20 order-only byte differences
are explicit. Review corrected MSVC alignment, Java11 prerequisites, alias
preconditions, stale report handling and overly broad compound normalization.
Keep gzip/regions/world writes and the broad inventory open. After this scoped
merge, select the next evidence-backed compression/region increment under M2.

## Access limits and decisions

Ordinary Minecraft Launcher capture timed out twice; no Play action or account
file access occurred. Retry when capture works. The original-client/server
baseline and all four interoperability pairings remain untested.

The wiki Git endpoint returned `Repository not found`; publishable pages remain
prepared in `docs/wiki/`, with no successful hosted update claimed.

MIT is owner-approved. Account authentication is the only approved gameplay
exception. Canonical external audio, unavailable legacy services and dormant
Sky-provider reachability need further research. No owner answer currently
blocks coding; ask about exceptions only when established unavailability affects
completion.
