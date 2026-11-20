# Byte decoding and line readers

The [native Reader](../../include/mc/core/java/reader.h) implements the reached
`BufferedReader(InputStreamReader(InputStream, Charset)).readLine()` path. It
uses 8192-byte and 8192-character buffers, malformed-input replacement and the
source stream's availability observation. CR, LF and CRLF terminate lines.
Returned UTF-16 lines retain their actual identities after later reads,
failures and close. Directly constructed empty lines have fresh owners; the
buffer-spanning StringBuilder path returns a shared empty literal. EOF returns
NULL; an empty line has a nonnull owner.

UTF-8, windows-1252, ISO-8859-1 and US-ASCII are implemented. The
[native default factory](../../src/platform/text/charset.c) queries the Windows
ANSI code page or a private POSIX character locale without changing process
locale. Other native charsets fail explicitly. Missing close support remains
unfinished and leaves the reader open. A provided close runs once and closes
the reader even if that operation reports an I/O error.

The caller retains all arena allocations until every line view retires.
Native size and allocation failures preserve the output argument and retained
earlier lines. They do not reproduce Java heap allocation or OutOfMemoryError
timing. Mark/reset, general Reader methods, caller-selected replacement policy
and concurrent access are outside this interface.

## Recorded JDK evidence

[ReaderObservation.java](../../tests/reference/java/ReaderObservation.java)
executes real JDK readers over explicit synthetic bytes. The 800 owners cover
four charset profiles, read sizes ALL/1/7/8192, zero or remaining availability,
availability errors, malformed UTF-8, all byte values, long lines and stream
read/close failures. Its 7,136 actions include repeated EOF, retained lines,
close before reading, repeated close and reads after close.

Interpreted, tier-1 and default Temurin 17.0.17+10 runs produced identical
3,117,607-byte [observations](../../tests/reference/java/reader_observation.tsv),
SHA-256 `c3d695ba660385dfcb49f8d5943f7c9691683228780ef8d1f0ccfb63c43a0da9`.
The [provenance](../../tests/reference/java/reader_provenance.json) binds source,
runtime, compiled classes and successful reaped children. These modes do not
establish JIT compilation or the behavior of every original-era JVM.

The [projector](../../scripts/reference/java/reader_protocol.py) sends only
declared bytes and controls to the [C probe](../../tests/core/java/reader_probe.c).
Expected UTF-16, identities and stream counters come from actual observations.
Only observed `java.io.IOException` maps to native IO; raw Java class/message
metadata remains in the fixture. Native code does not manufacture Throwables.
The [comparison](../../tests/tools/java/compare_reader.py) checks every projected
row, malformed observations, content/input separation and deliberate native
transport rejection. The [independent unit](../../tests/core/java/test_reader.c)
also exercises retained allocation failures and source-derived decoder rules.

All 19,594 projected rows and 134,207 native checks pass on MSVC, GCC and
Clang. Linux runs use address/undefined-behavior sanitizers and leak detection.
The first comparison exposed a buffered empty-line identity mismatch; the
implementation now preserves the shared empty literal demonstrated by the
unchanged observation.

Recreate the observation with the pinned Windows JDK and a fresh directory:

```powershell
python scripts/reference/java/capture_reader.py --java '<jdk>/bin/java.exe' --javac '<jdk>/bin/javac.exe' --output-dir .local/reader-fresh
```

Public CTest replays the recorded data without a Java installation. This
reader supports [achievement-key loading](../statistics/keys.md); it does not
complete statistics registration or game startup.
