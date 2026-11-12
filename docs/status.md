# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and measured FPS
remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

[Sapling and Tall Grass neighbors](world/plants.md#owned-sapling-and-tall-grass-neighbors-during-player-wake)
now share retained Player wake execution. Supported plants stay in place;
unsupported plants publish their actual virtual drop before removal. Tall Grass
can remove itself without an Item or Math owner. Observer exceptions preserve
completed cells, Items, Player state and Random progress through checked cleanup.

Two fresh unchanged-original VMs agree on eight prepared profiles. Windows and
sanitized Linux each match 379 selected original values. Fifteen direct modes
and seven real Arrow-hit cases cover success and distinct failure prefixes.
These component results do not establish graphical sleeping-Player damage.

The [current test record](testing.md#sapling-and-tall-grass-during-player-wake)
lists the complete suite results, source snapshot and remaining comparison limits.
Fresh monitor-2 Bed and Arrow/Pig scenes pass independent save readback, inspected
images and complete window cleanup. Original audiovisual equality, unrestricted
plant graphs, general focus reliability and an FPS gain remain unqualified.

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
