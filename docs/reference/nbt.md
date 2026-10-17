# Original NBT and Java string evidence

Research recorded 2026-10-17. The C raw codec and original-class differential are
implemented. The observations and comparisons below cover raw tags and Java
string behavior, not complete world save compatibility.

## References and reproduction

| Side | Supplied file | SHA-256 |
| --- | --- | --- |
| C (client) | b1.7.3.jar | af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c |
| S (server) | b1.7.3_server.jar | 033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d |

Used installed Temurin javap 17.0.17. These commands exited 0. Output was inspected
in memory; this note records observations rather than copied original source or
complete disassembly. CFR 0.152 output was used only to locate classes.

```powershell
javap -classpath b1.7.3.jar -c -s -p ij lh qp ul pp mi p sz hn xb sp nu as ki qj
javap -classpath b1.7.3_server.jar -c -s -p fo hf kb mv jq hv k lx fb ou lr iq ag gt jx
```

Every offset below is a method-relative JVM instruction offset in the side/hash
above. Matching offsets were checked on both sides, not assumed from the client.

## Tag family and primitive payloads

Factory methods C `ij.a(B)Lij;` and S `fo.a(B)Lfo;` have `tableswitch` at offset 1
covering exactly IDs 0..10; each case constructs the class below. Default branch
offset 148 pushes null and 149 returns it. IDs 11/12 (later integer/long arrays)
are not supported by these factories. Unknown application field names with
known tag IDs are different from unknown binary tag IDs.

| ID | Type | C class | S class | Factory allocation offset |
| --- | --- | --- | --- | ---: |
| 0 | End | lh | hf | 60 |
| 1 | Byte | qp | kb | 68 |
| 2 | Short | ul | mv | 76 |
| 3 | Int | pp | jq | 84 |
| 4 | Long | mi | hv | 92 |
| 5 | Float | p | k | 100 |
| 6 | Double | sz | lx | 108 |
| 7 | Byte array | hn | fb | 116 |
| 8 | String | xb | ou | 124 |
| 9 | List | sp | lr | 132 |
| 10 | Compound | nu | iq | 140 |

Scalar classes 1..6 use `a(Ljava/io/DataInput;)V` offset2 to invoke the matching
DataInput readByte/readShort/readInt/readLong/readFloat/readDouble. Their
`a(Ljava/io/DataOutput;)V` offset5 invokes the matching write operation. These
are big-endian Java data-stream values, not host-endian C values. Float/double
NaN rewrite behavior comes from the selected runtime's writeFloat/writeDouble
and must be included in the differential rather than inferred from a C cast.

## Named tags, strings, lists and compounds

| Side/class and method descriptors | Offsets | Observation |
| --- | --- | --- |
| C `ij.b(Ljava/io/DataInput;)Lij;`; S `fo.b(Ljava/io/DataInput;)Lfo;` | 1; 8-18; 20; 26; 36 | Reads signed byte type; type 0 returns End without a name/payload; other types use factory, DataInput.readUTF for name, then virtual payload read. |
| C `ij.a(Lij;Ljava/io/DataOutput;)V`; S `fo.a(Lfo;Ljava/io/DataOutput;)V` | 5; 14-17; 23; 30 | Writes type byte, returns for End, otherwise writeUTF(name) then virtual payload write. |
| C `xb.a(Ljava/io/DataInput;)V`; S `ou.a(Ljava/io/DataInput;)V` | 2 | String payload uses DataInput.readUTF. |
| C `xb.a(Ljava/io/DataOutput;)V`; S `ou.a(Ljava/io/DataOutput;)V` | 5 | String payload uses DataOutput.writeUTF. |
| C `sp.a(Ljava/io/DataInput;)V`; S `lr.a(Ljava/io/DataInput;)V` | 2; 11; 28-32; 39; 47; 56 | Reads element-type byte and signed int count; creates fresh list; loop creates element from type and reads only its payload, without names. Negative counts skip the loop. No validation of element type occurs when count is0. |
| C `sp.a(Ljava/io/DataOutput;)V`; S `lr.a(Ljava/io/DataOutput;)V` | 4-29; 35-37; 45; 60; 94 | Nonempty list chooses type from first child. Empty list forcibly chooses type 1. Then writes type, signed int size, and unnamed payloads. Consequently reading an empty list typed 0/8/10 and rewriting changes its header type to1. |
| C `hn.a(Ljava/io/DataInput;)V`; S `fb.a(Ljava/io/DataInput;)V` | 1; 9; 19 | Reads signed int byte-array length; allocates byte[length]; calls readFully. Negative length reaches Java newarray, unlike the list's empty-loop behavior. |
| C `hn.a(Ljava/io/DataOutput;)V`; S `fb.a(Ljava/io/DataOutput;)V` | 6; 16 | Writes int array length then byte contents. |
| C `nu.a(Ljava/io/DataInput;)V`; S `iq.a(Ljava/io/DataInput;)V` | 4; 10; 15-18; 26; 30 | Clears map, reads named tags until End, and Map.put(name,tag). Duplicate names therefore replace earlier logical values. |
| C `nu.a(Ljava/io/DataOutput;)V`; S `iq.a(Ljava/io/DataOutput;)V` | 4; 9; 36; 43-44 | Iterates Map.values(), writes each named child, then writes byte 0 terminator. Constructors allocate HashMap at offsets 5/9. Wire order is not established as insertion order. |
| C `as.a(Ljava/io/DataInput;)Lnu;`; S `ag.a(Ljava/io/DataInput;)Liq;` | 1; 6; 9; 21-26 | Reads generic named tag, accepts compound, otherwise throws IOException. The error calls it a named compound, but the name is not tested for nonempty text. |

