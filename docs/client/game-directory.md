# GameDirectory

GameDirectory owns the native game's selected directory path above GameSettings
and the title screen. The [interface](../../src/client/startup/game_directory.h)
creates the directory when required, exposes a borrowed UTF-8 path and copies
the completed native-operation state. Successful owners retain no open native
handles. Freeing an owner releases memory only; it neither deletes directories
nor saves options.

The real-file tests pass on Windows MSVC x64 Debug: 57 native checks and six
independent file checks. Linux execution remains pending.
The [reference contract](../reference/game-directory.md) records the original
File policy and the selected JDK home-path source. Static source agreement is
not a measured default-directory or filesystem comparison.

## Selecting a path

An explicit `--game-directory DIRECTORY` passes that UTF-8 path directly. It is
separate from `--assets` and is not a `user.home` override. A relative input is
resolved through normal filesystem operations. An explicit empty input remains
empty and fails its reached creation path; it does not request the default.

With no explicit input, Linux uses the account database for the real UID,
through `getuid` and caller-owned `getpwuid_r` storage. A missing or failed
lookup selects the source `?` home value. It does not read HOME, parse a passwd
file itself or substitute the asset directory. The selected home gets the
`.minecraft` child. A successful native account record with a null pw_dir is
rejected as a native I/O failure before C-string access. That malformed-record
refusal has not been exercised or mapped to a Java outcome.

Windows queries the current user's Profile known folder with
`KF_FLAG_DONT_VERIFY`, then the legacy Profile query, then the source `C:\`
fallback. It separately reads a wide environment snapshot. An APPDATA entry,
including an empty value, takes precedence over the queried home; a missing
entry leaves the home selected. The child is `.minecraft`. Empty APPDATA is
not the same as absence: the source File parent rule produces a root-relative
child. No USERPROFILE, HOME or SDL preference-path fallback is added.

The App retains one resulting owner for its lifetime. The original caches a
File in a static field; separate calls to this native constructor remain
independent owners and do not claim original VM-global caching or property
history.

## Existence, creation and failure

Creation performs the original outer `exists || mkdirs` decision. An existing
regular file is accepted at that check. There is no preliminary is-directory
test. If the path is absent, native mkdirs first attempts a real mkdir, then
uses canonicalization and parent recursion when needed. Canonicalization does
not replace the retained owner path. A missing-prefix `missing/../child` may
create canonical `child` while keeping the original path text; it does not
repair the missing prefix.

Creation failure preserves the caller's previous owner output and returns its
entered operation prefix when requested. Already created directories stay on
disk. A final false mkdirs result is `CREATION_FAILED`; native argument,
allocation, bound, encoding and unsupported-path errors have separate statuses.
The state reports native work and failures, not observed Java call counts.
Failed release of a temporary native resource is fatal to admission.

The App loads `options.txt` only after GameDirectory succeeds. A later caught
GameSettings load error retains defaults and partial field changes in the real
settings owner. It does not select a second directory. Startup and teardown
perform no automatic options save.

## Filename and resource limits

The current File boundary is strict UTF-8, with strict UTF-16 conversion on
Windows. Encoded paths and account/environment lookup storage each have a
1 MiB bound; parent recursion admits at most 1,024 levels. UTF-16 conversion
storage has its own unit bound, and temporary storage inside native APIs is
outside these encoded-input bounds. Native canonicalization also has platform
path limits.

Required behavior still includes general JDK locale/filename conversion,
malformed native bytes, arbitrary UTF-16 paths, property/security overrides,
cached user-directory semantics and broader symlink/network/permission/race
cases. Windows paths needing 248 or more UTF-16 units, wildcard or trailing-dot
canonicalization, and duplicate case-insensitive APPDATA names are explicitly
refused in the current implementation. These are unfinished requirements,
not approved compatibility exceptions or reasons to choose a different path.

The new `client.game_directory` test confines all writes to fresh authored
directories, including Unicode names, existing files, nested creation and
failure paths. Its Windows default case supplies and checks a fresh APPDATA
parent. Linux native-account default creation is excluded from that driver.
The successful run retains the exact directory family and four untouched
sentinels after owner retirement. Its child exits naturally and is reaped.
These finite results make no original-library or ordinary-user-directory claim;
general native default selection and the unfinished path forms remain outside
the tested scope.
