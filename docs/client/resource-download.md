# Resource download thread

The native `cz` owner implements the source constructor, local recursive scan,
two-pass resource listing, typed sound registration, download loop and source
cancellation. It uses the real App, canonical Files and the same SoundPool
entries. Native allocation and thread retirement are separate from source
constructor/run completion.

The source endpoint remains `http://s3.amazonaws.com/MinecraftResources/`.
A contemporaneous research GET returned S3 `404 NoSuchBucket`; this is a host
observation, not a replacement endpoint or a permanent availability promise.
The source catch scans local resources before printing its exception.
`Throwable.printStackTrace` remains an explicit reached dependency at entry143,
hr157 and run221; a failed directory constructor also stops at new
RuntimeException59. These frontiers do not report a completed source handler.

File/URI/URL CPU ownership has no HTTP, XML, display or audio-device link.
Libcurl8.23.0 supplies the real HTTP pull stream; Expat2.9.0 supplies the selected
ordinary XML tree. Their archives are pinned in the client-only CMake module.
Core, World and Files do not depend on either transport. The existing canonical
DataInputStream owner supplies its actual byte80/char80/byte8 arrays and one
bulk read; the downloader never invokes the NBT parser or substitutes tags.
Binary output uses the canonical FOS/DOS kernel and preserves partial writes.

The worker's native mutex serializes its Files, arrays, streams and retained
prefixes. The File namespace retains its existing atomic lifetime/cache guards;
immutable Name holds are atomic, including cached root aliases shared across
distinct Files. This does not make mutable WorldInfo/Data/Player objects,
document backing lifetimes or arbitrary same-File calls concurrent. Their
existing external lifetime/exclusive-access contracts still apply. SoundPool
selection and typed registration share one native monitor; legacy split-token
registration keeps its exclusive contract. Pool entries contain immutable
copies of the real resolved paths, not a mutable File handed to the foreground.

A registered App has a distinct native resource-thread borrow, with no counted
App-to-thread cycle. Stop/join and all fallible prefix cleanup must finish before
that borrow is released. Native retirement never writes source c/J, claims a
source close/finally, or deletes a partial download. Source b() only sets c.
Source run success, asynchronous Thread.start success, native completion and
cleanup success are separate observations.

Supported ordinary profiles use the actual default factory, bounded UTF8/UTF16
XML without DTD/external entities, canonical absolute File paths, and ordinary
ASCII HTTP resource URLs. Unimplemented provider/encoding, URL corner forms,
TLS and exception-printing work remains explicit. No new original comparison,
complete startup or gameplay claim follows from these source bodies.

Authored controls cover primitive bounds, raw/failed constructor prefixes,
existing regular-file resources, same-owner DIS aliases/null/zero-length read
priority, partial binary effects, shared Name/cache lifetimes, pool overlap,
App retirement and real local HTTP/XML/download/cancel effects. CTest starts an
ephemeral loopback peer for the HTTP control, including a 65,539-byte transfer,
redirect and missing-resource response. It never contacts the public resource
endpoint. Filesystem cases use fresh temporary directories and remove them when
the native process exits. Native tests and original-JVM observations retain
their separate scopes.

## Selected native qualification

Six Windows MSVC Debug CTests pass for the resource owner, real loopback HTTP,
File/URL/output leaves, DataInputStream bulk reads, sound-pool registration and
shared immutable Name lifetime. The HTTP case verifies every byte of the
65,539-byte payload, a redirect, a missing resource, XML node kinds, download
output and cancellation before source close. The Name case runs two workers
through 20,000 shared aliases. Strict enclosing-owner retirement preserves its
slot when another counted occurrence exists.

These authored controls do not establish original worker, public-endpoint or
Linux correspondence. The constructor's reached RuntimeException allocation
and the worker's exception-printing paths remain explicit unfinished calls.
