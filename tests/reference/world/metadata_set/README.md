# Quiet metadata setter observations

This optional collector executes unchanged Beta 1.7.3 client metadata setters.
The original archive has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The runner pins the whole archive and the reached method Code attributes before
execution; the helper checks that game classes originate in that archive.

The actual route is `fd.e(IIII)Z`, through the actual cached Chunk lookup and
`lm.b(IIII)V`, to `wi.a(IIII)V`. The World method stores true only after the
Chunk call returns. The normal Chunk stores its dirty flag at bytecode 2 before
reading its current `e` NibbleArray at 6 and invoking it at 14. NibbleArray's
even branch first reads its selected backing byte at 42 and stores at 54; the
odd branch reads at 70 and stores at 83. Genuine Blank Chunk `li.b(IIII)V` is
the literal return at 0.

The helper constructs a real World, its real `ok` provider and nine real cached
empty Chunks. No replacement World subclass, getter or source method supplies a
result. The replacement NibbleArray, null fields and short arrays are explicit
fixture operands. The original fields are restored only between independent
declared calls. Each source method executes once, without retry or instrumentation.

Fifteen profiles run in order:

- Replacement backing of length 8,737, selected byte 8,736 initially `0xa3`:
  even metadata 18 produces `0xa2`, then the same object's odd metadata -1
  produces `0xf2`. The original fixed-plane array remains unchanged.
- Null NibbleArray, null byte backing and zero-length backing: each runs both
  even and odd coordinates. The dirty store survives each actual exception;
  the World Boolean never returns.
- A genuine Blank Chunk with null `e`, followed by out-of-domain Y 128/Y -1,
  X 32,000,000 and Z 32,000,001 calls with null `e`. Blank returns true without
  a dirty store; out-of-domain calls return false without reaching the Chunk.
- Two ordinary full-plane calls repeat the even-18/odd-minus-1 sequence and
  change the actual fixed plane's selected byte.

The ordinary coordinate is `(8,64,8)`; odd writes use Y 65. Rows use the
`METADATA_SET` prefix and retain actual void-failure/Boolean outcome, exception
class, dirty prefix, nullable backing identity state/length, selected bytes,
saved fixed-plane bytes and unchanged-state guards. Whole-array comparisons
verify that all other bytes remain unchanged. Separate failure records retain
the actual Java message and game stack frames; no bytecode PC is invented from
an exception message or native diagnostic.

```powershell
python -B tests/reference/world/metadata_set/capture_reference.py `
  --jar b1.7.3.jar --work-dir .local/metadata-set-original
```

`--native-stdout PATH` compares all fifteen typed native rows exactly and requires
the native fixture's completion line. Its default pattern is
`PASS: [positive count] Metadata set...checks`; `--native-pass REGEX` accepts a
different existing fixture signature. Complete row inventory, scalar types,
ordered profiles and negative comparator controls remain mandatory.

Network, listeners and native library loads are denied. Writes outside the new
private root and all deletes are denied and counted. Inputs and compiled helper
products are fingerprinted before and after execution. Original archives,
classes and raw observations remain outside the repository. Constructor entropy
is independent; each call separately verifies that the original World's Random
state and provider membership are unchanged. This finite setter scope establishes
no full World mutation, Bed wake, Arrow, rendering or performance result.
