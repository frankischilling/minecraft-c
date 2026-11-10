# Counted UTF16 case comparison

`mc_java_string_equals_ignore_case` compares counted UTF16 spans using the
qualified JDK17 `String.equalsIgnoreCase` behavior. It does not allocate,
normalize text, select a locale or perform full uppercase expansion. Native
callers classify Java null receivers separately; positive-length null spans
are rejected without reading them. Empty spans compare equal.

The implementation follows the actual `StringUTF16.compareToCIImpl` order:
compare UTF16 units first, then their simple case keys. Only a remaining
mismatch assembles a surrogate pair. A low surrogate can use the preceding
high surrogate after that preceding unit already compared equal. A high
surrogate at the current position can consume its following low surrogate.
Unpaired surrogates remain individual units. Equal initial UTF16 lengths are
required, as in `String.equalsIgnoreCase`.

The case key is `Character.toLowerCase(Character.toUpperCase(int))`. The actual
comparison first accepts equal uppercase values, then equal lowercase values
of those uppercase values. The single key preserves both outcomes because
equal uppercase values necessarily have equal lowercase values. The runtime
uses an ASCII fast path and a sorted table of 1,390 changed non-ASCII keys.
It needs no JVM, ICU or host locale at runtime.

## Authority and reproduction

The authored Java producer invokes unchanged bootstrap `java.base` Character
and String methods. It writes case keys for all 1,114,112 code point values and 1,306,160 actual
String comparison results. The latter include every codepoint paired with its
actual simple fold, selected character cross-products, every surrogate unit
with selected mixed operands, and fixed-seed mixed strings. Constructor or game
classes are not involved.

The qualified local runtime is Eclipse Adoptium `17.0.17+10`. The complete
big-endian key stream has SHA-256
`6db4c52baaef252f2bc0fa4536528871ff077f4cf0338c81228edda1ee32d628`.
The producer records the actual runtime/version and class-byte fingerprints.
For that runtime, `StringUTF16.class` has SHA-256
`6c3edcca58cfeacee4c98c80c2c46f596616b88e0173382b1afb4f1f71c7a721`.
The full input/helper fingerprints remain in each private capture receipt.

```powershell
python -B tests/reference/core/string_case/capture_reference.py `
  --executable build-windows/mc_test_string_case.exe `
  --work-dir .local/string-case-validation
```

Select matching JDK17 tools with `--java` and `--javac` when PATH differs.
`MC_ENABLE_STRING_CASE_REFERENCE_TESTS=ON` or the general
`MC_ENABLE_JAVA_REFERENCE_TESTS=ON` registers the same comparison in CTest,
including the existing Java-enabled native CI jobs. Missing or mismatched tools fail the enabled comparison; they do not
produce a passing result.

The runner compares all native scalar keys byte for byte, checks every native
comparison against its original Boolean, and regenerates the tracked table
from the actual Java data. It also verifies unchanged JDK/source/native/helper
inputs and refuses altered answers, wrong magic, truncation, trailing bytes,
missing records, invalid Boolean bytes and oversized harness lengths.
Raw generated observations stay under the ignored output directory.

`core.java_string_case` requires no Java. Its 21 public controls cover ASCII,
Turkish I forms, long S, Kelvin sign, sigma/final sigma, sharp S, supplementary
Deseret/Adlam, malformed surrogate boundaries, counted lengths and a golden
fingerprint over all scalar keys. The optional comparison provides the stronger
point-by-point scalar and finite String result evidence. Neither a finite corpus
nor the scalar inventory proves equality for every possible UTF16 sequence.
