# SoundManager

`mc_sound_manager` implements the client `yo` call ordering above the sound
backend. It owns effects, streaming and music SoundPools, a separate ordinary
Random, the effect counter and music delay. Managers borrow a shared owner for
the original static SoundSystem reference and initialized flag. Creating or
retiring a manager does not reset that shared state.

The backend interface represents actual SoundSystem calls and playing queries.
It must copy borrowed command arguments before returning. Manager unit tests
record those calls. Separate [backend tests](audio-backend.md) use real OpenAL
buffers, loopback and workers; they do not establish original device correspondence.
The [original sound contract](../reference/client-sound.md) identifies the
supplied class bodies and their ordering.

## Setup and settings

Binding settings first disables digit stripping for streaming names, then
retains the settings owner. Before shared initialization, two exactly zero
volumes suppress setup. Other values, including NaN, retain the source gate.
Null settings reach setup's caught dereference and latch initialization.

Setup saves zero music/sound volumes, registers OpenAL and Ogg/MUS/WAV codecs,
constructs and publishes the backend, restores both volumes, then saves again.
Both saves use the production GameSettings writer. A caught source Throwable
leaves the reached prefix and sets initialized; there is no restoring finally.
The report records that exception without inventing a Java stack trace.

Native allocation, diagnostic and backend failures retain a different outcome.
They stop setup and latch a terminal native failure on the shared owner.
Later setup calls return that failure without overwriting an already published
backend or repeating writes. Native retirement still releases the handle.
This resource policy does not qualify original exception equivalence.

Normal startup now invokes this bind on its actual `B` manager and `z` settings.
The two setup saves use the canonical options File and typed FileWriter. The
real Windows Session returns from the bind at488, all nine texture-effect
registrations and both scene constructors, stopping before resource-thread
allocation664. This establishes that startup's earlier settings-save failure
is cleared; original SoundManager/device correspondence remains unverified.

`mc_game_settings_attach_sound_manager` binds the actual borrowed owner used
by music/sound option actions. The attached manager must retain those settings.
The original null-Minecraft failure still occurs before manager access.
Detach before retiring the manager. A narrow internal volume/save route permits
setup's nested operations while the option action holds the settings guard.
Public mutation, save, retirement and diagnostic reentry still return `BUSY`.
An action performs no additional save of its own.

## Playback requests

UI and positional effects share the modulo-256 counter. Selection consumes the
pool draw before positional volume admission. UI requests use the source's
upper clamp and quarter-volume scaling; positional requests retain priority,
distance and attenuation arguments. Streaming stops its previous source before
testing a null name, and a successful request can stop background music.

Music checks background and streaming activity before decrementing its delay.
A successful selection draws the next delay before sending backend requests.
Listener updates preserve float yaw arithmetic, double position interpolation,
the original lookup-table orientation and position-before-orientation calls.
Source cleanup and native shared retirement remain separate operations.

## Validation and remaining work

Native Windows MSVC x64 Debug passes 20,066 SoundManager checks and 62 separate
settings integration checks. Fresh Ubuntu Clang 18 and GCC 13 ASan/UBSan
profiles each pass all 11 selected sound/settings/reference tests without
failures or skips. The tests cover command order, Random continuation,
counter wrap, nonfinite volumes, failures, nested saves, retained field prefixes
and public reentry. They use an authored backend recorder and real settings
owners. The concrete backend and [WAV codec](sound/wav-codec.md) have separate
contracts and evidence. Original SoundManager execution, device correspondence,
resource registration order and GUI/WorldAccess adoption remain open.

Run `client.sound_manager` and `client.settings_sound` without a display, audio
device, original archive or Java. Settings and manager sources share one static
target because their original operations call each other; codec libraries stay
optional and outside the dedicated-server dependency graph.
