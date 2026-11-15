# Development status

Development targets a fully playable single-player client with Java Beta 1.7.3
behavior on Windows and Linux. Full parity, sustained play and measured FPS
remain incomplete. The [parity matrix](parity.md) records each qualified scope;
the [inventory](inventory.md) and [roadmap](roadmap.md) preserve the remaining work.

## Current client work

Ordinary client [Fire source bodies](world/fire.md) now run through scheduled
and Player-centered random World ticks. Fire addition, support loss, rain,
burning and spread use the actual World queries, quiet age write and notifying
block writes. Reached Portal creation/collapse, delegated Stairs hooks and
immediate TNT construction/publication/sound share those calls.

Two fresh unchanged-original VMs agree on 71 prepared profiles. Native replay
on Windows and Linux matches 88,077 selected original scalars, including all
five complete Chunk planes, Random streams, ordered callbacks, scheduled queues and TNT
constructor fields. Three separate native lifetime controls retain failed
callback receipts until the World tick retires.

The audit also corrected scheduled membership insertion and resize order.
Independent JDK17 observations qualify the reached linked-bin histories;
collision-tree identity order remains unqualified. Player-chunk set traversal
already matched the observed Java order and needed no runtime change.

The full Windows MSVC Release suite passes 1,256 tests in 364.40 seconds.
Focused Linux Clang ASan/UBSan/leak checks pass 80 tests plus the revised plant
lifecycle case; strict GCC compilation covers 25 changed C units. All sources
remain frozen during their respective runs. Releasing an unnecessary lock lets
56 isolated World query controls pass with eight jobs in 52.54 seconds.

The [test record](testing.md#fire-source-bodies) keeps the original comparisons,
native controls and regression results separate. Earlier
[Lava wake](testing.md#lava-mixing-during-player-wake),
[Cactus/Reeds](testing.md#cactus-and-reeds-during-player-wake) and
[flower-family](testing.md#remaining-flower-family-neighbors-during-player-wake)
evidence retains its own scope. Retained owned Fire execution, delayed TNT
ticks/explosions, Portal travel, unrestricted play, original audiovisual equality
and measured FPS remain unfinished. Fresh monitor-2 graphical qualification
also remains incomplete; the test record preserves failed Bed and Arrow/Pig
opportunities and their separate diagnostic evidence.

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
