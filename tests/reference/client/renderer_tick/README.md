# Renderer tick observations

The fixture contains 43 observations from unchanged original `px.a()V` in the
pinned Beta 1.7.3 client. It is the renderer's tick body, distinct from picking
`px.a(F)V` and frame drawing. [Source pins](source-pins.json) record its
187-byte body, SHA256
`2ae449775d9ac71e3f1f5b1976e806ed0e5d258d5a658306a6dd4249fffee48e`.

The observer uses Object-only allocation for genuine original PX, App/kq,
World, LocalPlayer, RA and Settings receivers. Their source constructors do
not run. It constructs a genuine RH Dimension and InventoryPlayer, authors
the actual receiver fields and invokes the unchanged PX body. There is no
subclass replacement or overridden source method. These preparations qualify
the selected bodies, not App construction or a playable client.

Camera X=32000000 takes the original brightness query's outside-border path
and reads the actual Dimension float[16] entry15, authored as 0.3f. World rain
fields are zero, so the real rain body reads Settings and returns35. The
held-item body runs on an empty real Inventory and returns212. No Random,
device, particle or sound backend is substituted for either body.

Thirty-five rows combine seven signed render distances with five initial M
values. They preserve the binary32 blend and update operations, signed integer
wrap, six interpolation copies, App.i publication, and RA c/d/g fields. The
render distances include both integer limits; copied interpolation fields
include signaling/quiet NaN bits, negative zero, a subnormal and infinity.
The arithmetic M inputs are finite. This is not general NaN-arithmetic parity.

Eight rows observe null App, World, Player/view, Settings, RA, RA.App,
RA.Player and Inventory prefixes. The complete field rows capture which
interpolation, M, counter, view and RA stores survive each source exception.
They do not normalize failed calls into successful ticks.

To recapture with JDK17 and the four pinned SDK metadata JARs:

```text
python -B tests/reference/client/renderer_tick/capture_reference.py \
  --client-jar /private/b1.7.3.jar --library-root /private/launcher/libraries \
  --output /private/new-renderer-tick-capture
```

The output directory must be fresh. The runner verifies selected original
method identities, SDK hashes, JDK version and observer inputs, then records
bounded process receipts. The observer denies network/listeners, file writes
and native links. Original executable classes and assets are not distributed.

The public `client.renderer_tick` CTest needs no JAR or JVM. It requires
successful native completion and compares all [43 rows](finite.json).
The native test uses canonical allocated PX/RA/World/Player/Inventory owners
with authored fields matching the original preparations. It also checks
occupied output rejection, owner retention while a call is pending, child
retirement, World-call cleanup and the independent App.i occurrence.

These finite scenarios do not qualify loaded-Chunk brightness, nonzero rain,
full World ticking, Player movement, focused rendering or extended play.
