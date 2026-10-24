# Runtime dependencies

The storage compression module links statically to upstream zlib 1.3.2. It is
written in C and provides gzip/zlib framing, deflate, and checksum validation.
The shared numerical/text core remains independent of it. The optional native
client links SDL3 for windows and OpenGL contexts, and libpng for image decoding.
Its current [title-screen scope](client/startup.md) remains incomplete.

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

## Client libraries

`MC_BUILD_CLIENT=ON` is the default. It adds pinned static SDL3 3.4.18 and
libpng 1.6.59 from their upstream source archives:

| Library | Source archive | SHA-256 |
| --- | --- | --- |
| SDL3 3.4.18 | [SDL release](https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz) | `9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3` |
| libpng 1.6.59 | [libpng release](https://sourceforge.net/projects/libpng/files/libpng16/1.6.59/libpng-1.6.59.tar.gz/download) | `86a3e4b501f7f50e392c4e456ea158893e9a595b1c63eb88c7bb9f6cf8772dad` |

SDL supplies platform windowing and input; the project's OpenGL 3.3 core
renderer draws the GUI. SDL's renderer and GPU modules are disabled. Audio is
built into SDL but the client has no sound playback yet. libpng uses the same
pinned static zlib as storage. No arbitrary system zlib is substituted.
The simulation and server libraries do not link SDL, libpng or OpenGL.
`-DMC_BUILD_CLIENT=OFF` excludes both client dependencies and client targets.

Offline source overrides are `FETCHCONTENT_SOURCE_DIR_MC_SDL3` and
`FETCHCONTENT_SOURCE_DIR_MC_LIBPNG`, each an absolute path to the unpacked
corresponding source. As with zlib, source overrides bypass archive hashing;
verify those sources before configuring. Build directories retain their own
dependency configuration. Use a fresh directory when changing dependency sources.

SDL3 and libpng retain their upstream licenses, copied verbatim in
[licenses/SDL3.txt](../licenses/SDL3.txt) and
[licenses/libpng.txt](../licenses/libpng.txt). Include those notices and the
zlib notice with any future executable containing these static libraries.
The project's MIT license does not replace them. Original Minecraft resources
remain separately supplied through the local asset importer.
