# Native storage paths on Windows

The C region transport previously opened an ordinary 236-unit target but failed
to save its replacement: the added `.mc-0000000000000000.tmp` suffix made the
temporary name 260 UTF-16 units. The MSVC runtime returned `ENOENT` from the
exclusive temporary open. Shortening the work directory hid this failure.

`src/platform/files/windows_path.c` resolves long ordinary names to private absolute
extended paths before calling the wide CRT and Windows APIs. This covers both
the target and temporary sibling, plus directory creation, reads, replacement
and cleanup. The caller's UTF-8 path and region cache key retain their identity.
No registry or application manifest change is needed for this transport.
The conversion follows Microsoft's [extended path rules](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation).

The converted prefix and terminator must fit 32,768 UTF-16 units. Component and
filesystem limits still apply. Relative paths require a stable current working
directory during each call. Dot and parent components resolve before prefixing;
remaining trailing-dot/space component aliases fail explicitly to avoid silently
addressing a different object. Explicit device/extended names and drive-relative
directory creation retain their previous unsupported boundary. UNC conversion
is implemented but has not been exercised against a network share.

## Verification

Build `mc_world_regions_probe`, `mc_world_store_probe` and `mc_test_stored_chunk`,
then run:

```powershell
ctest --test-dir BUILD -C Release --output-on-failure -R "^(world.stored_chunk|tools.world_store_interop|tools.regions_reference|tools.regions_interop)$"
```

The region runner tests exact target lengths of 235, 236, 259, 260 and 400 UTF-16
units, including an accented character and a supplementary character. Each case
creates and replaces the region, independently decodes disk contents, reopens
with a fresh cache, and checks relative slash/dot/parent spelling. A preexisting
temporary sibling survives collision handling; successful writes leave no newly
owned temporary. The 400-unit Windows case also rejects three unsupported name
forms without changing the target.

The same new runner fails against the retained preceding Windows executable at
`write.open.errno=2`. The corrected MSVC 19.51 Release executable passes all four
focused CTests. GCC Debug passes the same four tests with ASan, UBSan and leak
detection. Strict GCC compilation and clang-format 18 checks also pass.

## Source File provider

The shared converter also serves the modeled `java.io.File` provider. Its old
248-unit admission check rejected `exists()` before reaching the filesystem.
The same 400-unit ordinary path succeeds with Temurin 17.0.17+10 on Windows:
`mkdirs`, attributes, output/input streams, rename, deletion and canonical path
identity all complete. The independent standard-library probe is
`tests/reference/core/file/FileLongPath.java`. This is evidence about that JDK
provider, not a new execution of the game's classes or a claim about every JDK.
The probe takes one ASCII hex string containing UTF-16BE path units, generated
with Python's `str(path).encode('utf-16-be').hex()`. Direct launcher arguments
replaced a supplementary character with two question marks on this host; that
failed attempt did not test the intended File path. The encoded probe passes
Unicode roots of 235, 236, 247, 248, 249, 259, 260, 261 and 400 units.

Windows recursive directory creation now handles extended drive and UNC roots
and uses bounded dynamic canonicalization buffers for long names. Short names
retain the existing stack buffers. Source File/String identities and names in
enumerated File arrays retain their ordinary lexical paths; the native prefix
does not become a source name. The C transport and Source File provider share
one conversion implementation.

The four File attribute CTests run the same real operations under their normal
fixture, Unicode roots of 247, 248, 260 and 400 UTF-16 units, and a relative spelling
of a separate 400-unit root. Both ordinary and native-fault controls run in
normal and optimized Python modes. The controls include file and directory
attributes, rename, enumeration, input-stream contents, deletion, source path
identity and cleanup ownership.
The Windows native-fault variant also forces canonicalization allocation failure
through the real File `mkdirs` call. It must retain the caller's Boolean, report
allocation failure and leave an inactive READY File that can retry successfully.
Allocation failure must not become successful `mkdirs=false`.
A separate 247-unit missing-path query contains an internal trailing-dot/space
component. This catches prefixing one unit too early: the native capacity
includes NUL, while the provider's 248-unit threshold excludes it. The query
must return successful `false`, retain its source path, and create nothing.
The [JDK threshold](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/java.base/windows/native/libjava/io_util_md.c#L144-L158)
uses path length excluding NUL. This control fails before the one-unit
correction and passes afterward. Encoded JDK observations are retained in
`tests/reference/core/file/observations_windows.json`.

`storage.source_region_output_long_path` runs all four lower region-output
profiles beneath a 400-unit work root. It independently decodes region sectors,
compression and NBT payloads, including replacement, late growth consumption
and close/ownership controls. Its capture CWD stays short because Windows
process creation rejected a long CWD before starting the child. Independent
snapshot reading uses an explicitly shared wide CRT stream; the first narrow
reader failed at the long name, and a secure-CRT reader then denied sharing
with the live RAF. Those failed preparations supplied no passing runtime
result. Production region I/O remains separate from this test reader.

The region runner launches its relative-path control with an explicit CWD on
the fixture drive. A hosted Windows run exposed the old assumption when the
checkout was on `D:` and temporary files on `C:`. A local `M:`/`C:` control
reproduced the old `relpath` rejection and passed the corrected full runner;
the temporary drive alias was then removed.

The complete client-disabled MSVC Release build and all 1,114 default CTests
passed in 378.17 seconds after the allocation-failure correction. That sweep
preceded the final 247-unit threshold correction; the final eleven focused
Windows filesystem/storage tests then passed, including the new alias control.
Linux qualification remains the eleven focused GCC Debug tests with ASan,
UBSan and leak detection. Full graphical-client and final hosted checks are
separate gates.

The provider change does not qualify full-client saves, save selection UI,
extended play, UNC network behavior, or every Windows name alias. Long
trailing-dot/space component aliases remain an explicit unsupported boundary.
No gameplay FPS or sustained-play claim is made.
