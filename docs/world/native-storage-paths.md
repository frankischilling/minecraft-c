# Native storage paths on Windows

The C region transport previously opened an ordinary 236-unit target but failed
to save its replacement: the added `.mc-0000000000000000.tmp` suffix made the
temporary name 260 UTF-16 units. The MSVC runtime returned `ENOENT` from the
exclusive temporary open. Shortening the work directory hid this failure.

`src/world/storage/files.c` now resolves long ordinary names to private absolute
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
has implementation coverage only; network-share runtime testing remains needed.

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

These tests qualify the C transport, not the separately modeled
`java.io.File` provider. That provider's extended-path boundary remains, so this
does not establish long-path support throughout ordinary single-player saving.
No gameplay FPS, sustained-play, or new original-runtime comparison is claimed.
