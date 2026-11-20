# Default DateFormat observations

`DateFormatObservation.java` calls the selected JDK17's real no-argument
`SimpleDateFormat` constructor. The supported English profiles use the observed
SHORT/SHORT pattern `M/d/yy, h:mm a`. The default-profile comparison also changes
the JVM's default locale and timezone after construction and verifies that the
original formatter retains its captures. No JVM is used by the native runtime.

`vectors.tsv` records 2,926 supplied timestamps in UTC years1967 through2099,
with separate UTF16 results for `America/New_York` and UTC. It brackets every
Eastern transition in that interval, including the1974/1975 emergency rules,
and covers negative epochs, leap/year boundaries and the2000 century. The
generator obtains transitions from the JDK's actual zone rules and formats each
timestamp with the actual `java.text` formatter. It does not calculate expected
calendar fields in test code.

The native owner admits English/en_US and captured UTC/US Eastern zones in this
range. Other profiles fail explicitly. Windows uses captured native zone identity
and biases with the observed historical Eastern rule closure; extrapolating
Windows' first dynamic rule backwards would change older dates. Linux uses its
native timezone data under the existing Calendar stable-environment contract.
This evidence does not claim equality for other locales, zones, date ranges or
all timezone database releases. Changed Linux TZ is refused before conversion.

The interface tests verify constructor ownership, preserved outputs/state on
failure, explicit unsupported profiles, cleanup, and one real constructor clock
with no formatting clock calls. The optional reference controller checks the
full vector corpus against a supplied JDK17 and compares the actual native host
default profile. MSVC Windows and GCC/Clang Linux sanitizer runs passed for the
selected profiles; exact observation inputs are recorded in source-pins.json.
