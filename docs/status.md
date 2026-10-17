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

Source `a18615f0d9d90758f0ea95b0218cec97733d2adf` passes all 37 configured
CTests on native MSVC19.51 and WSL GCC13.3/Clang18.1, without skips. Both Linux
builds enable ASan/UBSan. Each build runs 32 public tests, the numerical Java
oracle and original finishing, queue, chunk and nibble comparisons.
All six finishing cases match in three independent pairings. The twelve
untouched controls and 48 marker inventories pass per build. The manifests
retain matching before/after fingerprints for 54 inputs and six helper classes;
all 44 recorded project source files independently match the exact commit.
See [finishing evidence](reference/finishing.md) for manifest hashes.

Constructed finishing tests pass 3259 checks; seven helper guards pass.
Native Windows and Linux GCC/Clang each pass all 33 hosted public/numerical
tests plus formatting for 75 C/header files in
[run39075690802](https://github.com/frankischilling/minecraft-c/actions/runs/39075690802).

The tracing helper has run on native Temurin17 and WSL OpenJDK21. Its four
suspended marker inventories require the measured non-daemon main thread plus
unrelated daemon threads without original-loader frames. These finite headless
observations do not establish general game scheduling.

## Scope and next work

Independent source and fixture review finds no remaining defects in this
scope. The implementation is committed; integrate after the final branch head
passes hosted checks, then publish the matching versioned wiki pages.

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
