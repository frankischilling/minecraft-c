# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and measured FPS
remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

[Lava mixing and fizz](world/fluids.md#owned-lava-mixing-during-player-wake)
now share retained Player wake execution. Reached Obsidian/Cobblestone writes,
height callbacks, light queues, recursive Water conversion and both possible
fizz passes preserve their real order. Source callback exceptions and native
refusals keep separate diagnostics and completed effects through checked
cleanup.

Two fresh unchanged-original VMs agree on 29 prepared profiles. Native replay
pairs selected before/after Player and Random state, all five complete Chunk
planes, Lava/Water cells, light queues, scheduled-class state and ordered
callbacks. Five ownership controls cover FP admission, sound/particle child
scopes and the separate Java Math and Minecraft MathHelper dependencies.

The [current test record](testing.md#lava-mixing-during-player-wake) separates
these original pairs, native controls and regression scopes. Earlier
[Cactus/Reeds](testing.md#cactus-and-reeds-during-player-wake) and
[flower-family evidence](testing.md#remaining-flower-family-neighbors-during-player-wake)
keep their own qualification. Natural encounters, complete owned ticks,
original audiovisual equality and FPS remain unfinished.

The full Windows MSVC Release suite passes 1,246 tests without skips. Focused
Clang ASan/UBSan/leak-detection passes 112 tests; strict GCC compilation passes
the new fixture and three changed runtime units. Windows and Linux each match
9,811 selected original scalars plus 748 from supported ownership controls.

Fresh monitor-2 Bed and Arrow/Pig scenes pass independent NBT readback, six
inspected captures and complete window cleanup. The
[preceding hosted checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/43670553109)
passes formatting, Windows, GCC and Clang. New results belong to their recorded
source snapshots and do not qualify unrestricted play.

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
