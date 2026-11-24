# Actual ChunkCache

The implementation executes `ew(World,IIIIII)` and every declared cache
accessor. Constructor acquisition is inclusive, X-major/Z-minor and uses the
actual World getter. Each published cell retains the same canonical Chunk
identity with a Store lease; the cache also retains its supplying World.
Accessors select those captured identities and their live arrays, never a
fresh World Chunk lookup. A reached lazy TileEntity factory retains its own
actual World calls. The original Y and rectangular bounds differ by method.

The source fixture constructs a genuine World over four authored empty saved
Chunks, then invokes the unchanged constructor and accessors. It observes
950 rows: null/empty/negative/inclusive constructors, every registered Block
ID0..96 through nine accessors, Y and asymmetric border rules, array failures,
live sky-subtraction wrapping, canonical manager identity and an actual lazy
Chest factory/Map lookup. Input block/light bytes and World.f are explicitly
authored. The real World provider is set NULL only after cache construction
during ordinary reads, making unintended accessor reacquisition observable.
No original method or Chunk/TileEntity return is replaced.

Original capture is bounded and pins the original JAR, selected Code bodies,
JDK17 and four SDK metadata JARs. Network/native links are denied; writes are
confined to the disposable authored World. Its files must remain unchanged
except session.lock. No original classes or assets are distributed.

The native fixture uses actual World/Registry/Chunk construction, preserves
the same captured identities, checks the independent leases and balanced World
hold, and compares all950 rows exactly. Failed constructor owners retain their
source prefix for explicit native retirement. Native rectangular allocation
has an explicit1048576-cell limit, separate from original Java OOM timing.
This component does not establish RenderBlocks geometry or a terrain frame.

```text
python -B tests/reference/client/chunk_cache/capture_reference.py \
  --client-jar /private/b1.7.3.jar --library-root /private/libraries \
  --output /private/new-ew-original
python -B tests/reference/client/chunk_cache/run_native.py \
  --executable /private/mc_test_chunk_cache --work-dir /private/new-ew-native
```
