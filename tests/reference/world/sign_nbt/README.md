# Original Sign constructor and NBT components

These observations call the unchanged Beta 1.7.3 client `yk` constructor,
reader and writer directly. They cover object-level NBT behavior. They do not
establish native parity, World attachment, factory behavior, rendering, editing,
network behavior, binary NBT order or region-save roundtrips.

The original client archive has SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`source-pins.json` records 14 original Class identities and 114 methods with
Code attributes. The observed Class-origin inventory is smaller: seven in the
main cohort and six in the null-line supplement. Static pins do not imply that
every method ran.

Both cohorts ran on Windows x64 with Eclipse Adoptium Temurin 17.0.17+10 on
2026-11-16. Each cohort used four fresh VMs in interpreter, default, interpreter,
default order. All four complete stdout streams and all four complete stderr
streams were byte-identical within each cohort.

| Cohort | Cases per VM | Selected calls per VM | Rows per VM | Retained stdout |
| --- | ---: | ---: | ---: | ---: |
| Main constructor/read/write | 22 | 35 constructors, 39 reads, 27 writes | 126 | 267675 bytes |
| Public null-line writer supplement | 4 | 4 constructors, 4 writes | 17 | 24729 bytes |

The two `.stdout` files preserve actual Windows CRLF bytes. One stream per
identical cohort is stored; `provenance.json` retains all eight actual process
receipts, including each raw stream hash and length. Runtime stderr contained
exactly four JDK SecurityManager deprecation warnings. Its raw hashes and lengths
are retained, while the private helper-directory URI is replaced in the
published warning text. No other stderr is discarded under that rule. Compiler
streams were empty and complete. Every process had a 60-second limit, 4 MiB
stdout limit and 1 MiB stderr limit.

The authored callers and static inspector are included byte-for-byte. Original
archives, assets, generated classes, private paths and original method
instructions are excluded. `audit-summary.json` records the independent
retained-byte audit of the main cohort and the separate null-writer prefix audit.
The original private audit hashes remain recorded; decoded instruction operands
were omitted from the public summary.

## Observed behavior

Construction creates four references to the same empty String, edit row `-1`
and private Boolean `c=true`. The reader sets `c=false` before the parent reader.
It truncates each line to 15 UTF-16 units. A supplementary pair cut at that
boundary leaves its high surrogate. Completed coordinate and text assignments
survive a later wrong-tag failure; the old suffix remains.

The writer does not truncate. A separately declared public-array prestate reaches
overlong lines that a completed reader would already have shortened. Four
supplemental cases put `null` in one public line at a time. The original String
tag constructor throws `IllegalArgumentException`, with message
`Empty string not allowed`. Base fields and completed earlier text writes remain
in the output Compound; the failed line and later seeded fields remain unchanged.
The observed Tile fields do not change during writing.

Unknown input fields are not copied into a fresh writer target. Unrelated fields
already present in an existing output target survive its setter calls. Native
storage overlays are a separate contract and receive no equivalence credit
from these results. Likewise, these observations retain original String aliases
and Map encounter order without claiming that native held strings or binary NBT
serialization have the same identities or order.

The observer reads original private fields through reflection and retains
output-only linear `==` reference labels, reset per case. The only authored
Source-field writes are the explicitly marked public overlong/null-line
prestates. Input Compounds use genuine original constructors and setters.
Complete Throwables, tag contents, UTF-16 units, aliases and Map encounter keys
remain in the raw records. Read-only reflection and retained references do not
prove observer-off, whole-VM, startup, GC or timing neutrality.

The helpers deny network, native linking, subprocesses, file writes/deletes and
explicit exit after preparation; only stdout/stderr descriptor writes remain
allowed. Denied-attempt counts are zero. The executed controllers reject
`JAVA_TOOL_OPTIONS`, `_JAVA_OPTIONS` and `JDK_JAVA_OPTIONS` when nonempty. This
environment check is established by the retained controller source/audit; no
separate environment snapshot is claimed.

## Offline verification and fresh capture

Run both forms from the repository root:

```text
python -B tests/reference/world/sign_nbt/test_packet.py
python -O -B tests/reference/world/sign_nbt/test_packet.py
```

The validator checks the publication inventory, frozen bytes, original Class
origins, all process receipts, ordered cases and phases, complete states/tags,
UTF-16 read prefixes, full writer values and null-line failure prefixes.
Corruption controls also exercise the semantic parser directly, independently
of fixed raw hashes. No JVM, native executable, original asset or game window is
needed for these checks.

To make a new private observation with the supplied unchanged archive and a
selected JDK, choose a fresh ignored output directory for each cohort:

```text
python -B tests/reference/world/sign_nbt/capture_reference.py --jar SUPPLIED_CLIENT_JAR --jdk SELECTED_JDK --output .local/sign-nbt-main-new --cohort main
python -B tests/reference/world/sign_nbt/capture_reference.py --jar SUPPLIED_CLIENT_JAR --jdk SELECTED_JDK --output .local/sign-nbt-null-new --cohort null-lines
```

The path arguments are portable; qualification remains tied to the recorded
Windows/JDK context. A different context is retained as a failed comparison
instead of being normalized into this packet. The new controller pins inputs
and helper products around every fresh child and preserves full commands and
pipes privately. It never overwrites this historical packet or manufactures a
new native or gameplay result.
