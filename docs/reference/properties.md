# Properties byte-stream compatibility

The [C17 decoder](../../src/core/java/properties.c) implements the reached
`Properties.load(InputStream)` and String-only `getProperty` behavior with null
defaults. Its [public API](../../include/mc/core/java/properties.h) accepts real
byte streams and a caller-owned retained allocator. It has no JVM dependency.

Bytes decode as ISO-8859-1. Logical lines preserve the selected JDK's comment,
whitespace, separator, continuation, CR/LF and EOF behavior. Escapes produce
exact UTF-16 units, including NUL and lone surrogates. Duplicate puts retain the
first key owner and replace the value with a distinct String owner. Earlier
puts survive malformed Unicode or I/O failure. Repeated loads mutate the same
Properties object, while saved String views remain live.

The LineReader refills its 8192-byte input buffer at a line boundary before
returning that line. A read failure can therefore prevent a completed physical
line from being put. The implementation follows this ordering rather than
splitting a decoded input buffer into lines first.

## Executed evidence

[PropertiesObservation.java](../../tests/reference/java/PropertiesObservation.java)
calls the actual installed Temurin 17.0.17+10 `Properties` implementation. It
uses 60 fixed byte-input cases, each with unrestricted, one-byte, seven-byte
and 8192-byte read shapes: 240 fresh owners, 252 loads and 980 independent
queries. Outcomes include actual exception metadata, consumed bytes, read and
close counts, complete map contents and persistent String identities. Only
map enumeration is sorted, by unsigned UTF-16 keys.

The interpreted, tier-1 and default JVM runs produced the same 1,778,120-byte
[observation](../../tests/reference/java/properties_observation.tsv), SHA-256
`68541d5487f886ac443fd0e6519c08792b3d17b939bad299ef1a988f1a156707`.
The [provenance](../../tests/reference/java/properties_provenance.json) binds
the executed sources, runtime profile, compiled classes and successful child
completion. Mode equality does not establish JIT compilation or an original-era
JVM result.

The [projector](../../scripts/reference/java/properties_protocol.py) validates
the complete observation and supplies only declared byte/query inputs to the
[native probe](../../tests/core/java/properties_probe.c). Expected maps and
Strings come solely from the Java outcomes. Native `IO` and `MALFORMED` statuses
correspond to the observed exception categories; C does not throw or synthesize
Java Throwable objects. All 3,806 projected rows compare exactly. Three corrupt
observation cases and three invalid native transports are rejected separately.

The independent [native unit](../../tests/core/java/test_properties.c) passes
5,377 checks, including first-key/value identities, retained overwritten values,
partial puts, read-boundary failures and one native allocation failure at every
request in its finite measured paths. The observation and native checks pass
on MSVC, GCC and Clang, with Linux address/undefined-behavior sanitizers and leak
detection.

Recreate the reference data with the pinned Windows JDK:

```powershell
python scripts/reference/java/capture_properties.py --java '<jdk>/bin/java.exe' --javac '<jdk>/bin/javac.exe' --output-dir .local/properties-fresh
```

The output directory must be fresh. The capture verifies the retained profile
and checks all recorded inputs before and after execution. Ordinary public
CTest runs replay the recorded data and need no Java installation.

## Boundaries

Diagnostic enumeration uses native first-insertion order; it makes no claim
about Java Hashtable iteration. General Object values, inherited defaults,
Reader/XML loading, stores and concurrent access are not implemented. The
caller must retain allocator storage until every owner and String view is
retired. Native allocation/size failures are explicit resource policy, not
Java OutOfMemoryError timing. This module supports the separate
[translation bootstrap](../resources/translation.md); it does not establish
complete game initialization.
