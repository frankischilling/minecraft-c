# Java modified UTF-8

The shared text codec in `include/mc/core/text/mutf8.h` converts Java modified
UTF-8 payloads to and from explicit arrays of UTF-16 code units. Embedded zero
units, lone surrogates and surrogate pairs remain intact. It adds no terminator,
allocates no memory and handles no stream prefix. NBT supplies its own unsigned
16-bit byte length and enforces the 65535-byte limit. Packet strings use a
different encoding; see [the original NBT/string evidence](nbt.md).

Encoding uses the canonical Java representation: zero becomes `C0 80`, units
`0001..007F` take one byte, `0080..07FF` take two and `0800..FFFF` take three.
Surrogate pairs occupy six bytes. The
[DataInput format and readUTF contract](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/io/DataInput.html#modified-utf-8)
describe the byte groups and decoder rules. The
[DataOutputStream writeUTF contract](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/io/DataOutputStream.html#writeUTF(java.lang.String))
defines the stream prefix and encoded-length restriction.

Decoding follows `DataInputStream.readUTF` acceptance, including raw `00` and
non-shortest two-/three-byte forms. Four-byte UTF-8 sequences and standalone or
malformed continuation bytes fail. Reencoding decoded data is canonical, so
accepted noncanonical byte sequences need not retain their original bytes.

## API and bounds

Length functions report the required element count. Conversion functions take a
destination capacity in elements and report the number written. All operations
require their count output pointer; a null input is allowed only with count
zero, and a null destination only for an empty result. Input, destination and
count storage must be disjoint. No conversion changes its destination or count
output on failure, including failures discovered after valid input prefixes.

The decoder validates the entire payload and capacity before writing. The
encoder measures before writing. Arithmetic guards reject impossible input
array sizes and encoded-size overflow. The core codec supports payloads larger
than 65535 bytes so callers can apply their own framing limits deliberately.
The public tests exercise the early impossible-array guard; they do not allocate
near `SIZE_MAX` to execute the accumulated-byte overflow branch.

## Runtime observations, 2026-10-17

An ignored, project-owned standalone probe called installed Eclipse Adoptium
Temurin `17.0.17+10` `DataInputStream.readUTF` and
`DataOutputStream.writeUTF` directly. Each decoder payload below was prefixed
with its actual byte length using `DataOutputStream.writeShort`. The probe used
no original game classes.

| Payload bytes | Decoded UTF-16 units | Canonical bytes after writeUTF, without prefix |
| --- | --- | --- |
| empty | empty | empty |
| `00` | `0000` | `C0 80` |
| `41 00 42` | `0041 0000 0042` | `41 C0 80 42` |
| `C0 80` | `0000` | `C0 80` |
| `C0 81` | `0001` | `01` |
| `C1 BF` | `007F` | `7F` |
| `E0 80 80` | `0000` | `C0 80` |
| `E0 81 BF` | `007F` | `7F` |
| `E0 9F BF` | `07FF` | `DF BF` |
| `ED A0 80` | `D800` | `ED A0 80` |
| `ED BF BF` | `DFFF` | `ED BF BF` |
| `ED A0 BD ED B8 80` | `D83D DE00` | `ED A0 BD ED B8 80` |

The same runtime threw `java.io.UTFDataFormatException` for payloads `80`, `BF`,
`C0`, `C2`, `C2 41`, `E0`, `E0 80`, `E0 41 80`, `E0 80 41`, `F0 90 80 80`,
`FF` and `41 C0`. These are format failures in complete framed payloads; truncated
streams and transport I/O are outside this payload codec.

For a canonical encoding comparison, the probe encoded every individual Java
`char` value `0000..FFFF` through `writeUTF`, removed each two-byte prefix and
concatenated the payloads. This avoids asking `writeUTF` to encode a single
oversized string. Encoding the same units with the C codec produced the same
194433 bytes. Both files had SHA-256
`ab707e980889b90f8b1db0d459b5135ce36193211f21fe0c6318dd5b33c8105e`.
On Windows the C dump used a file opened with `"wb"` to preserve byte `0A`.

To reproduce the Java canonical payload, save this helper inside `.local/` and
compile/run it with the pinned JDK. Its only argument is an output file path.
Compare that file with concatenated `mc_mutf8_encode` outputs for the same
ascending unit sequence.

```java
import java.io.*;
import java.nio.file.*;

class Mutf8Canonical {
    public static void main(String[] args) throws Exception {
        var all = new ByteArrayOutputStream();
        for (int unit = 0; unit <= 65535; ++unit) {
            var framed = new ByteArrayOutputStream();
            new DataOutputStream(framed).writeUTF(new String(new char[]{(char) unit}));
            byte[] bytes = framed.toByteArray();
            all.write(bytes, 2, bytes.length - 2);
        }
        Files.write(Path.of(args[0]), all.toByteArray());
    }
}
```

These observations pin the selected installed runtime. They establish text
payload behavior, not a historical runtime comparison, original-class NBT
differential or world save compatibility result.

## Public tests

`tests/core/text/test_mutf8.c` performs 596848 checks without Java or supplied
assets. It covers canonical round trips for all 65536 code units, exact selected
runtime byte fixtures, every three-byte decode form, every two-byte decode form,
all one-byte inputs, malformed groups, incomplete groups, destination capacities,
unchanged outputs on failure and a combined 194433-byte payload.

The standalone tests passed with native Windows MinGW GCC 15.2.0 at `-O2` and
MSVC `/std:c17 /W4 /WX /O2`, and under WSL Ubuntu GCC 13.3.0 with AddressSanitizer
and UndefinedBehaviorSanitizer. GCC builds used C17 and
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. Formatting uses the
repository `.clang-format` and clang-format 22.1.0. Integrated build and reference
results belong to the increment's recorded commit.