The base `b()Ljava/lang/String;` getter maps an unset internal name to the empty
string. Empty tag names and empty string payloads must remain representable.
Names belong to named tags; children of lists have payloads only. Compound
termination is an End type byte with no length/name. Generic named-tag parsing
and the utility's compound-root requirement are separate observable interfaces.

The inspected base/list/compound methods have no depth parameter, explicit depth
counter or allocation budget. Recursive calls occur through generic tag/payload
read/write. This establishes absence of a local guard in those inspected paths,
not a measured maximum supported nesting depth. Do not probe stack exhaustion or
large allocations merely to discover a host crash threshold.

## Java text encoding is two distinct formats

NBT names and tag 8 values use Java modified UTF-8 via readUTF/writeUTF. The length
prefix counts encoded bytes as an unsigned 16 value, capped at 65535 on output.
NUL encodes as C0 80; a Java surrogate pair encodes as two three-byte code units,
not ordinary UTF-8's four-byte scalar encoding. Lone surrogate units are valid
Java String contents and cannot be silently replaced. A C string API that only
stores scalar Unicode text would lose valid reference values. Store explicit
UTF-16 code-unit counts (or an equivalently lossless representation), allowing
embedded NUL, and encode/decode deliberately. Decoder acceptance of raw NUL and
non-shortest forms must follow the chosen original runtime's readUTF behavior;
do not assume a strict modern UTF-8 decoder matches it.

