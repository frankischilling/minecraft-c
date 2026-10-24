# Counted Java float parsing

The [parser](../../include/mc/core/java/float_parse.h) reads counted UTF-16 text
and constructs IEEE binary32 bits directly. It belongs to `mc_core` and needs
no graphics, locale, Java runtime or original assets. GameSettings' exact
`true`/`false` mapping belongs to its caller; this primitive rejects those words.
The [legacy formatter](java-float-format.md) is a separately validated primitive;
options-file persistence remains unfinished.

## Admission and results

Only edge code units at or below U+0020 are trimmed. Decimal and hexadecimal
tokens accept an optional sign and numeric `f`, `F`, `d` or `D` suffix.
Hexadecimal tokens require a `p` or `P` exponent with ASCII decimal digits.
Exact signed `NaN` and `Infinity` are accepted without suffixes. Every token
must be consumed. Underscores, Unicode digits, locale commas and interior NUL
characters do not gain C-string parsing semantics.

A null text pointer represents Java null text. A nonnull empty view, including
`{NULL, 0}`, represents an empty String. Native argument and limit errors are
separate from null-text and number-format outcomes. Every failure preserves
the output. Input, view and output storage must remain live and disjoint.

The native text budget is 16 MiB, or 8,388,608 UTF-16 units. Length is checked
before traversal. This bound is native policy, not Java allocation-failure
timing. The implementation allocates no heap storage and performs no floating
arithmetic. It rounds directly to nearest binary32 with ties to even, preserves
negative underflow zero, and emits canonical positive quiet NaN `0x7fc00000`.

## Integer rounding

The scanner retains 192 significant digits and records whether any discarded
digit is nonzero. Whole-token validation precedes numerical overflow or
underflow classification. Large explicit exponents saturate beyond a bound
that the admitted digit count cannot cancel into the finite range.

Every binary32 value and adjacent rounding midpoint is a multiple of
`2^-150`. Decimal midpoints therefore lie on the `10^-150` grid. When decimal
digits are discarded in the relevant range, the retained prefix grid is at
most `10^-153`. A discarded tail can affect a comparison only when the prefix
equals the boundary; the nonzero-tail flag then selects strictly above it.
The hexadecimal prefix grid is at most `2^-637`, giving the same property.

Checked integer products and shifts compare the retained magnitude with
finite values and midpoints. A monotone search over positive bit patterns
finds the lower value, then the midpoint comparison chooses the rounded result.
The largest required workspace is 1,045 bits; fixed 64-limb arrays provide
2,048 bits. Sign is applied after rounding. No CRT conversion or binary64
intermediate participates.

## Evidence and remaining work

The [public unit test](../../tests/core/java/test_float_parse.c) covers 100
checks: grammar, signed zero, special values, midpoint ties, overflow,
subnormals, distant nonzero tails, counted text and the full text budget.
The [probe](../../tests/core/java/float_parse_probe.c) accepts input tokens
only and emits statuses and raw result bits. Expectations stay outside it.

Standalone and public shared-core builds passed those checks and 5,632 probe records on
MSVC 19.51, GCC 13.3 and Clang 18.1. Linux used ASan and UBSan. The corpus
combines 494 lexical/rounding cases, exact decimal representations of 5 source
defaults and 5,132 other binary32 controls, and one null-text record. Integer
and rational arithmetic supplied external expectations. Authored standard-library
probes on Temurin 17.0.17 and Oracle JDK 21.0.10 corroborated the original
494 parsing cases. These are finite primitive observations, not executed
Minecraft settings-load or whole-client parity.

The later public integration builds disabled the client and passed 13 selected core,
server-construction and standard-library reference tests per compiler. The
actual target and direct-import graphs contained no graphics dependencies.
This verifies shared server construction, not a standalone server loop.

The original client `kv.a(String)F` recognizes exact `true` and `false` before
calling `Float.parseFloat` at PC 23. Its save method `kv.b()V` uses
`StringBuilder.append(F)` for music, sound and sensitivity. Arbitrary loaded
floats are not restricted to normal slider positions. Installed JDK 17 and 21
produced 591 different output strings across 585 tested bit patterns, despite
matching parsing results. A general shortest-decimal formatter or `snprintf`
does not establish the required legacy save strings. The new legacy formatter
has separate implementation and validation scope. The GameSettings wrapper,
file ordering and options-screen integration remain open.
