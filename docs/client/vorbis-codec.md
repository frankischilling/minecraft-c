# Vorbis codec policy

`mc_vorbis_codec` implements the bundled CodecJOrbis page and buffer policy
using low-level libogg/libvorbis synthesis. It opens actual retained file/MUS
identities through the existing sound stream owner. It uses no JVM, device or
libvorbisfile reader. The separate `mc_vorbis_decoder` remains a generic
sequential decoder with different chaining and read policy.

Initialization divides the actual streaming-buffer setting by two. Header
processing keeps the original final refill even after both secondary headers
arrive. Each read processes one already buffered page. An absent or corrupt
page sets EOS without first refilling. Page submission precedes the granule-zero
EOS check, which stops before audio packets. The first logical EOS ends the
codec; chained streams do not automatically restart it.

Each packet allocates conversion storage. The original trim helper returns
null when the converted array fills exactly, discarding that array. Shorter
zero output remains a distinct nonnull empty array. Conversion uses binary32
times 32767, Java truncation/saturation and signed16 clamping, with interleaved
little-endian bytes and no dither. The first known granule is assigned without
libvorbis's additional first-granule trimming, matching the JOrbis transition.
This coordination changes no PCM sample value.

Tiny buffers can leave PCM unread between packets. A bounded owned queue
retains its exact float bits before consuming libvorbis's view to admit the
next block. Conversion reads the saved prefix before new frames, and a final
granule trim can remove the saved suffix. Saved PCM is bounded to 16 MiB, with
at most twice that storage during replacement. Cleanup releases it.

`read_all` reads once, then appends another page before checking the signed
maximum-file-size setting. That setting is a stopping threshold, not a
truncation length. Reaching it can leave EOS false. Results distinguish an
absent SoundBuffer, a present buffer with null data and present empty data.
Returned data belongs to the caller and survives later reads, reinitialization,
cleanup and codec retirement.

Cleanup retains initialized/EOS flags, format and source count/index. Native
retirement additionally releases comments and the actual MUS file handle.
Repeated cleanup is safe. Reads after cleanup with EOS false fail safely.
Input, result and per-call work ceilings are explicit native bounds. Negative
sync-fill states are avoided rather than passed into unsafe native indexing.
These resource policies do not establish general malformed-input equivalence.

## Original comparison

The supplied archive and unchanged CodecJOrbis/JOrbis classes remain the
authority. The [reference record](../reference/client-sound.md) pins those
methods and earlier observations. A new reflective probe changes no decoder
fields. It compares initialization, each read, repeated EOF, post-EOS readAll
and cleanup using frozen, project-authored Ogg inputs.

Fresh Windows original/native runs match all 14 declared page/buffer profiles:
five streams, five fresh readAll thresholds, three tiny-buffer controls and
an end-granule trim reaching saved PCM. Fresh sanitized Ubuntu GCC and Clang
profiles pass those same 14 comparisons. Queue-active controls contain silence;
nonzero queued PCM order still needs independent original observation.
The stereo output matches all 21,248 PCM bytes. Exact mono PCM still differs:
four bytes in the short stream and 33 in the long stream. The chain retains
the short-mono difference and stops after its first stream. No sample tolerance,
correction or omitted suffix is used. Policy equality and PCM equality are
separate results; full original PCM correspondence remains unfinished.

Public `client.vorbis_codec` checks the frozen inputs, retained result ownership,
full-buffer trimming, readAll thresholds, cleanup, reinitialization and native
failure limits. Its input manifest records the authored encoder recipes and
hashes; none of those files contains original game audio. Public encoder
regressions continue to generate fresh inputs independently.

The local comparison tool is `tests/tools/client/compare_vorbis_policy.py`.
It requires the verified client archive, JDK17 and the native test executable.
Its receipt reports complete PCM hashes and exact differences separately.
`--require-pcm` also fails on the known synthesis mismatch. Policy success
does not establish device playback or complete original audio behavior.
