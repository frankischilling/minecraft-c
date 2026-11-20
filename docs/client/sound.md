# Client sound resources

The sound foundation owns SoundPool entries, resolved file identities and binary
streams. [SoundManager](sound-manager.md) owns shared setup, nested settings saves
and ordered playback requests; the [Vorbis codec](vorbis-codec.md) implements
the original page/buffer policy. Device channels, startup registration and the
actual GUI/WorldAccess routes remain required. The [source contract](../reference/client-sound.md) records the
original owners and ordering.

## SoundPool ownership

`mc_client_sound_pool` creates a distinct ordinary Random through the caller's
shared `mc_java_random_profile`. It never resets that profile or accepts a
caller-selected result. Entries own copies of their counted UTF-16 logical
names, native UTF-8 filenames and resolved URL paths. Duplicate registrations
remain distinct weighted entries in the group and full insertion-order list.

Registration has two stages. `mc_sound_pool_begin` computes the group key and
publishes a new empty group before File/URI/URL resolution. An owned registration
keeps the pool exclusive until commit or retirement. Commit copies a real
resolved identity, appends the group list, appends the full list and increments
the count in that order. A failure retains the reached group or entry/list
prefix. A failed commit cannot be retried through the same token. The concrete
`mc_sound_pool_add_file` route performs both stages around the native resolver.

Keys use the prefix before the first dot, optional trailing decimal-unit
stripping, then slash-to-dot replacement. Character classification uses the
selected JDK17 BMP profile already checked by the shared integer parser. It is
not an ASCII-only rule or a claim about every historical JVM Unicode version.
Missing-dot, stripped-empty and null names retain their distinct source failure
sites. Streaming can disable stripping, preserving names such as `13`.

Missing keys and an empty full list return no entry without a Random draw.
A present group always calls `nextInt(size)`, including size one. A retained
empty group therefore reaches the source bound failure. Native inspection
indices expose group insertion order; they do not claim Java HashMap iteration
order. Every view remains borrowed until pool retirement.

## Files, URLs and MUS streams

`mc_sound_file_resolve` accepts an already absolute, bounded, strict UTF-8
native path. It normalizes the selected File profile's separators and trailing
separator, retains dot segments and queries the actual directory kind for the
URL's trailing slash. A missing file still has a valid identity. Relative paths,
ill-formed UTF-8 and Windows device namespaces are outside this native seam.
The imported resource directory already supplies absolute filenames; general
Java File construction, drive-relative paths and arbitrary UTF-16 filenames
remain separate work.

The retained counted UTF-16 URL path uses URI path quoting. Spaces become
`%20`, literal percent signs become `%25`, and ordinary Unicode remains Unicode.
This differs from form encoding and from taking a raw native basename. Playback
opens the selected entry's retained identity directly, preserving its original
URL path even if a later filesystem query would produce a different one.

`mc_sound_stream` owns one native descriptor for a regular binary file. Windows
permits concurrent writers, matching the existing FileInputStream adapter
policy. Direct descriptor reads observe a concurrent write without a separate
stdio read-ahead cache. The input limit is checked against the opened handle
and enforced during reads; a growing file can reach a terminal native limit. A partial I/O prefix
advances MUS state before the later error is reported.

MUS mode hashes the last segment of the retained URL path using wrapped Java
String arithmetic. For each byte actually read, the original signed shift/XOR
and signed decoded-byte recurrence advance that state. It never tries alternate
seeds until an Ogg header appears. Single-byte reads retain the original signed
return quirk: decoded `FF` has value `-1`, separately identified from native EOF.

Source close and native retirement are distinct. The original MUS wrapper
inherits the no-op `InputStream.close()`, so closing that facade leaves it
readable. Raw file close closes once. Native retirement always closes the actual
handle, including after MUS source close, EOF or failure.

## Sequential Vorbis decoding

The decoder increment uses the pinned C interfaces from libogg 1.3.6 and
libvorbis 1.3.7. CMake keeps these outside the shared runtime and dedicated-server
link graph. `MC_BUILD_CLIENT_SOUND` defaults to the graphical-client setting;
it can be enabled with `MC_BUILD_CLIENT=OFF` for decoder tests without a display.
Offline overrides are `FETCHCONTENT_SOURCE_DIR_MC_OGG` and
`FETCHCONTENT_SOURCE_DIR_MC_VORBIS`. Codec corrections apply only to private
build copies; both supplied override trees stay unchanged.

