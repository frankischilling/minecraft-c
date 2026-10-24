# Reached Java string formatting

The [C API](../../include/mc/core/java/format.h) implements the String/null
argument profile reached by formatted Translation names. It parses and
validates the whole format before printing any argument, preserving Java's
error precedence. It supports ordinary, explicit and previous argument
indices, String hashes, Boolean/null conversions, width, precision, literal
percent and the actual platform line separator. Numeric, character and date
conversions accept null arguments and reject nonnull String arguments with
the reached conversion category. Numeric/date value formatting and arbitrary
Object or Formattable behavior are outside this interface.

UTF-16 precision truncation happens before English full uppercase; padding
uses the resulting expanded length. Lone surrogates remain valid units.
The full uppercase table comes from all 1,114,112 code-point values observed
with `String.toUpperCase(Locale.ENGLISH)` on the pinned Windows Temurin
17.0.17+10 profile. It contains 1,485 changed mappings. English composition
follows that runtime's source loop; other locale mappings are not claimed.

The allocator retains storage through result use. Inputs may borrow earlier
retained allocations; new allocations must not overlap live inputs or output
slots. A failed operation preserves the result slot. Native allocation and
length limits have separate statuses from Java text errors. Typed diagnostic
operands identify the actual reached failure, without fabricating Java
Throwable identities, stack traces or allocation timing.

The [Java observer](../../tests/reference/java/FormatObservation.java) emits
declarations separately from actual `String.format` results and public error
getters. Its 275 synthetic cases include null and empty arrays, malformed
formats, validation precedence, Unicode, indices and limits. The
[replay](../../scripts/reference/java/compare_format.py) sends only declarations
to the native probe and compares exact UTF-16 output, exception categories
and 109 typed flags/conversion/width/precision operands. Actual messages and
normalized specifiers remain in the fixture but are outside that native
comparison. The nonpublic argument-index getter is explicitly unavailable.
Three malformed transports must fail before any output. Changed observed
outcomes must leave declared input unchanged and fail comparison.

The [uppercase replay](../../scripts/reference/java/compare_string_case.py)
compares the native formatter's actual `%S` traversal over every code-point
value with the complete Java capture. The probe receives no expected mappings.
The independent [native unit](../../tests/core/java/test_format.c) checks
output preservation, retained allocation failures and unsupported locales.
Selected MSVC, GCC and Clang runs pass, with Linux ASan/UBSan and leak
detection. The native formatter unit reports 613 checks per compiler.

To reproduce either Java capture, use the pinned Windows JDK17 installation
and a fresh output directory:

```powershell
python scripts/reference/java/capture_strings.py format --java <jdk>/bin/java.exe --javac <jdk>/bin/javac.exe --output-dir <fresh-directory>
python scripts/reference/java/capture_strings.py case --java <jdk>/bin/java.exe --javac <jdk>/bin/javac.exe --output-dir <another-fresh-directory>
```

The recorder checks runtime inputs, runs interpreted, tier-1 and default
modes, retains child streams and reap receipts, and requires equal complete
results. Both commands have reproduced the retained fixtures. The
[table generator](../../scripts/reference/java/generate_case_table.py) writes
an exclusive new file from the pinned Java observation; apply the repository
formatter before comparing it with the C data header.
