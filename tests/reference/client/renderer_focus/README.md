# Renderer focus-prefix observations

The fixture contains 72 rows from unchanged original `px.b(F)V` through its
focused-input prefix. The observer creates a genuine LWJGL2 Display and Mouse,
then uses Object-only allocation for original PX, App/kq, Settings, LocalPlayer
and MouseHelper receivers. It constructs two real CU filters. These receiver
preparations do not qualify their constructors.

The actual Mouse dx/dy static operands are authored after Display creation.
Unchanged Mouse.getDX/getDY consume them through the real `vy.c()V`. The PX
method reads sensitivity/invert/smooth settings, updates its own n/o filters
and turns App.h through inherited `sn.d(FF)V`. App.w=true makes the original
method return at204 before frame drawing. H=Long.MAX_VALUE prevents the
inactive-window pause branch; actual Display.isActive still executes.

[Inputs](inputs.json) define twelve histories combining three sensitivities,
inversion and smooth camera. Each history resets Player angles and the two
filters, then runs six sequential delta pairs. Rows record four Player angle
fields, all six filter fields, MouseHelper deltas and consumed Mouse dx/dy.
This covers pitch limiting, previous-angle updates and filter history through
the actual composed original bodies.

To recapture on native Windows with JDK17 and the pinned launcher libraries:

```text
python -B tests/reference/client/renderer_focus/capture_reference.py \
  --client-jar /private/b1.7.3.jar --library-root /private/launcher/libraries \
  --output /private/new-renderer-focus-capture
```

The output directory must be fresh. The runner pins the original JAR and
selected PX/CU/Entity/MouseHelper method identities, four SDK metadata JARs
and the cached LWJGL2.9.0 Windows native archive. It extracts only lwjgl64.dll
into the private output and requires that exact native link. Network,
listeners and Java file writes are denied during observation. The finite
[numeric fixture](finite.json) redistributes no original executable classes,
native libraries or assets.

The Windows World-selection test compares the same rows after source loading,
using genuine C App/PX/MouseHelper/Player owners and authored SDL motion. Its
direct segment calls cover38..194; the original tail194..204 only reads App.w
and returns. The surrounding C test separately checks unfocused short-circuit
behavior, null-child prefixes, consumed deltas, retained calls and floating
admission. Inspect the test run before claiming those checks passed.

The observations qualify selected focus arithmetic and composition. They do
not establish OS mouse mapping equivalence, original App/World construction,
unrestricted sensitivity values, full frame drawing or playable sessions.
