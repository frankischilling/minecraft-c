# Held-item equip tick

The fixture contains 120 numeric observations from unchanged original
`ra.a()V` in the pinned Beta 1.7.3 client JAR. The observer prepares actual
RA, App and LocalPlayer receivers without invoking their constructors. It
constructs genuine original InventoryPlayer and ItemStacks and calls the
real equip method. No original class, asset or source body is redistributed.

Ten cases distinguish null items, the same Stack identity, equivalent but
distinct Stacks, different IDs, different metadata, a changed selected slot,
out-of-range selected fields and equivalent Stacks with different counts.
Each case uses twelve equip fractions: signed zeros, the 0.1 threshold,
intermediate values, one, negative one, both infinities, two quiet NaNs and a
positive subnormal. Rows contain the previous/current fraction bits, selected
slot field and resulting Stack identity. These finite observations do not
establish every NaN payload or floating-point input.

[Source pins](source-pins.json) identify the original RA body, InventoryPlayer
current-Stack getter, Stack metadata getter and the two genuine constructors.
The JAR SHA256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
To recapture with an installed JDK17 and locally supplied pinned SDK metadata:

```text
python -B tests/reference/client/held_item_tick/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-held-item-capture
```

The runner requires a fresh directory, verifies all original/runtime/helper
inputs before and after execution and records bounded process receipts. The
observer denies file writes, network access, listeners and native-library
links. The successful original run records zero network/native attempts.

The public `client.held_item_tick` replay requires successful native completion
and compares all 120 rows exactly without a JAR or JVM. Authored controls check
null App/Player/Inventory prefixes, bit-preserving previous-fraction stores,
occupied output slots, independent retained failures, owner retirement guards,
unsupported floating control after a reached equivalent-Stack store, the
getter's out-of-range array bypass and null-array failure, and the independent
published Stack occurrence after Inventory/caller/call holds have ended.

RA/App/Player construction, Font/GL ownership, actual held-item drawing,
connected renderer ticking, full frames and playable sessions are outside
this prepared-body fixture's scope.
