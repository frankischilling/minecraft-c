# Development status

Active work continues on `feature/normal-chunk-generator`, based on
`491962cfc0cce03e2572432b4ff3a00b5f2fc905` on `main`. Normal composition is a
verified checkpoint at `a08c12056a06261ed01d9f66999459f7ce9543b4`; it has not
been integrated into `main`. Inspect the live worktree before continuing.
Cache acquisition/lifecycle and related read-only region work are extending
this branch. The complete rewrite remains unfinished.

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
branch has been removed; the normal-generator branch remains active.

The published wiki remains at `4e4a9f3853ac8db012bca9aaf179d956e288a686`, with
remote head and five page blobs verified for the finishing checkpoint. The
updated versioned pages under `docs/wiki/` await combined integration and have
not been published. README remains unchanged.

## Scope and next work

The normal entry uses the same world's borrowed biome manager, a separate
biome output preserving manager `d`, all 13 current material pairs, and finishing
before unpublished return. Both coordinates stay within `INT32_MIN + 8` through
`INT32_MAX - 9`. Measurements cover fresh finite headless worlds and complete
controls; they do not establish concurrent simulation or general scheduling.

Current production observations use client base world `fd`/cache `ok` and
server base world `dj`/cache `jb`. Actual `MinecraftServer` construction uses
world `dp` with cache `he`. Research must treat base-world acquisition separately
from the running dedicated-server path and observe genuine `ie` loads. Retain
pending-removal order, load/fallback behavior, generator reentrancy, publication,
ordered insertion, lifecycle hooks and natural population as distinct effects.
The C cache now also has a [synchronous acquisition coordinator](world/cache.md).
Its 838 constructed contract checks pass on MSVC, GCC and Clang. Independent
review matches all 231 acquisition instructions across `ok`/`jb`/`he`.
The combined retained-provider production comparison remains under development;
these contract checks do not establish complete cache lifecycle parity.

Earlier seven-operation cache research observes finishing before publication
and natural population on a 2x2 square. It does not verify successful disk
loads, fallback or reentrancy. New private genuine persistent client and server
reload observations distinguish a successful disk load from generation: saved
arrays return unchanged, while the loaded minimum-height field remains zero.
The [read-only region inspector](reference/region.md) has a guarded development
comparison covering all 49 queries per original side, with byte-equal controls,
eight compatible cases and 41 explicit diagnostic departures. Final exact-source
platform acceptance remains pending. No region result follows from a08 alone.

Cache-managed generation, complete lifecycle/population, light propagation,
other dimensions, compatible persistence and protocol remain open. See the
[roadmap](roadmap.md), [testing](testing.md) and [parity matrix](parity.md).
Earlier integrations remain in [history](history.md). M1 is complete; M2 through
M5 remain open.

There is no playable C client/server or supported world writer. All four
multiplayer pairings, native graphical play, controls and presentation/audio
comparisons remain required. Original offline loopback server startup/save/stop
passed without a client. Graphical Launcher capture remains unavailable.
Account authentication is the only approved gameplay exception. No unanswered
owner question blocks current work.
