# Player spawn reset

`mc_player_spawn_reset` executes actual client `gs.t_()` on a genuine ready
LocalPlayer. It sets eye height and size, invokes inherited `sn.t_()`, then
stores health20 and death counter0. The size dispatch passes through actual
`ls.b(FF)` and its `sn.b(FF)` body. `mc_player_spawn_reset_body` exposes only
the inherited `sn.t_()` body on that same concrete Player.

The base body checks the source World once before its loop. A null World
returns immediately. Otherwise it tests current Y and, while positive,
repositions the same bounds, requests the actual World's collision List,
and raises current Y by1 if that List is nonempty. An empty result exits the
search. The tail clears Z, Y and X velocities in that order, then pitch.
The loop does not repeat the initial null-World check after callbacks.

| Source state | Base effect |
|---|---|
| Null World | Preserve bounds, velocities and pitch. |
| World with zero, negative or NaN Y | Skip search; clear velocities and pitch. |
| Positive Y | Search using real Block and Entity collision contributions. |
| Null bounds with positive Y | Preserve position/size prefix and fail at `sn.e` invoke86. |
| Null bounds with nonpositive or NaN Y | Bounds remain unread; tail completes. |

The wrapper's health/death stores occur after a completed base return,
including the null-World return. A failed collision or position descendant
prevents those later stores.

Each call retains the actual Player and owns private heap diagnostics. The
existing World collision operation and returned List lease protect their
actual owners during synchronous work. Every successful lease is released
before the next search iteration or return. Source World and bounds references
are reloaded at their actual later bytecode points; no spawn-position proxy,
copied collision result or substitute World is used.

Caller error storage is published only at final return. Failed collision
children retain their full Block, acquisition, nearby Entity and TileEntity
diagnostics. Position and size wrapper PCs remain separate. Success returns
a fresh empty error. Native invalid owner, phase, allocation, floating profile
and work-limit failures are distinct from original exceptions.

The optional search ceiling counts pending positive-Y iterations. Exhaustion
preserves earlier effects and reports failure before the next position call.
It never returns a successfully shortened search. There is no implicit bounds
or vector pool reset, Player publication, controller call, World update, save,
GUI transition or input operation.

Position arithmetic is shared with the existing Entity position helper.
Its coordinate stores and width/height prefix remain in their original order;
the shared arithmetic computes only the six bounds values. Strict SSE2
operations preserve binary32 widening, binary64 association, subtraction's
left NaN priority and addition's right NaN priority. The latter is a separately
observed correction to compiler-selected two-NaN results in the old native
arithmetic, rather than a claim that extracting it alone changed no results.
The public position setter keeps its existing admission before source stores.

The finite original reference uses unchanged constructed application, Session,
World, Chunk and LocalPlayer classes. It covers both the wrapper and a verified
direct `REF_invokeSpecial` base body, including ordinary searches, slabs,
fences, beds, signed zero, subnormal/NaN coordinates and actual null-reference
prefixes. A separate position probe supplies explicit raw-bit vectors. See
the authored reference procedures under `tests/reference/entity/spawn_reset`
and `tests/reference/entity/position`.

These entries supply spawn/reset bodies only. Application `h/i` publication,
single-player controller setup, complete World entry, World scheduling and an
interactive client remain separate work.
