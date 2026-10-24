# Java output encoding and PrintWriter

The [writer interface](../../include/mc/core/java/writer.h) implements the
selected `FileWriter` / `OutputStreamWriter` / `PrintWriter(Writer)` chain needed
by [GameSettings](../reference/game-settings.md). It belongs to the shared core
and builds with the graphical client disabled. It requires no Java runtime,
original archive, assets or graphics context.

One opaque owner combines an 8,192-byte encoder with an optional PrintWriter
policy. The caller selects a charset, LF or CRLF, and direct or PrintWriter
mode. The byte-output callbacks and their context form the stream seam; a
separate entry point opens an actual file. Construction of a callback-backed
owner performs no I/O. Each owner requires exclusive access, and callback
reentry returns `BUSY`.

## Encoding and buffered state

UTF-8, windows-1252, ISO-8859-1 and US-ASCII use `?` replacement for malformed
or unmappable input. A valid surrogate pair produces one replacement in a
single-byte charset. A final high surrogate remains pending across writes
and flushes; source close ends that input and emits replacement. No BOM,
escaping or newline conversion is added to counted UTF-16 writes. NUL and
lone surrogates remain counted input rather than C-string terminators.

The native default selector reuses the [Reader charset discovery](../reference/reader.md).
It does not reproduce an arbitrary JVM's cached `file.encoding` override.
An original comparison must pass its measured charset and line separator
explicitly. Only LF and CRLF profiles are admitted.

The byte buffer retains the selected JDK's failure state. A failed output
write leaves it flipped, with position zero and its previous buffered length
as the limit. A later write can overwrite earlier buffered bytes; a later
flush can skip them. The module preserves those effects instead of retrying
the original bytes or rolling back a committed prefix. Positive short native
writes complete within one logical output operation. An error stops that
operation even if its callback committed some or all of the requested bytes.

At an exact buffer boundary, ISO-8859-1/windows-1252 can drain before inspecting
a trailing high surrogate, while UTF-8/ASCII inspect that input first. The
implementation preserves this callback order. The contract comes from the
selected Temurin 17.0.17+10 library sources; it is not a universal claim about
all Java releases or character encoders.

## PrintWriter and close

| Operation | Direct writer | PrintWriter mode |
| --- | --- | --- |
| Write or newline, ordinary I/O failure | Return the failure | Set sticky trouble and return normally |
| Write or newline, interrupted I/O | Return the failure | Record a reinterrupt request and return normally |
| Flush or close, either I/O failure | Return the failure | Set sticky trouble and return normally |
| Invalid native argument, callback contract or resource limit | Return the native failure | Return the native failure |

The reinterrupt flag records a native request; it does not signal an OS thread
or report a complete Java thread state. `print` and `println` map a null String
view to `null`. `println` attempts its line separator after a swallowed text
write failure. Auto-flush is disabled, matching `PrintWriter(Writer)`.

`check_error` explicitly flushes while the underlying reference is present.
GameSettings does not call it. Read-only state inspection does not flush.
A failed close leaves PrintWriter's reference present even though the encoder
is closed. A subsequent close can clear that reference. The first encoding,
write or flush failure and a secondary close failure remain distinct.

Native retirement is separate from source close. It retires an otherwise-open
sink without encoding pending input or flushing buffered bytes. Its result
reports whether that additional cleanup occurred. It does not invent a source
finally block or turn an incomplete save into a complete one.

## Actual file adapter and verification scope

`mc_java_file_writer_open` opens, creates or truncates the destination before
the caller formats settings. Binary, unbuffered stdio adds no character buffer
or newline translation; the inherited FileOutputStream flush remains a no-op.
UTF-8 paths use checked UTF-16 conversion on Windows. No parent directory,
temporary file, backup, rename, fsync or rollback is added.
Windows opens permit concurrent read/write handles through explicit
[`_SH_DENYNO` sharing](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fsopen-wfsopen?view=msvc-170),
including the independent reader used by the file test.

The public stream tests cover every partition of an authored mixed UTF-16
sequence across all four charsets, buffer boundaries, partial output failures,
surrogate/flush ordering, PrintWriter state and native retirement. Separate
real-file checks exercise truncation, a Unicode filename, exact encoded bytes,
missing parents and retirement without source close. The Windows test converts
wide process arguments explicitly rather than interpreting the active code
page as UTF-8. Each actual public-CMake build on MSVC, GCC and Clang passes
110,708 stream checks, 19 real-file checks and five independent file checks.
All 15 selected headless tests pass per compiler; Linux enables ASan/UBSan.
The retained 1,188-file source copy matches all 1,014 current non-Markdown
project files, including 630 formatted C/header files.

The MSVC and GCC controllers stopped after CTest clipped one successful
Dungeon footer in its JUnit output. Their failed reports remain failed.
Separate file audits verify every complete raw Passed record and source
footer; subsequent inventory and import inspections complete the missing
metadata without rerunning tests. Clang's complete controller passes with an
explicit passed-output bound. These are native contracts. The separate
68-case original JDK observation does not yet establish native correspondence.

Native allocation and length limits do not model Java heap failure timing.
Arbitrary Java File paths, CRT/JDK partial-write and interruption differences,
sharing/security behavior, disk-full devices and exception-text equality need
separate real-file observations. Callback controls do not establish those
claims. BufferedWriter, channels, nested PrintWriters, auto-flush, arbitrary
encoders and concurrent Java monitor behavior are outside this interface.
Complete GameSettings ownership, file loading/saving, menus and effects remain
unfinished.
