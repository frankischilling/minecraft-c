# Original compression evidence and local comparison

The memory gzip/zlib adapters and original-class comparison are tracked in
[issue #11](https://github.com/frankischilling/minecraft-c/issues/11). These checks
cover compressed containers and separate raw NBT compound parsing. They do not
establish region storage, complete world saves, or historical JVM behavior.

## Original classes

| Side | Local reference | SHA-256 |
| --- | --- | --- |
| Client | `b1.7.3.jar` | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` |
| Server | `b1.7.3_server.jar` | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` |

Python verifies both hashes before launching the harness; Java verifies the
selected side again before loading it. Each side runs in its own JVM with a
platform-parent class loader. The harness reflects untouched original methods;
it does not compile decompiled implementations or initialize a game or server.

These Temurin 17 `javap` commands establish the method-relative offsets below:

```text
javap -classpath b1.7.3.jar -c -s -p as qj
javap -classpath b1.7.3_server.jar -c -s -p ag jx
```

| Interface | Evidence checked on both sides |
| --- | --- |
| Client `as.a(InputStream):nu`; server `ag.a(InputStream):iq` | GZIPInputStream allocation 4, constructor call 9; DataInputStream constructor 12; raw compound read 17; close 22 on success, 29 on a body exception. Constructor failures precede the protected range 16..21. |
| Client `as.a(nu,OutputStream):void`; server `ag.a(iq,OutputStream):void` | GZIPOutputStream allocation 4, constructor 9; DataOutputStream constructor 12; raw writer call 18; close 22/30. |
| `as/ag.a(DataInput):compound` | Generic named-tag read at 1, compound check 6, IOException path 17..26. The empty root name is accepted. |
| `as/ag.a(compound,DataOutput):void` | Generic named-tag write at 2; return 5. This overload performs no compression or close. |
| `qj/jx.a(int,int):DataInputStream` | Selector read 152; selector 1 branch uses GZIPInputStream allocation 185 / constructor 198; selector 2 uses InflaterInputStream allocation 237 / constructor 250. Unknown selectors return null at 289..290. |
| `qj/jx.b(int,int):DataOutputStream` | DeflaterOutputStream allocation 15 / constructor 29. Private sector commit `a(int,byte[],int):void`, descriptor `(I[BI)V`, writes selector 2 at 50..51. |

The allocation/constructor pairs above are different instructions, rather than
client/server differences. Region codecs were inspected but not executed on
region files. Default Java inflater/deflater streams use zlib framing, separate
from the gzip compound wrappers. Region addressing and save replacement remain
outside these checks.

## Root completion and container validation

The original gzip wrapper returns after one compound root and closes its wrapper.
It does not drain decompressed input. On the tested JDKs, a complete root can
therefore be returned before CRC, ISIZE, or missing trailer bytes are checked.
The generated 24-byte gzip for raw `0a000000` still returns that root when cut
to 15 bytes; a full JDK gzip drain rejects that prefix.

The C memory adapter validates every recognized member through its trailer.
Its stricter malformed-input behavior is recorded separately:

| Fixture group | Original/JDK observation | C result |
| --- | --- | --- |
| Corrupt or missing unread trailer | Original root read succeeds; full JDK drain rejects | Container error |
| Root split across members, corrupt first trailer | Original read needs to cross the first trailer and fails | Container error |
| Reserved gzip flag bit | Selected JDK readers accept it | Invalid stream |
| Recognized next-member magic with a failed/truncated header | Selected JDK drain ignores the failed next header after a complete first member | Container error |

These are named departures, not matches counted as compatibility passes. Valid
concatenated members, empty members, and a root split across members produce the
expected exact raw bytes. A raw compound parse can then select the first root.
Bytes after that root remain distinct from nonmagic bytes after a compressed
member. Zlib decoding stops at its first stream.

Stream spies record close calls and bytes delivered. Successful original gzip
reads/writes close the supplied stream; raw overloads do not. Gzip constructor
errors occur before the original close path. Delivered compressed bytes depend
on source chunking and read-ahead: they are not the C adapter's exact member-end
consumed count. Java gzip control documentation:
[input](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/util/zip/GZIPInputStream.html),
[output](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/util/zip/GZIPOutputStream.html).

## Fixtures and comparisons

The generated suite contains 80 containers: 61 gzip and 19 zlib. It includes
FEXTRA, FNAME, FCOMMENT, FHCRC, combined optional fields, malformed header fields,
checksums, truncation, concatenation, compressed tails, decompressed tails, and
separate wrong/incomplete NBT roots. Every compound has at most one child at each
level, so all canonical NBT bytes compare exactly without an ordering exception.

Each side schedules 181 baseline records: 65 original gzip reads, 8 original gzip
writes, 13 original raw-root reads, 65 full JDK gzip drains, 19 JDK zlib drains,
and 11 JDK zlib writes. Original writers feed 35 C decode/root checks. C encoders
feed 38 original/JDK checks per side. Another 22 C encodes and 38 C re-reads cover
all generated raw payloads. The manifest reports actual executed counts and
each explicit departure; scheduled counts alone do not establish a passing run.

Initial runs on 2026-10-17 passed with native Windows MSVC and Temurin 17.0.17 /
Oracle 21.0.10, and Linux Clang with Ubuntu OpenJDK 21.0.12.1. Each run retained
728 records and 36 explicit departure records (18 fixture departures per side).
Exact runtime/build fingerprints remain in each local manifest.

The comparison requires exact payloads and raw canonical NBT bytes. It retains
all compressed bytes but does not require identical deflate blocks from different
compressors. C gzip headers must have MTIME 0 and OS 255. No large allocations,
deep nesting, existing worlds, network listeners, or crash thresholds are probed.

## Run locally

Use Python 3.10+ and a JDK with `java` and `javac` 11+. The harness always compiles
with `--release 11`. Enable the optional CTest with local reference paths:

```text
cmake -S . -B build-reference -DBUILD_TESTING=ON -DMC_ENABLE_COMPRESSION_REFERENCE_TESTS=ON -DMC_REFERENCE_CLIENT_JAR=<absolute-client.jar> -DMC_REFERENCE_SERVER_JAR=<absolute-server.jar>
cmake --build build-reference
ctest --test-dir build-reference -R reference.compression --output-on-failure
```

For a directly selected runtime/build:

```text
python scripts/reference/compression/compare_compression.py --executable <mc_compression_probe> --client <client.jar> --server <server.jar> --java <java> --javac <javac> --work-dir .local/reference/compression --tested-commit <40-hex-commit>
```

The work directory must be separate from all supplied inputs, tools, and source
files. Missing/mismatched JARs, missing tools, or a selected JDK below 11 return
`UNAVAILABLE` with exit 77. Actual comparison, compilation, or execution failures
return 1. Version commands that fail or cannot be parsed are execution failures.
A fresh running/unavailable/failure manifest replaces an earlier result; unsafe
output directories are refused before any write.

The ignored manifest records runtime properties/fingerprints, original/source/C
binary hashes, caller-supplied commit or explicit unrecorded state, fixture and
executed counts, raw records, request/output hashes, and departures. Requests are
bounded to 256 KiB binary input each, 1024 per process, and 4 MiB per batch;
decompressed reference output is capped at 256 KiB and subprocesses at 120 seconds.
The Java and C record formats intentionally distinguish exact C consumption from
Java stream read-ahead. Public C/Python container checks do not need either JAR.

## Native dependency

The upstream release verified on 2026-10-17 is zlib 1.3.2, released February 17,
2026. Its official tar.gz SHA-256 is
`bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`.
[Official release](https://zlib.net/)

Upstream provides a C interface and CMake static/shared targets; preserve its
license with distributed source. The private dependency build used during
research passed all 16 upstream Linux tests; project checks remain separate.
[Pinned interface](https://raw.githubusercontent.com/madler/zlib/v1.3.2/zlib.h),
[CMake support](https://raw.githubusercontent.com/madler/zlib/v1.3.2/CMakeLists.txt),
[license](https://zlib.net/zlib_license.html).
The [zlib manual](https://zlib.net/manual.html) describes explicit gzip/zlib
wrapping, trailer checks, and the reset needed between concatenated gzip members.
