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
Fresh `-DMC_BUILD_CLIENT=OFF` configurations exclude SDL/libpng/OpenGL and
default to audio/codec dependencies off. Pure C client helpers remain available
for headless tests. Explicit audio/codec options can enable their separate
graphs without making the simulation or server libraries depend on them.

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

## Sound codecs

`MC_BUILD_CLIENT_SOUND` enables static libogg 1.3.6 and libvorbis 1.3.7.
It defaults to `MC_BUILD_CLIENT` and can be enabled in a headless build for
decoder tests. These C libraries stay outside the shared simulation and server
link graph. Their pinned archives are:

| Library | Source archive | SHA-256 |
| --- | --- | --- |
| libogg 1.3.6 | [Xiph release](https://downloads.xiph.org/releases/ogg/libogg-1.3.6.zip) | `e347b59c29e8f938bb48b2fda5da02f2d5d97316c004abaa76e171cf4a2fbdd7` |
| libvorbis 1.3.7 | [Xiph release](https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.zip) | `57c8bc92d2741934b8dc939af49c2639edc44b8879cba2ec14ad3189e2814582` |

Offline overrides are `FETCHCONTENT_SOURCE_DIR_MC_OGG` and
`FETCHCONTENT_SOURCE_DIR_MC_VORBIS`. CMake leaves those supplied trees unchanged.
A private Ogg copy guards empty-buffer page synchronization. A private Vorbis
copy adapts its build metadata and corrects the exact chain,
Bark packing, noise interpolation, decode search-word and floor-deviation
operations described
in the [sound contract](client/sound.md#sequential-vorbis-decoding). Source hashes
and expression checks reject an unexpected codec version. UNIX configuration
probes `alloca.h` and enables its existing include guard on the three Vorbis
targets. Sanitizer instrumentation covers Ogg and those targets.

Both libraries retain their BSD notices in [licenses/libogg.txt](../licenses/libogg.txt)
and [licenses/libvorbis.txt](../licenses/libvorbis.txt). Keep the notices with any
distributed executable or source containing these libraries. The decoder's
native contracts do not establish original JOrbis PCM equivalence.

## OpenAL device runtime

`MC_BUILD_CLIENT_AUDIO` adds shared OpenAL Soft 1.25.2 through its C API. It
defaults to the graphical client setting and requires the codec target. The
dependency is C++20; all project-owned runtime sources remain C17. Headless
simulation and server targets do not link it.

CMake downloads the [official source archive](https://openal-soft.org/openal-releases/openal-soft-1.25.2.tar.bz2)
with SHA-256 `1dbaac44e7579d5bc8847ca8db4b2e8b9fd3961041f35ee20def4958301e1089`.
An offline override is `FETCHCONTENT_SOURCE_DIR_MC_OPENAL`. Its original tree
stays unchanged. A private build copy contains four hash-guarded explicit
aggregate initializations needed by Clang 18 with libstdc++ 13. Build metadata,
the C header, the four source files and their common filter header are checked
before applying those equivalent expressions. No diagnostic or sanitizer is
disabled.

Utilities, examples, installation, router, EAX and unrelated output transports
are disabled. Native Windows/Linux devices, loopback and embedded HRTF data
remain available. The shared runtime is copied beside Windows backend test
executables. Audio consumers use the matching C++ linker for the dependency's
sanitizer runtime; their own sources compile as C17.

[OpenAL notices](../licenses/openal-soft/NOTICE.txt) retain the LGPL, BSD, PFFFT,
fmt, GSL, bs2b and KEMAR terms and attributions. A future executable distribution
must account for the shared library's license and corresponding source. No
release package is created by the current build.
