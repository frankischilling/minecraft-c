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