The Ogg copy guards page synchronization before evaluating a pointer offset.
A fresh sync owner has a null buffer and zero counters; adding its zero offset
to null is still undefined C. Keeping that page pointer null preserves the
normal need-more-data return. The pristine source hash and exact expression
are checked before the correction.

That copy also corrects one libvorbis nonseekable chain branch. The header reader
has already submitted the final header page when a new logical stream opens.
The caller now returns to its packet loop instead of submitting that page
again, which otherwise reports a false packet hole. The source hash and exact
branch spelling are checked before this change. Genuine packet holes remain
decoder errors. Both libraries retain their BSD license notices under
`licenses/`.

The UNIX build probes for `alloca.h` and enables libvorbis's existing include
guard on its three targets. Upstream CMake omits that check; strict C17 Clang
otherwise rejects implicit `alloca` calls. No warning suppression or upstream
source change is used for this header configuration. Both Linux builds at
`d9456c2` reach the test suite with this correction.

The private copy also corrects three operations reached by the sanitized
authored-fixture test. Bark packing multiplies a bounded signed index by 65536
instead of left-shifting a negative value. Noise interpolation reuses the last
band when its next-band weight is zero, avoiding an out-of-range read that C
still evaluates. Decode search-table construction converts its index to
`ogg_uint32_t` before shifting bits into the high word. Exact upstream source
hashes and expression spellings guard each change. Sanitizers remain enabled
on Ogg and all three Vorbis targets.

`mc_vorbis_decoder` opens the retained file identity through sequential callbacks.
Each read makes at most one `ov_read_float` call and publishes interleaved signed
16-bit little-endian PCM with its actual logical link, channel count and sample
rate. Conversion multiplies each binary32 sample by 32767, applies Java's
float-to-int truncation and saturation, then clamps to signed16. It adds no
dither or gain. This follows the original conversion policy; libvorbis's integer
API instead scales by 32768 and rounds. Zero-valued PCM cannot expose that
difference. The floating synthesis engines still need direct comparison.

Changed links recheck their format. A new link can return more channels than the
request's previous format, so the decoder retains borrowed planar samples and
emits complete frames that fit the output capacity. Pending frames drain before
another library read; passive state reads leave those samples intact. Retirement
can discard a pending suffix after a native prefix limit.
Decode and stream errors are terminal;
there is no packet-hole retry. EOF returns an empty view and remains stable on
later reads. Retirement clears initialized decoder state once, source-closes
the stream and always retires its native handle, including the MUS facade.

Native bounds admit at most 256 MiB of input and a 1 GiB PCM prefix. A read with
fewer than 510 bytes left in its PCM budget fails before entering the library,
since 510 bytes hold one maximal 255-channel frame. The ceiling need not fill
every remaining byte or prove EOF. These resource limits are native safety
policy, not measured original codec behavior.

## Verified native scope

The Windows MSVC 19.51.36260.0 x64 Debug headless run passes all six
selected tests with no failures or skips:

| Test | Checks |
| --- | ---: |
| `client.sound_pool` | 18,654 |
| `client.sound_file` | 581 |
| `client.vorbis_decoder` | 579 |
| `client.vorbis_nonzero` | 5,480 |
| `core.java_integer_parse` | 43 |
| `core.java_random_profile` | 191 |

Pool checks cover independent LCG selection, duplicates, UTF-16 names, allocation
prefixes and bounds. File checks preserve two literal inputs and cover URL/MUS
vectors, concurrent writes, close behavior and pool-selection-to-stream ownership.
`client.vorbis_decoder` retains seven authored inputs and four exact zero-PCM outputs:
6,400-byte mono and MUS, 19,200-byte stereo and a 25,600-byte chain. They also
exercise malformed/truncated headers, a real missing-page error, prefix limits
and cleanup. The chain changes from mono 32 kHz to stereo 48 kHz.