Primary API sources: [DataInput modified UTF-8/readUTF](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/io/DataInput.html#modified-utf-8)
and [DataOutput writeUTF/writeChars](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/io/DataOutput.html#writeUTF(java.lang.String)).
The original bytecode establishes the API calls; JDK documentation defines the
library format. Runtime details remain version-pinned, as in the numerical work.

Protocol strings are separate: C `ki.a(Ljava/lang/String;Ljava/io/DataOutputStream;)V`
and S `gt.a(Ljava/lang/String;Ljava/io/DataOutputStream;)V` call String.length at 1,
compare against 32767 at 4-7, writeShort at 25, and writeChars at 30. Their read method
`a(Ljava/io/DataInputStream;I)Ljava/lang/String;` calls readShort at 1, checks the
caller maximum at 7, checks negative length at 52, and calls readChar at 84 in the
loop. The prefix counts UTF-16 code units; payload units occupy two bytes each.
Do not reuse NBT modified-UTF encoding for packets or change both together.

## Compression and ownership boundaries

C `as.a(Ljava/io/InputStream;)Lnu;` / S `ag.a(Ljava/io/InputStream;)Liq;` create
GZIPInputStream at 4/9 and DataInputStream at 12, then call the raw DataInput
overload at 17. They close the wrapped input at 22 on success and 29 on the
exception path. `as.a(Lnu;Ljava/io/OutputStream;)V` and
`ag.a(Liq;Ljava/io/OutputStream;)V` analogously create GZIPOutputStream at 4/9,
DataOutputStream at 12, and close at 22/30. A raw codec should not silently sniff
compression or own a caller's stream. Keep raw bytes and gzip adaptation explicit.

Region compression belongs outside raw NBT: C `qj.a(II)Ljava/io/DataInputStream;`
and S `jx.a(II)Ljava/io/DataInputStream;` inspect the region compression selector,
use GZIPInputStream at 185/198 for selector 1 and InflaterInputStream at 237/250 for
selector 2. Their `b(II)Ljava/io/DataOutputStream;` uses DeflaterOutputStream
at 15/29. Region addressing/allocation/recovery and writable saves are outside
the proposed raw codec increment; this evidence prevents conflating .mcr zlib
payloads with level.dat-style gzip wrappers.

## Implementation scope

The C implementation is `src/storage/nbt/nbt.c`, with lossless text helpers in
`src/core/text/mutf8.c`. Public tests are under `tests/storage/nbt/` and
`tests/core/text/`. [The API guide](../storage/nbt.md) describes ownership,
limits, retained unknown fields and error behavior. Raw NBT, compression,
region storage and complete world writes are separate boundaries. No
original-compatible world save or round-trip claim follows from these notes.

## Original-class differential

The project-owned `tests/reference/nbt/NbtProbe.java` invokes the untouched JAR
classes reflectively. It contains no copied decompiled implementation.
`scripts/reference/nbt/compare_nbt.py` verifies each side's SHA-256 before loading.
Each original side runs in separate JVM processes with a dedicated class loader
whose parent is the platform loader. Both obfuscated JARs are never put on one
application class path. Loading the inspected NBT families requires no Minecraft
startup, display, account or server listener.

The harness reflects exact signatures: C `Class.forName("ij", true, loader)` / S `fo`;
`getDeclaredMethod("b", DataInput.class)` for generic named read; and static
`getDeclaredMethod("a", baseClass, DataOutput.class)` for named write. Pass
DataInputStream/DataOutputStream over byte arrays using the interface-specific
method selection. Use `getDeclaredMethod("a", DataInput.class)` and
`getDeclaredMethod("a", DataOutput.class)` with setAccessible(true) for direct
payload-only tests on factory-created tags. Match the return descriptors above;
do not select overloaded methods solely by name.

For compound-root helpers it selects `as`/`ag` with DataInput.class explicitly;
select InputStream.class only for the gzip variant. Overload ambiguity is easy
because DataInputStream is both InputStream and DataInput. Record reflected
class/method, runtime version/fingerprints, original JAR hash, input hex, consumed
length, output hex, exact Java exception class and logical tree. Traverse original
objects by public type/name getters and original list accessors/compound values;
reflection on private list/Map fields can inspect wire element typing and tree
contents without altering them. Do not replace original readers with a second
home-written Java parser as the oracle.

Generated fixtures cover UTF-16 units 0,1,7F,80,7FF,800,FFFF, lone high/low
surrogates, paired supplementary values, empty strings and the encoded-length
65535/65536 boundary. Names and payload strings are exercised independently.
Packet controls demonstrate distinct framing. Small malformed cases cover
negative list/byte-array lengths, unsupported IDs, bad/truncated modified UTF,
noncompound roots, early End and duplicate keys. The harness does not execute
giant allocations or stack-exhaustion probes.

Initial parsed trees retain raw floating bits on both sides. Original
constructed writes independently establish canonical NaN output; comparisons
across a write boundary explicitly expect that transformation. No blanket NaN
normalization is applied to the initial parse comparison.

Compound logical trees sort children by exact UTF-16 name solely for comparing
map state; type, name, values, nested state, list order and floating bits remain
checked. Differing compound wire bytes are accepted only if recursively
sorting compound child byte records makes the wires identical. This comparison
keeps every other byte unchanged, including encoded names/strings, lengths,
types, primitive payloads, list order and trailing bytes. A duplicate-key
emission or noncanonical text encoding cannot pass as a mere ordering change.
Complete original and C outputs are retained in the local report.

## Observed scenarios and reproduction

The first integrated Windows/MSVC comparison with Temurin 17.0.17+10 passed
these per-side scenarios against both supplied JARs:

| Operation | Per-side count |
| --- | ---: |
| Original raw reads, including expected errors | 194 |
| Original constructed object writes, including length failures | 40 |
| Original packet-string controls | 12 |
| Original-produced byte streams parsed by C | 157 |
| Original re-reads of original/C output | 435 |

Four negative-list rejections across the two sides are recorded as malformed
input departures, not parity passes. Twenty byte differences across comparison
phases are recorded as compound child-order differences. The C writer preserves
retained order, while the tested JVM's original writer iterates `HashMap`.
These results do not establish arbitrary compound wire identity, gzip/region
compatibility, schema completeness or any original-created/rewrite-created
world round trip. The selected current JVMs are not an original-era JVM claim.

The optional harness requires JDK 11 or newer and compiles with `--release 11`.
Run from the repository root after building `mc_nbt_probe`:

```powershell
python scripts/reference/nbt/compare_nbt.py --executable build-windows/mc_nbt_probe.exe --client b1.7.3.jar --server b1.7.3_server.jar --work-dir .local/reference/nbt
```

Use `--java` and `--javac` to pin tool paths, and `--tested-commit` with the
exact 40-character source commit. Linux uses its own executable/JAR/tool paths.
The report records JAR, tool, source, compiled harness and C executable hashes,
runtime versions, full request/response artifacts, timestamps, scope and the
caller-supplied tested commit. An omitted commit is explicitly unrecorded.
Local reports belong in ignored directories, not source or release artifacts.

CTest can enable the same comparison with `MC_ENABLE_NBT_REFERENCE_TESTS=ON`,
`MC_REFERENCE_CLIENT_JAR`, `MC_REFERENCE_SERVER_JAR` and
`MC_REFERENCE_TESTED_COMMIT`. Public CI keeps this option off and downloads no
reference files. Missing or mismatched inputs, missing tools and unsupported
Java versions report `UNAVAILABLE`, exit 77; CTest marks that skipped. Execution
errors and mismatches return 1. A fresh running/unavailable/fail report replaces
prior results so an interrupted or missing-input run cannot leave a stale pass.
The exact final tested commit and native results are in [the resume note](../status.md).
