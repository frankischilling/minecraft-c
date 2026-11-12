# Source region output

The client SaveHandler uses its shared region cache for both reads and writes.
Each output stream holds the actual region owner after cache removal. A cache
flush can close that owner's file while an older stream remains live; closing
the older stream then reaches the original closed-file failure path.

## Stream and commit order

`rj.d` obtains the cached region and constructs the 8,096-byte Chunk buffer,
default zlib deflater with a 512-byte output buffer, and DataOutputStream.
Plain named NBT passes through these owners. Explicit outer close finishes
compression, then closes the Chunk buffer and commits its bytes. Native
retirement releases allocations and descriptors without finishing or committing
an abandoned stream.

The sector requirement is `(compressed_length + 5) / 4096 + 1`. Requirements
of 256 or more sectors return without a region write. A same-size allocation
rewrites its payload. Otherwise the region frees the old run and searches for
a suitable free run before appending sectors. Reuse publishes the offset
before occupying the selected sectors and writing the payload. Append writes
zero sectors, updates the free list and growth, writes the payload, then
publishes the offset. Each successful branch samples its real timestamp at
the reached commit call. The later `rj.b` lookup consumes and resets growth;
stream close does not perform World Info accounting.

The implementation retains the region's offset and timestamp arrays, free
sectors, file pointer, growth and stream count. Checked C allocation ceilings
are native policies. They do not establish Java heap-allocation failure timing.
The buffer constructor stores local coordinates only after its buffer exists.

## Failure ownership

An output owns its region, namespace, byte buffer, deflater and DataOutputStream.
The region holds its actual File and RandomAccessFile. Native cleanup releases
these owners in order and retains consumed-descriptor failures for retry.
Retry does not close an already consumed descriptor or commit pending bytes.
Native close failures preserve their lower file cause without claiming a
source exception or catch.

The source catch at `qj.a:441` still depends on exception printing. The original
prints and returns in the captured closed-file case; native execution retains
that diagnostic frontier. Only their common failed prefix is compared.
Per-operation write failures, exact sector boundaries and the 255/256-sector
transition retain separate qualification requirements.

## Evidence and reproduction

The public `storage.source_region_output` test covers six normal writes,
negative coordinates, growth reset, abandoned output, cache flush with a live
older stream, shared ownership, native consumed-close retry and constructor
limits. An independent decoder checks real region files, zlib/Adler framing
and every typed payload byte. Windows and sanitized Linux pass all four
profiles. The constructor-limit regression first failed on the early
coordinate stores, then passed after their ordering was corrected.

The unchanged original runs use ordinary and read-only JDI processes on each
host. Each profile records nine output chains, six normal commits, one caught
closed-file commit, 45 snapshots per process, 1,252 events and 33 Code pins.
The comparator matches six normal cases and six common abandon/cache-flush
snapshots per host. Real timestamps are checked against each process's own
interval; compression byte identity across zlib implementations is not required.

Build `mc_test_source_region_output`, then run:

```text
ctest --test-dir <build> -C Debug -R '^storage.source_region_output$' --output-on-failure
```

The [original capture](../../tests/reference/world/source_region_output/README.md)
and [comparison recipe](../../tests/reference/world/source_region_output/comparison-README.md)
record tool versions, class hashes, observation boundaries and commands.
Original-dependent tests are off by default and require the supplied verified
client archive and a full JDK17. Lower stream evidence does not establish
whole-World or gameplay parity. The [Chunk save contract](saving.md) describes
caller integration and its separate native save/reload tests.
