# Nested Stair side-texture getters

This packet observes the unchanged original `SS.a(I)I` over the actual Wood
Stairs (53), Cobblestone Stairs (67) and Wood (5) singletons. Two fresh guarded
ordinary VMs and a third fresh VM under read-only JDI agree on 30 calls each.
No GL context or enclosing CV inventory draw runs.

The declared source chain is `53 -> 67 -> Wood5`. All six face indices, -1, 6,
INT_MIN and INT_MAX return the actual Wood texture, 4. Changing that same Wood
receiver's `bm` field to 83 makes all ten calls return 83. Setting the inner
Stair's actual `a` field to NULL then produces ten original NPEs. JDI records
both source frames at `SS.a(I)I:5`, with receiver 67 inside receiver 53. The
actual source aliases and bounds remain unchanged through every getter call.

Reflection establishes that `SS.a` is a private, non-final UU field; assignment
and alias checks run on the original objects. The complete original Registry
contains 96 Blocks. No RAW allocation, game subclass, replacement game body,
network attempt, native link, target file write, debugger target write or
debugger method invocation supplies a reached result. Cyclic source chains are
not executed. Native bounded frame admission is a separate safety check, not
an original StackOverflow comparison.

`observations.json` contains the selected inputs, 30 outcomes, 80 getter entries,
ten exact nested exception records and both live Code fingerprints. The live
9-byte SS getter and 5-byte UU getter match the supplied archive. `source-pins.json`
also binds their complete original class fingerprints. Public data is typed LF
JSON. Raw CRLF stdout remains private, with byte hashes in `provenance.json`.
No original class bytes, assets, JDK files or private absolute paths are included.

The Java/JDI helper bytes match the preceding private proof. The public driver's
repository paths and packet audit were qualified with a fresh three-VM cohort,
with all seven producer/control inputs and compiled helpers unchanged. The
preceding private captures remain retained. The recorded profile uses Windows
x64 and JDK 17.0.17.10; this does not qualify another JVM or runtime platform.

Run the Java-free packet audit and corruption controls with:

```text
python -B tests/reference/client/inventory_geometry/side_chains/test_packet.py
```

All 16 controls pass in 0.039 seconds on the recorded Windows host. They check
actual source order and aliases, late textures, signed sides, both NULL frames,
Code identities, typed schema, external-effect guards and current file hashes.
That elapsed time measures the controls, not game performance.

To repeat the original observation with locally supplied prerequisites:

```text
python -B tests/reference/client/inventory_geometry/side_chains/capture_reference.py --output .local/nested-stair-original
```

The driver writes to a fresh output directory. It does not publish its results
or replace the checked-in packet automatically. These direct getter controls do
not establish additional CV geometry, pixels, whole-client play or FPS parity.