All seven supervised stages and eleven final audits pass. Independent file-only
readback consumes 14,431 retained source, input, product, stream, tool and SDK
files, joining all 1,406 physical source files, including 1,236 public members
and 1,056 non-Markdown inputs. All 658 C/header files pass clang-format 18.1.3.
The terminal report's SHA-256 is
`1a2a9ae0aa7e1fc4e20b9e8c764d1776b28faf10dabb4375a46b37ec334665cf`.
Later result-documentation changes are recorded separately from that snapshot.

The decoder tests include 24 literal nonzero/special-value vectors through the
production converter, pending-plane interleaving and an actual 7,000-byte
chain-budget case. Helper tests drain a nonzero planar suffix across calls;
the encoded chain case checks a bounded stereo prefix and terminal limit.
It does not prove a later successful decoder drain of a pending suffix. Those
encoded fixtures are silence; the separate nonzero regression below exercises
real encoded audio. The separate original comparison below records four
one-step sample differences in a nonzero mono stream.

Earlier failed runs remain failed: private-copy CRLF drift, a rejected MSVC
fixture cast, stale stdio reads and the chain's false packet hole. Their captures
remain preserved. The final run uses explicit LF copies, unsigned byte fixtures,
native descriptors and the exact chain correction above; assertions and strict
warnings remain enabled.

