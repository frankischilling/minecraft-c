# Millisecond wall clock

The private `src/platform/time/millis.h` interface reads milliseconds since the
Unix epoch on x86-64 Windows and Linux. It belongs to `mc_platform_time` beside
the separate monotonic clock used by the Random profile. World construction
does not call this new backend yet.

## Native contract

`mc_platform_current_time_millis` makes one selected OS API call: Windows
`GetSystemTimeAsFileTime`, or Linux `clock_gettime(CLOCK_REALTIME)`. A successful
conversion publishes the result. Null output rejects before querying; a query
or conversion failure preserves output. The clock may move backward. There is
no fallback, clamping, retained clock state, allocation or Random operation.
Integer-only conversion requires no floating-point environment admission.

The production FILETIME converter accepts ticks through `INT64_MAX`, subtracts
`116444736000000000`, then divides the signed difference by `10000` with
truncation toward zero. One tick before the Unix epoch therefore returns zero.
The timespec converter requires nanoseconds in `[0,999999999]` and computes
`seconds * 1000 + nanoseconds / 1000000`, checking both the multiplication and
addition. Seconds `-1` and nanoseconds `999999900` return `-1` millisecond.

Invalid arguments, OS failure and range failure have distinct statuses. Rejecting
high-bit FILETIME values or an overflowing seconds intermediate is an explicit
native policy, including when a later fractional addition could rescue the
mathematical result. These statuses are not JVM exception categories. Both pure
conversion helpers preserve output on failure; they neither sample a clock nor
inject a value into World construction.

## Source binding

The installed Windows Temurin 17.0.17+10 release identifies source prefix
`3ae1373f8abb`. The upstream commit resolves to
`3ae1373f8abb8add664985ab772915e681f56ea0` in `adoptium/jdk17u`.
The retained release file has SHA-256
`fdf03ea4c142f886ff60e4f2fb44b1a9f8c47aa9784c8d34fc7f8cbada48cf44`.
Five exact upstream files were fetched through the repository contents API;
their recorded Git blob IDs were recomputed from the saved bytes.

| Pinned source | Reached behavior | File SHA-256 |
| --- | --- | --- |
| [System.c, line40](https://github.com/adoptium/jdk17u/blob/3ae1373f8abb8add664985ab772915e681f56ea0/src/java.base/share/native/libjava/System.c#L40) | Native method registration maps `currentTimeMillis` to `JVM_CurrentTimeMillis` | `0834ec74bd016eb6ae33ddf8f9bc5214298cf5b313b5ebf61092558b2e1cf1e1` |
| [jvm.cpp, line234](https://github.com/adoptium/jdk17u/blob/3ae1373f8abb8add664985ab772915e681f56ea0/src/hotspot/share/prims/jvm.cpp#L234) | Entry returns `os::javaTimeMillis` | `5057686a66a8872f3a5f06526123eb28c790b0afdab9bfd94a6362aff5a520cd` |
| [os_windows.cpp, lines1006-1085](https://github.com/adoptium/jdk17u/blob/3ae1373f8abb8add664985ab772915e681f56ea0/src/hotspot/os/windows/os_windows.cpp#L1006) | Epoch offset, signed subtract/divide and one FILETIME query | `d377ef690142592a5a9e11f40780d76a904f3c62b484454282d4494689279cf4` |
| [os_posix.cpp, line1484](https://github.com/adoptium/jdk17u/blob/3ae1373f8abb8add664985ab772915e681f56ea0/src/hotspot/os/posix/os_posix.cpp#L1484) | Realtime seconds/fraction conversion | `345160f92c1c1173e4fba9f9b5a2f8d95b2bdb55469eec742c2c0e36b97638c3` |
| [globalDefinitions.hpp, lines275 and628](https://github.com/adoptium/jdk17u/blob/3ae1373f8abb8add664985ab772915e681f56ea0/src/hotspot/share/utilities/globalDefinitions.hpp#L275) | Unit constants and high/low-half `jlong_from` helper | `c9246a2d6b11c4fdbea8f85def640947e663ca14391674601c04c6785cd609fe` |

The four-file capture receipt has SHA-256
`6727e5b4dc7ab91c9b836dcdc0d0e515a98d2b88714e9fa497f01552eedb03cd`;
the additional helper receipt has SHA-256
`bbca072b65576001cc0d2267162f6a1b3ce00587819d5c5966e02aa0b521106c`.
Both retain unchanged before/after release fingerprints. These are source
bindings, not disassembly or execution proof for the installed native library,
JIT intrinsics, all transitive headers or an original World constructor.

## Verification and limits

The selected runs retain these exact source fingerprints:

| File | SHA-256 |
| --- | --- |
| `src/platform/time/millis.h` | `be29675facc5b5b98d186c89374c0486228182d6d8bfa9fea0ec64f8785b7da9` |
| `src/platform/time/millis.c` | `12370ff6516f4273704cb07e9f595ddcabcb7e8856c510c04f9261089183dfe4` |
| `tests/platform/time/test_millis.c` | `cc4611c4a32c5af3c486d44eb18af41c88d00e59cfcea7c2f8f44766912bb738` |

`platform.millis` passes 251 checks on native Windows MSVC 19.51, Ubuntu
GCC 13.3 and Clang 18.1.3. Linux uses ASan/UBSan and leak checks; Windows uses
strict Debug without sanitizers. All three runs observe four ordered OS clock
brackets out of four. The separate five-case selection also passes
`platform.nanotime`, `platform.nanotime_conversion`, `core.java_random_profile`
and `core.java_random_constructor_observation` on each compiler. This is a
selected run, not a fresh complete public-suite or original-game qualification.

The new test covers 17 literal FILETIME vectors, three unsigned range failures,
19 literal timespec vectors, 12 timespec range failures, null precedence,
unchanged rejected outputs and neighboring guard words. Six MXCSR variants
exercise FTZ, DAZ and nondefault rounding without changing the APIs' integer
results or control state; the test restores the complete original register
before assertions and diagnostics.

Live brackets query the selected OS before and after the production call.
Their endpoints use the separately literal-tested production converters;
they are not independent conversion oracles. A detected endpoint reversal is
reported and removes only that interval's containment assertion. At least one
ordered bracket is required for the test to pass. Undetected clock jumps
between observations remain a limitation. No cross-call monotonicity is assumed.

No OS failure is forced, and no original/JVM timestamps are compared. World
field publication, constructor ordering, metadata/provider startup and complete
native-library provenance remain unfinished. This backend alone establishes
none of those behaviors.
