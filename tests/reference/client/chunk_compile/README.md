# Empty ChunkRenderer compilation

This fixture invokes the unchanged whole `dk.a()V` on a prepared genuine DK
whose origin is (1,128,1), dimensions are (2,2,2), dirty flag is true, both pass
flags start false, A starts true, E starts false and B/F are distinct empty
ArrayLists. Its actual World, provider and saved Chunks are constructed. The
body constructs the real EW cache and CV renderer; every Y128/129 block query
returns zero through EW's original Y restriction, so no GL command is reached.

`DK_EMPTY` records normal return, p0, p1, A, E, u, wrapped counter increment,
B count and F count. The source leaves u=true; only its later N caller clears
that field. The observer does not replace any method, returned Chunk or cache,
and verifies that World still supplies the same genuine Chunk after the call.
Network and native links are denied and must remain unused. Source/JDK/SDK
inputs and disposable World files are fingerprinted; only session.lock may
change. DK construction, nonempty geometry, GL commands and a terrain frame
are outside this one-row original fixture's scope.

```text
python -B tests/reference/client/chunk_compile/capture_reference.py \
  --client-jar /private/b1.7.3.jar --library-root /private/libraries \
  --output /private/new-dk-empty-original
```

## Separate native nonempty integration controls

The native test also exercises the actual N selector, DK compiler, leased EW,
CV dispatcher and NW singleton on a real one-stone section. Its actual
Minecraft defining namespace has a null static App, so the source `v()` query
selects flat shading. The call completes the real display list: seven recorded
commands, one draw and 36 expanded vertices. Pass 0 is nonempty, pass 1 is
empty, NW's batch is closed and its translation returns to zero. Only the
completed N caller clears `DK.u` and removes the queue entry. Checked child
retirement restores the earlier World binding count.

A following torch section reaches a remaining specialized family and fails
at `cv.b(Luu;III)Z:111`, retained beneath DK464 and N442. The real queue slot
has already become null; DK stays dirty, and the compiler retains its cache,
open list, pushed matrix and started NW batch. Native cleanup preserves the
earlier completed 36-vertex list and the unfinished definition's five pending
commands. It does not claim that the failed source call executed `glEndList`.

These device controls are separate from `finite.json`, which still contains
only the unchanged-original empty `DK_EMPTY` row. The nonempty controls add
native integration evidence; they are not an original nonempty differential
fixture or a complete terrain-frame or pixel comparison. The face, flat,
AO and color references have their own finite scopes, described in
[RenderBlocks source geometry](../../../../docs/client/render-blocks.md).
