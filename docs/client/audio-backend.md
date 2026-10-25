# Native sound backend

`mc_sound_backend` implements the reached ordinary SoundManager backend profile
with real OpenAL buffers, sources and device queries. It copies request names
and file identities, owns decoded and device caches separately, defers queued
plays, services streams and joins native workers during retirement. A successful
queue submission does not mean a source is playing: that query reaches the
attached channel's actual `AL_SOURCE_STATE`.

The application does not yet create this owner or register imported sound files
with SoundManager. Tests exercise the concrete backend directly through the
manager interface. GUI/WorldAccess adoption, complete startup ordering and
original device/output comparison remain unfinished.

## Owners and execution

Factory creation allocates a callback context without opening a device or
drawing entropy. Actual SoundSystem construction consumes the additional
ordinary Random after SoundManager's pool and manager Random construction,
then creates queues/workers and the device/context. The normal profile selects
the default native device and runs distinct command and streaming workers.
Explicit loopback/manual profiles support deterministic tests using real OpenAL.

Ordinary command draining applies deferred PLAY requests after its ordinary
phase. Background music retains its separate immediate quick-play and queued
play behavior. Replacement names can leave displaced sources retained by
channels or stream watches. Gain, pitch and playing queries respect attached
identity; stale stop retains its reached channel behavior. Listener position and
orientation calls remain separate operations.

Decoded cache publication precedes device-format/buffer success. A later device
failure can therefore leave decoded PCM without an OpenAL buffer. Streaming
uses actual codecs for format selection, reinitialization, preload, refill and
EOS. WAV preserves its raw source encoding; device-format selection follows
channels and sample width rather than silently converting every input to PCM16.
Vorbis's unresolved mono synthesis differences remain documented separately.

The shared SoundManager owner first retires its backend instance; the callback
context then retires. Source cleanup and native owner retirement are separate.
Native bounds, transfer errors and failed joins retain explicit status and
ownership. Device/context attributes are bounded native policy; the effective
original LWJGL attributes remain unqualified.

## Tests and limits

Build with `MC_BUILD_CLIENT_AUDIO=ON` and `MC_BUILD_CLIENT_SOUND=ON`.
`client.sound_backend` opens actual loopback contexts, uploads authored files,
renders nonzero PCM and checks source transitions. It exercises copied resource
lifetimes, deferred order, cache aliases, channel stealing, stale stops,
listener ordering, pitch/volume, Ogg/WAV streaming and real worker cleanup.
Every fixture checks created/deleted OpenAL buffer counters. Displaced sources,
partial cache publication, interrupted drains, retained first-worker diagnostics
and full SoundManager/settings composition have separate regressions. Cleanup
and native retirement remain distinct, including their reported reached state.
It needs no original assets, Java or physical audio output.

The test executable also accepts `--default-device` after the absolute
`tests/client/sound/fixtures/vorbis` path. That profile opens the host device and
plays an authored looping tone briefly. A passing native run is not an original
SoundSystem/device correspondence result or proof of audible speaker output.

MIDI, raw streams, fades/sequences, activation/culling, temporary sources,
silent-library fallback and adversarial HashMap tree buckets are outside the
implemented ordinary profile. Original backend execution, device timing,
full PCM equality and general audio parity remain unverified. See
[SoundManager](sound-manager.md), [Vorbis policy](vorbis-codec.md),
[WAV policy](sound/wav-codec.md) and [dependency setup](../dependencies.md).
