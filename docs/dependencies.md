# Runtime dependencies

The storage compression module links statically to upstream zlib 1.3.2. It is
written in C and provides gzip/zlib framing, deflate, and checksum validation.
The shared numerical/text core remains independent of it. A graphical runtime
and its window, renderer, and audio dependencies have not been added.

The [official release site](https://zlib.net/) lists version 1.3.2, released
February 17, 2026. CMake downloads the versioned
[source archive](https://zlib.net/fossils/zlib-1.3.2.tar.gz) and verifies SHA-256
`bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`.
The first configuration needs network access unless a local source override is
provided; subsequent builds use the build directory's downloaded source.

For an offline build, verify and unpack that archive yourself, then configure
with `-DFETCHCONTENT_SOURCE_DIR_MC_ZLIB=<absolute source directory>`. This uses
CMake's source override and bypasses archive verification, so verifying the
local source is the caller's responsibility. CMake does not search for or
silently substitute an arbitrary system zlib version.

Only the static library is enabled. Upstream shared libraries, tests and install
rules are disabled in normal project builds. Project warning flags are applied
to project source; sanitizer instrumentation also covers the downloaded zlib
when `MC_ENABLE_SANITIZERS` is enabled. The public headers expose no zlib types.

The unmodified upstream source keeps its license. A copy is retained at
[licenses/zlib.txt](../licenses/zlib.txt); the project's MIT license does not
replace it. Include this notice with any future packaged source or executable
that contains zlib. No release artifact has been published yet.
