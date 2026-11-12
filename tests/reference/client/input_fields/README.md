# Client input field operations

The fixture contains 63 numeric observations from unchanged original
`ix.b(I)V` in the pinned Beta 1.7.3 client. Each case constructs an original
InventoryPlayer with a null Player, authors its selected slot and calls the
original wheel body. The observer checks that both slot arrays, all empty
Stack entries, null Player, cursor and changed flag remain unchanged. It
denies file writes, network access, listeners and native-library links.

The selected inputs are -28, -9, -1, 0, 1, 7, 8, 9 and 28. Wheel inputs include
both signed integer limits, -120, -1, zero, 1 and 120. The zero-wheel cases
exercise the original method's normalization of out-of-range selected fields;
the Minecraft caller normally bypasses that method for zero wheel events.
These finite cases do not establish every possible selected-field value.

The 67-byte original wheel body has SHA256
`cc1338d65bf8f56b0a07706479bd005c953442af274daa23fc4bc86cbdf3f277`.
[Source pins](source-pins.json) record its original class, constructor and the
enclosing `Minecraft.k()V` body. No original classes or resources are included.

To recapture with an installed JDK17:

```text
python -B tests/reference/client/input_fields/capture_reference.py \
  --client-jar /private/b1.7.3.jar --output /private/new-input-field-capture
```

The runner requires a fresh output directory, verifies pinned original inputs
and writes bounded process receipts. It checks runtime, observer and input
hashes before and after execution. No SDK libraries, downloads or native
binaries are needed for this particular original body.

The public `client.input_fields` CTest runs the native executable, requires its
successful completion and compares every selection row with the captured
[fixture](finite.json). Native controls also check hotbar assignment bounds,
busy/null/server admission, untouched inventory fields, four Settings toggles,
wheel float bits, disabled-D and zero-wheel paths under unsupported floating
controls, rejection before a reached arithmetic store, and RAW Settings
admission without output mutation.

The Settings operations reproduce literal field stores in `Minecraft.k`:
hide GUI at668, debug at698, third person at728, smooth camera at758 and
hotbar selection at874. The wheel branch reads D at386 and stores G at418.
These locations were inspected with `javap -c -p`; they are not new Java
methods on Settings. The native Settings checks are authored arithmetic and
ownership contracts, separate from the original Inventory observations. This
fixture does not execute a full `Minecraft.k`, reproduce keyboard dispatch,
establish live gameplay or qualify a playable client.
