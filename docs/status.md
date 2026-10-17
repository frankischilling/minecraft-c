# Development status

Latest runtime integration: original light-region merging and ordered queue
storage at `fe83e6fe67f287698312bd32136b83a7f0cd31f2` on `main`. Continue
from `main` and inspect the live worktree. Full chunk finishing and the complete
rewrite remain unfinished.

## Current work

The C module retains wrapped bounds/volume arithmetic, newest-first coalescing
across five entries, explicit caller-owned capacity and the original million-entry
abort. Actual original observations cover 48 cases with 72 merge requests,
43 world requests per side and the real overflow clear. Public fixtures replay
complete queues from both observed baselines.

Exact source `900369d2bdea598324e5faf61e9a97d9a1aabd70` passes all 33 local
CTests on native Windows/MSVC19.51 and WSL GCC13.3/Clang18.1 without skips.
Each build runs 29 public tests, the numerical Java oracle and actual original
queue, chunk and nibble comparisons. GCC/Clang use ASan/UBSan. Queue comparisons
use Temurin17 on Windows and OpenJDK21 on WSL.

The public queue test passes 2001560 checks. Each original side contributes
72 merge results, 43 complete queue snapshots and a real million-entry abort.
C/client and C/server replay each match 112 records. The three manifests retain
identical before/after input and helper fingerprints. Different setup baselines
remain explicit. Independent source/fixture review found no remaining defects;
all 86 original queue states reconstruct exactly from the public fixture deltas.
Clang-format18 passes all 66 C/header files.

Branch source `d9f065bb3d9dcaa462e08d6afcbaf0d1e8fc658f` passes all 30 hosted
public/numerical tests per compiler plus formatting in
[run39071978648](https://github.com/frankischilling/minecraft-c/actions/runs/39071978648).
The runtime integration tree matches that tested tree. The resulting
[main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39072086470)
also pass all 30 tests per compiler and formatting. The implementation branch
has been removed.

The wiki is published at `d373a098ada79390e0719dca5ab6c8d82f62f7ef`; remote
head verification succeeds. All five page blobs match versioned sources at the
runtime integration. README remains unchanged. Earlier detailed integration
results moved to [history](history.md), keeping this resume note short.

The repository has `.clang-format`, matching system/subsystem folders and
separate public/local reference tests. README stays a short overview; details
live in [building](building.md), [architecture](architecture.md), the
[parity matrix](parity.md), [queue contract](world/lighting.md) and
[original queue evidence](reference/lighting.md).

## Earlier foundations

Local chunk arrays, packed access, heights and direct skylight are integrated at
`16ceea3c254b636cd6d24bb56f0bfa2dd88d8e01`. Its evidence checkpoint is
`c98ffc4`. Previous detailed results are
preserved in [integration history](history.md).

M1 is complete. M2 through M5 remain open. The read-only region inspector is
still a separate draft on `feature/region-inspector`; its live original-region
comparison remains pending. Preserve that branch.

## Next work

Continue original world-height query tracing, cache insertion/visibility, world admission and
finishing dirty effects. Retain all 1024 ordered height queries and compare
traced calls with untouched controls. Queue propagation, population, save round
trips, client/server runtimes and the full gameplay inventory remain required.

Existing chunk records retain complete cache/queue baselines but contain no
dynamic query events. Evaluate JDI entry/exit tracing of actual `fd.d(II)I` and
`dj.d(II)I` during the measured finishing calls. Verify return-value capability,
single-threaded mutation and bounded complete traces before relying on it.
Retain the original packed cache-key behavior, including coordinate collisions.
This tracing approach is statically supported; runtime feasibility is unverified.

There is no playable C client or server, writable-world support or successful
client interoperability test. The original server passed an offline loopback
startup/save/stop smoke; all four client/server pairings remain untested.

Original Launcher capture previously timed out. Continue headless work while
rechecking graphical access when needed. MIT is owner-approved; account
authentication is the only approved gameplay exception. Legacy audio/services
and dormant Sky-provider reachability still need research. No unanswered owner
question blocks the current implementation.
