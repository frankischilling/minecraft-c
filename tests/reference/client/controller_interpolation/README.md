# Controller render interpolation

The fixture contains 72 numeric observations from unchanged original
`os.a(F)V` in the pinned Beta 1.7.3 client. Each case constructs a genuine OS
controller and prepares actual App, GuiIngame and RenderGlobal receivers
without invoking those child constructors. Four operand modes cover null App,
null GUI, null RenderGlobal and a healthy field-store path. The latter null
case verifies that the earlier GUI store survives the later failure.

Eighteen float triples cover signed zeros, negative values, subnormals, ordinary
interpolation, partial values outside0..1, infinities, quiet NaNs and a signaling
NaN. Rows retain both target field bits and source completion. The original
fcmpg makes unordered NaNs take the arithmetic branch; it does not treat the
branch as a positive-finite test. This finite fixture does not establish every
NaN payload or full rendering behavior.

[Source pins](source-pins.json) record the original OS constructor and 74-byte
`a(F)V` body. The original JAR SHA256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
To recapture with an installed JDK17 and locally supplied pinned SDK metadata:

```text
python -B tests/reference/client/controller_interpolation/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-controller-interpolation-capture
```

The runner requires a fresh output directory and verifies original, runtime and
helper inputs before and after execution. Process output and duration are
bounded. The observer denies file writes, network access, listeners and native
library links; a successful capture records zero network/native attempts.

The public `client.controller_interpolation` replay requires native completion
and exact comparison of all 72 rows without a JAR or JVM. Native controls also
check entered-controller admission without changing output, literal-zero work
under unsupported floating controls, refusal before interpolation arithmetic
and the retained GUI store when the RenderGlobal owner refuses native access.
The production body writes the actual source-owned fields and performs no GL,
tick, draw or save operation. Child constructors, a complete render frame and
a playable client remain outside this fixture's scope.
