# Original NBT and Java string evidence

Research for [issue #9](https://github.com/frankischilling/minecraft-c/issues/9),
recorded 2026-10-17. This is bytecode research, not an implemented codec or a
world save compatibility result. No NBT oracle classes were executed for this research.
The proposed original-class differential below still needs to be built and run.

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

The codec acceptance criteria are tracked in [issue #9](https://github.com/frankischilling/minecraft-c/issues/9).
The C implementation and original-class differential are not started. Raw NBT,
compression, region storage and complete world writes are separate boundaries.
No original-compatible world save or round-trip claim follows from these notes.

## Original-class differential design (not yet executed)

Write one project-owned Java harness, not a copy of decompiled implementations.
Verify the expected side's JAR SHA-256 before loading. Use a separate process per
side (simplest) or a dedicated URLClassLoader with the platform class loader as
parent; never put both obfuscated JARs on one application class path. The NBT
classes inspected reference Java data/collection classes, so loading these
families does not require initializing Minecraft or a server listener.

Reflect exact signatures: C `Class.forName("ij", true, loader)` / S `fo`;
`getDeclaredMethod("b", DataInput.class)` for generic named read; and static
`getDeclaredMethod("a", baseClass, DataOutput.class)` for named write. Pass
DataInputStream/DataOutputStream over byte arrays using the interface-specific
method selection. Use `getDeclaredMethod("a", DataInput.class)` and
`getDeclaredMethod("a", DataOutput.class)` with setAccessible(true) for direct
payload-only tests on factory-created tags. Match the return descriptors above;
do not select overloaded methods solely by name.

For compound-root helpers select `as`/`ag` with DataInput.class explicitly;
select InputStream.class only for the gzip variant. Overload ambiguity is easy
because DataInputStream is both InputStream and DataInput. Record reflected
class/method, runtime version/fingerprints, original JAR hash, input hex, consumed
length, output hex, exact Java exception class and logical tree. Traverse original
objects by public type/name getters and original list accessors/compound values;
reflection on private list/Map fields can inspect wire element typing and tree
contents without altering them. Do not replace original readers with a second
home-written Java parser as the oracle.

Fixtures should cover UTF-16 units 0,1,7F,80,7FF,800,FFFF, lone high/low surrogates,
paired supplementary values, zero-length and encoded-length 65535/65536 boundary;
NBT name and string payload independently. Add packet-string controls with the
same strings to prove distinct wire formats. For each tag, test parse->inspect,
original construct->write, parse->write and C->original->logical-tree. Small
malformed cases distinguish negative list and byte-array lengths, unsupported
type IDs, malformed/truncated modified UTF, noncompound roots, early End and
duplicate keys. Avoid giant lengths or deliberate stack-exhaustion executions.
For compounds use controlled single-key fixtures for byte-exact comparison,
then multikey/nested cases with explicit order-insensitive logical comparison.
Separate failures from Java memory/stack exhaustion; do not treat a crash as an
expected compatibility outcome for the bounded C codec.