The [nonzero sound checkpoint `aa39f8f`](https://github.com/frankischilling/minecraft-c/actions/runs/40190426923)
passes all four hosted jobs: 229 Windows, 233 GCC and 233 Clang cases, with 695
complete Start/Passed pairs and no failures, skips or missing tests. All 658
C/header files, controller mirrors and 556 positive/2,011 negative lifecycle
controls pass. All four checkouts use synthetic merge `839f22e`, whose complete
tree equals the head. Independent readback reconstructs 331 trees/1,566 entries
and joins all 1,236 literal public files and 1,056 Windows-tested non-Markdown
inputs.

The earlier [sound checkpoint `e10a5e2`](https://github.com/frankischilling/minecraft-c/actions/runs/40171237434)
passes all four hosted jobs: 228 Windows, 232 GCC and 232 Clang cases, with 692
complete Start/Passed pairs and no failures, skips or missing tests. Formatting
covers 657 C/header files; controller mirrors and all 556 positive/2,011 negative
lifecycle controls pass. The four checkouts share synthetic merge `ec63077`,
whose complete tree equals that head. Independent readback reconstructs 331
trees and joins all 1,234 literal source files and 1,054 Windows-tested
non-Markdown inputs. This includes the Ogg and Vorbis corrections above.

The earlier [sanitized checkpoint `184a6ce`](https://github.com/frankischilling/minecraft-c/actions/runs/40169607338)
passes all 228 Windows and 232 GCC cases, plus 657-file formatting. Clang passes
231 of 232 tests. Its decoder check exits 0 but emits a UBSan report for
libogg's null-buffer page offset, so the strict stderr assertion rejects it.
An exact-commit Ubuntu Clang 18 reproduction confirms that same path before
the Ogg correction above. This failed run and its complete chronology remain
preserved; the success-only hosted auditor was not executed.

The earlier [PCM checkpoint `d9456c2`](https://github.com/frankischilling/minecraft-c/actions/runs/40165382172)
passes all 228 Windows cases and 657-file formatting. Both Linux builds finish;
each passes 231 of 232 tests and fails only `client.vorbis_decoder`, during
fixture creation. Local Ubuntu GCC ASan/UBSan reproduction identifies the
negative Bark shift, then the last-band table read. After those corrections,
the full public driver creates its inputs and exposes the decode search-word
shift. Each failed source/build/capture family remains preserved. The driver
now includes the actual bounded child stderr in its failure message without
changing its success conditions.

With all three arithmetic/bounds corrections, a fresh Ubuntu x86-64 GCC 13
Debug build passes the complete public decoder driver under ASan/UBSan:
563 checks, seven authored inputs and four exact PCM outputs. Both native
children exit naturally with empty stderr. Independent file-only readback joins
all 1,234 copied source members, retained inputs/outputs, closed streams and
the supervisor's closed PIDFD ledger. This is a focused decoder diagnostic,
not a full Linux suite or original PCM comparison. Windows has 16 additional
platform-specific decoder checks.

After the Ogg correction, a fresh Ubuntu Clang 18 ASan/UBSan build also passes
all 563 decoder checks with empty child stderr. Its independent file readback
passes against the copied source and complete retained captures. The prior
exact-commit Clang failure stays preserved separately.

`client.vorbis_nonzero` encodes four deterministic inputs from authored rational
PCM: mono 32 kHz, distinct-channel stereo 48 kHz, their exact concatenated chain
and a longer mono 44.1 kHz stream. It uses ordinary audio-page output and forces
only the headers. The production decoder checks format changes, complete frame
counts, repeated EOF, progress and owner retirement. The driver preserves all
four inputs and four nonzero PCM outputs (8,192, 19,200, 27,392 and 160,000 bytes),
checks distinct stereo planes and joins chained PCM to the two separate decodes.
These lossy codec contracts do not assert identity with the supplied PCM.

This regression first fails under Clang UBSan at libvorbis `floor1.c:820`:
packing a negative floor deviation left-shifts a negative integer. The pinned
private copy replaces that shift with multiplication by two. The reached
deviation is between -128 and -1, so the product is defined and preserves the
intended zigzag value. The official dependency source remains unchanged.

A fresh Ubuntu Clang 18 ASan/UBSan build passes 5,469 nonzero checks and all
563 existing decoder checks, with empty child stderr. Independent file readback
checks the 1,236-member source snapshot, all retained inputs/PCM, closed process
ledgers and complete dependency copies. Only the five reviewed Vorbis files
and one Ogg file differ from their supplied sources. The fresh Windows profile
above passes 5,480 checks in the same regression; its complete capture and
dependency readback also passes. Neither run compares original PCM.

The earlier [sound checkpoint `d4838ea`](https://github.com/frankischilling/minecraft-c/actions/runs/40162747830)
passes all 228 Windows cases and 655-file formatting. Both Linux builds fail
on the missing `alloca` declaration or link; their test stages never execute.

To run all six public contracts in a headless build:

```sh
cmake -S . -B build-sound -DMC_BUILD_CLIENT=OFF -DMC_BUILD_CLIENT_SOUND=ON
cmake --build build-sound --config Debug --target mc_test_sound_pool mc_test_sound_file mc_test_vorbis_decoder mc_test_vorbis_nonzero mc_test_java_integer_parse mc_test_java_random_profile
ctest --test-dir build-sound -C Debug --output-on-failure -R '^(client\.(sound_pool|sound_file|vorbis_decoder|vorbis_nonzero)|core\.(java_integer_parse|java_random_profile))$'
```

These are native contracts. They execute no original game classes, original
audio assets or audio device. Original PCM and device comparisons remain
separate work.

An [arranged original mono control](../reference/client-sound.md#arranged-original-codec-observation)
now records a source-policy difference. CodecJOrbis initializes, then returns
null and sets EOS on the authored split-page input; its later readAll returns
a SoundBuffer with null data. The generic native decoder emits 6,400 PCM bytes
from those same encoded bytes. The original's granule-zero EOS branch and the
captured buffered page explain this edge. Floating synthesis was not compared.
The separate [source-policy codec](vorbis-codec.md) now reproduces that
granule-zero stop while retaining the generic decoder's independent behavior.

A separate [nonzero original observation](../reference/client-sound.md#nonzero-original-pcm-observation)
returns the same 4,096 mono frames as native decoding. Four samples differ by
one signed16 step. Independent readback verifies the complete outputs and all
retained evidence; exact equality fails without relaxing tolerance. The
conversion-entry floats were not captured, and the numerical cause remains
unresolved.

Complete audio still needs WAV behavior, original JOrbis/native PCM and error
comparisons, canonical resource and registration order, queued backend commands,
normal/streaming channel ownership, pitch/gain/listener behavior and controlled
device measurements. Music/sound option actions now reach the concrete
SoundManager and its two nested settings saves. Backend calls remain an
explicit boundary; device playback and real client adoption are unfinished.
