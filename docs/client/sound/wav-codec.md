# WAV codec

[`mc_wav_codec`](../../../include/mc/client/sound/wav_codec.h) owns an actual
resolved sound file and its sequential raw stream. It implements the supplied
`paulscode.sound.codecs.CodecWav` read policy and the selected JDK17 WAVE
providers in C17. The runtime needs no JVM or audio device. The original
comparison helper runs separately and requires a locally supplied client JAR.

The original calls `AudioSystem.getAudioInputStream` directly. It does not
request conversion to signed 16-bit PCM. The native format therefore retains
the source encoding, channel count, sample size, frame size, binary32 sample
and frame rates, frame length and little-endian flag. Extensible WAVE also
retains valid sample bits and the numeric channel mask. A device backend must
decide whether it supports that format before submitting the returned bytes.

| WAVE tag | Encoding and provider rules |
| --- | --- |
| 1 | Eight-bit PCM is unsigned; other admitted sample sizes are signed. Frame size is `ceil(bits / 8) * channels`, regardless of the header's block alignment. |
| 6, 7 | A-law and mu-law bytes retain their encoding and sample size. Frame sizing follows the classic PCM provider. |
| 3 | IEEE float bytes retain the header's block alignment and unsigned sample rate. |
| 65534 | Extensible PCM or float requires the matching subtype GUID, extension size 22 and valid bits no greater than sample bits. Frame size retains the header's block alignment. |

Classic providers interpret channels and sample bits as signed 16-bit values
and sample rate as a signed 32-bit integer before converting to float. Float
and extensible providers use unsigned header values. This distinction includes
the observed `0xffffffff` sample-rate cases. Only a 16-bit sample size enters
CodecWav's short-copy conversion. On the selected little-endian target it
retains each complete short's bytes; a trailing unmatched byte becomes zero.
Other sample sizes retain their raw bytes. No A-law, mu-law or float-to-PCM
conversion occurs in this owner.

Initialization first clears the initialized flag and cleans prior resources.
Only successful setup publishes the stream and format, resets EOS to false
and sets initialized to true. Source unsupported-audio and I/O failures caught
by initialization return a false initialization result with their source
diagnostic. Native allocation, input and work limits have separate statuses.
Native ownership retires a failed setup's file handle.

A streaming read uses the configured byte count, then AudioInputStream's
whole-frame rounding. If the remaining capacity cannot hold a frame, the
original treats the resulting zero read as EOS even when more data remains.
An exactly filled result leaves EOS false until a later read. A zero configured
size returns an absent result without changing EOS; a negative size reaches
the original array-size exception. A read I/O failure sets EOS and discards
that call's partial result. An absent buffer and a present empty buffer remain
different outcomes.

`readAll` first computes Java's wrapped
`channels * (int) frameLength * sampleBits / 8`. A positive result allocates
that entire original length, reads from the current position and preserves the
unwritten zero suffix. This path ignores the configured chunk and maximum
sizes. It closes the underlying stream while retaining the stream field and
format, and does not change EOS. The chunk/maximum loop is reached only when
the wrapped size is nonpositive. Its empty-data path can reach an uncaught
null-conversion exception. These outcomes are represented as source exceptions,
separately from native resource failures.

Cleanup clears the stream and format but retains initialized and EOS. Buffer
results belong to the caller and survive cleanup, reinitialization and owner
retirement. Zero-initialize output buffers and release present results with
`mc_wav_codec_buffer_free` before reusing them. Calls require exclusive access;
the state view exposes file position, buffered bytes, stream closure and a
terminal native failure without replacing the real stream.

Header parsing follows the selected providers' 8 KiB buffered-input behavior.
Unknown chunks and odd chunk padding are skipped through the real input.
The classic provider does not skip an odd `fmt ` pad. A large chunk before
`fmt ` can invalidate provider mark/reset and cause initialization I/O failure;
the authored 8,192-byte JUNK case exercises that boundary. Float and extensible
data also respect the RIFF container limit. Parsing, allocation and read work
remain bounded by caller limits: input is at most 256 MiB, one result at most
1 GiB, a frame at most 1 MiB and a call at most 1,048,576 work steps. These
native ceilings can stop malformed-input behavior that would allocate or loop
without a bound in the original.

The supplied archive is 1,465,375 bytes, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
[`pins.json`](../../../tests/reference/sound/wav/pins.json) records the complete
class identities and all 64 Code-body hashes, stack/local bounds and exception
tables across CodecWav, ICodec, SoundBuffer, SoundSystemConfig and
SoundSystemLogger. It contains no original instruction bytes. Selected
CodecWav bodies are:

| Method | Code bytes | SHA-256 |
| --- | ---: | --- |
| `initialize(URL)Z` | 92 | `141ddaf1de19bd8ae94f9378faf2fdc87cfa6effb15922cc3de9ad882581d715` |
| `read()SoundBuffer` | 164 | `19cf7a035ec7e9dc306e01e389cd244faa4d876bbe1b064270368ecc1fda42a6` |
| `readAll()SoundBuffer` | 332 | `5689f733b97fa317589bcb6b2862556c4483c4b3e8e1b821328115d31a49826e` |
| `cleanup()V` | 24 | `a477156233fc3cda5d99b466a36a9059ab5dd9b92ec2c04532297e7dca137705` |

