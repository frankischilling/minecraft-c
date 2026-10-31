# Mouse filter observations

The fixture contains 96 rows from unchanged original `cu.a(FF)F` in the pinned
Beta 1.7.3 client. Each of eight histories constructs a genuine CU, authors its
actual private a/b/c float fields, and makes twelve sequential calls. The
observer records the returned float and all three fields after every call.

The histories include finite accumulated input, opposite signs, zero/negative
factors, subnormals, infinities, signed zero and selected signaling/quiet NaN
payloads. They check the source comparisons as well as separately rounded
arithmetic. The native implementation preserves the observed right-operand
NaN choice for additions when both operands are NaNs. It does not replace
selected results with a canonical NaN or relax the fixture comparison.

The 87-byte CU body has SHA256
`b0a669faa1169e9142475121f0702291e8e9282f2fa69328948c6782ae1e2bb4`.
[Source pins](source-pins.json) also record its constructor and original class.
These are finite observations from JDK17 interpreter execution, not proof of
every floating-point input or another original runtime's NaN-payload choices.

To recapture with JDK17:

```text
python -B tests/reference/client/mouse_filter/capture_reference.py \
  --client-jar /private/b1.7.3.jar --output /private/new-mouse-filter-capture
```

The output directory must be fresh. The runner verifies the original JAR,
method identities, runtime and helper inputs, then records bounded process
receipts. The observer denies network/listeners, file writes and native links.
No SDK metadata libraries, assets or downloads are needed.

The public `client.mouse_filter` CTest requires successful native completion
and exact comparison with all [96 rows](finite.json). Native controls also
check argument refusal and unsupported floating controls before any field or
output mutation. The runtime uses the canonical Player/PX mouse-filter field
type; it allocates no second filter owner or copied settings state.

This fixture does not execute SDL input, the renderer focus prefix, Player
movement, a complete World frame or extended play. The separate
[focus observations](../renderer_focus/README.md) cover composed turning.
