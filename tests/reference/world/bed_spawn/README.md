# Original Bed spawn observations

This packet retains 30 prepared calls to the unchanged client `ve.f(fd,IIII)`
Bed exit helper and `gs.a(fd,br)` saved-Bed wrapper. The supplied client archive
has SHA256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.

Two ordinary JDK17 interpreter VMs agree on every selected result, input plane,
Random state and identity relation. A separate read-only JDI VM agrees on those
same observations and supplies 272 ordered caller-level World/provider queries.
Seven actually reached method bodies match their pinned original Code bytes.
All twelve selected classes verify their archive origin and complete class hash.
The observer constructs the real World, its normal Map ChunkProvider and sixteen
normal Chunks. It does not subclass, replace or rewrite an original game class.

The fixture prepares the normal Chunks at X/Z -2 through 1 as air, then stores
the explicit cells in `profiles.tsv`. Each line contains `case kind x y z metadata
skip cell_count`, followed by five integers per cell: X, Y, Z, Block ID, metadata.
Kind 0 invokes the Bed helper; kind 1 invokes the Player wrapper. The separate
`profiles.json` supplies names and the exact same ordered integer transport.
Each profile sets the genuine World Random to seed 2468 before entering the
target. Full selected Block/metadata planes and all three Random fields remain
unchanged by every observed call. Constructor state outside those selected
planes is not compared or reused as a native answer.

The cases cover all sixteen orientation/occupied/head-bit combinations; first,
last and second-pass-only exits; no exit; positive, zero and negative skip
counts; overlapping candidates; the wrapper's Bed/non-Bed gate; positive and
negative Chunk boundaries; and Y=0/127. The wrapper records all four calls to one
captured provider before the late Block query. The helper records metadata,
solid-support and both air tests in their actual short-circuit order.

`finite.json` retains the ordinary observations, JDI journals and live Code
hashes. `provenance.json` binds the producer inputs, JDK tools, helper classes,
bounded process receipts and private JDI outputs. The raw VM pipes, class-origin
receipts, helper classes, supplied archives and World outputs remain under the
ignored capture directory. The original target attempts no network connection,
native library link or rejected filesystem write. JDI uses its ordinary local
debugger transport; it performs no remote invocation or target mutation.

Reproduce with a local JDK17 and supplied SDK libraries:

```powershell
python -B tests/reference/world/bed_spawn/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --java 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/java.exe' `
  --javac 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/javac.exe' `
  --output .local/bed-spawn-original/fresh
```

The capture writes a new ignored output directory. It does not overwrite the
published packet. Review its finite/provenance files before selecting a successor.
The Java-free native reader accepts `--executable` and `--work-dir`. Its command
is `EXE bed-spawn WORLD_ROOT_HEX RESOURCE_ROOT_HEX PROFILES_PATH`; it compares 30
result/null outcomes and every declared query field, totaling 2,014 scalar values.
`test_packet.py` rejects altered values, ordering, identities, input planes,
producer bindings and failed/truncated process receipts in normal and optimized
Python modes. Those reader controls do not establish native execution.

This packet does not qualify bed placement, use, occupancy changes, sleep/wake,
loaded respawn, complete World/Chunk construction, unsupported failure branches,
other dimensions, graphical output, audio or performance. Native callback
refusals and source exceptions need their own explicitly scoped comparisons.
