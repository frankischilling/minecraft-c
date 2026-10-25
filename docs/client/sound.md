# Client sound resources

The sound foundation owns SoundPool entries, resolved file identities and binary
streams. It does not yet play sound. SoundManager, its shared backend, device
channels, startup registration and the actual GUI/WorldAccess routes remain
required. The [source contract](../reference/client-sound.md) records the
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
`FETCHCONTENT_SOURCE_DIR_MC_VORBIS`. Vorbis build compatibility changes apply
only to a private build copy; the supplied override tree stays unchanged.

That copy also corrects one libvorbis nonseekable chain branch. The header reader
has already submitted the final header page when a new logical stream opens.
The caller now returns to its packet loop instead of submitting that page
again, which otherwise reports a false packet hole. The source hash and exact
branch spelling are checked before this change. Genuine packet holes remain
decoder errors. Both libraries retain their BSD license notices under
`licenses/`.

`mc_vorbis_decoder` opens the retained file identity through sequential callbacks.
Each read makes at most one `ov_read` call and publishes interleaved signed
16-bit little-endian PCM with its actual logical link, channel count and sample
rate. Changed links recheck their format. Decode and stream errors are terminal;
there is no packet-hole retry. EOF returns an empty view and remains stable on
later reads. Retirement clears initialized decoder state once, source-closes
the stream and always retires its native handle, including the MUS facade.

Native bounds admit at most 256 MiB of input and a 1 GiB PCM prefix. A read with
fewer than 510 bytes left in its PCM budget fails before entering the library,
since 510 bytes hold one maximal 255-channel frame. The ceiling need not fill
every remaining byte or prove EOF. These resource limits are native safety
policy, not measured original codec behavior.

## Verified native scope

The Windows MSVC 19.51.36260.0 x64 Debug headless run passes all five selected
tests with no failures or skips:

| Test | Checks |
| --- | ---: |
| `client.sound_pool` | 18,654 |
| `client.sound_file` | 581 |
| `client.vorbis_decoder` | 469 |
| `core.java_integer_parse` | 43 |
| `core.java_random_profile` | 191 |

Pool checks cover independent LCG selection, duplicates, UTF-16 names, allocation
prefixes and bounds. File checks preserve two literal inputs and cover URL/MUS
vectors, concurrent writes, close behavior and pool-selection-to-stream ownership.
Decoder checks retain seven authored inputs and four exact zero-PCM outputs:
6,400-byte mono and MUS, 19,200-byte stereo and a 25,600-byte chain. They also
exercise malformed/truncated headers, a real missing-page error, prefix limits
and cleanup. The chain changes from mono 32 kHz to stereo 48 kHz.

All seven supervised stages and eleven final audits pass. Independent file-only
readback consumes 14,273 retained source, input, product, stream, tool and SDK
files, joining all 1,402 physical source files, including 1,232 public members
and 1,052 non-Markdown inputs. All 655 C/header files pass clang-format 18.1.3.
The terminal report's SHA-256 is
`f3df20b9c88d5cbf09b5e8ec66e2890b3e57cb03707c0959651eb918fba2c10e`.
Later result-documentation changes are recorded separately from that snapshot.

Earlier failed runs remain failed: private-copy CRLF drift, a rejected MSVC
fixture cast, stale stdio reads and the chain's false packet hole. Their captures
remain preserved. The final run uses explicit LF copies, unsigned byte fixtures,
native descriptors and the exact chain correction above; assertions and strict
warnings remain enabled.

To run the same five public contracts in a headless build:

```sh
cmake -S . -B build-sound -DMC_BUILD_CLIENT=OFF -DMC_BUILD_CLIENT_SOUND=ON
cmake --build build-sound --config Debug --target mc_test_sound_pool mc_test_sound_file mc_test_vorbis_decoder mc_test_java_integer_parse mc_test_java_random_profile
ctest --test-dir build-sound -C Debug --output-on-failure -R '^(client\.(sound_pool|sound_file|vorbis_decoder)|core\.(java_integer_parse|java_random_profile))$'
```

These are native contracts. They execute no original game classes, original
audio assets or audio device. Linux qualification and original PCM comparisons
remain separate work.

Complete audio still needs WAV behavior, original JOrbis/native PCM and error
comparisons, canonical resource and registration order, queued backend commands,
normal/streaming channel ownership, pitch/gain/listener behavior and controlled
device measurements. Options effects require the real shared SoundManager and
its two nested settings saves; an acknowledging callback does not satisfy that
dependency.
