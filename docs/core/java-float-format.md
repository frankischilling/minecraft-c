# Java float text formatting

The [formatter](../../include/mc/core/java/float_format.h) exposes
`mc_java_float_to_string(float, mc_java_float_string *)` for the retained
Temurin 17.0.17 `Float.toString` and `StringBuilder.append(float)` profile.
The owned result has a 24-byte ASCII buffer and a counted length excluding the
terminating NUL. It requires no allocation or destructor. A null output returns
`INVALID_ARGUMENT`; checked internal invariant failures return `INTERNAL_LIMIT`.
Every failure preserves the output.

All binary32 values are admitted. Signed zero prints `0.0` or `-0.0`, NaNs
print `NaN`, and infinities retain their sign. Finite output uses ordinary
notation for decimal orders -3 through 6 and uppercase `E` elsewhere, with a
fractional digit and no locale-dependent characters. Input bits are read by
`memcpy`; the function performs no arithmetic on the supplied float value.

The legacy profile does not always choose the shortest round-trip decimal.
The implementation uses an independently derived integer grid and exact
rational interval model, including the reached primitive-width modular
comparisons. For example, the retained minimum-normal output is
`1.17549435E-38`. Replacing the selector with a general shortest-decimal routine
would change some saved strings.

The bounded exponent estimator requires strict IEEE binary64 nearest/even
evaluation, disabled floating traps, and no excess precision, reassociation or
fused operations. The formatter does not alter floating controls. Fixed 512-bit
integers, a double-width multiplication temporary and a ten-attempt bound limit
the conversion; a conservative output
bound is 16 characters plus NUL. The 24-byte result leaves room within that bound.
The implementation has no I/O, CRT float conversion, callbacks or mutable
global state. NaN payload preservation and floating exception flags are outside
the text contract.

## Evidence and scope

The independently authored private predecessor passed 31 literal cases plus a
null-output control on MSVC 19.51, GCC 13.3 and Clang 18.1. Linux enabled ASan and
UBSan. Each compiler matched 5,708 actual retained JDK 17 value rows, containing
5,518 unique bit patterns. A separate 49,532 assertions exercised mathematical
model boundaries derived from complete estimator/dispatch partitions. Those
additional strings are model-derived expectations; they were not newly
observed from a JVM.

The public float-argument adaptation also passes those 55,240 external rows
and the literal/null controls on all three compilers through the actual shared
core library. Client-disabled builds pass 13 selected core, server-construction
and authored standard-library reference tests per compiler; their target and
direct-import graphs contain no graphics dependencies. An independent file-only
audit rechecks every output ID, ordinal, input bit pattern, counted text and
expected category, plus all 39 actual test records.

These results qualify the finite scenarios. The model's universal partition properties remain separate
from finite native/JDK observations. Java 6, other JDK profiles and whole
GameSettings persistence are not qualified by these results. The
[counted parser](java-float.md) is a separate primitive; GameSettings mutation,
file ordering and options-screen behavior belong to their callers.

The [public unit test](../../tests/core/java/test_float_format.c) retains the 31
literal cases and null-output control. Its
[input-only probe](../../tests/core/java/float_format_probe.c) accepts this ASCII
format:

```text
minecraft-c-float-format-inputs-v1
basic/one	3f800000
basic/negative-zero	80000000
```

The separator is one actual tab. IDs contain 1..80 ASCII letters, digits, `/`,
`-` or `_`; each input has exactly eight lowercase hexadecimal digits and a
final LF. The file is bounded to 8 MiB and 65,536 rows. The producer converts
the raw bits by `memcpy` before calling the public float API, emits the counted
ID and original input bits, and checks output capacity and termination before
printing. Expected strings remain outside the producer. The probe passively
reports the initial rounding profile and checks it remains unchanged.
It checks each stdout write and the final flush/error state before success.
The public transport controls cover valid records, rejected input and a closed
output pipe; they require ordinary failure status 1, rather than accepting a
signal or sanitizer abort as a rejected record.
