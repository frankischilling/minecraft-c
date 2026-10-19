# Translation bootstrap and imported files

The [translation API](../../include/mc/resources/translation/translation.h)
executes the original client `do`/`nh` and server `ch`/`ii` initialization on the
same native class owner as Block and Item. The wrapper and singleton have
separate COLD, INITIALIZING, READY and FAILED states, initializer positions and
retained first failures. READY here describes these translation classes only.

The singleton constructor publishes its Properties field at PC12, opens
`/lang/en_US.lang` at PC23 and loads it at PC26, then opens
`/lang/stats_US.lang` at PC37 and loads it at PC40. It publishes the singleton
static only after both loads return. The wrapper then publishes that same
reference. The original constructor never closes these streams.

Both loads use the real [Properties decoder](../reference/properties.md).
Registry String wrappers retain each source String's identity and borrow its
immutable UTF-16 storage. Bindings are prepared during loading, so plain lookup
does not allocate. The actual wrapper's missing-key result is the original key
reference. The public diagnostic property reader returns NULL for an absent
property without performing a class active use.

## Native resource source

[mc_resource_files](../../include/mc/resources/files/files.h) opens byte streams
under an imported directory. Pass the asset importer's `jar/` directory as its
root, then borrow `mc_resource_files_source(files)` in `mc_registry_options`.
Creation copies the UTF-8 root without reading files. Windows converts paths to
UTF-16 for native file opening; Linux uses the UTF-8 path. Absolute class-loader
names use ASCII path segments; empty, dot, parent, drive and backslash segments
are rejected. The local imported directory is trusted input, not a symlink
sandbox.

The source retains stream contexts through EOF, failures and explicit close.
An explicit close retires the file immediately; repeated close succeeds and
later reads report I/O failure. Retire the class owner before freeing the source.
Source retirement closes every still-open stream and releases its storage,
including when a close reports failure. No JAR, Java,
launcher or network request is used by the native consumer.

Missing files produce a null stream, which fails at the subsequent original
load PC26 or PC40. Malformed Unicode fails there with ILLEGAL_ARGUMENT and
retains earlier puts. A caught read error reaches the explicitly unfinished
`printStackTrace` effect at constructor PC48; it does not publish an empty or
partially loaded singleton as success. Missing resource configuration stops at
the reached open operation. Formatted lookup, statistics, achievements and
complete registry/bootstrap wiring remain unfinished.

## Evidence and tests

Ordinary `Class.forName` initialization through isolated loaders for both
[pinned original JARs](../reference/binaries.md) completed normally in the
[translation observer](../../tests/reference/resources/TranslationObservation.java).
The observer reads the actual published singleton, wrapper and Properties and
calls the original plain wrapper for five explicit keys. It supplies no
replacement game class or resource and makes no World or statistics entry.

Both originals, across interpreted, tier-1 and default Temurin 17.0.17+10 modes,
produced identical 95,654-byte observations: 633 properties, 1,268 retained
String identities and 1,911 rows, SHA-256
`06488e213cbc9840fbe72f32d44b129b51f86b44d7d378417e86d9e30c9ab6a5`.
The [native file probe](../../tests/resources/translation/files_probe.c), using
the exact privately extracted resource bytes, matches the complete observations
on MSVC, GCC and Clang. Original resource contents and these complete text
observations remain private.

Public verification uses synthetic resource bytes only. The
[translation unit](../../tests/resources/translation/test_translation.c) passes
4,504 checks over both sides, successful and partial loads, String lifetimes,
native resource bounds, saved failures and reentrant public-call refusal.
Diagnostic inspection remains available during the admitted flow. The
[filesystem test](../../tests/tools/resources/test_files.py) checks Unicode
roots, trailing separators, binary bytes, actual map and fallback identities,
and missing-file load boundaries. Fourteen selected cases, including existing
Block/Item comparisons, pass on all three compilers with Linux sanitizers.
