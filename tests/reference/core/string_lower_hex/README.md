# Whole-String lowercase hex projection

`mc_java_string_lower_hex_prepare` admits counted UTF16 Strings whose qualified
JDK17 default-Locale lowercase paths preserve UTF16 positions and ASCII hex
projection. `mc_java_string_lower_hex_project` then projects the unchanged original
unit at a valid index into `0123456789abcdef`, with fallback15. A prepared view
borrows immutable caller storage. The module allocates nothing and reads no host
Locale. It does not implement general String lowercase.

Original `sj.a(String,int,int,int,boolean)` calls whole-String `toLowerCase()` at
PC178, then `charAt(originalIndex+1)` at PC185 and hex lookup at PC188. Native
BitmapFont prepares at that reached callsite, preserving earlier GL/NIO effects
when admission fails. CPU layout and commands share the same preparation and
projection during their existing preflight. Unformatted text never needs this
lowercase dependency.

The exhaustive selected-JDK observations cover 1,114,112 codepoint values per VM:
1,112,064 Unicode scalars and 2,048 isolated-surrogate values. They record actual
`Character.toLowerCase`, ROOT `String.toLowerCase`, returned `charAt` units and
canonical combining classes. No simple lower mapping changes UTF16 width or maps
a non-ASCII codepoint to an ASCII hex unit. The only ROOT String width change is
U+0130. The complete shipped `ConditionalSpecialCasing` table adds Lithuanian
I/J/I-ogonek MORE_ABOVE insertions, Lithuanian precomposed I expansions, and
Turkish/Azerbaijani U+0307 AFTER_I deletion. The kernel refuses every potential
width hazard across these Locale paths. Contextual sigma remains a one-unit
non-hex mapping.

The source predicates inspect original codepoint contexts. CCC230 signals
MORE_ABOVE; CCC0 ends the context; other nonzero classes continue it. AFTER_I
looks through other nonzero classes and stops at CCC0/230. Runtime data stores
this three-way partition as 297 sorted nonzero ranges, derived from 365 complete
CCC runs. Of all observed codepoint values, 484 have CCC230, 388 have another
nonzero CCC, and 1,113,240 have CCC0. Valid surrogate pairs are decoded for this
property lookup; isolated surrogates remain single units. ASCII uses no table
lookup.

Two fresh ordinary VMs, interpreter and default mode, produced identical
159,748-byte stdout. Each also observed 37 Strings in six Locale slots: ROOT,
English, Turkish, Azerbaijani, Lithuanian and the unchanged process default.
There were 138 admitted and 84 conservatively refused rows per VM, with no
admitted hex-projection mismatch. Source/JDK/product brackets matched, and
post-preparation forbidden-operation checks were zero. These component
observations and complete special-case source review support the bounded
invariant domain; they do not establish full String/Locale or native GL parity.

`source-reference.json` retains sanitized source/Class/Code/data/helper hashes,
all four actual transport hashes, exhaustive counts, generation hashes and the
private independent audit fingerprint. It contains no JDK/game Class bytes,
JARs, native libraries, assets, Worlds or host paths. The raw observations remain
in the ignored capture directory.

To regenerate from the exact qualified raw stdout:

```powershell
python -B tests/reference/core/string_lower_hex/generate_ccc_partition.py `
  --stdout .local/<qualified-capture>/interpreter.stdout `
  --output src/core/java/string_lower_hex_ccc_partition.inc --check
```

Omit `--check` to write the table. The generator rejects altered raw bytes,
partial inventories, malformed JSON, missing/overlapping CCC ranges and failed
projection obligations. It redistributes only the derived property partition.

The native `test_string_lower_hex.c` controls include the reached `§béΩ` input,
non-ASCII prefixes, Kelvin/sigma, supplementary and isolated surrogate units,
embedded NUL, exact CCC boundaries, all width hazards, suffix-only hazards and
output preservation on refusal. Two source contexts for every codepoint value
also have a golden fingerprint derived independently from the actual full CCC
observations. CPU font tests retain unsafe-expansion/removal refusals and check
newly admitted Unicode colors, commands, widths and glyph positions. Native
test results are recorded separately by the build owner.
