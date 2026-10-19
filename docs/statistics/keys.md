# Achievement-key resource loading

The [key API](../../include/mc/statistics/keys/keys.h) executes the actual shared
class `b` on the Block/Item/Translation class owner. Its constructor publishes
map field `b` at PC12, opens `/achievement/map.txt` at PC27 and creates its
default-charset reader at PC30. Each line is split on a comma without trimming;
trailing empty segments are removed. The first segment passes through checked
signed integer parsing and the second becomes the value. Extra segments are
ignored. Duplicate IDs replace values; missing lookup IDs return NULL.

Successful EOF closes the reader at PC83. Static singleton `a` is published at
PC7 only after the constructor returns. A failure retains the actual map prefix
and earlier String owners. Equal nonempty values keep distinct identities;
empty substrings share the owner's empty literal. Diagnostic enumeration uses
native first-insertion order and makes no claim about HashMap traversal.

The [reader](../reference/reader.md) handles actual bytes and line boundaries.
The [imported-directory source](../resources/translation.md) closes the file
immediately when requested, retaining its context until source retirement.
Availability queries inspect the open file size and logical stream position
without consuming bytes. Translation's separate Properties loads still do not
close their streams.

The original catches Exception over constructor PCs15..86, prints at PC91 and
then returns. Printing the original stack trace remains unfinished: a reached
catch stops explicitly at PC91, retaining the caught category and source PC
for diagnostics. It cannot report successful initialization with that effect
missing. Native allocation, unsupported charset and unavailable close support
remain explicit native failures. Native storage layout and allocation timing
are not a replica of the Java object graph.

## Original observations

[KeyObservation.java](../../tests/reference/statistics/KeyObservation.java)
normally initializes untouched original `b` through an isolated class loader,
reads the actual singleton/map and calls original `a(int)` for every entry and
seven fixed boundary IDs. Every lookup is checked against its actual map value
by reference identity. Sorting signed integer keys is an observation format,
not an assertion about original HashMap iteration.

Both [verified original JARs](../reference/binaries.md), across interpreted,
tier-1 and default Temurin 17.0.17+10 runs, produced identical 82,102-byte
observations: 446 entries, 453 queries, 446 String identities and 1,353 rows.
SHA-256: `15f5827db492ee388805d256a20637e5a4c03c3a0cd932fbc916c5949d6f482a`.
Normal completion and empty stderr were checked; the observer does not inspect
the original reader's file handle. Original resource text and raw observations
remain private.

The [native file probe](../../tests/statistics/keys/files_probe.c) emits that
same complete view from imported bytes. Public
[unit tests](../../tests/statistics/keys/test_keys.c) use synthetic resources
for publication order, parsing, retained failures, String identity, close,
reentry and native allocation budgets. The
[file test](../../tests/tools/statistics/test_keys_files.py) also uses synthetic
bytes and Unicode filesystem roots. This class-specific work does not complete
achievements, recipes, full registry bootstrap or population. Actual
[common-stat registration](registry.md) now calls this loader at the original
registration PC121, after list/map publication and before the `h` assignment.

The complete original view matches on MSVC, GCC and Clang. All 4,299 native
key checks pass within 14 selected registry/resource cases on each compiler;
Linux runs use address/undefined-behavior sanitizers and leak detection.
