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

Hosted checks and wiki publication are pending. The exact source is published
on `feature/light-queue`; merge only after native hosted checks pass. The
versioned wiki pages already describe this increment.

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

Complete hosted queue verification and integration. Then continue original
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
