# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and measured FPS
remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

[Cactus and Reeds neighbors](world/plants.md#owned-cactus-and-reeds-neighbors-during-player-wake)
now share retained Player wake execution. Removing the bottom plant can remove
a complete column through recursive notifications. Real Sand scheduling and
Water conversion below the Bed preserve metadata, class sequence and callbacks.
Source exceptions keep completed effects and the actual sleeping Player state
through checked cleanup.

Two fresh unchanged-original VMs agree on 21 prepared profiles. Windows and sanitized Linux each pair
2,628 selected values, including complete Chunk planes, column state, Random,
Item motion and exact scheduler/callback effects. Four ownership controls
include one supported no-Item branch matching another 70 values. The range
observer exception keeps the converted Water and suppression flag without
performing the second schedule request or waking the Player.

The [current test record](testing.md#cactus-and-reeds-during-player-wake) separates
the original pairs, native ownership controls and broader regression scopes.
Earlier [flower-family evidence](testing.md#remaining-flower-family-neighbors-during-player-wake)
retains its own Windows/Linux and graphical qualification. Unrestricted plant
graphs, natural encounters, original audiovisual equality and FPS remain
unqualified.

The full Windows MSVC Release suite passes 1,241 tests without skips. The
focused Clang ASan/UBSan/leak-detection selection passes 107 tests. Fresh
monitor-2 Bed and Arrow/Pig scenes pass independent readback, six inspected
captures and complete window cleanup.

The [preceding hosted checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/43666280343)
passes formatting, Windows, GCC and Clang. New local results belong to their
recorded source snapshots and do not qualify unrestricted play.

## Next single-player work

- Complete remaining owned neighbor and entity paths reached by ordinary play.
- Extend natural encounters, inventory interactions, travel, death/respawn and
  save/reload scenarios beyond the existing prepared cases.
- Validate extended Windows and Linux play, including Chunk unload/reload,
  dimension changes, persistence and state drift.
- Measure frame times, stutters, memory and loading on matched original/native
  scenarios before adopting performance changes.

Multiplayer interoperability and the dedicated server follow the client work.
Account authentication is the only approved gameplay exception. A playable
subset does not complete the original behavior inventory.

Earlier results remain in [development checkpoints](history/status-checkpoints.md),
[client runtime history](history/client-runtime.md), [population history](history/population.md)
and [integration history](history.md). Their results apply to their own source
and scenario scopes.
