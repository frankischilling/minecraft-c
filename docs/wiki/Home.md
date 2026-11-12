# minecraft-c

A C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows and Linux.
Development focuses on the single-player client. Full parity and unrestricted
playability remain incomplete; multiplayer and the dedicated server follow
the client work. Account authentication is the only approved gameplay exception.

[Player wake during damage](Combat-and-projectiles#recursive-bed-notification-during-wake)
now reaches the original Bed update through the same wake body as direct Player
damage. A nearby orphan head or foot removes itself through the recursive
neighbor call. The foot publishes one Bed Item before wake finishes. The
[Ladder neighbor](Combat-and-projectiles#ladder-drop-during-wake) also publishes
its Item before the notifying air setter removes the Ladder. Interrupted
observers preserve each completed prefix and inserted Item. Prepared real
Arrow hits cover wake and staged failure cleanup. The
[ordinary Torch neighbor](Combat-and-projectiles#torch-removal-and-scene-inputs)
shares that execution through both support-loss removal paths. The
[Flower neighbor](Combat-and-projectiles#flower-survival-during-wake) keeps
supported Flowers and orders unsupported removal after Item publication.

[Sapling and Tall Grass neighbors](Combat-and-projectiles#sapling-and-tall-grass-during-wake)
now share that retained survival path with their actual virtual drops. Tall
Grass can remove itself without publishing an Item or requiring Math. Finite
original comparisons and actual Arrow-hit controls retain separate scopes.

[Slime removal](Combat-and-projectiles#slime-removal-and-splitting) runs through
ordinary Mob ticks. Two Windows scenes use the usual controls to pause and
save after four children replace the parent. Independent readback finds the
four living Slimes.

Graphical wake from damage, other neighbor hooks, full Mob
environment and particles, original audiovisual equality, and measured FPS
remain open.

## Start here

- [Startup and controls](Startup)
- [Build and resource import](Build-and-tests)
- [Combat and projectiles](Combat-and-projectiles)
- [World simulation](World-simulation)
- [Storage and reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
- [Parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
- [Test results and limits](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
- [Earlier evidence](History)

Import original resources locally. After import, ordinary runtime operation needs
no JVM, original archive or account services. Original resources and reference
binaries remain outside the distributed project.
