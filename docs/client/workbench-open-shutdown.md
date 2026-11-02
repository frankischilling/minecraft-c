# Native shutdown with an open Workbench

A real Frame close can stop the client while Workbench remains open. The
Player still publishes the same `IT` through `e`, and the Screen still exposes
it through `ID.j`. Native cleanup now joins those lifetimes without running
`GS.r`, `OO.h`, Container close, cursor/grid drops or a World save. This is a
checked C ownership path. The complete original `Minecraft.d`/`run` shutdown
handlers and Java garbage-collection timing remain unqualified.

## Ownership and admission

Ordinary Workbench retirement still returns BUSY when a Player publishes its
owned Container. The selected menu terminal route has additional authority:
the bootstrap records its actual creator menu, that inactive menu ends its
reached calls, and bootstrap getters close. A private token freshly checks
the same live bootstrap, menu, App and startup association. It cannot authorize
World destruction by itself.

The router checks its complete reciprocal Load ledger and exact namespace
call count. It releases retained Inventory-screen captures and already closed
Workbench owners before identifying the live Screen's exact Player and World
epoch. Constructor and publication records must all name that actual Player.
The route uses the concrete native owning slot, rather than a copy of `e` or
`j`.

Player and Inventory then prove their canonical client namespace, exact
Player/World/Registry identities, default `AA`, live `IT`, actual `IX` and two
concrete Container bindings. Entered operations, child holds, malformed native
storage and inconsistent ownership refuse before sealing. The native proof
checks backing allocation extents, Slot owners and child lists; Stack values,
signed counts and legitimate Stack aliases keep their existing semantics.

After that proof, the same `IT` moves into the Player's native owner record.
No allocation or fallible operation separates the seal from the move. Source
`c`, `d`, `e`, `j` and the current Screen stay unchanged. New Player/Container
holds and Inventory mutations refuse; existing release and passive snapshots
remain available. Retiring Screen inspectors end their borrowed Container
views, while the actual Player keeps `IT` alive through later BUSY returns.

Dispatcher retirement and a fresh exact World audit remain mandatory. An
external Player or World hold still blocks the destructive tail. Releasing
that actual hold permits a new audit; an old successful prefix is never a
cached destruction permit. Final zero-hold Player disposal frees `IT`, `AA`
and `IX` in that order before its World binding ends. It performs no Source
close or field repair.

## Windows qualification

The [native receipt](../../tests/reference/client/workbench_open_shutdown/native-validation.json)
records four fresh Windows MSVC Release processes. Real SDK input opens the
Table at the declared saved-world location, puts two Logs in its grid, obtains
the four-Plank preview and carries seven Planks. The actual Frame close
callback sets `J=false` before native terminal preparation.

| Profile | Actual blocker and retry | Checks | Seconds |
| --- | --- | ---: | ---: |
| Clean | Same-container transfer and complete checked cleanup | 1,991 | 28.20 |
| Container | Extra `IT` hold refuses transfer; release permits retry | 2,301 | 29.66 |
| Player | Extra Player hold refuses the later World audit | 2,400 | 28.27 |
| World | Extra World binding refuses the later World audit | 2,212 | 14.43 |

The four CTests pass in 100.70 seconds. Each process keeps all 1,917 selected
inputs and 413 imported assets unchanged. It compares 97 Inventory, cursor,
grid, result and snapshot Stack roles, Player/Screen aliases, statistics,
World time, Entity membership and the reached Random states across native
preparation and declared BUSY retries. No additional World tick or drop runs.
The selected save tree stays byte-identical from stopped entry through final
cleanup, including the unrelated region. Both native windows are absent after
checked bootstrap retirement. Native screenshots are inspected separately;
there is no original-pixel comparison.

Earlier four-profile cohorts pass in 65.44, 66.43 and 68.52 seconds. Their separate
entropy, check counts and source bindings remain recorded. The final cohort
also includes passive Frame inspection through the still-owned bootstrap
after getters and the menu have ended. This retains the original Frame,
Canvas, App and SDK parent checks without reopening a getter or revisiting
the freed menu. A focused seven-test attempt passes the three prior public
regressions and two SDK profiles, while two other SDK processes fail during
SDK setup before terminal preparation; those remain separate failed observations. The initial
reproduction reaches the unchanged live-Container BUSY guard with
the same Source state and save bytes. Two earlier attempts fail in inherited
test setup before that guard; those captures remain separate. Java-free local
Inventory and genuine Player/OO controls exercise malformed ownership,
external holds, alias lifetimes, admission denial and final disposal. Their
authored fixtures do not establish the selected root's authority on their own.

A later initial four-profile attempt passes three cases in 69.32 seconds;
the clean case fails during SDK input setup before terminal preparation.
The helper treated a 50-record motion batch as evidence that its tap had
been consumed. Setup now drains SDK buffers through ordinary frames, sends
one timestamped tap and observes its actual consumption before retrying.
It retains the 12-attempt/60-second bound without an unrelated render-frame
limit. The final four-profile cohort above uses that helper. Queue, picking
and Player observations remain passive; no runtime input behavior changes.

Enable `MC_ENABLE_CLIENT_WORKBENCH_OPEN_SHUTDOWN_TESTS` with the existing
imported-resource and display options, then run:

```powershell
ctest --test-dir build-windows -R '^client\.workbench_open_shutdown_' --output-on-failure --no-tests=error
```

This qualification covers the four declared single-World Windows scenarios.
Multiple retained World epochs with live Workbenches, unrestricted Containers,
Linux SDK gameplay, extended play, complete Source shutdown and FPS remain
unqualified. Ordinary [keyboard close and save](workbench.md) and
[Player-tick eligibility close](workbench-auto-close.md) have separate scopes.
