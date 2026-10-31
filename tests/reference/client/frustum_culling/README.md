# Frustum wrapper and ChunkRenderer culling

Eight finite rows compare unchanged Beta 1.7.3 methods with the C implementation.
Four rows run genuine `sr` constructors, each calling the real `w.a()` on a
64x64 LWJGL Display with identity projection and modelview matrices. They check
the shared `dr` identity, raw recenter coordinates, both box overloads, signed
zero and a subnormal coordinate. Four rows exercise complete `n.a(yn,float)`
culling with 20 source `dk` receivers and different `n.j` values, including
integer wrap. Their visible-field masks distinguish empty-pass skipping,
already-visible throttling and actual `dr` plane tests.

The N and DK receivers use Object-only allocation with explicitly prepared
fields. Their constructors do not run in this observer. Bounds come from the
original `eq.a` factory. This measures the selected culling bodies and genuine
SR/W/DR construction; it does not qualify N/DK construction, terrain meshes,
the whole World frame or other matrix profiles.

`capture_reference.py` requires the pinned client JAR, SDK metadata and Windows
native archive. It verifies method and input hashes, denies network and writes
inside the observer, permits only the pinned LWJGL native link, and preserves
process receipts. The JAR, classes and resources remain private. The public
fixture contains numeric observations only.

```text
python -B tests/reference/client/frustum_culling/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-culling-capture
```

The native test uses the actual SR/W singleton, source N/DK owners, live N array,
real class-owned bounds and canonical Display matrix ledger. Its separate
controls check retained owner admission, null pass/frustum failures, unchanged
loop counters on failure, and namespace cleanup. `run_native.py` requires native
completion and exact equality for all eight original rows.
