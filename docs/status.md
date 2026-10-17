# Development status

Active branch: `feature/light-queue`. The next increment implements original
light-region merging and ordered queue storage after world admission. Full
chunk finishing and the complete rewrite remain unfinished.

## Current work

The C module retains wrapped bounds/volume arithmetic, newest-first coalescing
across five entries, explicit caller-owned capacity and the original million-entry
abort. Actual original observations cover 48 cases with 72 merge requests,
43 world requests per side and the real overflow clear. Public fixtures replay
complete queues from both observed baselines.

Native Windows/MSVC and WSL GCC13/Clang18 builds succeed. Initial Windows
checks pass the new original queue comparison and public fixtures. Exact-source
local tests, independent review, hosted checks and wiki publication are pending.
The prior local chunk comparison requires a full tested-source label, which will
be set after committing the implementation.

The repository has `.clang-format`, matching system/subsystem folders and
separate public/local reference tests. README stays a short overview; details
live in [building](building.md), [architecture](architecture.md), the
[parity matrix](parity.md), [queue contract](world/lighting.md) and
[original queue evidence](reference/lighting.md).

## Integrated baseline

Local chunk arrays, packed access, heights and direct skylight are integrated at
`16ceea3c254b636cd6d24bb56f0bfa2dd88d8e01`. The latest baseline is
`c98ffc4`, including its evidence checkpoint. Previous detailed results are
preserved in [integration history](history.md).

M1 is complete. M2 through M5 remain open. The read-only region inspector is
still a separate draft on `feature/region-inspector`; its live original-region
comparison remains pending. Preserve that branch.

## Next work

Complete exact-source queue verification and integration. Then continue original
world-height query tracing, cache insertion/visibility, world admission and
finishing dirty effects. Retain all 1024 ordered height queries and compare
traced calls with untouched controls. Queue propagation, population, save round
trips, client/server runtimes and the full gameplay inventory remain required.

There is no playable C client or server, writable-world support or successful
client interoperability test. The original server passed an offline loopback
startup/save/stop smoke; all four client/server pairings remain untested.

Original Launcher capture previously timed out. Continue headless work while
rechecking graphical access when needed. MIT is owner-approved; account
authentication is the only approved gameplay exception. Legacy audio/services
and dormant Sky-provider reachability still need research. No unanswered owner
question blocks the current implementation.
