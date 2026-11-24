# Source image downloads

RenderEngine now owns the actual `ji.h` String-to-`ek` map, real native `tj`
workers and concrete `rr` image processors. The factory and CPU image path use
the same source String and canonical image identities as their callers.

The interfaces are [image downloads](../../src/client/render_engine/image_download.h),
[ImageBufferDownload](../../src/client/render_engine/image_buffer.h) and the
[shared ImageIO reader](../../src/client/images/image_io.h). These operations
do not yet complete downloaded-image texture lookup/upload or the full texture
refresh method.

## Factory and source reference counts

`ji.a(String,nf)` captures `h.get5` in its local at13. A miss constructs `ek`
before `h.put32`; a hit increments the existing source `b` with Java int wrapping.
The return at52 is the original captured local. The first insertion therefore
returns NULL even though the map contains the new entry.

`ek` starts with image `a=NULL`, `b=1`, texture `c=-1` and uploaded flag `d=false`.
Its constructor creates `tj`, capturing the same nullable URL String, processor
and entry. `Thread.start29` occurs before map publication. The worker can
complete before the factory returns. A failed publication retains the started
child; retry must not replay construction or start another worker for that
invocation.

`ji.c(String)` decrements `b`; only exact zero reaches deletion/removal. Source
removal does not cancel or join the worker. An orphaned entry remains alive
through native worker/retained-call ownership and can still receive its result.
A later request for the same text creates a new entry and worker. The current
source removal body completes for the reached negative-`c` profile. Nonnegative
`c` retains the downloaded-texture deletion frontier at47.

String equality uses counted UTF-16 content, with a distinct source NULL key.
The map and worker retain actual `mc_player_string` occurrences. URL parsing
uses a separate native text projection; it does not replace or normalize the
source key. Native counts, source `b`, worker completion and map membership are
separate facts.

## Worker order and failures

| PCs in tj.run | Reached operation |
| --- | --- |
| 2/10 | Allocate and construct URL from the captured String. |
| 15/18/21 | Open connection, cast to HttpURLConnection, then assign the connection local. |
| 24/29/33 | Input enabled, output disabled, connect. |
| 37..51 | Read response code. Every 4xx disconnects at48 and returns silently at51. |
| 64/67/70 | With NULL processor: obtain input, ImageIO.read, publish its nullable result. |
| 85/88/91/96 | With processor: obtain input, ImageIO.read, call that processor even for NULL decoded input, publish its result. |
| 100/125 | Normal disconnect and source return. |
| 106/108/112 | Catch Exception, print its stack, then disconnect. |
| 118/120/124 | Finally disconnect for the other Throwable/printing-failure route, then rethrow. |

A 5xx response differs from 4xx: it reaches getInputStream and the source I/O
exception path. Unrecognized image data is a successful NULL decode; malformed
selected PNG data can throw. The scoped reader implements PNG8 RGB, RGBA and
palette handling. Other provider families, gray, 16-bit and Adam7 remain explicit
dependencies. It leaves the source InputStream open. The worker's source cleanup
is disconnect, not an added InputStream.close.

Exception printing at108 remains unfinished. A reached source Exception retains
its original site and typed cause, marks printing pending and stops there.
Native retirement can close transport resources, but does not acknowledge the
unreached source disconnect112 or mark source return125. Native allocation,
thread, bound and provider failures remain native failures; they are not invented
Java Exceptions passed into that catch. Failure before local assignment21 also
retains the distinction between the original error and a later null connection
in source cleanup.

The native transport is HTTP-only. HTTPS passes URL parsing but stops as a
native provider dependency at openConnection15; it does not invent a
ClassCastException at18 or printing108. This version's original skin/cloak
prefixes are HTTP. Broader TLS support and exact JDK transport timing are
unqualified. The shared transport's default 30-second limit, buffer/body bounds
and supported redirect behavior remain native policy, separate from source
worker completion.

Default factory limits are 128 retained entries, 128 map entries, 16 active
workers, 1,024 retained invocations and 16,384 URL code units. Removed or failed
entries remain retained until checked cleanup and still count toward that
budget. Lower configured limits keep their literal values, including zero.
The worker's scoped ImageIO bounds are 16 MiB encoded, 16 MiB decoded and 8 MiB
inflater storage. The shared HTTP defaults separately bound buffered intake to
64 KiB and body transfer to 256 MiB. These limits report reached native failures;
they do not supply a placeholder image or successful source return.