The positive-size `readAll` branch is established by the actual instructions:
the size arithmetic is at PCs 37 through 58, allocation at 84, read at 105 and
the jump at 151 goes to 285. It skips the chunk loop, whose configuration reads
are at 166 and 185. Conversion is at 300 and stream close at 321. A decompiler's
structured loop alone does not establish that branch's behavior.

The public comparison uses
[`WavOriginalProbe.java`](../../../tests/reference/sound/wav/WavOriginalProbe.java)
to call the unchanged supplied classes through an isolated loader. The helper
constructs the real logger and uses the real configuration setters. It reads
the original format, buffer, initialized and EOS fields after calls without
installing decoder state or expected bytes. Its compilation has an empty
classpath; the original archive is supplied only to the later runtime loader.
Each run verifies archive, class and Code pins, records the observed JDK class
resources and hashes the helper, tools, native executable and authored inputs.
The JDK resource hashes identify that run's provider implementation; they do
not establish every historical JVM's behavior.

[`fixtures.py`](../../../tests/reference/sound/wav/fixtures.py) generates 24
authored inputs and 60 call profiles. They cover PCM sample sizes 7, 8, 12, 16,
24 and 32; stereo with contradictory block alignment; A-law and mu-law; float
32 and 64; extensible PCM and float; odd JUNK chunks; signed and unsigned rate
interpretation; empty, truncated and partial-frame data; the buffered mark
boundary; odd `fmt ` padding; and an unsupported tag. Profiles include fresh
readAll, repeated reads followed by readAll, zero/negative/subframe/exact/long
streaming sizes, and a zero readAll maximum. No original sound assets or
precomputed decoded outputs are required.

Each successful-init trace performs either up to ten reads, stopping at first
EOS, or one fresh readAll. It then performs another read, readAll and cleanup.
Failed initialization performs cleanup only. Every row contains the operation
and sequence, initialized, EOS, buffer presence and length, uncaught exception
name, encoding, channels, sample bits, frame size, raw sample-rate float bits,
endianness and frame length. Every present returned array is saved in full.
The comparator requires exact trace equality and exact byte equality, including
zero padding and present empty results. Original logger diagnostics remain in
the retained raw output and are reported separately from trace equality.
Extensible numeric properties and native ownership counters have native tests;
they are outside this 14-field original trace.

Run the public input/protocol checks without the original archive or Java:

```text
python tests/reference/sound/wav/test_protocol.py
```

Build `mc_test_wav_codec` in a normal CMake test build. Its direct trace command
is `mc_test_wav_codec INPUT OUTPUT_DIR trace read|all STREAM_SIZE CHUNK_SIZE
MAX_SIZE`; both paths must be absolute and the output directory must exist.
For an original comparison, select a little-endian JDK17 and run from the
repository root, replacing the archive and executable paths:

```text
python tests/reference/sound/wav/compare.py --jar /absolute/b1.7.3.jar --java java --javac javac --native /absolute/mc_test_wav_codec --work-dir .local/wav-reference
```

The tool creates a unique capture directory and prints its `comparison.json`
path. Each child has a 20-second deadline and bounded streams and result files.
A child that fails is terminated and reaped; its partial output and terminal
status remain in the failed receipt. Missing local inputs or executables exit
77 as unavailable. A wrong archive hash, nonzero child exit, unexpected native
output, incomplete result inventory or mismatch fails the comparison.

The capture retains authored input recipes and bytes, compiled authored helper,
complete stdout and stderr, per-call returned bytes, runtime identities and
process outcomes. Readback checks all 60 profiles and 121 process phases,
regenerates every input, and repeats the complete trace and byte comparisons
without running either codec:

```text
python tests/reference/sound/wav/compare.py --audit-only /absolute/capture-directory
```

CTest exposes the comparison as `reference.wav_policy` when
`MC_ENABLE_WAV_REFERENCE_TESTS=ON`, with `MC_REFERENCE_CLIENT_JAR` set to the
supplied archive. Java must resolve to feature version 17. A headless build can
disable `MC_BUILD_CLIENT`, `MC_BUILD_CLIENT_SOUND` and `MC_BUILD_CLIENT_AUDIO`;
the WAV target has no Vorbis or OpenAL dependency. Run `client.wav_codec` for
the native ownership and parsing suite. An unavailable reference test is a
skip, not successful original evidence.

On 2026-10-25, Windows x64 MSVC Debug passed the native suite's 93,087 checks,
all 17 public protocol checks and all 60 original/native profiles with exact
trace and byte equality. Independent retained-output readback also passed.
The observed original runtime was Eclipse Adoptium 17.0.17+10, amd64,
little-endian. This qualifies the declared cases for that provider profile.
It does not establish all WAVE permutations, concurrent-file schedules or
historical JVMs. The integrated Clang 18 and GCC 13 Linux ASan/UBSan runs also
pass all 60 exact profiles with OpenJDK 17.0.20. Each retained comparison records
its actual provider classes and runtime identity; this does not qualify other
JDK versions or providers.

CodecWav's generic AudioSystem call can also admit other installed-provider
containers. Recognized AIFF, AU and MIDI headers currently return native
`MC_WAV_CODEC_UNIMPLEMENTED_CONTAINER`; their provider behavior remains work.
The codec comparison opens no device and supplies no evidence for OpenAL
format conversion, playback gain, channels, streaming refill or audible output.
The broader [sound resource contract](../sound.md) and
[original sound reference](../../reference/client-sound.md) describe those
separate owners.
