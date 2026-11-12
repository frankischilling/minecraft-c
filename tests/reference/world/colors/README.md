# Original World color observations

`finite.json` contains 432 rows from the unchanged Beta 1.7.3 client JAR:
`fd.a(Lsn;F)Lbt;` (sky), `fd.c(F)Lbt;` (cloud), and `fd.d(F)Lbt;`
(Dimension fog). Each row records the three returned Vec3 double bit patterns
and the actual shared pool cursor. The JAR, selected method Code attributes,
whole declaring classes, helper inputs, and SDK metadata JARs are pinned by
`source-pins.json` and the capture receipt.

The matrix covers actual Overworld, Nether, and Sky Dimensions; six World times
including negative values and `Long.MAX_VALUE`; two partial ticks; four camera
positions; four rain/thunder histories; lightning; and four cloud-color fields.
Dimension construction and binding use the original methods, including their
real WorldChunkManagers, noise, Biomes, raw temperature, virtual sky-color
dispatch, and pooled Vec3 allocation. The observer asserts that resetting the
pool reuses the previous sky vector even while the observer retains its Java
reference.

World and camera instances use Object-only allocation. Their reached fields are
prepared directly; this fixture does not execute their constructors, Chunk
queries, weather ticks, or frame drawing. AWT Color class bootstrap runs before
the deny guard because JDK initialization can load host native libraries. All
measured source calls then run under the guard with zero network and native
link attempts. No game method body, class, biome, camera, or color service is
replaced.

Capture with JDK 17 and the existing pinned SDK metadata JARs:

```powershell
python -B tests/reference/world/colors/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/world-colors-original-fresh
```

`run_native.py` requires successful native completion and exact equality of all
432 rows. The native harness prepares the corresponding canonical owners and
also checks source-null ordering, unsupported floating-point state, untouched
pool state on these failures, released call guards, and camera/vector holds.
These native failure controls are separate from the captured numeric rows.
