# Java decimal integer parsing

`mc_java_integer_parse` implements the counted UTF-16 decimal
`Integer.parseInt(String)` primitive for the selected JDK 17 character profile.
It accepts ASCII signs, mixed decimal digit scripts and leading zeroes, with
checked signed 32-bit bounds. It does not trim whitespace, recognize radix
prefixes or combine UTF-16 surrogate pairs into code points. Null and empty
text return the number-format category. Every failure preserves the output.

The [public interface](../../include/mc/core/java/integer_parse.h) separates
native invalid arguments and the 8,388,608-unit traversal budget from Java
number-format outcomes. The implementation uses fixed integer operations and
37 decimal ranges, with no heap, locale, CRT conversion or floating arithmetic.
The range table describes code points; it does not copy the JDK's generated
lookup implementation.

An authored library-only producer observed `Character.digit(char,10)` and
`Integer.parseInt` for every one of the 65,536 UTF-16 units on Temurin
17.0.17+10. Exactly 370 units form decimal digits in this profile, arranged in
37 ten-unit ranges. Supplementary mathematical digits still fail when passed
as surrogate units to this Java overload. The initial producer left a partial
success label before a caught parse exception; its failed framing audit is
retained separately from the corrected complete run. No original game class
ran in this experiment.

The native [unit test](../../tests/core/java/test_integer_parse.c) covers signed
bounds, malformed syntax, mixed scripts, surrogate units, counted NULs, output
preservation and the traversal limit. A separate
[projection](../../tests/core/java/integer_parse_probe.c) enumerates each
one-unit input through the public API, without receiving expected values.
Public client-disabled core integration passes 43 Integer checks and all
65,536 externally compared one-unit records on MSVC 19.51, GCC 13.3 and Clang
18.1, with ASan/UBSan on Linux. Four selected core tests pass per compiler;
the actual target/direct-import graphs contain no graphics dependency.
An independent file-only audit rechecks all 12 actual test records and 196,608
projection rows. The original controllers rejected a formatter output label
in their report parser; they remain failed. Separate postexecution audits
correct that label and complete the remaining read-only import/SDK checks,
without rerunning builds, tests or probes.

Complete alphabet coverage does not establish every multi-unit input, another
JDK/Unicode profile or GameSettings file behavior.

GameSettings must use this primitive for integer values and retain its own
colon splitting, per-line catches, defaults and partial mutations. The
[float parser](java-float.md) and [float formatter](java-float-format.md) are
separate primitives with different text rules.