## ImageBufferDownload

`rr` construction calls Object only and leaves `a=NULL,b=0,c=0`. A NULL input
returns NULL without resetting those fields. A nonnull call stores `b=64` at9
and `c=32` at15 before constructing a new canonical 64×32 ARGB image at31.
Drawing clips or pads at (0,0), with no scaling. Field `a67` aliases the returned
image's actual pixel array; a private image occurrence keeps that array alive.

The selected Graphics SrcOver path quantizes each straight RGB channel through
8-bit premultiplication and 24-bit reciprocal unpremultiplication. It is not a
raw ARGB copy. At (0,0), after the later opacity mask, input `0x01ff8020` gives
`0xffffff00`, alpha127 gives `0xffff8120` and alpha254 gives `0xffff7f20`.
The integer implementation was checked against the pinned OpenJDK
[alpha tables](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/java.desktop/share/native/libawt/java2d/loops/AlphaMath.c)
and measured with the actual original `rr`.

Processing forces opacity in `[0,32)×[0,16)`. It scans the entire right half
`[32,64)×[0,32)`; any alpha below128 leaves that rectangle unchanged. Otherwise
it clears that rectangle's alpha while preserving RGB. It finally forces the
whole bottom half opaque. The final source scans perform no further writes.
Skin and cloak requests both construct this same `rr` family. There is no
later 64×64 skin remap or separate cloak conversion.

The output quota is the actual canonical image header plus 8,192 pixel bytes.
A reached quota failure at31 preserves the earlier dimensions. Processor
occurrences and process admission are atomic. Canonical image occurrence holds
are also atomic, so processors and callers can share the same image safely.
Published pixel content remains immutable unless callers provide the existing
exclusive mutation contract. Processor inspectors borrow mutable state only
under exclusive access or after the worker exits.

## Native lifetime and terminal cleanup

Each retained factory invocation records its own lookup, candidate, publication
and failure prefix. Worker errors and cleanup errors remain separate. Image
publication uses a native mutex; exit/stop and processor occurrences use atomics.
The render thread can observe image NULL while work is outstanding without a
network-long monitor.

Checked terminal retirement first refuses retained invocations or foreign Engine
occurrences. Once admitted, it permanently closes new image-download admission,
requests native stop, waits for exit, joins workers and retires their actual
connection, URL, processor and image occurrences. BUSY/failure keeps the same
graph for retry. This also applies to an Engine that never allocated a sidecar.
A refusal before admission neither latches terminal state nor stops workers.
Source release alone never performs this cancellation/join sequence.

## Qualification and remaining work

The updated Windows native RR control passes in 0.04 s, including 262,144 output
pixels checked against the original observer hash, alpha thresholds, clip/pad,
NULL preservation, quota prefixes, independent image lifetime and 40,000
concurrent processor occurrence pairs. Another control shares a live image
while RR replaces its alias, with 40,000 concurrent image retain/release pairs
and 10,000 inspections on the calling thread. The authored original observer covered
524,288 full output pixels across TYPE_INT_ARGB and TYPE_4BYTE_ABGR, with all
65,536 alpha/channel pairs in each format. See the published
[observer and provenance](../../tests/reference/client/image_buffer/README.md).

The latest selected Windows native factory/worker control passes in 0.50 s with
14 real GETs across 12 authored routes and six synchronization barriers. It
exercises streaming/decode/processing, cache hits, running-entry removal,
failed map publication, HTTP-only classification and terminal admission. It
uses no original host or original image assets.

GCC13 and Clang18 Debug ASan/UBSan runs each pass ten public headless cases and
two private CPU-wrapper cases, 24 total without failures or skips. The wrappers
compile the actual Engine/Font/RR source closure with pinned dependencies;
their RR and Engine CPU controls exercise no SDL, window or graphics context.
Leak detection is enabled. These results qualify the atomic image-sharing
controls, rather than a complete Linux graphical runtime. The later final
selection adds the three Player action/tick cases per compiler, for 30 total
sanitized passes. The complete local Windows selection passes 482 cases.

The downloaded-texture query `ji.a(String,String)` still needs its reached `c`
registration/re-upload and `d` stores. Refresh's `h.values` loop must clear each
actual `d` without redownloading, alongside the rest of source `ji.b()`.
Nonnegative texture deletion, Exception printing and the remaining providers
also retain explicit frontiers. These controls do not establish original
HTTP/thread timing, full image-provider coverage or a playable World frame.
