# Development status

Current branch: `feature/chunk-finishing`, based on main
`75a7eb48488a90558e76e14bfc145a49a35a38ea`.
Full normal finishing now connects local heights/direct sky, packed cached-world
queries, original light admission, ordered queue storage and dirty state.
The complete rewrite remains unfinished.

## Current evidence

The original-only six-case capture passes at
`.local/reference/finish-fixtures/capture-ca34ded514654e0eb6891161b3feaa50/manifest.json`.
Every case has two consecutive full passes per original side and a fresh
untouched control. All complete traced/control snapshot files match byte-for-byte.
The resulting public fixture passes C replay of 12 original inputs, 24576 exact
height-query records and 48 complete local/queue stages on native Windows.

Native MSVC19.51 and WSL GCC13.3/Clang18.1 compile the new modules. Constructed
finishing tests pass 3259 checks; seven helper guards pass. Clang-format18 checks
all 75 C/header files. Full exact-source platform comparisons and hosted checks
are pending. No pending check is a parity pass.

The tracing helper has run on native Temurin17 and WSL OpenJDK21. Its four
suspended marker inventories require the measured non-daemon main thread plus
unrelated daemon threads without original-loader frames. These finite headless
observations do not establish general game scheduling.

## Scope and next work

Finish review fixes, commit the implementation, then run the complete configured
suite and both-original comparisons on all three toolchains. Record exact
source and manifest fingerprints, integrate after successful checks and publish
the matching versioned wiki pages.

The cache supports nonnull lookup and the post-generation map/list publication
phase. Full load/generate reentrancy, pending unloads, lifecycle/population,
light propagation, complete generation and save compatibility remain open.
See [the contract](world/finishing.md), [evidence](reference/finishing.md),
[testing](testing.md) and [parity matrix](parity.md).

The previous light-queue integration and earlier exact platform evidence are
preserved in [history](history.md). M1 is complete; M2 through M5 remain open.
Preserve the separate `feature/region-inspector` draft until its live original
comparison is complete.

There is no playable C client/server or supported world writer. All four
multiplayer pairings, native graphical play, controls and presentation/audio
comparisons remain required. Original offline loopback server startup/save/stop
passed without a client. Graphical Launcher capture remains unavailable.
Account authentication is the only approved gameplay exception. No unanswered
owner question blocks current work.
