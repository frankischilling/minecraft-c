# Protected World callback ownership

The client Arrow flight work now has explicit protected WorldAccess handoff
and owned Entity-sound/particle components. Complete SL flight, its World-tick
dispatch and the ownership joins below inherited base/damage/block queries
remain unfinished. Existing public calls still use the same legacy bodies.

A frame-resident execution context owns the exact evaluated World separately
from mutable Entity.world. Its Math pointer borrows the enclosing lazy Math
context; these World children never initialize or retire Math. The caller opens
a scope before intervening argument work. Each child checks that exact nullable
receiver and uses its precharged stable lease node. Neither closes the scope.

On a concrete callback failure, the dispatcher transfers its existing active
receiver occurrence into the node before releasing its temporary slot. It adds
no after-failure allocation or receipt capture increment. An intrusive receipt
guard separately prevents generic retirement even at ordinary capture count
one; the same actual active right prevents RenderGlobal detach. Managed receipt
identity is validated before dereferencing a returned candidate pointer. Unsafe
foreign views are suppressed; original value-only callback status/cause remain
separate from the native ownership refusal. Actual null sound receivers retain
their unowned null-reference envelope. Success and legacy paths reuse one body.

The caller clears every expired typed borrow before checked lease release and
keeps actual effect work alive until its own retirement. Native structural
refusal consumes no held right. The owner reserves storage for selected loader copies and producer diagnostics;
the loader acquisition and forwarding callers remain unfinished. The current owner supports a single evaluated
World scope; it does not close the nested acquisition/activation joins.

An actual callback reentry control first fails the prospective owner on both
hosts at its compound scope invariant, then passes the unchanged fixture after
the release preflight fix. The fix refuses an ARMED node before changing scope state. The
invalid-current-extent preflight also preserves that state; this additional
branch is static review scope. Original failed streams and real cleanup remain
retained; no source-field repair is used.

Both native hosts pass 326 handoff checks and 148 execution-child checks. Real
World dispatch covers self-removal and duplicate/live observer order, exact
arguments, unchanged private/World/profile Random state, NULL error ownership,
Source/native receipt distinctions, capture saturation, foreign-view rejection,
HELD/ARMED refusal and cleanup retry. Actual allocated-prefix N controls retain
the rr child after entity-added failure26, preserve Entity-sound failure22 and
exercise particle's normal null-guard return27. These are allocated prefixes,
not a completed N graphics constructor or returned game frame. Deeper particle
failure with an actual App/effect renderer remains unqualified.

The bounded public controller retains fresh Unicode fixture roots, both native
streams, process completion and current source/executable/DLL fingerprints.
Normal and optimized Python modes reject 34 corrupt transport envelopes each.
There is no new original JVM execution or whole-SL outcome comparison.

The local Linux client graph was restored from five exact pinned upstream
archives with normal X11/GL/Fontconfig prerequisites. Real RenderGlobal baseline
controls pass 342 checks and match eight unchanged finite sky-change rows on
both hosts. Client-only configuration exposed a Create World optional-target
guard error; guards now attach only to available bridges and require the actual
runtime for runtime tests. Both the minimal Linux and full Windows configuration
are exercised. The previous headless graph/inventory and failed attempts remain
retained. No replacement renderer, disabled required X11 backend or game window
was used for these controls.

The frozen regression selection passes **Windows 78/78 in 96.77 seconds** and
**Linux 76/76 in 84.74 seconds**, with every selected name actually Passed and
none skipped or NotRun. Linux uses Clang Debug with ASan/UBSan, leak detection
and halt-on-error. Its minimal client graph does not register
`client.arrow_draw_cpu` or `tools.arrow_draw_controls`; those two Windows cases
receive no Linux credit. Both builds pass. Exact clang-format 18.1.3 checks
2,267 C/header/fragment files; six real Linux compilation recipes also pass GCC
syntax checks with their actual includes and definitions.

## Source authority and reproduction

The unchanged original client archive has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its Entity-sound `FD.a(Lsn;Ljava/lang/String;FF)V` has 65 Code bytes with SHA-256
`09ab73ca4cacb1419a1f01389eaa0b97acbb36838d2ef89a48d5b0cc8418f651`.
Particle `FD.a(Ljava/lang/String;DDDDDD)V` has 55 Code bytes with SHA-256
`7327f1e8a539dfb9bd82998e5f8ba88793a47003cef7e3d3ff5a7d13a685e835`.
These static identities do not establish a new original execution comparison.

The native fixtures are [handoff](../../tests/world/runtime/world_access/test_handoff.c)
and [sound/particle children](../../tests/world/runtime/world_access/test_execution_children.c).
Their [bounded controller](../../tests/tools/world/test_execution_ownership.py)
registers through [CMake](../../cmake/tests/world_access_handoff.cmake).
Run the configured build's four CTests with:

```text
ctest --test-dir <build> -C Release --output-on-failure -R "^(world\.(access_handoff|execution_children)|tools\.world_execution_ownership_(normal|optimized))$"
```

Use the configured Linux Debug build without the Windows configuration argument.
The controller retains its report and raw streams even on a failed admission or
terminal check. The reentry red/green controls retain the original failed streams;
the production owner changed while the tested fixture stayed byte-identical.

## Remaining integration

Remaining work includes the complete Arrow parent, six further direct children,
owned inherited base/effect paths, stable nested evaluated Worlds and selected
loader forwarding before lossy TileEntity projections. Joint Store teardown for
a coordinator/Chunk.d mismatch is unqualified. Pickup, natural encounters,
original audiovisual/game-frame equality and sustained FPS remain unfinished.
