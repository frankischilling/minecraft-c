# Falling Sand active tick

The client World now dispatches actual `ju.w_()V` for canonical Falling Sand
entities. It uses the existing Entity mover, notifying World setters, placement
predicate, Stack/Item constructors and Entity publication. The constructor and
NBT fields remain the same owners used by generated Sand and Gravel.

The authority is the supplied Beta 1.7.3 client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its 321-byte tick has no superclass tick call, first-age removal gate or Piston
branch. Constructor entropy and source Random state are not reset by ticking.
The `ju` class SHA256 is
`0e9cdae6a5ae0ea2e6199e0d690fd8c7d93c056cf0696de33f66c401d997fb25`;
the `w_()V` Code SHA256 is
`5cd57fd3ebc67c58bc361278a63b486bad4a4c4bf0618f7bb27c977113cf7c2f`.

## Source order

| Source site | Operation |
| --- | --- |
| JU8 | Carried Block ID zero invokes inherited `K()` and returns. |
| JU17/25/33/43 | Store previous position and increment the age with Java int wrapping. |
| JU55/71/83/95/107 | Subtract gravity, invoke the real mover, then damp all three velocity components. |
| JU114/122/130/141/159 | Floor the current position, read the actual World Block and clear it through the notifying setter when its ID matches the carried ID. This comparison runs every tick. |
| JU179/191/203/207 | On ground, damp horizontal motion, bounce vertical motion and mark the Entity dead before attempting placement. |
| JU223/238/255 | Run World placement with ignore-entities true and face1, invoke the actual falling-Block pass-through predicate below, then run the notifying setter. |
| JU268/280 | A failed landing checks the freshly read World remote flag before dropping one carried Item. |
| JU300/312/317 | Airborne age above100 checks the remote flag, drops the Item, then marks the Entity dead. |

The drop executes the actual inherited `sn.b(II)` wrappers and Stack-based
body with offset zero. It constructs a genuine one-Item Stack and Item Entity,
stores pickup delay10 and publishes through the freshly read World. The source
publication Boolean is discarded. Drop constructor and publication failures
retain their completed prefix; a timeout failure precedes `K`, while a landing
failure follows it.

## Native ownership

The retained diagnostic owns its exact actor, Registry, reached Worlds, failed
Stack/Item allocations and child producers. Each source World field read stays
independent across callbacks. Typed movement, mutation, placement, constructor
and publication views borrow that private work until checked retirement.
Retirement releases native occurrences and preserves published actors and
completed World mutations. Failed retirement retains its remaining work and
cleanup cause.

The leaf requires a zeroed, noncopyable error record. The World dispatcher
initializes the newly selected union member before invocation, then transfers
any failed work into its caller's output. The runtime retires that work before
World teardown. A World call with a NULL error output stops before a reached
Falling Sand tick because it has no owner for a failed prefix; earlier loop
effects remain. The graphical client supplies that owner.

## Verification

Windows and Linux Clang ASan/UBSan each pass 112 authored actual-owner checks. They cover
exact free-motion/damping words, unchanged private Random and base-tick state,
age overflow, matching-Block removal after the first age, real floor collision
and successful Dirt placement, remote landing/timeout behavior, ages100/101,
strict-FP and movement limits, actual Stack/Item publication, active-retirement
refusal and failed publication cleanup. World dispatch and NULL-output admission
have separate controls.

The [Create World scenario](../client/create-world.md) reaches this body in a
genuinely generated World and completes ordinary save/title/reselection. Its
mixed Entity sequence also exercises retained diagnostic initialization. These
native results do not establish executed-original tick/Item-entropy equality,
Falling Sand renderer pixels, every collision/hook closure or unrestricted play.

```text
cmake --build build --target mc_test_falling_tick
ctest --test-dir build -R '^entity\.falling_tick$' --output-on-failure
```
