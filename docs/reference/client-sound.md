# Original client sound contract

The authority is the supplied Beta 1.7.3 client JAR, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The following are observations of class bytes, Code instructions and exception
tables. They are not original audio execution or a measured PCM comparison.
The [native resource owners](../client/sound.md) implement the current foundation.

| Owner | Original client alias | Relevant body |
| --- | --- | --- |
| SoundManager | `yo` | Construction, settings binding, setup, option changes, cleanup, effects, streaming, music and listener |
| SoundPool | `hr` | `<init>()V`, `a(String,File)bh`, `a(String)bh`, `a()bh` |
| SoundPoolEntry | `bh` | Original logical name and actual URL |
| MUS codec | `io` | Subclasses bundled CodecJOrbis and wraps its input |
| MUS InputStream | `np` | `<init>(io,URL,InputStream)V`, `read()I`, `read([BII)I` |

SoundManager owns three pools and a fourth, separate Random. Each pool constructs
its own ordinary default Random. The manager consumes `nextInt(12000)` for its
initial music delay. The SoundSystem reference and initialized flag are static;
constructing a replacement manager does not reset them. Source cleanup and
native owner retirement therefore have different meanings.

## Registration and selection

`hr.a(String,File)bh` takes the prefix before the first dot, optionally strips
trailing `Character.isDigit(char)` units, then replaces `/` with `.`. Streaming
disables stripping; music and effects retain it. Null-name dereference reaches
PC7, missing-dot substring PC10, and stripped-empty `charAt` PC28.

A new empty group is inserted at PC86. File-to-URI/URL calls follow at PCs98/101,
then group append124, full-list append136 and count store149. Return154 follows
success. The local catch handles only MalformedURLException. Duplicate entries
are retained. Native resource/encoding failures remain native errors rather
than invented Java exceptions or silently repaired names.

Key lookup returns null at PC19 when absent, without a draw. Both selection
bodies call actual `Random.nextInt(list.size())` at PC31 and return at PC42. The
full-list body returns null at PC13 when empty. A present empty group reaches
`nextInt(0)`; size one still consumes a draw. No HashMap iteration order is
needed to select from a known key, while each group's list order matters.

Minecraft resource registration routes `sound`/`newsound` to effects,
`streaming` to the non-stripping pool and `music`/`newmusic` to music. The original
`cz` loader traverses unsorted `File.listFiles()` recursively. A sorted imported
manifest does not establish original registration order.

## Setup and reached GUI effects

`yo.a(kv)V` sets the streaming flag at PC5 and retains settings at PC10. When the static
initialized flag is false, only nonnull settings with both volumes exactly zero
suppress setup. Null settings enter setup, whose initial dereference is caught.
NaN and other nonfinite values are not clamped by this gate.

Setup `yo.d()V` reads the two volumes at PCs4/12, sets them to zero at PCs21/29 and calls
the actual GameSettings save at PC36. It registers the OpenAL library and Ogg/MUS/
WAV codecs at PCs41/48/55/62, constructs SoundSystem at PC69 and publishes it at PC72,
restores volumes at PCs80/88 and saves again at PC95. The Throwable handler covers
`[0,98)` and starts at PC101. There is no restoring finally; a failure may retain
zero volumes, partial files and an older or absent backend. The initialized flag
is set true at PC115 after either the normal path or caught failure.

Options change may enter this setup, then stops `BgMusic` for zero music or
sets that source's volume. It does not update every currently playing effect.
The GUI mouse handler stores the pressed button at PC54, plays the actual click at PC68
and dispatches its action at PC74. Sliders also require their draw-time setting
actions. Sound setup's genuine nested saves must coexist with settings' external
diagnostic reentry guard; disabling that guard globally changes the contract.

## Playback

UI one-shots select an effect, advance shared `(counter+1)%256`, create a source
at the origin with attenuation 0, set pitch, upper-clamp requested volume, scale
it by `0.25 * settings.sound`, then play. The positional method selects before
checking requested volume positive. A matching zero, negative or NaN volume can
therefore consume a pool draw. Positive calls use the same counter, attenuation 2
and distance 16 or `16 * requestedVolume`, with priority for volume greater than 1.

Streaming stops an existing `streaming` source before testing a null new name.
A matching positive request stops `BgMusic`, creates fixed `streaming` at
distance 64, sets `0.5 * settings.sound` and plays. Requested pitch and positive
volume magnitude are not forwarded. Music checks `BgMusic` then `streaming`,
decrements a positive delay and returns even if it just became zero. A successful
pool selection consumes manager `nextInt(12000)+12000` before starting music.

Listener interpolation uses binary32 yaw arithmetic and double positions with
the float partial converted to double. Its orientation uses the original sine/
cosine lookup operations; position is set before orientation.

## MUS and backend ownership

`np` seeds its int from the Java String hash of the URL path's final segment.
Its bulk read transforms only the actual underlying prefix, XORing each byte
with arithmetic `state >> 8`. The signed decoded byte then feeds wrapped
`state * 498729871 + 85731 * decodedByte`. Single-byte read returns a signed byte.
The class has no close override. Exact Code SHA-256 values are:

| Body | SHA-256 |
| --- | --- |
| Constructor,53 bytes | `752abe34baeac51209ad0e9a5f9a54829132fda0b1d6cbee76a4cfebe7ce1a77` |
| Single read,24 bytes | `1458d8de4dda6c6794cf1b1f979617d452d4ee84398b13a70f46f3f7994c1f65` |
| Bulk read,65 bytes | `99b38105a7a79dd8941816c784f4d17373ac9226d2fcd05cf592c30b9cc30522` |

The JAR bundles Paulscode SoundSystem, its LWJGL OpenAL library/channel/source
classes, Jogg/JOrbis and codecs. Setters enqueue commands for a command thread;
they do not synchronously call OpenAL. Default configuration has 28 normal and 4
streaming channels, three 131,072-byte streaming buffers and master gain 1.
For attenuation 2, SourceLWJGLOpenAL itself computes ordered bounded linear gain
from listener distance. Selecting an OpenAL distance mode alone omits that code.
Source gain, pitch, reuse, channel stealing, streaming refill and shutdown still
need complete native implementation and controlled observations.

The local availability check finds 326 matching imported/external audio files,
44,011,339 bytes. This is an availability floor, not a canonical Beta track list
or registration-order proof. Bounded MUS header projections, including differing
raw/escaped space-bearing names, do not establish playable PCM or justify
choosing a basename merely because it yields an Ogg signature.
