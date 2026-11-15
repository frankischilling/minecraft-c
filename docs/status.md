# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and measured FPS
remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

[Dead Bush, Crops and Mushroom neighbors](world/plants.md#owned-dead-bush-crops-and-mushroom-neighbors-during-player-wake)
now share retained Player wake execution with their actual virtual survival and
drops. Dead Bush removes itself without an Item. Crops retain Wheat followed by
three Seed trials; Mushroom survival uses real light below 13 and live substrate.
Observer exceptions preserve completed cells, Items and Random progress through
checked cleanup.

Two fresh unchanged-original VMs agree on 25 prepared profiles. Windows and
sanitized Linux each match
1,940 selected original values, including all five Chunk planes, seeded Math
and Item motion. Five native ownership controls include two no-Item paths that
match another 106 original values. Ten additional real Arrow-hit cases cover
distinct committed prefixes. These component results do not establish graphical
sleeping-Player damage. The earlier
[Sapling/Tall Grass checkpoint](testing.md#sapling-and-tall-grass-during-player-wake)
retains its own Windows/Linux and graphical qualification.

The [current test record](testing.md#remaining-flower-family-neighbors-during-player-wake)
lists the new coverage and comparison limits. Original audiovisual equality,
unrestricted plant graphs, general focus reliability and an FPS gain remain
unqualified.

The full Windows MSVC Release suite passes 1,229 tests without skips. All 43
affected tests also pass on Linux with Clang ASan/UBSan and leak detection.
Fresh monitor-2 Bed and Arrow/Pig scenes pass independent save readback, six
inspected captures and complete window cleanup.

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
