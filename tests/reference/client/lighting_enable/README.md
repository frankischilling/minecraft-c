# Standard lighting state

This capture invokes unchanged `u.b()V` from the supplied client archive. It
observes state and parameters rather than drawing geometry. A Windows JDK 17
capture completed all five profiles and 86 rows on an NVIDIA GeForce RTX 3060,
driver 581.42, with unchanged input and helper hashes. The promoted `finite.json`
has SHA-256 `6ffa6fb8004e06b1009e044efbbdb37c8770936f94d3d237e48a9744537d3fea`.
Native Windows MSVC Debug replay matches all 86 rows exactly. The three focused
lighting CTests pass in 2.05 seconds, with zero failures or skips. Hosted source
0c0cc5a passes all four jobs and 1,520 tests; both Linux sanitizer profiles also
match all 86 rows. These are state and parameter results; lit drawing has its
separate [raster controls](../lighting_raster/README.md).

Five fresh JVM profiles cover:

- `cold_no_context`: two calls fail at the first GL child. The real U class
  initialization still publishes its direct FloatBuffer, and the second call
  keeps the same untouched object and cursors. The helper does not inspect or
  initialize the BT pool in this profile.
- `identity_cold`: the BT pool is explicitly initialized and cleared. U and Ge
  initialize through the first whole lighting call. MathHelper is not eagerly
  initialized. Both source lights use identity modelview and projection.
- `seeded_projection`: U's actual 16-float buffer starts with authored values,
  position 5 and limit 11. Two earlier pooled vectors remain present. Modelview
  has scale `(2, 3, 0.5)` and translation `(7, -9, 11)`; a different projection
  with scale `(3, 5, 7)` remains selected during the call.
- `seeded_replaced_modelview`: the same nonidentity upload occurs with projection
  selected. Modelview is then replaced with identity before inspection. The
  queried light position therefore observes the matrix at upload time.
- `seeded_warm`: two complete calls reuse the actual seeded U buffer and BT List
  without a pool reset between calls.

Each normal call emits 17 rows: enable/material/shade/matrix-mode state, the
complete buffer and cursors, pool cursors and identities, four consumed vectors,
eight light parameter vectors, global ambient, and before/after matrices. Object
identity is compared within the process; no address or identity hash is emitted.
All 16 buffer words are observed through a duplicate view without changing its
actual cursors. The helper checks that words 4 through 15 retain their authored
suffix, that each whole call consumes four pooled vectors, and that earlier pool
objects and their coordinates remain unchanged. The no-context profile emits
one comparable failure row and records its actual exception types separately.
Five normal calls and one failure profile produce 86 comparable rows.

Light position, diffuse, ambient and specular come from actual `glGetLight`.
Global ambient and matrices come from actual GL queries. Position is the stored
eye-space value, not the source vector before its modelview transform. The
capture records raw binary32 parameter bits and binary64 BT coordinates. It
does not normalize driver results or choose a tolerance. Any later native
comparison must investigate mismatches before defining a supported scope.

Use JDK 17 and the pinned local SDK/native archives:

```powershell
python -B tests/reference/client/lighting_enable/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/lighting-enable-original
```

The driver checks archive, complete selected classes, method Code, SDK/native,
tool, helper and input identities. Each profile runs a fresh process. The guard
permits only the pinned `lwjgl64.dll`, rejects writes, connections and listeners,
and requires zero network attempts. Device profiles destroy their context
before reporting completion. Before/after hashes and bounded process receipts
are retained in `capture.json`; a successful run writes `finite.json` alongside
it. Game classes are not replaced or transformed.

Replay the native emitter with `run_native.py --executable ... --work-dir ...`. The
runner requires all 86 rows exactly, complete process capture, empty native
stderr and one normal fixture completion after cleanup for each profile. Its
`run.json` preserves per-profile mismatches and before/after input identities.

This scope excludes lit pixels, normal-array decoding, flat/smooth rasterization,
material inheritance outside U's writes, Entities, particles and complete World
frames. It does not establish that a later lit geometry draw succeeds.
