# minecraft-c

A C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows and Linux.
Development focuses on the single-player client. Full parity and unrestricted
playability remain incomplete; multiplayer and the dedicated server follow
the client work. Account authentication is the only approved gameplay exception.

[Player wake during damage](Combat-and-projectiles#recursive-bed-notification-during-wake)
now reaches the original Bed update through the same wake body as direct Player
damage. A nearby orphan head or foot removes itself through the recursive
neighbor call. The foot publishes one Bed Item before wake finishes. An
interrupted observer leaves both completed block changes and the inserted
Item in place. Prepared real Arrow hits cover wake and staged failure cleanup.

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
